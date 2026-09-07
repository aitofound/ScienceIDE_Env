# fastwave-amr-3d

Upstream test: `make test23_3d`. Policy: `pointwise`.

## The test

The three-dimensional form of the adaptively refined fast-wave problem.

`run.sh nominal` copies the pinned source into a scratch tree, installs it
(`./Config.pl -install=BATSRUS -compiler=gfortran`), builds the AMReX library
that FLEKS needs, configures `./Config.pl -default -amrex3d -v=Empty,PC/FLEKS,GM/BATSRUS`; `./Config.pl -o=GM:u=Default,e=MhdAnisoP,ng=2,g=8,8,8 -o=PC:lev=9`, builds `SWMF.exe`, makes the run
directory the upstream `rundir` target makes, and runs 1 run of `SWMF.exe`. The graded
window is the fast magnetosonic wave to t = 5.0. Post-processing is the upstream `PostProc.pl`, which merges
the per-rank pieces into the formatted ASCII IDL files listed below.

`run.sh --help` prints the runtime knobs. `SAB_STOP_SCALE` multiplies every
positive iteration count and simulated end time of the deck's `#STOP` blocks;
its graded default of 0.75 is the shortened upstream physics window used for the 36-check calibration. `SAB_MPI_RANKS` is
the rank count (the upstream `Makefile.test` runs `mpiexec -n 2`) and `SAB_MAKE_JOBS` only changes how fast the
build goes. The graded values are the defaults.

Graded files, all of them ASCII:

- `pc_z0_fluid.out` (formatted ASCII IDL plot file): the last frame matching `RESULTS/3D/PC/z=0*.out` in the run directory
- `pc_energy.log` (ASCII log table): the last frame matching `RESULTS/3D/PC/log_pic_n*.log` in the run directory

## The two initial conditions

`ic/nominal/` holds the deck exactly as the pinned tree ships it.
`ic/variant/` is the same input with one number changed: the GM #UNIFORMSTATE mass density. The
value is multiplied by 1 + 2e-10 and printed to twelve significant digits, a
relative change an order of magnitude above the last digit the coarsest graded
ASCII file carries (the plot files print eleven significant digits, the log
tables sixteen) and far below any physically meaningful difference in the
input. It is generic numerical-noise calibration: the two decks differ by
one number, and the spread between the two runs is the floor this pass policy
can be held to.

`run.sh altbuild` runs the nominal inputs on a second legitimate build of the
same source: `./Config.pl -O0` before the build, which rewrites every `OPTn`
line of `Makefile.conf` to `-O0` where the shipped gfortran template
(`share/build/Makefile.Linux.gfortran`) sets `-O3`. `OPT3` is the level both
the Fortran rules and the C++ rule of `Makefile.conf` use, so the framework,
BATSRUS and the FLEKS particle-in-cell solver are all rebuilt at `-O0`.

## The pass policy

This check uses the declared **pointwise** policy for the final state of the graded window as the upstream check reads it: pc_z0_fluid.out, pc_energy.log. The default graded runtime is 27 s (build time excluded), and `default_vs_upstream` is `upstream`. `run.sh --help` exposes `SAB_STOP_SCALE=0.75`, the rank count, and `SAB_MAKE_JOBS`; the defaults are the checked-in grading configuration.

The validator requires every rubric-listed file, finite numeric values, complete ordered schemas and physical time/coordinate/header consistency where that format carries them. It rejects missing, extra, malformed, truncated, non-finite, reordered, or wrong-time data; no row, coordinate, or location allowlist is used. Bookkeeping columns such as `nStep`/`it` are not physical observables and are not graded.

For each retained physical value, the default pointwise condition is `abs(candidate - nominal) <= 1e-12 + 0.001 * abs(nominal)` in that file's native emitted units. This is a single positional comparison of the structured-grid cell/diagnostic record, not a storage-order or dynamic-intersection test.

Graded files and coverage:

- `pc_z0_fluid.out` (`swmf_idl`).

- `pc_energy.log` (`swmf_log`).

The active-input perturbation is: The GM #UNIFORMSTATE mass density, multiplied by 1 + 2e-10 in ic/variant and printed to twelve significant digits -- a relative change far below any physically meaningful difference in the input and an order of magnitude above the last digit the coarsest graded ASCII file carries. It is generic numerical-noise calibration: the two inputs differ by one number, and the spread between the two runs is the floor this pass policy can be held to.

`run.sh altbuild` is declared as: run.sh altbuild rebuilds the same pinned source and deck with ./Config.pl -O0 (every OPTn line of Makefile.conf forced to -O0 where the shipped gfortran template builds at -O3, which is what both the Fortran rules and the C++ rule of Makefile.conf use, so the framework, BATSRUS and the FLEKS solver are all rebuilt); the floor it measures is written into evidence.floor by selfcheck.

## Evidence


This narrative is backed by the checked-in rubric and the frozen terminal record at `workspace/swmf-takeover-20260906/mhd-epic/post-freshness-calibration-20260907T0843Z/terminal/`; no science solve is rerun by this repair. The frozen nominal-versus-variant verifier row for this check recorded
`distance=9.999999983634211e-08` and `bound_fraction=0.004561911603163727` with pass=True.
The frozen alternative-build row recorded `distance=0.0`, `bound_fraction=0.0`, pass=True, identical=True.
These are calibration measurements, not hard-coded outputs; grading still runs the check against freshly generated nominal and candidate files. The source test, configuration, observable, and physical rationale remain in the preceding sections and the machine-readable rubric.
