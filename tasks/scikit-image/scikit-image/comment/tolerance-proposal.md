# Final scientific policy after Docker calibration

The human confirmed the TV-L1 and template-matching limits and requested equal family weights. The author applied the decision to all relevant outputs and the verifier. The current self-validation result and freshness are owned by the canonical CLI; the separate `docker-calibration.json` preserves the earlier successful calibration unchanged.

## Final numerical criteria

TV-L1 uses mean absolute component error at most **0.001 pixel per flow field**, separately for each of six nonzero fields across its unit-test and gallery checks. Two no-motion fields remain exactly zero. This average permits larger errors at individual pixels; it is not a pointwise cap.

All **ten `match_template` correlation maps**, nine from the official unit tests and one from the gallery, use **abs(candidate-reference) <= 0.0002 + 0.00002 * abs(reference)**. This criterion concerns correlation scores; other outputs in those checks retain their existing policies, including exact discrete coordinates. The complete per-output rules are in the two check rubrics and the generated catalogue in `task.toml`.

Ordinary binary16 outputs retain (rtol, atol) = (0.002, 2e-7), binary32/complex64 retain (2e-5, 2e-6), and binary64/complex128 retain (1e-7, 1e-10). Integer/Boolean quantities, physical counts and canonical regions are exact. Shapes, numerical categories, nonfinite masks and coupled geometry are enforced. Source analytic-target assertions are documented separately from these port-equivalence criteria.

## Equal family weights and whole-library acceptance

TV-L1 contributes 0.5 only when both `unit-registration-tvl1` and `example-registration-opticalflow` pass. Template matching contributes 0.5 only when both `unit-feature-template` and `example-features-detection-template` pass. The other 243 checks are mandatory prerequisites: a failure makes the total reward zero. A partial reward of 0.5 does not count as acceptance. All 247 checks must pass for acceptance and performance reporting.

The real verifier driver passed 11 integration cases for family failures, unrelated failures, missing outputs and missing family directories; see `weighted-driver-behavior-tests.json`. Each of the ten template maps was separately tested with a 0.0001 bias that passed and a 0.001 bias that failed, using the actual numerical validator; see `template-family-policy-tests.json`.

## Evidence behind the decision

The earlier Docker calibration passed all 247 checks with reward 1.0. It measured 108 nonzero graded changes and 139 checks with identical graded values while an ungraded file differed. Nominal computation took 505.7 seconds and source compilation 267.7 seconds, with 2 CPUs and a 6 GB memory limit. These are historical calibration measurements, not an assertion that an edited contract has already passed a fresh selfcheck.

For the float32 unit-template case, promoting the same stored operands exactly to float64 changes the pinned source output by up to 0.0001025523. Local variance in `src/skimage/feature/template.py` subtracts nearly equal moments, amplifying cancellation. The final criterion gives 1.95x margin over that measured source-precision difference. The actual validator rejects a uniform 0.001 correlation bias and a one-pixel response-map shift. This native precision observation was measured for that case only; it is not a separately measured floor for the other nine maps, nor a Docker alternative-build floor. The earlier one-map proposal was superseded by the human's family-wide criterion.

For TV-L1, the core-input Docker perturbation produced MAE 0.0000701522 pixel, with maximum local difference 0.0572544 pixel. Across stored native precision families, float32 versus float64 produced maximum per-field MAE 0.000184415 pixel while isolated differences reached 3.73375 pixels. These results support the explicitly chosen average-error metric rather than an unmeasured pointwise cap.

Other low-margin calibration quantities are identified in `calibration-attention-investigation.json`. The BRIEF suite's raw distance of 512 belongs to a floating Harris response with magnitude up to 1.1365e10; descriptor bits remain exact. No bound was relaxed for those other families.

## Coverage and limits

Retained calls cover 123/147 official test files and 124/141 galleries, comprising 17,150 distinct API/input pairs. In total, 8,219/9,500 parametrized test items contribute at least one retained call. These are not complete assertion, line or workflow coverage counts. Graph objects/merges/cuts, fitted classifiers, geometric-model fitting/composition, some random-stream workflows and callbacks remain absent or partial; `COVERAGE.md` and `coverage.json` identify them.

Each active variant probes a selected call rather than every call in a check. Identical graded variants provide no sensitivity evidence. Experimental skimage2 is outside the upstream default stable build. No GPU speedup, accelerator floor or alternative-build floor is claimed. The full per-check table, including the fresh measurements when available, is produced by `task review`; this document does not replace a CLI record.
