# hitlet-properties

Upstream test: code/strax/tests/test_hitlet.py. Proposed policy: pointwise.

## The test

128 separated 16-sample records on two channels, converted to hitlets and processed for waveform properties. SAB_CASES scales the number of deterministic cases linearly; the graded default is 128 and the provisional runtime is 2 seconds on one CPU after Numba warm-up.

## The two initial conditions

the ADC-to-photoelectron scale is raised by two float32 ulps, perturbing hitlet area and amplitude without changing identities. No alternative build is declared.

## The pass policy

The proposed 1e-5 absolute bound grades hitlets sorted by physical time and channel, including charge and amplitude; a dropped sample or wrong calibration factor moves these quantities by much more. strax/processing/hitlets.py accumulates float32 waveform-derived properties, so float32 rounding is the expected mechanism. The floor has not yet been measured; the two-ULP scale variant will measure it during calibration and the human will finalize the bound.

## Evidence

No Docker build or calibration run has been performed before STOP 3. The policy type, 0.00001 absolute tolerance, window and variant are hypotheses for the first consented self-validation.

