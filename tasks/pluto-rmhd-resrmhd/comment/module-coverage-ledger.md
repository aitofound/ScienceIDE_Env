# RMHD + radiation-coupled RMHD module coverage ledger

## Boundary and closure rule

This ledger is the executable coverage contract for the 21 direct checks in
`tests/checks/`. A row is covered only when its `build/deck.py` prepares the
official check configuration, `solution/solve-in-container.sh` compiles the
leaf-vendored source and runs the resulting `./pluto` binary with no arguments,
and `tests/test-in-container.sh` invokes that row's `validate.py` against both
independently generated output roots. The continuation adds three explicit
official Particles/CR/Xpoint ResRMHD rows; they are executable coverage, not
inventory-only annotations. Importing, compiling without running,
listing a symbol, or relying on a conservation invariant does not count.

The source closure is the complete local `code/pluto` tree at the pinned
PLUTO 4.4-patch4 archive hash recorded in `comment/provenance.txt`. For the
radiation-coupled RMHD rows, `Src/RMHD/mod_defs.h:46-57` includes
`../Radiation/radiation.h`; therefore the production radiation implementation
is the leaf's root `Src/Radiation/` subtree. The solver copies this entire
source tree into an isolated per-check build, runs setup, compiles it, and
executes it twice. There is no sibling checkout or network path.

Every row below is **RUNNABLE/PASS-ELIGIBLE**, not staged, blocked, unsupported,
or inventory-only. Human-owned scientific tolerance calibration remains in the
individual `rubric.json` files; it does not remove a path from this ledger.

## Direct-check matrix (every declared row)

| Direct check (executable path) | Official configuration and mode family | Production paths/algorithms exercised | Output/acceptance witness |
|---|---|---|---|
| `rad-rmhd-blast-01` | `Radiation/Relativistic/RMHD_Blast` 01; 2-D Cartesian, RMHD + radiation, Ideal EOS, Linear + RK2, CT arithmetic, hydro HLLC/radiation HLLC, Newton-gas implicit solve, diffusion limiting | Root `Src/Radiation/{radiation.h,rad_step.c,newtonsolve.c,rad_sourcef.c,rad_flux.c,rad_hllc.c,rad_speed.c,flux_lim.c,eddtensor.c,rad_errors.c,rad_tools.c,rad_set_solver.c}` plus RMHD flux/primitive/CT paths | Reviewed executable deck: 64x64, `tstop=3.6`, `dbl=.36` (the source/rubric path remains the shipped config; the mesh is reduced only for runnability); `data.*.dbl`, `dbl.out`, and CT `grid.out`; all post-initial frames are compared |
| `rad-rmhd-blast-02` | Radiation blast 02; 2-D Cartesian, Ideal + Linear + RK2, CT arithmetic, hydro HLL/radiation default fixed-point-radiation path, strong absorption | Same root radiation closure, including the default `RADIATION_IMPL` branch and HLL radiation solver, plus RMHD and CT paths | Reviewed executable deck: 180x180, `tstop=3.6`, `dbl=.36` (the smallest stable mesh documented by the row rubric); raw variable-major float64 frames, metadata, and staggered-grid sizing |
| `rad-rmhd-blast-03` | Radiation blast 03; 2-D Cartesian, Ideal + Linear + RK2, CT arithmetic, hydro HLL, `RADIATION_IMEX_SSP2=YES`, diffusion limiting | Root radiation IMEX/explicit-stage path (`rad_rk_step.c`, `rad_update_stage.c`, `rad_rhs.c`, `rad_substepping.c`) together with implicit coupling, RMHD and CT | Reviewed executable deck: shipped 360x360, `tstop=1.28`, `dbl=.16` (the shipped CFL=0.03 IMEX-SSP2 high-absorption configuration fails RMHD primitive recovery at coarse 64x64 and 180x180 meshes; the source/rubric time window is retained); every post-initial frame, exact nstep/time/dt, variables, and CT grid |
| `rmhd-blast-08` | `RMHD/Blast` 08; 2-D Cartesian, Ideal, WENOZ + RK3, CT_CONTACT, HLL solver with `ENABLE_HLLEM=YES` | RMHD `hllem.c`, HLL speed/flux, WENOZ states, RK update, CT contact EMF and staggered-field I/O | CT `grid.out` and all variable blocks/frames; largest numerical shock/CT witness in this family |
| `rmhd-blast-09` | `RMHD/Blast` 09; 2-D Cartesian, Ideal, Linear + Hancock, CT arithmetic, HLLC_MB | RMHD `hllc_mb.c`, Hancock stage path, CT arithmetic, primitive recovery and energy inversion | CT grid and every post-initial state/metadata frame |
| `rmhd-cp-alfven-01` | `RMHD/CP_Alfven` 01; 2-D Cartesian, Ideal, Linear + RK2, CT arithmetic, HLLD | RMHD `hlld.c`, circular-polarized wave initialization, CT arithmetic/field averaging, exact-wave propagation | CT grid plus all field/primitive blocks and time base |
| `rmhd-cp-alfven-06` | `RMHD/CP_Alfven` 06; 3-D Cartesian, Ideal, WENOZ + RK4, UCT_GFORCE, GFORCE solver | RMHD `gforce.c`, WENOZ, RK4, UCT GFORCE EMF, 3-D directional loops and CT staggered arrays | 32^3 CT grid; all frames and staggered block sizes |
| `rmhd-jet-01` | `RMHD/Jet` 01; 3-D Polar, Ideal, Linear + RK2, EIGHT_WAVES, TVDLF | RMHD `tvdlf.c`, Powell/eight-wave source path, polar geometry/rotation, 3-D jet initialization and source update | All variable-major frames and exact time/nstep metadata (grid file intentionally not required by this deck) |
| `rmhd-jet-04` | `RMHD/Jet` 04; 2-D Cylindrical, Taub EOS, Linear + RK2, DIV_CLEANING, HLLD | RMHD `hlld.c`, `Src/EOS/Taub/eos.c`, GLM `Src/MHD/GLM/glm.c`, cylindrical geometry, vector-potential startup | CT/GLM-compatible grid and all primitive/GLM variables in every frame |
| `rmhd-kh-01` | `RMHD/KH` 01; 2-D Cartesian, Ideal, Parabolic + RK3, CT arithmetic, HLL | Parabolic/PPM state reconstruction, RMHD HLL, CT arithmetic and KH seed/roll-up path | CT grid plus every eligible frame |
| `rmhd-kh-02` | `RMHD/KH` 02; 2-D Cartesian, Ideal, Linear + Hancock, CT arithmetic, HLLD | Hancock integration, RMHD HLLD, CT arithmetic and KH perturbation path | CT grid plus every eligible frame |
| `rmhd-rotor-08` | `RMHD/Rotor` 08; 3-D Cartesian, Ideal, LimO3 + RK3, DIV_CLEANING, TVDLF | LimO3 reconstruction, RMHD TVDLF, GLM divergence-cleaning scalar waves, 3-D rotor source/geometry | Every state and exact metadata frame; acceleration-labelled 96^3 direct workload |
| `rmhd-rotor-09` | `RMHD/Rotor` 09; 2-D Cartesian, Ideal, Linear + RK2, UCT_GFORCE, GFORCE | RMHD GFORCE, UCT GFORCE EMF, CT averaging/update, rotor initialization | CT grid and all eligible frames |
| `rmhd-shock-cloud-03` | `RMHD/Shock_Cloud` 03; 2-D Cartesian, Taub EOS, selective entropy switch, MULTID flattening, Linear + RK2, no div-B control, HLL | Taub thermodynamics, selective entropy/pressure recovery, multidimensional shock flattening, HLL and no-CT source path | All state blocks and exact time base |
| `rmhd-sod-01` | `RMHD/Sod` 01; 1-D Cartesian, Ideal, Linear + RK3, no div-B control, HLL | 1-D RMHD Riemann/HLL, RK3, ideal EOS, shock-tube initialization and output | Every post-initial frame and metadata |
| `rmhd-sod-1d` | Leaf-native RMHD/Sod smoke contract; 1-D Cartesian, Ideal, Linear + RK3, no div-B control, HLL; whole-state L2 policy | Same one-dimensional solver/recovery path as the official Sod family, independently exercised with its own deck and 21-frame cadence | Whole-state L2 over all variables plus exact frame/time/nstep checks |
| `rmhd-toroidal-jet-01` | `RMHD/Toroidal_Jet` 01; 2-D Cylindrical, Ideal, Linear + Hancock, OSPRE limiter, HLLC | Cylindrical coordinate geometry, OSPRE limiting, Hancock and HLLC, toroidal-jet initialization | All eligible frames and exact metadata |
| `rmhd-toroidal-jet-03` | `RMHD/Toroidal_Jet` 03; 2-D Cylindrical, Ideal, Linear + RK2, OSPRE limiter, GFORCE | Cylindrical geometry, OSPRE, RK2 and GFORCE solver/source path | All eligible frames and exact metadata |
| `resrmhd-xpoint-03` | Official `Particles/CR/Xpoint` 03; 2-D Cartesian, `PHYSICS ResRMHD`, Ideal EOS, Linear + RK2, electric-field and vector-potential evolution, CR particles | ResRMHD primitive/conservative closure, Maxwell/electric-field branches, particle coupling and user deposition, official `init.c`/`particles_init.c` | Native no-argument `./pluto`; raw float64 state frames, exact `dbl.out` time base and all emitted variables |
| `resrmhd-xpoint-04` | Official `Particles/CR/Xpoint` 04; same ResRMHD mode with guide field and unit normalization | ResRMHD guide-field/electric-field branch, CR particle coupling, vector-potential update and official user deposition | Native no-argument `./pluto`; raw float64 state frames, exact `dbl.out` time base and all emitted variables |
| `resrmhd-xpoint-05` | Official `Particles/CR/Xpoint` 05; 2-D Cartesian, `PHYSICS ResRMHD`, guiding-center CR branch, RK midpoint particle stepping | ResRMHD guiding-center particle path, Maxwell/electric-field closure, particle deposition and no-vector-potential-update branch | Native no-argument `./pluto`; raw float64 state frames, exact `dbl.out` time base and all emitted variables |

## Owned production-path coverage

The following groups are the owned, source-selected production closure. The
check lists are direct executable checks from the matrix above; each check
builds and runs the path, rather than merely importing it.

### RMHD module and primitive/conservative closure

| Production path group | Concrete source files in the leaf | Direct executable checks |
|---|---|---|
| Module indices, geometry labels, wave labels, solver dispatch | `Src/RMHD/mod_defs.h`, `set_solver.c`, `mappers.c`, `mappers_loc.c` | All 21; solver dispatch diversity is directly run by `rad-01`, `rad-02`, `rad-03`, `rmhd-blast-08/09`, `rmhd-cp-alfven-01/06`, `rmhd-jet-01/04`, `rmhd-kh-01/02`, `rmhd-rotor-08/09`, `rmhd-shock-cloud-03`, and both Sod/Toroidal rows |
| RMHD primitive equations, four-velocity, fluxes, eigenstructure and wave speeds | `prim_eqn.c`, `four_vel.c`, `fluxes.c`, `eigenv.c`, `hll_speed.c`, `quartic.c`, `vel_limiter.c` | All 21; Cartesian, polar, and cylindrical witnesses; 1-D/2-D/3-D witnesses |
| Riemann algorithms in the declared production family | `hll.c`, `hllc_mb.c`, `hlld.c`, `hllem.c`, `gforce.c`, `tvdlf.c` | HLL: rad-02/03, blast-08, KH-01/02, shock-cloud, Sod; HLLC_MB: rad-01/blast-09; HLLD: CP-01, KH-02, jet-04; HLLEM: blast-08; GFORCE: CP-06, rotor-09, toroidal-03; TVDLF: jet-01, rotor-08 |
| RMHD conserved-to-primitive and pressure/entropy recovery | `rmhd_energy_solve.c`, `rmhd_entropy_solve.c`, `rmhd_pressure_fix.c`, `rhs.c`, `rhs_source.c`, `source.c`, `riemann_check.c` | All 21; Taub and Ideal EOS rows additionally force the two EOS/recovery families |
| RMHD compile-time switches and source terms | `RMHD/mod_defs.h` branches for `DIVB_CONTROL`, `HAVE_ENERGY`, geometry labels, radiation labels, reduced energy; `source.c`, `rhs_source.c` | CT: rad-01/02/03, blast-08/09, CP-01/06, KH-01/02, rotor-09; GLM: jet-04 and rotor-08; eight-wave: jet-01; no-divB: shock-cloud, Sod/Sod-1d, Toroidal-jet |

The official dispatcher exposes `hll`, `hllc`/`hllc_mb`, `hllc_kb`, `hlld`,
`hllem` (when enabled), `gforce`, and `tvdlf`. The declared official RMHD
portfolio uses every enabled production family that appears in the selected
21-row full module cut: HLL, HLLC_MB, HLLD, HLLEM, GFORCE, and TVDLF. `hllc_kb` is not
selected by any official deck in this pinned RMHD cut and is therefore not an
owned configuration family of this 21-row module; the dispatcher remains covered
by every selected solver call. Commented-out legacy `gmusta1`, `musta`, and
`rusanov_dw` branches are not production paths and are not silently counted.

### Radiation-coupled RMHD closure

| Production path group | Concrete source files | Direct executable checks |
|---|---|---|
| Radiation labels, moments, implicit state and source interface | `Src/Radiation/radiation.h`, `rad_tools.c`, `rad_sourcef.c`, `rad_errors.c`, `newtonsolve.c` | rad-rmhd-blast-01/02/03 |
| Radiation Riemann/flux/speed choices | `rad_flux.c`, `rad_hll.c`, `rad_hllc.c`, `rad_set_solver.c`, `rad_speed.c` | `rad-rmhd-blast-01` directly selects HLLC; `rad-rmhd-blast-02/03` directly select HLL; all three run `Rad_SetSolver`, flux, and speed paths |
| Matter-radiation coupling and iteration modes | `rad_step.c`, `newtonsolve.c`, `rad_errors.c`; `RADIATION_IMPL=RADIATION_NEWTON_GAS` in 01, default fixed-point-radiation in 02, `RADIATION_IMEX_SSP2=YES` in 03 | rad-rmhd-blast-01/02/03; each writes and validates every post-initial coupled state |
| Diffusion/closure/variable Eddington paths | `flux_lim.c`, `eddtensor.c`, `radiation.h` diffusion switches | rad-rmhd-blast-01/02/03 (`RADIATION_DIFF_LIMITING=YES`) |
| Explicit radiation stage and RHS path | `rad_rk_step.c`, `rad_update_stage.c`, `rad_rhs.c`, `rad_substepping.c` | rad-rmhd-blast-03 (`RADIATION_IMEX_SSP2=YES`); rad-01/02 exercise the non-IMEX RMHD coupling path |
| Radiation output layout and CT interaction | `Src/RMHD/mod_defs.h:46-57`, `Src/Radiation/radiation.h`, root `Src/bin_io.c`, `Src/set_grid.c`, `Src/write_data.c` | All three radiation rows require and parse `grid.out`, including nonuniform/staggered `Bx1s/Bx2s/Bx3s` block sizes |

The parallel directory `Src/RMHD/Radiation/` is audited source material but is
not selected by the production include path: the RMHD module includes
`../Radiation/radiation.h`, resolving to root `Src/Radiation/`, and the three
radiation deck builds use that root makefile/object closure. It is therefore
not a second hidden implementation or an omitted runnable row. The ledger
covers the source that the declared builds actually compile and execute.

### Constrained transport, GLM, and divergence-control modes

| Mode family | Concrete source paths | Direct checks |
|---|---|---|
| CT arithmetic/contact/UCT GFORCE | `Src/MHD/CT/ct.h`, `ct_emf.c`, `ct_emf_average.c`, `ct_field_average.c`, `ct_fill_mag_field.c`, `ct_flux.c`, `ct_update.c`, `emf_boundary.c`, `emf_userdef.c`, `ct_imex_update_ELR.c` as selected by compile macros | Arithmetic: rad-01/02/03, blast-09, CP-01, KH-01/02; CT_CONTACT: blast-08; UCT_GFORCE: CP-06 and rotor-09; every CT row validates staggered output |
| GLM divergence cleaning | `Src/MHD/GLM/glm.h`, `glm.c`, `analysis.c` | `rmhd-jet-04`, `rmhd-rotor-08`; both output the GLM state and enforce exact metadata |
| Eight-wave/Powell | RMHD `mod_defs.h` eight-wave branch, `source.c`, `rhs_source.c`, MHD source helpers | `rmhd-jet-01` |
| No divergence control | RMHD source/flux branch with `DIVB_CONTROL=NO` | shock-cloud-03, Sod-01, Sod-1d, toroidal-jet-01/03 |

The ResRMHD-specific electric-field/Maxwell branches in `ct.h` are compile-time
conditional on `PHYSICS == ResRMHD`. The continuation's official Xpoint rows
03/04/05 select that token byte-identically in their definitions headers and
execute the corresponding fluid/electric/particle closure. Rows 03 and 04
exercise vector-potential evolution and guide-field variation; row 05 executes
the guiding-center branch with vector-potential updates disabled. These are
native solver rows, not inventory-only or blocked claims.

### ResRMHD and particle-coupled Maxwell closure

| Production path group | Concrete source/config files | Direct executable checks |
|---|---|---|
| ResRMHD compile-time module and electric-field state | `Src/pluto.h`, `Src/structs.h`, `Src/set_indexes.c`, `Src/var_names.c`, `Src/RMHD/mod_defs.h`, `Src/RMHD/rhs.c`, `Src/MHD/CT/ct.h` | `resrmhd-xpoint-03/04/05` with `PHYSICS ResRMHD` |
| ResRMHD configuration dispatch | `Tools/Python/define_problem.py:108-112,349-364,548,614-651` (`ProcessResRMHDModule`, ResRMHD menu/flags and source closure) | all three Xpoint package builds invoke `setup.py --auto-update` against the leaf's staged source |
| Resistive RMHD Maxwell/electric update and primitive recovery | `Src/MHD/CT/ct_maxwell_solver.c`, `ct_maxwell_solver_balsara_rec.c`, `ct_emf.c`, `ct_update.c`, `Src/RMHD/rmhd_energy_solve.c`, `rmhd_pressure_fix.c` | `resrmhd-xpoint-03` and `resrmhd-xpoint-04` native no-argument runs |
| Guide-field and vector-potential branches | official `Test_Problems/Particles/CR/Xpoint/definitions_03.h`, `_04.h`, `init.c`, `pluto_03.ini`, `pluto_04.ini`; `Src/vec_pot_update.c` | `resrmhd-xpoint-03/04` |
| Guiding-center particle coupling and midpoint stepping | official `definitions_05.h`, `particles_init.c`, `Src/Particles/particles_cr_gc*.c`, `Src/Particles/particles_cr_update.c` | `resrmhd-xpoint-05` native run; `check.json` carries the acceleration label |
| User particle deposition and output | official `userdef_output.c`, `Src/Particles/particles_deposit.c`, `Src/write_data.c`, `Src/var_names.c` | all three ResRMHD rows; validators compare every native `dbl.out` variable block |

### EOS, states, geometry, and time stepping

| Family | Source paths | Direct checks |
|---|---|---|
| Ideal EOS and relativistic recovery | `Src/EOS/Ideal/eos.c`, `eos.h`; RMHD recovery files above | All rows except `rmhd-jet-04` and `rmhd-shock-cloud-03`; radiation rows are Ideal |
| Taub EOS | `Src/EOS/Taub/eos.c`, `eos.h` | `rmhd-jet-04`, `rmhd-shock-cloud-03` |
| Linear, WENOZ, Parabolic, LimO3 reconstruction | `Src/States/flat_states.c`, `plm_coeffs.c`, `plm_states.c`, `weno3_states.c`, `mp5_states.c`, `ppm_coeffs.c`, `ppm_states.c`, `limo3_states.c`, `char_tracing.c`, `fd_states.c`, `hancock.c`, `flatten.c` as selected by definitions | Linear: rad-01/02/03, blast-09, CP-01, jet rows, KH-02, rotor-09, shock-cloud, Sod, Toroidal; WENOZ: blast-08 and CP-06; Parabolic: KH-01; LimO3: rotor-08 |
| RK2, RK3, RK4, Hancock | `Src/Time_Stepping/rk_step.c`, `update_stage.c`, `rk_step_failsafe.c`, `ctu_step.c`; reconstruction `hancock.c` | RK2: rad-01/02/03, CP-01, jet-01/04, rotor-09, shock-cloud, toroidal-03; RK3: blast-08, KH-01, rotor-08, Sod/Sod-1d; RK4: CP-06; Hancock: blast-09, KH-02, toroidal-01 |
| Cartesian, polar, cylindrical and 1-D/2-D/3-D indexing | `Src/set_geometry.c`, `cartcoord.c`, `rotate.c`, `rotate.h`, `mappers3D.c`, `set_grid.c`, geometry branches in RMHD/radiation headers | Cartesian: rad rows, blast/CP/KH/rotor/Sod; Polar: jet-01; Cylindrical: jet-04 and both Toroidal rows; 1-D: Sod rows; 3-D: CP-06, jet-01, rotor-08 |
| CFL reduction and exact output time base | `Src/set_grid.c`, `get_nghost.c`, `runtime_setup.c`, `set_output.c`, `write_data.c`, `bin_io.c`, time-step portions of `rhs.c` and time-stepping files | All 21; each validator requires contiguous frames, finite time/dt, exact nstep sequence, and rubric time tolerance |

### Common executable/output closure

The build and run of every direct check also traverses the applicable common
production paths: `Src/main.c`, `startup.c`, `initialize.c`, `input_data.c`,
`parse_file.c`, `cmd_line_opt.c`, `set_indexes.c`, `set_grid.c`,
`set_geometry.c`, `runtime_setup.c`, `boundary.c`, `check_states.c`,
`reconstruct.c`, `adv_flux.c`, `fd_flux.c`, `parabolic_update.c`,
`split_source.c`, `set_output.c`, `write_data.c`, `bin_io.c`, `output_log.c`,
`var_names.c`, `userdef_output.c`, `failsafe.c`, and `tools.c`, plus the
selected `Src/Math_Tools/` helpers used by reconstruction/recovery and the
selected setup-generated object list. The acceptance output contract is
executable, not documentary: raw little-endian float64 variable-major blocks,
all variables named in `dbl.out`, all post-initial frames, exact metadata, and
`grid.out`/staggered sizes for CT rows.

## Fixture and verifier evidence

Every direct check has `validate.py`, `rubric.json`, and `fixtures/make.py`.
The complete task-local fixture gate runs every fixture through its own
validator. For each row it includes identity, allowed roundoff/denormal flush,
near-miss, wrong cell, frame omission, nstep/dt error, wrong output mode,
float32, NaN/Inf, and (where the output contract requires it) missing-grid and
staggered-block cases. The ResRMHD rows use the same perturbation-sensitive
raw-float64 and metadata predicate and add explicit source/config closure;
`resrmhd-xpoint-05/check.json` carries the acceleration label for its guiding-
center work, while the prior `rmhd-rotor-08` workload label remains valid.

The full no-argument Docker evidence is recorded in
`comment/canonical-entrypoint-run.md`: the solver must run all 21 rows twice
into physically distinct retained roots, and the verifier must report
`passed_checks=21`, `total=21`, `reward=1.0`, and `self_test_ok=true`, with both
commands exiting 0. Docker images and containers use lowercase unique names,
are persistent (no `--rm`), and logs/roots/build evidence are retained.

## Status

Coverage is declared at the complete 21-row RMHD+ResRMHD boundary. The three
ResRMHD rows are now package-backed executable checks and remain subject to the
canonical no-argument Docker solve/test evidence. Remaining review items are
human-owned pass-policy calibration only (scientific tolerances, convergence
and any non-identical accelerator policy); until that full run reaches terminal
0/0, this ledger must not be represented as a passing self-test.

Continuation status (2026-08-28T10:17Z): strict preparation copied only the 12
independently validated historical pairs into
`comment/docker-continuation-20260828t10181787912312z-43416/`; the nine pending
rows, including all three ResRMHD rows, remained absent. Docker image creation
then failed before any container or native pending output was produced because
`/var/lib/docker/tmp` reported `no space left on device` (solver log
`solve.docker.20260828T101842Z-43562.log`; verifier log
`test.docker.20260828T101851Z-43648.log`). The manifest remains
`pending-fresh-rows`; no genuine ResRMHD execution has been accepted and the
ledger is incomplete rather than a 21/21 pass. The final exact
`./solution/continue.sh` retry created
`comment/docker-continuation-20260828t10221787912569z-44888/` and retained
`solve.docker.20260828T102259Z-44972.log`, which records the same pre-container
Docker ENOSPC blocker.
