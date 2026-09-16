<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `snana` | CLI |
| source payload | `code/snana/` | CLI |
| upstream pin | `f7ad9ad6a1f58d7c550b646d9aeb86a9e8c34eb1` | registered metadata |
| license | `NOASSERTION; public source, manual permits modification; no repository-wide redistribution license located` | registered metadata |
| source fingerprint | `39af311ecaa368643d807ea2ae62396e2823d50db781e901a0a1ae29b2b622cb` | CLI |
| size | 239 files / 15233168 bytes / 402006 text lines | CLI |

### Build and run (Step 1.2: what was actually built and run natively)

Build: `Native GNU C/C++ compilation of upstream wfit target on Midway Linux x86_64`, ok, 23.6530110090971 s; commands: `source SNANA_setup.sourceme conda`; `export CC=gcc CXX=g++ OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 MKL_NUM_THREADS=1`; `python3 native-build.py --source source --out native-build`; `gcc -O2 -fcommon -fno-fast-math '-DGIT_SNANA_VERSION="f7ad9ad6a1f5"' -std=gnu99 -I/software/gsl-1.16-el7-x86_64/include…`; `gcc -O2 -fcommon -fno-fast-math '-DGIT_SNANA_VERSION="f7ad9ad6a1f5"' -std=gnu99 -I/software/gsl-1.16-el7-x86_64/include…`; `gcc -O2 -fcommon -fno-fast-math '-DGIT_SNANA_VERSION="f7ad9ad6a1f5"' -std=gnu99 -I/software/gsl-1.16-el7-x86_64/include…`; `g++ -O2 -fcommon -fno-fast-math '-DGIT_SNANA_VERSION="f7ad9ad6a1f5"' -std=c++11 -I/software/gsl-1.16-el7-x86_64/include…`; `g++ -O2 -fcommon -fno-fast-math '-DGIT_SNANA_VERSION="f7ad9ad6a1f5"' -std=c++11 -I/software/gsl-1.16-el7-x86_64/include…`; `g++ -O2 -fcommon -fno-fast-math '-DGIT_SNANA_VERSION="f7ad9ad6a1f5"' -std=c++11 -I/software/gsl-1.16-el7-x86_64/include…`; `g++ -o '$INVESTIGATION/native-build/wfit.exe' '$INVESTIGATION/native-build/wfit.o' '$INVESTIGATION/native-build/sntools…`.
- build pitfall: Source shared setup before enabling strict shell error handling because harmless interactive unalias calls return nonzero.
- build pitfall: The supplied conda Python is 3.7; use compatible scratch-wrapper file-copy and command-printing operations.
- build pitfall: Select gcc/g++ explicitly and set GNU99/C++11; a plain cc driver selected an older C default.
- build pitfall: Disable optional ROOT through the existing scratch header feature switch; all scientific source bodies remain byte-identical.

| family | kind | decks | how to run | references |
|---|---|---:|---|---|
| SNANA code regression harness | regression | 3 | `SNANA_code_tests.py with SNANA_DIR and external SNANA_TESTS/tasks, inputs, reference logs` | partial |
| Official external WFIT regression decks | regression | 3 | `Exact WFIT_K09, WFIT_DES3YR and WFIT_Pantheon commands in the dated external task snapshot` | shipped |
| SNANA batch submission harness | test-suite | 0 | `SNANA_submit_tests.py with external SNANA_TESTS/inputs_submit_batch and scheduler configuration` | none |
| Manual cosmology comparisons | examples | 4 | `wfit.exe [HubbleDiagram] -cmb_sim -sigma_Rcmb 0.007 -wsteps 101 -omsteps 101 with covariance and mo…` | partial |
| Manual velocity-covariance example | examples | 1 | `wfit.exe <fitresFile> -mucovar <mucovarFile> <other options>` | none |
| wfit usage example | examples | 1 | `wfit.exe` | none |
| Simulation unit tests | test-suite | 0 | `snlc_sim.exe with an external input deck and UNIT_TEST setting` | none |
| Covariance validation utility | other | 0 | `check_create_covariance.py with BBC covariance configuration and products` | none |

Actually run: 39 of 39 attempted.

| run | ran | wall s | reproduced / reason | pitfalls |
|---|---|---:|---|---|
| `wfit-k09-native-linux` | yes | 2.8555413633584976 | yes: all four COSPAR values match the stored upstream reference to five decimal places; reference revision v12_02b-22-g… | - |
| `wfit-des3yr-native-linux` | yes | 1.8475552052259445 | yes: all four COSPAR values match the stored upstream reference to five decimal places; reference revision v12_02b-22-g… | - |
| `wfit-pantheon-native-linux` | yes | 3.1354530453681946 | yes: all four COSPAR values match the stored upstream reference to five decimal places; reference revision v12_02b-22-g… | - |
| `custom-correlated-nominal-1` | yes | 1.1870857080211863 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-correlated-nominal-2` | yes | 1.1957416249788366 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-diagonal-nominal-1` | yes | 1.0851467090542428 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-diagonal-nominal-2` | yes | 1.0811742500518449 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-inverse-nominal-1` | yes | 1.1940452909911983 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-inverse-nominal-2` | yes | 1.2381696249940433 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-npz-nominal-1` | yes | 1.186360334046185 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-npz-nominal-2` | yes | 1.1951658749603666 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-permutation-nominal-1` | yes | 1.194908250006847 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-permutation-nominal-2` | yes | 1.1953111669863574 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-prior-shift-nominal-1` | yes | 1.185315708979033 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-prior-shift-nominal-2` | yes | 1.2455367910442874 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-redshift-cut-nominal-1` | yes | 0.8247891250066459 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-redshift-cut-nominal-2` | yes | 0.8080685829627328 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-scatter-nominal-1` | yes | 1.1963929170160554 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-scatter-nominal-2` | yes | 1.1872359170229174 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-correlated-perturbed-1` | yes | 1.2014549580053426 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-correlated-perturbed-2` | yes | 1.2013440419686958 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-diagonal-perturbed-1` | yes | 1.0825094580068253 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-diagonal-perturbed-2` | yes | 1.0683631250285544 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-inverse-perturbed-1` | yes | 1.2281197079573758 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-inverse-perturbed-2` | yes | 1.2470829170197248 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-npz-perturbed-1` | yes | 1.1963667499949224 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-npz-perturbed-2` | yes | 1.218822332972195 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-permutation-perturbed-1` | yes | 1.1980789579683915 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-permutation-perturbed-2` | yes | 1.1846130000194535 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-prior-shift-perturbed-1` | yes | 1.1900153750320897 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-prior-shift-perturbed-2` | yes | 1.1846451250021346 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-redshift-cut-perturbed-1` | yes | 0.8256511249928735 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-redshift-cut-perturbed-2` | yes | 0.8218983340193518 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-scatter-perturbed-1` | yes | 1.1847608330426738 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `custom-scatter-perturbed-2` | yes | 1.1867854580050334 | no upstream reference; independent NumPy quadrature/covariance oracle passed every graded quantity | - |
| `upstream-wfit-help` | yes | 0.012951750017236918 | no reference; printed usage and source version v12_02b-23-gf7ad9ad6 | - |
| `wfit-k09-native-mac` | yes | 1.094013832975179 | yes: all four official COSPAR values match the stored upstream reference to five decimal places; reference revision v12… | - |
| `wfit-des3yr-native-mac` | yes | 0.7395741670043208 | yes: all four official COSPAR values match the stored upstream reference to five decimal places; reference revision v12… | - |
| `wfit-pantheon-native-mac` | yes | 1.6511427500518039 | yes: all four official COSPAR values match the stored upstream reference to five decimal places; reference revision v12… | - |

Pitfalls of running the codebase: 6
- [build] Pinned clean native build hits glibc APIs, then a separate fnam diagnostic bug when upstream Mac switches are enabled. -> Preserve the native Mac failure as a portability observation; the clean pinned-source build now succeeds natively on Midway Linux without any scientific-source patch.
- [general] SNANA_TESTS was absent locally, although the Git repository carries only harness scripts. -> Located the official three-item WFIT list on Midway; copied only 12 required files (12,156,883 bytes) and recorded every hash. Full SNANA datasets were not copied.
- [custom-probes] Summary chi-square is printed to one decimal, correlation to three decimals, residuals to four decimals. -> Compare full scientific grid and independent numerical oracle, and account for upstream output quantization in specified tolerances.
- [midway-setup] The shared interactive SNANA setup contains unalias calls that return nonzero in a batch shell. Starting errexit before sourcing stops before compilation. -> Source the existing setup before enabling errexit/nounset for the actual isolated build and test commands.
- [midway-python] The shared conda environment uses Python older than 3.8, so copytree(dirs_exist_ok=...) and shlex.join are unavailable. -> Use equivalent Python 3.7-compatible wrapper operations in the private scratch builder. No SNANA source change is needed.
- [midway-compiler] The cc driver selected an older compiler whose default C dialect rejects loop-variable declarations. -> Select gcc/g++ explicitly and declare GNU99 for C and C++

[PR section truncated at 12,000 characters; canonical JSON and HTML retain the full report.]
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
