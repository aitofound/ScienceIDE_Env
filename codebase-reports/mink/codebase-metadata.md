<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `mink` | CLI |
| source payload | `code/mink/` | CLI |
| upstream pin | `14625beca2ce0918f88d1fc84a3c0cdb591e0729` | human/state |
| license | `Apache-2.0` | human/state |
| source fingerprint | `fcd6d9544ed7c368a0936ffdc1b5597f12a569eda3120fca830e7d1509fa69e3` | CLI |
| size | 772 files / 415770537 bytes / 5754304 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `mink-constrained-differential-ik` | approved | Only one module is proposed. It groups robot state, objective construction, constraints, QP solve and integration because they form the reusable differential-IK pipeline. Lie math… | 8 | 1456 | 159 | `shared-infrastructure` |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | SE(3) and SO(3) representations, exponential/logarithmic maps, adjoints and Jacobians; optional C hot-path implementations with Python fallbacks; joint-width constants, exceptions… | ["mink-constrained-differential-ik"] | 15 | 2074 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 15 | 67825 | 2074 |
| owned | 8 | 53156 | 1456 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 749 | 415649556 | 5750774 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 23 | files |
| `test_definitions` | 248 | source-level test definitions |
| `collected_items` | 262 | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- Source presence and Git byte identity establish provenance, not correctness of every upstream algorithm or model.
- Model-backed official tests beyond the executed subset need dependency pins and native execution evidence before task packaging.
- The source-built fallback and official binary wheel were exercised; local compilation of the C extension and Linux image builds have not been performed.
- Files outside the eight owned files and shared infrastructure are intentionally visible as unclassified, including official tests, examples, model assets, documentation and additional implementations.
- Which external robot model snapshots must be bundled in a follow-up source update for reproducible execution of all relevant official tests?
- Which runtime environment can provide the required compiler, public dependencies and model assets for the subsequent task build?
- CLI: 749 regular file(s) are unclassified; this is visible but non-blocking
- CLI: modules.mink-constrained-differential-ik.hazards[0]: local/private absolute path redacted

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
