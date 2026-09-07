<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `tsid` | CLI |
| source payload | `code/tsid/` | CLI |
| upstream pin | `591f737435f4f84be7844c9b6c59ec1a8792a738` | human/state |
| license | `BSD-2-Clause` | human/state |
| source fingerprint | `5baa6032779e270bc81dba12457fd0313923eb0133f328245ade128d480ee70a` | CLI |
| size | 316 files / 37102784 bytes / 56860 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `talos-fixed-contact-inverse-dynamics` | approved | Only one module is proposed: task-space objectives are coupled in one controller; individual robot poses and reaching scenarios are not separate modules. | 18 | 2919 | unknown | `shared-infrastructure` |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | Common robot dynamics, trajectories, spatial algebra, bindings, build configuration and base classes support the selected controller and other upstream examples. | ["talos-fixed-contact-inverse-dynamics"] | 101 | 8283 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 101 | 301644 | 8283 |
| owned | 18 | 98881 | 2919 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 197 | 36702259 | 45658 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 16 | files |
| `test_definitions` | unknown | source-level test definitions |
| `collected_items` | unknown | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- Native source build and short official-test runs are pending; this source draft does not complete the native investigation.
- Nine C++ and seven Python entries are registered in source; four additional Python scripts are commented out and are not counted as registered.
- Repeated HQP assembly and solution is the candidate expensive path; no profiling evidence is available.
- Historical LGPL notices remain alongside the root BSD-2-Clause license.
- Verify TALOS model and frame conventions on the pinned runtime.
- Determine a reproducible native dependency set, including pinned JRL CMake modules.
- Design the later independent validator within the repository standard-library and NumPy boundary.
- CLI: 197 regular file(s) are unclassified; this is visible but non-blocking

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
