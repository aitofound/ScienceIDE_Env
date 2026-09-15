# models-crosschecks

Runs the grouped upstream QuSpin test files listed in `runner.py` for this
module. Each listed file keeps its own upstream assertions: pytest-style files
run under pytest, and the two script-style files (top-level assertions with no
pytest function) run directly so their own exit status decides pass or fail.

The check also records a finite spin-chain ground energy as a physical
calibration observable. Nominal uses `L=8,h=0.5`; variant changes the active
longitudinal field by two binary64 ulps (`h=0.5000000000000002`) with the chain
length fixed, so the perturbation is a genuine numeric calibration rather than a
change of physical model.

### Upstream-known-broken entries

`test_block_tools.py` (owned by `models-crosschecks`) is decorated
`@pytest.mark.xfail` in full: upstream marks its single `test()` as expected to
fail. Running it therefore reports `1 xfailed` and exits 0 without asserting
anything. It is kept in the run so the file is exercised and any future upstream
repair is picked up automatically, but it contributes **no** verification and
must not be counted as evidence that `block_tools` behaves correctly. The cause
is visible in the file itself: the dynamic term passes the `np` module as a
runtime argument, which multiprocessing cannot pickle.

No other upstream file carries an `xfail` or `skip` marker.
