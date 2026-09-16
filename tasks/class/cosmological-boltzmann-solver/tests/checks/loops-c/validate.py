#!/usr/bin/env python3
"""Check loops-c: the PASS POLICY half of the check (pointwise, per column).

The official driver prints, for each of eleven omega_b values, one row per
multipole: the integer l followed by the raw lensed TT, EE and TE Cl. The four
columns are physically different quantities and are bounded separately, because
their magnitudes and their reproducibility differ by orders of magnitude:

    l    the multipole index; exact
    TT   the temperature spectrum
    EE   the E-mode polarization spectrum
    TE   the temperature-E-mode cross spectrum, which changes sign

Measured between the pinned default build and the same-input OPTFLAG=-O2 build,
the three spectra move by at most 5.0e-16, 8.2e-20 and 4.8e-18 respectively.
Those floors are absolute, not relative: each spectrum falls by four to eight
decades between its peak near l=2 and its oscillating tail near l=3000, and in
the tail the printed value is a cancellation residual whose sign the two builds
do not even agree on (measured: one TE value of 8.0e-20 moves by 89 percent
relative). A purely relative bound therefore cannot admit a legitimate
alternative build, and a purely absolute bound would have to be so wide that
most of the column is ungraded. Each spectrum column gets both terms, with the
absolute term set from its own measured floor, and the reason field reports how
many values the absolute term covers so the reviewer can see what is graded.

    python3 validate.py --reference DIR --candidate DIR --rubric rubric.json --out result.json
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path


def load(path: Path) -> list[float]:
    doc = json.loads(path.read_text(encoding="utf-8"))
    return [float(v) for v in doc["values"]]


def main() -> int:
    ap = argparse.ArgumentParser()
    for flag in ("--reference", "--candidate", "--rubric", "--out"):
        ap.add_argument(flag, required=True)
    a = ap.parse_args()
    rubric = json.loads(Path(a.rubric).read_text(encoding="utf-8"))
    comparison = rubric["comparison"]
    atol, rtol = float(comparison["atol"]), float(comparison.get("rtol", 0.0))
    groups = comparison["groups"]
    reference, candidate = Path(a.reference), Path(a.candidate)
    worst, worst_frac, failures, details = 0.0, 0.0, [], {}
    rel = "observable.json"
    ref_path, cand_path = reference / rel, candidate / rel
    if not ref_path.is_file() or not cand_path.is_file():
        failures.append(f"{rel}: missing on {'reference' if not ref_path.is_file() else 'candidate'}")
    else:
        r, c = load(ref_path), load(cand_path)
        if len(r) != len(c):
            failures.append(f"{rel}: value count {len(c)} differs from reference {len(r)}")
        elif not all(math.isfinite(x) for x in c):
            failures.append(f"{rel}: candidate contains non-finite values")
        else:
            # The stream is the concatenation of fixed-width rows; the row width
            # is the span of the columns the groups name.
            rows = max(col for g in groups for col in g["columns"]) + 1
            if len(r) % rows:
                failures.append(f"{rel}: {len(r)} values are not a whole number of {rows}-column rows")
            else:
                group_details = {}
                for group in groups:
                    gname = group["name"]
                    col = group["columns"][0]
                    gatol, grtol = float(group.get("atol", 0.0)), float(group.get("rtol", 0.0))
                    errs, gover, ungraded, ratios = [], 0, 0, []
                    for i in range(col, len(r), rows):
                        err = abs(c[i] - r[i])
                        bound = gatol + grtol * abs(r[i])
                        errs.append(err)
                        if bound == 0.0:
                            if err != 0.0:
                                gover += 1
                            ratios.append(0.0 if err == 0.0 else math.inf)
                        else:
                            ratios.append(err / bound)
                            if err > bound:
                                gover += 1
                        if gatol and abs(r[i]) <= gatol:
                            ungraded += 1
                    gmax = max(errs, default=0.0)
                    gfrac = max(ratios, default=0.0)
                    group_details[gname] = {
                        "columns": [col], "values": len(errs), "max_abs_error": gmax,
                        "values_over_bound": gover, "bound_fraction": gfrac,
                        "atol": gatol, "rtol": grtol,
                        "values_inside_the_absolute_term": ungraded,
                    }
                    if gover:
                        failures.append(f"{rel}/{gname}: {gover} values exceed atol={gatol:g} rtol={grtol:g}")
                    worst = max(worst, gmax)
                    worst_frac = max(worst_frac, gfrac)
                details[rel] = {
                    "values": sum(g["values"] for g in group_details.values()),
                    "max_abs_error": max((g["max_abs_error"] for g in group_details.values()), default=0.0),
                    "values_over_bound": sum(g["values_over_bound"] for g in group_details.values()),
                    "bound_fraction": max((g["bound_fraction"] for g in group_details.values()), default=0.0),
                    "groups": group_details,
                }
    passed = not failures
    result = {"passed": passed, "policy": "pointwise", "atol": atol, "rtol": rtol,
              "distance": worst, "bound_fraction": worst_frac, "files": details,
              "reason": "all graded values within bound" if passed else "; ".join(failures[:8])}
    Path(a.out).write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(result["reason"], file=__import__("sys").stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
