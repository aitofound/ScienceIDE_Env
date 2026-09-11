#!/usr/bin/env python3
"""Compile and run the header-only F3 boundary-event unit tests."""
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[1]
source = here / "test_trajectory_boundary.cpp"
with tempfile.TemporaryDirectory(prefix="f3-boundary-test-") as tmp:
    exe = Path(tmp) / "test_trajectory_boundary"
    cmd = ["g++", "-std=c++11", "-Wall", "-Wextra", "-pedantic",
           "-I", str(root), str(source), "-o", str(exe)]
    subprocess.check_call(cmd)
    subprocess.check_call([str(exe)])
