# Phantom module coverage ledger — HD, shocks, waves and turbulence

**Primary module owner:** `phantom-hd-turbulence`
**Shared source authority:** `code/phantom` at
`e53ea16758d2a261680506852a528f21270dca1c`
**Active target:** `cpu-docker`
**Reward:** `passed / 9`; every row is binary and contributes exactly `1/9`.
There is one active target, hence the target/check matrix has 9 active cells.

## Active official checks

| # | Check | Official mode | Upstream provenance | HD pathway / observable |
|---:|---|---|---|---|
| 1 | `sedov-blast-3d` | `SETUP=sedov` | `build/Makefile_setups:669-675`; `src/setup/setup_sedov.f90` | Strong spherical shock; thermal blast, pressure force, artificial viscosity, neighbour/density loop, individual steps; two full states + `.ev` history. |
| 2 | `kelvin-helmholtz-instability` | `SETUP=kh` | `build/Makefile_setups:696-701`; `src/setup/setup_kh.f90` | Multidimensional shear instability, density/pressure gradients and mixing. |
| 3 | `taylor-green-vortex` | `SETUP=taylorgreen` | `build/Makefile_setups:143-151`; `src/setup/setup_taylorgreen.f90` | Periodic isothermal quintic-kernel vortex transport and numerical dissipation. |
| 4 | `linear-sound-wave` | `SETUP=wave` | `build/Makefile_setups:649-655`; `src/setup/setup_wave.f90` | Weak hydrodynamic sound-wave propagation and phase/dispersion path; gas only. |
| 5 | `upstream-kernel-consistency` | `phantomtest kernel` | `src/tests/testsuite.f90:177-178,252-254`; `src/tests/test_kernel.f90` | SPH kernel normalization, derivatives and tabulation. |
| 6 | `upstream-hydro-derivatives` | `phantomtest derivshydro` | `src/tests/test_derivs.f90:102-103,184`; `src/tests/testsuite.f90:145,273` | Density and pressure derivative implementation. |
| 7 | `upstream-artificial-viscosity` | `phantomtest derivsav` | `src/tests/test_derivs.f90:104-105,854-859` | Shock artificial-viscosity force/energy terms with source-owned tolerances. |
| 8 | `upstream-cullen-dehnen-switch` | `phantomtest derivscd` | `src/tests/test_derivs.f90:116-117,910` | `d(div v)/dt` path in the Cullen–Dehnen shock detector/switch. |
| 9 | `upstream-eos-consistency` | `phantomtest eos` | `src/tests/testsuite.f90:205-206,280`; `src/tests/test_eos.f90` | Classical gas pressure/internal-energy conversions used by HD. |

## Source pathways owned or reproduced

Primary owned implementation includes `src/main/phantom.f90`, the classical
HD portions of `src/main/derivs_driver.f90`, `src/main/step_lf_global.f90` and
`src/main/step_lf_ind.f90`, `src/main/timestep.f90`, `src/main/densityforce.f90`,
`src/main/force.F90`, `src/main/deriv.f90`, `src/main/viscosity.f90`, kernel,
neighbour/tree and equation-of-state modules reached by the active rows, and
the four setup and five upstream test modules cited above. Generic particle,
MPI, boundary, input and dump modules are reproduced dependencies rather than
separate scientific ownership claims.

## Cross-family primary owners and exclusions

| Family | Primary leaf | Treatment here |
|---|---|---|
| Classical gas HD, shock dissipation, physical viscosity, waves and turbulence | **`phantom-hd-turbulence`** | Owned and directly checked by the 9 rows above. |
| Ideal/non-ideal MHD | `phantom-mhd-nonideal` | Excluded. No `MHD=yes`, `NONIDEALMHD=yes`, `mhdshock`, `mhdblast`, `mhdwave`, `wavedamp` or magnetic shock choice. |
| Dust and grain growth | `phantom-dust-growth` | Excluded. No `DUST=yes`, dusty setup, drag, growth or porosity path. |
| Self-gravity, sinks and N-body | `phantom-gravity-sinks-nbody` | Excluded. No `GRAVITY=yes`, sink creation, point-mass or collapse setup. The sphere-in-box row compiles no gravity and sets `icreate_sinks=0`. |
| Radiation and thermochemistry | `phantom-radiation-thermochemistry` | Excluded. No radiation shock/pulse, cooling or chemistry selector. |
| Relativity/spacetime | `phantom-relativity-spacetime` | Excluded. No `GR=yes`, relativistic shock/blast or spacetime path. |
| Winds, accretion, injection and feedback | `phantom-winds-accretion-feedback` | Excluded. No wind, injection, accretion or feedback setup. |

Specific reviewed rejections: `jadvect` enables MHD in
`Makefile_setups:711-717`; `wavedamp` enables MHD and non-ideal MHD in
`Makefile_setups:657-667`; `sphere` is a self-gravitating/cooling collapse
family; `dustysedov`, `dustywave`, `dustyshock`, `mhdshock`, `mhdblast`,
`mhdwave`, `radshock` and `srblast` are assigned to the owners above. There is
no upstream `nwave` setup in the pinned source.

## Policy and evidence status

Production rows preserve two complete evolved particle states in stable
`iorig` order and every numeric diagnostic column; focused tests preserve the
official source-owned selector outcomes. The active exact comparison is a CPU
packaging rule only. Docker execution, repeated-reference determinism and
accelerator calibration remain required before making any device equivalence
or performance claim. See `comment/README.md`.
