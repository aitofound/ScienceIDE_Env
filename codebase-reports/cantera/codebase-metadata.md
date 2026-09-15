<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `cantera` | CLI |
| source payload | `code/cantera/` | CLI |
| upstream pin | `4a8358eb80cfeb50474386b5f9ec0b3a83519889` | human/state |
| license | `BSD-3-Clause` | human/state |
| source fingerprint | `389106e7431911487677dfbbb2a9b44d4afd4264e95ac7bc4335ed2e59539b58` | CLI |
| size | 5993 files / 85710035 bytes / 1719217 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `cantera` | approved | Single whole-codebase module; all internal shared infrastructure belongs to this root. Use one whole-codebase module because thermodynamics, kinetics, transport and reactor/flame … | 5993 | 1719217 | unknown | unknown |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | unknown | ["cantera"] | 0 | 0 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 0 | 0 | 0 |
| owned | 5993 | 85710035 | 1719217 |
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
- Only six transport tests were executed; other physics suites, optional HDF5, external data and full framework collection remain pending.
- All pinned submodule sources are expanded into the payload, including dependency tests and examples. The parent inventory does not count their tests.
- The whole-codebase scope may require more than fifty suitable official tests; no suitable test is dropped because of a count or timing target.
- Complete the official-test and standard-example survey after the source PR is merged.

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
