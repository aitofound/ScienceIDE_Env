# Cooling + Chemistry module-coverage ledger

## Gate and evidence contract

`solution/solve.sh` and `tests/test.sh` are no-argument Docker entrances. They
build the public pinned image
`python:3.12-bookworm@sha256:581429e3df12d76e6af4be5ab7d0e7fc2013eb57dc23d2de691411c8efdbb970`
with the pinned public full-Python image's GCC/build-essential and Python 3 toolchain, use `--network none`, mount the
pinned source read-only, and create retained named Docker objects. The image
runs `tests/runner.py` with real vendored C translation units; it does not use a
Python or reimplemented numerical substitute.

The solve oracle stores the archive receipt, per-file hashes for 32 owned
`Src/Cooling/` files and 32 audited `Src/EOS/` plus `Src/Math_Tools/` dependency
files, all eight shipped Jet selector/deck files, every compile command/process
result, and the exact current contract. The verifier requires the same source,
harness digest, row mappings, check IDs, external-input bits, and explicit pass
bits before rerunning the complete executable suite.

The deterministic harness digest covers `runner.py`, `row-status.json`,
`tests/Dockerfile`, both no-argument wrappers, and every checked-in C/header
probe source. A changed probe, header, wrapper, runner, manifest, or image
contract therefore cannot reuse an older oracle. Family and constructed probes
require finite nonzero ODE/Jacobian/source behavior, perturbation sensitivity,
and positive updates. Numerical threshold and convergence calibration remain
human-owned and are not asserted here.

## Executable-check index

| Check | Direct executable evidence |
|---|---|
| `configuration-manifest` | Compiles/runs `deck_probe.c` across all eight shipped selector/deck files and checks exact family/EOS selectors, grid/deck values, and source-family paths; it does not simulate a Jet. |
| `jet-sneq-07`, `jet-mineq-08`, `jet-h2-09`, `jet-h2-pvte-18` | Each runs `deck_probe.c` for one exact Jet pair; family checks below run the matching one-zone source closure, not the full deck. |
| `family-power-law` / `power-law` | `Power_Law/cooling.c` analytic update, fractional cooling schedule, positivity, and low-temperature cutoff. (`power-law` is the final closure check id.) |
| `family-tabulated` | `cooling_source.c`, all common ODE entry points, `Tabulated/radiat.c`, `Tabulated/maxrate.c`, source update, table read, and finite positive output. |
| `family-sneq` | `cooling_source.c`, common ODE entry points, `SNEq/radiat.c`, `SNEq/maxrate.c`, equilibrium/source update, and finite output. |
| `family-mineq` | `cooling_source.c`, common ODE entry points, `MINEq/radiat.c`, `maxrate.c`, `comp_equil.c`, `ion_init.c`, `make_tables.c`, LU-backed ion/table construction, all-ion source update, and finite output. |
| `family-h2` | `cooling_source.c`, common ODE entry points, `H2_COOL/radiat.c`, `maxrate.c`, `comp_equil.c`, normalization/equilibrium, and source update. |
| `restart-tabulated`, `restart-sneq`, `restart-mineq`, `restart-h2` | Each serializes the family cooling state, reconstructs a fresh state, resumes one source step, and compares with uninterrupted split steps; this is a cooling-state restart contract, not a full hydrodynamic restart. |
| `constructed-tabulated-interior`, `constructed-tabulated-edge` | Distinct table fixtures execute an interior negative source or below-range cutoff, respectively. |
| `constructed-sneq-relaxation`, `constructed-mineq-relaxation`, `constructed-h2-equilibrium`, `constructed-h2-stiff` | Distinct compile-time cases execute off-equilibrium chemical source/equilibrium checks or the H2 adaptive/stiff witness. |
| `power-law-analytic`, `power-law-cutoff` | Distinct Power_Law fixtures execute the analytic update and low-temperature cutoff row contracts. |
| `tabulated-extract` | Compiles and runs the vendored `Tabulated/extract.c` against `cooltab_z03z3.dat`, producing a writable derived `cooltable.dat`. |
| `fallback-tabulated`, `fallback-sneq`, `fallback-h2` | Compiles/runs each vendored `jacobian.c`; expected exit 1 and exact `Jacobian not defined` output is checked, so intentional fallback is executable evidence rather than omission. |
| `math-default` | Direct calls to LU decomposition/back-substitution, inverse/multiply, Gaussian and tridiagonal solves, Brent/Ridder/quadratic/cubic/quartic roots, QR decomposition/solve, RK2/RK4/CK45 ODE, Gauss quadrature, spline coefficients, 2-D table interpolation, Bessel, sorting, vector, uniform/Gaussian/power-law random and seed helpers. |
| `math-mt` | Same helper probe with `PRNG=2`, executing the Mersenne-Twister branch of `math_random.c` plus the separately vendored `math_rand_mt19937-64.c` through a private-symbol bridge; repeatability and seed perturbation are checked. |
| `mineq-test` | Vendored standalone `MINEq/test.c` configuration/ion-schema executable. |
| `eos-ideal` | Vendored `EOS/Ideal/eos.c`: `SoundSpeed2`, `Enthalpy`, and `Entropy`. |
| `eos-isothermal` | Vendored `EOS/Isothermal/eos.c`: isothermal `SoundSpeed2` mode. Its intentional unsupported Enthalpy/Entropy functions are not falsely called as successes. |
| `eos-taub` | Vendored `EOS/Taub/eos.c` under the RHD `mod_defs.h` mode: relativistic `SoundSpeed2`, `Enthalpy`, and `Entropy`. |
| `eos-pvte-dangelo` | Vendored canonical `PVTE/pvte_law.c`, `internal_energy.c`, `thermal_eos.c`, `fundamental_derivative.c`, `zeta_tables.c`, and `mean_mol_weight.c`; executes zeta construction, non-equilibrium `GetHFracs`, internal energy, pressure, exact p-to-T and e-to-T inversions, `Gamma1`, and fundamental-derivative closure. The H2 fixture intentionally selects PVTE's exact chemistry branches rather than fabricating the chemistry-free 1200x1200 bracket table. |
| `eos-pvte-hplus` | Focused direct probe of mutually-exclusive `PVTE/pvte_law_H+.c` with `thermal_eos.c` and `mean_mol_weight.c`; executes H+ `GetMu`, internal energy, pressure, p-to-T inversion, and `Gamma1`. The generic chemistry `InternalEnergyBracket`/e-to-T hook is absent from this vendored H+ variant and is documented as an implementation boundary rather than stubbed. |
| `eos-pvte-dangelo-alt` | Vendored `PVTE/pvte_law_dAngelo.c` variant plus the common exact-chemistry PVTE closure and `mean_mol_weight.c`. |
| `eos-pvte-template` | Vendored `PVTE/pvte_law_template.c` is compiled and its intentional supplied `Gamma1` hook executes; its empty user hooks are documented as a skeleton, not fabricated physics. |
| `eos-pvte-scvh-external` | Vendored `PVTE/scvh.c` is compiled and reaches the exact `H_TAB_I.A` missing-file branch (expected exit 1, `File not found`). The SCvH table is a genuinely absent external input, not synthesized. |
| `krome-boundary` | Direct absence assertion for `Src/Cooling/KROME`; the licensed generator, networks, generated interface, and headers are not in the pinned archive and are recorded as external inputs. |

## Exhaustive vendored path map

Every path below is within the runner's hashed source manifest.  “Included”
means its exact translation unit/header/data is used by the named check;
makefiles are covered by the equivalent selector and include/link closure
because they are build metadata rather than callable code.

### Common Cooling

| Production path | Direct check(s) | Covered behavior |
|---|---|---|
| `Src/Cooling/cooling.h` | all `family-*` checks | selector constants, `NVAR_COOLING`, species indices, ODE/source prototypes |
| `Src/Cooling/cooling_source.c` | all `family-*` checks | common `CoolingSource` and `Numerical_Jacobian` |
| `Src/Cooling/cooling_ode_solver.c` | all `family-*` checks | RKF12, RKF23, RK4, CK45; ROS34 under MINEq |
| `Src/Cooling/makefile` | all `family-*` and `configuration-manifest` | selector-to-source/object closure reproduced with explicit gcc closure |

### Power-law family

| Production path | Direct check(s) | Covered behavior |
|---|---|---|
| `Src/Cooling/Power_Law/cooling.c` | `power-law` | analytic parcel integration, cutoff, `dt_cool`, positivity |
| `Src/Cooling/Power_Law/makefile` | `power-law` | selected object/source closure |

### Tabulated family

| Production path | Direct check(s) | Covered behavior |
|---|---|---|
| `Src/Cooling/Tabulated/radiat.c` | `family-tabulated` | file-backed radiative loss interpolation and RHS |
| `Src/Cooling/Tabulated/maxrate.c` | `family-tabulated` | maximum cooling-rate estimate |
| `Src/Cooling/Tabulated/jacobian.c` | `fallback-tabulated` | intentional unsupported Jacobian fallback, expected exit/message |
| `Src/Cooling/Tabulated/extract.c` | `tabulated-extract` | shipped parser and derived table generation |
| `Src/Cooling/Tabulated/cooltable.dat` | `family-tabulated`, `tabulated-extract` | runtime table input |
| `Src/Cooling/Tabulated/cooltab_z03z3.dat` | `tabulated-extract` | parser input |
| `Src/Cooling/Tabulated/makefile` | family/parser checks | selected object and data closure |

### SNEq family

| Production path | Direct check(s) | Covered behavior |
|---|---|---|
| `Src/Cooling/SNEq/radiat.c` | `family-sneq` | `MakeTable`, radiative RHS, `CompEquil` |
| `Src/Cooling/SNEq/maxrate.c` | `family-sneq` | rate estimate |
| `Src/Cooling/SNEq/jacobian.c` | `fallback-sneq` | intentional unsupported Jacobian fallback |
| `Src/Cooling/SNEq/makefile` | `family-sneq`, fallback | selector/object closure |

### MINEq family and chemistry network

| Production path | Direct check(s) | Covered behavior |
|---|---|---|
| `Src/Cooling/MINEq/radiat.c` | `family-mineq` | all-ion RHS, `Find_Rates`, normalization, mass/electron/source path |
| `Src/Cooling/MINEq/maxrate.c` | `family-mineq` | rate estimate |
| `Src/Cooling/MINEq/comp_equil.c` | `family-mineq` | equilibrium ion/electron solve |
| `Src/Cooling/MINEq/ion_init.c` | `family-mineq` | all configured H/He/C/N/O/Ne/S ion initialization through `INIT_ATOM` |
| `Src/Cooling/MINEq/make_tables.c` | `family-mineq` | LU-backed `Solve_System`, coefficient symmetrization, Lagrange interpolation, ion and loss table construction |
| `Src/Cooling/MINEq/jacobian.c` | `family-mineq` | implemented analytic chemistry Jacobian is compiled into the closure and numerical source/Jacobian path is exercised |
| `Src/Cooling/MINEq/cooling_defs.h` | `family-mineq`, `mineq-test` | ion schema, coefficients, element labels, table ranges |
| `Src/Cooling/MINEq/test.c` | `mineq-test` | standalone compile-time network configuration executable |
| `Src/Cooling/MINEq/makefile` | `family-mineq`, `mineq-test` | selector/object closure |

### H2 chemistry family

| Production path | Direct check(s) | Covered behavior |
|---|---|---|
| `Src/Cooling/H2_COOL/radiat.c` | `family-h2` | H2 rate table, thermal/chemical RHS |
| `Src/Cooling/H2_COOL/maxrate.c` | `family-h2` | rate estimate |
| `Src/Cooling/H2_COOL/comp_equil.c` | `family-h2` | equilibrium and electron relation |
| `Src/Cooling/H2_COOL/jacobian.c` | `fallback-h2` | intentional unsupported Jacobian fallback |
| `Src/Cooling/H2_COOL/pluto.ini` | `configuration-manifest`, `family-h2` | shipped H2 setup is retained and selector closure is checked |
| `Src/Cooling/H2_COOL/makefile` | `family-h2`, fallback | selector/object closure |

### Audited EOS dependency closure

| Production path | Direct check(s) | Covered behavior |
|---|---|---|
| `Src/EOS/Ideal/eos.c` | `eos-ideal` | HD ideal sound speed, enthalpy, entropy |
| `Src/EOS/Ideal/eos.h` | `eos-ideal` | HD ideal EOS declarations and constants used by the probe |
| `Src/EOS/Ideal/makefile` | `eos-ideal` | selected object/include closure reproduced by the direct compile |
| `Src/EOS/Isothermal/eos.c` | `eos-isothermal` | HD isothermal sound speed; intentional unsupported energy functions are not called |
| `Src/EOS/Isothermal/eos.h` | `eos-isothermal` | HD isothermal declarations and constants used by the probe |
| `Src/EOS/Isothermal/makefile` | `eos-isothermal` | selected no-energy object/include closure reproduced by the direct compile |
| `Src/EOS/Taub/eos.c` | `eos-taub` | RHD Taub sound speed, enthalpy, entropy |
| `Src/EOS/Taub/eos.h` | `eos-taub` | RHD Taub declarations and constants used by the probe |
| `Src/EOS/Taub/makefile` | `eos-taub` | selected RHD object/include closure reproduced by the direct compile |
| `Src/EOS/PVTE/eos.h` | all PVTE checks | PVTE prototypes, table switches, and `func_param` contract |
| `Src/EOS/PVTE/pvte_law.c` | `eos-pvte-dangelo` | canonical D'Angelo chemistry-free law, Saha fractions, molecular zeta, `GetMu`, `Gamma1` |
| `Src/EOS/PVTE/pvte_law_H+.c` | `eos-pvte-hplus` | H+ Saha law, `GetMu`, internal energy, Gamma1 |
| `Src/EOS/PVTE/pvte_law_dAngelo.c` | `eos-pvte-dangelo-alt` | alternate vendored D'Angelo law implementation |
| `Src/EOS/PVTE/pvte_law_template.c` | `eos-pvte-template` | user-supplied skeleton and default Gamma1 hook |
| `Src/EOS/PVTE/internal_energy.c` | all non-template PVTE checks | internal energy, derivative, table-backed and inversion paths |
| `Src/EOS/PVTE/thermal_eos.c` | all non-template PVTE checks | pressure, P-to-T inversion, reduced table construction |
| `Src/EOS/PVTE/fundamental_derivative.c` | non-template PVTE checks | fundamental derivative branches |
| `Src/EOS/PVTE/zeta_tables.c` | canonical/alternate PVTE checks | zeta table construction and interpolation helper |
| `Src/EOS/PVTE/scvh.c` | `eos-pvte-scvh-external` | SCvH parser reaches absent `H_TAB_I.A` input branch |
| `Src/EOS/PVTE/makefile` | all PVTE checks | mutually-exclusive law/object closure |

### Audited Math_Tools dependency closure

| Production path | Direct check(s) | Covered behavior |
|---|---|---|
| `Src/Math_Tools/math_tools.h` | `math-default`, `math-mt`, all family/PVTE links | public helper contracts |
| `Src/Math_Tools/math_lu_decomp.c` | `math-default`, family-MINEq, PVTE | LU, back-substitution, inverse, multiply, Gaussian, tridiagonal, row swap |
| `Src/Math_Tools/math_root_finders.c` | `math-default`, family probes, PVTE | Brent, Ridder, polynomial roots and table inversions |
| `Src/Math_Tools/math_qr_decomp.c` | `math-default`, family-MINEq/PVTE links | QR decomposition/solve closure |
| `Src/Math_Tools/math_misc.c` | `math-default`, family/PVTE links | Bessel, sort, vector helpers |
| `Src/Math_Tools/math_ode.c` | `math-default` | RK2/RK4/CK45 standard ODE driver |
| `Src/Math_Tools/math_quadrature.c` | `math-default` | Gauss quadrature |
| `Src/Math_Tools/math_interp.c` | `math-default`, PVTE/scvh links | spline coefficients |
| `Src/Math_Tools/math_table2D.c` | `math-default`, PVTE/scvh links | initialization/interpolation/finalization and binary-table closure |
| `Src/Math_Tools/math_random.c` | `math-default`, `math-mt` | default and MT-selected random APIs |
| `Src/Math_Tools/math_rand_mt19937-64.c` | `math-mt` | MT state, integer, and real generator |
| `Src/Math_Tools/makefile` | `math-default`, `math-mt` | helper object closure |

## Row completion

All 16 PR-304 identifiers are present in `tests/row-status.json` with executable
statuses and exact check mappings. Fifteen rows are production-execution rows;
the KROME row is an explicit executable external-input assertion and is excluded
from that denominator by the runner's `production_execution` and summary fields.
Jet rows are selector/deck plus one-zone source contracts, not full Jet
simulations. The restart row is a serialized cooling-state source-split
contract, not a full hydrodynamic restart. They map as follows:

| Row | Final status | Direct check mapping |
|---|---|---|
| `sneq-mhd-jet-07` | passed | `jet-sneq-07`, `family-sneq` |
| `mineq-mhd-jet-08` | passed | `jet-mineq-08`, `family-mineq` |
| `h2-mhd-jet-09` | passed | `jet-h2-09`, `family-h2` |
| `h2-hd-pvte-jet-18` | passed | `jet-h2-pvte-18`, `family-h2`, `eos-pvte-hplus` |
| `power-law-analytic-parcel` | passed | `power-law-analytic` |
| `power-law-cutoff-crossing` | passed | `power-law-cutoff` |
| `tabulated-interior-parcel` | passed | `constructed-tabulated-interior` |
| `tabulated-edge-cutoff` | passed | `constructed-tabulated-edge`, `tabulated-extract` |
| `sneq-equilibrium-relaxation` | passed | `constructed-sneq-relaxation`, `family-sneq` |
| `mineq-equilibrium-relaxation` | passed | `constructed-mineq-relaxation`, `family-mineq` |
| `h2-equilibrium-relaxation` | passed | `constructed-h2-equilibrium`, `family-h2` |
| `h2-stiff-transition` | passed | `constructed-h2-stiff`, `family-h2` |
| `cooling-restart-source-split` | passed | `restart-tabulated`, `restart-sneq`, `restart-mineq`, `restart-h2` |
| `cooling-object-closure` | passed | `configuration-manifest`, all four `family-*`, `math-default`, `eos-ideal` |
| `cooling-table-provenance-closure` | passed | `tabulated-extract`, `family-mineq`, `eos-pvte-dangelo` |
| `krome-boundary-closure` | passed-external-input | `krome-boundary`; absent licensed generator is explicit external input |

No local generated binary is a deliverable: the existing executable
`tests/probes/power_probe` remains retained local evidence and is excluded from
the deliverable list. Named Docker containers/images and all solve/test logs and
JSON receipts remain retained under their task-local artifact directories.
