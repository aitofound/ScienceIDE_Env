<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `hmmer` | CLI |
| source payload | `code/hmmer/` | CLI |
| upstream pin | `9acd8b6758a0ca5d21db6d167e0277484341929b` | human/state |
| license | `BSD-3-Clause` | human/state |
| source fingerprint | `5c3210952cd39fea2427a2971b25726e407dbe716beb93178bbb6e127930bbe0` | CLI |
| size | 892 files / 35736328 bytes / 435343 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `hmmsearch-profile-search` | approved | This module is separated by the hmmsearch production CLI contract, not by an internal function family. It consumes a profile HMM and a protein sequence database and owns every sta… | 38 | 37738 | 58 | `shared-infrastructure` |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | Easel provides alphabets, digital sequence and alignment I/O, option parsing, random-number generation, vector helpers, work queues, and other utilities used by hmmsearch and its … | ["hmmsearch-profile-search"] | 531 | 297903 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 531 | 17341697 | 297903 |
| owned | 38 | 1507421 | 37738 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 323 | 16887210 | 99702 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | unknown | files |
| `test_definitions` | 394 | source-level test definitions |
| `collected_items` | 394 | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- The complete release is intentionally preserved, so files outside the approved module and named shared component remain visibly unclassified rather than being deleted or assigned artificial ownership.
- Native measurement used the NEON backend on arm64. The likely x86_64 execution host will select SSE, so target-host calibration must measure backend-sensitive score and threshold spread.
- The shipped biological fixtures are small. The fixed-seed generated 40-million-residue probe established the expensive full-pipeline path but is diagnostic only and is not an approved check.
- Potential module candidates exceed fifty when formatting exercises, component drivers, regressions, and examples are all counted. The post-merge survey must classify every item individually rather than cap the suite arbitrarily.
- After source merge, which fixed and provenance-clean production-scale target database should anchor acceleration measurement without introducing an unreviewed download?
- After the official-test survey, does the suitable scientific-check count remain above fifty and therefore require a further human decision on module cut or check organization?
- CLI: 323 regular file(s) are unclassified; this is visible but non-blocking

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
