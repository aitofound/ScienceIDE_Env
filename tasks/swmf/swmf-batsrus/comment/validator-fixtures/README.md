# Validator smoke fixtures (not graded checks)

These tiny fixtures exercise the BATSRUS ASCII loader without a solver run.
`valid-reference/final_z0.out` and `valid-candidate/final_z0.out` are identical
and must pass. `malformed-candidate/final_z0.out` is deliberately ragged and
must be rejected with a non-zero scientific result (the validator writes
`passed: false`, rather than silently dropping a row).

The current BATSRUS check set grades structured-grid cells and time-history
rows, whose positions are physical; it has no unordered particle/mode
collection. `permutation-positive.json` therefore documents the positive
identity-keyed permutation rule for future collection outputs: every value
array is reordered by the carried `id`, not by storage position. It is not an
additional check.
