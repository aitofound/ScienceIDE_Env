# dynamics-utilities

Runs the grouped upstream QuSpin test files listed in `runner.py` for this
module. Each listed file keeps its own upstream assertions: pytest-style files
run under pytest, and the two script-style files (top-level assertions with no
pytest function) run directly so their own exit status decides pass or fail.

The check also records a finite spin-chain ground energy as a physical
calibration observable. Nominal uses `L=8,h=0.5,J=1.0`; the variant moves the
active binary64 bond coupling to `J=1.0000000000001` (450 ulps, `dJ/J = 1e-13`)
with the chain length and field fixed. The longitudinal field is deliberately
not the perturbed input: inside a fixed-magnetization sector it contributes a
constant times the identity, so changing it would leave the calibration
observable at the eigensolver noise floor. The displacement is above that floor
rather than the two-ulp convention because a two-ulp coupling change is
indistinguishable from ARPACK repeat noise.
