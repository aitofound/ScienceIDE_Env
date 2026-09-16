<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `pyxsim` | CLI |
| source payload | `code/pyxsim/` | CLI |
| upstream pin | `e21e7fb782174e2e88253c6fb1de24f90f4f45dc` | human/state |
| license | `BSD-3-Clause` | human/state |
| source fingerprint | `ed51359e9c41040ecd1ee03e3d82356ff3544b0c3731aa5263866e1cf1f8fc76` | CLI |
| size | 90 files / 3700108 bytes / 14826 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `thermal-spectral-synthesis` | approved | Unlike analytic line and power-law sources, this module depends on tabulated atomic physics and performs high-dimensional table interpolation and abundance composition before spec… | 7 | 1993 | 13 | `shared-infrastructure` |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | Public package exports, unit parsing, progress handling, geometry/relativistic helpers, and the SourceModel lifecycle used by thermal source implementations. | ["thermal-spectral-synthesis"] | 5 | 908 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 5 | 30603 | 908 |
| owned | 7 | 75547 | 1993 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 78 | 3593958 | 11925 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 9 | files |
| `test_definitions` | 22 | source-level test definitions |
| `collected_items` | 21 | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- The initial native investigation ran on Apple ARM without a CUDA device, so no GPU implementation or transfer behavior was measured.
- Only the two APEC files needed by measured tests were downloaded; Cloudy/PION, yt cosmology, sloshing, and answer-test assets remain unmeasured.
- The upstream build-isolation path selected NumPy 2.5.2 and failed on this host's Accelerate framework; the official NumPy-1 CI path built successfully.
- Per-test peak memory was not measured during Step 1.
- Whether the first packaged check should retain the full 128^3 beta-model workload or use an official-test-derived smaller grid that preserves the same spectral and Doppler operations.
- Whether final validation should be pointwise only or combine pointwise spectral shape with integrated photon/energy flux invariants to allow GPU reduction-order differences.
- CLI: 78 regular file(s) are unclassified; this is visible but non-blocking

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
