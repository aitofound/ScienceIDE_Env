#!/usr/bin/env python3
"""Write the trusted root manifest of one oracle/candidate result root.

`solution/solve.sh` calls this on the host after the container exits, when the
image and container identities of the execution are known.  The manifest binds
one result root to one role, one image, one container, one execution id and one
campaign run token, and records the hash of every check's decoded artifacts and
execution manifest.  The verifier cross-checks it against the per-check
`execution_manifest.json` files it covers.

This is trusted-workflow evidence: it authenticates which controlled execution
produced a root, not that arbitrary code is what it claims to be.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

sys.dont_write_bytecode = True
SCHEMA = "athena-mhd-root-manifest/v1"
PRODUCTS = ("mhd_state.json", "mhd_observables.json", "execution_manifest.json")


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--role", required=True, choices=("reference", "candidate"))
    parser.add_argument("--execution-id", required=True)
    parser.add_argument("--run-token", required=True)
    parser.add_argument("--run-id", default=None)
    parser.add_argument("--image", default=None)
    parser.add_argument("--image-id", default=None)
    parser.add_argument("--container", default=None)
    parser.add_argument("--container-id", default=None)
    parser.add_argument("--source-commit", required=True)
    args = parser.parse_args()
    root = args.root
    if root.is_symlink() or not root.is_dir():
        print(f"write_root_manifest: {root} is not a directory", file=sys.stderr)
        return 2
    cases = sorted(path.name for path in root.iterdir() if path.is_dir() and not path.is_symlink() and not path.name.startswith("."))
    results = {}
    for case in cases:
        results[case] = {}
        for name in PRODUCTS:
            path = root / case / name
            if path.is_symlink() or not path.is_file():
                print(f"write_root_manifest: {case} lacks {name}", file=sys.stderr)
                return 1
            results[case][name] = digest(path)
    document = {
        "schema": SCHEMA,
        "role": args.role,
        "execution_id": args.execution_id,
        "run_token": args.run_token,
        "run_id": args.run_id,
        "image": args.image,
        "image_id": args.image_id,
        "container": args.container,
        "container_id": args.container_id,
        "source_commit": args.source_commit,
        "checks": cases,
        "results": results,
        "limits": ("trusted-workflow evidence: this binds the root to one image, container, execution and campaign "
                   "token and to the hashes of its own products; it is not cryptographic proof of arbitrary code"),
    }
    with open(root / "root_manifest.json", "w", encoding="utf-8") as handle:
        json.dump(document, handle, indent=2, sort_keys=True)
        handle.write("\n")
    print(f"root_manifest={root / 'root_manifest.json'} role={args.role} checks={len(cases)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
