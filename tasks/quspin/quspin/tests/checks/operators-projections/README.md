# operators-projections

Runs the grouped upstream QuSpin test files listed in `runner.py` for this
module. Each listed file keeps its own upstream assertions: pytest-style files
run under pytest, and the two script-style files (top-level assertions with no
pytest function) run directly so their own exit status decides pass or fail.

The check also records a finite spin-chain ground energy as a physical
calibration observable. Nominal uses `L=8,h=0.5`; variant changes the active
longitudinal field by two binary64 ulps (`h=0.5000000000000002`) with the chain
length fixed, so the perturbation is a genuine numeric calibration rather than a
change of physical model.

## Known upstream exclusions

`test_quantum_operator.py` is owned by `operators-projections`, but its
`test_eigsh` case is deselected: it compares two ARPACK `eigsh` outputs
element-by-element without sorting the eigenvalues. The two runs return the same
eigenvalue set in a platform-dependent order, so the assertion fails on some x86
builds even though the spectra agree exactly (the observed error equals the
swap of the two returned values). The remaining three cases in the same file
still run. Every other upstream `test_*.py` file runs with its own assertions
intact.
