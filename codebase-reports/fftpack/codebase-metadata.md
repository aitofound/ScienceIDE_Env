<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `fftpack` | CLI |
| source payload | `code/fftpack/` | CLI |
| upstream pin | `ab983f0b16170b018770ec3bf70102c55f3fa3d8` | human/state |
| license | `UCAR BSD-style (LICENSE)` | human/state |
| source fingerprint | `fe91cf354864b354ef81f49069e86c559f1ccf5509486cbb8a442adbe17119fa` | CLI |
| size | 135 files / 1142623 bytes / 18662 text lines | CLI |

### Build and run (Step 1.2: what was actually built and run natively)

Build: `make (GNU make; one top-level Makefile per library, each including a make.inc that selects the compiler by `uname -s`; src/ builds a static library, test/ compiles and immediately runs every test program)`, ok, 12 s; commands: `make MAKE=make AR=ar F90="gfortran -fdefault-real-8 -O2 -std=legacy -J LIBDIR -I LIBDIR" CPP="gfortran -cpp -E"   (doub…`; `make MAKE=make AR=ar F90="gfortran -O2 -std=legacy -J LIBDIR -I LIBDIR" CPP="gfortran -cpp -E"   (single precision, the…`.
- build pitfall: make.inc chooses the compiler from `uname -s`: on Linux it wants pgf90 or g95, on Darwin gfortran but MAKE := the hard-coded gmake path, which does not exist on current macOS; override MAKE=make AR=ar F90=... on the make command line (all five libraries)
- build pitfall: gfortran 15.2 (and every gfortran >= 10) refuses the Fortran 77 argument-type and rank mismatches in FFTPACK, FISHPACK and SPHEREPACK ("Type mismatch between actual argument ... REAL(8) vs COMPLEX(8)", "Rank mismatch in argument theta") unless -std=legacy (or -fallow-argument-mismatch) is given; measured: fftpack 12 errors, spherepack 2 errors without it
- build pitfall: the shipped darwin.dp transcripts were produced with -fdefault-real-8; the libraries are written for single precision and the darwin.sp build reproduces fewer digits (FISHPACK 1 to 4 digits, MUDPACK 0 to 2) and mudpack tcud3cr does not converge in single precision (ierror -10 here, ierror -4 and NaN in the shipped darwin.sp)
- build pitfall: wall_s is the double-precision `make` run, which includes the 8 test runs the test Makefile executes (under 2 s in total); the single-precision build took the same

| family | kind | decks | how to run | references |
|---|---|---:|---|---|
| fftpack-test | test-suite | 8 | `make (the test Makefile compiles each t*.f against lib/libfftpack.a and runs it)` | shipped |

Actually run: 8 of 8 attempted.

| run | ran | wall s | reproduced / reason | pitfalls |
|---|---|---:|---|---|
| `tcfft1` | yes | 0.06 | not compared: random test vectors (unseeded RANDOM_SEED); residual 5e-16 to 8e-16 here, 6e-16 to 8e-16 in darwin.dp | - |
| `trfft1` | yes | 0.03 | not compared: random test vectors (unseeded RANDOM_SEED); residual 5e-16 to 8e-16 here, 6e-16 to 8e-16 in darwin.dp | - |
| `tcosq1` | yes | 0.03 | not compared: random test vectors (unseeded RANDOM_SEED); residual 5e-16 to 8e-16 here, 6e-16 to 8e-16 in darwin.dp | - |
| `tcost1` | yes | 0.03 | not compared: random test vectors (unseeded RANDOM_SEED); residual 5e-16 to 8e-16 here, 6e-16 to 8e-16 in darwin.dp | - |
| `tsinq1` | yes | 0.03 | not compared: random test vectors (unseeded RANDOM_SEED); residual 5e-16 to 8e-16 here, 6e-16 to 8e-16 in darwin.dp | - |
| `tsint1` | yes | 0.03 | not compared: random test vectors (unseeded RANDOM_SEED); residual 5e-16 to 8e-16 here, 6e-16 to 8e-16 in darwin.dp | - |
| `tcfft2` | yes | 0.03 | not compared: random test vectors (unseeded RANDOM_SEED); residual 5e-16 to 8e-16 here, 6e-16 to 8e-16 in darwin.dp | - |
| `trfft2` | yes | 0.04 | not compared: random test vectors (unseeded RANDOM_SEED); residual 5e-16 to 8e-16 here, 6e-16 to 8e-16 in darwin.dp | - |

Pitfalls of running the codebase: 4
- [build] make: pgf90/g95 not found (Linux) or the hard-coded gmake path not found (macOS) -> make MAKE=make AR=ar F90="gfortran -fdefault-real-8 -O2 -std=legacy -J LIBDIR -I LIBDIR" CPP="gfortran -cpp -E"
- [build] gfortran >= 10: Error: Type mismatch between actual argument ... (REAL(8) vs COMPLEX(8)); Error: Rank mismatch in argument theta -> -std=legacy (or -fallow-argument-mismatch)
- [tcfft1] every run prints a different max error (5e-16 to 8e-16) -> the tests call RANDOM_SEED() without arguments; a check must fix its input vector (or seed) itself and grade the round-trip error as an invariant
- [general] every test program writes only to stdout; the test Makefile pattern rule runs each executable right after linking it (the pattern rule executes the program it just linked), so `make` in test/ is build plus run and output/darwin.dp is the whole transcript -> run the .exe files again by hand to capture per-test stdout

Not run:
- reference transcripts aix.dp/sp and linux.pgf90.dp/sp: other platforms; compared against darwin.dp only

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `fftpack` | approved | single whole-codebase module | 135 | 18662 | 8 | unknown |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | unknown | ["fftpack"] | 0 | 0 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 0 | 0 | 0 |
| owned | 135 | 1142623 | 18662 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 0 | 0 | 0 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 8 | files |
| `test_definitions` | 8 | source-level test definitions |
| `collected_items` | 8 | framework-collected items |
| `inner_cases` | 16 | inner cases |

### Gaps and warnings
- the vendored tree is the extracted tarball, not a git checkout, so the review CLI cannot diff it against an upstream checkout; VENDORING.md carries the tarball SHA-256
- fix a deterministic input for every FFT check (a seeded generator or a stored vector), since the drivers use RANDOM_SEED() with no argument
- whether to add checks for the untested multiple-sequence routines from the documented calling sequences

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
