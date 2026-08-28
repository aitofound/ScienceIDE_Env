"""Strict shared predicate for the eight staged RHD checks.

Scientific numeric bounds are deliberately absent until trusted CPU repeats and
an independent arithmetic realization are measured.  Staging accepts finite
numeric equality (including signed-zero normalization), warns on identity, and
fails closed on any nonzero numerical difference.

The required output file set is PLUTO's real contract (see pluto_io.py) plus
the genuine side files a results directory may legitimately carry
(``pluto.ini``/``restart.out``/``pluto.0.log`` -- see
``pluto_io.TOLERATED_SIDE_FILES`` for the evidence trail); those are
tolerated, never required, and anything else is still rejected.

Entropy recovery is EOS-aware, matching the two branches PLUTO itself
compiles (Src/RHD/rhd_energy_solve.c:193-201): the IDEAL branch is
``p*lor/rho**(gamma-1)``; the TAUB branch is the gamma-free Taub-Mathews form
``p*lor/rho**(2/3) * (1.5*theta + sqrt(2.25*theta**2 + 1))`` with
``theta = p/rho``. ``physics.gamma`` is ``null`` for every TAUB-EOS row in
this rubric set: PLUTO's ``g_gamma`` is declared only under
``#if EOS == IDEAL`` (Src/globals.h:117-121, Src/pluto.h:925-929) and is not
a compiled/linkable symbol under TAUB, so no numeric gamma is defensible for
those rows (see comment/build-decision.md for the row-by-row evidence).
"""
from __future__ import annotations

import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
from metrics import fields, signature, state_scale
from pluto_io import OutputError, TOLERATED_SIDE_FILES, read_output
from taub import reconstruct


def _verdict(check: str) -> dict:
    return {"check": check, "passed": False, "outcome": "no_output",
            "frames_scored": 0, "frames_ok": 0, "bytewise": False,
            "warnings": [], "detail": []}


def _same_number(left: float, right: float) -> bool:
    return left == right and math.isfinite(left) and math.isfinite(right)


def _same_vector(left: list[float], right: list[float]) -> bool:
    return len(left) == len(right) and all(_same_number(a, b) for a, b in zip(left, right))


def _files(root: str) -> set[str]:
    return {name for name in os.listdir(root)
            if os.path.isfile(os.path.join(root, name))}


def _bytewise(left: dict, right: dict) -> bool:
    names = {"grid.out", "dbl.out"}
    names.update(f"data.{row['frame']:04d}.dbl" for row in left["index"])
    for name in names:
        with open(os.path.join(left["root"], name), "rb") as stream:
            a = stream.read()
        with open(os.path.join(right["root"], name), "rb") as stream:
            b = stream.read()
        if a != b:
            return False
    return True


def _metadata_error(ref: dict, cand: dict) -> str | None:
    if ref["grid"] != cand["grid"]:
        return "grid dimensions or geometry differ"
    if len(ref["index"]) != len(cand["index"]):
        return "frame count differs"
    for a, b in zip(ref["index"], cand["index"]):
        for key in ("frame", "time", "dt", "nstep", "mode", "endian", "variables"):
            if a[key] != b[key]:
                return f"dbl.out field {key} differs at frame {a['frame']}"
    return None


def _lorentz_factor(x: float, y: float, z: float) -> float:
    speed2 = x * x + y * y + z * z
    lorentz2 = 1.0 / max(1.0 - speed2, 1.0e-300)
    return math.sqrt(lorentz2)


def _expected_entropy(eos: str, gamma: float, rho: list[float], prs: list[float],
                       vx1: list[float], vx2: list[float], vx3: list[float]) -> list[float]:
    expected = []
    for r, p, x, y, z in zip(rho, prs, vx1, vx2, vx3):
        lor = _lorentz_factor(x, y, z)
        if eos == "ideal":
            expected.append(p * lor / (r ** (gamma - 1.0)))
        elif eos == "taub":
            theta = p / r
            expected.append(p * lor / (r ** (2.0 / 3.0)) * (1.5 * theta + math.sqrt(2.25 * theta * theta + 1.0)))
        else:
            raise OutputError(f"entropy recovery has no formula for eos {eos!r}")
    return expected


def _physics(output: dict, rubric: dict) -> tuple[list[dict], list[dict]]:
    physics = rubric["physics"]
    dims = output["grid"]["dims"]
    ndim = sum(value > 1 for value in dims)
    if ndim != int(physics["dimensions"]):
        raise OutputError(f"expected {physics['dimensions']}-D output, got {dims}")
    if output["grid"]["geometry"] != physics["geometry"]:
        raise OutputError("geometry does not match the check contract")
    eos = physics["eos"]
    gamma_raw = physics.get("gamma")
    if eos == "ideal":
        if gamma_raw is None:
            raise OutputError("ideal EOS requires a non-null physics.gamma")
        gamma = float(gamma_raw)
    else:
        # TAUB carries no compiled g_gamma (Src/globals.h:117); taub.py's
        # reconstruct() never reads gamma on this branch, so an absent value
        # is passed through as NaN rather than guessed.
        gamma = float(gamma_raw) if gamma_raw is not None else float("nan")
    names = output["variables"]
    reconstructions = []
    metrics = []
    for index, frame in enumerate(output["frames"]):
        data = fields(frame, names)
        if any(value <= 0.0 for value in data["rho"]):
            raise OutputError(f"frame {index}: rho is not strictly positive")
        if any(value <= 0.0 for value in data["prs"]):
            raise OutputError(f"frame {index}: prs is not strictly positive")
        state_scale(frame)
        reconstructed = reconstruct(data, eos, gamma)
        if any(not math.isfinite(value) for values in reconstructed.values() for value in values):
            raise OutputError(f"frame {index}: reconstructed conserved state is nonfinite")
        if "entropy" in names:
            if any(value <= 0.0 for value in data["entropy"]):
                raise OutputError(f"frame {index}: entropy is not positive")
            expected = _expected_entropy(eos, gamma, data["rho"], data["prs"],
                                          data["vx1"], data["vx2"], data["vx3"])
            if not _same_vector(data["entropy"], expected):
                raise OutputError(f"frame {index}: entropy recovery is inconsistent")
        reconstructions.append(reconstructed)
        metrics.append(signature(frame, names, dims, physics["geometry"],
                                  list(rubric["observables"])))
    return reconstructions, metrics


def validate(reference: list[str], candidate: list[str], check_dir: str) -> dict:
    with open(os.path.join(check_dir, "rubric.json"), "r", encoding="utf-8") as stream:
        rubric = json.load(stream)
    verdict = _verdict(rubric["check"])
    if len(reference) != 1 or len(candidate) != 1:
        verdict["outcome"] = "wrong_shape"
        verdict["error"] = "deterministic check requires exactly one reference and one candidate"
        return verdict
    try:
        ref = read_output(reference[0], rubric)
        cand = read_output(candidate[0], rubric)
        error = _metadata_error(ref, cand)
        if error:
            verdict["outcome"] = "wrong_shape"
            verdict["error"] = error
            return verdict
        required = {"grid.out", "dbl.out"}
        required.update(f"data.{row['frame']:04d}.dbl" for row in ref["index"])
        actual = _files(cand["root"])
        missing = required - actual
        unexpected = actual - required - TOLERATED_SIDE_FILES
        if missing or unexpected:
            verdict["outcome"] = "wrong_shape"
            parts = []
            if missing:
                parts.append(f"missing {sorted(missing)}")
            if unexpected:
                parts.append(f"unexpected {sorted(unexpected)}")
            verdict["error"] = "candidate has missing or unexpected output files: " + "; ".join(parts)
            return verdict
        ref_cons, ref_metrics = _physics(ref, rubric)
        cand_cons, cand_metrics = _physics(cand, rubric)
        verdict["frames_scored"] = max(0, len(ref["frames"]) - 1)
        for index, (left, right) in enumerate(zip(ref["frames"], cand["frames"])):
            equal = all(_same_vector(a, b) for a, b in zip(left, right))
            recon_equal = all(_same_vector(ref_cons[index][name], cand_cons[index][name])
                              for name in ref_cons[index])
            metric_equal = ref_metrics[index] == cand_metrics[index]
            ok = equal and recon_equal and metric_equal
            verdict["frames_ok"] += int(ok)
            verdict["detail"].append({"frame": index, "fields_equal": equal,
                                       "conserved_equal": recon_equal,
                                       "observables_equal": metric_equal, "ok": ok})
            if not ok:
                verdict["outcome"] = "pending_calibration" if equal else "diverged"
                verdict["error"] = ("numeric values or independently reconstructed conserved/"
                                     "observable state differs; no scientific tolerance is measured yet")
                return verdict
        verdict["bytewise"] = _bytewise(ref, cand)
        if verdict["bytewise"]:
            verdict["warnings"].append(
                "reference and candidate are byte-identical; informational only, not a universal requirement")
        else:
            verdict["warnings"].append(
                "candidate differs only by accepted IEEE representation (signed zero); numeric tolerance pending")
        verdict["outcome"] = "passed"
        verdict["passed"] = True
        return verdict
    except (OSError, ValueError, OutputError, KeyError, TypeError) as exc:
        verdict["outcome"] = "invalid_output"
        verdict["error"] = str(exc)
        return verdict
