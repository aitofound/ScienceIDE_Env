<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `consistent-kmedian` | CLI |
| source payload | `code/consistent-kmedian/` | CLI |
| upstream pin | `a4df7ea97daf40e4e790dc718f923c8b934610c2` | human/state |
| license | `MIT` | human/state |
| source fingerprint | `3785075bcc6fc821962296f7b95cbb08d55f78ffcf1d77dfb4b40739be5d9d5c` | CLI |
| size | 2 files / 7871 bytes / 200 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `consistent-kmedian` | approved | {"note": "not supplied", "status": "unknown"} | 2 | 200 | 0 | unknown |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | unknown | ["consistent-kmedian"] | 0 | 0 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 0 | 0 | 0 |
| owned | 2 | 7871 | 200 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 0 | 0 | 0 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 0 | files |
| `test_definitions` | 0 | source-level test definitions |
| `collected_items` | 0 | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- thin official-test coverage: exactly one demo script, no isolated unit tests
- no upstream repository or reference implementation to validate the pin or the port against
- should later custom checks target internal routines (cost_p, _best_swap, _num_outliers) individually, or only end-to-end demo output?
- CLI: codebase.upstream_url: local/private absolute path redacted
- CLI: repository/cache directory excluded from source accounting: .git

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
