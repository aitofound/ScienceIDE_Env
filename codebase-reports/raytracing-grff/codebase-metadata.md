<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `raytracing-grff` | CLI |
| source payload | `code/raytracing-grff/` | CLI |
| upstream pin | `3d0306f0cf2484475ee9c7b0a33230eeae1c6330` | human/state |
| license | `Unknown: upstream contains no license file or license metadata` | human/state |
| source fingerprint | `237c8634e2e61e32eaec95e028ac2faf9c2367ad450ad7dfdbd5c2474bf6c3c6` | CLI |
| size | 32 files / 295095 bytes / 7592 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `ray-transport` | approved | Only proposed module; external emission synthesis consumes its samples and is deferred. | 8 | 1892 | 15 | `package-support` |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `package-support` | Package assembly, imports, GRFF loading and image-processing helpers | ["ray-transport"] | 4 | 325 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 4 | 10844 | 325 |
| owned | 8 | 65626 | 1892 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 20 | 218625 | 5375 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 3 | files |
| `test_definitions` | 15 | source-level test definitions |
| `collected_items` | 15 | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- No upstream license file or declaration
- Existing CPU/CUDA parity failure
- numpy-only packaging declaration omits scipy and matplotlib imports
- MAS data and GRFF shared library absent
- Upstream machine-specific paths preserved for provenance
- Workflow scripts outside proposed module intentionally unclassified
- Which license should the owner declare?
- Which MAS dataset and GRFF pin should support emission tasks?
- How should invalid samples be represented consistently on CPU and CUDA?
- CLI: 20 regular file(s) are unclassified; this is visible but non-blocking

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
