#!/usr/bin/env python3
"""Run C13's complete offline package test from any working directory.

This wrapper deliberately imports no C13 implementation symbols.  Executing the
public ``--self-test`` interface verifies the same entry point used by CI and by
users, including reference-file discovery relative to run_C13.py.
"""

from pathlib import Path
import subprocess
import sys


RUNNER = Path(__file__).resolve().parents[1] / "run_C13.py"


def main() -> int:
    """Return the runner's status unchanged so failures propagate to CI."""
    completed = subprocess.run([sys.executable, str(RUNNER), "--self-test"])
    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())
