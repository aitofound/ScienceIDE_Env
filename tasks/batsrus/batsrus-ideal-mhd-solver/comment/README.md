# batsrus-ideal-mhd-solver: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

This module is the ideal MHD and hydrodynamic finite-volume core of BATSRUS, the part every other
configuration of the code sits on top of: the Riemann solvers of `src/ModFaceFlux.f90` and the
physical fluxes of `src/ModPhysicalFlux.f90` and `src/ModCharacteristic.f90`; the face-value
reconstruction and limiters of `src/ModFaceValue.f90`, second order through fifth order with its
low-order fallback; the four divergence-B strategies (the eight-wave Powell source, hyperbolic
cleaning through the MhdHyp equation set, constrained transport in `src/ModConstrainDivB.f90`, and
projection in `src/ModProjectDivB.f90` and `src/ModCleanDivB.f90`); the conservative versus
non-conservative energy switch of `src/ModConservative.f90` and `src/ModUpdateState.f90` with the
fused update path of `src/ModUpdateStateFast.f90`; the Boris correction of
`src/ModBorisCorrection.f90`; and the time-stepping variants, the explicit multi-stage loop of
`src/ModAdvanceExplicit.f90`, `src/ModTimeStepControl.f90`, `src/ModLocalTimeStep.f90`, the
partially steady block skipping of `src/ModPartSteady.f90` and the space-time transform of
`src/ModTimewarp.f90`. It also owns the geometry and region machinery those updates run on
(`srcBATL/BATL_geometry.f90`, `srcBATL/BATL_region.f90`), the four single-fluid equation sets
(`Mhd`, `MhdHyp`, `Hd`, `MhdNonCons`) and the `Waves` and `KelvinHelmholtz` user modules.

Deliberately excluded, because they are other modules of the same cut: the non-ideal terms and the
implicit and semi-implicit solvers (Hall, resistive, viscous, conductive); the multi-fluid, multi-ion
and five- and six-moment equation sets; planet-specific boundaries, dipoles and couplings; and the
AWSoM corona and outer-heliosphere applications. The OpenACC GPU port of `ModUpdateStateFast` is in
the module's source but cannot be exercised here: the pinned build has no nvfortran, so the decks
BATSRUS names `*_gpu` run the same code on the CPU with OpenACC off, which is what these checks do.

## Checks

Twenty-four checks: the fifteen suitable `Makefile.test` targets of this module
(`test_shocktube`, `test_shocktube_1d`, `test_shockround`, `test_shockramp`, `test_fastwave`,
`test_fastwave_2d`, `test_fastwave_athena`, `test_bx0`, `test_mhdnoncons`,
`test_kelvinhelmholtz_hd`, `test_kelvinhelmholtz_mhd`, `test_partsteady`, `test_timewarp_1d`,
`test_timewarp_2d`, `test_region2d`) and nine of the twelve upstream example decks of
`Param/SHOCKTUBE`, `Param/RUNGEKUTTA` and `Param/CYLOUTFLOW` that have no test target. Every check
rebuilds `BATSRUS.exe` inside its own `run.sh`, because every official BATSRUS test sets its own
compile-time equation set, user module, block size and ghost-cell count with `Config.pl`; the build
seconds are printed as `SAB_BUILD_SECONDS` and the driver keeps them out of the graded run time.

Three upstream examples of this module could not be made to run in the pinned tree and are left out.
They are worth a reviewer's attention, because two of them are upstream inconsistencies rather than
anything about this packaging:

- `Param/CYLOUTFLOW/PARAM.in` (radial outflow on a cylindrical grid). `srcUser/ModUserCylOutflow.f90`
  reads its initial condition from `Observation/intensities_t000.txt`, an observational intensity map
  that is not part of any of the four pinned repositories (the same class of missing input as
  `SWMF_data`). The run aborts in `read_plot_file` before the first step.
- `Param/RUNGEKUTTA/PARAM.in` (Runge-Kutta verification with a point-implicit friction source). Its
  `#USERMODULE` names `srcUser/ModUserPointImplicit.f90`, which does not compile against the pinned
  `src/`: `Error: Function 'rmin_b' at (1) has no IMPLICIT type` and
  `Error: Variable 'isdynamicpointimplicit' is PROTECTED and cannot appear in a variable definition
  context`. No other user module provides the `#FRICTION` command the deck needs.
- `Param/SHOCKTUBE/PARAM.in.sphericalwedge` (magnetised flow on a spherical wedge). The deck combines
  `#PLANET NONE` with `#MONOPOLEB0`, and the run aborts at the first update with
  `get_axes ERROR: init_axes has not been called`, raised from `update_b0`
  (`src/ModUpdateState.f90` line 1889) because the coordinate-transformation matrices are only
  initialised when a planet is named. Adding `#HELIOUPDATEB0 -1.0` did not stop it; `#UPDATE fast`
  gets past it through the early return of `update_b0` but then fails in
  `set_boundary_for_cell: Unimplemented boundary type` on the wedge's `reflect` boundaries. The
  curvilinear paths this deck would have covered are still covered by `shockround` (round cube) and
  `region2d` (cylindrical and logarithmic-radius cylindrical grids).

## Tolerances

Every bound was finalised by the agent under the human's blanket go-ahead of 2026-09-04 ("go on, i
consent to use either local or remote device for the docker runs, no need for further consent") and
is open to revision by the reviewer.

The evidence is the nominal-versus-variant spread the self-validation measures inside the declared
container, per check, over every graded value. The variant of each check moves one active
initial-condition number of the deck by about four units in the last place of the double precision
BATSRUS parses it with (a left-state or uniform-state density in most checks, the radial-state
density amplitude in `timewarp-2d`, the rotation rate in `ex-shocktube-rotation`, the region radius
in `region2d`, whose graded output is geometry rather than fluid state). The physics is unchanged;
only the round-off path of the run differs, so the spread is what two legitimate runs of the same
physics can differ by on this platform.

The bound of every check is the smallest power of ten that leaves a margin of at least a thousand
over the measured spread, floored at 1e-9 and capped at 1e-5. The floor of 1e-9 is not arbitrary:
every graded file is ASCII with eleven significant digits, so on a field of order unity the output
itself only resolves 1e-10, and a bound below that would be comparing the print format rather than
the physics. That is why the checks with the smallest spreads carry the largest margins in the
review table: their spread is a measure of the output format, not of the solver.

The bound is deliberately not tightened to the spread: the spread is a same-binary, same-machine,
same-rank-count measurement, while the bound has to survive a legitimate accelerator port that
reassociates its reductions and fuses its arithmetic differently.

Four checks needed a larger variant than the usual four ulps of binary64, because at that size the
perturbation was erased by the eleven-digit output format and the calibration run reported the two
initial conditions byte-identical: `fastwave-athena`, `kelvinhelmholtz-hd`, `kelvinhelmholtz-mhd`
and `region2d` move their chosen input by two units of the last printed digit (2e-10 relative)
instead. Their measured spreads are then 2.0e-10, 1.0e-7, 2.1e-8 and 9.0e-10; for the two that do
not amplify (`fastwave-athena`, `region2d`) the spread is the perturbation carried through undamped,
which is what a non-amplifying scheme should do, and their bound is a hundred times the resolution
of the graded output rather than a chaos measurement. The two Kelvin-Helmholtz checks amplify it by
about a hundred over the graded window, which is the early exponential phase of the instability and
the reason both carry the chaotic flag. `kelvinhelmholtz-hd` also had its variant re-anchored: the
deck's `#UNIFORMSTATE` is overridden by `srcUser/ModUserKelvinHelmholtz.f90`, so the perturbation
now moves `rhoInner` of the module's own `#PERTURBATION` block, which is the density the shear layer
is actually built from.

Three checks changed after the calibration run beyond their bound:

- `ex-shocktube-sphalfven` is graded at t = 1 rather than at the upstream t = 4. At t = 4 the
  calibration run measured an absolute spread of 2.0e-4 with sign changes in the low-amplitude
  current cells (a relative spread of 2 on a peak of 3.9): past that point two legitimate runs of
  this deck are no longer equivalent. The mechanism is the fifth-order reconstruction, whose
  `minmod4` four-way selection (`src/ModFaceValue.f90` line 3444) flips branch at a smooth extremum
  when a round-off difference reaches it, and the reconstructed face value then jumps by the local
  truncation error rather than by round-off. At t = 1 the spread is 1.8e-6 and the bound is 1e-3,
  the loosest in the suite; `SAB_TIME_SCALE=4` restores the upstream window for anyone who wants to
  see the divergence.
- `shocktube`, `shocktube-1d` and `partsteady` no longer grade the volume-average log file the deck
  writes. BATSRUS prints that log with six significant digits, so its floor is 1e-6 relative --
  five orders coarser than the eleven-digit plot files -- and the calibration run measured exactly
  that: log spreads of 1e-9, 1e-6 and 1e-9 against plot spreads of 1e-14. Grading the log would have
  forced each check's single bound up to the log's print floor and weakened the plot comparison by
  five orders. Every other check keeps its log, because there the log's spread is at or below the
  plot's.
- `partsteady` freezes a block when its largest normalised change falls below a hard threshold
  (`src/ModPartSteady.f90` lines 122 to 128, against `RelativeEps` 1e-3 and `AbsoluteEps` 1e-4). A
  round-off difference decides at which step a marginal block stops being advanced, so its spread is
  much larger than the round-off of the update itself. This is also why the Step 1 native
  investigation reproduced the upstream reference only to about 1e-8 absolute against its stated
  1e-11. The bound comes from the measured spread, not from the upstream number.
- `bx0` is graded on the x=0 and y=0 cuts and the log file rather than on the Bx=0 isosurface point
  list that upstream compares. That list is a set of interpolated surface points whose ordering and
  positions moved between platforms; the Step 1 investigation could not reproduce
  `Param/SHOCKTUBE/TestOutput/bx0_ref.out` within the upstream 1e-3. The isosurface is still written
  by the run, it is simply not graded.

`kelvinhelmholtz-hd`, `kelvinhelmholtz-mhd` and `ex-shocktube-rayleigh-taylor` carry the `chaotic`
flag. All three grow from a fixed, deterministic seed rather than from round-off, so the graded
window is deterministic; the flag records that a long enough run of any shear or interface
instability eventually amplifies round-off, and every one of them exposes `SAB_TIME_SCALE` so a
reviewer can shorten the window.

## Runtime and budget

The corrected final self-validation measured 329.6 s of graded run time across the 24 nominal
checks on the declared 8 cores and 6 GB (`suite_seconds_nominal` in
`comment/pipeline/runtime-metadata.json`). The catalogue's conservative declared expected runtimes
sum to 724 s, and `suite_budget_s` remains 900 s: the measured suite fits inside it, so no check had
to be shortened for the budget and none was.

What does not fit in fifteen minutes is the build. Every one of the 24 checks reconfigures and
rebuilds BATSRUS inside its own `run.sh`, because every official BATSRUS test chooses its own
compile-time equation set, user module, block size and ghost-cell count. The corrected final record
reports 1454 s of build time across the nominal checks, about 61 s each on 8 cores; the nominal and
variant solve walls were 1787.501 s and 1918.532 s. The budget deliberately excludes builds
(`run.sh` prints `SAB_BUILD_SECONDS` and the driver records them separately). Including both solves
and verification, the exact selfcheck ran from 19:45:20Z to 20:47:12Z, about sixty-two minutes.

Two checks were shortened from their upstream window, both for reasons that are not the budget and
both exposed as `SAB_TIME_SCALE`: `ex-shocktube-rotation` runs half a rotation instead of three
(the full window is about eight minutes of run time and writes a fifty-frame movie of a 42 MB
refined plane; the plot cadence is also set to write only the graded frame), and
`ex-shocktube-sphalfven` runs to t = 1 instead of t = 4 because the upstream window is past the
point where two legitimate runs of that deck agree at all.

## Open items for the reviewer

- **No separate two-build floor was measured.** `evidence.floor` is null in every rubric. The
  evidence for each bound is the in-container nominal-versus-variant spread plus, where upstream
  ships a reference, the Step 1 native reproduction of that reference within upstream's own DiffNum
  tolerance on a different compiler and MPI stack (gfortran 15.2 and Open MPI 5 on macOS/arm64
  against the Debian gfortran 12 and Open MPI 4.1 of the container). A dedicated two-toolchain or
  -O3/-O2 floor would sharpen both and was left out for time.
- **The three excluded examples** listed above are upstream problems, not packaging ones; a reviewer
  who can get `ModUserPointImplicit.f90` to compile, or who has the `Observation/` input for
  `ModUserCylOutflow.f90`, can add those two checks back with the specs already written here.

## Blind spots

- **No GPU path.** The pinned build has no OpenACC compiler, so `src/ModUpdateStateFast.f90` is
  exercised only in its CPU form. A port that is correct on the CPU and wrong on the device would
  pass here; that is the same limitation the upstream `*_gpu` test targets have on this hardware.
- **Constrained transport and projection are configured but lightly driven.** No official test of
  this module selects `#DIVB UseConstrainB` or `UseProjection` as its primary divergence control;
  the checks cover the eight-wave source directly (every `Mhd` deck leaves `UseDivbSource` at its
  default true: `ex-shocktube`, `shockround`, `fastwave`, `bx0` and the rest) and hyperbolic
  cleaning through the three `MhdHyp` decks (`shocktube`, `shocktube-1d`, `partsteady`, which set
  `#DIVB` to false on all four alternatives), but no deck of this module sets `UseProjection` or
  `UseConstrainB` to true. A reviewer who wants those two paths graded would have to add custom
  decks.
- **Boris correction is not graded.** `src/ModBorisCorrection.f90` belongs to the module's owned
  paths, but every deck that switches it on in this repository belongs to the geospace or planetary
  modules.
- **The Roe and Linde solvers are graded, HLLD and the exact Godunov solver are not.** The decks of
  this module select Rusanov, Roe and Linde; `#SCHEME` also offers HLLD and Godunov, and no upstream
  test of `Param/SHOCKTUBE` uses them.
- **Single-precision output is avoided rather than tested.** Every graded plot is written in
  BATSRUS's ASCII IDL format, including the ones the upstream deck writes in binary; a port that
  broke only the binary plot writer would pass. That writer is `share/Library/src/ModPlotFile.f90`,
  shared infrastructure rather than module source.
- **Restart is not covered.** This module has no `*_restart` test target; `src/ModRestartFile.f90`
  is shared infrastructure and is graded by other modules of this codebase.
- **Step counts are compared implicitly.** The graded log files carry one row per saved step, so a
  port that reached the same physical state in a different number of steps would fail on a shape
  mismatch. All these decks are either fixed-step or CFL-limited with the end time hit exactly, so
  the step count is stable; a reviewer who disagrees can drop the log files from the rubrics.
