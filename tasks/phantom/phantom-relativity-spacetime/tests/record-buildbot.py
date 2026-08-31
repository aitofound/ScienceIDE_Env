#!/usr/bin/env python3
"""Record source-owned buildbot artifacts without creating a science result."""
import hashlib
import json
import os
import sys
from pathlib import Path

PIN = "e53ea16758d2a261680506852a528f21270dca1c"
TREE = "ae40f54661feb12f0550092fd2188e5738b7b955"
BOT = "scripts/buildbot.sh"

def digest(path):
    h = hashlib.sha256()
    size = 0
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            size += len(block)
            h.update(block)
    return {"path": path.name, "size": size, "sha256": h.hexdigest()}

def manifest(root, names):
    entries = [digest(root / name) for name in names]
    entries.sort(key=lambda x: x["path"])
    stream = "".join(f"{x['path']}\t{x['size']}\t{x['sha256']}\n" for x in entries)
    return {"entries": entries, "sha256": hashlib.sha256(stream.encode()).hexdigest()}

def main():
    if len(sys.argv) != 8:
        raise SystemExit("usage: record-buildbot.py catalog check source-root output batch total exit")
    catalog, check, source_root, output, batch, total, exit_code = sys.argv[1:]
    source_root = Path(source_root)
    out = Path(output)
    rows = json.loads(Path(catalog).read_text(encoding="utf-8"))["checks"]
    row = next((x for x in rows if x["folder"] == f"checks/{check}"), None)
    if not row:
        raise SystemExit("unknown catalog row")
    official = row["official_test"]
    source_hashes = {}
    for rel, expected_hash in official.get("input_sha256", {}).items():
        source_path = source_root / rel
        if not source_path.is_file() or source_path.is_symlink():
            raise SystemExit(f"missing staged source input {rel}")
        observed_hash = digest(source_path)["sha256"]
        if observed_hash != expected_hash:
            raise SystemExit(f"staged source input hash mismatch {rel}")
        source_hashes[rel] = observed_hash
    required = ["myrun.setup", "myrun.in", "myrun_00000", "buildbot.log"]
    for name in required:
        p = out / name
        if not p.is_file() or p.is_symlink():
            raise SystemExit(f"missing artifact {name}")
    raw = []
    logs = out / "source-logs"
    if logs.is_dir():
        for p in sorted(logs.iterdir()):
            if p.is_file() and not p.is_symlink():
                raw.append(digest(p))
    artifact = manifest(out, required)
    byname = {x["path"]: x["sha256"] for x in artifact["entries"]}
    record = {
        "schema": "phantom-buildbot-smoke/v1",
        "check": check,
        "setup": official["registration"]["target"],
        "metric": official["metric"],
        "source_commit": PIN,
        "source_tree": TREE,
        "source_input_sha256": source_hashes,
        "official_test_ref": f"tests/checks.json#{check}",
        "registration": official["registration"],
        "invocation": {
            "program": BOT,
            "argv": ["./buildbot.sh", "--parallel", batch, total],
            "working_directory": "scripts",
            "environment": {
                "SYSTEM": "gfortran",
                "RETURN_ERR": "yes",
                "source_defined": ["MESAEOS=no", "NOWARN=yes", "setup DEBUG=yes"],
            },
        },
        "execution": {
            "attested": True,
            "exit_code": int(exit_code),
            "raw_stdout_stderr": "buildbot.log",
            "source_log_directory": "source-logs",
            "process_record": {"pid": os.getpid(), "wrapper": "tests/run-check.sh"},
        },
        "generated_inputs": [
            {"path": n, "sha256": byname[n]} for n in required[:2]
        ],
        "generated_dump": {
            "path": "myrun_00000",
            "sha256": byname["myrun_00000"],
            "nonempty": True,
        },
        "artifact_manifest": artifact,
        "raw_source_logs": raw,
        "tolerance_source": {"kind": "none", "policy": "source-owned completion only"},
        "result": {
            "source_buildbot_exit_code": int(exit_code),
            "source_nfail_policy": "RETURN_ERR=yes",
            "status": "passed" if int(exit_code) == 0 else "failed",
        },
    }
    (out / "official-test.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n", encoding="utf-8")

if __name__ == "__main__":
    main()
