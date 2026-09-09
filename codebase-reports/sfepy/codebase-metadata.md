<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `sfepy` | CLI |
| source payload | `code/sfepy/` | CLI |
| upstream pin | `3f01a19fad86d14c1d54706372fe591f8f7bf46c` | human/state |
| license | `BSD-3-Clause` | human/state |
| source fingerprint | `0d5b224b41575aa2a1d59bb05c1cf511d2047998a916b9ee3a162a9d47848e1f` | CLI |
| size | 940 files / 25113522 bytes / 978151 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `static-linear-elasticity` | approved | Only one module is proposed. Time evolution, hereditary material response, contact, reduced-dimensional structures and multiphysics require different contracts and are excluded ev… | 22 | 4757 | 18 | `shared-infrastructure` |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | Mesh/topology and basis evaluation, DOF and constraint handling, sparse assembly and linear solvers, material/tensor helpers, Python/Cython term bindings, command-line and solve_p… | ["static-linear-elasticity"] | 203 | 74287 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 203 | 2430675 | 74287 |
| owned | 22 | 142262 | 4757 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 715 | 22540585 | 899107 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 50 | files |
| `test_definitions` | 149 | source-level test definitions |
| `collected_items` | 219 | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- Sparse solver ordering and floating-point reduction may change rounding; grade physical node/cell identities, not internal DOF order or iteration counts.
- Mesh refinement, nearly incompressible material parameters and point loads affect conditioning and stress convergence.
- terms_elastic.py contains excluded history, wave and truss classes; owning the file does not extend the physics scope.
- Some optional examples or backends need packages beyond the baseline; record missing dependencies and unmeasured cases explicitly.
- The native investigation is one Linux x86_64 build; cross-platform equivalence and task policies remain unmeasured.
- Only one module is proposed; the rest of the complete upstream snapshot is deliberately not assigned to task modules.
- Contributor authorization is the user response to the SfePy linear-elasticity proposal: 我觉得没问题，你先vendor codebase. The exact file boundary is an agent-authored elaboration; organizer source/module review remains pending.
- No Docker execution or accelerator implementation was performed.
- Does the organizer accept the proposed static continuum elasticity boundary and file-granular ownership?
- Which mesh/order makes the later acceleration workload scientifically worthwhile? This source investigation did not profile or measure speedups.
- Which additional shared-term tests and static example configurations belong in the later complete official-test survey?
- CLI: 715 regular file(s) are unclassified; this is visible but non-blocking

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
