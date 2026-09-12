#!/usr/bin/env python3
"""Download the historical NOAA/NCEI SEM-2 Level-2 files needed by C10.

The December-2006 products are under the official NCEI Level-2 ``v01r00``
archive.  NCEI publishes both 16-second ASCII and CDF files.  The directory
layout and exact daily-file names have changed over the lifetime of the archive,
so this utility does not hard-code a single filename template.  Instead it:

1. starts at the official year directory;
2. recursively reads ordinary Apache-style directory listings;
3. selects links whose names contain the requested date and satellite tokens;
4. downloads matching files; and
5. writes a manifest with URL, size, and SHA-256 for every file.

The downloader uses only the Python standard library.  It is intentionally
separate from the scientific reference builder: a user can download once,
inspect and freeze the source files, and rebuild the reference repeatedly
without further network access.

Typical command
---------------

    python3 download_poes_sem2.py \
      --start 2006-12-05 --end 2006-12-16 \
      --satellites n15,n16,n17,n18,m02 \
      --format txt --output-dir data/reference_source

If an institutional mirror blocks directory indexing, use ``--url-list`` with a
text file containing one exact HTTPS URL per line.  This preserves the same
checksum manifest and avoids any filename assumption in the code.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import time
from datetime import date, datetime, timedelta, timezone
from html.parser import HTMLParser
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Set, Tuple
from urllib.error import HTTPError, URLError
from urllib.parse import urljoin, urlparse
from urllib.request import Request, urlopen

BASE_BY_FORMAT = {
    "txt": "https://www.ncei.noaa.gov/data/poes-metop-space-environment-monitor/access/l2/v01r00/txt/{year}/",
    "cdf": "https://www.ncei.noaa.gov/data/poes-metop-space-environment-monitor/access/l2/v01r00/cdf/{year}/",
}

# Archive spacecraft tokens.  The names are deliberately the short tokens used
# in historical POES filenames and in NCEI notices.
VALID_SATELLITES = ("n15", "n16", "n17", "n18", "m02")


class LinkParser(HTMLParser):
    """Collect href values from a simple NCEI directory-index page."""

    def __init__(self) -> None:
        super().__init__()
        self.hrefs: List[str] = []

    def handle_starttag(self, tag: str, attrs: Sequence[Tuple[str, Optional[str]]]) -> None:
        if tag.lower() != "a":
            return
        for name, value in attrs:
            if name.lower() == "href" and value:
                self.hrefs.append(value)


def parse_date(text: str) -> date:
    """Parse an ISO calendar date used by the downloader CLI."""

    return datetime.strptime(text, "%Y-%m-%d").date()


def date_tokens(start: date, end: date) -> Set[str]:
    """Return compact YYYYMMDD strings for every requested day, inclusive."""

    if end < start:
        raise ValueError("--end must be on or after --start")
    tokens: Set[str] = set()
    current = start
    while current <= end:
        tokens.add(current.strftime("%Y%m%d"))
        current += timedelta(days=1)
    return tokens


def fetch_bytes(url: str, timeout: float, retries: int) -> bytes:
    """Fetch one URL with a descriptive user agent and bounded retries."""

    request = Request(url, headers={"User-Agent": "AMPS-C10-POES-reference/1.0"})
    last_error: Optional[Exception] = None
    for attempt in range(retries + 1):
        try:
            with urlopen(request, timeout=timeout) as response:
                return response.read()
        except (HTTPError, URLError, TimeoutError) as exc:
            last_error = exc
            if attempt < retries:
                time.sleep(1.5 * (attempt + 1))
    raise RuntimeError(f"failed to fetch {url}: {last_error}")


def list_links(url: str, timeout: float, retries: int) -> List[str]:
    """Read one directory listing and return absolute child URLs."""

    text = fetch_bytes(url, timeout, retries).decode("utf-8", errors="replace")
    parser = LinkParser()
    parser.feed(text)
    result: List[str] = []
    for href in parser.hrefs:
        if href.startswith(("?", "#")):
            continue
        absolute = urljoin(url, href)
        # Keep crawling on the same official host and below the starting path.
        if urlparse(absolute).netloc != urlparse(url).netloc:
            continue
        result.append(absolute)
    return result


def crawl_year_directory(
    root_url: str,
    wanted_dates: Set[str],
    satellites: Set[str],
    data_format: str,
    timeout: float,
    retries: int,
    maximum_depth: int = 3,
) -> List[str]:
    """Recursively discover requested daily files below an NCEI year index."""

    suffixes = (".txt", ".txt.gz", ".asc", ".asc.gz", ".dat", ".dat.gz") if data_format == "txt" else (".cdf", ".cdf.gz")
    queue: List[Tuple[str, int]] = [(root_url, 0)]
    visited: Set[str] = set()
    matches: Set[str] = set()

    while queue:
        url, depth = queue.pop(0)
        if url in visited:
            continue
        visited.add(url)
        try:
            children = list_links(url, timeout, retries)
        except RuntimeError as exc:
            print(f"warning: cannot list {url}: {exc}", file=sys.stderr)
            continue

        for child in children:
            lower = child.lower()
            if child.endswith("/"):
                if depth < maximum_depth and child.startswith(root_url):
                    queue.append((child, depth + 1))
                continue
            basename = Path(urlparse(child).path).name.lower()
            if not basename.endswith(suffixes):
                continue
            if not any(token in basename for token in wanted_dates):
                continue
            if not any(satellite in re.sub(r"[^a-z0-9]", "", basename) for satellite in satellites):
                continue
            matches.add(child)
    return sorted(matches)


def read_url_list(path: Path) -> List[str]:
    """Read exact user-supplied URLs, ignoring blank and comment lines."""

    urls: List[str] = []
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if line and not line.startswith("#"):
            urls.append(line)
    return urls


def sha256_bytes(data: bytes) -> str:
    """Return a lowercase SHA-256 digest for downloaded bytes."""

    return hashlib.sha256(data).hexdigest()


def validate_download_payload(url: str, data: bytes) -> None:
    """Reject empty and obvious HTML responses before they reach the parser."""

    if not data:
        raise RuntimeError(f"archive returned an empty file for {url}")
    prefix = data[:512].lstrip().lower()
    if prefix.startswith((b"<!doctype html", b"<html", b"<head", b"<body")):
        raise RuntimeError(f"archive returned an HTML page instead of data for {url}")


def download_urls(urls: Sequence[str], output_dir: Path, timeout: float, retries: int, overwrite: bool) -> List[Dict[str, object]]:
    """Download selected URLs and return manifest entries.

    Existing non-empty data are reused unless ``--overwrite`` is requested.
    Existing zero-byte destinations are re-downloaded automatically.  Failed
    transfers never leave a ``.part`` file and never replace valid data.
    """

    output_dir.mkdir(parents=True, exist_ok=True)
    manifest: List[Dict[str, object]] = []
    for index, url in enumerate(urls, start=1):
        name = Path(urlparse(url).path).name
        destination = output_dir / name
        destination_existed = destination.exists()
        had_empty_destination = destination_existed and destination.stat().st_size == 0
        existing_is_usable = destination_existed and not had_empty_destination

        if existing_is_usable and not overwrite:
            data = destination.read_bytes()
            validate_download_payload(url, data)
            status = "existing"
        else:
            if had_empty_destination:
                print(f"warning: re-downloading zero-byte file {destination}", file=sys.stderr)
            print(f"[{index}/{len(urls)}] downloading {url}")
            data = fetch_bytes(url, timeout, retries)
            validate_download_payload(url, data)
            temporary = destination.with_suffix(destination.suffix + ".part")
            try:
                temporary.write_bytes(data)
                temporary.replace(destination)
            finally:
                if temporary.exists():
                    temporary.unlink()
            status = "redownloaded_empty" if had_empty_destination else "downloaded"

        manifest.append({
            "url": url,
            "local_path": str(destination),
            "name": destination.name,
            "size_bytes": len(data),
            "sha256": sha256_bytes(data),
            "status": status,
        })
    return manifest


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--start", type=parse_date, default=parse_date("2006-12-05"))
    parser.add_argument("--end", type=parse_date, default=parse_date("2006-12-16"))
    parser.add_argument("--satellites", default=",".join(VALID_SATELLITES),
                        help="Comma-separated historical tokens: n15,n16,n17,n18,m02")
    parser.add_argument("--format", choices=("txt", "cdf"), default="txt")
    parser.add_argument("--output-dir", type=Path, default=Path("data/reference_source"))
    parser.add_argument("--manifest", type=Path, default=None,
                        help="Default: <output-dir>/download_manifest.json")
    parser.add_argument("--url-list", type=Path, default=None,
                        help="Use exact URLs from a file instead of crawling the NCEI index")
    parser.add_argument("--maximum-depth", type=int, default=3)
    parser.add_argument("--timeout", type=float, default=60.0)
    parser.add_argument("--retries", type=int, default=3)
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args(argv)
    satellites = {token.strip().lower() for token in args.satellites.split(",") if token.strip()}
    unknown = satellites.difference(VALID_SATELLITES)
    if unknown:
        parser.error("unknown satellite token(s): %s" % ", ".join(sorted(unknown)))
    args.satellite_set = satellites
    return args


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    if args.url_list:
        urls = read_url_list(args.url_list)
    else:
        tokens = date_tokens(args.start, args.end)
        years = sorted({token[:4] for token in tokens})
        urls: List[str] = []
        for year in years:
            root = BASE_BY_FORMAT[args.format].format(year=year)
            year_tokens = {token for token in tokens if token.startswith(year)}
            urls.extend(crawl_year_directory(
                root, year_tokens, args.satellite_set, args.format,
                args.timeout, args.retries, args.maximum_depth,
            ))
    if not urls:
        print(
            "No matching archive files were discovered. See README.md section 'Obtaining the "
            "reference data'. You can browse the official year directory and pass exact URLs "
            "with --url-list.",
            file=sys.stderr,
        )
        return 2

    entries = download_urls(urls, args.output_dir, args.timeout, args.retries, args.overwrite)
    manifest_path = args.manifest or (args.output_dir / "download_manifest.json")
    payload = {
        "archive": "NOAA_NCEI_POES_METOP_SEM2_LEVEL2_V01R00",
        "format": args.format,
        "requested_start": args.start.isoformat(),
        "requested_end": args.end.isoformat(),
        "satellites": sorted(args.satellite_set),
        "created_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "files": entries,
    }
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(payload, indent=2) + "\n")
    print(f"downloaded/verified {len(entries)} files")
    print(f"manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
