# swpc-aepic

Upstream test: `make test_swpc_aepic`. Policy: `pointwise`.

## The test

MHD-AEPIC: the SWPC operational Geospace configuration (GM/BATSRUS + IE/Ridley_serial + IM/RCM2) with an adaptively embedded FLEKS region in the magnetotail, driven by the shipped 2014-04-10 IMF file. This is the heaviest check of the module and the only one where the PIC region is embedded in a real magnetosphere rather than a periodic box.

`run.sh nominal` copies the pinned source into a scratch tree, installs it
(`./Config.pl -install=BATSRUS -compiler=gfortran`), builds the AMReX library
that FLEKS needs, configures `./Config.pl -default -amrex`; `./Config.pl -v=Empty,GM/BATSRUS,IE/Ridley_serial,IM/RCM2,PC/FLEKS`; `./Config.pl -o=GM:u=Default,e=Mhd,ng=2,g=8,8,8,IE:g=91,181`, builds `SWMF.exe` and `INTERPOLATE.exe`, makes the run
directory the upstream `rundir` target makes, and runs 1 run of `SWMF.exe`. The graded
window is the operational Geospace startup window with a PIC region in the tail, 20 PC steps. Post-processing is the upstream `PostProc.pl`, which merges
the per-rank pieces into the formatted ASCII IDL files listed below.

`run.sh --help` prints the runtime knobs. `SAB_STOP_SCALE` multiplies every
positive iteration count and simulated end time of the deck's `#STOP` blocks;
its graded default of 1 leaves the deck exactly as shipped. `SAB_MPI_RANKS` is
the rank count (the upstream `Makefile.test` runs `mpiexec -n 2`) and `SAB_MAKE_JOBS` only changes how fast the
build goes. The graded values are the defaults.

Graded files, all of them ASCII:

- `gm_y0_var.out` (formatted ASCII IDL plot file): the last frame matching `GM/IO2/y=0_var_1_e*.out` in the run directory
- `gm_z0_var.out` (formatted ASCII IDL plot file): the last frame matching `GM/IO2/z=0_var_2_e*.out` in the run directory
- `pc_y0_var.out` (formatted ASCII IDL plot file): the last frame matching `PC/plots/y=0_var_region0_1_t*_n*.out` in the run directory
- `pc_z0_var.out` (formatted ASCII IDL plot file): the last frame matching `PC/plots/z=0_var_region0_0_t*_n*.out` in the run directory

## The two initial conditions

`ic/nominal/` holds the deck exactly as the pinned tree ships it.
`ic/variant/` is the same input with one number changed: the GM #BODY number density held at the ionospheric inner boundary and used for the initial state inside the body. The
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
