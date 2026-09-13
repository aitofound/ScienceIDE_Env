<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `openfoam` | CLI |
| source payload | `code/openfoam/` | CLI |
| upstream pin | `28ce4a2776f63c8d677c5df93c41bb5d4a012768` | human/state |
| license | `GPL-3.0` | human/state |
| source fingerprint | `987f3def12868e804e6eeb76ad7a283f7a66bf3f141bc2c3c05b6bbd5d032f0e` | CLI |
| size | 17612 files / 166189681 bytes / 4465402 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `icofoam-incompressible-cavity` | approved | The module is distinguished by the incompressible laminar equations and icoFoam entrypoint; other solver families use different equations, models or numerical stages. | 28 | 1319 | 1 | `shared-infrastructure` |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | unknown | ["icofoam-incompressible-cavity"] | 4479 | 752805 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 4479 | 21583642 | 752805 |
| owned | 28 | 30858 | 1319 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 13105 | 144575181 | 3711278 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 24 | files |
| `test_definitions` | 1 | source-level test definitions |
| `collected_items` | 1 | framework-collected items |
| `inner_cases` | 1 | inner cases |

### Gaps and warnings
- The first short run used the official development image binary against the pinned tutorial inputs; a source build of the pinned commit is still required before final task self-validation.
- GPL-3.0 vendoring compatibility is a maintainer decision.
- Confirm whether the curator wants the Foundation OpenFOAM-dev line or a tagged release for the source PR.
- Confirm whether the base cavity case alone is sufficiently broad for a first module or whether cavityClipped/cavityGrade should be included after calibration.
- CLI: 13105 regular file(s) are unclassified; this is visible but non-blocking
- CLI: repository/cache directory excluded from source accounting: .git

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
