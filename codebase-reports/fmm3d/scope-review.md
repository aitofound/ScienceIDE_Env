## Codebase

FMM3D evaluates long-range interactions among points in three dimensions for wave propagation, electrostatics, viscous flow and electromagnetism. Its fast multipole routines are useful to researchers whose direct all-pairs calculations are too expensive.

Upstream: [FMM3D](https://github.com/flatironinstitute/FMM3D). Pin: `d2b5e6e983e1ec915d15330ce74df44221a9ac42`. License: **Apache-2.0**, with bundled component notices preserved.

This source PR vendors the approved whole-codebase scope and its metadata/bibliography. Task checks and calibration follow in a separate task PR.

## Size and vendored dependencies

Language counts use cloc 1.90 on the complete vendored tree. LOC means nonblank, noncomment lines; documentation/data languages are included. cloc skips binary/unrecognized/duplicate files, so its file total differs from the exact Git-entry total.

| Language | Counted files | LOC |
|---|---:|---:|
| Fortran 77 | 82 | 170,781 |
| C/C++ Header | 38 | 13,528 |
| C | 10 | 12,279 |
| MATLAB | 26 | 2,863 |
| Python | 11 | 2,620 |
| Fortran 90 | 7 | 2,231 |
| reStructuredText | 10 | 1,700 |
| Julia | 4 | 1,429 |
| make | 3 | 553 |
| YAML | 5 | 158 |
| C++ | 2 | 149 |
| Meson | 1 | 137 |
| Markdown | 7 | 135 |
| Windows Module Definition | 1 | 130 |
| TOML | 3 | 129 |
| Bourne Shell | 7 | 42 |
| TeX | 1 | 39 |
| HTML | 1 | 13 |
| CSS | 1 | 3 |
| **Total** | **220** | **208,919** |

The exact payload contains 296 Git blob entries and 14,898,891 bytes, including 0 preserved symbolic links. No dependency repository or scientific code was added beyond the pinned parent tree.

## Build and official tests

GNU Make, GCC/GFortran 11.4, make -j2 lib: 33.227 s from a fresh scratch tree; official test compilation a further 1.556 s. Default OpenMP, native CPU optimization, ordinary Fortran kernels (FAST_KER not enabled).

2 Python files containing 2 source-level test definitions; 19 native test source files under test/. Framework-collected item totals and nested/parameterized case counts remain unknown until the complete survey. 14 example source/notebook files were inventoried (counts may include helpers or multiple language versions, not unique scientific problems).

Four official Fortran programs passed: Helmholtz 18/18 internal cases (2.324 s), Laplace 27/27 (2.224 s), Stokes 6/6 (0.518 s), Maxwell 6/6 (1.271 s). Maximum printed relative errors were respectively 2.2553e-4, 4.9168e-5, 1.1290e-12 and 1.3495e-4. These are direct-sum comparisons at the upstream settings, not a final benchmark tolerance. The two Python tests, C/Julia/MATLAB tests and remaining native programs were not run.

| Native official test | Run command (from a built source root) | Wall time | Result |
|---|---|---:|---|
| `test/Maxwell/int2-test-emfmm3d` | `test/Maxwell/int2-test-emfmm3d` | 1.271 s | Passed |
| `test/Helmholtz/int2-test-hfmm3d` | `test/Helmholtz/int2-test-hfmm3d` | 2.324 s | Passed |
| `test/Laplace/int2-test-lfmm3d` | `test/Laplace/int2-test-lfmm3d` | 2.224 s | Passed |
| `test/Stokes/int2-test-stfmm3d` | `test/Stokes/int2-test-stfmm3d` | 0.518 s | Passed |

Official suite/example locations: `test/Helmholtz`, `test/Laplace`, `test/Stokes`, `test/Maxwell`, `python/test`, `examples`, `c`, `julia`, `matlab`. These include unrun inventory; the full collection and suitability survey remains pending. Investigation environment: Ubuntu 22.04 WSL x86-64, Python 3.12.14, GCC/GFortran 11.4, OMP_NUM_THREADS=2 and OPENBLAS_NUM_THREADS=1, with each test capped at 180 seconds.

FMM3D emits textual numerical-error/pass summaries. No benchmark output contract or repeatability/alternative-build campaign has been authored or run. Passing once does not establish reproducibility across compilers or devices.

## Approved module cut

| Slug / title | What it computes | Owned paths | LOC | Expensive path | Official tests | Status |
|---|---|---|---:|---|---|---|
| `fmm3d` / FMM3D scientific interaction library | FMM3D evaluates long-range interactions among points in three dimensions for wave propagation, electrostatics, viscous flow and electromagnetism. | `code/fmm3d/` (whole root) | 208,919 | Adaptive tree passes, multipole translations and direct near-neighbour interactions across the shipped physics kernels. | `test/Helmholtz`, `test/Laplace`, `test/Stokes`, `test/Maxwell`, `python/test`, `examples`, `c`, `julia`, `matlab` | Approved |

All shipped physics capabilities and language interfaces stay in the single module. Internal numerical schemes are workloads within that scope. Approval recorded **2026-09-12 (America/Los_Angeles)**:

> Approve the 3 tasks as whole codebase tasks, and submit the PRs, leave the CAMB for now

Scientific and execution hazards:

- Tree decisions and OpenMP reductions can change floating-point accumulation; compare physical outputs against requested accuracy, not iteration order.
- Preserve Apache-2.0 and all bundled component notices. Fortran, C/C++, Python, MATLAB and Julia interfaces need a full survey; only native Fortran suites were run in this investigation.
- The upstream test programs report pass counts in text and may exit successfully even on a numerical failure; their result summaries must be checked.

## Shared infrastructure and exclusions

Cross-module shared infrastructure: **0 separately allocated files / 0 LOC**. All internal shared code is owned by the single root module. Source exclusions: **none**. Unrun optional features and examples remain survey work; they are not dropped from scope. Dependency notices and upstream authors are retained.

## Submitter declaration

**Jingxu Xie — UC Berkeley — GitHub @jingxuxie.** Task submitter and packager of publicly available upstream code; not an upstream author or maintainer. Final equivalence criteria remain subject to domain review in the later task PR. Original copyright, author and licence notices are preserved.
