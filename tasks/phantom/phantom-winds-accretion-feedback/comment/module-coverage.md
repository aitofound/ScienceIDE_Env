# Declared module boundary and active coverage ledger

This ledger is normative preparation evidence for the declared cut. “Initial
execution” means the official nmax=0 production run reaches `initial.F90` and,
for `INJECT_PARTICLES` builds, calls both the selected injector's `init_inject`
and `inject_particles` at `time=0`. “Numerical execution” means the owner-written
wind test integrates and evaluates its physical assertions.

## Owned production paths

| Owned path / behavior | Active check(s) | Actual execution evidence | Acceptance policy |
|---|---|---|---|
| `src/main/injection.f90` option wrapper and selected injector interface | every SRCINJECT/setup wind row; all 3 wind units | official runtime input read/write plus time-zero injection; wind units iterate | buildbot conformance or upstream assertions |
| `src/main/partinject.f90` injected-particle accounting | all time-zero injector rows; all 3 wind units | `initial.F90` calls `update_injected_particles`; wind tests update injected particles repeatedly | artifact success; upstream injected-mass/count checks in units |
| `src/main/utils_inject.f90` shared injection geometry/helpers | wind rows and 3 wind units | reached by selected wind injector, including profile/shell initialization | upstream wind-unit assertions where numerical |
| `src/main/inject_wind.f90`, `wind.F90`, `wind_equations.f90` | `wind-buildbot-smoke`, `isowind-buildbot-smoke`, `test2-wind-unit`, `testcyl-wind-unit`, `test-wind-unit` | time-zero wind injection in setup rows; full transonic and Bowen integrations in units | upstream buildbot plus `test_wind.f90` tolerances |
| adiabatic wind setup | `wind-buildbot-smoke` | `setup_wind.f90` and time-zero default wind injection | upstream buildbot policy |
| isothermal wind setup | `isowind-buildbot-smoke` | same setup source with official isothermal compile mode | upstream buildbot policy |
| baseline transonic wind | `test2-wind-unit` | owner unit integrates wind and checks mass flux, Bernoulli, profile, sink/injection accounting | upstream numeric assertions |
| individual-timestep/free-boundary wind | `testcyl-wind-unit` | owner unit sets `testcyl` conditions and integrates | upstream numeric assertions |
| transonic plus Bowen radiative wind | `test-wind-unit` | owner unit executes both scenario markers and integrations | upstream numeric assertions; radiation/dust are dependencies |
| `src/main/inject_BHL.f90` | `bhl-buildbot-smoke` | exact `SETUP=BHL`; initial call to BHL injector | upstream buildbot structural runtime policy |
| analytic Bondi setup (`setup_bondi.f90`, `bondiexact.f90`) | `bondi-buildbot-smoke` | official setup and real initial dump | upstream buildbot policy; no claim on `inject_bondi.f90` |
| `src/main/inject_windtunnel.f90` | `windtunnel-buildbot-smoke` | official SRCINJECT selected and called at time zero | upstream buildbot policy |
| `src/setup/readwrite_mesa.f90`, `src/main/inject_masstransfer.f90` default path | `masstransfer-buildbot-smoke` | official default setup and time-zero injector call | upstream buildbot policy; no invented MESA series |
| `evolve_planet.f90`, `inject_randomwind.f90` with asteroid setup | `asteroidwind-buildbot-smoke` | official initial setup and injector call | upstream buildbot policy |
| same random-wind injector with disc setup | `randomwind-buildbot-smoke` | official initial setup and injector call | upstream buildbot policy; disc dependency |
| same injector with boiling-planets setup | `boilingplanets-buildbot-smoke` | official initial setup and injector call | upstream buildbot policy; disc dependency |
| `src/main/inject_disk.f90` | `qpe-buildbot-smoke` | official QPE setup and time-zero injector call | upstream buildbot policy |
| `inject_galcen_winds.f90` plus official stellar data | `galcen-buildbot-smoke` | official stars setup, data lookup, and time-zero galactic wind call | upstream buildbot policy |
| `inject_streamer_pineda.f90` | `streamerdisc-buildbot-smoke` | official streamer setup and time-zero injector call | upstream buildbot policy |
| `inject_sne.f90` initialization | `balsarakim-supernova-buildbot-smoke` | exact official setup calls selected injector at time zero | upstream buildbot policy; MHD/H2 evolution unowned |
| official protostellar-jet initial condition (`setup_sphereinbox.f90`) | `jet-buildbot-smoke` | setup generation and real initial dump | upstream buildbot policy; no evolved MHD-jet claim |

## Compile-time modes

| Mode | Active owner check | Ownership boundary |
|---|---|---|
| `INJECT_PARTICLES=yes`, default wind injector | wind/isowind and unit profiles | owned |
| `SRCINJECT=inject_BHL.f90` | BHL | owned |
| `SRCINJECT=inject_windtunnel.f90` | windtunnel | owned; gravity is dependency |
| `SRCINJECT=readwrite_mesa.f90 inject_masstransfer.f90` | masstransfer | owned default path |
| `SRCINJECT=evolve_planet.f90 inject_randomwind.f90` | asteroidwind/randomwind/boilingplanets | owned injection; disc physics dependency |
| `SRCINJECT=inject_disk.f90` | qpe | owned; no radiation primary ownership |
| `SRCINJECT=inject_galcen_winds.f90` | galcen | owned stellar feedback injection |
| `SRCINJECT=inject_streamer_pineda.f90` | streamerdisc | owned streamer injection |
| `SRCINJECT=inject_sne.f90` | balsarakim | owned feedback initialization; MHD/H2 dependency |
| `ISOTHERMAL=yes` | isowind and relevant setup rows | only its effect on selected injection/setup path is owned |
| `IND_TIMESTEPS=yes` | testcyl plus BHL/windtunnel/etc. setup rows | numerical wind interaction owned only in testcyl; other solver behavior dependency |
| `SINK_RADIATION=yes`, `DUST_NUCLEATION=yes` | test-wind-unit | only their Bowen wind interaction is covered/owned |

## Explicit non-claims / closure

The leaf's full declared boundary is the table above, not all files whose names
start with `inject_`. `inject_firehose.f90`, `inject_keplerian.f90`,
`inject_keplerianshear.f90`, `inject_streamer.f90`, `inject_unifwind.f90`,
`inject_sim.f90`, and `inject_bondi.f90` are outside this selected official
scenario cut. GR/radiation/MHD/dust primary families are likewise outside.

The official nmax=0 policy gives broad initialization coverage but no evolved
trajectory equivalence for BHL, mass transfer, disc/streamer, galactic, SN, or
jet families. That limitation is exposed in the rubric and comments; it must
not be restated as full evolved-physics validation. The only evolved numerical
coverage in this leaf is the three upstream wind-unit profiles. Expanding the
owned boundary to long-time feedback requires new owner-approved official
analyses or calibrated acceptance policies; none are invented here.
