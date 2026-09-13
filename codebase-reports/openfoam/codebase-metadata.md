<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `openfoam` | CLI |
| source payload | `code/openfoam/` | CLI |
| upstream pin | `28ce4a2776f63c8d677c5df93c41bb5d4a012768` | human/state |
| license | `GPL-3.0` | human/state |
| source fingerprint | `b6a4b3507e1b1c5098316a93a8187efbdb9ea0bb859219c76a5cf7eeb08dc1bd` | CLI |
| size | 17612 files / 166189681 bytes / 4465402 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `openfoam` | approved | This is the whole-codebase module required by the current pipeline rule; individual solver families are downstream coverage choices. | 17612 | 4465402 | 1 | unknown |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | unknown | ["openfoam"] | 0 | 0 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 0 | 0 | 0 |
| owned | 17612 | 166189681 | 4465402 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 0 | 0 | 0 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 4 | files |
| `test_definitions` | 1 | source-level test definitions |
| `collected_items` | 1 | framework-collected items |
| `inner_cases` | 1 | inner cases |

### Gaps and warnings
- The first short run used the official development image binary; a source build of the pinned commit remains a review item.
- GPL-3.0 vendoring compatibility is a maintainer decision.
- Confirm that #710 accepts the whole-codebase module framing and GPL-3.0 source.

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
