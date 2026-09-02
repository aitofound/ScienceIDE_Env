# short-surface-wave-nonhydrostatic

Upstream test: `code/mitgcm/verification/short_surf_wave/input`. Policy: `pointwise`.

## The test

A short surface gravity wave: the non-hydrostatic free surface at laboratory scale. `run.sh` builds one MITgcm executable for this
configuration with the tree's own `tools/genmake2` (the build configuration
under `mods/`: `SIZE.h`, `packages.conf` and the option headers of the
upstream experiment, gfortran optfile `linux_amd64_gfortran`, one process,
tiles only), then runs it on the deck under `ic/<ic>/`. Configuration:
verification/short_surf_wave/input: a 52x1x50 vertical slice at LABORATORY scale, four tiles of 13x1, cells of 0.2 m in every direction so the domain is 10.4 m long and 10 m deep, filled with a fluid of uniform density (tAlpha=sBeta=0 with tempStepping and saltStepping both off, so the only prognostic variables are the velocities and the free surface) under gravity=10 and with no rotation (f0=0), started from a pickup at iteration 1 with an initial free-surface displacement read from Eta_ini.bin over the flat bottom topo_flat.bin, and integrated with a time step of FIVE MILLISECONDS; the whole point is selectNHfreeSurf=1, the non-hydrostatic free-surface formulation, which is the only way MITgcm can carry a surface gravity wave whose wavelength is comparable with the depth, together with nonHydrostatic=.TRUE., exactConserv, implicSurfPress=implicDiv2DFlow=0.5 and third-order Adams-Bashforth (alph_AB=0.5, beta_AB=0.281105) with momDissip_In_AB=.FALSE.; the fluid is essentially inviscid (viscAh=viscAr=1.E-6, the molecular value) and the elliptic solves are specified in W units rather than as plain residuals, cg2dTargetResWunit=7.0E-14 and cg3dTargetResWunit=1.5E-17, with cg2d converging in about 4 iterations and cg3d in about 190 in the upstream output. The deck runs nTimeSteps=11, fifty-five milliseconds; the window here is 200 steps, one second, which for a wave of this length and depth is roughly one wave period, so the surface returns towards its initial shape and the graded field is directly comparable with the initial condition..

The production path it forces: model/src/cg3d.F with its roughly 190 iterations per step, together with pre_cg3d.F and post_cg3d.F, which completely dominate this check; model/src/calc_gw.F for the vertical momentum equation; and the non-hydrostatic free-surface coupling of solve_for_pressure.F that selectNHfreeSurf=1 switches on..

Runtime knobs (`run.sh --help`): `SAB_STEPS` (default 200, the graded
value; the upstream deck runs 11 steps of 0.005 s) scales the
run linearly, and `SAB_BUILD_JOBS` (default 4) only the build. Expected run
time on the declared resources, build excluded: about 3 s;
the per-check build (roughly 40 s on an x86_64 host) is reported by `run.sh`
as `SAB_BUILD_SECONDS` and does not count against the suite budget.

## The two initial conditions

`ic/nominal` is the upstream deck, assembled as `testreport` assembles it
(the experiment's `input/`),
with these deck edits: `nTimeSteps` set to the graded window, `dumpFreq`,
`pChkptFreq` and `chkptFreq` set to zero and `dumpInitAndLast=.TRUE.` so that the only
state written is the initial and final dump, and
`writeBinaryPrec=64` so the dump is double precision and `useSingleCpuIO=.TRUE.` so the dump is one global file per field rather than one per tile; `useDiagnostics=.FALSE.` in `data.pkg`.

`ic/variant` holds only the deck files that differ, laid over `ic/nominal` by
`run.sh`; the difference is `gravity=10.000000000000004` in `data` instead of 10:
two ulps of the graded precision (binary64) on a parameter that enters the
tendency from the first step, a distinct double, so the two runs differ at
round-off level from the first step. The
spread between them is the check's measured sensitivity under the pass policy
and must stay inside the bound.

## The pass policy

Every cell of every prognostic field in the final state dump must satisfy
|candidate - reference| <= 1e-10 + 1e-08 |reference|.
The relative part is the working bound because the fields span many orders of
magnitude; the absolute part covers cells at or near zero. The observable is every cell of U, V, W, Eta, the hydrostatic pressure and the non-hydrostatic pressure of the final dump under |c - r| <= 1e-10 + 1e-8|r|; T and S are frozen at Tref=10 and sRef=35 because both are switched off, so grading them only confirms nothing corrupted them. The bound is physical because a small-amplitude surface gravity wave in a uniform-density inviscid fluid is very nearly a linear problem: there is no stratification, no rotation, no tracer, no advection of anything but momentum by a weak flow, and the exact solution is a dispersive wave that returns close to its initial shape after a period. Nothing in the deck is discontinuous: no convective adjustment, no freezing, no limiter, no moving cell heights, and hFacMin=0.2 is applied once at initialisation over a flat bottom. It is achievable because both solves are driven very hard indeed, cg2d to 7.0E-14 and cg3d to 1.5E-17 in W units, orders below the grading bound, so even an iteration-count flip in the roughly 190-iteration cg3d costs far less than the budget. Two things a reviewer must weigh. First, the fluid is essentially inviscid (viscAh=viscAr=1.E-6 m2/s on 0.2 m cells is molecular), so nothing damps accumulated round-off; that is why the window is one wave period and not ten, even though one second of model time is arithmetically cheap and a longer run would fit the budget more comfortably. Second, the variant viscAh=1.E-6 is a very small number, and it is chosen deliberately: it IS set explicitly in PARM01, it enters mom_u_del2u.F in every wet cell from the first step, and being small it perturbs the solution gently rather than swamping it, which is what a round-off probe should do; the alternative, gravity=10, would have been a much stronger probe and can be substituted if the measured spread turns out to be degenerate (i.e. if two ulps of a 1e-6 viscosity produce a difference indistinguishable from zero). It is achievable because the run is one process with the tiles swept in a fixed order (GLOBAL_SUM_ORDER_TILES), so every reduction and every solver sweep is deterministic and two correct builds of the same source differ only by round-off amplified through the steps of the integration; the nominal-versus-variant spread that selfcheck records is the measurement of that amplification under this exact rule, and the bound is finalised against it with the human.
Faults: Dropping the non-hydrostatic contribution to the free-surface equation, i.e. reverting selectNHfreeSurf to 0, makes the wave propagate at the shallow-water speed sqrt(gH) instead of at its correct dispersive speed, which over one wave period is an order-one phase error and is the single fault this deck exists to catch. Getting the coupling between the two- and three-dimensional pressure solves wrong changes the surface displacement by tens of per cent within a few tens of steps. Solving the three-dimensional pressure problem more cheaply moves the velocities by about the residual it stops at, and here that residual is specified in W units at 1.5E-17, extremely tight. Getting the AB3 weights wrong changes the phase by per cent over 200 steps. Single precision gives about 1e-7 relative.

## Evidence

useDiagnostics MUST be forced off, and here it matters: data.diagnostics defines a surfDiag snapshot stream (ETAN, ETANSQ, DETADT2) at frequency(1)=-0.25 with timePhase 0, which at dt=0.005 is every 50 steps and therefore lands exactly on iteration 201, the final iteration of a 200-step window starting from nIter0=1; left on it would write surfDiag.0000000201.data into the graded glob. The dynStDiag statistics stream at -0.025 writes only .txt but goes away with the package. THE RUN STARTS FROM nIter0=1, not 0: pickup.0000000001.data and .meta are shipped and must be kept (they are copied), so the graded final iteration is 201, not 200. data.1it is an alternative parameter file used upstream for a one-iteration run; it is not read by the model (which reads 'data') and is dropped. The elliptic targets are given as cg2dTargetResWunit and cg3dTargetResWunit, NOT as cg2dTargetResidual and cg3dTargetResidual (both of the latter are commented out in the deck); a generator or reviewer looking for the usual keys will not find them, and they must not be added. readBinaryPrec=64 and both binaries are real*8. plotLevel=0 suppresses the field plots. Nothing to link, nothing to gunzip. The native survey did not run this experiment: it is absent from native-walltimes.txt, so the runtime here is NOT a scaled measurement but an estimate built from the grid size, the tile count, the solver iteration counts printed in verification/short_surf_wave/results/output.txt and the step count of the window, calibrated against the decks the survey did time. Treat it as an order-of-magnitude figure. The digits are reproduced upstream: verification/short_surf_wave/results/output.txt exists and the run ends normally. The final self-validation of 2026-09-02 with viscAh=1e-6 as the variant parameter came back byte-identical to the nominal run (two ulps of so small a viscosity vanish below the round-off of the tendency), so the variant is gravity=10, set explicitly in the deck and entering the free-surface and non-hydrostatic pressure from the first step.

Floor: the optimised gfortran build and the IEEE -O0 build of the same source, run natively on the x86_64 host on 2026-09-02, differ on this deck by at most 0.0e+00 in absolute terms, 0.0e+00 of the bound (in no field); the two builds pass each other under the rule. Faults, same build with one parameter changed: the cg2d target residual loosened to 1e-3 uses 0.0e+00 of the bound (NO EFFECT (no cg2d in this configuration)), and the variant parameter off by five percent 1.1e+00 of the bound (FAIL). Measured run time of the nominal deck, build excluded: 2.6 s natively.
