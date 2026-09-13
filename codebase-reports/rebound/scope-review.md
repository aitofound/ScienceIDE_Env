## Codebase

REBOUND integrates gravitational N-body systems and their response to changes in initial conditions. It supports planetary dynamics and related particle simulations through multiple integrators, collision models and variational equations.

Upstream: [REBOUND](https://github.com/hannorein/rebound). Pin: `33549d1d50d616a95a6d6a79e5e2c9c3b3730b1f`. License: **GPL-3.0-only**, with bundled component notices preserved.

This source PR vendors the approved whole-codebase scope and its metadata/bibliography. Task checks and calibration follow in a separate task PR.

## Size and vendored dependencies

Language counts use cloc 1.90 on the complete vendored tree. LOC means nonblank, noncomment lines; documentation/data languages are included. cloc skips binary/unrecognized/duplicate files, so its file total differs from the exact Git-entry total.

| Language | Counted files | LOC |
|---|---:|---:|
| C | 92 | 24,403 |
| Python | 82 | 10,704 |
| C/C++ Header | 34 | 5,709 |
| Markdown | 38 | 2,945 |
| Jupyter Notebook | 41 | 2,391 |
| Assembly | 1 | 796 |
| YAML | 12 | 612 |
| HTML | 3 | 586 |
| make | 15 | 397 |
| Bourne Again Shell | 7 | 143 |
| TOML | 2 | 76 |
| JavaScript | 1 | 15 |
| CSS | 1 | 10 |
| **Total** | **329** | **48,787** |

The exact payload contains 401 Git blob entries and 10,213,725 bytes, including 2 preserved symbolic links. No dependency repository or scientific code was added beyond the pinned parent tree.

## Build and official tests

Python editable source build with setuptools and GCC 11.4: 10.649 s in a fresh scratch tree.

43 Python files containing 366 source-level test definitions; 0 native test source files under test/. Framework-collected item totals and nested/parameterized case counts remain unknown until the complete survey. 111 example source/notebook files were inventoried (counts may include helpers or multiple language versions, not unique scientific problems).

Six official unittest methods passed (0.167-0.368 s each): IAS15 outer-solar-system integration, its small-initial-step variant, a hyperbolic orbit, full-system first-order variational equations, test-particle first-order variational equations, and WHFast. IAS15 energy drift satisfied the upstream bound below 1e-14; the variational finite-difference differences satisfied 1e-5. Those are upstream assertions, not final port tolerances. The other integrators, collisions, MPI/display/network examples and long runs were not evaluated.

| Native official test | Run command (from a built source root) | Wall time | Result |
|---|---|---:|---|
| `rebound.tests.test_integrator.TestIntegrator.test_ias15` | `python -m unittest rebound.tests.test_integrator.TestIntegrator.test_ias15 -v` | 0.367 s | Passed |
| `rebound.tests.test_integrator.TestIntegrator.test_ias15_small_initial_dt` | `python -m unittest rebound.tests.test_integrator.TestIntegrator.test_ias15_small_initial_dt -v` | 0.368 s | Passed |
| `rebound.tests.test_integrator.TestIntegratorWHFastHyper.test_ias_veryhyperbolic` | `python -m unittest rebound.tests.test_integrator.TestIntegratorWHFastHyper.test_ias_veryhyperbolic -v` | 0.217 s | Passed |
| `rebound.tests.test_variational.TestVariational.test_all_1st_order_full` | `python -m unittest rebound.tests.test_variational.TestVariational.test_all_1st_order_full -v` | 0.218 s | Passed |
| `rebound.tests.test_variational.TestVariationalTestParticle.test_all_1st_order_full` | `python -m unittest rebound.tests.test_variational.TestVariationalTestParticle.test_all_1st_order_full -v` | 0.167 s | Passed |
| `rebound.tests.test_integrator.TestIntegrator.test_whfast_smalldt` | `python -m unittest rebound.tests.test_integrator.TestIntegrator.test_whfast_smalldt -v` | 0.217 s | Passed |

Official suite/example locations: `rebound/tests`, `examples`, `python_examples`, `ipython_examples`. These include unrun inventory; the full collection and suitability survey remains pending. Investigation environment: Ubuntu 22.04 WSL x86-64, Python 3.12.14, GCC/GFortran 11.4, OMP_NUM_THREADS=2 and OPENBLAS_NUM_THREADS=1, with each test capped at 180 seconds.

REBOUND exposes numerical arrays in process and its official tests emit assertion summaries. No benchmark output contract or repeatability/alternative-build campaign has been authored or run. Passing once does not establish reproducibility across compilers or devices.

## Approved module cut

| Slug / title | What it computes | Owned paths | LOC | Expensive path | Official tests | Status |
|---|---|---|---:|---|---|---|
| `rebound` / REBOUND N-body dynamics library | REBOUND integrates gravitational N-body systems and their response to changes in initial conditions. | `code/rebound/` (whole root) | 48,787 | Gravitational forces, numerical integration and variational equations across independent systems. | `rebound/tests`, `examples`, `python_examples`, `ipython_examples` | Approved |

All shipped physics capabilities and language interfaces stay in the single module. Internal numerical schemes are workloads within that scope. Approval recorded **2026-09-12 (America/Los_Angeles)**:

> Approve the 3 tasks as whole codebase tasks, and submit the PRs, leave the CAMB for now

Scientific and execution hazards:

- Adaptive step counts and particle storage order are bookkeeping rather than physical answers. Particle arrays must be aligned by stable identity before comparison.
- Chaotic trajectories and collision boundaries may amplify rounding. Some examples require network access, MPI, a display or AVX-512; these need explicit survey decisions.
- GPL-3.0-only; preserve all original authors and notices. The 366 source-level Python test definitions, inherited/generated cases and official examples may lead to more than fifty suitable checks.

## Shared infrastructure and exclusions

Cross-module shared infrastructure: **0 separately allocated files / 0 LOC**. All internal shared code is owned by the single root module. Source exclusions: **none**. Unrun optional features and examples remain survey work; they are not dropped from scope. Dependency notices and upstream authors are retained.

## Submitter declaration

**Jingxu Xie — UC Berkeley — GitHub @jingxuxie.** Task submitter and packager of publicly available upstream code; not an upstream author or maintainer. Final equivalence criteria remain subject to domain review in the later task PR. Original copyright, author and licence notices are preserved.
