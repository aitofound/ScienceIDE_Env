# global-ocean-4deg-idemix

Upstream test: `code/mitgcm/verification/global_ocean.90x40x15/input.idemix`. Policy: `pointwise`.

## The test

Four-degree global ocean started from rest with GGL90 and the IDEMIX internal-wave energy model. `run.sh` builds one MITgcm executable for this
configuration with the tree's own `tools/genmake2` (the build configuration
under `mods/`: `SIZE.h`, `packages.conf` and the option headers of the
upstream experiment, gfortran optfile `linux_amd64_gfortran`, one process,
tiles only), then runs it on the deck under `ic/<ic>/`. Configuration:
verification/global_ocean.90x40x15/input.idemix: the four-degree global ocean of the same experiment (90x40x15, 36 tiles of 10x10, JMD95P, GM/Redi with gkw91 tapering, C-D grid with tauCD=321428 s, monthly Trenberth and NCEP forcing on a 360-day cycle with periodicExternalForcing, real fresh-water flux, allowFreezing, partial cells with hFacMin=0.05 and hFacMindr=50, cg2d at cg2dTargetResidual=1.E-13, momentum step 1800 s and tracer and clock step one day) but cold-started from the Levitus fields at nIter0=0 rather than from a pickup, with free-slip sides and bottom, with implicitViscosity as well as implicitDiffusion, with a much gentler convective enhancement (ivdc_kappa=1 instead of 10, chosen by upstream precisely because GGL90 handles the instabilities), and with the background vertical viscosity and diffusivity switched off entirely (viscAr, diffKrT and diffKrS are all commented out) so that the whole vertical mixing of the run is produced by pkg/ggl90 running the Gaspar-Gregoris-Lefevre TKE closure (GGL90ck=0.1, GGL90ceps=0.7, GGL90alpha=30, mxlMaxFlag=2, GGL90viscMax=GGL90diffMax=1.E2) coupled to the IDEMIX internal-wave energy model (useIDEMIX=.TRUE., IDEMIX_tau_v=86400 s, IDEMIX_jstar=10, IDEMIX_mu0=4/3) forced by the tidal and wind energy fluxes tidal_energy.bin and wind_energy.bin that the overlay ships. viscAh=5.E5 is the only remaining explicit viscosity. The window is 3 clock steps, three days of the cold start..

The production path it forces: pkg/ggl90 (ggl90_calc.F for the TKE equation and its tridiagonal solve, ggl90_mixing.F, and ggl90_idemix.F for the IDEMIX energy equation and its own vertical solve) evaluated on every column every step, which in this deck replaces the constant-coefficient vertical mixing entirely; model/src/cg2d.F at 1.E-13 with 36-tile global sums; pkg/gmredi's tensor; pkg/mom_fluxform and pkg/cd_code; and the implicit vertical viscosity and diffusion solves of solve_tridiagonal.F, which this deck uses for momentum as well as tracers..

Runtime knobs (`run.sh --help`): `SAB_STEPS` (default 3, the graded
value; the upstream deck runs 10 steps of 86400 s) scales the
run linearly, and `SAB_BUILD_JOBS` (default 4) only the build. Expected run
time on the declared resources, build excluded: about 3 s;
the per-check build (roughly 40 s on an x86_64 host) is reported by `run.sh`
as `SAB_BUILD_SECONDS` and does not count against the suite budget.

## The two initial conditions

`ic/nominal` is the upstream deck, assembled as `testreport` assembles it
(the experiment's `input/`, the input.idemix/ overlay, and the files its prepare_run links from sibling experiments),
with these deck edits: `nTimeSteps` set to the graded window, `dumpFreq`,
`pChkptFreq` and `chkptFreq` set to zero and `dumpInitAndLast=.TRUE.` so that the only
state written is the initial and final dump, and
`writeBinaryPrec=64` so the dump is double precision and `useSingleCpuIO=.TRUE.` so the dump is one global file per field rather than one per tile; `useDiagnostics=.FALSE.` in `data.pkg`.

`ic/variant` holds only the deck files that differ, laid over `ic/nominal` by
`run.sh`; the difference is `IDEMIX_jstar=10.000000000000004` in `data.ggl90` instead of 10:
two ulps of the graded precision (binary64) on a parameter that enters the
tendency from the first step, a distinct double, so the two runs differ at
round-off level from the first step. The
spread between them is the check's measured sensitivity under the pass policy
and must stay inside the bound.

## The pass policy

Every cell of every prognostic field in the final state dump must satisfy
|candidate - reference| <= 1e-10 + 1e-08 |reference|.
The relative part is the working bound because the fields span many orders of
magnitude; the absolute part covers cells at or near zero. The observable is every cell of U, V, W, T, S, Eta, PH, the C-D grid velocities and the GGL90 turbulent kinetic energy and IDEMIX energy fields that the packages carry in the dump, under |c - r| <= 1e-10 + 1e-8|r|; temperature spans about -2 to 30 C and salinity is near 35, so the relative part of the bound is the working test. The bound is physical because three days of a four-degree ocean released from the Levitus state is a smooth, strongly damped adjustment: the flow is spinning up from rest, the largest signal is the geostrophic adjustment of the initial density field, and nothing in it is chaotic at this resolution and this window. It is unusually clean in one respect and unusually delicate in another. Clean: with viscAr, diffKrT and diffKrS all switched off, the entire vertical mixing is the GGL90/IDEMIX closure, so this check grades that closure without a constant background masking it, and the closure is built out of smooth algebraic expressions and tridiagonal solves with no iteration and no sorting. Delicate: a cold start means every column is being mixed for the first time, so the convective enhancement ivdc_kappa=1 is exercised much harder than in the pickup-restart siblings, and with the non-linear JMD95P equation of state that switch is discontinuous. Three steps is therefore the window, chosen on the same evidence as the two siblings (the sea-ice task measured a switch flip at about the fourth daily step on the equivalent cubed-sphere global deck) and, if anything, more conservative here because the cold start makes marginal columns more common; it must not be lengthened. The variant is IDEMIX_jstar, in force through useIDEMIX=.TRUE. in GGL90_PARM02, which multiplies the internal-wave wavenumber and hence the diffusivity in every column from the first step, so the two-ulp probe measures the round-off sensitivity of exactly the physics this overlay adds; viscAh=5.E5 is the fallback if the IDEMIX path turns out to be too weakly coupled in three days.
Faults: IDEMIX_jstar sets the spectral bandwidth through pijstar = PI*IDEMIX_jstar in ggl90_idemix.F and therefore the vertical wavenumber that converts internal-wave energy into a diffusivity; getting that conversion wrong, or dropping the IDEMIX energy equation and falling back to a constant background, changes the vertical diffusivity by factors of order one over most of the ocean interior and moves T and S by per-cent amounts within three days, because in this deck there is no background diffusivity to hide behind. Getting the GGL90 mixing-length limiter (mxlMaxFlag=2) or the ceps/ck constants wrong changes the surface boundary layer depth immediately and shows up in the top few levels of THETA. Dropping the implicit vertical viscosity and going explicit changes U and V by tens of per cent. A cheaper cg2d at 1.E-7 gives about 1e-7 relative on Eta. Single precision gives about 1e-7 relative on T and S.

## Evidence

Cold start: this overlay sets nIter0=0, so the pickup files of input/ are not read; they are dropped so that ic/nominal does not carry 5.5 MB of dead weight, and the graded final iteration is 3, not 36003. prepare_run links the same nine .bin files from tutorial_global_oce_latlon/input; readBinaryPrec=32 must be preserved for them and for the overlay's own tidal_energy.bin and wind_energy.bin. The overlay's data has no EmPmRFile even though useRealFreshWaterFlux=.TRUE.; that is upstream's choice and must be left alone. data.ptracers is shipped by the overlay but data.pkg does not set usePTRACERS, so it is never read and no passive tracer is written. useDiagnostics must be forced off: the two streams write at 3153600 s, which at a one-day clock step is iteration 36.5 and can never fire on an integer step, but the package is switched off for the same reasons as in the siblings. useMNC is commented out. GGL90writeState=.FALSE. and GGL90mixingMaps=.FALSE. in the deck, so pkg/ggl90 writes no extra snapshot files; the prognostic GGL90 TKE and IDEMIX energy fields still reach the final dump through the package's own state writer and are graded, which is intended. data.exch2.mpi is dropped as in the siblings. Threshold hazard: ivdc_kappa=1 with JMD95P, harder-exercised here than in the pickup-restart siblings because the run is cold-started; this is the check of the three whose measured spread should be looked at first.

Floor: the optimised gfortran build and the IEEE -O0 build of the same source, run natively on the x86_64 host on 2026-09-02, differ on this deck by at most 1.6e-11 in absolute terms, 2.5e-03 of the bound (in PH); the two builds pass each other under the rule. Faults, same build with one parameter changed: the cg2d target residual loosened to 1e-3 uses 8.4e+07 of the bound (FAIL), and the variant parameter off by five percent 8.4e+06 of the bound (FAIL). Measured run time of the nominal deck, build excluded: 0.3 s natively.
