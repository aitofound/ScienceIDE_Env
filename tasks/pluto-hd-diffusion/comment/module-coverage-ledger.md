# PLUTO HD + diffusion module coverage ledger

## Scope and acceptance rule

This ledger is the executable coverage contract for the active leaf at
`tasks/pluto-hd-diffusion`. The active inventory is exactly the thirty-five
direct check directories C01-C35 under `tests/checks/`. C01-C20 cover HD, EOS,
viscosity, and FARGO; C21-C35 are executable HD thermal-conduction/diffusion
continuations using official vendored TCfront source/configuration variants.
Every active row is attempted by the no-argument `solution/solve.sh`, which
builds its own pinned-source Docker image, runs one uniquely named lowercase
container, copies the native outputs only after container exit, and applies
`solution/output_contract.py`.
The no-argument `tests/test.sh` then dispatches every row through
`tests/lib/run_check.py` and the row validator. A row is complete only when all
four solve stages (build, run, copy, contract) and the verifier pass; there is
no staged, blocked, unsupported, inventory-only, or zero-reward row in the
active denominator.

The mandatory self-test is one solve followed by one verifier invocation with
that solve's single oracle root mounted as both reference and candidate. It is
not a second solve or a fresh campaign. The validators compare every
post-initial native FP64 cell/frame and exact frame/step metadata. The current
fixture bounds and the human science pass policy remain provisional; a
byte-identical one-solve comparison is an informational self-pass and does not
promote a calibrated tolerance.

## Row-to-configuration ledger

| Row | Official family / configuration | Executed production mode | Direct executable acceptance |
|---|---|---|---|
| C01 `c01-hd-sod-08` | `Test_Problems/HD/Sod`, `08` | 1-D Cartesian PVTE EOS; RK2 + LINEAR + HLLC shock conversion | `tests/checks/c01-hd-sod-08/Dockerfile` -> `run.sh` -> native contract -> `validate.py` |
| C02 `c02-hd-riemann-2d-03` | `Test_Problems/HD/Riemann_2D`, `03` | 2-D Cartesian IDEAL four-quadrant Riemann; RK3 + WENO3 + HLLC | `tests/checks/c02-hd-riemann-2d-03/Dockerfile` -> `run.sh` -> native contract -> `validate.py` |
| C03 `c03-hd-isentropic-vortex-03` | `Test_Problems/HD/Isentropic_Vortex`, `03` | 2-D Cartesian IDEAL smooth vortex; RK3 + WENOZ_FD + finite-difference flux | `tests/checks/c03-hd-isentropic-vortex-03/Dockerfile` -> `run.sh` -> native contract -> `validate.py` |
| C04 `c04-hd-disk-planet-03` | `Test_Problems/HD/Disk_Planet`, `03` | 3-D spherical ISOTHERMAL potential/rotating frame; RK2 + LINEAR + user boundary | `tests/checks/c04-hd-disk-planet-03/Dockerfile` -> `run.sh` -> native contract -> `validate.py` |
| C05 `c05-hd-viscosity-flow-past-cylinder-02` | `Test_Problems/HD/Viscosity/Flow_Past_Cylinder`, `02` | 2-D polar IDEAL viscosity with SUPER_TIME_STEPPING; RK2 + LINEAR | `tests/checks/c05-hd-viscosity-flow-past-cylinder-02/Dockerfile` -> `run.sh` -> native contract -> `validate.py` |
| C06 `c06-hd-sedov-01` | `Test_Problems/HD/Sedov`, `01` | 1-D Cartesian IDEAL Sedov; CHARACTERISTIC_TRACING + PARABOLIC + two-shock | `tests/checks/c06-hd-sedov-01/Dockerfile` -> `run.sh` -> native contract -> `validate.py` |
| C07 `c07-hd-jet-01` | `Test_Problems/HD/Jet`, `01` | 3-D polar IDEAL axisymmetric jet; RK2 + LINEAR + user jet boundary | `tests/checks/c07-hd-jet-01/Dockerfile` -> `run.sh` -> native contract -> `validate.py` |
| C08 `c08-hd-underexpanded-jet-01` | `Test_Problems/HD/Underexpanded_Jet`, `01` | 3-D polar IDEAL Hancock + LINEAR underexpanded shock/flattening | `tests/checks/c08-hd-underexpanded-jet-01/Dockerfile` -> `run.sh` -> native contract -> `validate.py` |
| C09 `c09-hd-underexpanded-jet-02` | `Test_Problems/HD/Underexpanded_Jet`, `02` | 3-D polar IDEAL RK3 + WENO3 underexpanded shock/flattening | `tests/checks/c09-hd-underexpanded-jet-02/Dockerfile` -> `run.sh` -> native contract -> `validate.py` |
| C10 `c10-hd-sedov-04` | `Test_Problems/HD/Sedov`, `04` | 2-D Cartesian IDEAL LimO3 Sedov; RK3 | `tests/checks/c10-hd-sedov-04/Dockerfile` -> `run.sh` -> native contract -> `validate.py` |
| C11 `c11-hd-blast-02` | `Test_Problems/HD/Blast`, `02` | 2-D Cartesian ISOTHERMAL characteristic tracing + PARABOLIC; turbulence/input interpolation hook | `tests/checks/c11-hd-blast-02/Dockerfile` -> `run.sh` -> native contract -> `validate.py`; `build/deck.py` generates and `run.sh` stages `grid0.out` + `rho0.dbl` before solver start |
| C12 `c12-hd-riemann-2d-05` | `Test_Problems/HD/Riemann_2D`, `05` | 2-D Cartesian IDEAL RK3 + MP5_FD finite-difference flux | `tests/checks/c12-hd-riemann-2d-05/Dockerfile` -> `run.sh` -> native contract -> `validate.py` |
| C13 `c13-hd-sedov-02` | `Test_Problems/HD/Sedov`, `02` | 1-D polar IDEAL Hancock + LINEAR fourth-order/geometry limiter path | `tests/checks/c13-hd-sedov-02/Dockerfile` -> `run.sh` -> native contract -> `validate.py` |
| C14 `c14-hd-sedov-03` | `Test_Problems/HD/Sedov`, `03` | 1-D spherical IDEAL RK3 + WENO3 spherical-origin limiter | `tests/checks/c14-hd-sedov-03/Dockerfile` -> `run.sh` -> native contract -> `validate.py` |
| C15 `c15-hd-stellar-wind-04` | `Test_Problems/HD/Stellar_Wind`, `04` | 2-D cylindrical IDEAL characteristic tracing + LINEAR internal boundary | `tests/checks/c15-hd-stellar-wind-04/Dockerfile` -> `run.sh` -> native contract -> `validate.py` |
| C16 `c16-hd-stellar-wind-06` | `Test_Problems/HD/Stellar_Wind`, `06` | 3-D Cartesian IDEAL RK2 + LINEAR selective entropy/internal boundary | `tests/checks/c16-hd-stellar-wind-06/Dockerfile` -> `run.sh` -> native contract -> `validate.py` |
| C17 `c17-hd-disk-planet-08-fargo` | `Test_Problems/HD/Disk_Planet`, `08` | 3-D polar ISOTHERMAL rotating potential with FARGO orbital shift and 100-step average; RK2 + LINEAR | `tests/checks/c17-hd-disk-planet-08-fargo/Dockerfile` -> `run.sh` -> native contract -> `validate.py` |
| C18 `c18-hd-viscosity-taylor-couette-05` | `Test_Problems/HD/Viscosity/Taylor_Couette`, `05` | 3-D polar IDEAL viscosity with RK_LEGENDRE; Hancock + LINEAR | `tests/checks/c18-hd-viscosity-taylor-couette-05/Dockerfile` -> `run.sh` -> native contract -> `validate.py` |
| C19 `c19-hd-viscosity-flow-past-cylinder-01` | `Test_Problems/HD/Viscosity/Flow_Past_Cylinder`, `01` | 2-D polar IDEAL viscosity with EXPLICIT parabolic update; RK2 + LINEAR | `tests/checks/c19-hd-viscosity-flow-past-cylinder-01/Dockerfile` -> `run.sh` -> native contract -> `validate.py` |
| C20 `c20-hd-wind-tunnel-02` | `Test_Problems/HD/Wind_Tunnel`, `02` | 2-D Cartesian IDEAL RK3 + WENO3 reflected/internal boundary | `tests/checks/c20-hd-wind-tunnel-02/Dockerfile` -> `run.sh` -> native contract -> `validate.py` |
| C21 `c21-hd-mach-reflection-02` | `Test_Problems/HD/Mach_Reflection`, `02` | HD explicit thermal-conduction continuation using TCfront callback | Docker -> native `dbl.out`/`data.*.dbl` -> validator |
| C22 `c22-hd-jet-02` | `Test_Problems/HD/Jet`, `02` | HD explicit thermal-conduction continuation using TCfront callback | Docker -> native `dbl.out`/`data.*.dbl` -> validator |
| C23 `c23-hd-disk-vortex-01` | `Test_Problems/HD/Disk_Vortex`, `01` | HD explicit thermal-conduction continuation using TCfront callback | Docker -> native `dbl.out`/`data.*.dbl` -> validator |
| C24 `c24-hd-stellar-wind-08` | `Test_Problems/HD/Stellar_Wind`, `08` | HD explicit thermal-conduction continuation using TCfront callback | Docker -> native `dbl.out`/`data.*.dbl` -> validator |
| C25 `c25-hd-thermal-conduction-tcfront-01` | `Test_Problems/MHD/Thermal_conduction/TCfront`, `01` | HD 1-D Cartesian explicit diffusion front | Docker -> native `dbl.out`/`data.*.dbl` -> validator |
| C26 `c26-hd-thermal-conduction-tcfront-02` | `Test_Problems/MHD/Thermal_conduction/TCfront`, `02` | HD 1-D Cartesian explicit diffusion front (STS source family) | Docker -> native -> validator |
| C27 `c27-hd-thermal-conduction-tcfront-03` | `Test_Problems/MHD/Thermal_conduction/TCfront`, `03` | HD 1-D Cartesian explicit diffusion front (RKL source family) | Docker -> native -> validator |
| C28 `c28-hd-thermal-conduction-tcfront-04` | `Test_Problems/MHD/Thermal_conduction/TCfront`, `04` | HD cylindrical thermal front | Docker -> native -> validator |
| C29 `c29-hd-thermal-conduction-tcfront-07` | `Test_Problems/MHD/Thermal_conduction/TCfront`, `07` | HD 3-D Cartesian thermal front | Docker -> native -> validator |
| C30 `c30-hd-thermal-conduction-tcfront-10` | `Test_Problems/MHD/Thermal_conduction/TCfront`, `10` | HD polar thermal front | Docker -> native -> validator |
| C31 `c31-hd-thermal-conduction-tcfront-13` | `Test_Problems/MHD/Thermal_conduction/TCfront`, `13` | HD spherical thermal front | Docker -> native -> validator |
| C32 `c32-hd-thermal-conduction-tcfront-16` | `Test_Problems/MHD/Thermal_conduction/TCfront`, `16` | HD serial cylindrical #16 thermal front (no HDF5/Chombo dependency) | Docker -> native -> validator |
| C33 `c33-hd-thermal-conduction-blast-01` | `Test_Problems/MHD/Thermal_conduction/Blast`, `01` | HD explicit thermal-conduction blast | Docker -> native -> validator |
| C34 `c34-hd-thermal-conduction-blast-01-control` | `Test_Problems/MHD/Thermal_conduction/Blast`, `01` | HD explicit thermal-conduction control | Docker -> native -> validator |
| C35 `c35-hd-thermal-conduction-sedov-01` | `Test_Problems/HD/Sedov`, `01` | HD Sedov plus explicit thermal diffusion | Docker -> native -> validator |

## Production-path and algorithm coverage

The following rows are direct runtime witnesses, not compile/import/listing
claims. Each path is compiled into the row's Docker binary and reached by its
listed official deck; each row's native `dbl.out` and `data.%04d.dbl` output is
then compared by its validator.

| Owned production path / algorithm family | Runtime witnesses |
|---|---|
| `Src/prim_eqn.c`; `Src/HD/{eigenv.c,hll_speed.c,hll.c,hllc.c,fluxes.c,mappers.c,mappers_loc.c,set_solver.c,tvdlf.c,two_shock.c,riemann_full.c,ausm.c,ausm_up.c,roe.c,rusanov-dw.c,advection_solver.c,mod_defs.h}` | All Cartesian/polar/cylindrical/spherical HD rows C01-C20 compile the HD module; HLLC shock/flux path is reached by C01-C04, C06-C20; two-shock is reached by C06; finite-difference fluxes are reached by C03 and C12. The row-specific definitions and solver decks are copied into each image and retained in results. |
| EOS `Src/EOS/PVTE/{eos.h,pvte_law.c,pvte_law_H+.c,pvte_law_dAngelo.c,pvte_law_template.c,thermal_eos.c,internal_energy.c,fundamental_derivative.c,scvh.c,zeta_tables.c}` | C01's PVTE Sod row executes the PVTE pressure/temperature conversion path. |
| EOS `Src/EOS/Ideal/{eos.h,eos.c}` | C02-C03, C05-C16, and C18-C20 execute IDEAL primitive/total-energy conversions across smooth, shocked, jet, spherical, viscosity, and boundary families. |
| EOS `Src/EOS/Isothermal/{eos.h,eos.c}` | C04, C11, and C17 execute ISOTHERMAL pressure/density conversion in spherical/potential, external-input blast, and FARGO disk rows. |
| Reconstruction `Src/States/{plm_coeffs.c,plm_states.c,flat_states.c,flatten.c,char_tracing.c,hancock.c,ppm_coeffs.c,ppm_states.c,fd_states.c,mp5_states.c,limo3_states.c,weno3_states.c}` | LINEAR/PLM: C01-C05, C07-C08, C11, C15-C20; WENO3: C02, C09, C14, C20; WENOZ_FD: C03; PARABOLIC/PPM: C06 and C11; MP5_FD: C12; LimO3: C10; HANCOCK: C08, C13, C18. Characteristic tracing: C06, C11, C15. Finite-difference state/flux path: C03 and C12. |
| Time stepping `Src/Time_Stepping/{update_stage.c,rk_step.c,rk_step_failsafe.c,ctu_step.c}` | RK2: C01, C04-C05, C07, C11, C15-C17, C19; RK3: C02-C03, C09-C10, C12, C14, C20; HANCOCK: C08, C13, C18; characteristic/CTU path: C06 and C11. The Docker build uses the same pinned setup.py source selection for every row. |
| Parabolic scheduler `Src/parabolic_update.c`, `Src/split_source.c`, `Src/sts.c`, `Src/rkl.c`, `Src/Viscosity/{viscosity.h,viscous_flux.c,viscous_rhs.c,visc_nu.c}` | Viscosity EXPLICIT: C19; SUPER_TIME_STEPPING: C05; RK_LEGENDRE: C18. Each mode is a separate official configuration and native solver run; the mode is not inferred from a filename. |
| Thermal-conduction implementation `Src/Thermal_Conduction/{tc.h,tc_flux.c,tc_functions.c,tc_kappa.c,tc_rhs.c}` | C21-C35 compile and execute the HD-compatible TCfront conductivity callback with `THERMAL_CONDUCTION=EXPLICIT`, covering 1-D/2-D/3-D Cartesian, cylindrical, polar, and spherical diffusion-front selectors plus blast/Sedov continuation rows. Native `dbl.out`/`data.*.dbl` outputs are copied only after solver exit and checked directly. |
| FARGO orbital advection `Src/Fargo/{fargo.c,fargo.h,fargo_io.c,fargo_source.c,fargo_velocity.c}` | C17 alone executes the official rotating-frame Disk_Planet FARGO consumer with `FARGO_NSTEP_AVERAGE=100`; its deck, definitions, native output, and validator are all row-owned. |
| Geometry/coordinates `Src/{cartcoord.c,rotate.c,rotate.h,ring_average.c,get_nghost.c}` and geometry branches in grid/flux/update code | Cartesian: C01-C03, C06, C10-C12, C16, C20; polar: C05, C07-C09, C13, C17-C19; cylindrical: C15; spherical: C04 and C14. |
| Grid, startup, initialization, runtime, boundaries and output `Src/{set_grid.c,initialize.c,startup.c,runtime_setup.c,set_indexes.c,parse_file.c,input_data.c,boundary.c,fluid_interface_boundary.c,check_states.c,adv_flux.c,fd_flux.c,write_data.c,write_vtk.c,write_vtk_proc.c,output_log.c,var_names.c,bin_io.c,arrays.c,tools.c,debug_tools.c,failsafe.c}` | Every row runs the same pinned setup/startup/grid/output path. C11 additionally executes `input_data.c:InputDataOpen/InputDataInterpolate` using deterministic preprocessed `grid0.out` and `rho0.dbl`; C04/C07/C08/C09/C15/C16/C17/C20 execute user/internal boundary callbacks. Native output is copied after solver exit and is the only graded trajectory. |
| Shared parallel/math/setup dependency (`Src/Parallel/*`, `Src/Math_Tools/*`, `setup.py`) | All thirty-five Docker builds compile/link the selected serial C17 codebase with `PARALLEL=FALSE`, and all thirty-five solver processes execute the resulting setup/runtime and numerical helper paths. This is transitive owned production dependency, not a separate active science row. |

### Conditional paths and exclusions

This leaf's active configuration family is classical HD. The vendored MHD,
RHD, RMHD, radiation, particles, LES, Chombo-only, and HDF5-only consumers
remain in the pinned source snapshot but are not part of the active HD module
contract. They are not placed in the reward denominator or described as
executed coverage. C21-C35 are active direct rows and do not use hidden, staged, blocked,
unsupported, or inventory-only escape statuses.

## Exact Docker gate and evidence

The exact commands used for the final gate are the no-argument entrances from
the task root (with `HARBOR_WRITABLE_DIR` outside the leaf):

```sh
HARBOR_WRITABLE_DIR=<external-run-root> \
  tasks/pluto-hd-diffusion/solution/solve.sh
HARBOR_WRITABLE_DIR=<same-external-run-root> \
  tasks/pluto-hd-diffusion/tests/test.sh
```

`solve.sh` retains one oracle directory per invocation, one lowercase image and
one lowercase named container per row, per-row `docker_build.stdout/stderr`,
`docker_run.stdout/stderr`, copied solver logs/completion, deck manifest, native
outputs, and `output_contract.json`. It never uses Docker `--rm` and never
removes containers, images, pointers, scratch roots, or logs. `test.sh` retains
the verifier image/container and report run directory and emits the non-binary
reward JSON. Because this Docker Desktop sandbox cannot expose its /tmp bind
paths, the launcher stages only the required native files into the retained
verifier container before start; C17 VTK evidence is never copied to the
verifier.

Final evidence from this continuation:

- canonical solve: `/private/tmp/pluto-hd-final-continuation.4GCGPu`;
- solve manifest: `pluto-hd-diffusion-oracle.jRwVCE/oracle_manifest.json`,
  `status=complete`, 35/35 rows, zero failed/blocked, and native
  `output_contract.json` for every row;
- fresh C21-C35 Docker containers ran to exit 0 with lowercase retained names;
  C01-C20 were imported only after strict source/config/native-parser checks;
- verifier attempts are retained under the same external root, but the exact
  no-argument Docker verifier could not start after the Docker daemon exhausted
  its storage (including the minimal-context and retained-image fallback); no
  verifier reward is claimed;
- direct synthetic validator gate passed identity/roundoff and rejected one-cell,
  wrong-step/time, wrong-mode/endian, short-payload, and malformed-metadata cases;
- remaining status: verifier Docker self-pass is **incomplete due environment
  storage exhaustion**; human science pass-policy calibration remains pending.
