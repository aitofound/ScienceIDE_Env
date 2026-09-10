#!/usr/bin/env python3
"""Run C7 with the bundled IZMIRAN/T96 reference solution.

This Python entry point follows the same invocation convention as the other
Earth validation tests.  Run it from the directory containing the AMPS
executable, normally the repository root:

    python3 srcEarth/test/C7/run_C7_izmiran.py -np 4 -nt 16

Examples:

    # Validate the bundled 28-point reference without launching AMPS.
    python3 srcEarth/test/C7/run_C7_izmiran.py --validate-references

    # Preview the generated GRIDLESS input and MPI command.
    python3 srcEarth/test/C7/run_C7_izmiran.py --dry-run -np 4

    # Run the default GRIDLESS comparison.
    python3 srcEarth/test/C7/run_C7_izmiran.py -np 4

    # Run Mode3D with POSIX-thread parallel background-field initialization.
    python3 srcEarth/test/C7/run_C7_izmiran.py --solver GRIDDED \
        -mode3d-mesh-coarsening LINEAR -np 4 -nt 16 \
        --mode3d-parallel-field-init

    # Run both field-evaluation paths.
    python3 srcEarth/test/C7/run_C7_izmiran.py --solver BOTH -np 4 -nt 16

All command-line options are implemented by run_C7.py and are forwarded
unchanged.  The defaults already select the bundled IZMIRAN reference, source
IZMIRAN, and model T96.
"""

from __future__ import annotations

import sys
from pathlib import Path

# Make the sibling full runner importable even when this file is launched by an
# absolute path or from a directory other than srcEarth/test/C7.
SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from run_C7 import main  # noqa: E402


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
