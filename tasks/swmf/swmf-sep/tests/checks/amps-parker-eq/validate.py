"""Shared physical-ASCII validator for the two self-contained AMPS adapters.

The parser accepts ordinary AMPS text headers and extracts every finite
numeric field. It deliberately rejects an empty file, an unknown nonnumeric
line, NaN/Inf and a changed physical file set. Metadata and timestamps are not
accepted as a substitute for a physical output file.
"""
from __future__ import annotations

import argparse
import json
import math
import re
import sys
from pathlib import Path

REAL = re.compile(r"(?<![A-Za-z0-9_])[+-]?(?:(?:\d+(?:\.\d*)?)|(?:\.\d+))(?:[EeDd][+-]?\d+)?(?![A-Za-z0-9_])")
HEADER = re.compile(r"^(?:[#;!]|title\b|variables?\b|zone\b|data\b|time\b|field\b|column\b|format\b|version\b|output\b)", re.I)


def values(path: Path) -> list[float]:
    try:
        text = path.read_text(encoding="utf-8", errors="strict")
    except (OSError, UnicodeError) as exc:
        raise ValueError(f"{path}: unreadable physical ASCII: {exc}") from exc
    if not text.strip():
        raise ValueError(f"{path}: empty physical output")
    out: list[float] = []
    for lineno, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line:
            continue
        found = REAL.findall(line)
        if found:
            nums = [float(t.replace("D", "E").replace("d", "e")) for t in found]
            if any(not math.isfinite(x) for x in nums):
                raise ValueError(f"{path}:{lineno}: non-finite physical value")
            out.extend(nums)
        elif not HEADER.match(line):
            raise ValueError(f"{path}:{lineno}: unrecognised nonnumeric output line {line[:80]!r}")
    if not out:
        raise ValueError(f"{path}: no physical numeric fields")
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    for flag in ("--reference", "--candidate", "--rubric", "--out"):
        ap.add_argument(flag, required=True)
    args = ap.parse_args()
    rubric = json.loads(Path(args.rubric).read_text(encoding="utf-8"))
    cmp = rubric["comparison"]
    atol, rtol = float(cmp["atol"]), float(cmp.get("rtol", 0.0))
    refroot, candroot = Path(args.reference), Path(args.candidate)
    details, failures = {}, []
    worst_abs = worst_scaled = 0.0
    for spec in cmp["files"]:
        rel = spec["path"]
        rp, cp = refroot / rel, candroot / rel
        if not rp.is_file() or not cp.is_file():
            failures.append(f"{rel}: missing on {'reference' if not rp.is_file() else 'candidate'}")
            continue
        try:
            rv, cv = values(rp), values(cp)
        except ValueError as exc:
            failures.append(str(exc))
            continue
        if len(rv) != len(cv):
            failures.append(f"{rel}: {len(cv)} physical values, reference has {len(rv)}")
            continue
        errors = [abs(c - r) for r, c in zip(rv, cv)]
        scaled = [e / (atol + rtol * abs(r)) for e, r in zip(errors, rv)]
        max_abs = max(errors, default=0.0)
        max_scaled = max(scaled, default=0.0)
        over = sum(x > 1.0 for x in scaled)
        details[rel] = {"values": len(rv), "max_abs_error": max_abs,
                        "max_scaled_error": max_scaled, "bound_fraction": max_scaled,
                        "atol": atol, "rtol": rtol, "values_over_bound": over}
        if over:
            failures.append(f"{rel}: {over} values exceed physical tolerance")
        worst_abs, worst_scaled = max(worst_abs, max_abs), max(worst_scaled, max_scaled)
    result = {
        "passed": not failures,
        "policy": "physical_ascii_pointwise",
        "status": "measurement_needed" if not failures else "failed",
        "atol": atol, "rtol": rtol, "distance": worst_abs,
        "max_scaled_error": worst_scaled, "bound_fraction": worst_scaled,
        "files": details,
        "reason": "all physical fields within tolerance" if not failures else "; ".join(failures),
    }
    Path(args.out).write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(result["reason"], file=sys.stderr)
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
