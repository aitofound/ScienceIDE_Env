"""Pure, fail-closed validator for Athena++ primitive-tab Harbor artifacts.

Policy is check-owned: every bound is read from the subcase rubric.
``option_a_pointwise_primitive_tolerance`` is the human-approved policy for the
three exact anchor decks, which are the only *active*, scored subcases.  Every
other subcase is ``inactive_owner_decision_pending``: it has no approved
acceptance rule, is never scored, and cannot earn credit in any mode, including
the Docker self-test.  For those rows the validator still reports whether the
two roots were bit-identical, but purely as an unscored determinism diagnostic.
The frame schedule is bound to the exact printed ``%e`` token the check owns
(see ``tests/lib/provenance.py``), not to a tolerance, and no environment
variable can alter any bound.
"""
from __future__ import annotations

import errno
import json
import math
import os
import stat
from pathlib import Path
from typing import Any, Iterable

from extract_tab import PRIMITIVES, SCHEMA, SOURCE_FORMAT, VARIABLES

ARTIFACT = "primitive_tab.json"
POLICY = "option_a_pointwise_primitive_tolerance"
INACTIVE_POLICY = "inactive_owner_decision_pending"
COORDINATE_POLICY = "exact"
CYCLE_POLICY = "nonnegative_integer_diagnostic_not_compared"
SCHEDULE_POLICY = "exact_printed_time_token"
ABS_TOLERANCE = 1e-12
REL_TOLERANCE = 1e-10
ARTIFACT_DECLARATION = {
    "file": ARTIFACT,
    "format": "strict UTF-8 JSON",
    "required_fields": VARIABLES,
    "row_order": "global k-j-i order: cells sorted by (x3v, x2v, x1v) across all MeshBlocks",
    "dtype": "float64",
    "finite_values": True,
}
STATUS_APPROVED = "approved_option_a"
STATUS_INACTIVE = "inactive_owner_decision_pending"
STATUS_FAILED = "failed"


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
    return json.load(stream, object_pairs_hook=_pairs, parse_constant=_constant)


def _load(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as stream:
        return _load_stream(stream)


def _finite_tree(value: Any) -> bool:
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


def _failure(reason: str, *, policy: str = "unknown", case: str | None = None, status: str = STATUS_FAILED, **extra: Any) -> dict[str, Any]:
    result: dict[str, Any] = {
        "passed": False,
        "scored": policy == POLICY,
        "status": status,
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
        "case", "status", "scored", "dimensions", "frame_count", "expected_times",
        "expected_time_tokens", "schedule_policy", "artifact_schema", "variables",
        "source_format", "comparison_policy", "coordinate_policy",
        "cycle_policy", "primitive_abs_tolerance", "primitive_rel_tolerance",
    }
    if not required.issubset(rubric):
        return False, "rubric is missing required schema or policy fields"
    if rubric["status"] not in ("active", "inactive"):
        return False, "rubric status must be active or inactive"
    if rubric["scored"] is not (rubric["status"] == "active"):
        return False, "rubric scored flag disagrees with its status"
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
    rows = rubric.get("expected_rows", math.prod(dimensions))
    if isinstance(rows, bool) or not isinstance(rows, int) or rows <= 0:
        return False, "rubric expected_rows is malformed"
    frame_count = rubric["frame_count"]
    if isinstance(frame_count, bool) or not isinstance(frame_count, int) or frame_count <= 0:
        return False, "rubric frame count is malformed"
    expected_times = rubric["expected_times"]
    if not isinstance(expected_times, list) or len(expected_times) != frame_count:
        return False, "rubric time schedule is malformed"
    if not all(_number(value) for value in expected_times):
        return False, "rubric time schedule contains a non-finite value"
    if any(float(b) <= float(a) for a, b in zip(expected_times, expected_times[1:])):
        return False, "rubric time schedule is not strictly increasing"
    if rubric["schedule_policy"] != SCHEDULE_POLICY:
        return False, "rubric schedule policy is unsupported"
    tokens = rubric["expected_time_tokens"]
    if not isinstance(tokens, list) or len(tokens) != frame_count:
        return False, "rubric printed-token schedule is malformed"
    for token, value in zip(tokens, expected_times):
        # Athena++ prints the frame time with "%e" (formatted_table.cpp:85), so the
        # token is the representation itself and no schedule tolerance is needed.
        if not isinstance(token, str) or "%.6e" % float(value) != token:
            return False, "rubric printed-token schedule does not render its own frame times"
    if rubric["source_format"] != SOURCE_FORMAT:
        return False, "rubric source format is malformed"
    if rubric.get("artifact") != ARTIFACT_DECLARATION:
        return False, "rubric artifact declaration is malformed"
    policy = rubric["comparison_policy"]
    if policy == POLICY:
        if rubric["status"] != "active":
            return False, "only an active rubric may carry the approved Option A policy"
        if rubric.get("anchor_deck") in (None, ""):
            return False, "an approved Option A rubric must name its exact anchor deck"
        if not _number(rubric["primitive_abs_tolerance"]) or not _number(rubric["primitive_rel_tolerance"]):
            return False, "approved rubric primitive tolerances are malformed"
        if float(rubric["primitive_abs_tolerance"]) != ABS_TOLERANCE:
            return False, "rubric absolute primitive tolerance is not approved"
        if float(rubric["primitive_rel_tolerance"]) != REL_TOLERANCE:
            return False, "rubric relative primitive tolerance is not approved"
    elif policy == INACTIVE_POLICY:
        if rubric["status"] != "inactive":
            return False, "an owner-pending rubric may not be declared active"
        if rubric["primitive_abs_tolerance"] is not None or rubric["primitive_rel_tolerance"] is not None:
            return False, "inactive rubric must not assert a numeric tolerance"
        params = rubric.get("comparison_parameters")
        if not isinstance(params, dict) or params.get("status") != "human_decision_required":
            return False, "inactive rubric must declare the human parameter decision it needs"
        if params.get("scored") is not False or params.get("runtime_override") != "forbidden":
            return False, "inactive rubric must be unscored with no runtime override"
        if any(key in params for key in ("abs_env", "rel_env", "self_test_identity_only")):
            return False, "inactive rubric must not offer a runtime or self-test tolerance path"
    else:
        return False, "rubric comparison policy is unsupported"
    if rubric["coordinate_policy"] != COORDINATE_POLICY:
        return False, "rubric coordinate policy is unsupported"
    if rubric["cycle_policy"] != CYCLE_POLICY:
        return False, "rubric cycle policy is unsupported"
    floor = rubric.get("floor_witness")
    if floor is not None:
        if not isinstance(floor, dict) or not _number(floor.get("dfloor")) or not _number(floor.get("pfloor")):
            return False, "rubric floor_witness is malformed"
        if float(floor["dfloor"]) <= 0.0 or float(floor["pfloor"]) <= 0.0:
            return False, "rubric floor_witness floors must be positive"
        frame = floor.get("frame")
        cells = floor.get("min_cells_at_floor")
        if isinstance(frame, bool) or not isinstance(frame, int) or not 0 <= frame < frame_count:
            return False, "rubric floor_witness frame is malformed"
        if isinstance(cells, bool) or not isinstance(cells, int) or cells <= 0 or cells > rows:
            return False, "rubric floor_witness min_cells_at_floor is malformed"
    return True, ""


class _ArtifactSafetyError(RuntimeError):
    """Raised when an artifact cannot be inspected without pathname races."""


def _safe_stat(name: str, directory_fd: int) -> os.stat_result:
    stat_fn = getattr(os, "stat", None)
    if stat_fn is not None and stat_fn in getattr(os, "supports_dir_fd", ()):
        if stat_fn in getattr(os, "supports_follow_symlinks", ()):
            return stat_fn(name, dir_fd=directory_fd, follow_symlinks=False)
    lstat_fn = getattr(os, "lstat", None)
    if lstat_fn is not None and lstat_fn in getattr(os, "supports_dir_fd", ()):
        return lstat_fn(name, dir_fd=directory_fd)
    raise _ArtifactSafetyError("required descriptor no-follow stat primitive is unavailable")


def open_artifact_document(directory: Path) -> Any:
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
            directory_fd = open_fn(directory, os.O_RDONLY | directory_flag | nofollow_flag)
        except OSError as exc:
            if exc.errno == errno.ELOOP:
                raise _ArtifactSafetyError("artifact directory must not be a symlink") from exc
            raise _ArtifactSafetyError(f"artifact directory is missing or not a directory ({_bounded(exc)})") from exc
        try:
            names = sorted(listdir_fn(directory_fd))
        except OSError as exc:
            raise _ArtifactSafetyError(f"cannot inspect artifact directory ({_bounded(exc)})") from exc
        if names != [ARTIFACT]:
            raise _ArtifactSafetyError(f"directory must contain exactly {ARTIFACT}")
        try:
            artifact_stat = _safe_stat(ARTIFACT, directory_fd)
        except OSError as exc:
            raise _ArtifactSafetyError(f"cannot inspect artifact file ({_bounded(exc)})") from exc
        if stat.S_ISLNK(artifact_stat.st_mode):
            raise _ArtifactSafetyError("artifact file must not be a symlink")
        if not stat.S_ISREG(artifact_stat.st_mode):
            raise _ArtifactSafetyError("artifact file is missing or not a regular file")
        artifact_fd: int | None = None
        try:
            try:
                artifact_fd = open_fn(ARTIFACT, os.O_RDONLY | nofollow_flag | getattr(os, "O_CLOEXEC", 0), dir_fd=directory_fd)
            except OSError as exc:
                if exc.errno == errno.ELOOP:
                    raise _ArtifactSafetyError("artifact file must not be a symlink") from exc
                raise _ArtifactSafetyError(f"cannot open artifact file safely ({_bounded(exc)})") from exc
            try:
                opened_stat = os.fstat(artifact_fd)
            except OSError as exc:
                raise _ArtifactSafetyError(f"cannot inspect opened artifact file ({_bounded(exc)})") from exc
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


def load_artifact(directory: Path, rubric: dict[str, Any], label: str) -> tuple[dict[str, Any] | None, dict[str, Any] | None]:
    """Strictly open, parse and structurally validate one artifact; returns (document, failure)."""
    try:
        document = open_artifact_document(directory)
    except _ArtifactSafetyError as exc:
        return None, _failure(f"{label}: {exc}", policy=POLICY, case=rubric["case"])
    except (OSError, UnicodeError, json.JSONDecodeError, DuplicateKey, ValueError) as exc:
        return None, _failure(f"{label}: invalid strict JSON ({_bounded(exc)})", policy=POLICY, case=rubric["case"])
    if not _finite_tree(document):
        return None, _failure(f"{label}: artifact contains a non-finite JSON number", policy=POLICY, case=rubric["case"])
    ok, reason = validate_document(document, rubric)
    if not ok:
        return None, _failure(f"{label}: {reason}", policy=POLICY, case=rubric["case"])
    return document, None


def validate_document(document: Any, rubric: dict[str, Any]) -> tuple[bool, str]:
    if not isinstance(document, dict):
        return False, "artifact must be a JSON object"
    required = {"schema", "case", "dimensions", "variables", "primitive_fields", "source_format", "frames"}
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
    expected_rows = rubric.get("expected_rows", math.prod(rubric["dimensions"]))
    expected_times = rubric["expected_times"]
    for index, frame in enumerate(frames):
        if not isinstance(frame, dict) or set(frame) != {"time", "cycle", "rows"}:
            return False, f"frame {index} has malformed keys"
        if not _number(frame["time"]) or float(frame["time"]) != float(expected_times[index]):
            return False, f"frame {index} has wrong or non-finite time"
        if not isinstance(frame["cycle"], int) or isinstance(frame["cycle"], bool) or frame["cycle"] < 0:
            return False, f"frame {index} has invalid cycle"
        rows = frame["rows"]
        if not isinstance(rows, list) or len(rows) != expected_rows:
            return False, f"frame {index} has wrong row count"
        previous: tuple[float, float, float] | None = None
        for row_index, row in enumerate(rows):
            if not isinstance(row, list) or len(row) != len(VARIABLES):
                return False, f"frame {index} row {row_index} has wrong width"
            if not all(_number(value) for value in row):
                return False, f"frame {index} row {row_index} has a non-finite/non-numeric value"
            if row[3] <= 0.0 or row[4] <= 0.0:
                return False, f"frame {index} row {row_index} has non-positive rho or press"
            key = (float(row[2]), float(row[1]), float(row[0]))
            if previous is not None and key <= previous:
                return False, f"frame {index} row {row_index} breaks the global k-j-i cell order"
            previous = key
    floor = rubric.get("floor_witness")
    if floor is not None:
        frame = frames[int(floor["frame"])]
        dfloor, pfloor = float(floor["dfloor"]), float(floor["pfloor"])
        hits = sum(1 for row in frame["rows"] if float(row[3]) == dfloor and float(row[4]) == pfloor)
        if hits < int(floor["min_cells_at_floor"]):
            return False, f"floor witness: only {hits} cells sit exactly at dfloor/pfloor in frame {floor['frame']}"
    return True, ""


def _compare(reference: dict[str, Any], candidate: dict[str, Any], rubric: dict[str, Any], self_test_mode: bool) -> dict[str, Any]:
    max_difference = 0.0
    max_ratio = 0.0
    coordinate_mismatch: tuple[int, int, int] | None = None
    policy = rubric["comparison_policy"]
    if policy == POLICY:
        abs_tolerance = float(rubric["primitive_abs_tolerance"])
        rel_tolerance = float(rubric["primitive_rel_tolerance"])
    else:
        # Inactive row: no approved bound exists, so nothing can pass.  The loop
        # still runs to report the unscored identity diagnostic.
        abs_tolerance = 0.0
        rel_tolerance = 0.0
    identity = True
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
                        return _failure("primitive difference overflowed finite diagnostics", policy=policy, case=rubric["case"])
                    if difference != 0.0:
                        identity = False
                    tolerance = abs_tolerance + rel_tolerance * max(abs(ref_value), abs(cand_value))
                    if not math.isfinite(tolerance) or tolerance < 0.0:
                        return _failure("primitive tolerance became non-finite", policy=policy, case=rubric["case"])
                    if tolerance == 0.0:
                        ratio = 0.0 if difference == 0.0 else math.inf
                    else:
                        ratio = difference / tolerance
                    max_difference = max(max_difference, difference)
                    max_ratio = max(max_ratio, ratio)
    except (KeyError, IndexError, TypeError, ValueError, OverflowError) as exc:
        return _failure(f"comparison failed ({_bounded(exc)})", policy=policy, case=rubric["case"])
    if not math.isfinite(max_ratio):
        max_ratio = -1.0  # identity required and violated; keep the verdict finite
    within = coordinate_mismatch is None and 0.0 <= max_ratio <= 1.0
    identity_observed = identity and coordinate_mismatch is None
    if policy == POLICY:
        passed = within
        scored = True
        status = STATUS_APPROVED if passed else STATUS_FAILED
        if coordinate_mismatch is not None:
            reason = "coordinate mismatch at frame {}, row {}, coordinate {}".format(*coordinate_mismatch)
        elif passed:
            reason = "approved Option A primitive tolerance passed"
        else:
            reason = "one or more primitive values exceed approved tolerance"
    else:
        # Inactive in every mode, including the Docker self-test: identity is
        # reported as a determinism diagnostic and earns nothing.
        passed = False
        scored = False
        status = STATUS_INACTIVE
        reason = ("inactive subcase: no owner-approved acceptance rule exists for this deck, so it is never "
                  "scored; the two roots were "
                  + ("bit-identical" if identity_observed else "not bit-identical")
                  + " (unscored determinism diagnostic)")
    return {
        "passed": passed,
        "scored": scored,
        "status": status,
        "case": rubric["case"],
        "policy": policy,
        "coordinate_policy": COORDINATE_POLICY,
        "cycle_policy": CYCLE_POLICY,
        "schedule_policy": SCHEDULE_POLICY,
        "self_test_mode": bool(self_test_mode),
        "identity_observed": identity_observed,
        "max_abs_primitive_difference": max_difference,
        "max_normalized_tolerance_ratio": max_ratio,
        "max_abs_difference": max_difference,
        "reason": reason,
    }


def validate_dirs(
    reference_dirs: Iterable[str | Path],
    candidate_dirs: Iterable[str | Path],
    rubric_path: str | Path,
    *,
    documents: tuple[dict[str, Any], dict[str, Any]] | None = None,
    self_test_mode: bool = False,
) -> dict[str, Any]:
    """Compare one reference and one candidate directory without side effects.

    ``documents`` may carry already strictly-loaded (reference, candidate)
    artifacts supplied by the harness after provenance verification; the
    structural validation is still repeated here.
    """
    refs = [Path(value) for value in reference_dirs]
    cands = [Path(value) for value in candidate_dirs]
    try:
        rubric = _load(Path(rubric_path))
        rubric_ok, rubric_reason = _validate_rubric(rubric)
        if not rubric_ok:
            return _failure(f"malformed rubric: {rubric_reason}")
        assert isinstance(rubric, dict)
        if len(refs) != 1 or len(cands) != 1:
            return _failure("exactly one reference and candidate directory required", policy=POLICY, case=rubric["case"])
        loaded: list[dict[str, Any]] = []
        for index, (label, directory) in enumerate((("reference", refs[0]), ("candidate", cands[0]))):
            if documents is not None:
                document = documents[index]
                ok, reason = validate_document(document, rubric)
                if not ok:
                    return _failure(f"{label}: {reason}", policy=POLICY, case=rubric["case"])
            else:
                document, failure = load_artifact(directory, rubric, label)
                if failure is not None:
                    return failure
            assert document is not None
            loaded.append(document)
        return _compare(loaded[0], loaded[1], rubric, bool(self_test_mode))
    except (OSError, UnicodeError, json.JSONDecodeError, DuplicateKey, KeyError, TypeError, ValueError) as exc:
        return _failure(f"validator failure: {_bounded(exc)}")
