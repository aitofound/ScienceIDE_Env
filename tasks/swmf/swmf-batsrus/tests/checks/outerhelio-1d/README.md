# outerhelio-1d

Upstream test: `code/swmf/GM/BATSRUS/Makefile.test target test_outerhelio_1d, deck Param/OUTERHELIO/PARAM.in.1d`. Policy: `pointwise`.

## The test

`run.sh` builds the pinned BATSRUS with the upstream test's configuration and runs the 1-D charge-exchange deck: a periodic box of a single 4x4x4 block, uniform ion and four neutral hydrogen fluids, integrated for five years at a fixed step of 0.01 year. There is no shock and no boundary driving, so the solution is the charge-exchange source term alone; it is the cheapest check in the suite and the one that isolates the source term best. The graded `y=0 VAR` plot series writes 6 frames
(1 initial save plus `SAB_PLOT_FRAMES=5` periodic saves, every 1 year of the
5-year window); the last one is graded. The knobs are `SAB_ITER_SCALE` (every
`#STOP MaxIteration` and `tSimulationMax` of the deck), `SAB_PLOT_FRAMES`
(rewrites the plot's `DtSavePlot` cadence to window / `SAB_PLOT_FRAMES`, floor 5
saves; shortened from a single end-of-run write on 2026-09-13), `SAB_MPI_RANKS`
(ranks; the graded values are the
2-rank results) and `SAB_MAKE_JOBS` (build parallelism); the defaults are the
graded values, 5 s of run time declared on 4 cores, after a build of
about 84 s that the budget does not count.

## The two initial conditions

`ic/nominal` holds the upstream deck (or decks) of this test exactly as
`code/swmf/GM/BATSRUS/Param/OUTERHELIO/` ships them, with the `y=0 VAR` plot entry switched to `idl_ascii` and, since 2026-09-13, to a periodic write every 1 year of the 5-year window (6 frames; PostIDL then writes 11-digit text instead of a real4 binary record, and the graded snapshot is the last of these). `ic/variant` is the same
deck (or decks) with the initial ion density Rho of #UNIFORMSTATE, 0.01 amu/cc multiplied by 1 + 2e-10. The two directories differ byte-wise, and
the graded outputs differ: the nominal-versus-variant distance is what two runs of
the same physics with a perturbation at the printed precision come out at, and it
is evidence for the pass policy, not the bound itself.

`run.sh altbuild` runs `ic/nominal` on the same pinned source and the same deck(s), built with `./Config.pl -O0` appended after this check's own configure line(s) (every `OPTn` level of `Makefile.conf` rewritten from the shipped `-O3` to `-O0`) instead of the default build; grading never uses it, while self-validation measures the check's floor between the two legitimate builds from it.

## The pass policy

The graded observable is the 39-column volume-average history of the charge-exchange relaxation at 51 saved times and the final y=0 cut of the ion and the four neutral fluids. Every graded value is compared under |candidate - reference| <= atol + rtol x scale, with rtol = 1e-4 and atol = 1e-30; scale is the largest magnitude the reference reaches in that value's own column of the table (and the value itself for the numbers in the file header: the step, the simulation time, the grid size and the equation parameters). 1e-4 is the relative tolerance of this check. It is ten times the one the upstream regression uses for this very test (share/Scripts/DiffNum.pl -t -r=1e-5, with DiffNum's default absolute floor of 1e-30), and it is applied to a different scale; what changed is the normalisation, and the calibration says why: the worst element-wise relative difference two legitimate runs produce anywhere in this check is 2.8e-10, so the normalisation changes little here; it is used for consistency with the other six checks, and the absolute floor is what keeps the log's symmetry-zero columns from being graded on their own round-off. Physical: there is no shock and no advection here: the whole solution is the charge-exchange source term relaxing a uniform ion-plus-neutral box over five years, so a wrong cross section, a dropped momentum or energy moment, or a mis-indexed population changes the log columns directly and by percent or more. This is the cheapest check in the suite that isolates calc_charge_exchange_source from every other part of the solver. Every one of those faults moves the graded columns by four or more orders of magnitude beyond this bound. Achievable: the same pinned source built at -O3 and at -O2 and run through this check's own run.sh agrees bit for bit on every one of the 2304 graded values; the variant, an input perturbation of two units of the last printed digit, reaches 0.00028% of the bound (largest absolute difference 3e-09, in final_y0_var.out); and against the references blessed upstream on another compiler and platform, log.log: bit-identical over all 1734 values. Where the floor is not zero, the mechanism is in the source: get_region (srcUser/ModUserOuterHelio.f90, lines 3938 to 4137) assigns each cell to one of the four neutral populations through hard comparisons on the local Mach numbers, temperature and speed, so a cell sitting on a threshold can be assigned differently by two legitimate builds; the charge-exchange rate is read from a lookup table by interpolation (#LOOKUPTABLE ChargeExchange, Param/OUTERHELIO/ChargeExchangeRate_LS.dat); and the volume averages in the log are MPI reductions whose summation order the compiler and the rank layout may reorder. This bound sits 357,623 times the largest difference the variant produces, while the two builds of the pinned source are bit-identical on every graded value, so their floor is exactly zero.

## Evidence

The two-build floor was measured on the x86 worker by building the pinned source
twice from this check's own `run.sh`, once as the check builds it and once with
`./Config.pl -O2` appended to its configure lines, running both against
`ic/nominal` and taking the largest absolute difference over all graded values
(`~/.sciaccel_pipeline/batsrus/floor/swmf-batsrus/floor.sh`). The
cross-platform agreement quoted above is this check's own `run.sh` on macOS with
gfortran 15.2 against the reference `code/swmf/GM/BATSRUS/Param/OUTERHELIO/TestOutput/`
blessed upstream on another platform, compared over the file upstream compares.
The numbers are in `rubric.json` (`evidence`). The in-container
nominal-versus-variant spread and the run time on the declared cores are written
by `sab.py task selfcheck` into `rubric.json`
(`evidence.self_validation_spread`) and `comment/pipeline/self-validation.json`.
Nothing here describes the reference outputs.

The altbuild floor -- the same pinned source built with `./Config.pl -O0` instead of the shipped `-O3` -- is exactly zero: `run.sh altbuild` reproduces `run.sh nominal` bit for bit on every graded value.
