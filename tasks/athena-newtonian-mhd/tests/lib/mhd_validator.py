"""Fail-closed verifier for complete Athena++ MHD state/CT artifacts.

The current policy is explicitly provisional exact identity: this proves the
self-wiring of the CPU oracle and candidate runner without asserting a final
accelerator tolerance. Jason owns the final scientific comparison policy.
"""
from __future__ import annotations

import hashlib
import json
import math
import struct
from pathlib import Path
from typing import Any, Iterable

STATE = "mhd_state.json"
OBSERVABLES = "mhd_observables.json"
STATE_SCHEMA = "athena-mhd-state/v1"
OBS_SCHEMA = "athena-mhd-ct-observables/v1"
VARIABLES = ["x1", "x2", "x3", "rho", "press", "vel1", "vel2", "vel3", "Bcc1", "Bcc2", "Bcc3"]
PRIMITIVES = VARIABLES[3:8]
MAGNETIC = VARIABLES[8:]
PIPELINE = [
    "src/hydro/calculate_fluxes.cpp",
    "src/hydro/add_flux_divergence.cpp",
    "src/field/calculate_corner_e.cpp",
    "src/field/ct.cpp",
    "src/task_list/time_integrator.cpp",
]
POLICY = "self_wiring_exact_identity_pending_jason_science_policy"


class DuplicateKey(ValueError):
    pass


def _pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise DuplicateKey(f"duplicate object key {key!r}")
        result[key] = value
    return result


def _constant(value: str) -> None:
    raise ValueError(f"non-standard JSON constant {value}")


def _load(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=_pairs, parse_constant=_constant)


def _finite(value: Any) -> bool:
    if isinstance(value, bool) or value is None or isinstance(value, str):
        return True
    if isinstance(value, (int, float)):
        try:
            return math.isfinite(float(value))
        except (TypeError, ValueError, OverflowError):
            return False
    if isinstance(value, list):
        return all(_finite(item) for item in value)
    if isinstance(value, dict):
        return all(isinstance(key, str) and _finite(item) for key, item in value.items())
    return False


def _number(value: Any) -> bool:
    return not isinstance(value, bool) and isinstance(value, (int, float)) and math.isfinite(float(value))


def _fail(reason: str, **extra: Any) -> dict[str, Any]:
    result = {
        "passed": False,
        "policy": POLICY,
        "max_abs_difference": 0.0,
        "max_normalized_tolerance_ratio": 0.0,
        "reason": str(reason).replace("\n", " ")[:320],
    }
    result.update(extra)
    return result


def _files(directory: Path) -> tuple[bool, str]:
    if directory.is_symlink() or not directory.is_dir():
        return False, "artifact directory is missing, not a directory, or is a symlink"
    try:
        names = sorted(path.name for path in directory.iterdir())
    except OSError as exc:
        return False, f"cannot inspect artifact directory: {exc}"
    if names != [OBSERVABLES, STATE]:
        return False, f"artifact directory must contain exactly {STATE} and {OBSERVABLES}"
    for name in (STATE, OBSERVABLES):
        path = directory / name
        if path.is_symlink() or not path.is_file():
            return False, f"{name} must be a regular non-symlink file"
    return True, ""


def _rubric(rubric: Any) -> tuple[bool, str]:
    if not isinstance(rubric, dict) or not _finite(rubric):
        return False, "rubric must be a finite strict JSON object"
    required = {"rubric_version", "case", "upstream_deck", "check_deck", "dimensions", "meshblock_dimensions", "frame_count", "expected_times", "boundary_policy", "eos", "flux", "required_pipeline", "artifact_schema", "variables", "comparison_policy", "scientific_status"}
    if not required.issubset(rubric):
        return False, "rubric is missing required keys"
    if rubric["artifact_schema"] != STATE_SCHEMA or rubric["variables"] != VARIABLES:
        return False, "rubric schema or variable order is unsupported"
    dims = rubric["dimensions"]
    blocks = rubric["meshblock_dimensions"]
    if not (isinstance(dims, list) and len(dims) == 3 and all(isinstance(x, int) and not isinstance(x, bool) and x > 0 for x in dims)):
        return False, "rubric dimensions are malformed"
    if not (isinstance(blocks, list) and len(blocks) == 3 and all(isinstance(x, int) and not isinstance(x, bool) and x > 0 for x in blocks)):
        return False, "rubric meshblock dimensions are malformed"
    if any(n % b for n, b in zip(dims, blocks)):
        return False, "rubric dimensions are not divisible by meshblock dimensions"
    if not isinstance(rubric["frame_count"], int) or rubric["frame_count"] <= 0 or len(rubric["expected_times"]) != rubric["frame_count"]:
        return False, "rubric frame schedule is malformed"
    if not all(_number(value) for value in rubric["expected_times"]):
        return False, "rubric frame times are not finite numbers"
    if rubric["boundary_policy"] not in {"periodic", "outflow"} or rubric["eos"] not in {"adiabatic", "isothermal"}:
        return False, "rubric physical policy is unsupported"
    if rubric["required_pipeline"] != PIPELINE:
        return False, "rubric does not require the complete MHD/CT pipeline"
    if rubric["comparison_policy"] != POLICY:
        return False, "rubric comparison policy is not named provisional identity"
    return True, ""


def _state(document: Any, rubric: dict[str, Any]) -> tuple[bool, str]:
    required = {"schema", "case", "dimensions", "variables", "primitive_fields", "magnetic_fields", "source_format", "frames"}
    if not isinstance(document, dict) or set(document) != required:
        return False, "state artifact keys are incomplete or unexpected"
    if document["schema"] != STATE_SCHEMA or document["case"] != rubric["case"] or document["dimensions"] != rubric["dimensions"]:
        return False, "state schema, case, or dimensions do not match rubric"
    if document["variables"] != VARIABLES or document["primitive_fields"] != PRIMITIVES or document["magnetic_fields"] != MAGNETIC:
        return False, "state variable ordering is not the complete MHD contract"
    source_format = document["source_format"]
    if not isinstance(source_format, dict) or source_format.get("dtype") != "float64":
        return False, "state source-format metadata is malformed"
    expected_identity = {
        "source_commit": "823614c90b594472747a0ac2a699e4a454f300d2",
        "case": rubric["case"],
        "problem": rubric["problem"],
        "eos": rubric["eos"],
        "solver": rubric["flux"],
    }
    if any(source_format.get(key) != value for key, value in expected_identity.items()):
        return False, "state source/config identity does not match rubric"
    if source_format.get("source_path") != "ATHENA_SOURCE_DIR exact-pin vendored source":
        return False, "state source path evidence is missing or unexpected"
    if not isinstance(source_format.get("config_sha256"), str) or len(source_format["config_sha256"]) != 64:
        return False, "state configuration hash evidence is malformed"
    if not isinstance(source_format.get("runtime_overrides"), str):
        return False, "state runtime selector evidence is malformed"
    frames = document["frames"]
    if not isinstance(frames, list) or len(frames) != rubric["frame_count"]:
        return False, "state frame count does not match rubric"
    expected_rows = math.prod(rubric["dimensions"])
    for fi, frame in enumerate(frames):
        if not isinstance(frame, dict) or set(frame) != {"time", "cycle", "rows"}:
            return False, f"state frame {fi} keys are malformed"
        if not _number(frame["time"]) or float(frame["time"]) != float(rubric["expected_times"][fi]):
            return False, f"state frame {fi} time is not the exact scheduled endpoint"
        if not isinstance(frame["cycle"], int) or isinstance(frame["cycle"], bool) or frame["cycle"] < 0:
            return False, f"state frame {fi} cycle is invalid"
        rows = frame["rows"]
        if not isinstance(rows, list) or len(rows) != expected_rows:
            return False, f"state frame {fi} row count is wrong"
        for ri, row in enumerate(rows):
            if not isinstance(row, list) or len(row) != len(VARIABLES) or not all(_number(value) for value in row):
                return False, f"state frame {fi} row {ri} is not a finite full MHD row"
            if row[3] <= 0.0 or row[4] <= 0.0:
                return False, f"state frame {fi} row {ri} has non-positive rho or pressure"
    return True, ""


def _observables(document: Any, rubric: dict[str, Any]) -> tuple[bool, str]:
    required = {"schema", "case", "dimensions", "frame_count", "boundary_policy", "gamma", "frames", "scientific_status"}
    if not isinstance(document, dict) or set(document) != required:
        return False, "observable artifact keys are incomplete or unexpected"
    if document["schema"] != OBS_SCHEMA or document["case"] != rubric["case"] or document["dimensions"] != rubric["dimensions"]:
        return False, "observable schema, case, or dimensions do not match rubric"
    if document["frame_count"] != rubric["frame_count"] or document["boundary_policy"] != rubric["boundary_policy"] or not _number(document["gamma"]):
        return False, "observable metadata does not match rubric"
    frames = document["frames"]
    if not isinstance(frames, list) or len(frames) != rubric["frame_count"]:
        return False, "observable frame count is wrong"
    for fi, frame in enumerate(frames):
        required_frame = {"time", "cycle", "face_field", "discrete_div_b", "conserved_sums", "ct_update_evidence"}
        if not isinstance(frame, dict) or set(frame) != required_frame:
            return False, f"observable frame {fi} keys are malformed"
        if frame["time"] != rubric["expected_times"][fi] or not isinstance(frame["cycle"], int) or frame["cycle"] < 0:
            return False, f"observable frame {fi} time/cycle is invalid"
        face = frame["face_field"]
        if not isinstance(face, dict) or set(face) != {"locations", "shapes", "value_count", "sha256", "method"}:
            return False, f"observable frame {fi} face-field evidence is malformed"
        nx, ny, nz = rubric["dimensions"]
        expected_shapes = [[nz, ny, nx + 1], [nz, ny + 1, nx], [nz + 1, ny, nx]]
        if face["shapes"] != expected_shapes or face["locations"] != ["B1f(x1-face)", "B2f(x2-face)", "B3f(x3-face)"]:
            return False, f"observable frame {fi} face locations/shapes are wrong"
        if not isinstance(face["value_count"], int) or face["value_count"] != sum(math.prod(shape) for shape in expected_shapes) or not isinstance(face["sha256"], str) or len(face["sha256"]) != 64:
            return False, f"observable frame {fi} face hash/count is malformed"
        div = frame["discrete_div_b"]
        if not isinstance(div, dict) or set(div) != {"cell_count", "finite", "l1", "linf", "max_abs", "spacing", "boundary_policy"}:
            return False, f"observable frame {fi} div-B evidence is malformed"
        if div["cell_count"] != math.prod(rubric["dimensions"]) or div["finite"] is not True or div["boundary_policy"] != rubric["boundary_policy"] or not all(_number(div[name]) for name in ("l1", "linf", "max_abs")) or not isinstance(div["spacing"], list) or len(div["spacing"]) != 3 or not all(_number(x) and float(x) > 0 for x in div["spacing"]):
            return False, f"observable frame {fi} div-B evidence is invalid"
        sums = frame["conserved_sums"]
        if not isinstance(sums, dict) or set(sums) != {"mass", "momentum", "total_energy"} or not _number(sums["mass"]) or not _number(sums["total_energy"]) or not isinstance(sums["momentum"], list) or len(sums["momentum"]) != 3 or not all(_number(x) for x in sums["momentum"]):
            return False, f"observable frame {fi} conservation evidence is invalid"
        evidence = frame["ct_update_evidence"]
        if not isinstance(evidence, dict) or set(evidence) != {"pipeline", "compiled_path", "native_corner_emf_counter", "native_face_checkpoint", "status"} or evidence["pipeline"] != PIPELINE or evidence["native_corner_emf_counter"] is not None or evidence["native_face_checkpoint"] is not False or not isinstance(evidence["status"], str) or "self-wiring" not in evidence["status"]:
            return False, f"observable frame {fi} CT evidence is invalid"
        compiled_path = evidence["compiled_path"]
        if not isinstance(compiled_path, str) or any(token not in compiled_path for token in (
            "-b Newtonian MHD",
            "source_commit=823614c90b594472747a0ac2a699e4a454f300d2",
            f"solver={rubric['flux']}",
            f"eos={rubric['eos']}",
            f"problem={rubric['problem']}",
        )):
            return False, f"observable frame {fi} lacks forced source/configuration path evidence"
    if not isinstance(document["scientific_status"], str) or "Jason" not in document["scientific_status"]:
        return False, "observable scientific status does not disclose owner-pending policy"
    return True, ""


def _identity(reference: dict[str, Any], candidate: dict[str, Any], rubric: dict[str, Any]) -> dict[str, Any]:
    max_difference = 0.0
    for fi, (ref_frame, cand_frame) in enumerate(zip(reference["frames"], candidate["frames"])):
        if ref_frame["time"] != cand_frame["time"]:
            return _fail(f"state time mismatch at frame {fi}", case=rubric["case"])
        for ri, (ref_row, cand_row) in enumerate(zip(ref_frame["rows"], cand_frame["rows"])):
            for ci, (left, right) in enumerate(zip(ref_row, cand_row)):
                difference = abs(float(left) - float(right))
                max_difference = max(max_difference, difference)
                if left != right:
                    return _fail(f"exact self-wiring mismatch at frame {fi}, row {ri}, column {ci}", case=rubric["case"], max_abs_difference=max_difference)
    return {"passed": True, "case": rubric["case"], "policy": POLICY, "max_abs_difference": max_difference, "max_normalized_tolerance_ratio": 0.0, "reason": "exact state identity self-wiring check passed"}


def validate_dirs(reference_dirs: Iterable[str | Path], candidate_dirs: Iterable[str | Path], rubric_path: str | Path) -> dict[str, Any]:
    refs, cands = [Path(x) for x in reference_dirs], [Path(x) for x in candidate_dirs]
    try:
        rubric = _load(Path(rubric_path))
        ok, reason = _rubric(rubric)
        if not ok:
            return _fail(f"malformed rubric: {reason}")
        if len(refs) != 1 or len(cands) != 1:
            return _fail("exactly one reference and candidate directory required", case=rubric["case"])
        for label, directory in (("reference", refs[0]), ("candidate", cands[0])):
            ok, reason = _files(directory)
            if not ok:
                return _fail(f"{label}: {reason}", case=rubric["case"])
        documents: list[tuple[dict[str, Any], dict[str, Any]]] = []
        for label, directory in (("reference", refs[0]), ("candidate", cands[0])):
            state = _load(directory / STATE)
            obs = _load(directory / OBSERVABLES)
            if not _finite(state) or not _finite(obs):
                return _fail(f"{label}: artifact contains non-finite values", case=rubric["case"])
            ok, reason = _state(state, rubric)
            if not ok:
                return _fail(f"{label}: {reason}", case=rubric["case"])
            ok, reason = _observables(obs, rubric)
            if not ok:
                return _fail(f"{label}: {reason}", case=rubric["case"])
            documents.append((state, obs))
        result = _identity(documents[0][0], documents[1][0], rubric)
        if not result["passed"]:
            return result
        if documents[0][1] != documents[1][1]:
            return _fail("exact self-wiring mismatch in CT/conservation observable sidecar", case=rubric["case"])
        result["observable_identity"] = True
        result["face_field_policy"] = "derived-face-hash-equivalent; no native face checkpoint emitted by TAB mode"
        return result
    except (OSError, UnicodeError, json.JSONDecodeError, DuplicateKey, ValueError, TypeError, KeyError, IndexError) as exc:
        return _fail(f"fail-closed artifact validation error: {exc}")


def validate(reference_dirs: Iterable[str | Path], candidate_dirs: Iterable[str | Path], rubric_path: str | Path) -> dict[str, Any]:
    return validate_dirs(reference_dirs, candidate_dirs, rubric_path)
