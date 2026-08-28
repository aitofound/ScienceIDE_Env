#!/usr/bin/env python3
"""Exercise fail-closed rejection of perturbed, malformed, and symlink artifacts."""
from __future__ import annotations
import json
import shutil
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from mhd_validator import validate_dirs  # noqa: E402


STATE = "mhd_state.json"
OBSERVABLES = "mhd_observables.json"
ALLOWED_NAMES = [OBSERVABLES, STATE]


def copy_case(reference: Path, scratch: Path, name: str) -> tuple[Path, Path]:
    ref = scratch / name / "reference"
    cand = scratch / name / "candidate"
    shutil.copytree(reference, ref)
    shutil.copytree(reference, cand)
    return ref, cand


def copy_symlink_case(reference: Path, scratch: Path, name: str) -> tuple[Path, Path]:
    """Build exactly two candidate names, with state linked outside candidate."""
    ref = scratch / name / "reference"
    cand = scratch / name / "candidate"
    shutil.copytree(reference, ref)
    cand.mkdir(parents=True)
    shutil.copy2(ref / OBSERVABLES, cand / OBSERVABLES)
    (cand / STATE).symlink_to(ref / STATE)
    return ref, cand


def expect_reject(ref: Path, cand: Path, rubric: Path, label: str, expected_reason: str | None = None) -> dict[str, object]:
    result = validate_dirs([ref], [cand], rubric)
    if result.get("passed") is not False:
        raise SystemExit(f"negative fixture unexpectedly passed: {label}")
    reason = result.get("reason")
    if not isinstance(reason, str) or not reason.strip():
        raise SystemExit(f"negative fixture had no rejection reason: {label}")
    if expected_reason is not None and expected_reason not in reason:
        raise SystemExit(f"negative fixture reached the wrong rejection branch: {label}: {reason}")
    return {"fixture": label, "passed": False, "reason": reason}


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: negative_fixtures.py REFERENCE_CASE_DIR")
    reference = Path(sys.argv[1])
    if not reference.is_dir():
        raise SystemExit("reference case directory is missing")
    rubric = HERE.parent / "checks" / "linear-wave-3d" / "rubric.json"
    scratch = Path(tempfile.mkdtemp(prefix="athena-newtonian-mhd-negative-"))
    results = []

    ref, cand = copy_case(reference, scratch, "perturbed")
    state = json.loads((cand / STATE).read_text(encoding="utf-8"))
    state["frames"][1]["rows"][0][3] += 1.0e-3
    (cand / STATE).write_text(json.dumps(state, separators=(",", ":")) + "\n", encoding="utf-8")
    results.append(expect_reject(ref, cand, rubric, "perturbed-primitive"))

    ref, cand = copy_case(reference, scratch, "malformed")
    (cand / STATE).write_text('{"schema":', encoding="utf-8")
    results.append(expect_reject(ref, cand, rubric, "malformed-json"))

    ref, cand = copy_case(reference, scratch, "extra")
    (cand / "unexpected.txt").write_text("not an artifact\n", encoding="utf-8")
    results.append(expect_reject(ref, cand, rubric, "unexpected-artifact"))

    ref, cand = copy_symlink_case(reference, scratch, "symlink")
    if sorted(path.name for path in cand.iterdir()) != ALLOWED_NAMES:
        raise SystemExit("symlink fixture candidate must contain exactly the two allowed names")
    if not (cand / STATE).is_symlink():
        raise SystemExit("symlink fixture did not create a state symlink")
    results.append(expect_reject(ref, cand, rubric, "symlink-artifact", "mhd_state.json must be a regular non-symlink file"))

    # This probe intentionally emits one strict failed verdict: a negative-suite
    # rejection is evidence of fail-closed behavior, so its process exit is nonzero.
    record = {
        "schema": "athena-newtonian-mhd-negative-fixtures/v1",
        "reward": 0.0,
        "reward_range": [0.0, 1.0],
        "status": "failed",
        "reason": "negative fixtures intentionally exercise fail-closed rejection paths",
        "checks": results,
    }
    print(json.dumps(record, allow_nan=False, sort_keys=True, separators=(",", ":")))
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
