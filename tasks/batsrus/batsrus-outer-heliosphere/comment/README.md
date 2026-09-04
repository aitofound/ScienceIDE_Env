# batsrus-outer-heliosphere: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

The module is the outer-heliosphere application of BATSRUS: the supersonic solar
wind expanding into the local interstellar medium, its termination shock,
heliopause and heliotail, coupled to interstellar neutral hydrogen by charge
exchange. It owns `srcUser/ModUserOuterHelio.f90` (4895 lines) and
`srcUser/ModUserOuterHelio2d.f90` (1598 lines), the pickup-ion energy-bin
transport `src/ModPUI.f90`, the equation sets
`srcEquation/ModEquationOuterHelio*.f90` and `ModEquationSwh*.f90` (one ion plus
four neutral fluids, up to two ion fluids with separate electron pressure,
Alfven-wave turbulence and ten pickup-ion energy bins), and the parameter
directories `Param/OUTERHELIO` and `Param/HELIOSPHERE`. Everything else it uses
- BATL, the Riemann solvers, the parameter reader, the plot writers, PostIDL -
is shared infrastructure that other BATSRUS modules use too, and is not part of
this module's cut. Deliberately excluded: the inner-heliosphere AWSoM driver,
which belongs to the solar-corona module, and SWMF-coupled runs with the PT and
IH components, which cannot be exercised from the stand-alone repository.

The expensive path the checks force is the charge-exchange source term:
`user_calc_sources_expl` and the routines it calls
(`calc_charge_exchange_source`, `calc_charge_exchange_source_pui`,
`calc_photoion_source`, `calc_electron_impact_source`, `add_pui_source`)
evaluate the McNutt et al. (1998) exchange integrals between the solar-wind ion
fluid, the pickup-ion fluid and four neutral hydrogen populations in every cell
of every step, with a Lindsay-Stebbings or Maher-Tinsley cross section read from
a lookup table, on top of a five- to seven-fluid Riemann solve; and
`get_region`/`select_region` decides from the local Mach numbers, temperature
and speed which of the four Zank et al. (1996) populations each cell produces.

## Checks

Seven checks, one per suitable official test of the module. That is every
upstream input that exercises this module and runs: `Param/OUTERHELIO/` ships
eight PARAM files, of which two are the continuation halves of two of the
others, and the Makefile.test targets `test_outerhelio`, `test_outerhelio_1d`,
`test_outerhelio2d`, `test_outerhelioawsom` (start and restart halves, which
upstream grades separately and which are two checks here because the restart
runs a different equation set), `test_outerheliopui` and `test_outerheliopui_1d`
cover all of them. The module's other owned parameter directory,
`Param/HELIOSPHERE`, holds three files - `Arcade`, `CME_fluxrope` and
`Heliosphere` - that are dead fragments of a much older BATSRUS: their commands
(`#PROBLEMTYPE`, `#ARCADE`, `#HELIOSPHERE`, `#INERTIAL`, `#COROTATION`) appear
neither in `PARAM.XML` nor anywhere in `src/`, they carry no `#COMPONENT`,
`#STOP` or `#SAVEPLOT`, and BATSRUS cannot read them. They are therefore not
runnable inputs and were not made into checks. The other two files there,
`EARTH_TRAJ.in` and `SATELLITE.in`, are trajectory data, not decks. So this
module has seven usable official tests, below the ten the curator hoped for, and
no custom checks were invented to make up the number.

Every check builds the pinned source itself with the configuration of its own
upstream test (equation set, user module, block size, and for the restart check
a second build with a different equation set), runs the upstream deck or decks
with the upstream window on 2 MPI ranks, and grades the files the upstream
`_check` target grades plus, where upstream grades only a log file, the final
state as an IDL ASCII snapshot. `outerhelio` carries the `acceleration` label:
it is the widest and longest of the seven, a 3-D five-fluid heliosphere with
adaptive refinement over 170 steps.

## Tolerances

Every check is `pointwise` and none is chaotic: these runs are deterministic and
bit-identical on rerun, and they do not amplify - an initial-condition
perturbation of relative size e comes out of 500 steps as a difference of order
e, not e times a Lyapunov factor.

**The shape of the bound.** Every graded value must satisfy

    |candidate - reference| <= atol + rtol x scale

with `atol` = 1e-30 (the default absolute floor of upstream's own
`share/Scripts/DiffNum.pl`) and `rtol` 1e-4 for five checks and 3e-4 for the two AWSoM checks. What is not upstream's is the
*scale*: for the numbers in a file header (the step, the simulation time, the
grid size, the equation parameters) it is the value itself, but for the table -
the rows of the RAW log and the points of an IDL ASCII cut - it is the largest
magnitude the reference reaches in that value's own column. Every variable is
therefore held to the same fraction of its own dynamic range.

That normalisation is the one substantive decision of the calibration, and the
calibration is what forced it. With the same `rtol` applied element-wise, as
upstream's DiffNum applies it, two of the seven checks are failed by a second
legitimate run of the same physics. In `outerhelio` the transverse neutral
velocity NeuUy of the final y=0 cut is 4.6e-07 at one cell of a column that
reaches 2.5, and the two runs differ there by 7.8e-04 of that cell's value - 7.8
times an element-wise bound of 1e-4 - while the same file agrees to 3.5e-10 of
every column's dynamic range. In `outerhelio2d` the same thing happens at 3.7
times the bound, and it happens not only between the nominal and the variant but
between two builds of the pinned source at -O3 and at -O2: an element-wise bound
at this level is not achievable by the code itself. Grading a cell on a fraction
of itself when it is 1e-7 of its own column's maximum measures round-off, not
physics. The absolute floor of 1e-30 does the same job one step further down, for
the columns that are exactly zero by symmetry and the denormal noise beneath
them; it is what makes a neutral transverse momentum printed as 2.3e-36
harmless.

**How the numbers were chosen.** The bounds are 1e-4 and 3e-4, in every case ten times the
relative tolerance the upstream regression itself uses for that test
(`DiffNum.pl -t -r=1e-5`, or `-r=3e-5 -a=1e-29` for the two AWSoM checks).
Upstream's number is about one unit in the last printed digit of the log's six
significant figures: a bit-reproducibility test between builds of the same code
on one machine, appropriate for a nightly regression and too tight to be a
statement about scientific equivalence across implementations. Ten of those units
is still four or more orders of magnitude below every fault the warrants name (a
dropped charge-exchange moment, a wrong Lindsay-Stebbings cross section, a lost
neutral population, a pickup-ion bin transport that does not couple), which move
the graded columns by percent to orders of magnitude.

Three independent measurements say the bounds are achievable.

1. **Against the upstream references.** Every check's graded files were compared
   with the references in `code/batsrus/Param/OUTERHELIO/TestOutput/`, blessed
   upstream on another compiler and platform: outerhelio: log.log largest difference 1e-34 absolute, 0.00034% of the bound; outerhelio-1d: log.log bit-identical; outerhelio2d: final_z0_hd.out largest difference 1.04e-06 absolute, 0.0027% of the bound; interpolated_output.dat bit-identical; log.log not comparable with the stored reference: the 2-D log in TestOutput is from an older configuration of this test (13 columns and 400 rows against the 33 and 572 this deck writes) and the upstream check does not compare it either; outerhelioawsom: log.log bit-identical; outerhelioawsom-restart: log.log largest difference 1e-17 absolute, 3.3% of the bound; outerheliopui: log.log largest difference 1e-07 absolute, 1.7% of the bound; outerheliopui-1d: log.log bit-identical.

2. **Two builds of the pinned source.** The same source built at -O3 (the maximum
   optimisation level of `share/build/Makefile.Linux.gfortran`) and at -O2, run
   through each check's own `run.sh` on the same host, largest absolute
   difference over all graded values: outerhelio 0 (0% of its bound); outerhelio-1d 0 (0% of its bound); outerhelio2d 1.03e-06 (0.0027% of its bound); outerhelioawsom 2.05e-09 (0.00094% of its bound); outerhelioawsom-restart 1e-20 (2.6e-05% of its bound); outerheliopui 0 (0% of its bound); outerheliopui-1d 0 (0% of its bound).

3. **The variant.** Each check's `ic/variant` perturbs one active
   initial-condition input by 1 + 2e-10, two units of the last printed digit of
   the finest graded text output. Two ulps of binary64 were tried first and were
   invisible in every graded file, exactly because the solver does not amplify and
   the graded text carries 11 significant digits at best. Largest absolute
   difference: outerhelio 5e-08 (0.0037%); outerhelio-1d 3e-09 (0.00028%); outerhelio2d 2.06e-06 (0.0096%); outerhelioawsom 0.001 (0.00047%); outerhelioawsom-restart 0.001 (0.00045%); outerheliopui 2e-08 (0.00031%); outerheliopui-1d 1e-07 (8.3%).

The tightest of the seven is `outerheliopui-1d`, where the bound sits only twelve
times above the variant: the binding item there is a single flip of the last
printed digit of one log column (1.02271E-02 against 1.02272E-02), so the margin
is twelve units in the sixth significant figure of a six-figure printed table,
and a tighter bound would test the print format rather than the physics. The
two-build floor of that check is exactly zero. At the other end, five of the
seven sit between ten thousand and four hundred thousand times above everything
measured.

Where the floor is not zero, the mechanism is in the source: `get_region`
(`srcUser/ModUserOuterHelio.f90`, lines 3938 to 4137) assigns each cell to one of
the four neutral populations through hard comparisons on the local Mach numbers,
temperature and speed, so a cell on a threshold can be assigned differently by two
legitimate builds; the charge-exchange rate is an interpolation in a lookup table;
and the log's volume averages are MPI reductions whose summation order the
compiler and the rank layout may reorder.

**What the calibration changed.** The first self-validation run was a calibration
run and it changed three things. It found that the `1d VAR` snapshot of the AWSoM
restart is written as `1d__var_1_*` and not `1d_var_1_*`, so that check produced
no plot file and failed; the glob was corrected. It showed that mpiexec forwards
its own standard input to rank 0 and so drained the list of checks the verifier
driver was iterating over, which stopped the suite after the first check; every
`run.sh` now redirects its standard input from /dev/null. And it showed that an
element-wise relative bound cannot be met by two legitimate runs, which is where
the column normalisation above came from. The tolerances themselves, the windows
and the variants were then finalised; the `expected_runtime_s` of each rubric is
the run time the calibration run measured, in seconds, on four declared cores.
The tolerances were finalised by the agent under the human's blanket go-ahead for
STOP 4 (recorded 2026-09-04) and are open to revision by the reviewer.

## Blind spots

- No GPU path is exercised. `Config.pl -f` (the fast, GPU-portable update) is
  used only where the upstream test uses it (`outerhelio2d`); `OPENACC=-noacc`
  everywhere, as in the upstream CPU test run. A port that is correct on the CPU
  and wrong only under OpenACC would pass.
- Photoionization is exercised by one deck only (`PARAM.in.awsom`,
  `#PHOTOIONIZATION T` with a rate of 8e-8, so by the `outerhelioawsom` check
  and by the start half of `outerhelioawsom-restart`) and electron-impact
  ionization by three (`PARAM.in.awsom`, `PARAM.in.pui`, `PARAM.in.pui.restart`).
  The other four checks run with both off, as their upstream decks do, so
  `calc_photoion_source` and `calc_electron_impact_source` are covered by
  fewer checks than the charge exchange is.
- The neutral populations are graded through their effect on the state, not
  directly: `get_region` decides the population index per cell, and only the
  `outerhelio`, `outerhelioawsom` and `outerheliopui` checks grade a plot that
  carries that index explicitly (the `fluid` variable of the y=0 VAR cut). A
  port that mislabels a population but produces the same fluxes would only be
  caught through the small differences that follow.
- The windows are the upstream ones: 30 to 500 steps. None of these runs reaches
  a converged heliopause; they are regression windows, not physics validation.
  A port whose error grows only after thousands of steps would pass.
- `srcUserExtra` and `SWMF_data` are not vendored, but no test of this module
  needs them.
- The lookup tables (`ChargeExchangeRate_LS.dat`, `solarwind2D.dat`, the two
  1 AU observation files) are read from the pinned tree by every check; a port
  that changed the table interpolation but not the tables would be caught, a
  port that shipped different tables would not be, because the tables are part
  of the source under test.

## Where things were run

The native investigation of every check (build, run, comparison with the
upstream references, the two-build floor) ran on the x86 worker
`huangzesen@136.114.2.6` (88 cores, Ubuntu 24.04, gfortran 13.3, Open MPI 4.1)
and on this macOS machine (gfortran 15.2, Open MPI 5). The Docker images and the
self-validation ran on the same x86 worker under the human's recorded consent of
2026-09-04. The tolerances were finalised by the agent under the human's blanket
go-ahead for STOP 4 and are open to revision by the reviewer.
