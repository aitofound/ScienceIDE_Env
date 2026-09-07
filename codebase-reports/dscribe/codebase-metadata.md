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
| `soap` | approved | SOAP forms rotationally invariant power spectra from a neighbour-density basis expansion. Unlike structure matrices it is local to requested centers; unlike MBTR and ACSF its domi… | 8 | 7080 | 122 | `shared-infrastructure` |
| `mbtr-family` | approved | This family produces broadened many-body histograms on explicit grids and shares one principal C++ accumulator across MBTR and LMBTR; Valle-Oganov configures MBTR for a materials-… | 5 | 2977 | 160 | `shared-infrastructure` |
| `acsf` | approved | ACSF evaluates fixed radial and angular functions rather than a SOAP basis expansion or MBTR grid; its derivative path is numerical. | 3 | 745 | 35 | `shared-infrastructure` |
| `structure-matrices` | approved | These are global pair-interaction matrices rather than local or histogram representations. Ewald adds a distinct periodic summation workload while retaining the common matrix post… | 6 | 1254 | 78 | `shared-infrastructure` |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | Provides descriptor base classes, atomic-system conversion, parallel execution, geometry and neighbour-list utilities, the pybind11 bridge, common C++ descriptor classes, weightin… | ["soap", "mbtr-family", "acsf", "structure-matrices"] | 1859 | 400691 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 1859 | 15692514 | 400691 |
| owned | 22 | 859924 | 12056 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 1420 | 108924377 | 301390 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 11 | files |
| `test_definitions` | 159 | source-level test definitions |
| `collected_items` | 415 | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- All four proposed modules have human approval; task calibration beyond SOAP has not yet been performed.
- This source report does not establish task check suitability, policies, tolerances, rewards, or speedups.
- Cross-platform numerical variation remains unknown until task calibration.
- Which official behaviours in each module should become self-contained checks?
- Which policies and tolerances will be approved after calibration?
- CLI: 1420 regular file(s) are unclassified; this is visible but non-blocking

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
