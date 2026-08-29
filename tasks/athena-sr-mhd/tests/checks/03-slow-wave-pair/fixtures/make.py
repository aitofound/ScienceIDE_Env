#!/usr/bin/env python3
from __future__ import annotations
import copy
import json
import os
import subprocess
import sys
from pathlib import Path

def write(path, document):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(document, sort_keys=True, separators=(",", ":"), allow_nan=False) + "\n", encoding="utf-8")

def main():
    if len(sys.argv) != 2:
        raise SystemExit("fixtures/make.py OUTPUT_ROOT")
    output = Path(sys.argv[1])
    output.mkdir(parents=True, exist_ok=True)
    check = Path(__file__).resolve().parents[1]
    reference = output / "reference"
    environment = {**os.environ, "ATHENA_OUTPUT_DIR": str(reference), "ATHENA_SOURCE_DIR": "/does/not/exist"}
    completed = subprocess.run(["bash", str(check / "run.sh")], env=environment, capture_output=True, text=True)
    if completed.returncode:
        raise SystemExit("runner failed: " + completed.stderr)
    document = json.loads((reference / "observables.json").read_text(encoding="utf-8"))
    write(output / "accept-identity" / "observables.json", document)
    near_miss = copy.deepcopy(document)
    near_miss["cases"][0]["id"] = "tampered-near-miss"
    write(output / "reject-near-miss" / "observables.json", near_miss)
    (output / "reject-malformed").mkdir(parents=True, exist_ok=True)
    (output / "reject-malformed" / "observables.json").write_text('{"schema":', encoding="utf-8")
    (output / "reject-missing").mkdir(parents=True, exist_ok=True)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
