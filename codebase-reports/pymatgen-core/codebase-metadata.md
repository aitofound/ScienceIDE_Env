<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `pymatgen-core` | CLI |
| source payload | `code/pymatgen-core/` | CLI |
| upstream pin | `73af4e53f5f24e1dcf11e0d94ca13be10ea956ad` | human/state |
| license | `MIT` | human/state |
| source fingerprint | `fd630cbf79a1536a7406cd878e3497c839b9b80d83327ea5b7c5898cde039821` | CLI |
| size | 299 files / 13373096 bytes / 206637 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `pymatgen-phase-diagram-thermodynamics` | approved | Solid-state composition-energy convex hull; includes compound, grand-potential, patched and reaction utilities. | 1 | 4620 | 110 | `shared-infrastructure` |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | Shared composition, structure, geometry and test infrastructure. NumPy, SciPy and monty are external numerical/serialization dependencies. Umbrella pymatgen also requires separate… | ["pymatgen-phase-diagram-thermodynamics"] | 98 | 48435 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 98 | 2550388 | 48435 |
| owned | 1 | 191868 | 4620 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 200 | 10630840 | 153582 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 1 | files |
| `test_definitions` | 110 | source-level test definitions |
| `collected_items` | 110 | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- Windows lacked MSVC; both filtered pinned source packages subsequently built successfully on native Ubuntu. All 137 selected tests passed; the CPU-wide Pourbaix multiprocessing test remains deselected.
- Scientific check selection and calibration follow source review. Upstream fixture provenance is recorded; no blanket claim is made about unrelated upstream datasets.
- CLI: 200 regular file(s) are unclassified; this is visible but non-blocking

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
