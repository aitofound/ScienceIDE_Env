#!/usr/bin/env python3
"""
run_tecplot_plt.py

Loop through all .plt files in the current directory, run Tecplot in batch mode
using a Tecplot macro, and rename the generated out.png and/or out.jpeg files
so that they match the original .plt file name.

Default Tecplot command:
    tec360 -mesa

Default macro:
    cut-y-plane-density.mcr
"""

import argparse
import shlex
import subprocess
import sys
from pathlib import Path


def run_tecplot_for_file(
    plt_file: Path,
    macro_file: Path,
    tecplot_cmd: str,
    overwrite: bool,
    dry_run: bool,
    formats: set,
) -> None:
    """
    Run Tecplot in batch mode for one .plt file.

    The Tecplot macro may create:
        out.png
        out.jpeg

    Depending on the --formats option, the requested files are renamed to:
        <original_file_name>.png
        <original_file_name>.jpeg
    """

    out_png = Path("out.png")
    out_jpeg = Path("out.jpeg")

    final_png = plt_file.with_suffix(".png")
    final_jpeg = plt_file.with_suffix(".jpeg")

    requested_outputs = []

    if "png" in formats:
        requested_outputs.append((out_png, final_png))

    if "jpeg" in formats:
        requested_outputs.append((out_jpeg, final_jpeg))

    if not requested_outputs:
        raise RuntimeError("No output formats requested.")

    if not overwrite:
        existing_outputs = [final for _, final in requested_outputs if final.exists()]
        if existing_outputs:
            existing_list = ", ".join(str(f) for f in existing_outputs)
            print(f"Skipping {plt_file}: output already exists: {existing_list}")
            return

    # Remove stale output files before running Tecplot.
    for output_file in (out_png, out_jpeg):
        if output_file.exists():
            output_file.unlink()

    cmd = (
        shlex.split(tecplot_cmd)
        + [
            "-b",
            "-p",
            str(macro_file),
            str(plt_file),
        ]
    )

    print(f"Processing {plt_file}")
    print("Command:", " ".join(shlex.quote(arg) for arg in cmd))

    if dry_run:
        return

    try:
        subprocess.run(cmd, check=True)
    except subprocess.CalledProcessError as err:
        print(f"ERROR: Tecplot failed for {plt_file}", file=sys.stderr)
        print("Command:", " ".join(shlex.quote(arg) for arg in cmd), file=sys.stderr)
        raise err

    for temporary_output, final_output in requested_outputs:
        if not temporary_output.exists():
            raise RuntimeError(
                f"Tecplot did not create {temporary_output} for {plt_file}"
            )

        if final_output.exists() and overwrite:
            final_output.unlink()

        temporary_output.rename(final_output)
        print(f"Created {final_output}")

    # Remove unrequested output files if the macro created them.
    if "png" not in formats and out_png.exists():
        out_png.unlink()
        print(f"Removed unrequested file {out_png}")

    if "jpeg" not in formats and out_jpeg.exists():
        out_jpeg.unlink()
        print(f"Removed unrequested file {out_jpeg}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Loop through all .plt files in the current directory, run Tecplot "
            "in batch mode using a macro, and rename out.png and/or out.jpeg "
            "to match the input .plt file name."
        )
    )

    parser.add_argument(
        "-m",
        "--macro",
        default="cut-y-plane-density.mcr",
        help="Tecplot macro file to execute. Default: cut-y-plane-density.mcr",
    )

    parser.add_argument(
        "-t",
        "--tecplot",
        default="tec360 -mesa",
        help="Tecplot executable and options. Default: 'tec360 -mesa'",
    )

    parser.add_argument(
        "-f",
        "--formats",
        nargs="+",
        choices=["png", "jpeg"],
        default=["png", "jpeg"],
        help=(
            "Output image formats to process. "
            "Use 'png', 'jpeg', or both. Default: png jpeg"
        ),
    )

    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Overwrite existing .png and/or .jpeg files.",
    )

    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the Tecplot commands without running them.",
    )

    args = parser.parse_args()

    macro_file = Path(args.macro)

    if not macro_file.exists():
        print(f"ERROR: macro file not found: {macro_file}", file=sys.stderr)
        sys.exit(1)

    formats = set(args.formats)

    plt_files = sorted(Path(".").glob("*.plt"))

    if not plt_files:
        print("No .plt files found in the current directory.")
        sys.exit(0)

    for plt_file in plt_files:
        try:
            run_tecplot_for_file(
                plt_file=plt_file,
                macro_file=macro_file,
                tecplot_cmd=args.tecplot,
                overwrite=args.overwrite,
                dry_run=args.dry_run,
                formats=formats,
            )
        except Exception as err:
            print(f"Failed on {plt_file}: {err}", file=sys.stderr)
            sys.exit(1)


if __name__ == "__main__":
    main()
