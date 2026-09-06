# ohpt-swhpui

Upstream test: `make test22_oh_pt`. Policy: `pointwise`.

## The test

The multi-ion end of the outer-heliosphere chain: the four-neutral pickup-ion solution restarted into the SwhPui equation set with PT/FLEKS coupled, rebuilt between the prerequisite stages and the graded one.

`run.sh nominal` copies the pinned source into a scratch tree, installs it
(`./Config.pl -install=BATSRUS -compiler=gfortran`), builds the AMReX library
that FLEKS needs, configures `./Config.pl -default -v=Empty,OH/BATSRUS,PT/FLEKS`; `./Config.pl -o=OH:u=OuterHelio,e=OuterHelioPUI,ng=2,g=4,4,4 -o=PT:lev=5`, and again, between the prerequisite stages and the graded one, `./Config.pl -default -v=Empty,OH/BATSRUS,PT/FLEKS`; `./Config.pl -o=OH:u=OuterHelio,e=SwhPui,ng=2,g=4,4,4 -o=PT:lev=5`, builds `SWMF.exe`, makes the run
directory the upstream `rundir` target makes, and runs 3 runs of `SWMF.exe`, the first two ungraded and only there to produce the restart state. The graded
window is the two multi-ion prerequisite stages, then the SwhPui coupled run to t = 2. Post-processing is the upstream `PostProc.pl`, which merges
the per-rank pieces into the formatted ASCII IDL files listed below.

`run.sh --help` prints the runtime knobs. `SAB_STOP_SCALE` multiplies every
positive iteration count and simulated end time of the deck's `#STOP` blocks;
its graded default of 1 leaves the deck exactly as shipped. `SAB_MPI_RANKS` is
the rank count (the upstream `Makefile.test` runs `mpiexec -n 2`) and `SAB_MAKE_JOBS` only changes how fast the
build goes. The graded values are the defaults.

Graded files, all of them ASCII:

- `oh_z0.out` (formatted ASCII IDL plot file): the last frame matching `RESULTS/swhpui/OH/z=0_var_2_t*_n*.out` in the run directory
- `pt_z0.out` (formatted ASCII IDL plot file): the last frame matching `RESULTS/swhpui/PT/z=0_var_region0_1_t*_n*.out` in the run directory

## The two initial conditions

`ic/nominal/` holds the decks exactly as the pinned tree ships them.
`ic/variant/` is the same input with one number changed: the OH #SOLARWINDH solar-wind proton density, set in all three stage decks. The
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
