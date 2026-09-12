<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `pluto` | CLI |
| source payload | `code/pluto/` | CLI |
| upstream pin | `sha256:1ba5527b76d49fdd78ae24dbfbdad085ec83393748f1e618516a9d63bd945787` | human/state |
| license | `GPL-2.0` | human/state |
| source fingerprint | `unknown` | CLI |
| size | unknown files / unknown bytes / unknown text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `pluto-cooling-chemistry` | approved | 16 packaged checks (not upstream-test counts): Src/Cooling: TABULATED, POWER_LAW, SNEq, MINEq, H2_COOL; radiative jets and task-owned cooling/chemistry decks | unknown | unknown | unknown | `shared-infrastructure` |
| `pluto-hd-diffusion` | approved | 20 packaged checks (not upstream-test counts): Src/HD, Src/Viscosity, Src/Thermal_Conduction, STS and RKL; viscous cylinder/Couette, conduction fronts/blasts, inviscid HD problems | unknown | unknown | unknown | `shared-infrastructure` |
| `pluto-mhd-les` | approved | 15 packaged checks (not upstream-test counts): Classical MHD; CT, GLM, eight-wave, resistive and Hall terms, FARGO and shearing box | unknown | unknown | unknown | `shared-infrastructure` |
| `pluto-particles-dust` | approved | 16 packaged checks (not upstream-test counts): CR MHD-PIC: Bell instability, gyration, relative drift, X-point; particle push/interpolation/feedback and output | unknown | unknown | unknown | `shared-infrastructure` |
| `pluto-rhd-radiation` | approved | 19 packaged checks (not upstream-test counts): RHD and M1 radiation, relativistic and non-relativistic matter coupling; shock tubes, blasts, pulse/shadow, disk-planet checks | unknown | unknown | unknown | `shared-infrastructure` |
| `pluto-rmhd-resrmhd` | approved | 20 packaged checks (not upstream-test counts): Shipped PHYSICS=RMHD configurations: Riemann solvers, primitive recovery, divergence control, tubes/blasts/waves/jets | unknown | unknown | unknown | `shared-infrastructure` |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | Shared initialization, grid/geometry, boundary conditions, reconstruction/time stepping, parallelism, EOS, math, output and setup tooling. | ["pluto-hd-diffusion", "pluto-mhd-les", "pluto-cooling-chemistry", "pluto-particles-dust", "pluto-rhd-radiation", "pluto-rmhd-resrmhd"] | unknown | unknown |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | unknown | unknown | unknown |
| owned | unknown | unknown | unknown |
| overlapping_owned | unknown | unknown | unknown |
| unclassified | unknown | unknown | unknown |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | unknown | files |
| `test_definitions` | unknown | source-level test definitions |
| `collected_items` | unknown | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- The six shipped tasks contain 106 check.json files (16 cooling, 20 HD diffusion, 15 MHD, 16 particles, 19 RHD/radiation, 20 RMHD). These are packaged checks, not upstream official-test counts or new execution results.
- Cooling has 16 checks (3 official Jet configurations plus 13 task-owned decks), not the narrative 17; RHD/radiation has 19, not the narrative 20. Task metadata is unchanged.
- The particles-dust slug does not establish dust coverage: non-CR particles and DUST_FLUID are explicitly excluded by its module record.
- All RMHD nominal/variant physics definitions select PHYSICS=RMHD; resistive-relativistic coverage is not established by the rmhd-resrmhd slug.
- The MHD les slug and declared path do not establish a distinct LES validation family. The report describes actual MHD checks.
- AMR is a codebase-level citation, not evidence of Chombo execution. Shipped module records exclude Chombo AMR builds where noted.
- Source file/byte/line counts, ownership/overlap, source fingerprint, total upstream official tests, newly measured native performance and accelerator speedups remain unknown. Existing task timing prose was not reproduced.
- The active a100-sxm4-80gb descriptors are placeholders, not evidence of measured accelerator execution; no scientific tests were rerun.
- The original bibliography verification recorded an upstream website timeout. Pinned source headers and independent DOI/Crossref/arXiv records supplied the evidence; no unobserved CITATION/CFF file or software DOI is claimed.
- Two unrelated task DOI attributions (Reale conduction and RMHD HLLD) are corrected only in the preserved bibliography. Structured verification/correction records are retained in official_tests.summary.bibliography.
- The original pre-creation inventory mapped no open PR to pluto, and its live search returned no matches; this is a historical snapshot, not a claim that no Pluto PR is open now.
- CLI: Evidence-backed canonical backfill, not a new CLI source-accounting or performance run; null means unknown, not zero.
- CLI: The generated module table shows official collected tests as unknown; 106 shipped check descriptors are recorded separately.
- CLI: Native and accelerator execution were not run for this bibliography/report-only update.

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
