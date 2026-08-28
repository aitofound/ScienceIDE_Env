#!/usr/bin/env python3
"""Create deterministic malformed artifacts and focused fail-closed counterexamples."""
from __future__ import annotations
import argparse, copy, json, os, subprocess, sys
from pathlib import Path

LIB = Path(__file__).resolve().parent / "lib"
sys.path.insert(0, str(LIB))
from extract_observables import _diagnostics, VARIABLES  # noqa: E402
from observable_validator import _validate_case  # noqa: E402
from run_cases import validate_solver_interface  # noqa: E402


def _write(path: Path, document: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(document, allow_nan=False, sort_keys=True, separators=(",", ":")) + "\n", encoding="utf-8")


def _expect_reject(label: str, operation) -> dict[str, object]:
    try:
        value = operation()
    except SystemExit as exc:
        return {"name": label, "passed": True, "observed": f"rejected: {exc}"}
    if isinstance(value, tuple) and value and value[0] is False:
        return {"name": label, "passed": True, "observed": f"rejected: {value[1]}"}
    if isinstance(value, dict) and value.get("passed") is False:
        return {"name": label, "passed": True, "observed": f"rejected: {value.get('reason', '')}"}
    raise AssertionError(f"counterexample was accepted: {label}")


def counterexamples(output: Path) -> int:
    output.mkdir(parents=True, exist_ok=True)
    # The two-component example is component-wise sub-luminal but has
    # |v|^2=0.8^2+0.8^2 >= 1, so it must be a failed cell.
    rows = [
        [0.0, 0.0, 0.0, 1.0, 1.0, 0.8, 0.8, 0.0, 1.0, 0.0, 0.0],
        [1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0],
    ]
    observables = _diagnostics(rows, [2, 1, 1])
    frame = {"time": 0.0, "cycle": 0, "rows": rows, "observables": observables}
    case = {"name": "counterexample", "solver": "hlld", "deck": "athinput.linear_wave", "dimensions": [2, 1, 1], "variables": VARIABLES, "frames": [frame]}
    valid, reason = _validate_case(case, frame_count=1)
    if not valid:
        raise AssertionError(f"internally generated counterexample baseline rejected: {reason}")
    results: list[dict[str, object]] = [{"name": "vector-norm-v=(0.8,0.8,0)", "passed": observables["admissibility"]["all_sub_luminal"] is False and observables["recovery"]["failed_cells"] == 1, "all_sub_luminal": observables["admissibility"]["all_sub_luminal"], "failed_cells": observables["recovery"]["failed_cells"]}]

    wrong_count = copy.deepcopy(case)
    wrong_count["frames"][0]["observables"]["recovery"]["failed_cells"] = 0
    results.append(_expect_reject("failed_cells-mismatch", lambda: _validate_case(wrong_count, frame_count=1)))
    wrong_status = copy.deepcopy(case)
    wrong_status["frames"][0]["observables"]["recovery"]["status"] = "all_emitted_cells_admissible"
    results.append(_expect_reject("contradictory-status", lambda: _validate_case(wrong_status, frame_count=1)))
    wrong_boolean = copy.deepcopy(case)
    wrong_boolean["frames"][0]["observables"]["admissibility"]["all_sub_luminal"] = True
    results.append(_expect_reject("contradictory-admissibility-boolean", lambda: _validate_case(wrong_boolean, frame_count=1)))
    wrong_finite = copy.deepcopy(case)
    wrong_finite["frames"][0]["observables"]["finite_state"] = False
    results.append(_expect_reject("contradictory-finite-state", lambda: _validate_case(wrong_finite, frame_count=1)))
    wrong_range = copy.deepcopy(case)
    wrong_range["frames"][0]["observables"]["primitive_ranges"]["rho"]["min"] = 2.0
    results.append(_expect_reject("primitive-range-mismatch", lambda: _validate_case(wrong_range, frame_count=1)))

    same_binary = Path("/bin/sh")
    results.append(_expect_reject(
        "shock-same-binary-hlld-cannot-cover-hlle-llf",
        lambda: validate_solver_interface("shock", None, {solver: same_binary for solver in ("hlld", "hlle", "llf")}),
    ))
    results.append(_expect_reject(
        "shock-generic-binary-rejected",
        lambda: validate_solver_interface("shock", same_binary, {solver: same_binary for solver in ("hlld", "hlle", "llf")}),
    ))

    reward_dir = output / "reward-directory-target"
    reward_dir.mkdir(parents=True, exist_ok=True)
    env = {**os.environ, "HARBOR_REFERENCE_DIR": str(output / "missing-reference"), "HARBOR_CANDIDATE_DIR": str(output / "missing-candidate"), "HARBOR_REWARD_FILE": str(reward_dir)}
    completed = subprocess.run([sys.executable, str(Path(__file__).with_name("harness.py"))], env=env, check=False, text=True, capture_output=True)
    try:
        verdict = json.loads(completed.stdout.strip().splitlines()[-1])
    except (IndexError, json.JSONDecodeError) as exc:
        raise AssertionError(f"reward directory-target probe did not emit strict JSON: {exc}") from exc
    results.append({"name": "reward-directory-target-fails-closed", "passed": completed.returncode != 0 and verdict.get("reward") == 0.0 and verdict.get("status") == "failed" and "reward_file_error" in verdict, "exit": completed.returncode, "verdict": verdict})
    if not all(result.get("passed") is True for result in results):
        raise AssertionError("one or more focused counterexamples did not fail closed")
    _write(output / "counterexamples.json", {"schema": "athena-sr-mhd-counterexamples/v1", "results": results})
    print(json.dumps({"counterexamples": len(results), "status": "passed", "output": str(output / "counterexamples.json")}, sort_keys=True))
    return 0


def artifacts(base: Path, output: Path) -> int:
    source = json.loads((base / "observables.json").read_text(encoding="utf-8"))
    output.mkdir(parents=True, exist_ok=True)
    for name, document in (("accept-identity", source),):
        _write(output / name / "observables.json", document)
    perturbed = copy.deepcopy(source)
    perturbed["cases"][0]["frames"][0]["rows"][0][3] += 1e-3
    _write(output / "reject-perturbed" / "observables.json", perturbed)
    (output / "reject-malformed").mkdir(parents=True, exist_ok=True)
    (output / "reject-malformed" / "observables.json").write_text('{"schema":', encoding="utf-8")
    (output / "reject-missing").mkdir(parents=True, exist_ok=True)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--counterexamples", action="store_true")
    args = parser.parse_args()
    if args.counterexamples:
        return counterexamples(args.output)
    if args.base is None:
        parser.error("--base is required unless --counterexamples is supplied")
    return artifacts(args.base, args.output)


if __name__ == "__main__":
    raise SystemExit(main())
