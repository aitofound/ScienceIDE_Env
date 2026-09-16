<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `spherepack` | CLI |
| source payload | `code/spherepack/` | CLI |
| upstream pin | `ab983f0b16170b018770ec3bf70102c55f3fa3d8` | human/state |
| license | `UCAR BSD-style (LICENSE)` | human/state |
| source fingerprint | `254d765b2cc17e2cd21d555fe3708fc99952f9a202c43a7dfa966736b96ae379` | CLI |
| size | 126 files / 3528552 bytes / 92353 text lines | CLI |

### Build and run (Step 1.2: what was actually built and run natively)

Build: `make (GNU make; one top-level Makefile per library, each including a make.inc that selects the compiler by `uname -s`; src/ builds a static library, test/ compiles and immediately runs every test program)`, ok, 29 s; commands: `make MAKE=make AR=ar F90="gfortran -fdefault-real-8 -O2 -std=legacy -J LIBDIR -I LIBDIR" CPP="gfortran -cpp -E"   (doub…`; `make MAKE=make AR=ar F90="gfortran -O2 -std=legacy -J LIBDIR -I LIBDIR" CPP="gfortran -cpp -E"   (single precision, the…`.
- build pitfall: make.inc chooses the compiler from `uname -s`: on Linux it wants pgf90 or g95, on Darwin gfortran but MAKE := the hard-coded gmake path, which does not exist on current macOS; override MAKE=make AR=ar F90=... on the make command line (all five libraries)
- build pitfall: gfortran 15.2 (and every gfortran >= 10) refuses the Fortran 77 argument-type and rank mismatches in FFTPACK, FISHPACK and SPHEREPACK ("Type mismatch between actual argument ... REAL(8) vs COMPLEX(8)", "Rank mismatch in argument theta") unless -std=legacy (or -fallow-argument-mismatch) is given; measured: fftpack 12 errors, spherepack 2 errors without it
- build pitfall: the shipped darwin.dp transcripts were produced with -fdefault-real-8; the libraries are written for single precision and the darwin.sp build reproduces fewer digits (FISHPACK 1 to 4 digits, MUDPACK 0 to 2) and mudpack tcud3cr does not converge in single precision (ierror -10 here, ierror -4 and NaN in the shipped darwin.sp)
- build pitfall: wall_s is the double-precision `make` run, which includes the 17 test runs the test Makefile executes (under 2 s in total); the single-precision build took the same

| family | kind | decks | how to run | references |
|---|---|---:|---|---|
| spherepack-test | test-suite | 17 | `make` | shipped |
| spherepack-examples | examples | 3 | `link the program object against libspherepack.a by hand; no Makefile target runs them` | none |

Actually run: 20 of 20 attempted.

| run | ran | wall s | reproduced / reason | pitfalls |
|---|---|---:|---|---|
| `testrvsph` | yes | 0.1 | residual level: all 4 printed values are round-off errors (1e-13 to 1e-17) or CPU timings; same magnitude as darwin.dp,… | - |
| `tdiv` | yes | 0.04 | yes: 16 digits on the 15 non-residual printed values; 20 printed errors are round-off residuals (1e-14 to 1e-17) here a… | - |
| `testrssph` | yes | 0.13 | residual level: all 2 printed values are round-off errors (1e-13 to 1e-17) or CPU timings; same magnitude as darwin.dp,… | - |
| `testsshifte` | yes | 0.03 | residual level: all 2 printed values are round-off errors (1e-13 to 1e-17) or CPU timings; same magnitude as darwin.dp,… | - |
| `testvshifte` | yes | 0.03 | residual level: all 4 printed values are round-off errors (1e-13 to 1e-17) or CPU timings; same magnitude as darwin.dp,… | - |
| `testvtsgs` | yes | 0.04 | residual level: all 2 printed values are round-off errors (1e-13 to 1e-17) or CPU timings; same magnitude as darwin.dp,… | - |
| `tgaqd` | yes | 0.03 | residual level: all 23 printed values are round-off errors (1e-13 to 1e-17) or CPU timings; same magnitude as darwin.dp… | prints CPU timings (tusl/toe/tdoub) that read 0.0 here and 1e-5 s in darwin.dp: bookkeeping, not comparable |
| `tgrad` | yes | 0.12 | yes: 16 digits on the 33 non-residual printed values; 12 printed errors are round-off residuals (1e-14 to 1e-17) here a… | - |
| `tidvt` | yes | 0.08 | yes: 16 digits on the 25 non-residual printed values; 40 printed errors are round-off residuals (1e-14 to 1e-17) here a… | - |
| `tsha` | yes | 0.04 | yes: 16 digits on the 15 non-residual printed values; 4 printed errors are round-off residuals (1e-14 to 1e-17) here an… | - |
| `tshpe` | yes | 0.04 | residual level: all 27 printed values are round-off errors (1e-13 to 1e-17) or CPU timings; same magnitude as darwin.dp… | prints CPU timings (tusl/toe/tdoub) that read 0.0 here and 1e-5 s in darwin.dp: bookkeeping, not comparable |
| `tshpg` | yes | 0.04 | residual level: all 18 printed values are round-off errors (1e-13 to 1e-17) or CPU timings; same magnitude as darwin.dp… | prints CPU timings (tusl/toe/tdoub) that read 0.0 here and 1e-5 s in darwin.dp: bookkeeping, not comparable |
| `tslap` | yes | 0.04 | yes: 16 digits on the 15 non-residual printed values; 20 printed errors are round-off residuals (1e-14 to 1e-17) here a… | - |
| `tvha` | yes | 0.06 | yes: 16 digits on the 25 non-residual printed values; 8 printed errors are round-off residuals (1e-14 to 1e-17) here an… | - |
| `tvlap` | yes | 0.11 | yes: 16 digits on the 29 non-residual printed values; 16 printed errors are round-off residuals (1e-14 to 1e-17) here a… | - |
| `tvrt` | yes | 0.08 | yes: 16 digits on the 24 non-residual printed values; 16 printed errors are round-off residuals (1e-14 to 1e-17) here a… | - |
| `tvts` | yes | 0.06 | yes: 16 digits on the 25 non-residual printed values; 8 printed errors are round-off residuals (1e-14 to 1e-17) here an… | - |
| `example-advec` | yes | 0.26 | no reference: upstream runs this program nowhere and ships no output; 5 cycles printed | the three example programs sit in src/ and are archived into libspherepack.a by the src Makefile but never linked or run by any Makefile; link them by hand aga… |
| `example-helmsph` | yes | 0.21 | no reference: max error 0.250E-14 is a round-off residual | the three example programs sit in src/ and are archived into libspherepack.a by the src Makefile but never linked or run by any Makefile; link them by hand aga… |
| `example-shallow` | yes | 0.99 | no reference: upstream runs this program nowhere and ships no output; 11 cycles printed | the three example programs sit in src/ and are archived into libspherepack.a by the src Makefile but never linked or run by any Makefile; link them by hand aga… |

Pitfalls of running the codebase: 5
- [build] make: pgf90/g95 not found (Linux) or the hard-coded gmake path not found (macOS) -> make MAKE=make AR=ar F90="gfortran -fdefault-real-8 -O2 -std=legacy -J LIBDIR -I LIBDIR" CPP="gfortran -cpp -E"
- [build] gfortran >= 10: Error: Type mismatch between actual argument ... (REAL(8) vs COMPLEX(8)); Error: Rank mismatch in argument theta -> -std=legacy (or -fallow-argument-mismatch)
- [example-advec] advec, helmsph and shallow are archived into libspherepack.a but no Makefile links or runs them -> gfortran src/advec.f -o advec.exe -L LIBDIR -lspherepack
- [general] every test program writes only to stdout; the test Makefile pattern rule runs each executable right after linking it (the pattern rule executes the program it just linked), so `make` in test/ is build plus run and output/darwin.dp is the whole transcript -> run the .exe files again by hand to capture per-test stdout
- [general] SPHEREPACK tgaqd, tshpe and tshpg print CPU timings that are 0.0 here -> timings are bookkeeping and are not compared

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `spherepack` | approved | single whole-codebase module | 126 | 92353 | 20 | unknown |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | unknown | ["spherepack"] | 0 | 0 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 0 | 0 | 0 |
| owned | 126 | 3528552 | 92353 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 0 | 0 | 0 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 20 | files |
| `test_definitions` | 20 | source-level test definitions |
| `collected_items` | 20 | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- the vendored tree is the extracted tarball, not a git checkout, so the review CLI cannot diff it against an upstream checkout; VENDORING.md carries the tarball SHA-256
- the example models (advection, shallow water) are the natural expensive workloads; their graded window and resolution are chosen at the task step
- several test outputs are round-off residuals (1e-14 to 1e-17) that only an invariant bound can grade

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
