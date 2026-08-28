#!/usr/bin/env python3
from pathlib import Path
import hashlib, json, sys
DEFINITIONS = "definitions_01.h"
INI = "pluto_01.ini"
def main(argv):
    if len(argv) != 2:
        raise SystemExit("usage: deck.py BUILD_DECK")
    root = Path(argv[1])
    required = [root / "init.c", root / DEFINITIONS, root / INI, root / "definitions.h", root / "pluto.ini"]
    if not all(path.is_file() for path in required):
        raise SystemExit("deck closure is incomplete")
    if (root / "definitions.h").read_bytes() != (root / DEFINITIONS).read_bytes():
        raise SystemExit("definitions.h alias mismatch")
    if (root / "pluto.ini").read_bytes() != (root / INI).read_bytes():
        raise SystemExit("pluto.ini alias mismatch")
    manifest = {"check": "C06", "family": "Test_Problems/HD/Sedov", "configuration": "01",
                "source_files": {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                                 for path in root.iterdir() if path.is_file()}}
    (root / "deck_manifest.json").write_text(json.dumps(manifest, sort_keys=True, indent=2) + "\n", encoding="utf-8")
if __name__ == "__main__":
    main(sys.argv)
