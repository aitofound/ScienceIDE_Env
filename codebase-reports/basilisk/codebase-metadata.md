<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `basilisk` | CLI |
| source payload | `code/basilisk/` | CLI |
| upstream pin | `441418f2f4e12bba56e6148fc037bcace44e8ee5` | human/state |
| license | `ISC` | human/state |
| source fingerprint | `48fec0d4c284e07147039af41b3048046bf470a624ccca3568acf04c66d6b717` | CLI |
| size | 3818 files / 326345194 bytes / 2180974 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `spacecraft-reaction-wheel-dynamics` | approved | One module spans the coupled hub and wheel equations; orbital, torque-free and wheel-actuated scenarios are regimes, not separate modules. Other Basilisk FSW, sensing and flexible… | 67 | 12043 | unknown | `shared-infrastructure` |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | Message transport, scheduling, state registration/integration, Python model factories and build/binding infrastructure shared by the selected dynamics and other Basilisk modules. | ["spacecraft-reaction-wheel-dynamics"] | 467 | 71445 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 467 | 11120745 | 71445 |
| owned | 67 | 8957852 | 12043 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 3284 | 306266597 | 2097486 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 394 | files |
| `test_definitions` | 861 | source-level test definitions |
| `collected_items` | unknown | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- Native build failed before compilation because no C++ compiler was found; source-built scenarios and all timings remain unmeasured. The curator explicitly approved an early source-only Draft with this limitation visible.
- No Docker execution, finalized task policy, official task verification or accelerator implementation is included.
- The complete 326 MB payload includes optional assets and unrelated modules; the planned runtime does not use SPICE ephemerides or large-data downloads, but CSPICE and CFITSIO remain unconditional Conan dependencies.
- The balanced-wheel momentum reconstruction is source-derived but has not been cross-checked experimentally against the upstream logger.
- There is no measured evidence yet that a small single-wheel trajectory is a worthwhile timing workload.
- Which relevant upstream configurations provide a meaningful acceleration workload without expanding the agreed scientific scope?
- Can a source build with the optional features disabled run the required examples without pulling large support data?
- Which physical trajectory outputs and independent analytic/conservation checks should be finalized after native investigation and calibration?
- CLI: 3284 regular file(s) are unclassified; this is visible but non-blocking

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
