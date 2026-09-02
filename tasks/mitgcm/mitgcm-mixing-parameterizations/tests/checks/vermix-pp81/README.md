# vermix-pp81

Upstream test: `code/mitgcm/verification/vermix/input.pp81`. Policy: `pointwise`.

## The test

Pacanowski-Philander Richardson-number closure in the same column. `run.sh` builds one MITgcm executable for this
configuration with the tree's own `tools/genmake2` (the build configuration
under `mods/`: `SIZE.h`, `packages.conf` and the option headers of the
upstream experiment, gfortran optfile `linux_amd64_gfortran`, one process,
tiles only), then runs it on the deck under `ic/<ic>/`. Configuration:
verification/vermix with the input.pp81 overlay: the same 1x1x26 forced column with the same forcing and frozen advection, but data.pkg selects usePP81 alone, so the vertical viscosity is the algebraic Pacanowski-Philander function of the local gradient Richardson number, nu = PPnu0/(1+PPalpha*Ri)**PPnRi with the package defaults PPnu0=1e-2, PPalpha=5 and PPnRi=2, capped by the deck's PPviscMax=1 and floored by the background viscArNr, and the diffusivity is that viscosity divided once more by (1+PPalpha*Ri); PPwriteState is on; the window is 360 steps of 1200 s, five days, closing the overlay's dynDiag and DiagMXL_2d diagnostics intervals at the final iteration..

The production path it forces: pkg/pp81/pp81_calc.F, which evaluates the gradient Richardson number through pkg/pp81/pp81_ri_number.F and forms the viscosity and diffusivity profiles, and pkg/pp81/pp81_calc_visc.F and pp81_calc_diff.F which hand them to model/src/impldiff.F and model/src/solve_tridiagonal.F; density gradients come from model/src/find_rho.F each step..

Runtime knobs (`run.sh --help`): `SAB_STEPS` (default 360, the graded
value; the upstream deck runs 20 steps of 1200 s) scales the
run linearly, and `SAB_BUILD_JOBS` (default 4) only the build. Expected run
time on the declared resources, build excluded: about 3 s;
the per-check build (roughly 40 s on an x86_64 host) is reported by `run.sh`
as `SAB_BUILD_SECONDS` and does not count against the suite budget.

## The two initial conditions

`ic/nominal` is the upstream deck, assembled as `testreport` assembles it
(the experiment's `input/`, the input.pp81/ overlay),
with these deck edits: `nTimeSteps` set to the graded window, `dumpFreq`,
`pChkptFreq` and `chkptFreq` set to zero and `dumpInitAndLast=.TRUE.` so that the only
state written is the initial and final dump, and
`writeBinaryPrec=64` so the dump is double precision and `useSingleCpuIO=.TRUE.` so the dump is one global file per field rather than one per tile; `useMNC=.FALSE.` in `data.pkg`; `PPdumpFreq=432000.` in `data.pp81`.

`ic/variant` holds only the deck files that differ, laid over `ic/nominal` by
`run.sh`; the difference is `PPalpha=5.000000000000002` in `data.pp81` instead of 5:
two ulps of the graded precision (binary64) on a parameter that enters the
tendency from the first step, a distinct double, so the two runs differ at
round-off level from the first step. The
spread between them is the check's measured sensitivity under the pass policy
and must stay inside the bound.

## The pass policy

Every cell of every prognostic field in the final state dump must satisfy
|candidate - reference| <= 1e-10 + 1e-08 |reference|.
The relative part is the working bound because the fields span many orders of
magnitude; the absolute part covers cells at or near zero. The rule is again a relative bound on every cell of the final state and of the two PP81 profile fields, and it is physical because the closure is a deterministic pointwise function of shear and stratification: there is no averaging, no iteration and no tuning inside the step, so the value it returns is fully determined by the formula, and every fault above moves it by parts in a hundred or worse. It is achievable because this is the same disconnected single column as the other vermix checks, with no global reduction, a degenerate barotropic solve whose initial residual the upstream log records as exactly zero, and only direct tridiagonal factorisations downstream, so the round-off floor is set by fixed-length arithmetic rather than by any convergence tolerance. Five days is a short, dissipative window for a purely diffusive column, so the comparison stays pointwise rather than statistical. The one caution a reviewer needs is that PP81 contains two MAX limiters and one cap: PPviscMax at 1 m2/s, PPviscMin and PPdiffMin at zero, and the background viscArNr floor. These are clamps, and a clamped cell is insensitive to the parameter being perturbed, which is precisely why the variant perturbs PPalpha (the package default of 5.0, entering the denominator of the unclamped expression) rather than the deck's own PPviscMax; if the measured spread came out identically zero it would mean every cell was clamped, and that would be the signal to move the perturbation to PPnu0 or to the deck's diffKzT.
Faults: PP81 is a closed algebraic formula, so a fault in it is directly visible in the graded PPviscAr and PPdiffKr snapshots: a wrong exponent or a wrong alpha in the denominator of pp81_calc.F changes the viscosity by tens of percent wherever Ri is order one, and the temperature and velocity profiles follow within a few hours at parts in 1e-3. Computing the Richardson number from a one-sided instead of a centred difference in pp81_ri_number.F shifts the whole profile by a level and produces O(1) differences at the base of the mixed layer. Applying the PPviscMax cap before rather than after the background floor changes only the strongly sheared cells, but by a factor of order one there. Evaluating the closure in single precision leaves around 1e-7 relative in the viscosity, still an order of magnitude above the relative bound.

## Evidence

Overlay replaces data.pkg (useKPP off, usePP81 on); the unread input/data.kpp only triggers a weak warning. useMNC must be turned off. PPdumpFreq=432000. is what makes pkg/pp81/pp81_output.F write PPviscAr and PPdiffKr at the final iteration; without it the generator's dumpFreq=0 suppresses them. PPalpha is not written in data.pp81, so the generator must insert it into PP81_PARM01 with the base value 5.0 taken from pkg/pp81/pp81_readparms.F. All input real*8, readBinaryPrec=64; ivdc_kappa is off in this deck.

Floor: the optimised gfortran build and the IEEE -O0 build of the same source, run natively on the x86_64 host on 2026-09-02, differ on this deck by at most 0.0e+00 in absolute terms, 0.0e+00 of the bound (in no field); the two builds pass each other under the rule. Faults, same build with one parameter changed: the cg2d target residual loosened to 1e-3 uses 0.0e+00 of the bound (NO EFFECT (no cg2d in this configuration)), and the variant parameter off by five percent 7.3e+04 of the bound (FAIL). Measured run time of the nominal deck, build excluded: 0.6 s natively.
