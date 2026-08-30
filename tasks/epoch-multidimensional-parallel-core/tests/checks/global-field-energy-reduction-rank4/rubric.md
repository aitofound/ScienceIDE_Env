# Rubric: PAR-17 (global-field-energy-reduction-rank4)

## Pass policy

The `total_field_energy` scalar (already MPI_REDUCE'd across ranks inside
EPOCH) must agree between the rank=1 and rank=4 runs of the identical deck to
`rtol`/`atol` from `run.json`. The bound is tight (around 1e-8 relative)
because both runs perform the *same* floating-point sum, just reduced along a
different rank topology (MPI_REDUCE's summation order is not guaranteed
bit-identical across a different communicator shape); the value is
provisional pending remote-host calibration against the actual observed
reduction-order noise floor.

## Owner

Draft leaf; tolerances above are the packaging author's provisional,
documented policy pending the science owner's sign-off (see the packaging
skill's curation step). They are not copied from any other leaf's tolerances.
