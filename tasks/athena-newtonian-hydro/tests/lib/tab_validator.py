"""Pure validator for Athena++ primitive-tab Harbor artifacts."""
from __future__ import annotations

import json
import math
from pathlib import Path
from typing import Any, Iterable

ARTIFACT = "primitive_tab.json"
VARIABLES = ["x1", "x2", "x3", "rho", "press", "vel1", "vel2", "vel3"]
PRIMITIVES = VARIABLES[3:]


class DuplicateKey(ValueError):
    pass


def _pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise DuplicateKey(f"duplicate key {key!r}")
        result[key] = value
    return result


def _constant(value: str) -> None:
    raise ValueError(f"non-standard JSON constant {value}")


def _load(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=_pairs,
                      parse_constant=_constant)


def _number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(float(value))


def _files(directory: Path) -> tuple[bool, str]:
    if not directory.is_dir():
        return False, "artifact directory is missing or not a directory"
    names = sorted(path.name for path in directory.iterdir())
    if names != [ARTIFACT]:
        return False, f"directory must contain exactly {ARTIFACT}"
    return True, ""


def _validate_document(document: Any, rubric: dict[str, Any]) -> tuple[bool, str]:
    if not isinstance(document, dict):
        return False, "artifact must be a JSON object"
    required = {
        "schema", "case", "dimensions", "variables", "primitive_fields",
        "source_format", "frames",
    }
    if set(document) != required:
        return False, "artifact has missing or unexpected top-level keys"
    if document["schema"] != rubric["artifact_schema"]:
        return False, "schema marker does not match rubric"
    if document["case"] != rubric["case"]:
        return False, "case marker does not match rubric"
    if document["dimensions"] != rubric["dimensions"]:
        return False, "dimensions do not match rubric"
    if document["variables"] != VARIABLES or document["primitive_fields"] != PRIMITIVES:
        return False, "primitive variable order is not the full declared order"
    if document["source_format"] != rubric["source_format"]:
        return False, "source format metadata does not match rubric"
    frames = document["frames"]
    if not isinstance(frames, list) or len(frames) != rubric["frame_count"]:
        return False, "wrong frame count"
    expected_rows = math.prod(rubric["dimensions"])
    expected_times = rubric["expected_times"]
    if len(expected_times) != len(frames):
        return False, "rubric time schedule is malformed"
    for index, frame in enumerate(frames):
        if not isinstance(frame, dict) or set(frame) != {"time", "cycle", "rows"}:
            return False, f"frame {index} has malformed keys"
        if not _number(frame["time"]) or abs(float(frame["time"]) - float(expected_times[index])) > rubric["schedule_abs_tolerance"]:
            return False, f"frame {index} has wrong or non-finite time"
        if not isinstance(frame["cycle"], int) or isinstance(frame["cycle"], bool) or frame["cycle"] < 0:
            return False, f"frame {index} has invalid cycle"
        rows = frame["rows"]
        if not isinstance(rows, list) or len(rows) != expected_rows:
            return False, f"frame {index} has wrong row count"
        for row_index, row in enumerate(rows):
            if not isinstance(row, list) or len(row) != len(VARIABLES):
                return False, f"frame {index} row {row_index} has wrong width"
            if not all(_number(value) for value in row):
                return False, f"frame {index} row {row_index} has a non-finite/non-numeric value"
    return True, ""


def _max_row_difference(reference: dict[str, Any], candidate: dict[str, Any]) -> float | None:
    try:
        values = []
        for ref_frame, cand_frame in zip(reference["frames"], candidate["frames"]):
            for ref_row, cand_row in zip(ref_frame["rows"], cand_frame["rows"]):
                values.extend(abs(float(a) - float(b)) for a, b in zip(ref_row, cand_row))
        return max(values) if values else 0.0
    except (KeyError, TypeError, ValueError, OverflowError):
        return None


def validate_dirs(reference_dirs: Iterable[str | Path], candidate_dirs: Iterable[str | Path],
                  rubric_path: str | Path) -> dict[str, Any]:
    """Compare one reference and one candidate directory without side effects."""
    refs = [Path(value) for value in reference_dirs]
    cands = [Path(value) for value in candidate_dirs]
    try:
        rubric = _load(Path(rubric_path))
        if not isinstance(rubric, dict):
            return {"passed": False, "reason": "rubric is not an object"}
        if len(refs) != 1 or len(cands) != 1:
            return {"passed": False, "reason": "exactly one reference and candidate directory required"}
        for label, directory in (("reference", refs[0]), ("candidate", cands[0])):
            ok, reason = _files(directory)
            if not ok:
                return {"passed": False, "reason": f"{label}: {reason}"}
        documents = []
        for label, directory in (("reference", refs[0]), ("candidate", cands[0])):
            try:
                document = _load(directory / ARTIFACT)
            except (OSError, UnicodeError, json.JSONDecodeError, DuplicateKey, ValueError) as exc:
                return {"passed": False, "reason": f"{label}: invalid strict JSON ({exc})"}
            ok, reason = _validate_document(document, rubric)
            if not ok:
                return {"passed": False, "reason": f"{label}: {reason}"}
            documents.append(document)
        reference, candidate = documents
        difference = _max_row_difference(reference, candidate)
        policy = rubric["comparison_policy"]
        if policy != "decision_pending_exact_identity":
            return {"passed": False, "reason": "unsupported non-provisional comparison policy", "policy": policy}
        passed = reference == candidate
        return {
            "passed": passed,
            "case": rubric["case"],
            "policy": policy,
            "max_abs_difference": difference,
            "reason": "exact identity under provisional decision-pending policy" if passed else "candidate differs under provisional identity policy",
            "final_policy": "owner decision pending; identity self-test is not a port/performance result",
        }
    except (OSError, UnicodeError, json.JSONDecodeError, DuplicateKey, KeyError, TypeError, ValueError) as exc:
        return {"passed": False, "reason": f"validator failure: {exc}"}
