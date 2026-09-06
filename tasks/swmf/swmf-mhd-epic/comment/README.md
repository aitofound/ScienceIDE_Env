# swmf-mhd-epic: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

The module is FLEKS, the C++17 semi-implicit particle-in-cell and
particle-tracker code the framework carries in two slots: `PC/FLEKS` embedded in
the global magnetosphere through `CON/Interface/src/CON_couple_gm_pc.f90`
(MHD-EPIC, and MHD-AEPIC when the kinetic patch follows the current sheet), and
`PT/FLEKS` riding on the outer-heliosphere solution through
`CON_couple_oh_pt.f90` as a test-particle and neutral-fluid model. The two slots
are the same source: the mover, the moment deposition, the mass matrices, the
semi-implicit field solve with its GMRES iteration, the divergence-E
correction, the AMReX regridding of the particle region, the restart writer and
the ASCII plot writer all live under `PC/FLEKS/src` and `PC/FLEKS/include`,
which is why they are packaged as one module. The BATSRUS and OH/BATSRUS solver
internals that provide the MHD half of each coupled run belong to
`code/batsrus` and are not re-cut here. `PT/AMPS` is not vendored (its only
SWMF test needs the access-restricted `srcUserExtra`) and `PT/MITTENS` belongs
to `swmf-sep-transport`.

Building FLEKS offline was the first problem. `share/Scripts/Config.pl`
(`set_amrex_`, around line 905) git-clones `SWMFsoftware/AMREX` into
`util/AMREX` when it is missing and then builds it with a hard-coded
`make -j 4`. AMReX is vendored in the pinned tree, so nothing is cloned; every
`run.sh` runs the identical `./configure --prefix InstallDir<N>D --comp gnu
--enable-fortran-api no --debug no --enable-tiny-profile yes --dim <N>
--allow-different-compiler yes` and `make install` itself with `SAB_MAKE_JOBS`
jobs before calling `Config.pl`, so `Config.pl` finds
`util/AMREX/InstallDir<N>D/lib/libamrex.a` in place and only makes the
`InstallDir` symlink. Same flags, same library, only the job count differs
(101 s at the hard-coded `-j 4`, 49 s at `-j 16`). The image needs `cmake` and
`g++` on top of the BATSRUS dependency line; nothing in the build reaches the
network, and the `srcUserExtra` clone `Config.pl -install` attempts fails fast
under `GIT_TERMINAL_PROMPT=0` and is reported and ignored, as upstream intends.

The eighteen coupled checks are the eleven official `Makefile.test` targets that
use FLEKS, with every graded stage of a multi-stage target packaged as its own
check: `test17` becomes `alfven-2steps`, `alfven-restart`, `alfven-4steps` and
`alfven-adapt`; `test21` becomes `ohpt-4neu-start`, `ohpt-4neu-couple` and
`ohpt-swh`; `test22` becomes the three `ohpt-pui-*`/`ohpt-swhpui` checks. A
check whose graded stage needs an earlier one reruns that earlier stage inside
its own `run.sh` as an ungraded prerequisite, and `ohpt-swh` and `ohpt-swhpui`
also reconfigure and rebuild SWMF for the second equation set between the
prerequisites and the graded run, exactly as the upstream target does. The
eighteen standalone checks are the decks of `PC/FLEKS/tests`, which the shipped
`validate_tests.py` runner drives; they are official tests of the FLEKS
repository, they build the standalone `FLEKS.exe` the way `PC/FLEKS/tests/README.md`
documents (`./Config.pl -lev=2 -u=Exo`, with `-amrex2d` for
`fleks-reconnection-amr`, which the runner lists in `AMREX2D_TESTS` and skips
under a 3-D build), and they cost about a third of a coupled check to run.

Determinism was checked before the pointwise policy was chosen. FLEKS does not
seed from the clock or from `std::random_device`: `Particles::calc_random_seed`
(`PC/FLEKS/include/Particles.h`, lines 763-787) builds the seed arithmetically
from the refinement level, the global cell index, the cycle number, the species
id and the particles-per-cell vector, and feeds it to the Park-Miller generator
of `share/Library/src/RandNum.h`, whose whole state is one `int32_t`. Two runs
of the same deck therefore draw the same numbers. What is not fixed across
decompositions is the order of the MPI reductions and of the particle lists, so
every `run.sh` fixes the rank count (`SAB_MPI_RANKS`, default 2 for the coupled
checks as `Makefile.test` runs them, 1 for the standalone decks as
`validate_tests.py` runs them) and says so in its help text.

## Tolerances

<FILL: written from selfcheck run 1>

## Decks considered and left out

- `Param/PARAM.in.test.GMPC.start` and `Param/PARAM.in.test.GMPC.3D.start`:
  shipped GM+PC example decks with no `Makefile.test` target. Both were built
  and run (the first with `-o=GM:u=Default,e=MultiIonPe,ng=2,g=8,8,1 -o=PC:lev=9`,
  which is what its two-ion-plus-electron-pressure `#UNIFORMSTATE` block needs;
  the second with `-o=GM:u=GemReconnect,e=MhdHyp,ng=2,g=4,4,4`, which is what its
  `#USERINPUTBEGIN` block of `#GEM`, `#GEMPARAM` and `#GEMPERTURB` commands
  needs). Both abort at iteration 0 with
  `ERROR: In all directions, the PIC grid cell number (defined by #PICGRID)
  should be divisible by the patch size, which is defined by #PICPATCH.` The
  decks carry no `#PICPATCH` block and their `#PICGRID` extents do not divide by
  the default patch size, so they are stale with respect to the pinned FLEKS.
  Fixing them would mean editing an upstream input, so they are left out.
- `Param/PARAM.in.test.GMPC.start.aniso`, `PARAM.in.test.GMPC.start.2step.aniso`
  and `PARAM.in.test.GMPC.restart.2step.aniso`: the same family with an
  anisotropic-pressure equation set, and the same `#PICPATCH` failure.
- `Param/PARAM.in.test.GMPC.aniso.AMPS`, `.AMPS.2step`, `.AMPS.2step.restart`,
  `.AMPS.corr`, `.AMPS.dynamic`, `.AMPS.fluxrope`, `PARAM.in.test.GMPT` and
  `PARAM.in.test.GMPT.Europa`: PT/AMPS decks. `PT/AMPS` is not in the pinned
  tree and its only SWMF test (`test13`) additionally needs the access-restricted
  `GM/BATSRUS/srcUserExtra`.
- `Param/PARAM.in.test.OHPT`, `PARAM.in.test.OHPTpui`,
  `PARAM.in.test.OHPTshocktube`, `PARAM.in.test.OHPTpuishocktube` and
  `PARAM.in.test.restart.OHPT`: OH decks whose `#COMPONENTMAP` maps PT but which
  carry no `#BEGIN_COMP PT` block at all; they are the AMPS-era forms of the
  outer-heliosphere coupling that the `PARAM.in.test.OHPT.FLEKS.*` decks
  replaced, and the FLEKS-era decks are all packaged.
- `PC/FLEKS/tests/electronimpact`: a standalone deck with no `#SAVEPLOT` block,
  so its only output is the energy log; packaging it would grade one file where
  every other standalone check grades a plot frame and the log. Its physics (the
  Voronov electron-impact ionization source) sits next to `chargeexchange` and
  `photoionization`, which are packaged.
- `PC/FLEKS/tests/performance`: a benchmark, not a test; it has no README and no
  validator, and `validate_performance.py` writes a timing summary rather than a
  solution.
- `PC/FLEKS/tests/bc_wave`, `bc_inflow`, `chemistry`, `recombination`,
  `hyper_resistivity`, and the `hybrid` variants of `freestream`,
  `reconnection`, `reconnection_amr` and `shock`: all runnable, all left out only
  to keep the suite at 36 checks; the boundary, source-term and hybrid-solver
  paths they exercise are already covered by `fleks-bc-reflecting`,
  `fleks-bc-absorb`, `fleks-chargeexchange`, `fleks-photoionization`,
  `fleks-beam-hybrid` and `fleks-whistler-hybrid`.
- The 2-D AMReX build of the standalone suite (`PC/FLEKS/Config.pl -amrex2d`):
  the shipped runner supports it and skips the decks that need true 3-D, and
  under the default 3-D build it skips `reconnection_amr` instead
  (`AMREX2D_TESTS` and `AMREX2D_EXCLUDED_TESTS`, `validate_tests.py` lines
  332-346). `fleks-reconnection-amr` therefore builds against the true
  two-dimensional library and every other standalone check against the
  documented default 3-D one; the checks the 2-D suite excludes
  (`chargeexchange`, `photoionization`) are packaged only in their 3-D form.

## Blind spots

<FILL: written from selfcheck run 1>
