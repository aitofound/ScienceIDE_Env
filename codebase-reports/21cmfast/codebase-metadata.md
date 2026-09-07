<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `21cmfast` | CLI |
| source payload | `code/21cmfast/` | CLI |
| upstream pin | `2cb6000d61381c658ccbe68028c74ca6b0c46cdc` | human/state |
| license | `MIT` | human/state |
| source fingerprint | `85840def371e5833fc2fc566b1afb6ba9750f02a93c864d3ae56c8aaffa78a79` | CLI |
| size | 301 files / 64553533 bytes / 588272 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `initial-conditions-perturbed-fields` | approved | This is the only proposed module in the current contribution. It stops at matter density and peculiar velocity; downstream halo and baryonic-radiative evolution are intentionally … | 4 | 1291 | 23 | `shared-infrastructure` |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | Defines and validates input parameters, allocates typed output grids, initializes global C state, exposes the Python production API through CFFI, and supplies FFT, filtering, inde… | ["initial-conditions-perturbed-fields"] | 26 | 8917 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 26 | 351878 | 8917 |
| owned | 4 | 55359 | 1291 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 271 | 64146296 | 578064 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 31 | files |
| `test_definitions` | 294 | source-level test definitions |
| `collected_items` | 1006 | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- Only one intentionally scoped module is proposed, so most of the vendored repository is visibly unclassified by this module cut.
- The native investigation used one arm64 macOS host; Linux and accelerator numerical behaviour remain for later task calibration and review.
- The complete upstream suite was collected but not executed; execution evidence is limited to the approved module's short tests.
- Whether reviewers want additional raw-grid conservation summaries alongside upstream's spectrum/PDF regression diagnostics.
- Whether the high-resolution integration configuration is sufficient coverage for the upstream-skipped high-resolution 2LPT synthetic-roll test.
- CLI: 271 regular file(s) are unclassified; this is visible but non-blocking

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
