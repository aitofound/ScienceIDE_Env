# offline-ellipse-nonnormal

Upstream test: `code/mitgcm/verification/offline_exf_seaice/input.dyn_ellnnfr`. Policy: `pointwise`.

## The test

Ice-only channel, elliptical yield curve with a non-normal flow rule. `run.sh` builds one MITgcm executable for this
configuration with the tree's own `tools/genmake2` (the build configuration
under `mods/`: `SIZE.h`, `packages.conf` and the option headers of the
upstream experiment, gfortran optfile `linux_amd64_gfortran`, one process,
tiles only), then runs it on the deck under `ic/<ic>/`. Configuration:
verification/offline_exf_seaice/input.dyn_ellnnfr: the same 80x42 channel run as pure dynamics (no thsice, usePW79thermodynamics=.FALSE.), LSR solver at LSR_ERROR=1e-12 with up to 1500 linear iterations, advection scheme 41, free-slip sides, and the elliptical yield curve taken with a non-normal flow rule by setting the flow-rule eccentricity SEAICE_eccfr=1 different from the yield-curve eccentricity (which stays at its default 2), so that the plastic strain-rate direction is no longer normal to the yield surface; ocean frozen; 48 steps of 1800 s (one day) instead of the deck's 12.

The production path it forces: seaice_calc_viscosities.F with two eccentricities in play (the SEAICE_eccfr path, which changes the ratio of the shear to the bulk viscosity and the pressure correction), seaice_lsr.F, seaice_calc_stressdiv.F and seaice_advdiff.F.

Runtime knobs (`run.sh --help`): `SAB_STEPS` (default 48, the graded
value; the upstream deck runs 12 steps of 1800 s) scales the
run linearly, and `SAB_BUILD_JOBS` (default 4) only the build. Expected run
time on the declared resources, build excluded: about 12 s;
the per-check build (roughly 40 s on an x86_64 host) is reported by `run.sh`
as `SAB_BUILD_SECONDS` and does not count against the suite budget.

## The two initial conditions

`ic/nominal` is the upstream deck, assembled as `testreport` assembles it
(the experiment's `input/`, the input.dyn_ellnnfr/ overlay),
with these deck edits: `nTimeSteps` set to the graded window, `dumpFreq`,
`pChkptFreq` and `chkptFreq` set to zero and `dumpInitAndLast=.TRUE.` so that the only
state written is the initial and final dump, and
`writeBinaryPrec=64` so the dump is double precision and `useSingleCpuIO=.TRUE.` so the dump is one global file per field rather than one per tile; `SEAICEwriteState=.TRUE.` in `data.seaice`.

`ic/variant` holds only the deck files that differ, laid over `ic/nominal` by
`run.sh`; the difference is `SEAICE_strength=26780.000000000007` in `data.seaice` instead of 26780:
two ulps of the graded precision (binary64) on a parameter that enters the
tendency from the first step, a distinct double, so the two runs differ at
round-off level from the first step. The
spread between them is the check's measured sensitivity under the pass policy
and must stay inside the bound.

## The pass policy

Every cell of every prognostic field in the final state dump must satisfy
|candidate - reference| <= 1e-10 + 1e-08 |reference|; the fields `UWIND`, `VWIND` are not graded.
The relative part is the working bound because the fields span many orders of
magnitude; the absolute part covers cells at or near zero. The observable is every cell of UICE, VICE, AREA, HEFF, HSNOW and the frozen ocean fields of the final dump under |c - r| <= 1e-10 + 1e-8|r|. The relative part of the bound is what is actually tested because the graded fields span ten orders of magnitude (ice area of order one, ice and ocean velocities of order 1e-2 to 1 m/s, ice enthalpies of order 1e5 J/kg, heat fluxes of order 1e2 W/m2), so a single absolute number would be either unreachable for the enthalpies or vacuous for the velocities; the absolute part only covers cells at or near zero. The bound is physical for the same reason as on the other LSR decks of this experiment: the viscosities are smooth regularised functions of the strain-rate invariants, the momentum balance is solved to a 1e-12 relative residual, and with no thermodynamics there is no discrete freeze or melt event; the round-off floor is set by the LSR sweeps and by the exponential in the ice-strength law, which on the primary deck of this experiment measured 7e-13 relative between two legitimate builds. One simulated day is not long enough for the channel to be chaotic. It is achievable because the run is one process with the tiles swept in a fixed order (GLOBAL_SUM_ORDER_TILES), so every reduction and every solver sweep is deterministic and two correct builds of the same source differ only by round-off amplified through the iterations of the solve; the nominal-versus-variant spread that selfcheck records is the measurement of that amplification under this exact rule, and the bound is finalised against it with the human.
Faults: A port that hard-codes a single eccentricity, or that reuses the yield-curve eccentricity in the flow rule, reproduces the ellipse but not this deck: the shear viscosity is then wrong by the ratio of the two eccentricities and UICE moves by order one wherever the ice is plastic. Dropping the pressure correction that accompanies the non-normal rule shifts the divergent part of the flow by per cent. The generic dynamics faults apply as on the sibling decks: an early LSR exit at 1e-2 to 1e-1 relative, a five per cent air-ice drag error at order 1e-1.

## Evidence

SEAICE_eccfr is only read when the non-normal flow rule is compiled in; it is the only line that distinguishes this deck from the plain LSR one, so the check is a targeted test of that branch of seaice_calc_viscosities.F. The overlay ships its own data.pkg without useThSIce. 64-bit inputs, no pickup, no prepare_run links.

Floor: the optimised gfortran build and the IEEE -O0 build of the same source, run natively on the x86_64 host on 2026-09-02, differ on this deck by at most 1.7e-13 in absolute terms, 3.2e-04 of the bound (in FV); the two builds pass each other under the rule. Faults, same build with one parameter changed: the cg2d target residual loosened to 1e-3 uses 0.0e+00 of the bound (NO EFFECT (no cg2d in this configuration)), and the variant parameter off by five percent 1.2e+07 of the bound (FAIL). Measured run time of the nominal deck, build excluded: 11.1 s natively.
