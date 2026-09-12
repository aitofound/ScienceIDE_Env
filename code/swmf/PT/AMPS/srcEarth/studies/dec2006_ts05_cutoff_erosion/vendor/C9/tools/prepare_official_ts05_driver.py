#!/usr/bin/env python3
"""Prepare a provenance-preserving AMPS TS05 event driver.

The official Tsyganenko yearly files use one whitespace-delimited record per
five-minute epoch:

    year doy hour minute Bx By Bz Vx Vy Vz Np Temp SymH IMFflag SWflag
    dipole_tilt Pdyn W1 W2 W3 W4 W5 W6

This utility accepts either the yearly ``.dat`` file or the downloaded ``.zip``
archive, selects an inclusive UTC interval, and writes the simple header format
accepted by ``EarthUtil::LoadTsDriverFile``.  Provenance comments are inserted
above the numerical header.  The C9 runner recognizes those comments and will
only label a result scientifically eligible when the source kind is the official
Tsyganenko TS05 yearly product.

No physics is synthesized here: values and quality flags are copied verbatim.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import sys
import urllib.request
import zipfile
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Iterable, Iterator, Sequence, TextIO, Tuple

OFFICIAL_BASE = "https://geo.phys.spbu.ru/~tsyganenko/TS05_data_and_stuff/"
OUTPUT_COLUMNS = (
    "Bx By Bz Vx Vy Vz Np Temp SYM-H IMFflag SWflag Tilt Pdyn "
    "W1 W2 W3 W4 W5 W6"
)


def parse_utc(text: str) -> datetime:
    value = text.strip().replace("Z", "+00:00")
    dt = datetime.fromisoformat(value)
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=timezone.utc)
    return dt.astimezone(timezone.utc)


def acquire_source(source: str) -> Tuple[bytes, str]:
    """Return raw bytes and a human-readable source locator."""
    if source.startswith(("http://", "https://")):
        with urllib.request.urlopen(source, timeout=180) as response:
            return response.read(), source
    path = Path(source).expanduser().resolve()
    return path.read_bytes(), str(path)


def unpack_text(raw: bytes, locator: str) -> Tuple[str, str]:
    """Return decoded table text and member name for a zip or plain file."""
    if zipfile.is_zipfile(io.BytesIO(raw)):
        with zipfile.ZipFile(io.BytesIO(raw)) as archive:
            candidates = [
                name for name in archive.namelist()
                if not name.endswith("/") and name.lower().endswith((".dat", ".txt"))
            ]
            if len(candidates) != 1:
                raise ValueError(
                    "expected exactly one .dat/.txt member in %s; found %r" %
                    (locator, candidates)
                )
            name = candidates[0]
            return archive.read(name).decode("ascii", errors="strict"), name
    return raw.decode("ascii", errors="strict"), Path(locator).name


def parse_records(lines: Iterable[str]) -> Iterator[Tuple[datetime, Sequence[str]]]:
    """Yield official records after strict structural/date validation."""
    previous = None
    for line_number, raw_line in enumerate(lines, start=1):
        text = raw_line.strip()
        if not text or text.startswith(("#", "!")):
            continue
        fields = text.split()
        if len(fields) != 23:
            raise ValueError(
                "line %d has %d fields; expected the 23-column official TS05 format" %
                (line_number, len(fields))
            )
        try:
            year, doy, hour, minute = map(int, fields[:4])
            epoch = datetime(year, 1, 1, tzinfo=timezone.utc) + timedelta(
                days=doy - 1, hours=hour, minutes=minute
            )
            # Parse every remaining token now, so malformed or nonnumeric records
            # fail before an apparently valid driver is written.
            [float(value) for value in fields[4:13]]
            int(float(fields[13])); int(float(fields[14]))
            [float(value) for value in fields[15:]]
        except Exception as exc:
            raise ValueError("invalid numerical record at line %d: %s" %
                             (line_number, text)) from exc
        if previous is not None and epoch <= previous:
            raise ValueError("epochs are not strictly increasing at line %d" % line_number)
        previous = epoch
        yield epoch, fields[4:]


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "source",
        help=("Official YEAR_OMNI_5m_with_TS05_variables.zip/.dat path or URL. "
              "For 2006, the expected archive is "
              "2006_OMNI_5m_with_TS05_variables.zip."),
    )
    parser.add_argument("output", help="Output AMPS driver text file")
    parser.add_argument("--start", required=True, help="Inclusive UTC start")
    parser.add_argument("--end", required=True, help="Inclusive UTC end")
    parser.add_argument(
        "--source-url",
        default="",
        help="Override the provenance URL written when SOURCE is a local file",
    )
    args = parser.parse_args(argv)

    start = parse_utc(args.start)
    end = parse_utc(args.end)
    if end < start:
        parser.error("--end must not precede --start")

    raw, locator = acquire_source(args.source)
    source_sha = hashlib.sha256(raw).hexdigest()
    text, member = unpack_text(raw, locator)
    selected = [(epoch, values) for epoch, values in parse_records(text.splitlines())
                if start <= epoch <= end]
    if not selected:
        raise SystemExit("no official records fall inside the requested interval")

    # The official product is nominally five-minute.  Refuse a truncated or
    # accidentally filtered subset with a large internal gap.
    gaps = [int((b[0] - a[0]).total_seconds()) for a, b in zip(selected, selected[1:])]
    if gaps and max(gaps) > 600:
        raise SystemExit("selected driver has a gap larger than 10 minutes")

    output = Path(args.output).expanduser().resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    source_url = args.source_url.strip()
    if not source_url:
        if locator.startswith(("http://", "https://")):
            source_url = locator
        else:
            source_url = OFFICIAL_BASE + Path(member).name.replace(".dat", ".zip")

    with output.open("w", encoding="ascii", newline="\n") as stream:
        stream.write("# C9_DRIVER_SOURCE_KIND TSYGANENKO_TS05_OFFICIAL\n")
        stream.write("# C9_DRIVER_SOURCE_URL %s\n" % source_url)
        stream.write("# C9_DRIVER_SOURCE_ARCHIVE_SHA256 %s\n" % source_sha)
        stream.write("# C9_DRIVER_SOURCE_MEMBER %s\n" % member)
        stream.write("# C9_DRIVER_PREPARED_UTC %s\n" %
                     datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"))
        stream.write("# YYYY-MM-DDTHH:MM:SS %s\n" % OUTPUT_COLUMNS)
        for epoch, values in selected:
            stream.write(epoch.strftime("%Y-%m-%dT%H:%M:%S") + " " +
                         " ".join(values) + "\n")

    print("Wrote %d official TS05 records to %s" % (len(selected), output))
    print("Coverage: %s through %s" %
          (selected[0][0].isoformat(), selected[-1][0].isoformat()))
    print("Source SHA-256: %s" % source_sha)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
