#!/usr/bin/env python3
"""
compare_dirs.py

Compare files in two directories and report:

  1. Files present only in directory 1
  2. Files present only in directory 2
  3. Files present in both directories but whose contents differ

Optional features:
  - Recursive directory scanning
  - File-extension filtering
  - Creation of a .tar.gz report archive containing:
      * a summary report
      * a unified diff for every pair of differing text files
      * a note for binary files that differ

Examples
--------
Compare all files directly in two directories:

    python compare_dirs.py -d dir_a dir_b

Compare recursively:

    python compare_dirs.py -r -d dir_a dir_b

Compare only .cpp files:

    python compare_dirs.py -r -d dir_a dir_b -e cpp

Compare several extensions:

    python compare_dirs.py -r -d dir_a dir_b -e cpp h py

Save a report archive with an automatically generated name:

    python compare_dirs.py -r -d dir_a dir_b -e cpp h -save

Save the archive using a specific name:

    python compare_dirs.py -r -d dir_a dir_b -e cpp h -save my_report.tar.gz
"""

from __future__ import annotations

import argparse
import difflib
import hashlib
import os
import shutil
import sys
import tarfile
import tempfile
from datetime import datetime
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple


def parse_arguments() -> argparse.Namespace:
    """Parse command-line arguments."""

    parser = argparse.ArgumentParser(
        description=(
            "Compare two directories and report files that are unique to either "
            "directory or have different contents."
        ),
        formatter_class=argparse.RawTextHelpFormatter,
    )

    parser.add_argument(
        "-d",
        "--directories",
        nargs=2,
        metavar=("DIR1", "DIR2"),
        required=True,
        help="Two directories to compare.",
    )

    parser.add_argument(
        "-r",
        "--recursive",
        action="store_true",
        help="Scan subdirectories recursively.",
    )

    parser.add_argument(
        "-e",
        "--extension",
        nargs="+",
        default=None,
        metavar="EXT",
        help=(
            "Restrict the comparison to one or more file extensions.\n"
            "Examples:\n"
            "  -e cpp\n"
            "  -e cpp h py\n"
            "Both 'cpp' and '.cpp' forms are accepted."
        ),
    )

    # nargs='?' allows both:
    #
    #   -save
    #
    # and:
    #
    #   -save output.tar.gz
    #
    # When no filename follows -save, argparse stores the constant "__AUTO__".
    parser.add_argument(
        "-save",
        "--save",
        nargs="?",
        const="__AUTO__",
        default=None,
        metavar="ARCHIVE",
        help=(
            "Save a .tar.gz archive containing the comparison summary and diffs.\n"
            "If ARCHIVE is omitted, an automatic filename is generated."
        ),
    )

    return parser.parse_args()


def normalize_extensions(
    extensions: Optional[Sequence[str]],
) -> Optional[set[str]]:
    """
    Normalize extension names.

    For example:
        ["cpp", ".H", "py"] -> {".cpp", ".h", ".py"}

    None means that all files should be considered.
    """

    if not extensions:
        return None

    result: set[str] = set()

    for ext in extensions:
        ext = ext.strip()

        if not ext:
            continue

        if not ext.startswith("."):
            ext = "." + ext

        result.add(ext.lower())

    return result or None


def extension_matches(path: Path, extensions: Optional[set[str]]) -> bool:
    """Return True if a file satisfies the extension filter."""

    if extensions is None:
        return True

    return path.suffix.lower() in extensions


def scan_directory(
    root: Path,
    recursive: bool,
    extensions: Optional[set[str]],
) -> Dict[str, Path]:
    """
    Scan a directory and return:

        relative/path/to/file -> absolute Path

    Relative paths are used as comparison keys.  Therefore:

        DIR1/src/main.cpp

    is compared with:

        DIR2/src/main.cpp

    when recursive mode is enabled.
    """

    files: Dict[str, Path] = {}

    iterator: Iterable[Path]

    if recursive:
        iterator = root.rglob("*")
    else:
        iterator = root.iterdir()

    for path in iterator:
        if not path.is_file():
            continue

        if not extension_matches(path, extensions):
            continue

        relative = path.relative_to(root).as_posix()
        files[relative] = path

    return files


def files_are_identical(path1: Path, path2: Path) -> bool:
    """
    Compare two files efficiently.

    A size mismatch is detected first.  Files with equal sizes are then
    compared using SHA-256 hashes read in chunks, avoiding loading large files
    completely into memory.
    """

    try:
        if path1.stat().st_size != path2.stat().st_size:
            return False
    except OSError:
        return False

    def digest(path: Path) -> bytes:
        hasher = hashlib.sha256()

        with path.open("rb") as stream:
            while True:
                block = stream.read(1024 * 1024)

                if not block:
                    break

                hasher.update(block)

        return hasher.digest()

    return digest(path1) == digest(path2)


def is_probably_binary(path: Path) -> bool:
    """
    Make a lightweight binary/text classification.

    A NUL byte strongly indicates binary data.  Otherwise the file is treated
    as text and decoded later using UTF-8 with replacement for invalid byte
    sequences.
    """

    try:
        with path.open("rb") as stream:
            sample = stream.read(8192)
    except OSError:
        return True

    return b"\x00" in sample


def make_unified_diff(
    path1: Path,
    path2: Path,
    display1: str,
    display2: str,
) -> str:
    """
    Produce a unified text diff.

    The decoding strategy intentionally uses errors='replace'.  This allows
    useful diffs for source/configuration files that contain an occasional
    non-UTF-8 byte instead of aborting the entire comparison.
    """

    if is_probably_binary(path1) or is_probably_binary(path2):
        return (
            f"Binary files differ:\n"
            f"  {display1}\n"
            f"  {display2}\n"
        )

    try:
        text1 = path1.read_text(encoding="utf-8", errors="replace").splitlines(
            keepends=True
        )
        text2 = path2.read_text(encoding="utf-8", errors="replace").splitlines(
            keepends=True
        )
    except OSError as exc:
        return f"Could not generate diff: {exc}\n"

    diff = difflib.unified_diff(
        text1,
        text2,
        fromfile=display1,
        tofile=display2,
        lineterm="",
    )

    # difflib receives lines that generally still contain their newline
    # characters, while lineterm="" prevents extra line endings from being
    # appended to diff-control lines.  Normalize the result here so the saved
    # output remains readable.
    output: List[str] = []

    for line in diff:
        output.append(line.rstrip("\n"))

    if not output:
        # This can happen in unusual decoding cases even though the byte
        # comparison already established that the files are not identical.
        return (
            "Files differ at the byte level, but no useful text diff "
            "could be generated.\n"
        )

    return "\n".join(output) + "\n"


def safe_diff_filename(relative_path: str, index: int) -> str:
    """
    Build a filesystem-safe filename for a saved diff.

    The original relative path remains visible while directory separators and
    a few problematic characters are replaced.
    """

    safe = relative_path.replace("/", "__").replace("\\", "__")
    safe = safe.replace(":", "_")

    return f"{index:04d}_{safe}.diff"


def format_section(title: str, entries: Sequence[str]) -> str:
    """
    Format one screen/report section.

    Each file deliberately occupies exactly one line, as requested.
    """

    lines = [title, "-" * len(title)]

    if entries:
        lines.extend(entries)
    else:
        lines.append("(none)")

    return "\n".join(lines)


def build_report_text(
    dir1: Path,
    dir2: Path,
    recursive: bool,
    extensions: Optional[set[str]],
    only1: Sequence[str],
    only2: Sequence[str],
    different: Sequence[str],
    identical_count: int,
) -> str:
    """Build the human-readable summary used both on screen and in the archive."""

    if extensions is None:
        extension_text = "all files"
    else:
        extension_text = ", ".join(sorted(extensions))

    header = "\n".join(
        [
            "DIRECTORY COMPARISON REPORT",
            "=" * 80,
            f"Directory 1 : {dir1}",
            f"Directory 2 : {dir2}",
            f"Recursive   : {'yes' if recursive else 'no'}",
            f"Extensions  : {extension_text}",
            "",
        ]
    )

    sections = [
        format_section(
            f"ONLY IN DIRECTORY 1  [{len(only1)}]",
            only1,
        ),
        format_section(
            f"ONLY IN DIRECTORY 2  [{len(only2)}]",
            only2,
        ),
        format_section(
            f"DIFFERENT FILES      [{len(different)}]",
            different,
        ),
    ]

    footer = "\n".join(
        [
            "",
            "SUMMARY",
            "-------",
            f"Only in directory 1 : {len(only1)}",
            f"Only in directory 2 : {len(only2)}",
            f"Different files     : {len(different)}",
            f"Identical files     : {identical_count}",
        ]
    )

    return header + "\n\n".join(sections) + footer + "\n"


def choose_archive_path(save_value: str) -> Path:
    """
    Resolve the requested output archive name.

    With plain '-save', generate a timestamped name.
    """

    if save_value == "__AUTO__":
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        return Path(f"directory_comparison_{timestamp}.tar.gz").resolve()

    archive = Path(save_value).expanduser()

    # Make the result obviously a compressed tar archive even if the caller
    # supplied only a base name.
    name_lower = archive.name.lower()

    if not (
        name_lower.endswith(".tar.gz")
        or name_lower.endswith(".tgz")
    ):
        archive = archive.with_name(archive.name + ".tar.gz")

    return archive.resolve()


def save_report_archive(
    archive_path: Path,
    report_text: str,
    dir1: Path,
    dir2: Path,
    files1: Dict[str, Path],
    files2: Dict[str, Path],
    different: Sequence[str],
) -> None:
    """
    Create the report archive.

    Archive layout:

        report.txt
        diffs/
            0001_some__path.cpp.diff
            0002_other_file.py.diff

    Each diff is based on the two files with the same relative path.
    """

    archive_path.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="compare_dirs_") as tmp:
        temp_root = Path(tmp)
        diff_dir = temp_root / "diffs"
        diff_dir.mkdir(parents=True, exist_ok=True)

        (temp_root / "report.txt").write_text(
            report_text,
            encoding="utf-8",
        )

        for index, relative in enumerate(different, start=1):
            path1 = files1[relative]
            path2 = files2[relative]

            diff_text = make_unified_diff(
                path1,
                path2,
                f"{dir1.name}/{relative}",
                f"{dir2.name}/{relative}",
            )

            diff_name = safe_diff_filename(relative, index)

            (diff_dir / diff_name).write_text(
                diff_text,
                encoding="utf-8",
            )

        # Write directly to the requested .tar.gz path.
        with tarfile.open(archive_path, mode="w:gz") as tar:
            tar.add(temp_root / "report.txt", arcname="report.txt")
            tar.add(diff_dir, arcname="diffs")


def main() -> int:
    args = parse_arguments()

    dir1 = Path(args.directories[0]).expanduser().resolve()
    dir2 = Path(args.directories[1]).expanduser().resolve()

    for label, path in (("DIR1", dir1), ("DIR2", dir2)):
        if not path.exists():
            print(f"ERROR: {label} does not exist: {path}", file=sys.stderr)
            return 2

        if not path.is_dir():
            print(f"ERROR: {label} is not a directory: {path}", file=sys.stderr)
            return 2

    extensions = normalize_extensions(args.extension)

    try:
        files1 = scan_directory(dir1, args.recursive, extensions)
        files2 = scan_directory(dir2, args.recursive, extensions)
    except OSError as exc:
        print(f"ERROR while scanning directories: {exc}", file=sys.stderr)
        return 2

    names1 = set(files1)
    names2 = set(files2)

    only1 = sorted(names1 - names2)
    only2 = sorted(names2 - names1)

    common = sorted(names1 & names2)

    different: List[str] = []
    identical_count = 0

    for relative in common:
        try:
            identical = files_are_identical(
                files1[relative],
                files2[relative],
            )
        except OSError as exc:
            # Treat an unreadable pair as different instead of silently
            # omitting it from the result.
            print(
                f"WARNING: comparison failed for {relative}: {exc}",
                file=sys.stderr,
            )
            identical = False

        if identical:
            identical_count += 1
        else:
            different.append(relative)

    report_text = build_report_text(
        dir1=dir1,
        dir2=dir2,
        recursive=args.recursive,
        extensions=extensions,
        only1=only1,
        only2=only2,
        different=different,
        identical_count=identical_count,
    )

    print()
    print(report_text, end="")

    if args.save is not None:
        archive_path = choose_archive_path(args.save)

        try:
            save_report_archive(
                archive_path=archive_path,
                report_text=report_text,
                dir1=dir1,
                dir2=dir2,
                files1=files1,
                files2=files2,
                different=different,
            )
        except (OSError, tarfile.TarError) as exc:
            print(
                f"\nERROR: could not save report archive: {exc}",
                file=sys.stderr,
            )
            return 2

        print()
        print(f"Saved report archive: {archive_path}")

    # A non-zero exit status is useful in scripts/CI:
    #
    #   0 = directory contents match under the selected scan/filter
    #   1 = one or more differences were found
    #   2 = usage/I/O error
    if only1 or only2 or different:
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
