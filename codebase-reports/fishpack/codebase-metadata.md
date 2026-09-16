<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `fishpack` | CLI |
| source payload | `code/fishpack/` | CLI |
| upstream pin | `ab983f0b16170b018770ec3bf70102c55f3fa3d8` | human/state |
| license | `UCAR BSD-style (LICENSE)` | human/state |
| source fingerprint | `629ae29449281b293b463fc9d58847d0009bde859933a042670c14a2a21d6c7b` | CLI |
| size | 54 files / 1453666 bytes / 34830 text lines | CLI |

### Build and run (Step 1.2: what was actually built and run natively)

Build: `make (GNU make; one top-level Makefile per library, each including a make.inc that selects the compiler by `uname -s`; src/ builds a static library, test/ compiles and immediately runs every test program)`, ok, 12 s; commands: `make MAKE=make AR=ar F90="gfortran -fdefault-real-8 -O2 -std=legacy -J LIBDIR -I LIBDIR" CPP="gfortran -cpp -E"   (doub…`; `make MAKE=make AR=ar F90="gfortran -O2 -std=legacy -J LIBDIR -I LIBDIR" CPP="gfortran -cpp -E"   (single precision, the…`.
- build pitfall: make.inc chooses the compiler from `uname -s`: on Linux it wants pgf90 or g95, on Darwin gfortran but MAKE := the hard-coded gmake path, which does not exist on current macOS; override MAKE=make AR=ar F90=... on the make command line (all five libraries)
- build pitfall: gfortran 15.2 (and every gfortran >= 10) refuses the Fortran 77 argument-type and rank mismatches in FFTPACK, FISHPACK and SPHEREPACK ("Type mismatch between actual argument ... REAL(8) vs COMPLEX(8)", "Rank mismatch in argument theta") unless -std=legacy (or -fallow-argument-mismatch) is given; measured: fftpack 12 errors, spherepack 2 errors without it
- build pitfall: the shipped darwin.dp transcripts were produced with -fdefault-real-8; the libraries are written for single precision and the darwin.sp build reproduces fewer digits (FISHPACK 1 to 4 digits, MUDPACK 0 to 2) and mudpack tcud3cr does not converge in single precision (ierror -10 here, ierror -4 and NaN in the shipped darwin.sp)
- build pitfall: wall_s is the double-precision `make` run, which includes the 19 test runs the test Makefile executes (under 2 s in total); the single-precision build took the same

| family | kind | decks | how to run | references |
|---|---|---:|---|---|
| fishpack-test | test-suite | 19 | `make` | shipped |

Actually run: 19 of 19 attempted.

| run | ran | wall s | reproduced / reason | pitfalls |
|---|---|---:|---|---|
| `tblktri` | yes | 0.03 | yes: all 2 printed values identical to darwin.dp (16 digits) | - |
| `tcblktri` | yes | 0.04 | yes: all 2 printed values identical to darwin.dp (16 digits) | - |
| `tcmgnbn` | yes | 0.03 | yes: all 3 printed values identical to darwin.dp (16 digits) | - |
| `tgenbun` | yes | 0.04 | yes: all 3 printed values identical to darwin.dp (16 digits) | - |
| `thstcrt` | yes | 0.04 | yes: all 3 printed values identical to darwin.dp (16 digits) | - |
| `thstcsp` | yes | 0.04 | yes: all 3 printed values identical to darwin.dp (16 digits) | - |
| `thstcyl` | yes | 0.03 | yes: all 5 printed values identical to darwin.dp (16 digits) | - |
| `thstplr` | yes | 0.03 | yes: all 3 printed values identical to darwin.dp (16 digits) | - |
| `thstssp` | yes | 0.03 | yes: all 5 printed values identical to darwin.dp (16 digits) | - |
| `thw3crt` | yes | 0.03 | yes: all 2 printed values identical to darwin.dp (16 digits) | - |
| `thwscrt` | yes | 0.03 | yes: all 3 printed values identical to darwin.dp (16 digits) | - |
| `thwscsp` | yes | 0.04 | yes: all 4 printed values identical to darwin.dp (16 digits) | - |
| `thwscyl` | yes | 0.03 | yes: all 5 printed values identical to darwin.dp (16 digits) | - |
| `thwsplr` | yes | 0.03 | yes: all 3 printed values identical to darwin.dp (16 digits) | - |
| `thwsssp` | yes | 0.03 | yes: all 2 printed values identical to darwin.dp (16 digits) | - |
| `tpois3d` | yes | 0.03 | yes: all 2 printed values identical to darwin.dp (16 digits) | - |
| `tpoistg` | yes | 0.03 | yes: all 3 printed values identical to darwin.dp (16 digits) | - |
| `tsepeli` | yes | 0.03 | yes: all 4 printed values identical to darwin.dp (16 digits) | - |
| `tsepx4` | yes | 0.03 | yes: all 5 printed values identical to darwin.dp (16 digits) | - |

Pitfalls of running the codebase: 3
- [build] make: pgf90/g95 not found (Linux) or the hard-coded gmake path not found (macOS) -> make MAKE=make AR=ar F90="gfortran -fdefault-real-8 -O2 -std=legacy -J LIBDIR -I LIBDIR" CPP="gfortran -cpp -E"
- [build] gfortran >= 10: Error: Type mismatch between actual argument ... (REAL(8) vs COMPLEX(8)); Error: Rank mismatch in argument theta -> -std=legacy (or -fallow-argument-mismatch)
- [general] every test program writes only to stdout; the test Makefile pattern rule runs each executable right after linking it (the pattern rule executes the program it just linked), so `make` in test/ is build plus run and output/darwin.dp is the whole transcript -> run the .exe files again by hand to capture per-test stdout

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `fishpack` | approved | single whole-codebase module | 54 | 34830 | 19 | unknown |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | unknown | ["fishpack"] | 0 | 0 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 0 | 0 | 0 |
| owned | 54 | 1453666 | 34830 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 0 | 0 | 0 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 19 | files |
| `test_definitions` | 19 | source-level test definitions |
| `collected_items` | 19 | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- the vendored tree is the extracted tarball, not a git checkout, so the review CLI cannot diff it against an upstream checkout; VENDORING.md carries the tarball SHA-256
- the graded precision: the double-precision build is the reference and the library is written for single precision
- FISHPACK90 (same solvers, Fortran 90 interface) sits in the same upstream repository and is not vendored

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
