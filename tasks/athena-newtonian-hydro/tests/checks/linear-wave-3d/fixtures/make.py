#!/usr/bin/env python3
"""Generate deterministic accepts and rejects for the 8^3 check."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "lib"))
from make_fixture import main  # noqa: E402

if __name__ == "__main__":
    raise SystemExit(main())
