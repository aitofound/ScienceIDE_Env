# SfePy: complete module proposal

This plan covers the unchanged `release_2026.2` source, commit `3f01a19fad86d14c1d54706372fe591f8f7bf46c`, introduced in [source PR #608](https://github.com/aitofound/ScienceAccelBench/pull/608). Owner: @ElegantLin. It proposes **19 module entries**, including **one deferred candidate** (flexoelectricity). A planning entry is not an approved task or a guarantee of sufficient checks.

## Scope and approval

The curator accepted the original static-linear-elasticity cut with “lets merge” on 2026-09-09 at 05:52:50 UTC, at source head `a57fbffb596376e3715d645d3f8771bb2801bb77`. The CLI approval block now quotes that decision rather than the contributor's earlier words. **That decision does not approve the expanded static boundary or any of the 18 new module entries.** All additions and regroupings below are proposals for a new review.

The earlier unassigned `multi_point_constraints.py` is now explicitly assigned to `trusses-springs`: it uses directional/rotational spring terms to couple solid discs. Shared linear-elasticity and diffusion operators are listed once because several physical problems use them. Static elasticity adds the official prestress and optional IGA configurations as proposed coverage; IGA is a discretization of the same equations, not a separate physics task.

The codebase is retagged to `materials-engineering` (`physics.app-ph`, with `physics.comp-ph` secondary), following #608 review item 7. Future tasks must select their own scientific tags; the quantum module, for example, belongs under `quant-ph`. A direct-solver backend must be fixed when building task images, as requested in review item 6.

## How the cut was made

Each module groups a physical forward problem or an independently meaningful scale/spectral computation. Mesh dimension, boundary conditions, CG/DG/IGA, and equivalent formulations are variants within a module. Shared discretization, sparse solving, material kernels and dependencies do not force modules to merge. Some boundaries are deliberately provisional: porous rigid/Biot/finite-strain/cell-corrector regimes and shell/membrane mechanics may need further splitting after complete test investigation.

All **137 Python example files** are mapped in the inventory below. This includes configuration files, interactive drivers, helpers and package initializers; **file counts are not check counts**. All 940 source files reconcile into 149 module-owned files, 322 shared runtime/test files, and 469 retained resources/build/docs/demo files. There is no duplicate file ownership. The CLI calls the last group “unclassified” because it is outside module/shared ownership, but every such path has a disposition in the canonical JSON.

## Module table

“Scenario” means a source configuration/driver file after excluding known helpers, not a suitable independent check. “Collected” counts selected items in the unchanged 219-item pytest collection; additional official drivers need not be collected by pytest. “Native” counts measured pytest examples plus the separately labelled standalone driver; unit tests are described on the cards.

| Module | Scientific responsibility | Owned files / physical lines | Scenario files | Selected collected items | Native example evidence | Review status |
|---|---|---:|---:|---:|---|---|
| `static-linear-elasticity` | Static small-strain linear elasticity | 22 / 3287 | 20 | 19 | 12 pytest | proposed boundary; review required |
| `linear-elastodynamics` | Transient linear elastodynamics | 5 / 2092 | 4 | 6 | 1 pytest | proposed boundary; review required |
| `linear-viscoelasticity` | Hereditary linear viscoelasticity | 1 / 334 | 1 | 1 | 1 pytest | limited example inventory; investigate before task approval |
| `elastic-spectra-band-gaps` | Elastic eigenmodes, dispersion and band gaps | 9 / 4188 | 6 | 1 | 1 pytest | proposed boundary; review required |
| `elastic-contact` | Elastic contact constraints | 5 / 1287 | 3 | 3 | 1 pytest | limited example inventory; investigate before task approval |
| `shells-membranes` | Shell bending and hyperelastic membranes | 7 / 2009 | 3 | 2 | 1 pytest | limited example inventory; investigate before task approval |
| `trusses-springs` | Truss and spring structures | 3 / 395 | 3 | 2 | 1 pytest | limited example inventory; investigate before task approval |
| `nonlinear-solid-mechanics` | Nonlinear constitutive and finite-strain solids | 10 / 2103 | 9 | 3 | 1 pytest | proposed boundary; review required |
| `elliptic-diffusion` | Stationary scalar diffusion and Poisson problems | 25 / 4171 | 21 | 20 | 2 pytest | proposed boundary; review required |
| `transient-scalar-transport` | Transient heat, advection and scalar transport | 10 / 1809 | 10 | 6 | 2 pytest | proposed boundary; review required |
| `incompressible-flow` | Incompressible Stokes and Navier-Stokes flow | 10 / 2913 | 6 | 5 | 2 pytest | proposed boundary; review required |
| `acoustic-helmholtz` | Acoustic and Helmholtz wave response | 4 / 479 | 4 | 4 | 1 pytest | proposed boundary; review required |
| `single-particle-quantum` | Single-particle Schrodinger eigenproblems | 5 / 267 | 4 | 4 | 1 pytest | proposed boundary; review required |
| `porous-media-flow` | Porous flow and poromechanical coupling | 9 / 2395 | 8 | 7 | 1 pytest | proposed boundary; review required |
| `piezoelectricity` | Piezoelectric electromechanics | 7 / 1406 | 4 | 2 | 1 pytest | proposed boundary; review required |
| `thermoelasticity` | Thermal-expansion elasticity | 2 / 252 | 2 | 2 | 1 pytest | limited example inventory; investigate before task approval |
| `joule-heating` | Electric conduction and Joule heating | 4 / 229 | 1 | 0 | 0 pytest + 1 standalone | limited example inventory; investigate before task approval |
| `elastic-homogenization` | Elastic micro-macro homogenization | 10 / 1662 | 7 | 1 | 1 pytest | proposed boundary; review required |
| `flexoelectricity` | Strain-gradient flexoelectric coupling | 1 / 203 | 0 | 0 | 0 pytest | deferred: no dedicated official case |

## Native evidence and its limits

Full unchanged suite: 50 pytest files, 149 AST-counted definitions, 219 collected items. Cumulative native investigation: 31 distinct official pytest examples + 11 unit-test items = 42 passed, zero failed; one additional Joule-heating standalone driver completed. Initial build succeeded in 102.38 s including dependencies/compilation. Same Linux x86_64 environment: Python 3.14.3, GCC 13.3.0, NumPy 2.5.3, SciPy 1.18.1; OMP/OpenBLAS threads one, Agg plotting. No complete 219-item run. Per-case times include startup for example processes and pytest fixture allocations for unit XML; these units should not be compared as pure kernel timings.

Initial source investigation: 6 unit-test items and 12 declarative examples passed. This expansion added 19 declarative/DG example items and 5 unit-test items, all passing. The unchanged Joule-heating driver completed a stationary electric solve and 11 heat steps in 0.87 s, writing VTK files; it has no independent reference assertion in this run. No measured case exceeded the three-minute investigation limit. The build was reused from the same pinned source and environment.

Upstream small-strain formulation agreement passes its absolute vector-norm check of `1e-8`; the hyperelastic TL/UL/mixed regression passes its implemented absolute vector-norm check of `1e-3`. These are upstream assertions, not proposed task tolerances. The Laplace square test checks an analytic solution and boundary fluxes. Homogenization engine unit tests check dependency/chunk handling, not the physical correctness of all homogenized coefficients. Many declarative examples only assert convergence. The one-host repeatability result for the default elastic solve is unchanged: 1,062 displacement values byte-identical between two runs, not calibration evidence.

No optional-backend coverage, complete suite execution, finalized physical equivalence rules or acceleration measurements are claimed. The subsequent official-test survey must examine all scenario stages and relevant shared tests; it must not turn helpers, duplicate interfaces, or output arrays into independent checks.

## static-linear-elasticity

Static continuum force balance across 2D/3D/axisymmetry, element types, boundary conditions and prescribed prestress. IGA is an optional discretization of the same physics, not a separate task.

**Inputs:** mesh, stiffness and prescribed displacements/tractions; linear constraints or prescribed prestress/fibre strain.

**Outputs:** displacement, strain, stress; mixed pressure where used.

**Stages:** constitutive operator → element quadrature and assembly → constrained static solve → physical postprocessing.

**Expensive path:** LinearElasticTerm and dw_lin_elastic constitutive contractions, element integration and sparse constrained solve.

**Production entry points and logical kernel responsibility:**

- `sfepy.applications.solve_pde`
- `sfepy/terms/terms_elastic.py:LinearElasticTerm`
- `sfepy/terms/extmods/terms_elastic.c:dw_lin_elastic`

**Owned source files:**

- [`sfepy/tests/test_elasticity_small_strain.py`](../../code/sfepy/sfepy/tests/test_elasticity_small_strain.py)
- [`sfepy/tests/test_matcoefs.py`](../../code/sfepy/sfepy/tests/test_matcoefs.py)
- [`sfepy/examples/linear_elasticity/linear_elastic.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic.py)
- [`sfepy/examples/linear_elasticity/linear_elastic_interactive.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_interactive.py)
- [`sfepy/examples/linear_elasticity/linear_elastic_probes.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_probes.py)
- [`sfepy/examples/linear_elasticity/linear_elastic_tractions.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_tractions.py)
- [`sfepy/examples/linear_elasticity/linear_elastic_up.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_up.py)
- [`sfepy/examples/linear_elasticity/elastic2D_axisymmetric.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastic2D_axisymmetric.py)
- [`sfepy/examples/linear_elasticity/elastic_shifted_periodic.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastic_shifted_periodic.py)
- [`sfepy/examples/linear_elasticity/its2D_1.py`](../../code/sfepy/sfepy/examples/linear_elasticity/its2D_1.py)
- [`sfepy/examples/linear_elasticity/its2D_2.py`](../../code/sfepy/sfepy/examples/linear_elasticity/its2D_2.py)
- [`sfepy/examples/linear_elasticity/its2D_3.py`](../../code/sfepy/sfepy/examples/linear_elasticity/its2D_3.py)
- [`sfepy/examples/linear_elasticity/its2D_4.py`](../../code/sfepy/sfepy/examples/linear_elasticity/its2D_4.py)
- [`sfepy/examples/linear_elasticity/its2D_5.py`](../../code/sfepy/sfepy/examples/linear_elasticity/its2D_5.py)
- [`sfepy/examples/linear_elasticity/its2D_interactive.py`](../../code/sfepy/sfepy/examples/linear_elasticity/its2D_interactive.py)
- [`sfepy/examples/linear_elasticity/mixed_mesh.py`](../../code/sfepy/sfepy/examples/linear_elasticity/mixed_mesh.py)
- [`sfepy/examples/linear_elasticity/wedge_mesh.py`](../../code/sfepy/sfepy/examples/linear_elasticity/wedge_mesh.py)
- [`sfepy/examples/linear_elasticity/multi_node_lcbcs.py`](../../code/sfepy/sfepy/examples/linear_elasticity/multi_node_lcbcs.py)
- [`sfepy/examples/linear_elasticity/nodal_lcbcs.py`](../../code/sfepy/sfepy/examples/linear_elasticity/nodal_lcbcs.py)
- [`sfepy/examples/linear_elasticity/rigid_twist.py`](../../code/sfepy/sfepy/examples/linear_elasticity/rigid_twist.py)
- [`sfepy/examples/linear_elasticity/linear_elastic_iga.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_iga.py)
- [`sfepy/examples/linear_elasticity/prestress_fibres.py`](../../code/sfepy/sfepy/examples/linear_elasticity/prestress_fibres.py)

**Shared runtime:** `shared-runtime-and-tests`. Direct cross-module configuration dependencies: none identified by direct Python import inspection.

**Official example/driver inventory:**

| Source | Role | Native execution |
|---|---|---|
| [`sfepy/examples/linear_elasticity/linear_elastic.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic.py) | scenario-or-driver | measured |
| [`sfepy/examples/linear_elasticity/linear_elastic_interactive.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_interactive.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/linear_elasticity/linear_elastic_probes.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_probes.py) | scenario-or-driver | measured |
| [`sfepy/examples/linear_elasticity/linear_elastic_tractions.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_tractions.py) | scenario-or-driver | measured |
| [`sfepy/examples/linear_elasticity/linear_elastic_up.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_up.py) | scenario-or-driver | measured |
| [`sfepy/examples/linear_elasticity/elastic2D_axisymmetric.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastic2D_axisymmetric.py) | scenario-or-driver | measured |
| [`sfepy/examples/linear_elasticity/elastic_shifted_periodic.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastic_shifted_periodic.py) | scenario-or-driver | measured |
| [`sfepy/examples/linear_elasticity/its2D_1.py`](../../code/sfepy/sfepy/examples/linear_elasticity/its2D_1.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/linear_elasticity/its2D_2.py`](../../code/sfepy/sfepy/examples/linear_elasticity/its2D_2.py) | scenario-or-driver | measured |
| [`sfepy/examples/linear_elasticity/its2D_3.py`](../../code/sfepy/sfepy/examples/linear_elasticity/its2D_3.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/linear_elasticity/its2D_4.py`](../../code/sfepy/sfepy/examples/linear_elasticity/its2D_4.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/linear_elasticity/its2D_5.py`](../../code/sfepy/sfepy/examples/linear_elasticity/its2D_5.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/linear_elasticity/its2D_interactive.py`](../../code/sfepy/sfepy/examples/linear_elasticity/its2D_interactive.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/linear_elasticity/mixed_mesh.py`](../../code/sfepy/sfepy/examples/linear_elasticity/mixed_mesh.py) | scenario-or-driver | measured |
| [`sfepy/examples/linear_elasticity/wedge_mesh.py`](../../code/sfepy/sfepy/examples/linear_elasticity/wedge_mesh.py) | scenario-or-driver | measured |
| [`sfepy/examples/linear_elasticity/multi_node_lcbcs.py`](../../code/sfepy/sfepy/examples/linear_elasticity/multi_node_lcbcs.py) | scenario-or-driver | measured |
| [`sfepy/examples/linear_elasticity/nodal_lcbcs.py`](../../code/sfepy/sfepy/examples/linear_elasticity/nodal_lcbcs.py) | scenario-or-driver | measured |
| [`sfepy/examples/linear_elasticity/rigid_twist.py`](../../code/sfepy/sfepy/examples/linear_elasticity/rigid_twist.py) | scenario-or-driver | measured |
| [`sfepy/examples/linear_elasticity/linear_elastic_iga.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_iga.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/linear_elasticity/prestress_fibres.py`](../../code/sfepy/sfepy/examples/linear_elasticity/prestress_fibres.py) | scenario-or-driver | unmeasured |

**Selected pytest items:**

- `test_declarative_examples.py::test_examples[linear_elasticity/elastic2D_axisymmetric.py]`
- `test_declarative_examples.py::test_examples[linear_elasticity/elastic_shifted_periodic.py]`
- `test_declarative_examples.py::test_examples[linear_elasticity/its2D_2.py]`
- `test_declarative_examples.py::test_examples[linear_elasticity/linear_elastic.py]`
- `test_declarative_examples.py::test_examples[linear_elasticity/linear_elastic_probes.py]`
- `test_declarative_examples.py::test_examples[linear_elasticity/linear_elastic_tractions.py]`
- `test_declarative_examples.py::test_examples[linear_elasticity/linear_elastic_up.py]`
- `test_declarative_examples.py::test_examples[linear_elasticity/mixed_mesh.py]`
- `test_declarative_examples.py::test_examples[linear_elasticity/multi_node_lcbcs.py]`
- `test_declarative_examples.py::test_examples[linear_elasticity/nodal_lcbcs.py]`
- `test_declarative_examples.py::test_examples[linear_elasticity/prestress_fibres.py]`
- `test_declarative_examples.py::test_examples[linear_elasticity/rigid_twist.py]`
- `test_declarative_examples.py::test_examples[linear_elasticity/wedge_mesh.py]`
- `test_elasticity_small_strain.py::test_converged`
- `test_elasticity_small_strain.py::test_linear_terms`
- `test_matcoefs.py::test_elastic_constants`
- `test_matcoefs.py::test_conversion_functions`
- `test_matcoefs.py::test_stiffness_tensors`
- `test_matcoefs.py::test_wave_speeds`

**Additional shared-test reading targets:**

- [`sfepy/tests/test_conditions.py`](../../code/sfepy/sfepy/tests/test_conditions.py)
- [`sfepy/tests/test_high_level.py`](../../code/sfepy/sfepy/tests/test_high_level.py)
- [`sfepy/tests/test_lcbcs.py`](../../code/sfepy/sfepy/tests/test_lcbcs.py)
- [`sfepy/tests/test_term_consistency.py`](../../code/sfepy/sfepy/tests/test_term_consistency.py)
- [`sfepy/tests/test_term_sensitivity.py`](../../code/sfepy/sfepy/tests/test_term_sensitivity.py)

**Measured execution:**

- [`sfepy/examples/linear_elasticity/linear_elastic.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic.py): passed, 1.266 s process wall time; official-pytest-example.
- [`sfepy/examples/linear_elasticity/linear_elastic_probes.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_probes.py): passed, 2.82 s process wall time; official-pytest-example.
- [`sfepy/examples/linear_elasticity/linear_elastic_tractions.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_tractions.py): passed, 1.366 s process wall time; official-pytest-example.
- [`sfepy/examples/linear_elasticity/linear_elastic_up.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_up.py): passed, 1.216 s process wall time; official-pytest-example.
- [`sfepy/examples/linear_elasticity/elastic2D_axisymmetric.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastic2D_axisymmetric.py): passed, 1.216 s process wall time; official-pytest-example.
- [`sfepy/examples/linear_elasticity/elastic_shifted_periodic.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastic_shifted_periodic.py): passed, 1.166 s process wall time; official-pytest-example.
- [`sfepy/examples/linear_elasticity/its2D_2.py`](../../code/sfepy/sfepy/examples/linear_elasticity/its2D_2.py): passed, 1.116 s process wall time; official-pytest-example.
- [`sfepy/examples/linear_elasticity/mixed_mesh.py`](../../code/sfepy/sfepy/examples/linear_elasticity/mixed_mesh.py): passed, 1.167 s process wall time; official-pytest-example.
- [`sfepy/examples/linear_elasticity/wedge_mesh.py`](../../code/sfepy/sfepy/examples/linear_elasticity/wedge_mesh.py): passed, 1.267 s process wall time; official-pytest-example.
- [`sfepy/examples/linear_elasticity/multi_node_lcbcs.py`](../../code/sfepy/sfepy/examples/linear_elasticity/multi_node_lcbcs.py): passed, 1.116 s process wall time; official-pytest-example.
- [`sfepy/examples/linear_elasticity/nodal_lcbcs.py`](../../code/sfepy/sfepy/examples/linear_elasticity/nodal_lcbcs.py): passed, 1.317 s process wall time; official-pytest-example.
- [`sfepy/examples/linear_elasticity/rigid_twist.py`](../../code/sfepy/sfepy/examples/linear_elasticity/rigid_twist.py): passed, 1.166 s process wall time; official-pytest-example.
- [`sfepy/tests/test_elasticity_small_strain.py`](../../code/sfepy/sfepy/tests/test_elasticity_small_strain.py): test_converged passed (4.044 s pytest case), test_linear_terms passed (0.001 s pytest case).
- [`sfepy/tests/test_matcoefs.py`](../../code/sfepy/sfepy/tests/test_matcoefs.py): test_elastic_constants passed (0.018 s pytest case), test_conversion_functions passed (0.001 s pytest case), test_stiffness_tensors passed (0.001 s pytest case), test_wave_speeds passed (0.0 s pytest case).

**Gaps and proposed decisions:**

- Near incompressibility and point loads affect conditioning; compare mesh identity, not DOF storage order.
- IGA requires igakit and was not measured. The expanded example coverage and shared-kernel boundary are proposed revisions to the initially authorized direction.
- The complete Step-2 official-test survey has not been authored; shared-suite targets listed here are reading candidates, not asserted physical coverage.
- All unexecuted scenario files: linear_elasticity/linear_elastic_interactive.py, linear_elasticity/its2D_1.py, linear_elasticity/its2D_3.py, linear_elasticity/its2D_4.py, linear_elasticity/its2D_5.py, linear_elasticity/its2D_interactive.py, linear_elasticity/linear_elastic_iga.py, linear_elasticity/prestress_fibres.py

## linear-elastodynamics

Time evolution adds inertia or damping to elasticity. Material-identification example is mapped here as a driver of this forward model, not split into an optimizer-only task.

**Inputs:** density, stiffness, damping, initial displacement/velocity, impact or base motion.

**Outputs:** time-resolved displacement, velocity, acceleration and physical energy.

**Stages:** mass and stiffness assembly → time integration → boundary forcing → history output.

**Expensive path:** Mass/stiffness operations and repeated implicit or explicit time steps in ts_solvers.py and ts_controllers.py.

**Production entry points and logical kernel responsibility:**

- `sfepy/examples/linear_elasticity/elastodynamic.py:define`
- `sfepy/solvers/ts_solvers.py`
- `sfepy/terms/terms_mass.py`

**Owned source files:**

- [`sfepy/tests/test_ed_solvers.py`](../../code/sfepy/sfepy/tests/test_ed_solvers.py)
- [`sfepy/examples/linear_elasticity/elastodynamic.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastodynamic.py)
- [`sfepy/examples/linear_elasticity/elastodynamic_identification.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastodynamic_identification.py)
- [`sfepy/examples/linear_elasticity/seismic_load.py`](../../code/sfepy/sfepy/examples/linear_elasticity/seismic_load.py)
- [`sfepy/examples/linear_elasticity/linear_elastic_damping.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_damping.py)

**Shared runtime:** `shared-runtime-and-tests`. Direct cross-module configuration dependencies: `static-linear-elasticity`.

**Official example/driver inventory:**

| Source | Role | Native execution |
|---|---|---|
| [`sfepy/examples/linear_elasticity/elastodynamic.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastodynamic.py) | scenario-or-driver | measured |
| [`sfepy/examples/linear_elasticity/elastodynamic_identification.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastodynamic_identification.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/linear_elasticity/seismic_load.py`](../../code/sfepy/sfepy/examples/linear_elasticity/seismic_load.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/linear_elasticity/linear_elastic_damping.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_damping.py) | scenario-or-driver | unmeasured |

**Selected pytest items:**

- `test_declarative_examples.py::test_examples[linear_elasticity/elastodynamic.py]`
- `test_declarative_examples.py::test_examples[linear_elasticity/linear_elastic_damping.py]`
- `test_declarative_examples.py::test_examples[linear_elasticity/seismic_load.py]`
- `test_ed_solvers.py::test_ed_solvers`
- `test_ed_solvers.py::test_rmm_solver`
- `test_ed_solvers.py::test_active_only`

**Additional shared-test reading targets:**

- [`sfepy/tests/test_term_call_modes.py`](../../code/sfepy/sfepy/tests/test_term_call_modes.py)

**Measured execution:**

- [`sfepy/examples/linear_elasticity/elastodynamic.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastodynamic.py): passed, 1.769 s process wall time; official-pytest-example.

**Gaps and proposed decisions:**

- Time-step adaptivity and solver iterations are bookkeeping, not graded observables.
- Only default elastodynamic example measured; identification loop and all solver variants remain unmeasured.
- The complete Step-2 official-test survey has not been authored; shared-suite targets listed here are reading candidates, not asserted physical coverage.
- All unexecuted scenario files: linear_elasticity/elastodynamic_identification.py, linear_elasticity/seismic_load.py, linear_elasticity/linear_elastic_damping.py

## linear-viscoelasticity

Material memory is not ordinary viscous damping. Tabulated and exponential-history implementations are variants within one hereditary-material module.

**Inputs:** elastic stiffness, fading-memory kernel and load history.

**Outputs:** displacement/stress relaxation history.

**Stages:** initialize unloaded history → evaluate history convolution → solve equilibrium at each time → record creep/relaxation.

**Expensive path:** History convolution and recurrent stress evaluation in LinearElasticTHTerm/ETHTerm and CauchyStressTH/ETH terms.

**Production entry points and logical kernel responsibility:**

- `sfepy/terms/terms_elastic.py:LinearElasticTHTerm`
- `sfepy/terms/terms_elastic.py:LinearElasticETHTerm`
- `sfepy/homogenization/convolutions.py`

**Owned source files:**

- [`sfepy/examples/linear_elasticity/linear_viscoelastic.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_viscoelastic.py)

**Shared runtime:** `shared-runtime-and-tests`. Direct cross-module configuration dependencies: none identified by direct Python import inspection.

**Official example/driver inventory:**

| Source | Role | Native execution |
|---|---|---|
| [`sfepy/examples/linear_elasticity/linear_viscoelastic.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_viscoelastic.py) | scenario-or-driver | measured |

**Selected pytest items:**

- `test_declarative_examples.py::test_examples[linear_elasticity/linear_viscoelastic.py]`

**Additional shared-test reading targets:**

- [`sfepy/tests/test_term_call_modes.py`](../../code/sfepy/sfepy/tests/test_term_call_modes.py)

**Measured execution:**

- [`sfepy/examples/linear_elasticity/linear_viscoelastic.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_viscoelastic.py): passed, 2.218 s process wall time; official-pytest-example.

**Gaps and proposed decisions:**

- Only one shipped scenario file found; th/eth configurations require separate investigation, not inflated check counts.
- Unloaded initial state and history truncation matter; fewer than four suitable official checks would require explicit THIN approval.
- The complete Step-2 official-test survey has not been authored; shared-suite targets listed here are reading candidates, not asserted physical coverage.
- All unexecuted scenario files: 

## elastic-spectra-band-gaps

Elastic vibration spectra are a distinct output contract from transient trajectories. Homogenized phononic gaps and finite-cell dispersion are spectral variants; scalar refine_evp modes are shared diagnostics, not quantum physics.

**Inputs:** elasticity/mass matrices, periodic microstructure and wave vectors/frequency window.

**Outputs:** eigenvalues/frequencies, dispersion curves, effective mass and band-gap intervals.

**Stages:** assemble spectral operators → solve generalized eigenproblem → sweep wave vector or frequency → detect gaps.

**Expensive path:** Repeated generalized eigensolves and frequency-dependent effective-mass contractions in band_gaps_app.py and coefs_phononic.py.

**Production entry points and logical kernel responsibility:**

- `sfepy/examples/linear_elasticity/dispersion_analysis.py`
- `sfepy/homogenization/band_gaps_app.py`
- `sfepy/homogenization/coefs_phononic.py`

**Owned source files:**

- [`sfepy/homogenization/band_gaps_app.py`](../../code/sfepy/sfepy/homogenization/band_gaps_app.py)
- [`sfepy/homogenization/coefs_phononic.py`](../../code/sfepy/sfepy/homogenization/coefs_phononic.py)
- [`sfepy/examples/linear_elasticity/modal_analysis.py`](../../code/sfepy/sfepy/examples/linear_elasticity/modal_analysis.py)
- [`sfepy/examples/linear_elasticity/modal_analysis_declarative.py`](../../code/sfepy/sfepy/examples/linear_elasticity/modal_analysis_declarative.py)
- [`sfepy/examples/linear_elasticity/dispersion_analysis.py`](../../code/sfepy/sfepy/examples/linear_elasticity/dispersion_analysis.py)
- [`sfepy/examples/phononic/band_gaps.py`](../../code/sfepy/sfepy/examples/phononic/band_gaps.py)
- [`sfepy/examples/phononic/band_gaps_rigid.py`](../../code/sfepy/sfepy/examples/phononic/band_gaps_rigid.py)
- [`sfepy/examples/phononic/band_gaps_conf.py`](../../code/sfepy/sfepy/examples/phononic/band_gaps_conf.py)
- [`sfepy/examples/miscellaneous/refine_evp.py`](../../code/sfepy/sfepy/examples/miscellaneous/refine_evp.py)

**Shared runtime:** `shared-runtime-and-tests`. Direct cross-module configuration dependencies: none identified by direct Python import inspection.

**Official example/driver inventory:**

| Source | Role | Native execution |
|---|---|---|
| [`sfepy/examples/linear_elasticity/modal_analysis.py`](../../code/sfepy/sfepy/examples/linear_elasticity/modal_analysis.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/linear_elasticity/modal_analysis_declarative.py`](../../code/sfepy/sfepy/examples/linear_elasticity/modal_analysis_declarative.py) | scenario-or-driver | measured |
| [`sfepy/examples/linear_elasticity/dispersion_analysis.py`](../../code/sfepy/sfepy/examples/linear_elasticity/dispersion_analysis.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/phononic/band_gaps.py`](../../code/sfepy/sfepy/examples/phononic/band_gaps.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/phononic/band_gaps_rigid.py`](../../code/sfepy/sfepy/examples/phononic/band_gaps_rigid.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/phononic/band_gaps_conf.py`](../../code/sfepy/sfepy/examples/phononic/band_gaps_conf.py) | support | unmeasured |
| [`sfepy/examples/miscellaneous/refine_evp.py`](../../code/sfepy/sfepy/examples/miscellaneous/refine_evp.py) | scenario-or-driver | unmeasured |

**Selected pytest items:**

- `test_declarative_examples.py::test_examples[linear_elasticity/modal_analysis_declarative.py]`

**Additional shared-test reading targets:**

- [`sfepy/tests/test_eigenvalue_solvers.py`](../../code/sfepy/sfepy/tests/test_eigenvalue_solvers.py)
- [`sfepy/tests/test_homogenization_engine.py`](../../code/sfepy/sfepy/tests/test_homogenization_engine.py)

**Measured execution:**

- [`sfepy/examples/linear_elasticity/modal_analysis_declarative.py`](../../code/sfepy/sfepy/examples/linear_elasticity/modal_analysis_declarative.py): passed, 2.219 s process wall time; official-pytest-example.

**Gaps and proposed decisions:**

- Eigenvector sign/phase and bases of degenerate eigenspaces are not unique; compare spectra or subspace quantities.
- Only modal_analysis_declarative executed. Phononic sweeps, PRIMME defaults and refine_evp variants are unmeasured; count band_gaps_conf as support.
- The complete Step-2 official-test survey has not been authored; shared-suite targets listed here are reading candidates, not asserted physical coverage.
- All unexecuted scenario files: linear_elasticity/modal_analysis.py, linear_elasticity/dispersion_analysis.py, phononic/band_gaps.py, phononic/band_gaps_rigid.py, miscellaneous/refine_evp.py

## elastic-contact

Unilateral contact creates an active-set problem that ordinary static linear elasticity does not cover. Plane/sphere and two-body contact stay together.

**Inputs:** elastic bodies, obstacle geometry, gap and penalty/barrier parameters.

**Outputs:** deformation, contact gap and force/stress fields.

**Stages:** detect contact geometry → evaluate contact residual/tangent → nonlinear equilibrium iterations.

**Expensive path:** Contact search and contact residual/tangent assembly in ContactTerm/ContactIPCTerm and ContactPlaneTerm/ContactSphereTerm.

**Production entry points and logical kernel responsibility:**

- `sfepy/terms/terms_contact.py`
- `sfepy/terms/terms_surface.py:ContactPlaneTerm`
- `sfepy/mechanics/contact_bodies.py`

**Owned source files:**

- [`sfepy/terms/terms_contact.py`](../../code/sfepy/sfepy/terms/terms_contact.py)
- [`sfepy/mechanics/contact_bodies.py`](../../code/sfepy/sfepy/mechanics/contact_bodies.py)
- [`sfepy/examples/linear_elasticity/elastic_contact_planes.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastic_contact_planes.py)
- [`sfepy/examples/linear_elasticity/elastic_contact_sphere.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastic_contact_sphere.py)
- [`sfepy/examples/linear_elasticity/two_bodies_contact.py`](../../code/sfepy/sfepy/examples/linear_elasticity/two_bodies_contact.py)

**Shared runtime:** `shared-runtime-and-tests`. Direct cross-module configuration dependencies: none identified by direct Python import inspection.

**Official example/driver inventory:**

| Source | Role | Native execution |
|---|---|---|
| [`sfepy/examples/linear_elasticity/elastic_contact_planes.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastic_contact_planes.py) | scenario-or-driver | measured |
| [`sfepy/examples/linear_elasticity/elastic_contact_sphere.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastic_contact_sphere.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/linear_elasticity/two_bodies_contact.py`](../../code/sfepy/sfepy/examples/linear_elasticity/two_bodies_contact.py) | scenario-or-driver | unmeasured |

**Selected pytest items:**

- `test_declarative_examples.py::test_examples[linear_elasticity/elastic_contact_planes.py]`
- `test_declarative_examples.py::test_examples[linear_elasticity/elastic_contact_sphere.py]`
- `test_declarative_examples.py::test_examples[linear_elasticity/two_bodies_contact.py]`

**Additional shared-test reading targets:**

- [`sfepy/tests/test_semismooth_newton.py`](../../code/sfepy/sfepy/tests/test_semismooth_newton.py)
- [`sfepy/tests/test_term_call_modes.py`](../../code/sfepy/sfepy/tests/test_term_call_modes.py)

**Measured execution:**

- [`sfepy/examples/linear_elasticity/elastic_contact_planes.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastic_contact_planes.py): passed, 3.872 s process wall time; official-pytest-example.

**Gaps and proposed decisions:**

- Three scenario files; investigate upstream configurations before requesting THIN status.
- IPC needs ipctk; two-body implementations and penalty sensitivity are not measured. Active contact sets can change discretely.
- The complete Step-2 official-test survey has not been authored; shared-suite targets listed here are reading candidates, not asserted physical coverage.
- All unexecuted scenario files: linear_elasticity/elastic_contact_sphere.py, linear_elasticity/two_bodies_contact.py

## shells-membranes

Reduced-dimensional surface mechanics differs from 3D solid constitutive laws; bending shell and membrane regimes share a surface-structure scope.

**Inputs:** surface mesh, thickness, bending/membrane constitutive coefficients, pressure or end load.

**Outputs:** surface displacement/rotation, strain and inflation pressure/stretch.

**Stages:** surface kinematics → thickness and surface quadrature → linear or nonlinear structural equilibrium.

**Expensive path:** Shell10XTerm and TLMembraneTerm surface constitutive integration and tangent assembly.

**Production entry points and logical kernel responsibility:**

- `sfepy/terms/terms_shells.py:Shell10XTerm`
- `sfepy/terms/terms_membrane.py:TLMembraneTerm`

**Owned source files:**

- [`sfepy/terms/terms_shells.py`](../../code/sfepy/sfepy/terms/terms_shells.py)
- [`sfepy/terms/terms_membrane.py`](../../code/sfepy/sfepy/terms/terms_membrane.py)
- [`sfepy/mechanics/shell10x.py`](../../code/sfepy/sfepy/mechanics/shell10x.py)
- [`sfepy/mechanics/membranes.py`](../../code/sfepy/sfepy/mechanics/membranes.py)
- [`sfepy/examples/linear_elasticity/shell10x_cantilever.py`](../../code/sfepy/sfepy/examples/linear_elasticity/shell10x_cantilever.py)
- [`sfepy/examples/linear_elasticity/shell10x_cantilever_interactive.py`](../../code/sfepy/sfepy/examples/linear_elasticity/shell10x_cantilever_interactive.py)
- [`sfepy/examples/large_deformation/balloon.py`](../../code/sfepy/sfepy/examples/large_deformation/balloon.py)

**Shared runtime:** `shared-runtime-and-tests`. Direct cross-module configuration dependencies: none identified by direct Python import inspection.

**Official example/driver inventory:**

| Source | Role | Native execution |
|---|---|---|
| [`sfepy/examples/linear_elasticity/shell10x_cantilever.py`](../../code/sfepy/sfepy/examples/linear_elasticity/shell10x_cantilever.py) | scenario-or-driver | measured |
| [`sfepy/examples/linear_elasticity/shell10x_cantilever_interactive.py`](../../code/sfepy/sfepy/examples/linear_elasticity/shell10x_cantilever_interactive.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/large_deformation/balloon.py`](../../code/sfepy/sfepy/examples/large_deformation/balloon.py) | scenario-or-driver | unmeasured |

**Selected pytest items:**

- `test_declarative_examples.py::test_examples[large_deformation/balloon.py]`
- `test_declarative_examples.py::test_examples[linear_elasticity/shell10x_cantilever.py]`

**Additional shared-test reading targets:**

- [`sfepy/tests/test_term_call_modes.py`](../../code/sfepy/sfepy/tests/test_term_call_modes.py)
- [`sfepy/tests/test_tensors.py`](../../code/sfepy/sfepy/tests/test_tensors.py)

**Measured execution:**

- [`sfepy/examples/linear_elasticity/shell10x_cantilever.py`](../../code/sfepy/sfepy/examples/linear_elasticity/shell10x_cantilever.py): passed, 1.216 s process wall time; official-pytest-example.

**Gaps and proposed decisions:**

- Three example files include two interfaces to the same cantilever, not three independent physical workloads.
- Balloon has an upstream-special-cased load-step reduction and analytic pressure/stretch law; not run in this investigation.
- The complete Step-2 official-test survey has not been authored; shared-suite targets listed here are reading candidates, not asserted physical coverage.
- All unexecuted scenario files: linear_elasticity/shell10x_cantilever_interactive.py, large_deformation/balloon.py

## trusses-springs

One-dimensional members and discrete spring joints need different observables from continuum stress and surface bending. Mixed continuum/truss example exercises coupling without duplicating continuum ownership.

**Inputs:** bar or spring graph, cross sections, stiffness and joint constraints.

**Outputs:** joint displacements and axial/spring forces.

**Stages:** element directions and lengths → axial/joint constitutive operators → assemble coupled structure → solve and recover forces.

**Expensive path:** LinearTrussTerm, LinearDSpringTerm and LinearDRotSpringTerm local transformations and sparse structural solve.

**Production entry points and logical kernel responsibility:**

- `sfepy/terms/terms_elastic.py:LinearTrussTerm`
- `sfepy/terms/terms_elastic.py:LinearDSpringTerm`

**Owned source files:**

- [`sfepy/examples/linear_elasticity/truss_bridge.py`](../../code/sfepy/sfepy/examples/linear_elasticity/truss_bridge.py)
- [`sfepy/examples/linear_elasticity/truss_bridge3d.py`](../../code/sfepy/sfepy/examples/linear_elasticity/truss_bridge3d.py)
- [`sfepy/examples/linear_elasticity/multi_point_constraints.py`](../../code/sfepy/sfepy/examples/linear_elasticity/multi_point_constraints.py)

**Shared runtime:** `shared-runtime-and-tests`. Direct cross-module configuration dependencies: none identified by direct Python import inspection.

**Official example/driver inventory:**

| Source | Role | Native execution |
|---|---|---|
| [`sfepy/examples/linear_elasticity/truss_bridge.py`](../../code/sfepy/sfepy/examples/linear_elasticity/truss_bridge.py) | scenario-or-driver | measured |
| [`sfepy/examples/linear_elasticity/truss_bridge3d.py`](../../code/sfepy/sfepy/examples/linear_elasticity/truss_bridge3d.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/linear_elasticity/multi_point_constraints.py`](../../code/sfepy/sfepy/examples/linear_elasticity/multi_point_constraints.py) | scenario-or-driver | unmeasured |

**Selected pytest items:**

- `test_declarative_examples.py::test_examples[linear_elasticity/truss_bridge3d.py]`
- `test_declarative_examples.py::test_examples[linear_elasticity/truss_bridge.py]`

**Additional shared-test reading targets:**

No additional dedicated target identified; the later full survey still examines relevant shared runtime tests.

**Measured execution:**

- [`sfepy/examples/linear_elasticity/truss_bridge.py`](../../code/sfepy/sfepy/examples/linear_elasticity/truss_bridge.py): passed, 1.216 s process wall time; official-pytest-example.

**Gaps and proposed decisions:**

- Three shipped scenarios; separate 2D/3D examples do not establish sufficient future checks.
- Near-rigid mechanisms and element orientation affect conditioning; joint identities must be preserved.
- The complete Step-2 official-test survey has not been authored; shared-suite targets listed here are reading candidates, not asserted physical coverage.
- All unexecuted scenario files: linear_elasticity/truss_bridge3d.py, linear_elasticity/multi_point_constraints.py

## nonlinear-solid-mechanics

Nonlinear constitutive equilibrium, including the small-strain material-nonlinearity example, is separate from a constant-stiffness solve. TL/UL and mixed pressure are equivalent formulations within this module.

**Inputs:** reference mesh, nonlinear material parameters and load/displacement increments.

**Outputs:** deformation, stress, strain and optional pressure.

**Stages:** deformation gradient/invariants → constitutive stress and consistent tangent → nonlinear force balance → update configuration.

**Expensive path:** Finite-strain constitutive kernels in terms_hyperelastic_tl.py/ul.py, active-fibre response and repeated tangent assembly.

**Production entry points and logical kernel responsibility:**

- `sfepy/terms/terms_hyperelastic_tl.py`
- `sfepy/terms/terms_hyperelastic_ul.py`
- `sfepy/terms/terms_fibres.py`

**Owned source files:**

- [`sfepy/tests/test_hyperelastic_tlul.py`](../../code/sfepy/sfepy/tests/test_hyperelastic_tlul.py)
- [`sfepy/examples/large_deformation/active_fibres.py`](../../code/sfepy/sfepy/examples/large_deformation/active_fibres.py)
- [`sfepy/examples/large_deformation/compare_elastic_materials.py`](../../code/sfepy/sfepy/examples/large_deformation/compare_elastic_materials.py)
- [`sfepy/examples/large_deformation/gen_yeoh_tl_up_interactive.py`](../../code/sfepy/sfepy/examples/large_deformation/gen_yeoh_tl_up_interactive.py)
- [`sfepy/examples/large_deformation/hyperelastic.py`](../../code/sfepy/sfepy/examples/large_deformation/hyperelastic.py)
- [`sfepy/examples/large_deformation/hyperelastic_tl_up_interactive.py`](../../code/sfepy/sfepy/examples/large_deformation/hyperelastic_tl_up_interactive.py)
- [`sfepy/examples/large_deformation/hyperelastic_ul.py`](../../code/sfepy/sfepy/examples/large_deformation/hyperelastic_ul.py)
- [`sfepy/examples/large_deformation/hyperelastic_ul_by_fun.py`](../../code/sfepy/sfepy/examples/large_deformation/hyperelastic_ul_by_fun.py)
- [`sfepy/examples/large_deformation/hyperelastic_ul_up.py`](../../code/sfepy/sfepy/examples/large_deformation/hyperelastic_ul_up.py)
- [`sfepy/examples/linear_elasticity/material_nonlinearity.py`](../../code/sfepy/sfepy/examples/linear_elasticity/material_nonlinearity.py)

**Shared runtime:** `shared-runtime-and-tests`. Direct cross-module configuration dependencies: none identified by direct Python import inspection.

**Official example/driver inventory:**

| Source | Role | Native execution |
|---|---|---|
| [`sfepy/examples/large_deformation/active_fibres.py`](../../code/sfepy/sfepy/examples/large_deformation/active_fibres.py) | scenario-or-driver | measured |
| [`sfepy/examples/large_deformation/compare_elastic_materials.py`](../../code/sfepy/sfepy/examples/large_deformation/compare_elastic_materials.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/large_deformation/gen_yeoh_tl_up_interactive.py`](../../code/sfepy/sfepy/examples/large_deformation/gen_yeoh_tl_up_interactive.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/large_deformation/hyperelastic.py`](../../code/sfepy/sfepy/examples/large_deformation/hyperelastic.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/large_deformation/hyperelastic_tl_up_interactive.py`](../../code/sfepy/sfepy/examples/large_deformation/hyperelastic_tl_up_interactive.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/large_deformation/hyperelastic_ul.py`](../../code/sfepy/sfepy/examples/large_deformation/hyperelastic_ul.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/large_deformation/hyperelastic_ul_by_fun.py`](../../code/sfepy/sfepy/examples/large_deformation/hyperelastic_ul_by_fun.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/large_deformation/hyperelastic_ul_up.py`](../../code/sfepy/sfepy/examples/large_deformation/hyperelastic_ul_up.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/linear_elasticity/material_nonlinearity.py`](../../code/sfepy/sfepy/examples/linear_elasticity/material_nonlinearity.py) | scenario-or-driver | unmeasured |

**Selected pytest items:**

- `test_declarative_examples.py::test_examples[large_deformation/active_fibres.py]`
- `test_declarative_examples.py::test_examples[linear_elasticity/material_nonlinearity.py]`
- `test_hyperelastic_tlul.py::test_solution`

**Additional shared-test reading targets:**

- [`sfepy/tests/test_term_call_modes.py`](../../code/sfepy/sfepy/tests/test_term_call_modes.py)
- [`sfepy/tests/test_term_consistency.py`](../../code/sfepy/sfepy/tests/test_term_consistency.py)

**Measured execution:**

- [`sfepy/examples/large_deformation/active_fibres.py`](../../code/sfepy/sfepy/examples/large_deformation/active_fibres.py): passed, 3.421 s process wall time; official-pytest-example.
- [`sfepy/tests/test_hyperelastic_tlul.py`](../../code/sfepy/sfepy/tests/test_hyperelastic_tlul.py): test_solution passed (11.724 s pytest case).

**Gaps and proposed decisions:**

- Load stepping, loss of stability and mixed incompressibility need separate numerical investigation.
- Active-fibre test uses upstream n_step=5. Material-law/AD variants and interactive drivers remain unmeasured.
- The complete Step-2 official-test survey has not been authored; shared-suite targets listed here are reading candidates, not asserted physical coverage.
- All unexecuted scenario files: large_deformation/compare_elastic_materials.py, large_deformation/gen_yeoh_tl_up_interactive.py, large_deformation/hyperelastic.py, large_deformation/hyperelastic_tl_up_interactive.py, large_deformation/hyperelastic_ul.py, large_deformation/hyperelastic_ul_by_fun.py, large_deformation/hyperelastic_ul_up.py, linear_elasticity/material_nonlinearity.py

## elliptic-diffusion

Stationary scalar elliptic problems are one module across coefficients, BCs, CG/DG/IGA and mesh refinement. laplace_time_ebcs changes BCs without a time derivative and stays here.

**Inputs:** mesh, diffusion tensor, scalar source, boundary conditions and optional stationary advection.

**Outputs:** scalar potential/temperature and physical flux.

**Stages:** diffusion/source operators → Dirichlet/Neumann/periodic constraints → linear or nonlinear stationary solve → flux recovery.

**Expensive path:** LaplaceTerm/DiffusionTerm/NonlinearDiffusionTerm contractions, DG interior-penalty terms where used, and sparse solve.

**Production entry points and logical kernel responsibility:**

- `sfepy/terms/terms_diffusion.py`
- `sfepy/terms/terms_dg.py:DiffusionInteriorPenaltyTerm`

**Owned source files:**

- [`sfepy/tests/test_laplace_unit_disk.py`](../../code/sfepy/sfepy/tests/test_laplace_unit_disk.py)
- [`sfepy/tests/test_laplace_unit_square.py`](../../code/sfepy/sfepy/tests/test_laplace_unit_square.py)
- [`sfepy/tests/test_msm_laplace.py`](../../code/sfepy/sfepy/tests/test_msm_laplace.py)
- [`sfepy/tests/test_msm_symbolic.py`](../../code/sfepy/sfepy/tests/test_msm_symbolic.py)
- [`sfepy/examples/diffusion/cube.py`](../../code/sfepy/sfepy/examples/diffusion/cube.py)
- [`sfepy/examples/diffusion/laplace_1d.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_1d.py)
- [`sfepy/examples/diffusion/laplace_coupling_lcbcs.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_coupling_lcbcs.py)
- [`sfepy/examples/diffusion/laplace_fluid_2d.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_fluid_2d.py)
- [`sfepy/examples/diffusion/laplace_iga_interactive.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_iga_interactive.py)
- [`sfepy/examples/diffusion/laplace_refine_interactive.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_refine_interactive.py)
- [`sfepy/examples/diffusion/laplace_shifted_periodic.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_shifted_periodic.py)
- [`sfepy/examples/diffusion/laplace_time_ebcs.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_time_ebcs.py)
- [`sfepy/examples/diffusion/poisson.py`](../../code/sfepy/sfepy/examples/diffusion/poisson.py)
- [`sfepy/examples/diffusion/poisson_field_dependent_material.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_field_dependent_material.py)
- [`sfepy/examples/diffusion/poisson_functions.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_functions.py)
- [`sfepy/examples/diffusion/poisson_iga.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_iga.py)
- [`sfepy/examples/diffusion/poisson_neumann.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_neumann.py)
- [`sfepy/examples/diffusion/poisson_nonlinear_material.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_nonlinear_material.py)
- [`sfepy/examples/diffusion/poisson_nonlinear_parametric.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_nonlinear_parametric.py)
- [`sfepy/examples/diffusion/poisson_parallel_interactive.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_parallel_interactive.py)
- [`sfepy/examples/diffusion/poisson_parametric_study.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_parametric_study.py)
- [`sfepy/examples/diffusion/poisson_short_syntax.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_short_syntax.py)
- [`sfepy/examples/diffusion/sinbc.py`](../../code/sfepy/sfepy/examples/diffusion/sinbc.py)
- [`sfepy/examples/dg/laplace_2D.py`](../../code/sfepy/sfepy/examples/dg/laplace_2D.py)
- [`sfepy/examples/dg/advection_diffusion_2D.py`](../../code/sfepy/sfepy/examples/dg/advection_diffusion_2D.py)

**Shared runtime:** `shared-runtime-and-tests`. Direct cross-module configuration dependencies: none identified by direct Python import inspection.

**Official example/driver inventory:**

| Source | Role | Native execution |
|---|---|---|
| [`sfepy/examples/diffusion/cube.py`](../../code/sfepy/sfepy/examples/diffusion/cube.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/laplace_1d.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_1d.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/laplace_coupling_lcbcs.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_coupling_lcbcs.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/laplace_fluid_2d.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_fluid_2d.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/laplace_iga_interactive.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_iga_interactive.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/laplace_refine_interactive.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_refine_interactive.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/laplace_shifted_periodic.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_shifted_periodic.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/laplace_time_ebcs.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_time_ebcs.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/poisson.py`](../../code/sfepy/sfepy/examples/diffusion/poisson.py) | scenario-or-driver | measured |
| [`sfepy/examples/diffusion/poisson_field_dependent_material.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_field_dependent_material.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/poisson_functions.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_functions.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/poisson_iga.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_iga.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/poisson_neumann.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_neumann.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/poisson_nonlinear_material.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_nonlinear_material.py) | scenario-or-driver | measured |
| [`sfepy/examples/diffusion/poisson_nonlinear_parametric.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_nonlinear_parametric.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/poisson_parallel_interactive.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_parallel_interactive.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/poisson_parametric_study.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_parametric_study.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/poisson_short_syntax.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_short_syntax.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/sinbc.py`](../../code/sfepy/sfepy/examples/diffusion/sinbc.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/dg/laplace_2D.py`](../../code/sfepy/sfepy/examples/dg/laplace_2D.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/dg/advection_diffusion_2D.py`](../../code/sfepy/sfepy/examples/dg/advection_diffusion_2D.py) | scenario-or-driver | unmeasured |

**Selected pytest items:**

- `test_declarative_examples.py::test_examples[diffusion/cube.py]`
- `test_declarative_examples.py::test_examples[diffusion/laplace_1d.py]`
- `test_declarative_examples.py::test_examples[diffusion/laplace_coupling_lcbcs.py]`
- `test_declarative_examples.py::test_examples[diffusion/laplace_fluid_2d.py]`
- `test_declarative_examples.py::test_examples[diffusion/laplace_time_ebcs.py]`
- `test_declarative_examples.py::test_examples[diffusion/poisson.py]`
- `test_declarative_examples.py::test_examples[diffusion/poisson_field_dependent_material.py]`
- `test_declarative_examples.py::test_examples[diffusion/poisson_functions.py]`
- `test_declarative_examples.py::test_examples[diffusion/poisson_neumann.py]`
- `test_declarative_examples.py::test_examples[diffusion/poisson_nonlinear_material.py]`
- `test_declarative_examples.py::test_examples[diffusion/poisson_nonlinear_parametric.py]`
- `test_declarative_examples.py::test_examples[diffusion/sinbc.py]`
- `test_declarative_examples.py::test_examples_dg[dg/advection_diffusion_2D.py]`
- `test_declarative_examples.py::test_examples_dg[dg/laplace_2D.py]`
- `test_laplace_unit_disk.py::test_boundary_fluxes`
- `test_laplace_unit_square.py::test_solution`
- `test_laplace_unit_square.py::test_boundary_fluxes`
- `test_msm_laplace.py::test_msm_laplace`
- `test_msm_symbolic.py::test_msm_symbolic_laplace`
- `test_msm_symbolic.py::test_msm_symbolic_diffusion`

**Additional shared-test reading targets:**

- [`sfepy/tests/test_lcbcs.py`](../../code/sfepy/sfepy/tests/test_lcbcs.py)
- [`sfepy/tests/test_dg_terms_calls.py`](../../code/sfepy/sfepy/tests/test_dg_terms_calls.py)
- [`sfepy/tests/test_refine_hanging.py`](../../code/sfepy/sfepy/tests/test_refine_hanging.py)
- [`sfepy/tests/test_term_consistency.py`](../../code/sfepy/sfepy/tests/test_term_consistency.py)

**Measured execution:**

- [`sfepy/examples/diffusion/poisson.py`](../../code/sfepy/sfepy/examples/diffusion/poisson.py): passed, 1.166 s process wall time; official-pytest-example.
- [`sfepy/examples/diffusion/poisson_nonlinear_material.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_nonlinear_material.py): passed, 2.118 s process wall time; official-pytest-example.
- [`sfepy/tests/test_laplace_unit_square.py`](../../code/sfepy/sfepy/tests/test_laplace_unit_square.py): test_solution passed (0.015 s pytest case), test_boundary_fluxes passed (0.082 s pytest case).

**Gaps and proposed decisions:**

- Optional igakit, petsc4py/mpi4py and partitioners are not installed/tested.
- Manufactured-solution and flux tests must remain distinct from mere convergence; coefficient nonlinearity and conditioning matter.
- The complete Step-2 official-test survey has not been authored; shared-suite targets listed here are reading candidates, not asserted physical coverage.
- All unexecuted scenario files: diffusion/cube.py, diffusion/laplace_1d.py, diffusion/laplace_coupling_lcbcs.py, diffusion/laplace_fluid_2d.py, diffusion/laplace_iga_interactive.py, diffusion/laplace_refine_interactive.py, diffusion/laplace_shifted_periodic.py, diffusion/laplace_time_ebcs.py, diffusion/poisson_field_dependent_material.py, diffusion/poisson_functions.py, diffusion/poisson_iga.py, diffusion/poisson_neumann.py, diffusion/poisson_nonlinear_parametric.py, diffusion/poisson_parallel_interactive.py, diffusion/poisson_parametric_study.py, diffusion/poisson_short_syntax.py, diffusion/sinbc.py, dg/laplace_2D.py, dg/advection_diffusion_2D.py

## transient-scalar-transport

Time-dependent scalar conservation/advection-diffusion, including nonlinear Burgers flux, differs from stationary elliptic solves. CG/DG and explicit/implicit forms are variants, not separate physics modules.

**Inputs:** initial scalar field, diffusivity, velocity or nonlinear flux, sources and time-dependent BCs.

**Outputs:** temperature/concentration histories, conservation and physical flux.

**Stages:** mass and transport operators → numerical interface fluxes/limiters when DG → time integration → field output.

**Expensive path:** Repeated mass/diffusion/advection operations and DG Lax-Friedrichs fluxes and limiters.

**Production entry points and logical kernel responsibility:**

- `sfepy/terms/terms_diffusion.py:AdvectDivFreeTerm`
- `sfepy/terms/terms_dg.py`
- `sfepy/solvers/ts_dg_solvers.py`

**Owned source files:**

- [`sfepy/examples/diffusion/time_advection_diffusion.py`](../../code/sfepy/sfepy/examples/diffusion/time_advection_diffusion.py)
- [`sfepy/examples/diffusion/time_heat_equation_multi_material.py`](../../code/sfepy/sfepy/examples/diffusion/time_heat_equation_multi_material.py)
- [`sfepy/examples/diffusion/time_poisson.py`](../../code/sfepy/sfepy/examples/diffusion/time_poisson.py)
- [`sfepy/examples/diffusion/time_poisson_explicit.py`](../../code/sfepy/sfepy/examples/diffusion/time_poisson_explicit.py)
- [`sfepy/examples/diffusion/time_poisson_interactive.py`](../../code/sfepy/sfepy/examples/diffusion/time_poisson_interactive.py)
- [`sfepy/examples/diffusion/poisson_periodic_boundary_condition.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_periodic_boundary_condition.py)
- [`sfepy/examples/dg/advection_1D.py`](../../code/sfepy/sfepy/examples/dg/advection_1D.py)
- [`sfepy/examples/dg/advection_2D.py`](../../code/sfepy/sfepy/examples/dg/advection_2D.py)
- [`sfepy/examples/dg/burgers_2D.py`](../../code/sfepy/sfepy/examples/dg/burgers_2D.py)
- [`sfepy/examples/dg/imperative_burgers_1D.py`](../../code/sfepy/sfepy/examples/dg/imperative_burgers_1D.py)

**Shared runtime:** `shared-runtime-and-tests`. Direct cross-module configuration dependencies: none identified by direct Python import inspection.

**Official example/driver inventory:**

| Source | Role | Native execution |
|---|---|---|
| [`sfepy/examples/diffusion/time_advection_diffusion.py`](../../code/sfepy/sfepy/examples/diffusion/time_advection_diffusion.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/time_heat_equation_multi_material.py`](../../code/sfepy/sfepy/examples/diffusion/time_heat_equation_multi_material.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/time_poisson.py`](../../code/sfepy/sfepy/examples/diffusion/time_poisson.py) | scenario-or-driver | measured |
| [`sfepy/examples/diffusion/time_poisson_explicit.py`](../../code/sfepy/sfepy/examples/diffusion/time_poisson_explicit.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/time_poisson_interactive.py`](../../code/sfepy/sfepy/examples/diffusion/time_poisson_interactive.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/poisson_periodic_boundary_condition.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_periodic_boundary_condition.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/dg/advection_1D.py`](../../code/sfepy/sfepy/examples/dg/advection_1D.py) | scenario-or-driver | measured |
| [`sfepy/examples/dg/advection_2D.py`](../../code/sfepy/sfepy/examples/dg/advection_2D.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/dg/burgers_2D.py`](../../code/sfepy/sfepy/examples/dg/burgers_2D.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/dg/imperative_burgers_1D.py`](../../code/sfepy/sfepy/examples/dg/imperative_burgers_1D.py) | scenario-or-driver | unmeasured |

**Selected pytest items:**

- `test_declarative_examples.py::test_examples[diffusion/poisson_periodic_boundary_condition.py]`
- `test_declarative_examples.py::test_examples[diffusion/time_advection_diffusion.py]`
- `test_declarative_examples.py::test_examples[diffusion/time_heat_equation_multi_material.py]`
- `test_declarative_examples.py::test_examples[diffusion/time_poisson.py]`
- `test_declarative_examples.py::test_examples_dg[dg/advection_1D.py]`
- `test_declarative_examples.py::test_examples_dg[dg/advection_2D.py]`

**Additional shared-test reading targets:**

- [`sfepy/tests/test_dg_field.py`](../../code/sfepy/sfepy/tests/test_dg_field.py)
- [`sfepy/tests/test_dg_terms_calls.py`](../../code/sfepy/sfepy/tests/test_dg_terms_calls.py)
- [`sfepy/tests/test_term_call_modes.py`](../../code/sfepy/sfepy/tests/test_term_call_modes.py)

**Measured execution:**

- [`sfepy/examples/diffusion/time_poisson.py`](../../code/sfepy/sfepy/examples/diffusion/time_poisson.py): passed, 1.117 s process wall time; official-pytest-example.
- [`sfepy/examples/dg/advection_1D.py`](../../code/sfepy/sfepy/examples/dg/advection_1D.py): passed, 1.417 s process wall time; official-pytest-example.

**Gaps and proposed decisions:**

- Limiters and shock locations can change discretely; numerical diffusion and conservation need physical output checks.
- Only time_poisson and 1D DG advection measured; nonlinear/interactive drivers and source schedules remain unmeasured.
- The complete Step-2 official-test survey has not been authored; shared-suite targets listed here are reading candidates, not asserted physical coverage.
- All unexecuted scenario files: diffusion/time_advection_diffusion.py, diffusion/time_heat_equation_multi_material.py, diffusion/time_poisson_explicit.py, diffusion/time_poisson_interactive.py, diffusion/poisson_periodic_boundary_condition.py, dg/advection_2D.py, dg/burgers_2D.py, dg/imperative_burgers_1D.py

## incompressible-flow

Velocity-pressure incompressibility is different from scalar potential flow; steady Stokes is the zero-convection limit. Adjoints support the same flow response rather than a separate optimizer task.

**Inputs:** mesh, viscosity, velocity/pressure boundary data and stabilization settings.

**Outputs:** velocity, pressure, flux and optional adjoint responses.

**Stages:** viscous/convective and incompressibility blocks → stabilization or slip constraints → nonlinear/Oseen solve.

**Expensive path:** Navier-Stokes convection/tangent assembly and saddle-point solve; optional adjoint operators are mapped but have no dedicated executed scenario.

**Production entry points and logical kernel responsibility:**

- `sfepy/terms/terms_navier_stokes.py`
- `sfepy/solvers/oseen.py`
- `sfepy/terms/terms_adj_navier_stokes.py`

**Owned source files:**

- [`sfepy/terms/terms_adj_navier_stokes.py`](../../code/sfepy/sfepy/terms/terms_adj_navier_stokes.py)
- [`sfepy/terms/extmods/terms_adj_navier_stokes.c`](../../code/sfepy/sfepy/terms/extmods/terms_adj_navier_stokes.c)
- [`sfepy/terms/extmods/terms_adj_navier_stokes.h`](../../code/sfepy/sfepy/terms/extmods/terms_adj_navier_stokes.h)
- [`sfepy/examples/navier_stokes/navier_stokes.py`](../../code/sfepy/sfepy/examples/navier_stokes/navier_stokes.py)
- [`sfepy/examples/navier_stokes/navier_stokes2d.py`](../../code/sfepy/sfepy/examples/navier_stokes/navier_stokes2d.py)
- [`sfepy/examples/navier_stokes/navier_stokes2d_iga.py`](../../code/sfepy/sfepy/examples/navier_stokes/navier_stokes2d_iga.py)
- [`sfepy/examples/navier_stokes/stabilized_navier_stokes.py`](../../code/sfepy/sfepy/examples/navier_stokes/stabilized_navier_stokes.py)
- [`sfepy/examples/navier_stokes/stokes.py`](../../code/sfepy/sfepy/examples/navier_stokes/stokes.py)
- [`sfepy/examples/navier_stokes/stokes_slip_bc.py`](../../code/sfepy/sfepy/examples/navier_stokes/stokes_slip_bc.py)
- [`sfepy/examples/navier_stokes/utils.py`](../../code/sfepy/sfepy/examples/navier_stokes/utils.py)

**Shared runtime:** `shared-runtime-and-tests`. Direct cross-module configuration dependencies: none identified by direct Python import inspection.

**Official example/driver inventory:**

| Source | Role | Native execution |
|---|---|---|
| [`sfepy/examples/navier_stokes/navier_stokes.py`](../../code/sfepy/sfepy/examples/navier_stokes/navier_stokes.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/navier_stokes/navier_stokes2d.py`](../../code/sfepy/sfepy/examples/navier_stokes/navier_stokes2d.py) | scenario-or-driver | measured |
| [`sfepy/examples/navier_stokes/navier_stokes2d_iga.py`](../../code/sfepy/sfepy/examples/navier_stokes/navier_stokes2d_iga.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/navier_stokes/stabilized_navier_stokes.py`](../../code/sfepy/sfepy/examples/navier_stokes/stabilized_navier_stokes.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/navier_stokes/stokes.py`](../../code/sfepy/sfepy/examples/navier_stokes/stokes.py) | scenario-or-driver | measured |
| [`sfepy/examples/navier_stokes/stokes_slip_bc.py`](../../code/sfepy/sfepy/examples/navier_stokes/stokes_slip_bc.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/navier_stokes/utils.py`](../../code/sfepy/sfepy/examples/navier_stokes/utils.py) | support | unmeasured |

**Selected pytest items:**

- `test_declarative_examples.py::test_examples[navier_stokes/navier_stokes.py]`
- `test_declarative_examples.py::test_examples[navier_stokes/navier_stokes2d.py]`
- `test_declarative_examples.py::test_examples[navier_stokes/stabilized_navier_stokes.py]`
- `test_declarative_examples.py::test_examples[navier_stokes/stokes.py]`
- `test_declarative_examples.py::test_examples[navier_stokes/stokes_slip_bc.py]`

**Additional shared-test reading targets:**

- [`sfepy/tests/test_lcbcs.py`](../../code/sfepy/sfepy/tests/test_lcbcs.py)
- [`sfepy/tests/test_term_sensitivity.py`](../../code/sfepy/sfepy/tests/test_term_sensitivity.py)
- [`sfepy/tests/test_term_call_modes.py`](../../code/sfepy/sfepy/tests/test_term_call_modes.py)

**Measured execution:**

- [`sfepy/examples/navier_stokes/navier_stokes2d.py`](../../code/sfepy/sfepy/examples/navier_stokes/navier_stokes2d.py): passed, 3.471 s process wall time; official-pytest-example.
- [`sfepy/examples/navier_stokes/stokes.py`](../../code/sfepy/sfepy/examples/navier_stokes/stokes.py): passed, 1.166 s process wall time; official-pytest-example.

**Gaps and proposed decisions:**

- Pressure gauge and nonlinear convergence are not portable array offsets; use physical gauge/constraints.
- IGA and adjoint paths unmeasured; utils.py is helper code, not an independent scenario.
- The complete Step-2 official-test survey has not been authored; shared-suite targets listed here are reading candidates, not asserted physical coverage.
- All unexecuted scenario files: navier_stokes/navier_stokes.py, navier_stokes/navier_stokes2d_iga.py, navier_stokes/stabilized_navier_stokes.py, navier_stokes/stokes_slip_bc.py

## acoustic-helmholtz

Forced frequency-domain pressure waves, including the scalar Helmholtz propagation analogy and vibro-acoustic interface, are distinct from elastic eigenmode extraction.

**Inputs:** frequency, acoustic material, source, impedance/interface conditions.

**Outputs:** complex acoustic pressure and coupled interface motion.

**Stages:** complex Helmholtz operators → radiation/interface coupling → frequency-domain solve.

**Expensive path:** Complex sparse Helmholtz solve and coupled perforated-interface assembly.

**Production entry points and logical kernel responsibility:**

- `sfepy/examples/acoustics/acoustics.py`
- `sfepy/examples/acoustics/vibro_acoustic3d.py`

**Owned source files:**

- [`sfepy/examples/acoustics/acoustics.py`](../../code/sfepy/sfepy/examples/acoustics/acoustics.py)
- [`sfepy/examples/acoustics/acoustics3d.py`](../../code/sfepy/sfepy/examples/acoustics/acoustics3d.py)
- [`sfepy/examples/acoustics/helmholtz_apartment.py`](../../code/sfepy/sfepy/examples/acoustics/helmholtz_apartment.py)
- [`sfepy/examples/acoustics/vibro_acoustic3d.py`](../../code/sfepy/sfepy/examples/acoustics/vibro_acoustic3d.py)

**Shared runtime:** `shared-runtime-and-tests`. Direct cross-module configuration dependencies: none identified by direct Python import inspection.

**Official example/driver inventory:**

| Source | Role | Native execution |
|---|---|---|
| [`sfepy/examples/acoustics/acoustics.py`](../../code/sfepy/sfepy/examples/acoustics/acoustics.py) | scenario-or-driver | measured |
| [`sfepy/examples/acoustics/acoustics3d.py`](../../code/sfepy/sfepy/examples/acoustics/acoustics3d.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/acoustics/helmholtz_apartment.py`](../../code/sfepy/sfepy/examples/acoustics/helmholtz_apartment.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/acoustics/vibro_acoustic3d.py`](../../code/sfepy/sfepy/examples/acoustics/vibro_acoustic3d.py) | scenario-or-driver | unmeasured |

**Selected pytest items:**

- `test_declarative_examples.py::test_examples[acoustics/acoustics.py]`
- `test_declarative_examples.py::test_examples[acoustics/acoustics3d.py]`
- `test_declarative_examples.py::test_examples[acoustics/helmholtz_apartment.py]`
- `test_declarative_examples.py::test_examples[acoustics/vibro_acoustic3d.py]`

**Additional shared-test reading targets:**

- [`sfepy/tests/test_assembling.py`](../../code/sfepy/sfepy/tests/test_assembling.py)
- [`sfepy/tests/test_term_call_modes.py`](../../code/sfepy/sfepy/tests/test_term_call_modes.py)

**Measured execution:**

- [`sfepy/examples/acoustics/acoustics.py`](../../code/sfepy/sfepy/examples/acoustics/acoustics.py): passed, 1.116 s process wall time; official-pytest-example.

**Gaps and proposed decisions:**

- Complex phase tied to the imposed source is physical; do not discard phase arbitrarily.
- Near resonances and interface coupling may amplify rounding; only acoustics.py measured.
- The complete Step-2 official-test survey has not been authored; shared-suite targets listed here are reading candidates, not asserted physical coverage.
- All unexecuted scenario files: acoustics/acoustics3d.py, acoustics/helmholtz_apartment.py, acoustics/vibro_acoustic3d.py

## single-particle-quantum

Single-particle quantum Hamiltonians define their own physical spectrum; hydrogenic atom, oscillator and well are potentials within one module.

**Inputs:** potential function, domain mesh, boundary condition and requested eigenpairs.

**Outputs:** energy eigenvalues and probability/subspace observables.

**Stages:** kinetic and potential matrix assembly → generalized eigensolve → compare analytic spectra when available.

**Expensive path:** Sparse shift-invert Hamiltonian eigensolve in quantum_common.py and solvers/eigen.py.

**Production entry points and logical kernel responsibility:**

- `sfepy/examples/quantum/quantum_common.py:common`
- `sfepy/solvers/eigen.py`

**Owned source files:**

- [`sfepy/examples/quantum/boron.py`](../../code/sfepy/sfepy/examples/quantum/boron.py)
- [`sfepy/examples/quantum/hydrogen.py`](../../code/sfepy/sfepy/examples/quantum/hydrogen.py)
- [`sfepy/examples/quantum/oscillator.py`](../../code/sfepy/sfepy/examples/quantum/oscillator.py)
- [`sfepy/examples/quantum/well.py`](../../code/sfepy/sfepy/examples/quantum/well.py)
- [`sfepy/examples/quantum/quantum_common.py`](../../code/sfepy/sfepy/examples/quantum/quantum_common.py)

**Shared runtime:** `shared-runtime-and-tests`. Direct cross-module configuration dependencies: none identified by direct Python import inspection.

**Official example/driver inventory:**

| Source | Role | Native execution |
|---|---|---|
| [`sfepy/examples/quantum/boron.py`](../../code/sfepy/sfepy/examples/quantum/boron.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/quantum/hydrogen.py`](../../code/sfepy/sfepy/examples/quantum/hydrogen.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/quantum/oscillator.py`](../../code/sfepy/sfepy/examples/quantum/oscillator.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/quantum/well.py`](../../code/sfepy/sfepy/examples/quantum/well.py) | scenario-or-driver | measured |
| [`sfepy/examples/quantum/quantum_common.py`](../../code/sfepy/sfepy/examples/quantum/quantum_common.py) | support | unmeasured |

**Selected pytest items:**

- `test_declarative_examples.py::test_examples[quantum/boron.py]`
- `test_declarative_examples.py::test_examples[quantum/hydrogen.py]`
- `test_declarative_examples.py::test_examples[quantum/oscillator.py]`
- `test_declarative_examples.py::test_examples[quantum/well.py]`

**Additional shared-test reading targets:**

- [`sfepy/tests/test_eigenvalue_solvers.py`](../../code/sfepy/sfepy/tests/test_eigenvalue_solvers.py)

**Measured execution:**

- [`sfepy/examples/quantum/well.py`](../../code/sfepy/sfepy/examples/quantum/well.py): passed, 1.617 s process wall time; official-pytest-example.

**Gaps and proposed decisions:**

- Eigenvector sign/phase and degenerate bases are arbitrary. Singular Coulomb potentials and mesh discretization affect error.
- Only well.py measured. quantum_common.py is support, leaving four scenario files.
- The complete Step-2 official-test survey has not been authored; shared-suite targets listed here are reading candidates, not asserted physical coverage.
- All unexecuted scenario files: quantum/boron.py, quantum/hydrogen.py, quantum/oscillator.py

## porous-media-flow

Porous-fluid mass balance is the common physical responsibility; rigid Darcy, deformable Biot/perfusion and effective porous-cell coefficients are regimes. It does not own free-fluid Navier-Stokes or uncoupled elasticity.

**Inputs:** permeability, porosity/coupling tensors, pressure/flow BCs and solid material when deformable.

**Outputs:** pore pressure, Darcy flux, coupled deformation or effective transport coefficients.

**Stages:** Darcy diffusion and compartment exchange → Biot or finite-strain fluid/solid coupling → micro-correctors where requested → coupled pressure/deformation solve.

**Expensive path:** Coupled diffusion/deformation blocks, finite-strain permeability updates and porous-cell corrector solves.

**Production entry points and logical kernel responsibility:**

- `sfepy/terms/terms_biot.py`
- `sfepy/terms/terms_hyperelastic_tl.py:DiffusionTLTerm`
- `sfepy/examples/homogenization/perfusion_micro.py`

**Owned source files:**

- [`sfepy/tests/test_homogenization_perfusion.py`](../../code/sfepy/sfepy/tests/test_homogenization_perfusion.py)
- [`sfepy/examples/multi_physics/biot.py`](../../code/sfepy/sfepy/examples/multi_physics/biot.py)
- [`sfepy/examples/multi_physics/biot_npbc.py`](../../code/sfepy/sfepy/examples/multi_physics/biot_npbc.py)
- [`sfepy/examples/multi_physics/biot_npbc_lagrange.py`](../../code/sfepy/sfepy/examples/multi_physics/biot_npbc_lagrange.py)
- [`sfepy/examples/multi_physics/biot_parallel_interactive.py`](../../code/sfepy/sfepy/examples/multi_physics/biot_parallel_interactive.py)
- [`sfepy/examples/multi_physics/biot_short_syntax.py`](../../code/sfepy/sfepy/examples/multi_physics/biot_short_syntax.py)
- [`sfepy/examples/diffusion/darcy_flow_multicomp.py`](../../code/sfepy/sfepy/examples/diffusion/darcy_flow_multicomp.py)
- [`sfepy/examples/large_deformation/perfusion_tl.py`](../../code/sfepy/sfepy/examples/large_deformation/perfusion_tl.py)
- [`sfepy/examples/homogenization/perfusion_micro.py`](../../code/sfepy/sfepy/examples/homogenization/perfusion_micro.py)

**Shared runtime:** `shared-runtime-and-tests`. Direct cross-module configuration dependencies: `elliptic-diffusion`.

**Official example/driver inventory:**

| Source | Role | Native execution |
|---|---|---|
| [`sfepy/examples/multi_physics/biot.py`](../../code/sfepy/sfepy/examples/multi_physics/biot.py) | scenario-or-driver | measured |
| [`sfepy/examples/multi_physics/biot_npbc.py`](../../code/sfepy/sfepy/examples/multi_physics/biot_npbc.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/multi_physics/biot_npbc_lagrange.py`](../../code/sfepy/sfepy/examples/multi_physics/biot_npbc_lagrange.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/multi_physics/biot_parallel_interactive.py`](../../code/sfepy/sfepy/examples/multi_physics/biot_parallel_interactive.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/multi_physics/biot_short_syntax.py`](../../code/sfepy/sfepy/examples/multi_physics/biot_short_syntax.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/diffusion/darcy_flow_multicomp.py`](../../code/sfepy/sfepy/examples/diffusion/darcy_flow_multicomp.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/large_deformation/perfusion_tl.py`](../../code/sfepy/sfepy/examples/large_deformation/perfusion_tl.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/homogenization/perfusion_micro.py`](../../code/sfepy/sfepy/examples/homogenization/perfusion_micro.py) | scenario-or-driver | unmeasured |

**Selected pytest items:**

- `test_declarative_examples.py::test_examples[diffusion/darcy_flow_multicomp.py]`
- `test_declarative_examples.py::test_examples[large_deformation/perfusion_tl.py]`
- `test_declarative_examples.py::test_examples[multi_physics/biot.py]`
- `test_declarative_examples.py::test_examples[multi_physics/biot_npbc.py]`
- `test_declarative_examples.py::test_examples[multi_physics/biot_npbc_lagrange.py]`
- `test_declarative_examples.py::test_examples[multi_physics/biot_short_syntax.py]`
- `test_homogenization_perfusion.py::test_solution`

**Additional shared-test reading targets:**

- [`sfepy/tests/test_homogenization_engine.py`](../../code/sfepy/sfepy/tests/test_homogenization_engine.py)
- [`sfepy/tests/test_term_consistency.py`](../../code/sfepy/sfepy/tests/test_term_consistency.py)
- [`sfepy/tests/test_term_sensitivity.py`](../../code/sfepy/sfepy/tests/test_term_sensitivity.py)

**Measured execution:**

- [`sfepy/examples/multi_physics/biot.py`](../../code/sfepy/sfepy/examples/multi_physics/biot.py): passed, 1.667 s process wall time; official-pytest-example.

**Gaps and proposed decisions:**

- This is a broad proposed cut; rigid/finite-strain/homogenized regimes may warrant splitting after complete test investigation.
- Only biot.py measured; perfusion, MPI and microstructure reference relations remain unmeasured.
- The complete Step-2 official-test survey has not been authored; shared-suite targets listed here are reading candidates, not asserted physical coverage.
- All unexecuted scenario files: multi_physics/biot_npbc.py, multi_physics/biot_npbc_lagrange.py, multi_physics/biot_parallel_interactive.py, multi_physics/biot_short_syntax.py, diffusion/darcy_flow_multicomp.py, large_deformation/perfusion_tl.py, homogenization/perfusion_micro.py

## piezoelectricity

Electromechanical conversion links direct, dynamic and homogenized configurations. Macro/micro files form one coupled chain, not two independent end-to-end tests.

**Inputs:** elastic/piezoelectric/dielectric tensors, electrodes and mechanical loading.

**Outputs:** displacement, electric potential, induced voltage and effective coefficients.

**Stages:** mechanical/electric coupling assembly → electrode constraints → harmonic/transient or micro-macro solution.

**Expensive path:** PiezoCouplingTerm and surface piezo flux operators, coupled block solves and electrode response.

**Production entry points and logical kernel responsibility:**

- `sfepy/terms/terms_piezo.py:PiezoCouplingTerm`
- `sfepy/examples/multi_physics/piezo_elasticity_micro.py`

**Owned source files:**

- [`sfepy/terms/terms_piezo.py`](../../code/sfepy/sfepy/terms/terms_piezo.py)
- [`sfepy/terms/extmods/terms_piezo.c`](../../code/sfepy/sfepy/terms/extmods/terms_piezo.c)
- [`sfepy/terms/extmods/terms_piezo.h`](../../code/sfepy/sfepy/terms/extmods/terms_piezo.h)
- [`sfepy/examples/multi_physics/piezo_elasticity.py`](../../code/sfepy/sfepy/examples/multi_physics/piezo_elasticity.py)
- [`sfepy/examples/multi_physics/piezo_elastodynamic.py`](../../code/sfepy/sfepy/examples/multi_physics/piezo_elastodynamic.py)
- [`sfepy/examples/multi_physics/piezo_elasticity_macro.py`](../../code/sfepy/sfepy/examples/multi_physics/piezo_elasticity_macro.py)
- [`sfepy/examples/multi_physics/piezo_elasticity_micro.py`](../../code/sfepy/sfepy/examples/multi_physics/piezo_elasticity_micro.py)

**Shared runtime:** `shared-runtime-and-tests`. Direct cross-module configuration dependencies: none identified by direct Python import inspection.

**Official example/driver inventory:**

| Source | Role | Native execution |
|---|---|---|
| [`sfepy/examples/multi_physics/piezo_elasticity.py`](../../code/sfepy/sfepy/examples/multi_physics/piezo_elasticity.py) | scenario-or-driver | measured |
| [`sfepy/examples/multi_physics/piezo_elastodynamic.py`](../../code/sfepy/sfepy/examples/multi_physics/piezo_elastodynamic.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/multi_physics/piezo_elasticity_macro.py`](../../code/sfepy/sfepy/examples/multi_physics/piezo_elasticity_macro.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/multi_physics/piezo_elasticity_micro.py`](../../code/sfepy/sfepy/examples/multi_physics/piezo_elasticity_micro.py) | scenario-or-driver | unmeasured |

**Selected pytest items:**

- `test_declarative_examples.py::test_examples[multi_physics/piezo_elasticity.py]`
- `test_declarative_examples.py::test_examples[multi_physics/piezo_elastodynamic.py]`

**Additional shared-test reading targets:**

- [`sfepy/tests/test_term_sensitivity.py`](../../code/sfepy/sfepy/tests/test_term_sensitivity.py)
- [`sfepy/tests/test_term_call_modes.py`](../../code/sfepy/sfepy/tests/test_term_call_modes.py)

**Measured execution:**

- [`sfepy/examples/multi_physics/piezo_elasticity.py`](../../code/sfepy/sfepy/examples/multi_physics/piezo_elasticity.py): passed, 1.366 s process wall time; official-pytest-example.

**Gaps and proposed decisions:**

- Only direct piezo_elasticity measured; dynamic and micro-macro chain unmeasured.
- Four files do not prove four suitable independent checks; electrode gauge and coupled signs are critical.
- The complete Step-2 official-test survey has not been authored; shared-suite targets listed here are reading candidates, not asserted physical coverage.
- All unexecuted scenario files: multi_physics/piezo_elastodynamic.py, multi_physics/piezo_elasticity_macro.py, multi_physics/piezo_elasticity_micro.py

## thermoelasticity

Temperature-induced mechanical strain differs from electric heat generation or prescribed mechanical load. Prescribed/computed temperature are variants in one module.

**Inputs:** temperature or thermal BCs, reference temperature, expansion coefficient and elastic stiffness.

**Outputs:** temperature, thermal displacement and stress.

**Stages:** obtain temperature → construct thermal strain coupling → solve mechanical equilibrium.

**Expensive path:** Thermal-strain coupling through BiotTerm and sequential heat/elasticity solves.

**Production entry points and logical kernel responsibility:**

- `sfepy/examples/multi_physics/thermo_elasticity.py`
- `sfepy/examples/multi_physics/thermo_elasticity_ess.py`
- `sfepy/terms/terms_biot.py`

**Owned source files:**

- [`sfepy/examples/multi_physics/thermo_elasticity.py`](../../code/sfepy/sfepy/examples/multi_physics/thermo_elasticity.py)
- [`sfepy/examples/multi_physics/thermo_elasticity_ess.py`](../../code/sfepy/sfepy/examples/multi_physics/thermo_elasticity_ess.py)

**Shared runtime:** `shared-runtime-and-tests`. Direct cross-module configuration dependencies: none identified by direct Python import inspection.

**Official example/driver inventory:**

| Source | Role | Native execution |
|---|---|---|
| [`sfepy/examples/multi_physics/thermo_elasticity.py`](../../code/sfepy/sfepy/examples/multi_physics/thermo_elasticity.py) | scenario-or-driver | measured |
| [`sfepy/examples/multi_physics/thermo_elasticity_ess.py`](../../code/sfepy/sfepy/examples/multi_physics/thermo_elasticity_ess.py) | scenario-or-driver | unmeasured |

**Selected pytest items:**

- `test_declarative_examples.py::test_examples[multi_physics/thermo_elasticity.py]`
- `test_declarative_examples.py::test_examples[multi_physics/thermo_elasticity_ess.py]`

**Additional shared-test reading targets:**

No additional dedicated target identified; the later full survey still examines relevant shared runtime tests.

**Measured execution:**

- [`sfepy/examples/multi_physics/thermo_elasticity.py`](../../code/sfepy/sfepy/examples/multi_physics/thermo_elasticity.py): passed, 1.316 s process wall time; official-pytest-example.

**Gaps and proposed decisions:**

- Only two official scenario files; coverage is limited and THIN/custom decisions remain for later approval.
- Only prescribed-temperature example measured; computed-temperature equation sequencing unmeasured.
- The complete Step-2 official-test survey has not been authored; shared-suite targets listed here are reading candidates, not asserted physical coverage.
- All unexecuted scenario files: multi_physics/thermo_elasticity_ess.py

## joule-heating

Electrical energy conversion to heat has a distinct coupling term and output contract; it is not thermoelastic expansion.

**Inputs:** electric/thermal conductivity, potential BCs, heat capacity and initial temperature.

**Outputs:** electric potential and time-dependent temperature.

**Stages:** stationary electric solve → form conductivity times squared potential gradient → transient heat solve.

**Expensive path:** ElectricSourceTerm quadrature and repeated heat-equation solve driven by electric dissipation.

**Production entry points and logical kernel responsibility:**

- `sfepy/terms/terms_electric.py:ElectricSourceTerm`
- `sfepy/examples/multi_physics/thermal_electric.py:main`

**Owned source files:**

- [`sfepy/terms/terms_electric.py`](../../code/sfepy/sfepy/terms/terms_electric.py)
- [`sfepy/terms/extmods/terms_electric.c`](../../code/sfepy/sfepy/terms/extmods/terms_electric.c)
- [`sfepy/terms/extmods/terms_electric.h`](../../code/sfepy/sfepy/terms/extmods/terms_electric.h)
- [`sfepy/examples/multi_physics/thermal_electric.py`](../../code/sfepy/sfepy/examples/multi_physics/thermal_electric.py)

**Shared runtime:** `shared-runtime-and-tests`. Direct cross-module configuration dependencies: none identified by direct Python import inspection.

**Official example/driver inventory:**

| Source | Role | Native execution |
|---|---|---|
| [`sfepy/examples/multi_physics/thermal_electric.py`](../../code/sfepy/sfepy/examples/multi_physics/thermal_electric.py) | scenario-or-driver | measured |

**Selected pytest items:**

No selected item in the recorded collection. This does not turn generic term discovery into a physical test.

**Additional shared-test reading targets:**

No additional dedicated target identified; the later full survey still examines relevant shared runtime tests.

**Measured execution:**

- [`sfepy/examples/multi_physics/thermal_electric.py`](../../code/sfepy/sfepy/examples/multi_physics/thermal_electric.py): completed, 0.87 s process wall time; official-standalone-driver. exit and completion evidence only; no independent physical-reference assertion

**Gaps and proposed decisions:**

- One official driver; no independent assertion-based test found, so proposed only pending coverage/THIN decision.
- Driver completed with 11 steps and output files; no golden-output or energy assertion was run.
- The complete Step-2 official-test survey has not been authored; shared-suite targets listed here are reading candidates, not asserted physical coverage.
- All unexecuted scenario files: 

## elastic-homogenization

This module owns elastic scale transition, not the local constitutive kernel. Porous, piezoelectric and phononic coefficient chains are assigned by their own physical contracts.

**Inputs:** periodic heterogeneous material cell, constitutive data and optional macro strain.

**Outputs:** effective elastic tensors, correctors and recovered micro/macro fields.

**Stages:** resolve coefficient dependency graph → solve cell correctors → average effective coefficients → macro solve and local recovery.

**Expensive path:** Multiple cell corrector solves and nonlinear microproblems at macroscopic quadrature points.

**Production entry points and logical kernel responsibility:**

- `sfepy/homogenization/engine.py`
- `sfepy/homogenization/micmac.py`
- `sfepy/examples/homogenization/nonlinear_hyperelastic_mM.py`

**Owned source files:**

- [`sfepy/examples/homogenization/homogenization_opt.py`](../../code/sfepy/sfepy/examples/homogenization/homogenization_opt.py)
- [`sfepy/examples/homogenization/linear_elastic_mM.py`](../../code/sfepy/sfepy/examples/homogenization/linear_elastic_mM.py)
- [`sfepy/examples/homogenization/linear_elasticity_opt.py`](../../code/sfepy/sfepy/examples/homogenization/linear_elasticity_opt.py)
- [`sfepy/examples/homogenization/linear_homogenization.py`](../../code/sfepy/sfepy/examples/homogenization/linear_homogenization.py)
- [`sfepy/examples/homogenization/linear_homogenization_postproc.py`](../../code/sfepy/sfepy/examples/homogenization/linear_homogenization_postproc.py)
- [`sfepy/examples/homogenization/linear_homogenization_up.py`](../../code/sfepy/sfepy/examples/homogenization/linear_homogenization_up.py)
- [`sfepy/examples/homogenization/material_opt.py`](../../code/sfepy/sfepy/examples/homogenization/material_opt.py)
- [`sfepy/examples/homogenization/nonlinear_homogenization.py`](../../code/sfepy/sfepy/examples/homogenization/nonlinear_homogenization.py)
- [`sfepy/examples/homogenization/nonlinear_hyperelastic_mM.py`](../../code/sfepy/sfepy/examples/homogenization/nonlinear_hyperelastic_mM.py)
- [`sfepy/examples/homogenization/rs_correctors.py`](../../code/sfepy/sfepy/examples/homogenization/rs_correctors.py)

**Shared runtime:** `shared-runtime-and-tests`. Direct cross-module configuration dependencies: none identified by direct Python import inspection.

**Official example/driver inventory:**

| Source | Role | Native execution |
|---|---|---|
| [`sfepy/examples/homogenization/homogenization_opt.py`](../../code/sfepy/sfepy/examples/homogenization/homogenization_opt.py) | support | unmeasured |
| [`sfepy/examples/homogenization/linear_elastic_mM.py`](../../code/sfepy/sfepy/examples/homogenization/linear_elastic_mM.py) | scenario-or-driver | measured |
| [`sfepy/examples/homogenization/linear_elasticity_opt.py`](../../code/sfepy/sfepy/examples/homogenization/linear_elasticity_opt.py) | support | unmeasured |
| [`sfepy/examples/homogenization/linear_homogenization.py`](../../code/sfepy/sfepy/examples/homogenization/linear_homogenization.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/homogenization/linear_homogenization_postproc.py`](../../code/sfepy/sfepy/examples/homogenization/linear_homogenization_postproc.py) | support | unmeasured |
| [`sfepy/examples/homogenization/linear_homogenization_up.py`](../../code/sfepy/sfepy/examples/homogenization/linear_homogenization_up.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/homogenization/material_opt.py`](../../code/sfepy/sfepy/examples/homogenization/material_opt.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/homogenization/nonlinear_homogenization.py`](../../code/sfepy/sfepy/examples/homogenization/nonlinear_homogenization.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/homogenization/nonlinear_hyperelastic_mM.py`](../../code/sfepy/sfepy/examples/homogenization/nonlinear_hyperelastic_mM.py) | scenario-or-driver | unmeasured |
| [`sfepy/examples/homogenization/rs_correctors.py`](../../code/sfepy/sfepy/examples/homogenization/rs_correctors.py) | scenario-or-driver | unmeasured |

**Selected pytest items:**

- `test_declarative_examples.py::test_examples[homogenization/linear_elastic_mM.py]`

**Additional shared-test reading targets:**

No additional dedicated target identified; the later full survey still examines relevant shared runtime tests.

**Measured execution:**

- [`sfepy/examples/homogenization/linear_elastic_mM.py`](../../code/sfepy/sfepy/examples/homogenization/linear_elastic_mM.py): passed, 4.976 s process wall time; official-pytest-example.

**Gaps and proposed decisions:**

- Some files are optimization inputs or postprocessing helpers, not independent scenarios.
- Only linear_elastic_mM measured; nonlinear micro-macro solves, optimization and legacy rs_correctors driver remain unmeasured.
- The complete Step-2 official-test survey has not been authored; shared-suite targets listed here are reading candidates, not asserted physical coverage.
- All unexecuted scenario files: homogenization/linear_homogenization.py, homogenization/linear_homogenization_up.py, homogenization/material_opt.py, homogenization/nonlinear_homogenization.py, homogenization/nonlinear_hyperelastic_mM.py, homogenization/rs_correctors.py

## flexoelectricity

Strain-gradient electromechanics is a distinct source capability from ordinary piezoelectricity. This is a deferred candidate in the full map, not a task-ready commitment.

**Inputs:** strain-gradient and flexoelectric tensors, displacement-gradient and electric fields.

**Outputs:** coupled strain-gradient mechanical and electric responses.

**Stages:** map gradient to symmetric strain → contract higher-order tensors → assemble mixed gradient/electric blocks.

**Expensive path:** MixedStrainGradElasticTerm and MixedFlexoCouplingTerm higher-order tensor contractions.

**Production entry points and logical kernel responsibility:**

- `sfepy/terms/terms_flexo.py:MixedStrainGradElasticTerm`
- `sfepy/terms/terms_flexo.py:MixedFlexoCouplingTerm`

**Owned source files:**

- [`sfepy/terms/terms_flexo.py`](../../code/sfepy/sfepy/terms/terms_flexo.py)

**Shared runtime:** `shared-runtime-and-tests`. Direct cross-module configuration dependencies: none identified by direct Python import inspection.

**Official example/driver inventory:**

| Source | Role | Native execution |
|---|---|---|
| None found | — | unmeasured |

**Selected pytest items:**

No selected item in the recorded collection. This does not turn generic term discovery into a physical test.

**Additional shared-test reading targets:**

No additional dedicated target identified; the later full survey still examines relevant shared runtime tests.

**Measured execution:**

No native execution recorded for this module.

**Gaps and proposed decisions:**

- No dedicated official scenario or explicit test reference found at this pin; no native physics run made.
- Do not scaffold this candidate until an official test is identified or a custom-check proposal is explicitly approved. Generic term-call machinery alone is not physical validation.
- The complete Step-2 official-test survey has not been authored; shared-suite targets listed here are reading candidates, not asserted physical coverage.
- All unexecuted scenario files: 

## Shared infrastructure and remaining source files

Exact shared paths are in `shared_components[0].paths` in the canonical JSON. Shared runtime is not an additional benchmark module: it contains the common FE framework, physical kernels reused across equations, solvers/bindings, application I/O and cross-cutting tests. Specialized operators owned by one module remain in its card.

| Retained non-module category | Files |
|---|---:|
| build/developer tools | 13 |
| documentation | 346 |
| mesh resources | 96 |
| non-scientific demo | 1 |
| root metadata/build/licensing/CI | 13 |

The canonical JSON lists every path in these categories. Build scripts, meshes and documentation remain part of the original source snapshot; they are not discarded.

## Complete example-file inventory

| File | Assigned module / disposition | Role |
|---|---|---|
| [`sfepy/examples/__init__.py`](../../code/sfepy/sfepy/examples/__init__.py) | shared support / package initializer | support |
| [`sfepy/examples/acoustics/__init__.py`](../../code/sfepy/sfepy/examples/acoustics/__init__.py) | shared support / package initializer | support |
| [`sfepy/examples/acoustics/acoustics.py`](../../code/sfepy/sfepy/examples/acoustics/acoustics.py) | acoustic-helmholtz | scenario-or-driver |
| [`sfepy/examples/acoustics/acoustics3d.py`](../../code/sfepy/sfepy/examples/acoustics/acoustics3d.py) | acoustic-helmholtz | scenario-or-driver |
| [`sfepy/examples/acoustics/helmholtz_apartment.py`](../../code/sfepy/sfepy/examples/acoustics/helmholtz_apartment.py) | acoustic-helmholtz | scenario-or-driver |
| [`sfepy/examples/acoustics/vibro_acoustic3d.py`](../../code/sfepy/sfepy/examples/acoustics/vibro_acoustic3d.py) | acoustic-helmholtz | scenario-or-driver |
| [`sfepy/examples/dg/__init__.py`](../../code/sfepy/sfepy/examples/dg/__init__.py) | shared support / package initializer | support |
| [`sfepy/examples/dg/advection_1D.py`](../../code/sfepy/sfepy/examples/dg/advection_1D.py) | transient-scalar-transport | scenario-or-driver |
| [`sfepy/examples/dg/advection_2D.py`](../../code/sfepy/sfepy/examples/dg/advection_2D.py) | transient-scalar-transport | scenario-or-driver |
| [`sfepy/examples/dg/advection_diffusion_2D.py`](../../code/sfepy/sfepy/examples/dg/advection_diffusion_2D.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/dg/burgers_2D.py`](../../code/sfepy/sfepy/examples/dg/burgers_2D.py) | transient-scalar-transport | scenario-or-driver |
| [`sfepy/examples/dg/dg_plot_1D.py`](../../code/sfepy/sfepy/examples/dg/dg_plot_1D.py) | shared support / package initializer | support |
| [`sfepy/examples/dg/example_dg_common.py`](../../code/sfepy/sfepy/examples/dg/example_dg_common.py) | shared support / package initializer | support |
| [`sfepy/examples/dg/imperative_burgers_1D.py`](../../code/sfepy/sfepy/examples/dg/imperative_burgers_1D.py) | transient-scalar-transport | scenario-or-driver |
| [`sfepy/examples/dg/laplace_2D.py`](../../code/sfepy/sfepy/examples/dg/laplace_2D.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/__init__.py`](../../code/sfepy/sfepy/examples/diffusion/__init__.py) | shared support / package initializer | support |
| [`sfepy/examples/diffusion/cube.py`](../../code/sfepy/sfepy/examples/diffusion/cube.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/darcy_flow_multicomp.py`](../../code/sfepy/sfepy/examples/diffusion/darcy_flow_multicomp.py) | porous-media-flow | scenario-or-driver |
| [`sfepy/examples/diffusion/laplace_1d.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_1d.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/laplace_coupling_lcbcs.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_coupling_lcbcs.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/laplace_fluid_2d.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_fluid_2d.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/laplace_iga_interactive.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_iga_interactive.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/laplace_refine_interactive.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_refine_interactive.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/laplace_shifted_periodic.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_shifted_periodic.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/laplace_time_ebcs.py`](../../code/sfepy/sfepy/examples/diffusion/laplace_time_ebcs.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/poisson.py`](../../code/sfepy/sfepy/examples/diffusion/poisson.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/poisson_field_dependent_material.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_field_dependent_material.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/poisson_functions.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_functions.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/poisson_iga.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_iga.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/poisson_neumann.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_neumann.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/poisson_nonlinear_material.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_nonlinear_material.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/poisson_nonlinear_parametric.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_nonlinear_parametric.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/poisson_parallel_interactive.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_parallel_interactive.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/poisson_parametric_study.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_parametric_study.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/poisson_periodic_boundary_condition.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_periodic_boundary_condition.py) | transient-scalar-transport | scenario-or-driver |
| [`sfepy/examples/diffusion/poisson_short_syntax.py`](../../code/sfepy/sfepy/examples/diffusion/poisson_short_syntax.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/sinbc.py`](../../code/sfepy/sfepy/examples/diffusion/sinbc.py) | elliptic-diffusion | scenario-or-driver |
| [`sfepy/examples/diffusion/time_advection_diffusion.py`](../../code/sfepy/sfepy/examples/diffusion/time_advection_diffusion.py) | transient-scalar-transport | scenario-or-driver |
| [`sfepy/examples/diffusion/time_heat_equation_multi_material.py`](../../code/sfepy/sfepy/examples/diffusion/time_heat_equation_multi_material.py) | transient-scalar-transport | scenario-or-driver |
| [`sfepy/examples/diffusion/time_poisson.py`](../../code/sfepy/sfepy/examples/diffusion/time_poisson.py) | transient-scalar-transport | scenario-or-driver |
| [`sfepy/examples/diffusion/time_poisson_explicit.py`](../../code/sfepy/sfepy/examples/diffusion/time_poisson_explicit.py) | transient-scalar-transport | scenario-or-driver |
| [`sfepy/examples/diffusion/time_poisson_interactive.py`](../../code/sfepy/sfepy/examples/diffusion/time_poisson_interactive.py) | transient-scalar-transport | scenario-or-driver |
| [`sfepy/examples/homogenization/__init__.py`](../../code/sfepy/sfepy/examples/homogenization/__init__.py) | shared support / package initializer | support |
| [`sfepy/examples/homogenization/homogenization_opt.py`](../../code/sfepy/sfepy/examples/homogenization/homogenization_opt.py) | elastic-homogenization | support |
| [`sfepy/examples/homogenization/linear_elastic_mM.py`](../../code/sfepy/sfepy/examples/homogenization/linear_elastic_mM.py) | elastic-homogenization | scenario-or-driver |
| [`sfepy/examples/homogenization/linear_elasticity_opt.py`](../../code/sfepy/sfepy/examples/homogenization/linear_elasticity_opt.py) | elastic-homogenization | support |
| [`sfepy/examples/homogenization/linear_homogenization.py`](../../code/sfepy/sfepy/examples/homogenization/linear_homogenization.py) | elastic-homogenization | scenario-or-driver |
| [`sfepy/examples/homogenization/linear_homogenization_postproc.py`](../../code/sfepy/sfepy/examples/homogenization/linear_homogenization_postproc.py) | elastic-homogenization | support |
| [`sfepy/examples/homogenization/linear_homogenization_up.py`](../../code/sfepy/sfepy/examples/homogenization/linear_homogenization_up.py) | elastic-homogenization | scenario-or-driver |
| [`sfepy/examples/homogenization/material_opt.py`](../../code/sfepy/sfepy/examples/homogenization/material_opt.py) | elastic-homogenization | scenario-or-driver |
| [`sfepy/examples/homogenization/nonlinear_homogenization.py`](../../code/sfepy/sfepy/examples/homogenization/nonlinear_homogenization.py) | elastic-homogenization | scenario-or-driver |
| [`sfepy/examples/homogenization/nonlinear_hyperelastic_mM.py`](../../code/sfepy/sfepy/examples/homogenization/nonlinear_hyperelastic_mM.py) | elastic-homogenization | scenario-or-driver |
| [`sfepy/examples/homogenization/perfusion_micro.py`](../../code/sfepy/sfepy/examples/homogenization/perfusion_micro.py) | porous-media-flow | scenario-or-driver |
| [`sfepy/examples/homogenization/rs_correctors.py`](../../code/sfepy/sfepy/examples/homogenization/rs_correctors.py) | elastic-homogenization | scenario-or-driver |
| [`sfepy/examples/large_deformation/__init__.py`](../../code/sfepy/sfepy/examples/large_deformation/__init__.py) | shared support / package initializer | support |
| [`sfepy/examples/large_deformation/active_fibres.py`](../../code/sfepy/sfepy/examples/large_deformation/active_fibres.py) | nonlinear-solid-mechanics | scenario-or-driver |
| [`sfepy/examples/large_deformation/balloon.py`](../../code/sfepy/sfepy/examples/large_deformation/balloon.py) | shells-membranes | scenario-or-driver |
| [`sfepy/examples/large_deformation/compare_elastic_materials.py`](../../code/sfepy/sfepy/examples/large_deformation/compare_elastic_materials.py) | nonlinear-solid-mechanics | scenario-or-driver |
| [`sfepy/examples/large_deformation/gen_yeoh_tl_up_interactive.py`](../../code/sfepy/sfepy/examples/large_deformation/gen_yeoh_tl_up_interactive.py) | nonlinear-solid-mechanics | scenario-or-driver |
| [`sfepy/examples/large_deformation/hyperelastic.py`](../../code/sfepy/sfepy/examples/large_deformation/hyperelastic.py) | nonlinear-solid-mechanics | scenario-or-driver |
| [`sfepy/examples/large_deformation/hyperelastic_tl_up_interactive.py`](../../code/sfepy/sfepy/examples/large_deformation/hyperelastic_tl_up_interactive.py) | nonlinear-solid-mechanics | scenario-or-driver |
| [`sfepy/examples/large_deformation/hyperelastic_ul.py`](../../code/sfepy/sfepy/examples/large_deformation/hyperelastic_ul.py) | nonlinear-solid-mechanics | scenario-or-driver |
| [`sfepy/examples/large_deformation/hyperelastic_ul_by_fun.py`](../../code/sfepy/sfepy/examples/large_deformation/hyperelastic_ul_by_fun.py) | nonlinear-solid-mechanics | scenario-or-driver |
| [`sfepy/examples/large_deformation/hyperelastic_ul_up.py`](../../code/sfepy/sfepy/examples/large_deformation/hyperelastic_ul_up.py) | nonlinear-solid-mechanics | scenario-or-driver |
| [`sfepy/examples/large_deformation/perfusion_tl.py`](../../code/sfepy/sfepy/examples/large_deformation/perfusion_tl.py) | porous-media-flow | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/__init__.py`](../../code/sfepy/sfepy/examples/linear_elasticity/__init__.py) | shared support / package initializer | support |
| [`sfepy/examples/linear_elasticity/dispersion_analysis.py`](../../code/sfepy/sfepy/examples/linear_elasticity/dispersion_analysis.py) | elastic-spectra-band-gaps | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/elastic2D_axisymmetric.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastic2D_axisymmetric.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/elastic_contact_planes.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastic_contact_planes.py) | elastic-contact | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/elastic_contact_sphere.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastic_contact_sphere.py) | elastic-contact | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/elastic_shifted_periodic.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastic_shifted_periodic.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/elastodynamic.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastodynamic.py) | linear-elastodynamics | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/elastodynamic_identification.py`](../../code/sfepy/sfepy/examples/linear_elasticity/elastodynamic_identification.py) | linear-elastodynamics | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/its2D_1.py`](../../code/sfepy/sfepy/examples/linear_elasticity/its2D_1.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/its2D_2.py`](../../code/sfepy/sfepy/examples/linear_elasticity/its2D_2.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/its2D_3.py`](../../code/sfepy/sfepy/examples/linear_elasticity/its2D_3.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/its2D_4.py`](../../code/sfepy/sfepy/examples/linear_elasticity/its2D_4.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/its2D_5.py`](../../code/sfepy/sfepy/examples/linear_elasticity/its2D_5.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/its2D_interactive.py`](../../code/sfepy/sfepy/examples/linear_elasticity/its2D_interactive.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/linear_elastic.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/linear_elastic_damping.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_damping.py) | linear-elastodynamics | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/linear_elastic_iga.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_iga.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/linear_elastic_interactive.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_interactive.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/linear_elastic_probes.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_probes.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/linear_elastic_tractions.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_tractions.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/linear_elastic_up.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_elastic_up.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/linear_viscoelastic.py`](../../code/sfepy/sfepy/examples/linear_elasticity/linear_viscoelastic.py) | linear-viscoelasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/material_nonlinearity.py`](../../code/sfepy/sfepy/examples/linear_elasticity/material_nonlinearity.py) | nonlinear-solid-mechanics | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/mixed_mesh.py`](../../code/sfepy/sfepy/examples/linear_elasticity/mixed_mesh.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/modal_analysis.py`](../../code/sfepy/sfepy/examples/linear_elasticity/modal_analysis.py) | elastic-spectra-band-gaps | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/modal_analysis_declarative.py`](../../code/sfepy/sfepy/examples/linear_elasticity/modal_analysis_declarative.py) | elastic-spectra-band-gaps | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/multi_node_lcbcs.py`](../../code/sfepy/sfepy/examples/linear_elasticity/multi_node_lcbcs.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/multi_point_constraints.py`](../../code/sfepy/sfepy/examples/linear_elasticity/multi_point_constraints.py) | trusses-springs | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/nodal_lcbcs.py`](../../code/sfepy/sfepy/examples/linear_elasticity/nodal_lcbcs.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/prestress_fibres.py`](../../code/sfepy/sfepy/examples/linear_elasticity/prestress_fibres.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/rigid_twist.py`](../../code/sfepy/sfepy/examples/linear_elasticity/rigid_twist.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/seismic_load.py`](../../code/sfepy/sfepy/examples/linear_elasticity/seismic_load.py) | linear-elastodynamics | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/shell10x_cantilever.py`](../../code/sfepy/sfepy/examples/linear_elasticity/shell10x_cantilever.py) | shells-membranes | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/shell10x_cantilever_interactive.py`](../../code/sfepy/sfepy/examples/linear_elasticity/shell10x_cantilever_interactive.py) | shells-membranes | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/truss_bridge.py`](../../code/sfepy/sfepy/examples/linear_elasticity/truss_bridge.py) | trusses-springs | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/truss_bridge3d.py`](../../code/sfepy/sfepy/examples/linear_elasticity/truss_bridge3d.py) | trusses-springs | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/two_bodies_contact.py`](../../code/sfepy/sfepy/examples/linear_elasticity/two_bodies_contact.py) | elastic-contact | scenario-or-driver |
| [`sfepy/examples/linear_elasticity/wedge_mesh.py`](../../code/sfepy/sfepy/examples/linear_elasticity/wedge_mesh.py) | static-linear-elasticity | scenario-or-driver |
| [`sfepy/examples/miscellaneous/__init__.py`](../../code/sfepy/sfepy/examples/miscellaneous/__init__.py) | shared support / package initializer | support |
| [`sfepy/examples/miscellaneous/live_plot.py`](../../code/sfepy/sfepy/examples/miscellaneous/live_plot.py) | non-scientific logging demo | non-scientific-demo |
| [`sfepy/examples/miscellaneous/refine_evp.py`](../../code/sfepy/sfepy/examples/miscellaneous/refine_evp.py) | elastic-spectra-band-gaps | scenario-or-driver |
| [`sfepy/examples/multi_physics/__init__.py`](../../code/sfepy/sfepy/examples/multi_physics/__init__.py) | shared support / package initializer | support |
| [`sfepy/examples/multi_physics/biot.py`](../../code/sfepy/sfepy/examples/multi_physics/biot.py) | porous-media-flow | scenario-or-driver |
| [`sfepy/examples/multi_physics/biot_npbc.py`](../../code/sfepy/sfepy/examples/multi_physics/biot_npbc.py) | porous-media-flow | scenario-or-driver |
| [`sfepy/examples/multi_physics/biot_npbc_lagrange.py`](../../code/sfepy/sfepy/examples/multi_physics/biot_npbc_lagrange.py) | porous-media-flow | scenario-or-driver |
| [`sfepy/examples/multi_physics/biot_parallel_interactive.py`](../../code/sfepy/sfepy/examples/multi_physics/biot_parallel_interactive.py) | porous-media-flow | scenario-or-driver |
| [`sfepy/examples/multi_physics/biot_short_syntax.py`](../../code/sfepy/sfepy/examples/multi_physics/biot_short_syntax.py) | porous-media-flow | scenario-or-driver |
| [`sfepy/examples/multi_physics/piezo_elasticity.py`](../../code/sfepy/sfepy/examples/multi_physics/piezo_elasticity.py) | piezoelectricity | scenario-or-driver |
| [`sfepy/examples/multi_physics/piezo_elasticity_macro.py`](../../code/sfepy/sfepy/examples/multi_physics/piezo_elasticity_macro.py) | piezoelectricity | scenario-or-driver |
| [`sfepy/examples/multi_physics/piezo_elasticity_micro.py`](../../code/sfepy/sfepy/examples/multi_physics/piezo_elasticity_micro.py) | piezoelectricity | scenario-or-driver |
| [`sfepy/examples/multi_physics/piezo_elastodynamic.py`](../../code/sfepy/sfepy/examples/multi_physics/piezo_elastodynamic.py) | piezoelectricity | scenario-or-driver |
| [`sfepy/examples/multi_physics/thermal_electric.py`](../../code/sfepy/sfepy/examples/multi_physics/thermal_electric.py) | joule-heating | scenario-or-driver |
| [`sfepy/examples/multi_physics/thermo_elasticity.py`](../../code/sfepy/sfepy/examples/multi_physics/thermo_elasticity.py) | thermoelasticity | scenario-or-driver |
| [`sfepy/examples/multi_physics/thermo_elasticity_ess.py`](../../code/sfepy/sfepy/examples/multi_physics/thermo_elasticity_ess.py) | thermoelasticity | scenario-or-driver |
| [`sfepy/examples/navier_stokes/__init__.py`](../../code/sfepy/sfepy/examples/navier_stokes/__init__.py) | shared support / package initializer | support |
| [`sfepy/examples/navier_stokes/navier_stokes.py`](../../code/sfepy/sfepy/examples/navier_stokes/navier_stokes.py) | incompressible-flow | scenario-or-driver |
| [`sfepy/examples/navier_stokes/navier_stokes2d.py`](../../code/sfepy/sfepy/examples/navier_stokes/navier_stokes2d.py) | incompressible-flow | scenario-or-driver |
| [`sfepy/examples/navier_stokes/navier_stokes2d_iga.py`](../../code/sfepy/sfepy/examples/navier_stokes/navier_stokes2d_iga.py) | incompressible-flow | scenario-or-driver |
| [`sfepy/examples/navier_stokes/stabilized_navier_stokes.py`](../../code/sfepy/sfepy/examples/navier_stokes/stabilized_navier_stokes.py) | incompressible-flow | scenario-or-driver |
| [`sfepy/examples/navier_stokes/stokes.py`](../../code/sfepy/sfepy/examples/navier_stokes/stokes.py) | incompressible-flow | scenario-or-driver |
| [`sfepy/examples/navier_stokes/stokes_slip_bc.py`](../../code/sfepy/sfepy/examples/navier_stokes/stokes_slip_bc.py) | incompressible-flow | scenario-or-driver |
| [`sfepy/examples/navier_stokes/utils.py`](../../code/sfepy/sfepy/examples/navier_stokes/utils.py) | incompressible-flow | support |
| [`sfepy/examples/phononic/__init__.py`](../../code/sfepy/sfepy/examples/phononic/__init__.py) | shared support / package initializer | support |
| [`sfepy/examples/phononic/band_gaps.py`](../../code/sfepy/sfepy/examples/phononic/band_gaps.py) | elastic-spectra-band-gaps | scenario-or-driver |
| [`sfepy/examples/phononic/band_gaps_conf.py`](../../code/sfepy/sfepy/examples/phononic/band_gaps_conf.py) | elastic-spectra-band-gaps | support |
| [`sfepy/examples/phononic/band_gaps_rigid.py`](../../code/sfepy/sfepy/examples/phononic/band_gaps_rigid.py) | elastic-spectra-band-gaps | scenario-or-driver |
| [`sfepy/examples/quantum/__init__.py`](../../code/sfepy/sfepy/examples/quantum/__init__.py) | shared support / package initializer | support |
| [`sfepy/examples/quantum/boron.py`](../../code/sfepy/sfepy/examples/quantum/boron.py) | single-particle-quantum | scenario-or-driver |
| [`sfepy/examples/quantum/hydrogen.py`](../../code/sfepy/sfepy/examples/quantum/hydrogen.py) | single-particle-quantum | scenario-or-driver |
| [`sfepy/examples/quantum/oscillator.py`](../../code/sfepy/sfepy/examples/quantum/oscillator.py) | single-particle-quantum | scenario-or-driver |
| [`sfepy/examples/quantum/quantum_common.py`](../../code/sfepy/sfepy/examples/quantum/quantum_common.py) | single-particle-quantum | support |
| [`sfepy/examples/quantum/well.py`](../../code/sfepy/sfepy/examples/quantum/well.py) | single-particle-quantum | scenario-or-driver |

## Next review

Approve, regroup or defer the proposed boundaries. The original static module may proceed separately under its existing source approval, but this planning PR creates no task. Limited-example modules require explicit evidence and any necessary THIN/custom agreement later. Flexoelectricity remains deferred until an official physical case is found or custom testing is explicitly approved.

Packaging pipeline: **5.11.10**. This file is rendered from `codebase-metadata.json`; JSON, HTML and bounded Markdown were regenerated with the repository CLI.
