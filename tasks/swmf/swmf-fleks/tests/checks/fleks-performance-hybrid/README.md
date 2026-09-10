# fleks-performance-hybrid

Upstream source row: `PC/FLEKS/tests/performance/PARAM.in.hybrid` (gap `fleks-performance`). Policy: proposed `pointwise`; status: calibration-pending.

## The test

The official hybrid benchmark exercises kinetic-ion motion and Hall/Ohm plus Faraday advance on 128 cells and 2000 particles/cell. It has #SAVEPLOT 0, so the genuine output is its #SAVELOG PIC energy history.

`run.sh nominal`/`variant` use a private copy of the pinned source, the nearest correct 3-D adapter build (`./Config.pl -install=BATSRUS -compiler=gfortran`, AMReX, `PC/FLEKS/Config.pl -amrex3d -lev=2 -u=Exo`, `make EXE`, `make PIDL`, `make rundir`), one `FLEKS.exe` solve, and upstream `PostProc.pl` where the recipe emits plots. `SAB_STOP_SCALE=1.0` is the exact source window; `SAB_MPI_RANKS=1` is the proposed default and `SAB_MAKE_JOBS` changes build time only.

Source output evidence: performance/PARAM.in.hybrid has #SAVELOG dnSavePic/dnSavePT 100 and #SAVEPLOT 0; energy log is the physical output.

Declared physical outputs (one producer case; never split into separate checks):
- `pc_energy.log` (`swmf_log`): physical output selected by the pinned writer

Performance cases grade physical energy only. Wall clocks, iteration counts, ranks, chunks, and storage ordering are not science observables; runtime `20.0 s` is an explicitly unmeasured planning estimate.

## The two initial conditions

`ic/nominal/PARAM.in` is an exact copy of `PC/FLEKS/tests/performance/PARAM.in.hybrid` at the pinned source. `ic/variant/PARAM.in` changes only the first #UNIFORMSTATE rho from 5.0 to 5.000000001 (2e-10 relative); this is calibration input, not a finalized tolerance or expected PASS.

`run.sh altbuild` proposes the same source/deck with `Config.pl -O0`; it is not run here and has no measured floor.

## The pass policy

The proposed pointwise validator compares finite numeric values in the declared output schemas with `atol=1e-12` and `rtol=1e-3`, rejecting missing, malformed, non-finite, ragged, or wrong-shape records. The bound is provisional: a real implementation fault should cross it, but the implementation floor and suitable window require parent calibration and owner review. No clocks, iteration bookkeeping, rank/chunk layout, or storage order is graded.

## Evidence

No solver, reference generation, Docker/build, benchmark, selfcheck, or current fingerprint was run. The runtime is an unmeasured estimate. Parent calibration must measure nominal/variant and optional O0 spreads, inspect writer outputs, and obtain human decisions before any final PASS or canary claim.
