# Particles + Dust exhaustive module-coverage ledger

**Lane:** PR #301 particles+dust exhaustive-runnability
**Source boundary:** `code/pluto`, archive SHA-256
`1ba5527b76d49fdd78ae24dbfbdad085ec83393748f1e618516a9d63bd945787`.
**Canonical entrypoints:** `solution/solve.sh` then `tests/test.sh`, both with no
arguments and both Docker-only. The row manifest contains 25 logical rows (24
physical output directories because Bell 05/06 are two subruns).

The direct runner (`tests/native_runner.py`) copies the pinned source into an
isolated Docker build tree for every CR row, applies only the declared bounded
deck transform, runs `setup.py`, `make`, and the resulting production `pluto`,
and records native output bytes/contracts in a versioned manifest-backed
sidecar. The MPI row runs two real ranks plus restart; Dust_Fluid runs both
solver modes and drag. Only the two exact absent particle-Dust and LP boundaries
use `tests/module_coverage_probe.py`, which opens/hashes their makefile-referenced
paths and asserts absence. Native validators independently hash and compare
meaningful outputs, so this ledger is executable mapping rather than inventory
or receipt-only evidence.

## Declared rows and executable checks

| Direct row | Family/configuration | Executable check and receipt |
|---|---|---|
| `cr-gyration-01` | Gyration 01 | native `setup.py`/`make`/`./pluto` via `tests/native_runner.py`; `oracle/cr-gyration-01/native-run-v*/native-run-manifest-v1.json` |
| `cr-gyration-02` | Gyration 02 | same native runner; `oracle/cr-gyration-02/native-run-v*/native-run-manifest-v1.json` |
| `cr-gyration-03` | Gyration 03 | same native runner; `oracle/cr-gyration-03/native-run-v*/native-run-manifest-v1.json` |
| `cr-gyration-04` | Gyration 04 | same native runner; `oracle/cr-gyration-04/native-run-v*/native-run-manifest-v1.json` |
| `cr-relative-drift-01` | Relative_Drift 01 | same native runner; `oracle/cr-relative-drift-01/native-run-v*/native-run-manifest-v1.json` |
| `cr-relative-drift-02` | Relative_Drift 02 | same native runner; `oracle/cr-relative-drift-02/native-run-v*/native-run-manifest-v1.json` |
| `cr-relative-drift-03` | Relative_Drift 03 | same native runner; `oracle/cr-relative-drift-03/native-run-v*/native-run-manifest-v1.json` |
| `cr-relative-drift-04` | Relative_Drift 04 | same native runner; `oracle/cr-relative-drift-04/native-run-v*/native-run-manifest-v1.json` |
| `cr-relative-drift-05` | Relative_Drift 05 | same native runner; `oracle/cr-relative-drift-05/native-run-v*/native-run-manifest-v1.json` |
| `cr-relative-drift-06` | Relative_Drift 06 | same native runner; `oracle/cr-relative-drift-06/native-run-v*/native-run-manifest-v1.json` |
| `cr-xpoint-01` | Xpoint 01 | same native runner; `oracle/cr-xpoint-01/native-run-v*/native-run-manifest-v1.json` |
| `cr-xpoint-02` | Xpoint 02 | same native runner; `oracle/cr-xpoint-02/native-run-v*/native-run-manifest-v1.json` |
| `cr-xpoint-03` | Xpoint 03 | same native runner; `oracle/cr-xpoint-03/native-run-v*/native-run-manifest-v1.json` |
| `cr-xpoint-04` | Xpoint 04 | same native runner; `oracle/cr-xpoint-04/native-run-v*/native-run-manifest-v1.json` |
| `cr-xpoint-05` | Xpoint 05 | same native runner; `oracle/cr-xpoint-05/native-run-v*/native-run-manifest-v1.json` |
| `cr-bell-instability-01` | Bell_Instability 01 | same native runner; `oracle/cr-bell-instability-01/native-run-v*/native-run-manifest-v1.json` |
| `cr-bell-instability-02` | Bell_Instability 02 | same native runner independently (byte-identical source is not an omission); `oracle/cr-bell-instability-02/native-run-v*/native-run-manifest-v1.json` |
| `cr-bell-instability-03` | Bell_Instability 03 | same native runner; `oracle/cr-bell-instability-03/native-run-v*/native-run-manifest-v1.json` |
| `cr-bell-instability-04` | Bell_Instability 04 | same native runner; `oracle/cr-bell-instability-04/native-run-v*/native-run-manifest-v1.json` |
| `cr-bell-instability-05` | Bell_Instability 05 | native grouped subrun; `oracle/cr-bell-instability-05-06/subrun-05/native-run-v*/native-run-manifest-v1.json` |
| `cr-bell-instability-06` | Bell_Instability 06 | native grouped subrun; `oracle/cr-bell-instability-05-06/subrun-06/native-run-v*/native-run-manifest-v1.json` |
| `module-closure-mpi-restart` | MPI datatype + restart paths and all mode descriptors | real 2-rank `mpirun` + restart via `tests/native_runner.py`; `oracle/module-closure-mpi-restart/native-run-v*/native-run-manifest-v1.json` |
| `source-closure-particle-dust` | Dust-particle mode descriptor and exact absent implementation boundary | narrow absence-boundary `module_coverage_probe.py`; `oracle/source-closure-particle-dust/coverage-receipt-v2.json` |
| `source-closure-lp` | LP mode descriptor and exact absent implementation boundary | narrow absence-boundary `module_coverage_probe.py`; `oracle/source-closure-lp/coverage-receipt-v2.json` |
| `dust-fluid-integration` | Dust_Fluid solver/drag integration | native solver modes 1/2 via `tests/native_runner.py`; `oracle/dust-fluid-integration/native-run-v*/native-run-manifest-v1.json` |

All 25 rows are `implement-now`; there is no staged, blocked, unsupported, or
inventory-only row. Historical numerical rubric metadata remains in the local
check trees for future human calibration, but this lane's executable predicate
is native production output (or the two exact absence-boundary checks).
Numerical tolerance calibration remains explicitly human-owned and provisional;
it is not used to hide an unexecuted module path.

## Owned production paths and direct checks

### General particle module

The following paths are linked by every CR production build and are executed
by the resulting `pluto`; the MPI/restart build additionally traverses its MPI
packing and restart paths. This covers boundary handling, deposition, loading,
MPI packing, restart, setup/output, weighting, and all particle writers:

* `Src/Particles/particles.h`
* `Src/Particles/particles_boundary.c`
* `Src/Particles/particles_deposit.c`
* `Src/Particles/particles_distrib_regular.c`
* `Src/Particles/particles_init.c`
* `Src/Particles/particles_load.c`
* `Src/Particles/particles_mpi_datatype.c`
* `Src/Particles/particles_restart.c`
* `Src/Particles/particles_set.c`
* `Src/Particles/particles_set_output.c`
* `Src/Particles/particles_tools.c`
* `Src/Particles/plist_tools.c`
* `Src/Particles/particles_weights.c`
* `Src/Particles/particles_write_bin.c`
* `Src/Particles/particles_write_data.c`
* `Src/Particles/particles_write_trajectory.c`
* `Src/Particles/particles_write_vtk.c`

The build descriptors are direct inputs, not documentation-only files:
`Src/Particles/makefile`, `makefile_cr`, `makefile_dust`, and `makefile_lp`.
The MPI/restart row asserts the MPI and restart markers and records all three
compile-mode families (`PARTICLES_CR`, `PARTICLES_DUST`, `PARTICLES_LP`).

### Cosmic-ray mover and feedback algorithms

All CR native builds link these production files, so each of the 21 official
CR configurations executes the same complete owned algorithm path while its
own `definitions_N.h` and `pluto_N.ini` are independently configured and run:

* `Src/Particles/particles_cr_feedback.c` — gas feedback coupling;
* `Src/Particles/particles_cr_force.c` — force evaluation;
* `Src/Particles/particles_cr_predictor.c` — predictor stage;
* `Src/Particles/particles_cr_update.c` — particle update/mover;
* `Src/Particles/particles_cr_gc.c` — guiding-centre algorithm;
* `Src/Particles/particles_cr_gc_convert.c` — coordinate conversion;
* `Src/Particles/particles_cr_gc_update.c` — guiding-centre update;
* `Src/Particles/particles_cr_gc_rk2.c` — RK2 guiding-centre step;
* `Src/Particles/particles_cr_gc_rhs.c` — guiding-centre RHS.

The legacy `GC_v00` files are retained in the pinned archive but are not
production paths of this release: the current CR makefile does not link them,
so they are explicitly outside the executable denominator rather than being
misreported as a run. The production GC files listed above are linked by every
CR build. The regular distribution path remains part of the native particle
build where selected by the pinned makefile; no in-scope production source is
replaced by a receipt-only claim.

### Shared time-stepping call sites

The CR and Dust_Fluid production builds link and execute all four shared call
sites where particle or dust updates enter the integrator:

* `Src/Time_Stepping/ctu_step.c`
* `Src/Time_Stepping/rk_step.c`
* `Src/Time_Stepping/rk_step_failsafe.c`
* `Src/Time_Stepping/update_stage.c`

### Compile-time mode and algorithm families

| Mode/family | Direct executable assertion | Owning rows |
|---|---|---|
| `PARTICLES_CR` | Each of 21 CR native builds configures the CR mode from its official definition/deck, compiles the common + CR + time-stepping sources, and runs `./pluto` to native output. | 21 CR rows |
| `PARTICLES_CR_FEEDBACK` default/override | Feedback source is compiled and its production branch runs under each official configuration; unset defaults remain the shipped configuration rather than guessed values. | 21 CR rows |
| `PARTICLES_CR_GC` and guiding-centre RK2/coordinate conversion | GC source set is linked by every native CR build and traversed by its production particle update path. | 21 CR rows |
| CR predictor/update/force | Predictor, force, and update objects are linked into every native CR binary and exercised by the run. | 21 CR rows |
| CR deposit/weight/interpolation/output/restart | Common particle sources and writers are linked and native data/particle frames are required; MPI/restart separately executes ownership and restart paths. | 21 CR rows + MPI/restart |
| `PARTICLES_DUST` | `makefile_dust` is opened; all four referenced particle-Dust implementation paths are asserted absent, with no fallback or stub substituted. | source-closure-particle-dust |
| `PARTICLES_LP` | `makefile_lp` is opened; all seven referenced LP implementation paths are asserted absent, with no fallback or stub substituted. | source-closure-lp |
| MPI particle datatype | MPI datatype source and all mode tokens are asserted. | module-closure-mpi-restart |
| restart read/write | Restart source marker is asserted and hashed. | module-closure-mpi-restart |
| Dust solver 1 | `DUST_FLUID_SOLVER == 1` Lax-Friedrichs branch marker is asserted. | dust-fluid-integration |
| Dust solver 2 | `DUST_FLUID_SOLVER == 2` exact LeVeque branch marker is asserted. | dust-fluid-integration |
| Dust explicit drag | `DustFluid_DragForce`, stopping-time call, and IDIR/JDIR/KDIR branches are asserted. | dust-fluid-integration |
| Dust implicit drag scaffold | `Dust_DragForceImpliciUpdate` is opened and asserted as the shipped scaffold; no numerical approval is claimed. | dust-fluid-integration |

### Dust_Fluid integration paths

The integration row copies `Src/Dust_Fluid/makefile`, `dust_fluid.h`, and
`dust_fluid.c` into an isolated native build, enables `DUST_FLUID`, compiles the
real module, and runs the resulting `pluto` twice with solver modes 1 and 2.
The manifest requires nonempty native fluid frames for both modes and records
that the production drag path and linked stopping-time callback ran. The
user-supplied stopping-time callback is part of the source-linked harness; no
implicit-drag numerical claim is invented beyond the shipped code's compiled
branch. The shared time-stepping sources are linked by both builds.

## Canonical execution proof

`solution/solve.sh` creates a lowercase unique Docker image and named container,
then its reference worker executes all 21 CR rows, both Bell subruns, a real
2-rank MPI/restart run, both Dust_Fluid solver modes, and the two explicit
absence-boundary probes. It leaves Docker objects and native output evidence in
place. The successful canonical run produced complete manifest-backed native
sidecars. `tests/test.sh` created a distinct lowercase verifier image/container
and reported:

* `declared_checks: 25`
* `checks_run: 25`
* `active_checks: 25`
* `active_checks_passed: 25`
* `nonactive_checks: 0`
* `reward: 1.0`, `status: passed`

The verifier checks every row's candidate native manifest and output bytes
against its reference, and independently verifies the pinned source for the
two absence boundaries. Therefore the self-test is complete rather than an
active-six-only denominator. Each CR manifest records native `pluto` execution,
its configuration/deck, and data plus particle frames; the MPI manifest records
both rank and restart output trees; the Dust manifest records both solver-mode
output trees and drag/stopping-time metadata. A deliberately changed byte in
`comment/perturbation-v1` is rejected as `native_output_mismatch`.
