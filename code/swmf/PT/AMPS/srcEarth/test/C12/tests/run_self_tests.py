#!/usr/bin/env python3
"""Working-directory-independent entry point for the C12 offline self-test.

Keeping this wrapper separate makes C12 compatible with test drivers that scan
``tests/run_self_tests.py`` files, while all assertions remain next to the
production parser and comparison routines in ``run_C12.py``. The wrapper does
not import or duplicate C12 logic; it invokes the public ``--self-test``
contract exactly as a user or continuous-integration job would.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def main() -> int:
    runner = Path(__file__).resolve().parents[1] / "run_C12.py"
    return subprocess.call([sys.executable, str(runner), "--self-test"])


if __name__ == "__main__":
    raise SystemExit(main())
