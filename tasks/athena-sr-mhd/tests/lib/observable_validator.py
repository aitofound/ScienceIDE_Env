#!/usr/bin/env python3
"""Strict, fail-closed CPU/candidate validator for SR-MHD observables."""
from __future__ import annotations
import json, math
from pathlib import Path
from typing import Any, Iterable

ARTIFACT = "observables.json"
SCHEMA = "athena-sr-mhd-observables/v1"
VARIABLES = ["x1", "x2", "x3", "rho", "press", "vel1", "vel2", "vel3", "Bcc1", "Bcc2", "Bcc3"]
ABS_TOL = 1e-12
REL_TOL = 1e-10
POLICY = "provisional_exact_identity_wiring; Jason final tolerances pending"

class DuplicateKey(ValueError): pass

def _pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    out: dict[str, Any] = {}
    for key, value in pairs:
        if key in out: raise DuplicateKey(f"duplicate key {key!r}")
        out[key] = value
    return out

def _constant(value: str) -> None: raise ValueError(f"non-standard JSON constant {value}")

def _load(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=_pairs, parse_constant=_constant)

def finite_tree(value: Any) -> bool:
    if isinstance(value, bool) or value is None or isinstance(value, str): return True
    if isinstance(value, (int, float)):
        try: return math.isfinite(float(value))
        except (TypeError, ValueError, OverflowError): return False
    if isinstance(value, list): return all(finite_tree(x) for x in value)
    if isinstance(value, dict): return all(isinstance(k, str) and finite_tree(v) for k, v in value.items())
    return False

def bounded(value: object, limit: int = 260) -> str:
    text = str(value).replace("\n", " ")
    return text if len(text) <= limit else text[:limit - 3] + "..."

def fail(reason: str, **extra: Any) -> dict[str, Any]:
    return {"passed": False, "policy": POLICY, "max_abs_difference": 0.0, "max_normalized_ratio": 0.0, "reason": bounded(reason), **extra}

def _artifact_file(directory: Path) -> tuple[Path | None, str]:
    if directory.is_symlink() or not directory.is_dir(): return None, "artifact directory missing or symlinked"
    try: names = sorted(path.name for path in directory.iterdir())
    except OSError as exc: return None, f"cannot inspect artifact directory: {exc}"
    if names != [ARTIFACT]: return None, f"artifact directory must contain exactly {ARTIFACT}"
    path = directory / ARTIFACT
    if path.is_symlink() or not path.is_file(): return None, "artifact must be a regular file"
    return path, ""

def _number(value: Any) -> bool:
    return not isinstance(value, bool) and isinstance(value, (int, float)) and finite_tree(value)

def _close(a: float, b: float) -> bool:
    return abs(a - b) <= ABS_TOL + REL_TOL * max(abs(a), abs(b))


def _stats(values: list[float]) -> dict[str, float]:
    return {"min": min(values), "max": max(values), "l1_mean": sum(abs(value) for value in values) / len(values)}


def _case_metadata(case: dict[str, Any], check: str) -> tuple[bool, str]:
    """Bind labels to the audited solver/deck topology, not just case names."""
    name = case["name"]
    if check == "sr-mhd-shock-family":
        parts = name.split("-")
        if len(parts) != 6 or parts[:3] != ["sr", "mhd", "shock"] or parts[4] != "case" or parts[3] not in {"hlld", "hlle", "llf"} or parts[5] not in {"1", "2", "3", "4"}:
            return False, "shock case name does not identify one audited solver/deck"
        number = int(parts[5])
        expected = {"solver": parts[3], "deck": f"athinput.mub_{number}", "dimensions": [400 if number in (1, 3) else 800, 1, 1]}
    elif check == "sr-mhd-linear-wave":
        if not name.startswith("sr-mhd-seven-wave-flag-") or name.rsplit("-", 1)[-1] not in {str(value) for value in range(7)}:
            return False, "linear-wave case does not identify one of wave flags 0..6"
        expected = {"solver": "hlld", "deck": "athinput.linear_wave", "dimensions": [8, 4, 4]}
    elif check == "sr-mhd-3d-seven-wave-ct-acceleration":
        if name != "sr-mhd-3d-seven-wave-ct-acceleration": return False, "acceleration case name malformed"
        expected = {"solver": "hlld", "deck": "athinput.acceleration", "dimensions": [128, 64, 64]}
    else:
        return False, "unknown check topology"
    for key, value in expected.items():
        if case[key] != value: return False, f"case {key} does not match audited check topology"
    return True, ""


def _validate_case(case: Any, *, frame_count: int | None = None, check: str | None = None) -> tuple[bool, str]:
    if not isinstance(case, dict): return False, "case must be an object"
    required = {"name", "solver", "deck", "dimensions", "variables", "frames"}
    if set(case) != required: return False, "case has missing or unexpected keys"
    if not all(isinstance(case[x], str) and case[x] for x in ("name", "solver", "deck")): return False, "case metadata malformed"
    dims = case["dimensions"]
    if not isinstance(dims, list) or len(dims) != 3 or any(isinstance(x, bool) or not isinstance(x, int) or x <= 0 for x in dims): return False, "case dimensions malformed"
    if case["variables"] != VARIABLES: return False, "case variables do not expose SR-MHD primitive and Bcc channels"
    if check is not None:
        ok, reason = _case_metadata(case, check)
        if not ok: return False, reason
    frames = case["frames"]
    if not isinstance(frames, list) or not frames: return False, "case frames missing"
    if frame_count is not None and len(frames) != frame_count: return False, "case frame count does not match rubric"
    expected_rows = math.prod(dims)
    for index, frame in enumerate(frames):
        if not isinstance(frame, dict) or set(frame) != {"time", "cycle", "rows", "observables"}: return False, f"frame {index} keys malformed"
        if not _number(frame["time"]) or not isinstance(frame["cycle"], int) or isinstance(frame["cycle"], bool) or frame["cycle"] < 0: return False, f"frame {index} time/cycle malformed"
        rows = frame["rows"]
        if not isinstance(rows, list) or len(rows) != expected_rows: return False, f"frame {index} row count is not dimensions product"
        for ridx, row in enumerate(rows):
            if not isinstance(row, list) or len(row) != len(VARIABLES) or not all(_number(x) for x in row): return False, f"frame {index} row {ridx} malformed/non-finite"
            if row[3] <= 0.0 or row[4] <= 0.0: return False, f"frame {index} row {ridx} non-positive rho/press"
        if not isinstance(frame["observables"], dict) or not finite_tree(frame["observables"]): return False, f"frame {index} observables malformed/non-finite"
        obs = frame["observables"]
        for key in ("finite_state", "admissibility", "recovery", "primitive_ranges", "magnetic", "face_B", "corner_EMF", "discrete_divB"):
            if key not in obs: return False, f"frame {index} missing observable {key}"
        # Every row is finite by construction; the emitted assertion must agree.
        if obs["finite_state"] is not True: return False, f"frame {index} finite_state assertion is not true"
        velocity_squared = [sum(row[5 + q] ** 2 for q in range(3)) for row in rows]
        failed_cells = sum(value >= 1.0 for value in velocity_squared)
        expected_admissibility = {
            "all_rho_positive": all(row[3] > 0.0 for row in rows),
            "all_pressure_positive": all(row[4] > 0.0 for row in rows),
            "all_sub_luminal": all(value < 1.0 for value in velocity_squared),
        }
        admissibility = obs["admissibility"]
        if not isinstance(admissibility, dict) or set(admissibility) != set(expected_admissibility): return False, f"frame {index} admissibility keys malformed"
        if any(type(admissibility[key]) is not bool for key in expected_admissibility): return False, f"frame {index} admissibility booleans malformed"
        if admissibility != expected_admissibility: return False, f"frame {index} admissibility booleans contradict emitted rows"
        recovery = obs["recovery"]
        if not isinstance(recovery, dict) or set(recovery) != {"status", "failed_cells", "retries_or_fallbacks", "lorentz_factor_max"}: return False, f"frame {index} recovery fields malformed"
        expected_status = "all_emitted_cells_admissible" if failed_cells == 0 else "emitted_state_contains_nonadmissible_cells"
        if recovery.get("status") != expected_status: return False, f"frame {index} recovery status contradicts row-derived admissibility"
        if not isinstance(recovery.get("failed_cells"), int) or isinstance(recovery.get("failed_cells"), bool) or recovery["failed_cells"] < 0: return False, f"frame {index} recovery failure count is malformed"
        if recovery["failed_cells"] != failed_cells: return False, f"frame {index} recovery failed_cells does not equal superluminal row count"
        if not isinstance(recovery.get("retries_or_fallbacks"), int) or isinstance(recovery.get("retries_or_fallbacks"), bool) or recovery["retries_or_fallbacks"] < 0: return False, f"frame {index} recovery retry count is malformed"
        expected_lorentz = max(1.0 if value >= 1.0 else 1.0 / math.sqrt(1.0 - value) for value in velocity_squared)
        if not _number(recovery.get("lorentz_factor_max")) or not _close(float(recovery["lorentz_factor_max"]), expected_lorentz): return False, f"frame {index} Lorentz-factor reduction contradicts rows"
        ranges = obs["primitive_ranges"]
        if not isinstance(ranges, dict) or set(ranges) != set(VARIABLES[3:]): return False, f"frame {index} primitive range keys malformed"
        for offset, name in enumerate(VARIABLES[3:], start=3):
            reduction = ranges[name]
            if not isinstance(reduction, dict) or set(reduction) != {"min", "max", "l1_mean"} or not all(_number(reduction[key]) for key in reduction): return False, f"frame {index} primitive range malformed for {name}"
            expected_range = _stats([row[offset] for row in rows])
            if any(not _close(float(reduction[key]), expected_range[key]) for key in expected_range): return False, f"frame {index} primitive range contradicts rows for {name}"
        if not isinstance(obs["face_B"], dict) or obs["face_B"].get("status") != "derived_projection_from_cell_centered_Bcc": return False, f"frame {index} face-B evidence is missing or mislabelled"
        if not isinstance(obs["corner_EMF"], dict) or obs["corner_EMF"].get("status") != "derived_ideal_E_projection_from_primitive_and_Bcc": return False, f"frame {index} corner-EMF evidence is missing or mislabelled"
        divb = obs["discrete_divB"]
        if not isinstance(divb, dict) or not _number(divb.get("l1_mean")) or not _number(divb.get("max_abs")): return False, f"frame {index} divB reduction malformed"
    return True, ""

def _compare_values(a: Any, b: Any, path: str, state: dict[str, float]) -> str | None:
    if isinstance(a, bool) or isinstance(b, bool) or isinstance(a, str) or isinstance(b, str) or a is None or b is None:
        return None if a == b else f"metadata mismatch at {path}"
    if isinstance(a, list) or isinstance(b, list):
        if not isinstance(a, list) or not isinstance(b, list) or len(a) != len(b): return f"shape mismatch at {path}"
        for i, (x, y) in enumerate(zip(a, b)):
            issue = _compare_values(x, y, f"{path}[{i}]", state)
            if issue: return issue
        return None
    if isinstance(a, dict) or isinstance(b, dict):
        if not isinstance(a, dict) or not isinstance(b, dict) or set(a) != set(b): return f"metadata keys mismatch at {path}"
        for key in sorted(a):
            issue = _compare_values(a[key], b[key], f"{path}.{key}", state)
            if issue: return issue
        return None
    if not _number(a) or not _number(b): return f"unsupported values at {path}"
    aa, bb = float(a), float(b)
    difference = abs(aa - bb)
    tolerance = ABS_TOL + REL_TOL * max(abs(aa), abs(bb))
    ratio = difference / tolerance if tolerance > 0 else float("inf")
    state["max_abs"] = max(state["max_abs"], difference)
    state["max_ratio"] = max(state["max_ratio"], ratio)
    if path.endswith(".time") and difference > 1e-12: return f"frame schedule mismatch at {path}"
    # Cycles are integer diagnostics and deliberately not compared.
    if ".cycle" in path: return None
    return None if ratio <= 1.0 else f"numeric mismatch at {path}"

def validate_dirs(reference_dirs: Iterable[str | Path], candidate_dirs: Iterable[str | Path], rubric_path: str | Path) -> dict[str, Any]:
    try:
        rubric = _load(Path(rubric_path))
        if not isinstance(rubric, dict) or not finite_tree(rubric): return fail("rubric is not strict finite JSON")
        if rubric.get("artifact_schema") != SCHEMA or rubric.get("variables") != VARIABLES: return fail("rubric schema or variables malformed")
        if rubric.get("comparison_policy") != POLICY: return fail("rubric comparison policy is not the provisional declared policy")
        expected_cases = rubric.get("case_names")
        expected_frames = rubric.get("frame_count")
        refs, cands = [Path(x) for x in reference_dirs], [Path(x) for x in candidate_dirs]
        if len(refs) != 1 or len(cands) != 1: return fail("exactly one reference and candidate directory required")
        paths = []
        for label, directory in (("reference", refs[0]), ("candidate", cands[0])):
            path, reason = _artifact_file(directory)
            if path is None: return fail(f"{label}: {reason}")
            paths.append(path)
        docs = []
        for label, path in zip(("reference", "candidate"), paths):
            try: document = _load(path)
            except Exception as exc: return fail(f"{label}: malformed strict JSON ({bounded(exc)})")
            if not finite_tree(document): return fail(f"{label}: non-finite value")
            if not isinstance(document, dict) or set(document) != {"schema", "check", "source_commit", "variables", "cases"}: return fail(f"{label}: top-level artifact keys malformed")
            if document["schema"] != SCHEMA or document["variables"] != VARIABLES or document["source_commit"] != rubric.get("source_commit"): return fail(f"{label}: schema/source metadata mismatch")
            if document["check"] != rubric.get("check"): return fail(f"{label}: check metadata mismatch")
            cases = document["cases"]
            if not isinstance(cases, list) or len(cases) != len(expected_cases or []): return fail(f"{label}: wrong case count")
            names = [c.get("name") if isinstance(c, dict) else None for c in cases]
            if names != expected_cases: return fail(f"{label}: case names/order mismatch")
            for case in cases:
                ok, reason = _validate_case(case, frame_count=expected_frames, check=rubric.get("check"))
                if not ok: return fail(f"{label}: {reason}")
            docs.append(document)
        state = {"max_abs": 0.0, "max_ratio": 0.0}
        issue = _compare_values(docs[0]["cases"], docs[1]["cases"], "cases", state)
        return {"passed": issue is None, "policy": POLICY, "max_abs_difference": state["max_abs"], "max_normalized_ratio": state["max_ratio"], "case_count": len(docs[0]["cases"]), "reason": "provisional exact-identity wiring comparison passed" if issue is None else bounded(issue)}
    except Exception as exc: return fail(f"validator exception ({bounded(exc)})")
