# lab-sea-natl-box

Upstream test: `code/mitgcm/verification/lab_sea/input.natl_box`. Policy: `pointwise`.

## The test

North Atlantic box: the lab_sea ocean and KPP without sea ice. `run.sh` builds one MITgcm executable for this
configuration with the tree's own `tools/genmake2` (the build configuration
under `mods/`: `SIZE.h`, `packages.conf` and the option headers of the
upstream experiment, gfortran optfile `linux_amd64_gfortran`, one process,
tiles only), then runs it on the deck under `ic/<ic>/`. Configuration:
verification/lab_sea/input.natl_box: the second forward deck of the lab_sea experiment, a 20x16x23 North Atlantic box (2 degree spherical grid from 42W and 10N, 4 tiles of 10x8) started cold from the kf_* climatology, forced by the twelve-hourly kf_tx/kf_ty wind stress, kf_qnet heat flux, kf_empmr freshwater flux and kf_sw shortwave with periodicExternalForcing, relaxed to kf_sst and kf_sss on a 30-day time scale, with the POLY3 equation of state read from POLY3.COEFFS, the CD scheme, non-hydrostatic metric terms, Laplacian and biharmonic lateral viscosity (viscAh=5e4, viscA4=5e12), lateral tracer diffusion diffKhT=diffKhS=1e3, KPP vertical mixing and cg2d at cg2dTargetResidual=1e-13; the overlay's data.pkg switches GM/Redi, exf, cal and pkg/seaice all off, so no sea ice is computed at all; 48 steps of 3600 s (two days) instead of the deck's 20.

The production path it forces: pkg/kpp (kpp_calc.F and the Ri-number and bulk-Richardson column calculations), model/src/cg2d.F at a 1e-13 target residual, the flux-form momentum tendencies with the biharmonic viscosity, and the POLY3 equation of state.

Runtime knobs (`run.sh --help`): `SAB_STEPS` (default 48, the graded
value; the upstream deck runs 20 steps of 3600 s) scales the
run linearly, and `SAB_BUILD_JOBS` (default 4) only the build. Expected run
time on the declared resources, build excluded: about 3 s;
the per-check build (roughly 40 s on an x86_64 host) is reported by `run.sh`
as `SAB_BUILD_SECONDS` and does not count against the suite budget.

## The two initial conditions

`ic/nominal` is the upstream deck, assembled as `testreport` assembles it
(the experiment's `input/`, the input.natl_box/ overlay),
with these deck edits: `nTimeSteps` set to the graded window, `dumpFreq`,
`pChkptFreq` and `chkptFreq` set to zero and `dumpInitAndLast=.TRUE.` so that the only
state written is the initial and final dump, and
`writeBinaryPrec=64` so the dump is double precision and `useSingleCpuIO=.TRUE.` so the dump is one global file per field rather than one per tile.

`ic/variant` holds only the deck files that differ, laid over `ic/nominal` by
`run.sh`; the difference is `viscAh=50000.000000000015` in `data` instead of 50000:
two ulps of the graded precision (binary64) on a parameter that enters the
tendency from the first step, a distinct double, so the two runs differ at
round-off level from the first step. The
spread between them is the check's measured sensitivity under the pass policy
and must stay inside the bound.

## The pass policy

Every cell of every prognostic field in the final state dump must satisfy
|candidate - reference| <= 1e-10 + 1e-08 |reference|.
The relative part is the working bound because the fields span many orders of
magnitude; the absolute part covers cells at or near zero. The observable is every cell of the ocean U, V, W, T, S, Eta and pressure fields and the KPP diagnostics of the final state dump under |c - r| <= 1e-10 + 1e-8|r|. The relative part of the bound is what is actually tested because the graded fields span ten orders of magnitude (ice area of order one, ice and ocean velocities of order 1e-2 to 1 m/s, ice enthalpies of order 1e5 J/kg, heat fluxes of order 1e2 W/m2), so a single absolute number would be either unreachable for the enthalpies or vacuous for the velocities; the absolute part only covers cells at or near zero. The bound is physical because the configuration is a strongly damped, forced box at 2 degree resolution with no eddies and no convective adjustment (cAdjFreq=0, ivdc_kappa unset), so the two-day window is a smooth response to a prescribed forcing rather than an unstable integration, and the only reduction that could amplify round-off is cg2d, driven to a 1e-13 target residual, five orders below the bound. The reviewer must know what this check is and is not: it is the lab_sea experiment's ocean-only forward deck, and it exercises no line of pkg/seaice, pkg/thsice or pkg/salt_plume; it is included because it is a forward deck of an experiment of this module, and it earns its place as a control that isolates the ocean side that the four coupled lab_sea checks run on top of. It is achievable because the run is one process with the tiles swept in a fixed order (GLOBAL_SUM_ORDER_TILES), so every reduction and every solver sweep is deterministic and two correct builds of the same source differ only by round-off amplified through the iterations of the solve; the nominal-versus-variant spread that selfcheck records is the measurement of that amplification under this exact rule, and the bound is finalised against it with the human.
Faults: Since no sea-ice code runs, the faults this check can catch are the ocean-side ones the coupled sea-ice decks depend on: a wrong bulk Richardson interpolation for the KPP boundary-layer depth moves the mixed-layer depth by a level and the surface temperature by tenths of a kelvin within two days; a wrong POLY3 evaluation shifts the density and therefore the pressure gradient; loosening cg2d from 1e-13 changes Eta by of order the residual it stops at. The variant perturbs viscAh, which enters the Laplacian lateral viscosity in mom_u_del2u.F and its v counterpart from the first step.

## Evidence

DOUBT, flagged for the human: this deck computes no sea ice at all. The overlay's data.pkg lists only useKPP and useDiagnostics (useGMRedi=.FALSE., no useSEAICE, no useEXF, no useCAL), so pkg/seaice, pkg/thsice and pkg/salt_plume are all inactive and no sea-ice field is written or graded. It is included because the instruction is that every forward deck of every experiment of the module becomes a check; if the module owner prefers, it belongs with lab_sea/input.longstep in mitgcm-ocean-dynamics or mitgcm-mixing-parameterizations, by the same argument that moved global_ocean.cs32x15/input, input.in_p and input.viscA4 out of this module. The variant is therefore an ocean parameter (viscAh=5e4 in PARM01 of data), not a sea-ice one. The deck cold-starts (baseTime=startTime=21600, so nIter0=0), which makes the pickups inherited from input/ useless; they are dropped to keep ic/ small. readBinaryPrec=32. Moved from the sea-ice task: this lab_sea deck runs no sea ice (KPP, GM/Redi and diagnostics only) and belongs to the ocean dynamical core.

Floor: the optimised gfortran build and the IEEE -O0 build of the same source, run natively on the x86_64 host on 2026-09-02, differ on this deck by at most 9.1e-13 in absolute terms, 5.6e-04 of the bound (in U); the two builds pass each other under the rule. Faults, same build with one parameter changed: the cg2d target residual loosened to 1e-3 uses 1.4e+07 of the bound (FAIL), and the variant parameter off by five percent 1.8e+06 of the bound (FAIL). Measured run time of the nominal deck, build excluded: 0.4 s natively.
