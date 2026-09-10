#!/usr/bin/env python3
"""Run full, history-frozen, and instantaneous-frozen TS05 morphology cases.

This wrapper guarantees that the three empirical-field experiments use the
same epoch list, shell grid, rigidity list, mover, and parallel controls.  Only
the driver file changes.  That isolation is essential for interpreting their
differences as sensitivity to the TS05 parameterization.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List

from study_common import default_output_root, load_config, resolve_output_path, sha256


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", choices=("SMOKE", "ROUTINE", "FULL"), default="SMOKE")
    parser.add_argument("--amps", type=Path, default=Path("./amps"))
    parser.add_argument("--mpirun", default="mpirun")
    parser.add_argument("-np", type=int, default=4)
    parser.add_argument("-nt", type=int, default=16)
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument(
        "--output-root", type=Path,
        default=default_output_root() / "ts05_sensitivity",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root, config = load_config()
    output = resolve_output_path(args.output_root)
    output.mkdir(parents=True, exist_ok=True)
    driver_root = output / "drivers"
    generator = [
        sys.executable, str(root / "scripts" / "make_ts05_sensitivity_drivers.py"),
        "--output-root", str(driver_root),
    ]
    if subprocess.run(generator, cwd=root).returncode != 0:
        return 1
    variants = {
        "full": root / config["data"]["driver"],
        "history_frozen": driver_root / "ts05_dec2006_history_frozen.txt",
        "instantaneous_frozen": driver_root / "ts05_dec2006_instantaneous_frozen.txt",
    }
    commands: Dict[str, List[str]] = {}
    return_codes: Dict[str, int] = {}
    for name, driver in variants.items():
        command = [
            sys.executable, str(root / "scripts" / "run_morphology.py"),
            "--profile", args.profile, "--driver", str(driver),
            "--amps", str(args.amps), "--mpirun", args.mpirun,
            "-np", str(args.np), "-nt", str(args.nt),
            "--output-root", str(output / name),
        ]
        if args.prepare_only:
            command.append("--prepare-only")
        commands[name] = command
        completed = subprocess.run(command, cwd=root)
        return_codes[name] = completed.returncode
        if completed.returncode:
            break
    if (not args.prepare_only and len(return_codes) == 3
            and all(code == 0 for code in return_codes.values())):
        compare_command = [
            sys.executable, str(root / "scripts" / "compare_ts05_sensitivity.py"),
            "--suite-root", str(output),
        ]
        commands["comparison"] = compare_command
        return_codes["comparison"] = subprocess.run(compare_command, cwd=root).returncode
    manifest = {
        "created_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "profile": args.profile, "prepare_only": args.prepare_only,
        "warning": "TS05 parameterization sensitivity; not self-consistent magnetospheres",
        "driver_sha256": {name: sha256(path) for name, path in variants.items()},
        "commands": commands, "return_codes": return_codes,
        "passed": (
            len([name for name in variants if name in return_codes]) == 3
            and all(code == 0 for code in return_codes.values())
        ),
    }
    (output / "sensitivity_suite_manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    return 0 if manifest["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
