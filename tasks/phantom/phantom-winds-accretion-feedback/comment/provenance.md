# Official provenance and acceptance anchors

## Source identity

- Repository: `https://github.com/danieljprice/phantom`
- Pinned commit: `e53ea16758d2a261680506852a528f21270dca1c`
- Shared tree: repository-level `code/phantom`
- License: GPL-3.0-or-later, including the `LICENSE` Section 7(c) naming
  condition recorded in `task.toml`

No task-local Phantom source, patch, replacement setup, or copied PR #343 code
is present.

## Setup authority

`build/Makefile_setups` proves every case-sensitive setup name and source
selection used by the 14 setup-smoke checks:

| Setup | Official setup/injection selection | Cross-family note |
|---|---|---|
| `wind` | `setup_wind.f90`; `INJECT_PARTICLES=yes`; default `inject_wind.f90` | sink radiation/dust nucleation are dependencies |
| `isowind` | `setup_wind.f90`; isothermal; `INJECT_PARTICLES=yes` | wind owner |
| `BHL` | `setup_BHL.f90`; `SRCINJECT=inject_BHL.f90` | exact uppercase spelling |
| `bondi` | `bondiexact.f90 setup_bondi.f90`; isothermal | analytic Bondi setup, not `inject_bondi.f90` |
| `windtunnel` | `setup_windtunnel.f90`; `SRCINJECT=inject_windtunnel.f90` | gravity/common-envelope analysis are dependencies |
| `masstransfer` | `setup_masstransfer.f90`; `readwrite_mesa.f90 inject_masstransfer.f90` | no external MESA file is invented; official defaults apply |
| `asteroidwind` | `setup_asteroidwind.f90`; `evolve_planet.f90 inject_randomwind.f90` | isothermal/planet dependencies |
| `randomwind` | `setup_disc.f90`; same random-wind injector | disc solver is a dependency |
| `boilingplanets` | `setup_disc.f90`; same random-wind injector | disc solver is a dependency |
| `qpe` | `setup_empty.f90`; `SRCINJECT=inject_disk.f90` | public setup says radiation off |
| `galcen` | `setup_galcen_stars.f90`; `inject_galcen_winds.f90` | default data is `data/galcen/stars.m.pos.-vel.txt` |
| `streamerdisc` | `setup_disc.f90`; `inject_streamer_pineda.f90` | disc solver is a dependency |
| `balsarakim` | `setup_unifdis.f90`; `inject_sne.f90` | MHD/H2 are dependencies; seed `SNe` is not an official setup name |
| `jet` | `setup_sphereinbox.f90` | MHD/gravity evolution is owned elsewhere; this row owns the official jet initial condition only |

`docs/user-guide/setups-list.rst` independently lists these official setups.
`docs/examples/wind.rst` independently documents `wind`, `isowind`,
`phantomsetup wind`, and the generated `.setup`/`.in` interface.

## Runtime-smoke authority

`scripts/buildbot.sh`, function `check_phantomsetup`, is copied behaviorally,
not textually. It runs the official setup with 40 default-answer newlines and
`--np=1000`, invokes setup up to three times, changes the generated input to
`nmax=0`, executes Phantom, and requires a nonempty `<prefix>_00000` dump.

The production reachability fact comes from `src/main/initial.F90`: when
`inject_parts` is true, initialization calls `init_inject`, then
`inject_particles(time,0.,...)`, then `update_injected_particles`. Therefore the
setup rows are execution-backed at time zero, not source-text or compile-only
checks. The ledger does not extrapolate that fact to evolved timesteps.

## Wind numerical policy

`src/tests/test_wind.f90` declares owner Daniel Price and is invoked through the
official `phantomtest wind` selector in `src/tests/testsuite.f90`. Its embedded
rules, unchanged here, include:

- one-dimensional mass flux: `5e-16`;
- Bernoulli constant: `2e-4`;
- velocity profile: `1.6e-1`;
- internal-energy profile: `1.2e-1`;
- density profile: `9e-16`;
- sink particle mass: `8e-6`;
- zero accreted mass: `epsilon(0.)`;
- injected mass: the test's derived `npart_per_sphere * particle_mass / minject`;
- ejected-particle count: integer allowance `npart_per_sphere`.

`SETUP=test2` supplies the baseline non-isothermal profile;
`SETUP=testcyl` adds individual timesteps/free-boundary test conditions; and
`SETUP=test` adds sink radiation/dust nucleation so the Bowen radiative wind
scenario also executes. Those cross-family features are test dependencies, not
primary radiation/dust/MHD ownership.

## Deliberate exclusions

- `mhdwind`, `jetnimhd`, `jetdusty`, and `wddisc` are not primary rows because
  their distinguishing ownership is MHD, non-ideal MHD, or dust.
- `SNe` is not a setup; `balsarakim` is the proven setup selecting
  `inject_sne.f90`.
- GR/radiation-centric injectors such as `grbondi-inject` and `radiotde` are
  excluded from this module cut.
- Other compiled injector files are not silently claimed: only files mapped in
  `comment/module-coverage.md` are declared owned paths.
