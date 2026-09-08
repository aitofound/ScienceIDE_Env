<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `dscribe` | CLI |
| source payload | `code/dscribe/` | CLI |
| upstream pin | `0b62a970e1230a2b011341383609111a26f86362` | human/state |
| license | `Apache-2.0` | human/state |
| source fingerprint | `089ca38e79dceccd65ca6460cd70c74144ccd467684e086f53f4f2d42f96c7de` | CLI |
| size | 3301 files / 125476815 bytes / 714137 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `descriptors` | proposed-only | The families differ in representation and dominant kernel, but jointly constitute the public descriptor subsystem and share a larger common compiled/runtime foundation than any th… | 21 | 11636 | 395 | `descriptor-support` |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `descriptor-support` | Provides descriptor base classes, atomic-system conversion, parallel execution, geometry and neighbour-list utilities, the pybind11 bridge, common C++ descriptor classes, weightin… | ["descriptors"] | 1859 | 400691 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 1859 | 15692514 | 400691 |
| owned | 21 | 765186 | 11636 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 1421 | 109019115 | 301810 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 11 | files |
| `test_definitions` | 159 | source-level test definitions |
| `collected_items` | 415 | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- The revised one-module descriptor proposal is not yet approved by the curator.
- This source report does not authorize task scaffolding, checks, rewards, tolerances, or runs.
- Cross-platform numerical variation remains unknown at the source-proposal stage.
- Does the curator approve the combined descriptors module, its 11,636 owned source-like lines, and the supporting/shared accounting?
- CLI: 1421 regular file(s) are unclassified; this is visible but non-blocking
- CLI: no approved module cut is available; report remains informational

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
