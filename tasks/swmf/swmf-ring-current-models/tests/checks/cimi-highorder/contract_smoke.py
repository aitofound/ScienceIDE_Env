#!/usr/bin/env python3
"""Read-only tiny synthetic smoke for the Jason7016 three-flux contract.

The tracked fixture files are deliberately invalid and tiny. This does not build
or run SWMF, regrade preserved raw results, or fabricate solver output.
"""
from __future__ import annotations

import importlib.util
import json
import math
from pathlib import Path

HERE = Path(__file__).resolve().parent
FIXTURES = HERE / "fixtures" / "contract-smoke"
SPEC = importlib.util.spec_from_file_location("cimi_highorder_validate", HERE / "validate.py")
assert SPEC is not None and SPEC.loader is not None
validator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validator)
RUBRIC = json.loads((HERE / "rubric.json").read_text(encoding="utf-8"))

SCORED = ["CimiFlux_h.fls", "CimiFlux_o.fls", "CimiFlux_e.fls"]
DIAGNOSTICS = ["CIMI.log"]


def expect_value_error(call, fragment: str) -> None:
    try:
        call()
    except ValueError as exc:
        assert fragment in str(exc), (fragment, str(exc))
    else:
        raise AssertionError(f"expected ValueError containing {fragment!r}")


def main() -> None:
    assert validator.SCORED_FILES == SCORED
    assert validator.DIAGNOSTIC_FILES == DIAGNOSTICS
    assert not hasattr(validator, "parse_log")
    assert RUBRIC["diagnostics"] == [
        {
            "path": "CIMI.log",
            "format": "cimi_log",
            "role": "collected and hash/size-preserved in run-manifest.json for review; not parsed, numerically compared, or scored",
        }
    ]

    expected_bounds = {
        "normal": {
            "CimiFlux_h.fls": (1e-10, 0.001),
            "CimiFlux_o.fls": (1e-10, 0.001),
            "CimiFlux_e.fls": (1e-10, 0.001),
        },
        "altbuild": {
            "CimiFlux_h.fls": (0.01691681, 0.001),
            "CimiFlux_o.fls": (0.00999043, 0.001),
            "CimiFlux_e.fls": (1e-10, 0.001),
        },
    }
    comparisons = {
        "normal": RUBRIC["comparison"],
        "altbuild": RUBRIC["altbuild_policy"]["comparison"],
    }
    for mode, comparison in comparisons.items():
        specs = validator.comparison_specs(comparison)
        assert [spec["path"] for spec in specs] == SCORED
        assert "CIMI.log" not in [spec["path"] for spec in specs]
        for spec in specs:
            assert (spec["atol"], spec["rtol"]) == expected_bounds[mode][spec["path"]]
            detail, failure, _, _ = validator.compare_pointwise(
                spec["path"], [1.0, 2.0], [1.0, 2.0], spec, comparison
            )
            assert failure is None and detail["values_over_bound"] == 0
            _, failure, _, _ = validator.compare_pointwise(
                spec["path"], [1.0], [2.0], spec, comparison
            )
            assert failure and "values exceed" in failure
            expect_value_error(
                lambda spec=spec, comparison=comparison: validator.compare_pointwise(
                    spec["path"], [1.0], [math.nan], spec, comparison
                ),
                "candidate contains non-finite values",
            )

    invalid = FIXTURES / "invalid-flux"
    # The deliberately non-91-row log is accepted by provenance because its
    # content is diagnostic; every deliberately invalid scored flux is rejected.
    validator.check_manifest(invalid)
    for name in SCORED:
        expect_value_error(
            lambda name=name: validator.parse_flux(invalid / name),
            "missing canonical CimiFlux header",
        )

    # A present diagnostic can never compensate for a missing scored flux.
    diagnostic_only = FIXTURES / "diagnostic-only"
    expect_value_error(
        lambda: validator.check_manifest(diagnostic_only),
        "manifest file missing/empty",
    )

    print(
        "contract_smoke: exact three-flux inventories/bounds, missing-invalid-nonfinite-quantitative "
        "flux rejection, and diagnostic-only CIMI.log preservation passed"
    )


if __name__ == "__main__":
    main()
