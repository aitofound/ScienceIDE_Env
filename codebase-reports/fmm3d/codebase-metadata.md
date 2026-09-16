<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `fmm3d` | CLI |
| source payload | `code/fmm3d/` | CLI |
| upstream pin | `d2b5e6e983e1ec915d15330ce74df44221a9ac42` | human/state |
| license | `Apache-2.0` | human/state |
| source fingerprint | `adb0e58828f9bc00202794cc9608b10b1df9b0c1b74062becc04f4d89997fbec` | CLI |
| size | 296 files / 14898891 bytes / 430179 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `fmm3d` | approved | Single whole-codebase module; all internal shared infrastructure belongs to this root. One complete library at the vendored root, keeping every shipped physics kernel and language… | 296 | 430179 | unknown | unknown |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | unknown | ["fmm3d"] | 0 | 0 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 0 | 0 | 0 |
| owned | 296 | 14898891 | 430179 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 0 | 0 | 0 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | unknown | files |
| `test_definitions` | unknown | source-level test definitions |
| `collected_items` | unknown | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- Only four Fortran suites were run in the short investigation; the full interface/example inventory and framework collection remain pending.
- Complete the official-test and standard-example survey after the source PR is merged.

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
