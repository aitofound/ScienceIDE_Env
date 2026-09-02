# vermix-ggl90

Upstream test: `code/mitgcm/verification/vermix/input.ggl90`. Policy: `pointwise`.

## The test

GGL90 turbulent-kinetic-energy closure in the same column. `run.sh` builds one MITgcm executable for this
configuration with the tree's own `tools/genmake2` (the build configuration
under `mods/`: `SIZE.h`, `packages.conf` and the option headers of the
upstream experiment, gfortran optfile `linux_amd64_gfortran`, one process,
tiles only), then runs it on the deck under `ic/<ic>/`. Configuration:
verification/vermix with the input.ggl90 overlay: the same 1x1x26 forced column, the same forcing files and the same frozen advection, but data.pkg now selects useGGL90 alone (KPP off), so the vertical viscosity and diffusivity come from the Gaspar-Gregoris-Lefevre prognostic turbulent-kinetic-energy equation instead of a diagnostic profile; the deck runs it with GGL90TKEmin=1e-7, GGL90mixingLengthMin=3 m and mxlMaxFlag=3 (the mixing length limited by the distance to the surface and the bottom), GGL90writeState on, and the Langmuir extension compiled in but not activated; the window is again 360 steps of 1200 s, five days, so that the overlay's own diagnostics streams dynDiag, DiagMXL_3d (GGL90TKE, GGL90Lmx, GGL90Kr, GGL90ArU, GGL90ArV, GGL90Prl) and DiagMXL_2d close at the final iteration..

The production path it forces: pkg/ggl90/ggl90_calc.F, which builds and solves the tridiagonal implicit system for the turbulent kinetic energy every step, together with the mixing-length construction in pkg/ggl90/ggl90_mixinglength.F and the conversions to viscosity and diffusivity in pkg/ggl90/ggl90_calc_visc.F and ggl90_calc_diff.F; the resulting profiles are then applied by model/src/impldiff.F and model/src/solve_tridiagonal.F..

Runtime knobs (`run.sh --help`): `SAB_STEPS` (default 360, the graded
value; the upstream deck runs 20 steps of 1200 s) scales the
run linearly, and `SAB_BUILD_JOBS` (default 4) only the build. Expected run
time on the declared resources, build excluded: about 3 s;
the per-check build (roughly 40 s on an x86_64 host) is reported by `run.sh`
as `SAB_BUILD_SECONDS` and does not count against the suite budget.

## The two initial conditions

`ic/nominal` is the upstream deck, assembled as `testreport` assembles it
(the experiment's `input/`, the input.ggl90/ overlay),
with these deck edits: `nTimeSteps` set to the graded window, `dumpFreq`,
`pChkptFreq` and `chkptFreq` set to zero and `dumpInitAndLast=.TRUE.` so that the only
state written is the initial and final dump, and
`writeBinaryPrec=64` so the dump is double precision and `useSingleCpuIO=.TRUE.` so the dump is one global file per field rather than one per tile; `useMNC=.FALSE.` in `data.pkg`; `GGL90dumpFreq=432000.` in `data.ggl90`.

`ic/variant` holds only the deck files that differ, laid over `ic/nominal` by
`run.sh`; the difference is `GGL90ck=0.10000000000000003` in `data.ggl90` instead of 0.1:
two ulps of the graded precision (binary64) on a parameter that enters the
tendency from the first step, a distinct double, so the two runs differ at
round-off level from the first step. The
spread between them is the check's measured sensitivity under the pass policy
and must stay inside the bound.

## The pass policy

Every cell of every prognostic field in the final state dump must satisfy
|candidate - reference| <= 1e-10 + 1e-08 |reference|.
The relative part is the working bound because the fields span many orders of
magnitude; the absolute part covers cells at or near zero. The bound is a relative one applied to every cell of the final state and of the closure's own fields, and it is physical here because the TKE equation has a genuine equilibrium: production, dissipation and vertical transport balance, so the diffusivity that comes out is fixed by the coefficients, and any of the faults above moves it by parts in a hundred, six orders of magnitude above the bound. It is achievable for the same reason as the KPP check: the domain is one disconnected column, there is no global sum and no elliptic iteration (the upstream log shows the barotropic residual identically zero), and both the TKE solve and the momentum and tracer solves are direct tridiagonal factorisations with no convergence test, so two runs of the same build agree bit-for-bit and the only spread is the one the deliberate one-ulp change injects. The window matters more here than for KPP because TKE is prognostic and therefore remembers perturbations rather than re-diagnosing them, but five days of a dissipative, forced column is still firmly in the damped regime: the dissipation term acts on the perturbation as strongly as on the mean, so the spread should saturate rather than grow. A reviewer should know that the variant parameter GGL90ck is the package default of 0.1 rather than a value the deck writes; it is the coefficient in KappaM = GGL90ck * mixingLength * sqrt(TKE) in ggl90_calc.F, so it is in play in every wet cell at every step, which the deck's own settings (GGL90mixingLengthMin and GGL90TKEmin, both MAX limiters that may never bind) would not be.
Faults: GGL90 carries a prognostic variable, so an error compounds instead of being re-diagnosed each step: a wrong dissipation constant (GGL90ceps in the TKEdissipation term of pkg/ggl90/ggl90_calc.F) at the one-percent level changes the steady-state TKE by roughly two percent and the diffusivity by a comparable amount within a day, parts in 1e-2 of the graded GGL90TKE and GGL90Kr fields. Dropping the buoyancy production term or mis-signing the shear production in ggl90_calc.F collapses or inflates the mixed layer entirely, an O(1) change. A wrong mixing-length limiter in ggl90_mixinglength.F (for instance omitting the surface-distance bound selected by mxlMaxFlag=3) changes GGL90Lmx by metres near the surface, again parts in 1e-1. Solving the TKE equation explicitly instead of implicitly, or in single precision, leaves relative differences around 1e-7 in the temperature profile after 360 steps.

## Evidence

The overlay replaces data.pkg, so useKPP is off and the input/ copy of data.kpp is present but unread; MITgcm only prints a weak warning for that (model/src/packages_unused_msg.F), which is how testreport runs this deck upstream too. useMNC must be turned off. GGL90dumpFreq=432000. is needed for the same reason as kpp_dumpFreq in the KPP check, and adds the instantaneous GGL90viscArU, GGL90viscArV, GGL90diffKr and GGL90TKE snapshots at the final iteration. TKE.init is deliberately NOT dropped even though GGL90TKEFile is commented out in the deck, because a reviewer restoring that line would need it; it is inert as configured. GGL90_OPTIONS.h defines GGL90_MISSING_HFAC_BUG, so the run reproduces the upstream (bug-compatible) hFac treatment; do not clean that up. All input is real*8, readBinaryPrec=64.

Floor: the optimised gfortran build and the IEEE -O0 build of the same source, run natively on the x86_64 host on 2026-09-02, differ on this deck by at most 0.0e+00 in absolute terms, 0.0e+00 of the bound (in no field); the two builds pass each other under the rule. Faults, same build with one parameter changed: the cg2d target residual loosened to 1e-3 uses 0.0e+00 of the bound (NO EFFECT (no cg2d in this configuration)), and the variant parameter off by five percent 4.8e+06 of the bound (FAIL). Measured run time of the nominal deck, build excluded: 0.5 s natively.
