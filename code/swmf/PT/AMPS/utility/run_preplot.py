#!/usr/bin/env python3

import argparse
import glob
import os
import subprocess
import sys


def expand_input_files(file_args):
    """
    Expand wildcard patterns and return a sorted unique list of files.
    """
    files = []

    for item in file_args:
        matched = glob.glob(item)

        if matched:
            files.extend(matched)
        else:
            files.append(item)

    # Remove duplicates while preserving sorted order
    files = sorted(set(files))

    return files


def run_preplot(preplot_cmd, infile, force=False, dry_run=False):
    """
    Run preplot for one input file.
    """
    if not os.path.exists(infile):
        print("ERROR: input file does not exist: {}".format(infile))
        return False

    if not infile.endswith(".dat"):
        print("WARNING: input file does not end with .dat: {}".format(infile))

    outfile = os.path.splitext(infile)[0] + ".plt"

    if os.path.exists(outfile) and not force:
        print("SKIP: {} already exists".format(outfile))
        return True

    cmd = [preplot_cmd, infile, outfile]

    print("RUN: {}".format(" ".join(cmd)))

    if dry_run:
        return True

    try:
        subprocess.run(cmd, check=True)
        return True
    except subprocess.CalledProcessError as e:
        print("ERROR: preplot failed for {} with return code {}".format(
            infile, e.returncode
        ))
        return False


def main():
    help_text = """
Examples:

  Run preplot for explicitly listed files:

    ./run_preplot.py file1.dat file2.dat file3.dat

  Run preplot for all matching AMPS coupled-data files:

    ./run_preplot.py amps_coupled_data.swmf_n*_t*.dat

  Use a specific preplot executable:

    ./run_preplot.py --preplot /path/to/preplot file1.dat file2.dat

  Force overwriting existing .plt files:

    ./run_preplot.py --force file1.dat file2.dat

  Print commands without running preplot:

    ./run_preplot.py --dry-run file1.dat file2.dat

Output:

  For each input file named

    file.dat

  the script creates

    file.plt

Notes:

  The script accepts multiple input files.
  Wildcards are allowed.
  Existing .plt files are skipped unless --force is used.
"""

    parser = argparse.ArgumentParser(
        description="Run Tecplot preplot for one or more AMPS .dat files.",
        epilog=help_text,
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument(
        "files",
        nargs="+",
        help="Input .dat files. Multiple files and wildcards are allowed."
    )

    parser.add_argument(
        "-p", "--preplot",
        default="preplot",
        help="Path to preplot executable. Default: preplot"
    )

    parser.add_argument(
        "-f", "--force",
        action="store_true",
        help="Overwrite existing .plt output files."
    )

    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print commands without running preplot."
    )

    args = parser.parse_args()

    files = expand_input_files(args.files)

    if not files:
        print("ERROR: no input files were provided.")
        return 1

    n_ok = 0
    n_fail = 0

    for infile in files:
        success = run_preplot(
            args.preplot,
            infile,
            force=args.force,
            dry_run=args.dry_run
        )

        if success:
            n_ok += 1
        else:
            n_fail += 1

    print("")
    print("Summary:")
    print("  successful/skipped : {}".format(n_ok))
    print("  failed             : {}".format(n_fail))

    if n_fail > 0:
        return 2

    return 0


if __name__ == "__main__":
    sys.exit(main())
