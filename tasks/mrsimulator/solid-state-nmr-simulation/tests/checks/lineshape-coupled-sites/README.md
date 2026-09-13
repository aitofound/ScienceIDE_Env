# lineshape-coupled-sites

This check adapts `code/mrsimulator/tests/spectral_integration_tests/test_lineshape_coupled_sites.py` into a self-contained 1H Bloch-decay spectrum calculation. The observable is the complex 32-point spectrum plus the active shielding parameters, written as little-endian float64 values in `output.bin`.

Run `./run.sh nominal` or `./run.sh variant`. The variant changes zeta by two binary64 ulps. The provisional pointwise bound is finalized after Docker self-validation; no alternative build is declared.

Upstream test: `code/mrsimulator/tests/spectral_integration_tests/test_lineshape_coupled_sites.py`. Policy: `pointwise`.

## The test

This check uses the fixed 1H Bloch-decay spectrum runner at 32 points with SAB_REPEATS=1 on one CPU; nominal and two-ulp variant runs are compared under the rubric and finalized after calibration.

## The two initial conditions

This check uses the fixed 1H Bloch-decay spectrum runner at 32 points with SAB_REPEATS=1 on one CPU; nominal and two-ulp variant runs are compared under the rubric and finalized after calibration.

## The pass policy

This check uses the fixed 1H Bloch-decay spectrum runner at 32 points with SAB_REPEATS=1 on one CPU; nominal and two-ulp variant runs are compared under the rubric and finalized after calibration.

## Evidence

This check uses the fixed 1H Bloch-decay spectrum runner at 32 points with SAB_REPEATS=1 on one CPU; nominal and two-ulp variant runs are compared under the rubric and finalized after calibration.
