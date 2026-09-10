<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `quantum-espresso` | CLI |
| source payload | `code/quantum-espresso/` | CLI |
| upstream pin | `9f93ddec427d2b9a45bb72d828c6d324f62fcabd` | human/state |
| license | `GPL-2.0-or-later` | human/state |
| source fingerprint | `8d9c8a23ff17990a57fba437aaacc23bb82b17bd28114efc06de6d1ceb26eb9a` | CLI |
| size | 18183 files / 665601736 bytes / 11337666 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `pw-ground-state` | approved | This is the only module that computes a ground state; every other module consumes one. It owns PW/src apart from the five exact-exchange files, and KS_Solvers, which no other modu… | 305 | 129014 | 232 | `shared-infrastructure` |
| `pw-hybrid-exx` | approved | It owns only the five exx* files of PW/src and nothing else. It cannot run without the semi-local SCF of pw-ground-state, but the semi-local SCF never enters this code, so the bou… | 5 | 8608 | 23 | `shared-infrastructure` |
| `ph-linear-response` | approved | It never converges a ground state and never computes a total energy of its own; it linearises around one. It owns PHonon/, which no other module touches, and shares LR_Modules wit… | 218 | 56577 | 93 | `shared-infrastructure` |
| `cp-car-parrinello` | approved | It replaces the SCF fixed point with a dynamics, so it shares neither the diagonalisers nor the mixing of pw-ground-state. It owns CPV/src alone. | 108 | 50021 | unknown | `shared-infrastructure` |
| `tddfpt-linear-response` | approved | It answers a spectroscopic question rather than a structural one, and it never builds a dynamical matrix. It shares LR_Modules with ph-linear-response but owns TDDFPT/src alone. | 62 | 22120 | unknown | `shared-infrastructure` |
| `hp-hubbard-parameters` | approved | It computes a parameter of a model Hamiltonian rather than an observable of the system, which makes its output a small formatted table rather than a field. It owns HP/src alone. | 46 | 7451 | unknown | `shared-infrastructure` |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | Everything every module links against: the QE Modules layer (cell, ions, k-points, symmetry, pseudopotential handling, I/O and the XML schema writer), the FFT driver library FFTXl… | ["pw-ground-state", "pw-hybrid-exx", "ph-linear-response", "cp-car-parrinello", "tddfpt-linear-response", "hp-hubbard-parameters"] | 11453 | 6563537 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 11453 | 353387956 | 6563537 |
| owned | 744 | 9331061 | 273791 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 5986 | 302882719 | 4500338 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 356 | files |
| `test_definitions` | 359 | source-level test definitions |
| `collected_items` | 359 | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- The vendored payload is 634.8 MB and 18,182 files, dominated by external/wannier90 (196 MB) and external/d3q (127 MB), neither of which the CPU recipe compiles. They are kept because the vendoring policy is to keep the tree complete apart from files over 10 MB.
- Build times were measured on one host only (WSL2 Ubuntu 22.04, gfortran 11.4.0, OpenBLAS): configure 7 s, serial make pw 167 s, make ph a further 58 s, inside a network namespace with no interfaces.
- Step timings were taken with ten test directories running concurrently on a 24-core host; they are honest for selection but are not single-tenant benchmark numbers.
- cp_*, tddfpt_* and hp_* official tests were not run in this phase; their runtimes are unknown.
- No Docker image was built, no tolerance was finalised and no task was scaffolded in this phase.
- archive/sa-0004 currently has status = retired. Should its status be flipped now that the package is returning in the current leaf format, or should it stay archived with the lineage recorded only here? archive/ has not been touched.
- CURATOR-DECISIONS section 3 names PW/src/exx_band.f90, which does not exist in 7.6. The exact-exchange set in this release is exx.f90, exx_base.f90, exx_bp.f90, exx_bp_utils.f90 and exx_std.f90; the proposal uses the five files that exist.
- CURATOR-DECISIONS section 3 names CMake/ and MBD/ as shared infrastructure. Neither is a directory of the 7.6 tree: the CMake modules are in cmake/ and MBD/ is a build-output directory generated from external/mbd. The proposal lists external/ instead.
- The two zero-byte stamp files install/git_devx and install/git_mbd are the only additions to the upstream tree. Does the curator prefer them, or a documented pre-build step in run.sh instead?
- CLI: 5986 regular file(s) are unclassified; this is visible but non-blocking

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
