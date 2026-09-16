<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `mfem` | CLI |
| source payload | `code/mfem/` | CLI |
| upstream pin | `d964264cdb9a13e94a201b6c236c7721e0c8765f` | human/state |
| license | `BSD-3-Clause` | human/state |
| source fingerprint | `42aff6eb82a5d1c5f1ab48c84a7275f11834d3cb4c82899d38519ce187beaade` | CLI |
| size | 1594 files / 25383953 bytes / 821648 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `high-order-assembly` | proposed-only | This is the only module that owns the sum-factorized kernels of scalar and vector H1 and L2 operators, and the only one that owns the restriction and quadrature-interpolation laye… | 54 | 27066 | 129 | `shared-infrastructure` |
| `hcurl-hdiv-electromagnetics` | proposed-only | It is the only module that builds a curl or a div operator, the only one that owns vector-valued finite element bases whose degrees of freedom are moments on edges and faces rathe… | 25 | 27431 | 67 | `shared-infrastructure` |
| `dg-hyperbolic-transport` | proposed-only | It is the only module whose expensive work is a face loop rather than an element loop, the only one that owns a conservation-law flux interface, and the only one whose operator of… | 21 | 10009 | 8 | `shared-infrastructure` |
| `adaptive-mesh-refinement` | proposed-only | It is the only module that changes the mesh topology, and the only one that owns the hanging-node constraint machinery every space on a nonconforming mesh depends on. It does not … | 28 | 24057 | 58 | `shared-infrastructure` |
| `mesh-optimization-tmop` | proposed-only | It is the only module that treats the mesh node positions as unknowns and moves them; every other module treats the mesh as given. It answers a mesh-quality question rather than a… | 53 | 19601 | 2 | `shared-infrastructure` |
| `solvers-and-preconditioners` | proposed-only | It is the only module that owns the iteration rather than the operator: it consumes a discrete operator from any other module and produces a solution. It never assembles a physica… | 43 | 18883 | 27 | `shared-infrastructure` |
| `nurbs-isogeometric` | proposed-only | It is the only module whose basis is rational and spans knot spans rather than elements, the only one whose geometry is exact rather than approximated by the same basis as the sol… | 42 | 21919 | 11 | `shared-infrastructure` |
| `nonlinear-solvers-and-time-integration` | proposed-only | It is the only module that owns a residual-and-Jacobian interface and the only one that advances a state in time. The physics it carries, hyperelasticity, nonlinear heat conductio… | 15 | 7885 | 5 | `shared-infrastructure` |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | Everything every module links against: the device, memory-manager, container and communication layer of general/; the vector, dense-matrix and sparse-matrix core of linalg/ with i… | ["high-order-assembly", "hcurl-hdiv-electromagnetics", "dg-hyperbolic-transport", "adaptive-mesh-refinement", "mesh-optimization-tmop", "solvers-and-preconditioners", "nurbs-isoge… | 363 | 199775 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 363 | 5802877 | 199775 |
| owned | 281 | 5155560 | 156851 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 950 | 14425516 | 465022 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 157 | files |
| `test_definitions` | 573 | source-level test definitions |
| `collected_items` | 410 | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- Upstream ships no reference output for any example or miniapp. `make test` checks exit codes only, so no check can be a file diff against upstream; every check must be anchored on what the run writes (.mesh and .gf files) or prints (a final L2 error against an exact solution). The Catch2 unit tests are the exception and carry their own assertions.
- Vector::Randomize() called with no argument seeds libc srand() from time(0), and 38 call sites across the unit tests do exactly that. Measured on the investigation host, four consecutive processes drew different values while an explicitly seeded draw was bit-identical. Nine of the affected files lie inside the proposed cut. Because the stream is libc rand(), even a seeded sequence is reproducible only within one C library.
- Default example and miniapp problem sizes run in one to two seconds and are far below an accelerator workload. Refinement, order and final time are the knobs every check will have to raise, and the acceleration check in particular.
- Adaptive counts are pervasive: Krylov iterations, Newton iterations, line-search backtracks and adaptive time-step counts are all printed and all respond to rounding. None can enter a graded set.
- Discrete decisions taken on floating-point comparisons are the dominant correctness hazard of the cut: the refinement marker against a threshold, the TMOP line search on the sign of a Jacobian determinant, and the active set of the variational-inequality examples.
- The investigation ran on one arm64 macOS host with Apple clang. No x86-64 or glibc measurement exists yet, and libc rand() differs between the two, so any seeded random sequence will differ.
- No Docker image was built, no tolerance was finalized and no task was scaffolded in this phase.
- Serial or MPI? The serial build needs no external library and is what was measured. Enabling MPI requires hypre and METIS and would roughly double the official-test inventory, restoring the electromagnetics miniapps, the Navier fluids solver, the hooke solid-mechanics miniapp and the parallel half of every example. The whole proposal above assumes serial.
- mesh-optimization-tmop is thin in a serial build: five official invocations and two unit cases. Keep it and record that it is thin, or enable MPI to bring in pmesh-optimizer and its sample runs?
- The proposal splits fem/integ/ by file across four modules, so that each owns the partial-assembly kernels of its own operator family rather than leaving them in shared infrastructure. The curator has previously ruled on PRs #613, #615 and #616 that a module must be a genuinely independent, repository-like component rather than a subsystem or check-family label. The three assembly modules (high-order-assembly, hcurl-hdiv-electromagnetics, dg-hyperbolic-transport) are where that ruling bites har…
- Should tests/unit/catch.hpp and general/tinyxml2.cpp, both upstream-vendored third-party sources inside the pin, be called out separately in the licence record?
- CLI: 950 regular file(s) are unclassified; this is visible but non-blocking
- CLI: no approved module cut is available; report remains informational

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
