<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `mrsimulator` | CLI |
| source payload | `code/mrsimulator/` | CLI |
| upstream pin | `ded4cd5f85e5c1fbd8207d84d932bc310e093c1c` | human/state |
| license | `BSD-3-Clause` | human/state |
| source fingerprint | `227ba2352ca739b906765db368a41310898a3939e7642b8c19849532f6d154f5` | CLI |
| size | 668 files / 8388028 bytes / 136006 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `solid-state-nmr-simulation` | approved | Only proposed module; all numerical stages contribute to the same simulated spectrum observable. | 133 | 31094 | 95 | `shared-infrastructure` |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | Package build configuration, common utilities, isotope metadata and tensor/data-model helpers used by the simulation path. | ["solid-state-nmr-simulation"] | 22 | 4482 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 22 | 152056 | 4482 |
| owned | 133 | 1069575 | 31094 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 513 | 7166397 | 100430 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 30 | files |
| `test_definitions` | 95 | source-level test definitions |
| `collected_items` | 95 | framework-collected items |
| `inner_cases` | 389 | inner cases |

### Gaps and warnings
- A Linux/OpenBLAS run should be used by CI/review to confirm cross-platform floating-point tolerances.
- Whether a later task should target the Python transition/frequency path or the native C interpolation kernel first.
- CLI: 513 regular file(s) are unclassified; this is visible but non-blocking

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
