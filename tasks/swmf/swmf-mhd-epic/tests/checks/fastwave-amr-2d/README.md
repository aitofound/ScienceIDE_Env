# fastwave-amr-2d

Upstream test: `make test23_2d`. Policy: `pointwise`.

## The test

A fast magnetosonic wave crossing an adaptively refined FLEKS grid, built against the true two-dimensional AMReX library. The refinement lives inside the PIC region, so the check exercises the AMReX regrid, the level-to-level particle redistribution and the coarse-fine field interpolation as well as the particle push.

`run.sh nominal` copies the pinned source into a scratch tree, installs it
(`./Config.pl -install=BATSRUS -compiler=gfortran`), builds the AMReX library
that FLEKS needs, configures `./Config.pl -default -amrex2d -v=Empty,PC/FLEKS,GM/BATSRUS`; `./Config.pl -o=GM:u=Default,e=MhdAnisoP,ng=2,g=8,8,1 -o=PC:lev=9`, builds `SWMF.exe`, makes the run
directory the upstream `rundir` target makes, and runs 1 run of `SWMF.exe`. The graded
window is the fast magnetosonic wave to t = 5.0. Post-processing is the upstream `PostProc.pl`, which merges
the per-rank pieces into the formatted ASCII IDL files listed below.

`run.sh --help` prints the runtime knobs. `SAB_STOP_SCALE` multiplies every
positive iteration count and simulated end time of the deck's `#STOP` blocks;
its graded default of 1 leaves the deck exactly as shipped. `SAB_MPI_RANKS` is
the rank count (the upstream `Makefile.test` runs `mpiexec -n 2`) and `SAB_MAKE_JOBS` only changes how fast the
build goes. The graded values are the defaults.

Graded files, all of them ASCII:

- `pc_z0_fluid.out` (formatted ASCII IDL plot file): the last frame matching `RESULTS/2D/PC/z=0*.out` in the run directory
- `pc_energy.log` (ASCII log table): the last frame matching `RESULTS/2D/PC/log_pic_n*.log` in the run directory

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

PLACEHOLDER

## Evidence

PLACEHOLDER
