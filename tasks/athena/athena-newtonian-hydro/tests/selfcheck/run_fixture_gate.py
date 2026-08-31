#!/usr/bin/env python3
from pathlib import Path
import subprocess, sys
root=Path(__file__).resolve().parents[1]
raise SystemExit(subprocess.run([sys.executable, str(root/'selfcheck'/'check_catalog.py')], cwd=root.parent.parent).returncode)
