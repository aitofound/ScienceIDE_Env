#!/usr/bin/env python3
"""Shared candidate/reference comparison; all science bounds remain pending."""
from pathlib import Path
import json, os
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from output_contract import ContractError, parse_native

def _one(value):
    return [Path(value)] if isinstance(value, (str, os.PathLike, Path)) else [Path(item) for item in value]

def fail(check, outcome, error, **extra):
    result = {"check": check, "passed": False, "outcome": outcome, "error": error}
    result.update(extra)
    return result

def validate_check(here, check, reference, candidate):
    refs, cands = _one(reference), _one(candidate)
    if len(refs) != 1 or len(cands) != 1:
        return fail(check, "wrong_replication_count", "deterministic check needs one reference/candidate pair")
    try:
        rubric = json.loads((Path(here) / "rubric.json").read_text(encoding="utf-8"))
    except Exception as exc:
        return fail(check, "authoring_error", f"cannot read rubric: {exc}")
    try:
        ref = parse_native(refs[0])
    except ContractError as exc:
        return fail(check, "authoring_error", "malformed reference: " + str(exc))
    try:
        cand = parse_native(cands[0])
    except ContractError as exc:
        return fail(check, "wrong_output_contract", str(exc))
    observation_rule = rubric.get("runtime_observations", {})
    required = observation_rule.get("required", [])
    expected = observation_rule.get("expected", {})
    for label, artifact in (("reference", ref), ("candidate", cand)):
        observed = artifact["observations"].get("observed", {})
        for key in required:
            if observed.get(key) != expected.get(key):
                return fail(check, "missing_runtime_observation",
                            f"{label} lacks explicit {key}={expected.get(key)!r}")
    if ref["variables"] != cand["variables"]:
        return fail(check, "wrong_schema", "variable order differs")
    if ref["ncell"] != cand["ncell"] or len(ref["frames"]) != len(cand["frames"]):
        return fail(check, "wrong_shape", "frame or cell count differs")
    comparison = rubric["comparison"]
    fixture_bound = float(comparison["fixture_tolerance_abs"])
    time_bound = float(comparison.get("fixture_time_tolerance_abs", fixture_bound))
    worst = {"error": 0.0}
    identical = True
    for ref_frame, cand_frame in zip(ref["frames"], cand["frames"]):
        if ref_frame["frame"] != cand_frame["frame"] or ref_frame["step"] != cand_frame["step"]:
            return fail(check, "wrong_step_or_frame", "frame or exact step differs")
        if abs(ref_frame["time"] - cand_frame["time"]) > time_bound or abs(ref_frame["dt"] - cand_frame["dt"]) > time_bound:
            return fail(check, "wrong_time_base", "time/dt exceeds synthetic fixture bound")
        if ref_frame["frame"] == 0:
            continue
        for variable, ref_values, cand_values in zip(ref["variables"], ref_frame["payload"]["fields"], cand_frame["payload"]["fields"]):
            for cell, (ref_value, cand_value) in enumerate(zip(ref_values, cand_values)):
                error = abs(ref_value - cand_value)
                if error != 0:
                    identical = False
                if error > worst.get("error", 0.0):
                    worst = {"error": error, "frame": ref_frame["frame"], "variable": variable, "cell": cell}
                if error > fixture_bound:
                    return fail(check, "diverged", "pointwise post-initial field exceeds synthetic fixture bound",
                                worst=worst, bound=fixture_bound)
    calibration = rubric.get("calibration", {})
    # A deterministic/noiseless byte-identical result receives full pass as an
    # informational warning even while the row's science tolerance is still
    # pending calibration (AGENTS.md); it never promotes the provisional
    # fixture_tolerance_abs bound itself into a calibrated science tolerance.
    calibrated = calibration.get("status") == "calibrated" or identical
    result = {"check": check, "passed": True, "calibrated": calibrated,
              "outcome": "passed_fixture_predicate", "bound": fixture_bound, "worst": worst,
              "calibration_status": calibration.get("status", "unknown")}
    if identical:
        result["warning"] = "reference and candidate are byte-identical"
    return result
