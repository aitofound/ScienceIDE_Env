<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `degree-bounded-steiner` | CLI |
| source payload | `code/degree-bounded-steiner/` | CLI |
| upstream pin | `4215571745635840e5e504c0a48ea02a777cfe6f` | human/state |
| license | `MIT` | human/state |
| source fingerprint | `40550dafb7816b160510730fff47d9b72d34982c7daa89a552613f9c2cb6867c` | CLI |
| size | 2 files / 15250 bytes / 400 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `degree-bounded-steiner` | approved | not supplied | 2 | 400 | 0 | unknown |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | unknown | ["degree-bounded-steiner"] | 0 | 0 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 0 | 0 | 0 |
| owned | 2 | 15250 | 400 |
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
- thin official-test coverage: exactly one demo script exercising each part once, no isolated unit tests
- no upstream repository or reference implementation to validate the pin or a future port against
- DBGSTSolver depends on scipy.optimize.linprog(method="highs"); worth a pointwise-vs-invariants read at STOP 4 given the LP solver is an external numerical dependency
- should later custom checks target the LP construction, power-of-2 rounding, rescaling, or recursive-rounding steps individually, or only end-to-end demo output for each part?
- CLI: codebase.upstream_url: local/private absolute path redacted
- CLI: modules.degree-bounded-steiner.excluded[1]: local/private absolute path redacted
- CLI: repository/cache directory excluded from source accounting: .git

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
