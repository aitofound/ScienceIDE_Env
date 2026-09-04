# batsrus-solar-corona-awsom: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

The module is the Alfven-wave solar model of BATSRUS, AWSoM and AWSoM-R: the
production solar-corona and inner-heliosphere application of the code. A
photospheric magnetogram is expanded into a potential field; a three-dimensional
MHD corona is then relaxed on a stretched spherical grid in which the plasma is
heated by the turbulent cascade of counter-propagating Alfven waves
(`src/ModTurbulence.f90`, `src/ModCoronalHeating.f90`, `src/ModWaves.f90`), the
dissipated energy is apportioned between protons and electrons by stochastic
heating, the electrons carry a Spitzer heat flux along the field that becomes
collisionless far out, radiative losses come from a tabulated loss function
(`src/ModRadiativeCooling.f90`) and the transition region is artificially
broadened so the grid can carry it (`src/ModChromosphere.f90`). AWSoM-R replaces
the resolved transition region by field-line threads integrated down to the
chromosphere (`src/ModFieldLineThread.f90`, `src/ModThreadedLC.f90`,
`util/EMPIRICAL/srcSC`). The module also owns the stream-aligned and signed-B
representations (`src/ModStreamAligned.f90`, `src/ModReverseField.f90`), the
AWSoM equation sets in `srcEquation/`, `srcUser/ModUserAwsom.f90`, the
line-of-sight synthesis of `src/ModWritePlotLos.f90`, the magnetogram and
potential-field tools of `util/DATAREAD/srcMagnetogram`, and the `Param/CORONA`
inputs.

Deliberately excluded, and why: the charge-state and vector-magnetogram tests
(`test_awsomchargestate` needs `srcUser/ModUserAwsomIons.f90` from the
access-restricted `srcUserExtra` clone and the `SWMF_data` CHIANTI tables;
`test_bvector` needs `srcUser/ModUserBvector.f90` from the same clone), the
SPECTRUM synthetic-spectra post-processor (`test_spectrum`, `SWMF_data`), the
HYPRE variant of FDIPS (`test_fdips_hypre`, HYPRE is off in this build), the
`test_eeggl` flux-rope setup (it drives `util/EMPIRICAL/srcEE`, which the module
cut assigns elsewhere, through a Python script), the outer heliosphere beyond
the AWSoM inner boundary (its own module) and `Param/CORONA/PARAM.in.heat.cond`
(assigned to `batsrus-nonideal-closures`). `Param/HELIOSPHERE/` was inspected as
a possible source of further examples and is dead: `Arcade`, `CME_fluxrope` and
`Heliosphere` are 2001-era files whose commands (`#PROBLEMTYPE`, `#ARCADE`,
`#HELIOSPHERE`, `#AMRINIT`) exist neither in `PARAM.XML` nor anywhere in `src/`,
so they cannot be run against the pinned tree.

## Checks

Thirteen checks. Six are the module's `Makefile.test` targets (`test_awsom`,
`test_awsomr`, `test_awsom_signb`, `test_stitch`, `test_awsom_gpu`,
`test_awsom_large_gpu`). A seventh, `awsom-bvector`, is `test_awsom_bvector`,
which the Step 2 survey had recorded as unavailable together with the other
`bvector` target: it is not. `test_bvector` needs `-u=Bvector` from
`srcUserExtra`, but `test_awsom_bvector` compiles with `-u=Awsom -e=Awsom` and
reads only files that are vendored, and it is the only configuration in which
AWSoM runs on a non-potential `B0` with the curl-`B0` momentum flux switched on,
so it was added. Two are the upstream examples of `Param/CORONA` that carry no
`Makefile.test` target (`PARAM.in.1Dwedge`, `PARAM.in.2Dwedge`). The last four
are the official tests of `util/DATAREAD/srcMagnetogram`, an owned path with its
own `Makefile` test suite and its own blessed references
(`test_harmonics`, `test_potential`, `test_fdips`, `test_fdips_wedge`): they
produce the harmonics and potential-field files that every corona run reads
through `#HARMONICSFILE` and `#LOOKUPTABLE B0`, so they belong to this module
and they were not in the Step 2 survey, which only walked `Param/`.

Every check builds the pinned source itself, in a scratch copy, with the exact
`Config.pl` line of its upstream target, and reproduces the upstream recipe step
by step. Two departures are recorded in every rubric's `default_vs_upstream`:

* `PostProc.pl -f=ascii` asks PostIDL for the ASCII form of the same IDL plot
  files instead of the raw binary form, so the graded plot values are text with
  eleven significant digits and no reader of a Fortran record layout is needed.
* The two wedge examples carry stale configuration headers
  (`./Config.pl -f -u=AwsomFluids -e=MhdWavesPe -g=6,2,2,256,1`): neither
  `srcUser/ModUserAwsomFluids.f90` nor `srcEquation/ModEquationMhdWavesPe.f90`
  exists in the pinned tree, and `Config.pl -g` has taken three arguments, not
  five, since the block count moved to `#GRIDBLOCKALL`. The configuration was
  re-derived from the PARAM files themselves (`#USERSWITCH +init +ic` with
  `#CHROMOBC` is `ModUserAwsom`; the wave energies, the separate electron
  pressure and the `Ehot` variable of the log are `ModEquationAwsom`) and the
  graded window is set by `SAB_MAX_ITERATION` because the files ask for 60000
  iterations.

## Tolerances

The tolerances were finalised by the agent under the human's blanket go-ahead
of 2026-09-04 ("go on, i consent to use either local or remote device for the
docker runs, no need for further consent" covered the runs; the calibration
stop was delegated in the same session), and they are open to revision by the
reviewer.

All thirteen checks are `pointwise`. The graded values are ASCII tables the
pinned source writes: BATSRUS `RAW`/`VAR` log files (one row per saved step, six
significant digits, `src/ModWriteLogSatFile.f90`) and IDL-format plot and field
files (eleven significant digits, `share/Library/src/ModPlotFile.f90`). One such
table holds columns twenty decades apart -- a volume-averaged density near
1e-19, field components near 1e-5 and a peak pressure near 1e-1 on one log line;
g/cm3 next to km/s next to K on one plot row -- so a single absolute floor is
meaningless for most of the table and a purely relative bound is unusable on the
velocity and field components that pass through zero inside the domain. The rule
is therefore applied per column:

    |candidate - reference| <= atol * s + rtol * |reference|

with `s` the largest absolute reference value in that column (the largest of the
whole table when the column is identically zero). `distance`, the number the
self-validation records as the spread, is the largest
`|candidate - reference| / s` over every graded value, so it is directly
comparable with `atol`.

Floors. The pinned build is bit-reproducible: two runs of the same executable in
the same directory gave byte-identical log and plot files
(`Param/CORONA/PARAM.in.AwsomR`, 2026-09-04), so the whole spread between two
legitimate runs comes from the different order of floating-point operations a
port introduces. The state is binary64 end to end
(`share/build/Makefile.Linux.gfortran` compiles with `-fdefault-real-8`).

Variants. Each corona check perturbs `#POYNTINGFLUX PoyntingFluxPerBSi`, the
single number that sets the Alfven-wave energy the whole AWSoM solution is
driven by, by two ulps of binary64 (2.2e-16 relative). This was chosen after
measuring that `#CHROMOBC NchromoSi`, the obvious alternative, is inert in
AWSoM-R -- a 5 per cent change of it left every graded file byte-identical,
because the threaded lower boundary of `ModThreadedLC` supplies the
chromospheric state instead. A 2e-9 relative perturbation of the Poynting flux
moved the AWSoM-R plot files by 7.6e-3, so these configurations amplify an input
perturbation by six to seven decades over their fifty relaxation steps; that is
why the bound is set from the measured spread and not from the perturbation
size. Each magnetogram check scales every `Br` value of its input map by two
units of the last printed digit of that file (2e-10 for the eleven-digit
`dipole11*.out` maps, 2e-6 for the six-digit `fitsfile.dat` maps); the rewrite
was verified to reproduce the input byte for byte at scale 1.0.

Measured floors (calibration selfcheck `20260904T113532Z`, run on
`ale-worker`, x86_64, 88 cores, Docker 29.1.3, 8 declared cpus; both solves
completed, 13/13 checks ran on each):

| check | measured spread | bound (atol = rtol) | factor |
|---|---|---|---|
| awsom | 6.36e-11 | 1e-6 | 15700 |
| awsomr | 7.67e-11 | 1e-6 | 13000 |
| awsom-signb | 4.85e-11 | 1e-6 | 20600 |
| stitch | 4.08e-11 | 1e-6 | 24500 |
| awsom-gpu | 7.19e-11 | 1e-6 | 13900 |
| awsom-large-gpu | 4.99e-12 | 1e-6 | 200000 |
| awsom-bvector | 2.64e-11 | 1e-6 | 37900 |
| ex-corona-1dwedge | 3.08e-13 | 1e-6 | 3.2e6 |
| ex-corona-2dwedge | 3.85e-11 | 1e-6 | 26000 |
| magnetogram-harmonics | 2.29e-9 | 1e-6 | 436 |
| magnetogram-potential | 3.40e-6 | 1e-3 | 294 |
| magnetogram-fdips | 2.10e-10 | 1e-6 | 4760 |
| magnetogram-fdips-wedge | 3.41e-6 | 1e-3 | 294 |

Three checks were revised after that run, and nothing was deleted, skipped or
weakened to make a bound fit:

* `ex-corona-1dwedge` failed at 1.78e-1 on the current-density columns `jx`,
  `jy` and `jz`. In a single radial column of cells with a monopole B0 the
  current density is identically zero by symmetry: the reference holds
  |j| <= 1.8e-11 uA/m2 at the last graded snapshot against a 5.586 G field, and
  j is exactly `0.0` in every cell of the first snapshot. Grading those three
  columns compares cancellation noise, so they are listed in
  `comparison.files[1].ungraded_columns`, reported by the validator and never
  graded; every other column, including the density, the velocity, the field,
  the wave energies and both pressures, is graded as before. With them excluded
  the same two calibration outputs give 3.08e-13. The two-dimensional wedge,
  where the current is physical, needs no such rule and measured 3.85e-11 over
  the whole file.
* `magnetogram-potential` and `magnetogram-fdips-wedge` failed at 3.4e-6. Their
  bound was raised to 1e-3. Their measured spread is not a round-off floor: both
  read a magnetogram written with six significant digits, so two units of its
  last printed digit is a relative change of 2e-6, and a potential-field solve is
  linear in its boundary data, so the output moves by the same relative amount.
  `magnetogram-fdips` runs FDIPS on an eleven-digit magnetogram, where the same
  last-printed-digit perturbation is 2e-10, and measured 2.10e-10 -- four
  decades lower. 1e-3 still sits at least a decade below the smallest fault the
  warrants name (a wrong stencil, a wrong lateral boundary or a lost processor
  boundary row moves the field by 1e-2 to order one).

The bounds are otherwise a single number, 1e-6 of the column scale for the
eleven-digit IDL files and 1e-4 for the six-significant-digit log files, chosen
from the physics rather than tuned per check: 1e-6 relative on a solar-corona
state is far below any difference between valid implementations and four or
more decades above every measured floor, and 1e-4 on a log printed with six
digits is between ten and a hundred units of its last digit.

## Blind spots

* No GPU or OpenACC path is exercised. `OPENACC=-noacc` is the vendored default
  and the `*_gpu` tests are ordinary CPU runs whose only GPU-specific content is
  the `Config.pl -opt` layout that bakes parameters into `ModOptimizeParam`.
* The line-of-sight synthesis of `src/ModWritePlotLos.f90` runs in the `awsom`
  check but its output is not graded: the `los_lgq` file is 26 MB and the check
  already grades the state the synthesis reads.
* `src/ModSpectrum.f90`, `src/ModRadioWaveImage.f90` and
  `src/ModWritePlotRadioWave.f90` are owned but untested here: their upstream
  tests need `SWMF_data`.
* `util/DATAREAD/srcMagnetogram/dipole11.f90`, the generator upstream runs to
  make the synthetic magnetogram of its own tests, is not exercised: the four
  magnetogram checks ship that generated map under `ic/` instead, because a
  check needs two initial conditions that differ and the generator has no
  inputs. It is test scaffolding, not module physics.
* The charge-state, vector-magnetogram, SPECTRUM and HYPRE configurations listed
  above are not covered at all.
