<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `mudpack` | CLI |
| source payload | `code/mudpack/` | CLI |
| upstream pin | `ab983f0b16170b018770ec3bf70102c55f3fa3d8` | human/state |
| license | `UCAR BSD-style (LICENSE)` | human/state |
| source fingerprint | `33506887e777841465d1185820768c2fa23a41dc21bc47f92f4c19837f73c5ac` | CLI |
| size | 102 files / 3829968 bytes / 124606 text lines | CLI |

### Build and run (Step 1.2: what was actually built and run natively)

Build: `make (GNU make; one top-level Makefile per library, each including a make.inc that selects the compiler by `uname -s`; src/ builds a static library, test/ compiles and immediately runs every test program)`, ok, 53 s; commands: `make MAKE=make AR=ar F90="gfortran -fdefault-real-8 -O2 -std=legacy -J LIBDIR -I LIBDIR" CPP="gfortran -cpp -E"   (doub…`; `make MAKE=make AR=ar F90="gfortran -O2 -std=legacy -J LIBDIR -I LIBDIR" CPP="gfortran -cpp -E"   (single precision, the…`.
- build pitfall: make.inc chooses the compiler from `uname -s`: on Linux it wants pgf90 or g95, on Darwin gfortran but MAKE := the hard-coded gmake path, which does not exist on current macOS; override MAKE=make AR=ar F90=... on the make command line (all five libraries)
- build pitfall: gfortran 15.2 (and every gfortran >= 10) refuses the Fortran 77 argument-type and rank mismatches in FFTPACK, FISHPACK and SPHEREPACK ("Type mismatch between actual argument ... REAL(8) vs COMPLEX(8)", "Rank mismatch in argument theta") unless -std=legacy (or -fallow-argument-mismatch) is given; measured: fftpack 12 errors, spherepack 2 errors without it
- build pitfall: the shipped darwin.dp transcripts were produced with -fdefault-real-8; the libraries are written for single precision and the darwin.sp build reproduces fewer digits (FISHPACK 1 to 4 digits, MUDPACK 0 to 2) and mudpack tcud3cr does not converge in single precision (ierror -10 here, ierror -4 and NaN in the shipped darwin.sp)
- build pitfall: wall_s is the double-precision `make` run, which includes the 36 test runs the test Makefile executes (under 2 s in total); the single-precision build took the same

| family | kind | decks | how to run | references |
|---|---|---:|---|---|
| mudpack-test | test-suite | 36 | `make` | shipped |

Actually run: 36 of 36 attempted.

| run | ran | wall s | reproduced / reason | pitfalls |
|---|---|---:|---|---|
| `tcud2` | yes | 0.04 | yes: all 7 printed values identical to darwin.dp (16 digits) | - |
| `tcud24` | yes | 0.03 | yes: all 8 printed values identical to darwin.dp (16 digits) | - |
| `tcud24cr` | yes | 0.04 | yes: all 8 printed values identical to darwin.dp (16 digits) | - |
| `tcud24sp` | yes | 0.04 | yes: all 7 printed values identical to darwin.dp (16 digits) | - |
| `tcud2cr` | yes | 0.04 | yes: all 6 printed values identical to darwin.dp (16 digits) | - |
| `tcud2sp` | yes | 0.04 | yes: all 6 printed values identical to darwin.dp (16 digits) | - |
| `tcud3` | yes | 0.06 | yes: all 9 printed values identical to darwin.dp (16 digits) | - |
| `tcud34` | yes | 0.09 | yes: all 11 printed values identical to darwin.dp (16 digits) | - |
| `tcud34sp` | yes | 0.05 | yes: all 9 printed values identical to darwin.dp (16 digits) | - |
| `tcud3cr` | yes | 0.06 | yes: all 19 printed values identical to darwin.dp (16 digits) | - |
| `tcud3sp` | yes | 0.05 | yes: all 8 printed values identical to darwin.dp (16 digits) | - |
| `tcuh2` | yes | 0.03 | yes: all 7 printed values identical to darwin.dp (16 digits) | - |
| `tcuh24` | yes | 0.04 | yes: all 8 printed values identical to darwin.dp (16 digits) | - |
| `tcuh24cr` | yes | 0.04 | yes: all 7 printed values identical to darwin.dp (16 digits) | - |
| `tcuh2cr` | yes | 0.04 | yes: all 6 printed values identical to darwin.dp (16 digits) | - |
| `tcuh3` | yes | 0.04 | yes: all 9 printed values identical to darwin.dp (16 digits) | - |
| `tcuh34` | yes | 0.05 | yes: all 10 printed values identical to darwin.dp (16 digits) | - |
| `tmud2` | yes | 0.03 | yes: all 7 printed values identical to darwin.dp (16 digits) | - |
| `tmud24` | yes | 0.03 | yes: all 8 printed values identical to darwin.dp (16 digits) | - |
| `tmud24cr` | yes | 0.03 | yes: all 7 printed values identical to darwin.dp (16 digits) | - |
| `tmud24sp` | yes | 0.03 | yes: all 7 printed values identical to darwin.dp (16 digits) | - |
| `tmud2cr` | yes | 0.03 | yes: all 6 printed values identical to darwin.dp (16 digits) | - |
| `tmud2sa` | yes | 0.04 | yes: all 7 printed values identical to darwin.dp (16 digits) | - |
| `tmud2sp` | yes | 0.03 | yes: all 6 printed values identical to darwin.dp (16 digits) | - |
| `tmud3` | yes | 0.04 | yes: all 9 printed values identical to darwin.dp (16 digits) | - |
| `tmud34` | yes | 0.04 | yes: all 11 printed values identical to darwin.dp (16 digits) | - |
| `tmud34sp` | yes | 0.05 | yes: all 9 printed values identical to darwin.dp (16 digits) | - |
| `tmud3cr` | yes | 0.09 | yes: all 17 printed values identical to darwin.dp (16 digits) | - |
| `tmud3sa` | yes | 0.05 | yes: all 8 printed values identical to darwin.dp (16 digits) | - |
| `tmud3sp` | yes | 0.04 | yes: all 8 printed values identical to darwin.dp (16 digits) | - |
| `tmuh2` | yes | 0.04 | yes: all 8 printed values identical to darwin.dp (16 digits) | - |
| `tmuh24` | yes | 0.05 | yes: all 11 printed values identical to darwin.dp (16 digits) | - |
| `tmuh24cr` | yes | 0.04 | yes: all 7 printed values identical to darwin.dp (16 digits) | - |
| `tmuh2cr` | yes | 0.04 | yes: all 6 printed values identical to darwin.dp (16 digits) | - |
| `tmuh3` | yes | 0.04 | yes: all 9 printed values identical to darwin.dp (16 digits) | - |
| `tmuh34` | yes | 0.04 | yes: all 10 printed values identical to darwin.dp (16 digits) | - |

Pitfalls of running the codebase: 4
- [build] make: pgf90/g95 not found (Linux) or the hard-coded gmake path not found (macOS) -> make MAKE=make AR=ar F90="gfortran -fdefault-real-8 -O2 -std=legacy -J LIBDIR -I LIBDIR" CPP="gfortran -cpp -E"
- [build] gfortran >= 10: Error: Type mismatch between actual argument ... (REAL(8) vs COMPLEX(8)); Error: Rank mismatch in argument theta -> -std=legacy (or -fallow-argument-mismatch)
- [general] single-precision (default) build gives 0 to 4 matching digits and mudpack tcud3cr fails to converge -> build with -fdefault-real-8 as the shipped darwin.dp references were
- [general] every test program writes only to stdout; the test Makefile pattern rule runs each executable right after linking it (the pattern rule executes the program it just linked), so `make` in test/ is build plus run and output/darwin.dp is the whole transcript -> run the .exe files again by hand to capture per-test stdout

Not run:
- MUDPACK with OpenMP (-fopenmp): the OpenMP directives are opt-in; all 36 drivers ran serially, the thread-parallel build was not attempted in Step 1.2

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `mudpack` | approved | single whole-codebase module | 102 | 124606 | 36 | unknown |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | unknown | ["mudpack"] | 0 | 0 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 0 | 0 | 0 |
| owned | 102 | 3829968 | 124606 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 0 | 0 | 0 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 36 | files |
| `test_definitions` | 36 | source-level test definitions |
| `collected_items` | 36 | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- the vendored tree is the extracted tarball, not a git checkout, so the review CLI cannot diff it against an upstream checkout; VENDORING.md carries the tarball SHA-256
- whether the OpenMP build changes any graded value (reduction order in the relaxation loops)
- the cycle count is printed by every driver and is bookkeeping, not physics

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
