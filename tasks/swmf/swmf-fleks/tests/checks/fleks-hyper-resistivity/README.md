# fleks-hyper-resistivity

Upstream source row: `PC/FLEKS/tests/hyper_resistivity/PARAM.in` (gap `fleks-hyper-resistivity`). Policy: proposed `pointwise`; status: calibration-pending.

## The test

A frozen hybrid plasma seeds a circularly polarized transverse mode and isolates fourth-order hyper-resistive damping; fields and energy values are the physical contract.

`run.sh nominal`/`variant` use a private copy of the pinned source, the nearest correct 3-D adapter build (`./Config.pl -install=BATSRUS -compiler=gfortran`, AMReX, `PC/FLEKS/Config.pl -amrex3d -lev=2 -u=Exo`, `make EXE`, `make PIDL`, `make rundir`), one `FLEKS.exe` solve, and upstream `PostProc.pl` where the recipe emits plots. `SAB_STOP_SCALE=1.0` is the exact source window; `SAB_MPI_RANKS=1` is the proposed default and `SAB_MAKE_JOBS` changes build time only.

Source output evidence: hyper_resistivity/README.md and validate.py define exponential magnetic-energy decay and discrete damping; #SAVEPLOT/#SAVELOG emit the contract.

Declared physical outputs (one producer case; never split into separate checks):
- `pc_cut.out` (`swmf_idl`): physical output selected by the pinned writer
- `pc_energy.log` (`swmf_log`): physical output selected by the pinned writer

Performance cases grade physical energy only. Wall clocks, iteration counts, ranks, chunks, and storage ordering are not science observables; runtime `2.0 s` is an explicitly unmeasured planning estimate.

## The two initial conditions

`ic/nominal/PARAM.in` is an exact copy of `PC/FLEKS/tests/hyper_resistivity/PARAM.in` at the pinned source. `ic/variant/PARAM.in` changes only the #WAVEIC frac value from 0.5 to 0.5000000001 (2e-10 relative); this is calibration input, not a finalized tolerance or expected PASS.

`run.sh altbuild` proposes the same source/deck with `Config.pl -O0`; it is not run here and has no measured floor.

## The pass policy

The proposed pointwise validator compares finite numeric values in the declared output schemas with `atol=1e-12` and `rtol=1e-3`, rejecting missing, malformed, non-finite, ragged, or wrong-shape records. The bound is provisional: a real implementation fault should cross it, but the implementation floor and suitable window require parent calibration and owner review. No clocks, iteration bookkeeping, rank/chunk layout, or storage order is graded.

## Evidence

No solver, reference generation, Docker/build, benchmark, selfcheck, or current fingerprint was run. The runtime is an unmeasured estimate. Parent calibration must measure nominal/variant and optional O0 spreads, inspect writer outputs, and obtain human decisions before any final PASS or canary claim.
