# ex-shocktube-fivemoment

Upstream test: `code/swmf/GM/BATSRUS/Param/SHOCKTUBE/PARAM.in.FiveMoment` (an upstream example: no Makefile.test target and no reference output, so the pinned build generates this check's reference and the physics of the example anchors it). Policy: `pointwise`.

## The test

The five-moment electromagnetic (light) wave of the SHOCKTUBE example set on 40 cells with a light speed of 100 to t=2, with the plot written at every step: a By/Ez pair launched as a left-going wave, so the check follows Maxwell's equations as the five-moment system solves them, with equal ion and electron masses. `run.sh <ic>` copies the pinned source tree to a scratch
directory, installs it (`./Config.pl -install -compiler=gfortran`), configures it
for this check (`./Config.pl -default -u=Default -e=FiveMoment -ng=2 -g=10,1,1`), builds `BATSRUS.exe` and `PostIDL.exe`,
creates a run directory with `make rundir`, copies `ic/<ic>/PARAM.in` into it, runs
`mpiexec -n 2 ./BATSRUS.exe` and merges the per-processor pieces with
`./PostProc.pl -m -replace RESULT`: the same sequence the upstream `Makefile.test`
recipes of this module use. The graded file is `final.out`: the last snapshot of the `1d__mhd_1_*` plot
series, the state at the graded stop time in formatted ASCII IDL. Graded window:
t=2, the upstream window. Default versus upstream: ic/nominal adds the line '0.0  StateVar HypE' to the #UNIFORMSTATE block. The upstream example predates the HypE variable of srcEquation/ModEquationFiveMoment.f90 (nVar=17) and lists only 16 values, so BATSRUS aborts on it with 'Error reading missing variable ElP'. The added value is the hyperbolic divE cleaning potential, zero in every other five-moment deck of this module; nothing else differs. Runtime knobs (`run.sh --help`):
`SAB_TIME_SCALE` (multiplies every tSimulationMax of the deck; 1 is the graded
window), `SAB_PLOT_FRAMES` (informational, default matching the measured frame count: see below), `SAB_MPI_RANKS` (2 is the graded value) and `SAB_BUILD_JOBS` (build only).
Measured run time about 10 s on two cores (2026-09-13; shared-host load varies this), plus about 58 s to build
BATSRUS for this configuration; the build is reported separately as
`SAB_BUILD_SECONDS` and is not counted against the suite budget. Under the 2026-09-13
window revision this check is not rewritten by SAB_PLOT_FRAMES: the graded
`#SAVEPLOT` entry saves on DnOutput=1 (every step) while the window itself is
time-driven (tSimulationMax), and this deck has no fixed timestep (no
`#TIMESTEPPING` dt), so the step count in the window cannot be predicted before
the run and a cadence cannot be computed from it. The run already writes many
more than five frames of the graded series, so `run.sh` reports the measured
count on `SAB_PLOT_FRAMES` without rewriting the deck.

The alternative build (`./Config.pl -O0` in place of the shipped gfortran template's `-O3`, same pinned source and deck) reproduces this deck's graded output bit-identically (measured 2026-09-05): the -O0/-O3 difference does not show up at all, so the altbuild floor is exactly 0. ## The two initial conditions

`ic/nominal/PARAM.in` is the graded deck. ic/variant/PARAM.in raises the ion density of the #UNIFORMSTATE block from 1.0 to 1.0000000000000004, two units in the last place of the IEEE binary64 representation (4.4e-16 relative): the size of one rounding difference, which is what a faithful port introduces at every arithmetic operation. `run.sh altbuild` runs `ic/nominal/PARAM.in` on the same pinned source and deck, built with `./Config.pl -O0` immediately before `make BATSRUS` instead of the shipped gfortran template's `-O3`, a legitimately different build of the identical configuration.

## The pass policy

The graded observable is every value of the final ASCII IDL plot file: the simulation time, the plot parameters, and the coordinate and every variable of every cell of the one-dimensional cut snapshot at the graded stop time, compared value by value under |candidate - reference| <= 1e-06 + 1e-08|reference|. What the check exercises: the five-moment electromagnetic (light) wave of the SHOCKTUBE example set on 40 cells with a light speed of 100 to t=2, with the plot written at every step: a By/Ez pair launched as a left-going wave, so the check follows Maxwell's equations as the five-moment system solves them, with equal ion and electron masses. The bound is physical: the module's own work is the ion-electron source coupling of the five-moment system. A port that drops or mis-signs the Lorentz force on either fluid or the current source of Ampere's law (src/ModIonElectron.f90 ion_electron_source_impl builds all three from the charge-to-mass ratios of src/ModMultiFluid.f90), that updates those stiff sources explicitly instead of through the point-implicit solver (src/ModPointImplicit.f90 update_point_implicit), or that drops the hyperbolic divE cleaning, moves the graded state by 1e-2 to 1e0 relative and usually makes the run unstable within a few plasma periods -- six to eight decades above this bound. A port that keeps the physics and only reorders the arithmetic moves it by the rounding floor measured below. The relative term is one to four decades tighter than the tolerance upstream itself uses for this family of tests (DiffNum.pl -r=1e-5 for the multi-fluid and multi-ion tests, -r=8e-5 for the five-moment tests), and the absolute term governs the components that are zero or near zero by symmetry, where no relative bound has meaning. It is achievable: the measured floor of this check is the nominal-versus-variant spread: two runs of the same build, differing only by the initial-condition perturbation described under variant, differ by at most 1.16e-09 over every graded value (native pre-flight on the packaging worker, gfortran 12.2, Open MPI, 2 ranks, 2026-09-04; the same pair under sab.py task selfcheck is recorded in evidence). The bound sits 862 times above that floor. The mechanism that sets the floor is the printed precision of the graded file: BATSRUS writes the formatted IDL plot file with the format es18.10 (share/Library/src/ModPlotFile.f90:171), eleven significant digits, so a difference below 1e-10 of a value's own size cannot appear in it at all, while the state itself is double precision end to end (State_VGB in src/ModAdvance.f90).

## Evidence

Native pre-flight on the packaging worker (88-core x86, gfortran 12.2, Open MPI 4.1, 2 MPI ranks, 2026-09-04): the nominal and the variant deck were run in the same build and their graded files compared value by value. Largest absolute difference over every graded value: 1.16e-09; the bound is atol 1e-06 with rtol 1e-08, 862 times the measured spread on the absolute term.  Measured run time 4 s. The self-validation run recorded by `sab.py task selfcheck` repeats the same comparison inside the oracle image; its numbers are in `rubric.json` under evidence and in the review table of `comment/README.md`. This section never describes the reference outputs themselves.
