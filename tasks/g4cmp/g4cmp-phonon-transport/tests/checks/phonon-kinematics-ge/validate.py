#!/usr/bin/env python3
"""Check phonon-kinematics-ge: the PASS POLICY half of the check (pointwise, assignment-invariant for the transverse pair).

Every value of the nine tables written by tools/phononKinematics is compared pointwise. Each row is
one vector (a velocity in m/s or a slowness in s/m), and a component's bound is rtol times the length
of its own row vector plus atol, |cand - ref| <= atol + rtol * ||ref_row||, so that a component that
is small only because the vector is nearly perpendicular to that axis is not held to a relative bound
on its own tiny value. One further physical refinement. The two transverse modes are
labelled slow and fast by sorting their phase velocities, and along the acoustic axes of a cubic
crystal those two velocities are degenerate: a change of a few parts per million in an elastic
constant, or a different but correct eigensolver, flips which sheet is called which, and the two
group-velocity vectors swap with the labels. The label is a storage convention there, not physics,
so for every direction the two transverse group-velocity files are compared under both assignments
(slow<->slow, fast<->fast, and swapped) and the assignment with the smaller error is taken. Phase
velocity and slowness are equal at the degeneracy and need no such care.

Standard library and numpy only; reads only this check directory. Writes a result with "passed",
"reason", "distance" (largest relative deviation seen) and "bound_fraction" (the largest fraction of
its bound used by any graded value; its reciprocal is the headroom the presentation prints).

    python3 validate.py --reference DIR --candidate DIR --rubric rubric.json --out result.json
    python3 validate.py --selftest DIR      # DIR holds one run's nine files; must pass on a swapped
                                            # copy and fail on a copy with a scaled transverse sheet
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np

MAT = "Ge"
PAIR = (f"{MAT}_phonon_group_vel_trans_slow", f"{MAT}_phonon_group_vel_trans_fast")


def load(path: Path) -> np.ndarray:
    return np.loadtxt(path, delimiter=",", comments="#", ndmin=2).astype(np.float64)


def frac(a: np.ndarray, b: np.ndarray, atol: float, rtol: float) -> np.ndarray:
    """Per-row largest fraction of the bound used. Each row is one vector (velocity or slowness);
    the bound of a component is rtol times the vector's own length plus atol, so that a component
    that is small only because the vector is nearly perpendicular to that axis is not held to a
    relative bound on its own tiny value."""
    scale = np.linalg.norm(a, axis=1, keepdims=True)
    bound = atol + rtol * scale
    return (np.abs(a - b) / bound).max(axis=1)


def compare(reference: Path, candidate: Path, rubric: dict) -> dict:
    comparison = rubric["comparison"]
    atol, rtol = float(comparison["atol"]), float(comparison.get("rtol", 0.0))
    failures, details, worst_frac, worst_dist = [], {}, 0.0, 0.0
    tables = {}
    for spec in comparison["files"]:
        rel = spec["path"]
        pair = {}
        for label, root in (("reference", reference), ("candidate", candidate)):
            p = root / rel
            if not p.is_file():
                failures.append(f"{rel}: {label} is missing")
                continue
            try:
                pair[label] = load(p)
            except (OSError, ValueError) as exc:
                failures.append(f"{rel}: {label}: cannot load: {exc}")
        if len(pair) != 2:
            continue
        r, c = pair["reference"], pair["candidate"]
        if r.shape != c.shape:
            failures.append(f"{rel}: shape {c.shape} differs from reference {r.shape}")
            continue
        if not (np.all(np.isfinite(r)) and np.all(np.isfinite(c))):
            failures.append(f"{rel}: non-finite values")
            continue
        tables[rel] = (r, c)
    if all(name in tables for name in PAIR):
        (rs, cs), (rf, cf) = tables[PAIR[0]], tables[PAIR[1]]
        direct = np.maximum(frac(rs, cs, atol, rtol), frac(rf, cf, atol, rtol))
        swapped = np.maximum(frac(rs, cf, atol, rtol), frac(rf, cs, atol, rtol))
        best = np.minimum(direct, swapped)
        n_swapped = int(np.sum((swapped < direct) & (direct > 1.0)))
        for name in PAIR:
            del tables[name]
        over = int(np.sum(best > 1.0))
        worst_frac = max(worst_frac, float(best.max()))
        worst_dist = max(worst_dist, float(best.max() * rtol))
        details["transverse-group-velocity-pair"] = {"rows": int(best.size), "rows_over_bound": over,
                                                     "rows_taken_swapped": n_swapped, "bound_fraction": float(best.max())}
        if over:
            failures.append(f"transverse group velocities: {over} of {best.size} directions exceed the bound under either assignment")
    for rel, (r, c) in tables.items():
        f = frac(r, c, atol, rtol)
        over = int(np.sum(f > 1.0))
        worst_frac = max(worst_frac, float(f.max()))
        dist = float(f.max() * rtol)   # relative to the row's vector length, the scale the bound uses
        worst_dist = max(worst_dist, dist)
        details[rel] = {"values": int(r.size), "rows_over_bound": over, "max_rel_error": dist, "bound_fraction": float(f.max())}
        if over:
            failures.append(f"{rel}: {over} rows exceed atol={atol:g} rtol={rtol:g}")
    passed = not failures
    return {"passed": passed, "policy": "pointwise", "atol": atol, "rtol": rtol, "distance": worst_dist,
            "bound_fraction": worst_frac, "reason": "all graded values within the bound" if passed else "; ".join(failures),
            "details": details}


def selftest(run: Path, rubric: dict) -> int:
    import shutil, tempfile
    files = [s["path"] for s in rubric["comparison"]["files"]]
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        for case in ("identical", "swapped", "scaled"):
            d = tmp / case; d.mkdir()
            for f in files:
                shutil.copy(run / f, d / f)
            if case == "swapped":   # the transverse pair with labels exchanged on every row: a correct port may do this
                shutil.copy(run / PAIR[0], d / PAIR[1]); shutil.copy(run / PAIR[1], d / PAIR[0])
            if case == "scaled":    # one transverse sheet 1 per cent faster: a wrong elastic constant; must fail
                a = load(run / PAIR[1]) * 1.01
                np.savetxt(d / PAIR[1], a, delimiter=", ", fmt="%.6g")
            res = compare(run, d, rubric)
            want = case != "scaled"
            print(f"selftest {case}: passed={res['passed']} bound_fraction={res['bound_fraction']:.3g} (expected passed={want}) {'' if res['passed'] else res['reason'][:120]}")
            if res["passed"] != want:
                return 1
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--reference"); ap.add_argument("--candidate"); ap.add_argument("--rubric"); ap.add_argument("--out")
    ap.add_argument("--selftest")
    a = ap.parse_args()
    rubric_path = Path(a.rubric) if a.rubric else Path(__file__).with_name("rubric.json")
    rubric = json.loads(rubric_path.read_text(encoding="utf-8"))
    if a.selftest:
        return selftest(Path(a.selftest), rubric)
    if not (a.reference and a.candidate and a.out):
        ap.error("--reference, --candidate and --out are required")
    result = compare(Path(a.reference), Path(a.candidate), rubric)
    Path(a.out).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(("PASS" if result["passed"] else "FAIL") + ": " + result["reason"])
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
