<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `athenak` | CLI |
| source payload | `code/athenak/` | CLI |
| upstream pin | `c5a0d7f9155a70149931bf0be5a4ffb673f2532a` | human/state |
| license | `BSD-3-Clause` | human/state |
| source fingerprint | `54dfb8601444412a021344bbdbb617327eeeb853921282d1042f823c578a3c6f` | CLI |
| size | 2084 files / 22566788 bytes / 478614 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `newtonian-fluid-mhd` | approved | Evolves Newtonian fluid and magnetic states rather than relativistic states, radiation moments, the spacetime metric, or a standalone elliptic potential. | 69 | 14789 | 18 | `shared-infrastructure` |
| `relativistic-fluid-radiation` | approved | Uses prescribed Minkowski or curved coordinates; unlike numerical-relativity-grmhd it does not evolve the spacetime metric. | 20 | 4530 | 81 | `shared-infrastructure` |
| `numerical-relativity-grmhd` | approved | Evolves the spacetime degrees of freedom and their coupling to matter, rather than treating the metric as fixed. | 35 | 9891 | 16 | `shared-infrastructure` |
| `self-gravity-multigrid` | approved | Solves an elliptic Poisson problem with multilevel convergence criteria rather than an explicit hyperbolic evolution alone. | 9 | 8285 | 3 | `shared-infrastructure` |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | Kokkos execution, CMake configuration, mesh and AMR management, boundary exchange, coordinates, common EOS machinery, reconstruction, scheduling, I/O, problem-generator registrati… | ["newtonian-fluid-mhd", "relativistic-fluid-radiation", "numerical-relativity-grmhd", "self-gravity-multigrid"] | 1947 | 440717 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 1947 | 21108568 | 440717 |
| owned | 133 | 1444084 | 37495 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 4 | 14136 | 402 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 64 | files |
| `test_definitions` | unknown | source-level test definitions |
| `collected_items` | 119 | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- GPU and MPI numerical behavior was not measured on the macOS investigation host.
- Repeat-run bitwise determinism was not measured.
- The particle implementation has no dedicated official regression script in this pin.
- Later task calibration must choose device-appropriate tolerances from repeated Docker selfchecks rather than copying upstream CPU thresholds blindly.
- CLI: 4 regular file(s) are unclassified; this is visible but non-blocking

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
