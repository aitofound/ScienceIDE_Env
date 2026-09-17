<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `rtklib` | CLI |
| source payload | `code/rtklib/` | CLI |
| upstream pin | `71db0ffa0d9735697c6adfd06fdf766d0e5ce807` | human/state |
| license | `BSD-2-Clause with additional distribution terms; see readme.txt` | human/state |
| source fingerprint | `bab5964dfe802984ccf0ef16a451e820b81d0a74b0ea2600312cf2b9811edf51` | CLI |
| size | 685 files / 66039539 bytes / 506565 text lines | CLI |

### Build and run (Step 1.2: what was actually built and run natively)

Build: `GNU Make 3.81 and Apple Clang 17.0.0, macOS 15.7.3 arm64; separate scratch tree`, ok, 8.256333 s; commands: `cd app/convbin/gcc && make -j2 'OPTIONS=-DTRACE -DENAGLO -DENAQZS -DENAGAL -DENACMP -DNFREQ=6 -DNEXOBS=3 -D_DARWIN_C_SO…`; `cd test/utest && clang -O3 -ansi -pedantic -D_DARWIN_C_SOURCE -DTRACE -DENAGLO -DENAQZS -I"$RTKLIB_SOURCE/src" -o t_ppp…`; `cd app/rnx2rtkp/gcc && make -B -j2 'OPTS=-DTRACE -DENAGLO -DENAQZS -DENAGAL -DNFREQ=3 -D_DARWIN_C_SOURCE' LDLIBS=-lm`; `cd app/rnx2rtkp/gcc && make -j2`; `cd test/utest && make -k -j2 all 'CFLAGS=-Wall -O3 -ansi -pedantic -I"$RTKLIB_SOURCE/src" -DTRACE -DENAGLO -DENAQZS -D_…`; `cd test/utest && make -j2 t_time t_coord t_lambda t_atmos t_misc t_rinex 'CFLAGS=-Wall -O3 -ansi -pedantic -I"$RTKLIB_S…`.
- build pitfall: RNX2RTKP and CONVBIN built; selected unit tests built. The stock all-unit-test target failed; build.ok does not mean all targets passed.
- build pitfall: Measured wall_s sums all recorded build attempts, including failures and the explicit PPP support-test link.
- build pitfall: macOS-only flag overrides are recorded; original source and upstream makefiles are unchanged.

| family | kind | decks | how to run | references |
|---|---|---:|---|---|
| C unit-test sources | test-suite | 16 | `14 named make targets; t_filter.c and t_corrperf.c are incomplete/debug sources. Compile individual…` | partial |
| Offline positioning | examples | 25 | `Build locally, then make test1, test10, test11, test12, test13, test14, test15, test16, test17, tes…` | none |
| Receiver conversion | examples | 18 | `Build locally, then make test1, test10, test11, test12, test13, test14, test15, test16, test17, tes…` | none |
| Real-time positioning | examples | 3 | `Build locally, then make test1, test2, test3; inspect fixtures and services before execution.` | none |
| Stream relay | examples | 6 | `Build locally, then make test1, test2, test3, test4, test5, test6; inspect fixtures and services be…` | none |
| RINEX to RTCM | examples | 11 | `Build locally, then make test1, test10, test11, test2, test3, test4, test5, test6, test7, test8, te…` | none |
| LEX correction utilities | examples | 4 | `Build locally, then make test1, test2, test3, test4; inspect fixtures and services before execution.` | none |
| Ionosphere utilities | examples | 2 | `Build locally, then make test1, test2; inspect fixtures and services before execution.` | none |
| CRC generator | examples | 1 | `Build locally, then make test; inspect fixtures and services before execution.` | none |
| Observation simulation | examples | 4 | `make sim runs four receiver configurations; referenced util/simobs/sim fixtures are absent.` | none |

Actually run: 40 of 40 attempted.

| run | ran | wall s | reproduced / reason | pitfalls |
|---|---|---:|---|---|
| `convbin-test01` | yes | 0.184784 | No upstream reference comparison performed; command completed. | Output date is 2029 for the 2009 named fixture: adjgpsweek uses the host clock to resolve GPS week rollover. Do not treat this as a reproducible reference with… |
| `convbin-test03` | yes | 0.010949 | FAILED conversion; see recorded diagnostic. | Unspecified output filenames contain uninitialized bytes: ofile_[7][1024] is not initialized when only some output paths are supplied. The unchanged upstream c… |
| `convbin-test08` | yes | 0.040538 | No upstream reference comparison performed; command completed. | - |
| `convbin-test11` | yes | 0.010702 | FAILED conversion; see recorded diagnostic. | Official Javad conversion command failed with 'no time for output path'; no valid result claimed. |
| `rnx2rtkp-test01` | yes | 0.177295 | FAILED to produce positions: missing base-station observations/coordinates; process still returned zero. | Exit code zero is insufficient: inspect solution count and reported processing errors. |
| `rnx2rtkp-test02` | yes | 0.041154 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test03` | yes | 0.038584 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test04` | yes | 0.068885 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test05-repeat` | yes | 0.075198 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test05` | yes | 0.069968 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test06` | yes | 0.038453 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test07` | yes | 0.077188 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test08` | yes | 0.071242 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test09-repeat` | yes | 0.038742 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test09` | yes | 0.021695 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test10` | yes | 0.020749 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test11` | yes | 0.020807 | No upstream reference output supplied. Generated 230 NMEA sentences (not 230 independent solution epochs). | - |
| `rnx2rtkp-test12` | yes | 0.020479 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test13` | yes | 0.020706 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test14` | yes | 0.020541 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test15` | yes | 0.019973 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test16` | yes | 0.020633 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test17` | yes | 0.077103 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test18` | yes | 0.126604 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test19` | yes | 0.074199 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test20` | yes | 0.074026 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test21` | yes | 0.076496 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test22` | yes | 0.126847 | No upstream reference output supplied. Generated 1 non-comment rows; numeric correctness not independently established. | - |
| `rnx2rtkp-test23` | yes | 0.182224 | No upstream reference output supplied. Generated 120 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test24` | yes | 0.12723 | No upstream reference output supplied. Generated 115 non-comment rows; numeric correctness not independently establishe… | - |
| `rnx2rtkp-test25` | yes | 0.128572 | No upstream reference output supplied. Generated 1 non-comment rows; numeric correctness not independently established. | - |
| `unit-atmos` | yes | 0.128245 | Upstream test executable completed with all assertions enabled and passing. | - |
| `unit-coord` | yes | 0.1308 | Upstream test executable completed with all assertions enabled and passing. | - |
| `unit-ionex` | yes | 0.286584 | Upstream test executable completed with all assertions enabled and passing. | - |
| `unit-lambda` | yes | 0.132001 | Upstream test executable completed with all assertions enabled and passing. | - |
| `unit-matrix` | yes | 0.347741 | Upstream test executable completed with all assertions enabled and passing. | - |
| `unit-misc` | yes | 0.128044 | Upstream test executable completed with all assertions enabled and passing. | - |
| `unit-ppp-functions` | yes | 0.238213 | Upstream test executable completed with all assertions enabled and passing. This covers Earth rotation, Sun/Moon positi… | Compiled the unchanged test with an explicit source list to avoid the obsolete stec.c dependency and include RTCM dependencies; command recorded. |
| `unit-rinex` | yes | 0.13243 | Upstream test executable completed with all assertions enabled and passing. | - |
| `unit-time` | yes | 0.182224 | FAILED upstream utest7 assertion for 2038 GPST-to-UTC conversion. | The expected future offset is 16 s, while the pinned source's leap-second table has 18 s from 2017; do not weaken tolerances to conceal this stale expected val… |

Pitfalls of running the codebase: 7
- [native build] Upstream ANSI C flags hide strtok_r on macOS; upstream Linux link flags include unavailable librt. -> Add -D_DARWIN_C_SOURCE and override LDLIBS=-lm for this Mac only; preserve original source and upstream makefiles.
- [test/utest/makefile] make all fails on missing src/stec.c and missing input_rtcm3 linkage. -> Record failure. For t_ppp, an explicit source list supplies real RTCM dependencies without changing the production implementation; other targets remain unverified.
- [rnx2rtkp-test01] No average base-station position; zero exit code, zero solution rows. -> Mark this attempted run as unsuccessful. Future checks need explicit output-count validation and a documented valid input configuration.
- [unit-time] Future leap-second expectation predates source-table updates. -> Document stale reference and derive the expected value from the pinned leap-second table; no source or assertion changed in this investigation.
- [convbin-test01] A 2009 fixture generates 2029 dates using the current host clock. -> Investigate an explicit reference epoch before adopting receiver-conversion checks; not yet resolved.
- [convbin-test03] Garbage output paths and file-open failure with partial output filename options. -> Document the uninitialized filename mechanism. A complete explicit output-path configuration needs separate verification; no production patch made.
- [convbin-test11] Official Javad example cannot derive time for its output filename. -> Record failed example; time/input reconstruction n

[PR section truncated at 12,000 characters; canonical JSON and HTML retain the full report.]
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
