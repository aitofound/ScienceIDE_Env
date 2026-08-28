"""Pure, fail-closed validator for Athena++ primitive-tab Harbor artifacts."""
from __future__ import annotations

import errno
import json
import math
import os
import stat
from pathlib import Path
from typing import Any, Iterable

ARTIFACT = "primitive_tab.json"
SCHEMA = "athena-hydro-primitive-tab/v1"
VARIABLES = ["x1", "x2", "x3", "rho", "press", "vel1", "vel2", "vel3"]
PRIMITIVES = VARIABLES[3:]
POLICY = "option_a_pointwise_primitive_tolerance"
PROVISIONAL_POLICY = "provisional_parameterized"
COORDINATE_POLICY = "exact"
CYCLE_POLICY = "nonnegative_integer_diagnostic_not_compared"
ABS_TOLERANCE = 1e-12
REL_TOLERANCE = 1e-10
SOURCE_FORMAT = {
    "encoding": "UTF-8 JSON converted from Athena++ formatted TAB",
    "dtype": "float64",
    "ordering": "frame then native k-j-i TAB row order",
    "native_file_pattern": "<problem>.block<gid>.out<id>.<frame>.tab",
}
ARTIFACT_DECLARATION = {
    "file": ARTIFACT,
    "format": "strict UTF-8 JSON",
    "required_fields": VARIABLES,
    "row_order": "native Athena++ TAB k-j-i order within each frame",
    "dtype": "float64",
    "finite_values": True,
}


class DuplicateKey(ValueError):
    """Raised when strict JSON contains a duplicate object key."""


def _pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise DuplicateKey(f"duplicate key {key!r}")
        result[key] = value
    return result


def _constant(value: str) -> None:
    raise ValueError(f"non-standard JSON constant {value}")


def _load_stream(stream: Any) -> Any:
    return json.load(
        stream,
        object_pairs_hook=_pairs,
        parse_constant=_constant,
    )


def _load(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as stream:
        return _load_stream(stream)


def _finite_tree(value: Any) -> bool:
    """Reject every non-finite JSON number, including deeply nested metadata."""
    if isinstance(value, bool) or value is None or isinstance(value, str):
        return True
    if isinstance(value, (int, float)):
        try:
            return math.isfinite(float(value))
        except (OverflowError, TypeError, ValueError):
            return False
    if isinstance(value, list):
        return all(_finite_tree(item) for item in value)
    if isinstance(value, dict):
        return all(isinstance(key, str) and _finite_tree(item) for key, item in value.items())
    return False


def _number(value: Any) -> bool:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return False
    try:
        return math.isfinite(float(value))
    except (OverflowError, TypeError, ValueError):
        return False


def _bounded(value: object, limit: int = 280) -> str:
    text = str(value).replace("\n", " ")
    return text if len(text) <= limit else text[: limit - 3] + "..."


def _failure(reason: str, *, policy: str = "unknown", case: str | None = None, **extra: Any) -> dict[str, Any]:
    result: dict[str, Any] = {
        "passed": False,
        "policy": policy,
        "max_abs_primitive_difference": 0.0,
        "max_normalized_tolerance_ratio": 0.0,
        "max_abs_difference": 0.0,
        "reason": _bounded(reason),
    }
    if case is not None:
        result["case"] = case
    result.update(extra)
    return result


def _validate_rubric(rubric: Any) -> tuple[bool, str]:
    if not isinstance(rubric, dict) or not _finite_tree(rubric):
        return False, "rubric must be a finite strict JSON object"
    required = {
        "case", "dimensions", "frame_count", "expected_times",
        "schedule_abs_tolerance", "artifact_schema", "variables",
        "source_format", "comparison_policy", "coordinate_policy",
        "cycle_policy", "primitive_abs_tolerance", "primitive_rel_tolerance",
    }
    if not required.issubset(rubric):
        return False, "rubric is missing required schema or policy fields"
    if rubric["artifact_schema"] != SCHEMA or rubric["variables"] != VARIABLES:
        return False, "rubric schema or variable order is unsupported"
    if not isinstance(rubric["case"], str) or not rubric["case"]:
        return False, "rubric case is malformed"
    dimensions = rubric["dimensions"]
    if (
        not isinstance(dimensions, list)
        or len(dimensions) != 3
        or any(isinstance(value, bool) or not isinstance(value, int) or value <= 0 for value in dimensions)
    ):
        return False, "rubric dimensions are malformed"
    frame_count = rubric["frame_count"]
    if isinstance(frame_count, bool) or not isinstance(frame_count, int) or frame_count <= 0:
        return False, "rubric frame count is malformed"
    expected_times = rubric["expected_times"]
    if not isinstance(expected_times, list) or len(expected_times) != frame_count:
        return False, "rubric time schedule is malformed"
    if not all(_number(value) for value in expected_times):
        return False, "rubric time schedule contains a non-finite value"
    if not _number(rubric["schedule_abs_tolerance"]) or float(rubric["schedule_abs_tolerance"]) < 0.0:
        return False, "rubric schedule_abs_tolerance is malformed"
    if float(rubric["schedule_abs_tolerance"]) != 1e-12:
        return False, "rubric schedule tolerance is not the approved 1e-12"
    if rubric["source_format"] != SOURCE_FORMAT:
        return False, "rubric source format is malformed"
    if rubric.get("artifact") != ARTIFACT_DECLARATION:
        return False, "rubric artifact declaration is malformed"
    policy = rubric["comparison_policy"]
    if policy == POLICY:
        if not _number(rubric["primitive_abs_tolerance"]) or not _number(rubric["primitive_rel_tolerance"]):
            return False, "approved rubric primitive tolerances are malformed"
        if float(rubric["primitive_abs_tolerance"]) != ABS_TOLERANCE:
            return False, "rubric absolute primitive tolerance is not approved"
        if float(rubric["primitive_rel_tolerance"]) != REL_TOLERANCE:
            return False, "rubric relative primitive tolerance is not approved"
    elif policy == PROVISIONAL_POLICY:
        if rubric["primitive_abs_tolerance"] is not None or rubric["primitive_rel_tolerance"] is not None:
            return False, "provisional rubric must not assert a final numeric tolerance"
        params = rubric.get("comparison_parameters")
        if not isinstance(params, dict) or params.get("status") != "human_decision_required":
            return False, "provisional rubric must declare human parameter decision"
    else:
        return False, "rubric comparison policy is unsupported"
    if rubric["coordinate_policy"] != COORDINATE_POLICY:
        return False, "rubric coordinate policy is unsupported"
    if rubric["cycle_policy"] != CYCLE_POLICY:
        return False, "rubric cycle policy is unsupported"
    return True, ""


class _ArtifactSafetyError(RuntimeError):
    """Raised when an artifact cannot be inspected without pathname races."""


def _safe_stat(name: str, directory_fd: int) -> os.stat_result:
    """Stat a directory entry without following its final component."""
    stat_fn = getattr(os, "stat", None)
    if stat_fn is not None and stat_fn in getattr(os, "supports_dir_fd", ()):
        if stat_fn in getattr(os, "supports_follow_symlinks", ()):
            return stat_fn(name, dir_fd=directory_fd, follow_symlinks=False)
    lstat_fn = getattr(os, "lstat", None)
    if lstat_fn is not None and lstat_fn in getattr(os, "supports_dir_fd", ()):
        return lstat_fn(name, dir_fd=directory_fd)
    raise _ArtifactSafetyError("required descriptor no-follow stat primitive is unavailable")


def _open_artifact_document(directory: Path) -> Any:
    """Open and parse the sole artifact through one final-component-safe fd."""
    directory_flag = getattr(os, "O_DIRECTORY", None)
    nofollow_flag = getattr(os, "O_NOFOLLOW", None)
    open_fn = getattr(os, "open", None)
    listdir_fn = getattr(os, "listdir", None)
    if (
        directory_flag is None
        or nofollow_flag is None
        or open_fn is None
        or open_fn not in getattr(os, "supports_dir_fd", ())
        or listdir_fn is None
        or listdir_fn not in getattr(os, "supports_fd", ())
    ):
        raise _ArtifactSafetyError("required descriptor no-follow primitives are unavailable")

    directory_fd: int | None = None
    try:
        try:
            directory_fd = open_fn(
                directory,
                os.O_RDONLY | directory_flag | nofollow_flag,
            )
        except OSError as exc:
            if exc.errno == errno.ELOOP:
                raise _ArtifactSafetyError("artifact directory must not be a symlink") from exc
            raise _ArtifactSafetyError(
                f"artifact directory is missing or not a directory ({_bounded(exc)})"
            ) from exc

        try:
            names = sorted(listdir_fn(directory_fd))
        except OSError as exc:
            raise _ArtifactSafetyError(
                f"cannot inspect artifact directory ({_bounded(exc)})"
            ) from exc
        if names != [ARTIFACT]:
            raise _ArtifactSafetyError(f"directory must contain exactly {ARTIFACT}")

        try:
            artifact_stat = _safe_stat(ARTIFACT, directory_fd)
        except OSError as exc:
            raise _ArtifactSafetyError(
                f"cannot inspect artifact file ({_bounded(exc)})"
            ) from exc
        if stat.S_ISLNK(artifact_stat.st_mode):
            raise _ArtifactSafetyError("artifact file must not be a symlink")
        if not stat.S_ISREG(artifact_stat.st_mode):
            raise _ArtifactSafetyError("artifact file is missing or not a regular file")

        artifact_fd: int | None = None
        try:
            try:
                artifact_fd = open_fn(
                    ARTIFACT,
                    os.O_RDONLY | nofollow_flag | getattr(os, "O_CLOEXEC", 0),
                    dir_fd=directory_fd,
                )
            except OSError as exc:
                if exc.errno == errno.ELOOP:
                    raise _ArtifactSafetyError("artifact file must not be a symlink") from exc
                raise _ArtifactSafetyError(
                    f"cannot open artifact file safely ({_bounded(exc)})"
                ) from exc
            try:
                opened_stat = os.fstat(artifact_fd)
            except OSError as exc:
                raise _ArtifactSafetyError(
                    f"cannot inspect opened artifact file ({_bounded(exc)})"
                ) from exc
            if not stat.S_ISREG(opened_stat.st_mode):
                raise _ArtifactSafetyError("artifact file is missing or not a regular file")
            with os.fdopen(artifact_fd, "r", encoding="utf-8") as stream:
                artifact_fd = None
                return _load_stream(stream)
        finally:
            if artifact_fd is not None:
                os.close(artifact_fd)
    finally:
        if directory_fd is not None:
            os.close(directory_fd)


def _validate_document(document: Any, rubric: dict[str, Any]) -> tuple[bool, str]:
    if not isinstance(document, dict):
        return False, "artifact must be a JSON object"
    required = {
        "schema", "case", "dimensions", "variables", "primitive_fields",
        "source_format", "frames",
    }
    if set(document) != required:
        return False, "artifact has missing or unexpected top-level keys"
    if document["schema"] != SCHEMA or document["schema"] != rubric["artifact_schema"]:
        return False, "schema marker does not match rubric"
    if document["case"] != rubric["case"]:
        return False, "case marker does not match rubric"
    dimensions = document["dimensions"]
    if (
        not isinstance(dimensions, list)
        or any(isinstance(value, bool) or not isinstance(value, int) for value in dimensions)
        or dimensions != rubric["dimensions"]
    ):
        return False, "dimensions do not match rubric"
    if document["variables"] != VARIABLES or document["primitive_fields"] != PRIMITIVES:
        return False, "primitive variable order is not the full declared order"
    if document["source_format"] != SOURCE_FORMAT or document["source_format"] != rubric["source_format"]:
        return False, "source format metadata does not match rubric"
    frames = document["frames"]
    if not isinstance(frames, list) or len(frames) != rubric["frame_count"]:
        return False, "wrong frame count"
    expected_rows = math.prod(rubric["dimensions"])
    expected_times = rubric["expected_times"]
    for index, frame in enumerate(frames):
        if not isinstance(frame, dict) or set(frame) != {"time", "cycle", "rows"}:
            return False, f"frame {index} has malformed keys"
        if (
            not _number(frame["time"])
            or abs(float(frame["time"]) - float(expected_times[index])) > float(rubric["schedule_abs_tolerance"])
        ):
            return False, f"frame {index} has wrong or non-finite time"
        if (
            not isinstance(frame["cycle"], int)
            or isinstance(frame["cycle"], bool)
            or frame["cycle"] < 0
        ):
            return False, f"frame {index} has invalid cycle"
        rows = frame["rows"]
        if not isinstance(rows, list) or len(rows) != expected_rows:
            return False, f"frame {index} has wrong row count"
        for row_index, row in enumerate(rows):
            if not isinstance(row, list) or len(row) != len(VARIABLES):
                return False, f"frame {index} row {row_index} has wrong width"
            if not all(_number(value) for value in row):
                return False, f"frame {index} row {row_index} has a non-finite/non-numeric value"
            if row[3] <= 0.0 or row[4] <= 0.0:
                return False, f"frame {index} row {row_index} has non-positive rho or press"
    return True, ""


def _compare(reference: dict[str, Any], candidate: dict[str, Any], rubric: dict[str, Any]) -> dict[str, Any]:
    max_difference = 0.0
    max_ratio = 0.0
    coordinate_mismatch: tuple[int, int, int] | None = None
    policy = rubric["comparison_policy"]
    abs_tolerance = None
    rel_tolerance = None
    if policy == POLICY:
        abs_tolerance = float(rubric["primitive_abs_tolerance"])
        rel_tolerance = float(rubric["primitive_rel_tolerance"])
    else:
        # A provisional rubric is deliberately identity-only unless a future
        # human-approved parameter is supplied by the verifier environment.
        # This is self-test support, never a final scientific equivalence claim.
        params = rubric.get("comparison_parameters", {})
        abs_name, rel_name = params.get("abs_env"), params.get("rel_env")
        abs_text, rel_text = os.environ.get(abs_name or ""), os.environ.get(rel_name or "")
        if bool(abs_text) != bool(rel_text):
            return _failure("provisional comparison requires both tolerance parameters", policy=policy, case=rubric["case"])
        if abs_text and rel_text:
            try:
                abs_tolerance, rel_tolerance = float(abs_text), float(rel_text)
            except (TypeError, ValueError):
                return _failure("provisional comparison parameters are not numeric", policy=policy, case=rubric["case"])
            if not all(math.isfinite(v) and v >= 0.0 for v in (abs_tolerance, rel_tolerance)):
                return _failure("provisional comparison parameters are not finite/nonnegative", policy=policy, case=rubric["case"])
        else:
            abs_tolerance = 0.0
            rel_tolerance = 0.0
    try:
        for frame_index, (ref_frame, cand_frame) in enumerate(zip(reference["frames"], candidate["frames"])):
            for row_index, (ref_row, cand_row) in enumerate(zip(ref_frame["rows"], cand_frame["rows"])):
                for coordinate_index in range(3):
                    if ref_row[coordinate_index] != cand_row[coordinate_index] and coordinate_mismatch is None:
                        coordinate_mismatch = (frame_index, row_index, coordinate_index)
                for primitive_index in range(3, len(VARIABLES)):
                    ref_value = float(ref_row[primitive_index])
                    cand_value = float(cand_row[primitive_index])
                    difference = abs(cand_value - ref_value)
                    if not math.isfinite(difference):
                        return _failure(
                            "primitive difference overflowed finite diagnostics",
                            policy=POLICY,
                            case=rubric["case"],
                        )
                    tolerance = abs_tolerance + rel_tolerance * max(abs(ref_value), abs(cand_value))
                    if not math.isfinite(tolerance) or tolerance < 0.0:
                        return _failure(
                            "primitive tolerance became non-finite",
                            policy=policy,
                            case=rubric["case"],
                        )
                    # Provisional rubrics default to exact identity for the
                    # required self-test. Equal values have zero error; any
                    # nonzero difference with a zero tolerance fails closed.
                    if tolerance == 0.0:
                        if difference != 0.0:
                            return _failure(
                                "provisional identity comparison found a difference",
                                policy=policy,
                                case=rubric["case"],
                            )
                        ratio = 0.0
                    else:
                        ratio = difference / tolerance
                    if not math.isfinite(ratio):
                        return _failure(
                            "normalized tolerance ratio became non-finite",
                            policy=POLICY,
                            case=rubric["case"],
                        )
                    max_difference = max(max_difference, difference)
                    max_ratio = max(max_ratio, ratio)
    except (KeyError, IndexError, TypeError, ValueError, OverflowError) as exc:
        return _failure(
            f"comparison failed ({_bounded(exc)})",
            policy=POLICY,
            case=rubric["case"],
        )
    passed = coordinate_mismatch is None and max_ratio <= 1.0
    if coordinate_mismatch is not None:
        reason = (
            "coordinate mismatch at frame {}, row {}, coordinate {}"
            .format(*coordinate_mismatch)
        )
    elif passed and policy == PROVISIONAL_POLICY:
        reason = "provisional identity-only self-test passed; not final scientific equivalence"
    elif passed:
        reason = "approved Option A primitive tolerance passed"
    else:
        reason = "one or more primitive values exceed approved tolerance"
    return {
        "passed": passed,
        "case": rubric["case"],
        "policy": policy,
        "coordinate_policy": COORDINATE_POLICY,
        "cycle_policy": CYCLE_POLICY,
        "max_abs_primitive_difference": max_difference,
        "max_normalized_tolerance_ratio": max_ratio,
        # Retain the short legacy diagnostic key as a finite alias.
        "max_abs_difference": max_difference,
        "reason": reason,
    }


def validate_dirs(
    reference_dirs: Iterable[str | Path],
    candidate_dirs: Iterable[str | Path],
    rubric_path: str | Path,
) -> dict[str, Any]:
    """Compare one reference and one candidate directory without side effects."""
    refs = [Path(value) for value in reference_dirs]
    cands = [Path(value) for value in candidate_dirs]
    try:
        rubric_path_obj = Path(rubric_path)
        rubric = _load(rubric_path_obj)
        rubric_ok, rubric_reason = _validate_rubric(rubric)
        if not rubric_ok:
            return _failure(f"malformed rubric: {rubric_reason}")
        assert isinstance(rubric, dict)
        if len(refs) != 1 or len(cands) != 1:
            return _failure(
                "exactly one reference and candidate directory required",
                policy=POLICY,
                case=rubric["case"],
            )
        documents: list[dict[str, Any]] = []
        for label, directory in (("reference", refs[0]), ("candidate", cands[0])):
            try:
                document = _open_artifact_document(directory)
            except _ArtifactSafetyError as exc:
                return _failure(f"{label}: {exc}", policy=POLICY, case=rubric["case"])
            except (OSError, UnicodeError, json.JSONDecodeError, DuplicateKey, ValueError) as exc:
                return _failure(
                    f"{label}: invalid strict JSON ({_bounded(exc)})",
                    policy=POLICY,
                    case=rubric["case"],
                )
            if not _finite_tree(document):
                return _failure(
                    f"{label}: artifact contains a non-finite JSON number",
                    policy=POLICY,
                    case=rubric["case"],
                )
            ok, reason = _validate_document(document, rubric)
            if not ok:
                return _failure(f"{label}: {reason}", policy=POLICY, case=rubric["case"])
            documents.append(document)
        return _compare(documents[0], documents[1], rubric)
    except (OSError, UnicodeError, json.JSONDecodeError, DuplicateKey, KeyError, TypeError, ValueError) as exc:
        return _failure(f"validator failure: {_bounded(exc)}")
