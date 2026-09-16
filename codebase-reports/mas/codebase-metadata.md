<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `mas` | CLI |
| source payload | `code/mas/` | CLI |
| upstream pin | `3641eb2a5b22e1c3030baa8b3566fe25ebfa01b3` | human/state |
| license | `Apache-2.0` | human/state |
| source fingerprint | `3812f1790592a823577db676dbd06b526d3060fc7a21d9b12cba28256c139223` | CLI |
| size | 239 files / 12481894 bytes / 132973 text lines | CLI |

### Build and run (Step 1.2: what was actually built and run natively)

Build: `build.sh + generated make (conf file gives FC, FRTFLAGS, HDF5 include and lib paths; build.sh writes src/Makefile from Makefile.template, compiles the expmac macro expander, expands src/mas.F90 into src/mas_cpp.f90, compiles pchip_module_v1.0.0.f90 and mas_cpp.f90, links with -lhdf5_fortran -lhdf5_hl_fortran -lhdf5 -lhdf5_hl, and moves the executable to bin/mas)`, ok, 68 s; commands: `cp conf/gcc_cpu_mac_brew.conf conf/gcc_cpu_mac_brew_local.conf; edit HDF5_INCLUDE_DIR to the Homebrew prefix include di…`; `bash build.sh conf/gcc_cpu_mac_brew_local.conf   (FC mpif90 = gfortran 15.2 under Open MPI 5.0.8, FRTFLAGS -O3 -march=n…`.
- build pitfall: the shipped conf/gcc_cpu_mac_brew.conf points HDF5 at the Intel Homebrew prefix (Intel Homebrew); on Apple silicon Homebrew lives under the opt homebrew prefix, so the two HDF5 paths must be edited, otherwise the link fails on hdf5.mod
- build pitfall: the compile is one 74k-line translation unit at -O3 (63 s of the 68 s); there is no parallel make
- build pitfall: HDF5 must be built with the same Fortran compiler as FC (the Fortran .mod files are compiler-specific); Homebrew hdf5 (serial) with mpif90 wrapping the same gfortran links fine, no parallel HDF5 is required

| family | kind | decks | how to run | references |
|---|---|---:|---|---|
| testsuite | test-suite | 8 | `cd testsuite && bash run_test_suite.sh [-np=N] [-test=<name>] [-nocleanup] (needs bin/ on PATH or i…` | shipped |
| examples | examples | 6 | `in the deck directory, mpirun -np N the built mas executable <run_name> mas.in (the coronal decks r…` | none |

Actually run: 11 of 14 attempted.

| run | ran | wall s | reproduced / reason | pitfalls |
|---|---|---:|---|---|
| `zb-2d-alfven-wave-t` | yes | 15.9 | yes: PASSED the official comparison, the 8 graded history columns agree with reference/mas_history_a.out to 6 decimal d… | - |
| `zb-3d-alfven-wave-p-rot` | yes | 35.4 | yes: PASSED to 6 decimal digits | - |
| `zb-3d-tdm-3rope` | yes | 5.2 | yes: PASSED to 6 decimal digits (ntmax 20) | - |
| `zb-3d-fr-rbsl` | yes | 4.0 | yes: PASSED to 6 decimal digits on 1 rank and on 4 ranks; the 4-rank and 1-rank final history lines agree to 12 decimal… | - |
| `poly-3d-pw-relaxation` | yes | 30.5 | yes: PASSED to 6 decimal digits | - |
| `thermo-3d-relaxation` | yes | 23.7 | yes: PASSED to 6 decimal digits on 1 rank (23.7 s) and on 4 ranks (7.8 s); the 4-rank and 1-rank final history lines di… | - |
| `thermo-wtd-3d-relaxation` | yes | 26.3 | yes: PASSED to 6 decimal digits | - |
| `helio-2d-relaxation` | yes | 2.7 | yes: PASSED to 6 decimal digits on 1 rank and on 4 ranks; the 4-rank and 1-rank final history lines agree to 12 decimal… | - |
| `corona-polytropic-cr2124-steadystate` | yes | 2.8 | no reference shipped; the run completed (mas_timing.out written, potential field solved from the HMI map, history and H… | the deck reads the sibling input_data directory  by relative path, so the run directory must sit beside input_data/; at production resolution one time step costs minutes on 4 ranks (potential-field solve plus semi-implicit velocity solve at epscg 1e-9): the deck cannot be run… |
| `heliosphere-wsa-bc-cr2124-steadystate` | yes | 1.1 | no reference shipped; the run completed with the WSA boundary files in bc/ (br, rho, t, vr; bt, bp, vt, vp are symlinks… | bc/ contains symlinks (bp_wsa_idx000001.h5 -> zero_tp.h5 etc.); a copy must preserve them or dereference them |
| `corona-thermo-hm1-cr2124-steadystate` | yes | 3.0 | no reference shipped; the run completed with HM1 heating, thermal conduction and radiative losses on (heat001.h5, em001… | reads the sibling input_data directory  by relative path |
| `corona-thermo-hm2-cr2124-steadystate` | no | - | same input plumbing as the HM1 deck plus the closed-field heating mask closed_field_mask_pfss.h5 shipped in the deck; d… | - |
| `corona-thermo-hm1-tilted-dipole-steadystate` | no | - | DIPOLE initial field variant of the HM1 deck with no input data; deferred to the survey | - |
| `heliosphere-tmhdhm1-bc-cr2124-steadystate` | no | - | same heliospheric driver as the WSA deck with boundary slices from a thermodynamic coronal run (bc/slice_tp001_*.h5, 1.… | - |

Pitfalls of running the codebase: 7
- [build] link fails or hdf5.mod not found with the shipped mac conf on Apple silicon -> edit the two HDF5 paths in the conf to the Homebrew prefix include directory and the Homebrew prefix lib directory
- [testsuite] run_test_suite.sh -test=a,b reports "TEST a' is not a valid test" under macOS bash 3.2: the comma-to-space substitution inserts literal quotes -> pass one test per -test= (or run under bash 5); the full suite with no -test works
- [testsuite] the reference comparison compares the first 6 significant digits of each history value as strings but copies the exponent from the reference file (num1[-4:]) for both, so a difference only in the exponent is not caught; and the -compareprec default is 6 while the help text says 5 -> for the checks, grade the history columns numerically (relative tolerance), not with mas_compare_run_diags.py
- [testsuite] the speed-up line calls `python`, not python3; on a host without a python alias it prints a traceback after the run (the PASS/FAIL verdict is unaffected) -> provide a python shim or ignore the line
- [thermo-3d-relaxation] the final history values depend on the MPI rank count at the 1e-8 relative level (semi-implicit PCG solves) -> fix the rank count in every check; calibrate the tolerance for that rank count
- [general] mpirun (Open MPI 5 prterun) ignores SIGALRM, so `perl -e 'alarm N; exec @ARGV'` does not stop a run; the six-minute production-grid runs had to be killed by hand -> launch under a wrapper that kills the whole process group (start_new_session + killpg), as the runto.py wrapper did
- [examples] production decks take minutes per step on 4 ranks (7.8M to 28M cells with 1e-9 CG tolerances) -> reduce nr, nt, np and set ntmax for smoke runs; a check at this size needs its own reduced grid and window, recorded as a knob

Not run:
- the six legacy tests commented out in run_test_suite.sh (poly_1d_parker, poly_2d_shear, poly_2d_dipole, poly_2d_ip_cs, …: no directories in the pinned tree
- GPU builds (conf/nvidia_gpu_psi.conf, OpenACC + do concurrent under nvfortran) and the intel conf: no nvfortran or ifx on this machine; the CPU gfortran build is the native investigation build
- multi-rank runs of the remaining five testsuite decks: three decks cover the three behaviours (zero-beta, heliospheric 2D, thermodynamic); the rest are deferred to check calibration

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `mas` | approved | single whole-codebase module | 239 | 132973 | 8 | unknown |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | unknown | ["mas"] | 0 | 0 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 0 | 0 | 0 |
| owned | 239 | 12481894 | 132973 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 0 | 0 | 0 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 14 | files |
| `test_definitions` | 8 | source-level test definitions |
| `collected_items` | 8 | framework-collected items |
| `inner_cases` | 14 | inner cases |

### Gaps and warnings
- the shipped references come from an x86 GCC 12.3 4-rank run; the arm64 gfortran 15 runs meet the suite's 6-digit bar, and the thermodynamic decks differ between 1 and 4 ranks at the 8th digit
- no GPU compiler here: the OpenACC path is unverified
- the reduced grid and window for the six example-derived checks (set at calibration under the 300 s rule)
- whether the README's contact request to PSI needs a note to the developers before the task PR

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
