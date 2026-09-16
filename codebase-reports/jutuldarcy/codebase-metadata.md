<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `jutuldarcy` | CLI |
| source payload | `code/jutuldarcy/` | CLI |
| upstream pin | `7b9d4a7306e32a732d7c65e93d1d1c2dfef09844` | human/state |
| license | `MIT` | human/state |
| source fingerprint | `2366d693942ba31cef88e0c5c4a6a52e3ddd4c97a9e85de81ac3a2a6871c63ae` | CLI |
| size | 313 files / 7274900 bytes / 63480 text lines | CLI |

### Build and run (Step 1.2: what was actually built and run natively)

Build: `Julia Pkg`, ok, 369.62 s; commands: `JULIA_DEPOT_PATH=<scratch-depot> JULIA_NUM_THREADS=4 julia +1.11 --project=. -e 'using Pkg; Pkg.instantiate(); Pkg.prec…`; `JULIA_DEPOT_PATH=<scratch-depot> JULIA_NUM_THREADS=4 julia +release --project=. -e 'using Pkg; Pkg.instantiate(); Pkg.p…`; `JULIA_DEPOT_PATH=<scratch-depot> JULIA_NUM_THREADS=4 julia +1.11 --project=docs -e 'using Pkg; Pkg.develop(path=pwd());…`.
- build pitfall: Use Julia 1.11.9 for this pin: Julia 1.13.0 resolves dependencies but Jutul 0.4.31 hits unsupported CartesianIndex iteration in thermal simple-well paths.
- build pitfall: The release has no Manifest; Pkg resolves newer compatible dependencies and writes a scratch Manifest, so the environment must be locked independently of the clean vendored source.
- build pitfall: First load downloads GeoEnergyIO artifacts such as SPE1, SPE9 and grdecl; the primary artifact URL may fail before a fallback succeeds.
- build pitfall: The complete docs environment precompiled 510 dependencies in 614.88 wall seconds; task checks should avoid carrying visualization-only dependencies.

| family | kind | decks | how to run | references |
|---|---|---:|---|---|
| package test items | test-suite | 26 | `julia +1.11 --project=. -e 'using Pkg; Pkg.test()' or run each included test file directly during t…` | partial |
| compositional examples | examples | 3 | `instantiate docs/Project.toml and include the selected .jl script with a headless Makie backend` | none |
| data assimilation examples | examples | 4 | `instantiate docs/Project.toml and include the selected .jl script with a headless Makie backend` | none |
| discretization examples | examples | 2 | `instantiate docs/Project.toml and include the selected .jl script with a headless Makie backend` | none |
| fracture examples | examples | 1 | `instantiate docs/Project.toml and include dfm_validation.jl with a headless Makie backend` | partial |
| geothermal examples | examples | 2 | `instantiate docs/Project.toml and include the selected .jl script with a headless Makie backend` | none |
| introduction examples | tutorials | 8 | `julia +1.11 --project=. examples/introduction/afi_input_file.jl; the other scripts require the docs…` | none |
| property examples | examples | 2 | `instantiate docs/Project.toml and include the selected .jl script with a headless Makie backend` | none |
| validation examples | regression | 9 | `instantiate docs/Project.toml and include the selected .jl script with a headless Makie backend` | partial |
| workflow examples | tutorials | 10 | `instantiate docs/Project.toml and include the selected .jl script with a headless Makie backend` | none |

Actually run: 22 of 23 attempted.

| run | ran | wall s | reproduced / reason | pitfalls |
|---|---|---:|---|---|
| `utils` | yes | 5.71 | yes: 40 of 40 upstream assertions passed | - |
| `co2-brine-properties` | yes | 5.14 | yes: 13 of 13 upstream numerical assertions passed at the upstream rtol | - |
| `singlephase` | yes | 18.73 | yes: 5 of 5 official solver-path assertions passed | - |
| `mesh-and-reservoir` | yes | 17.24 | yes: 66 of 66 upstream assertions passed | - |
| `multiphase` | yes | 127.11 | yes: 132 of 132 official solver-path assertions passed | Restricted macOS execution first aborted in MPI_Init_thread because MPICH/OFI could not create an endpoint on utun4; the unchanged test passed outside the rest… |
| `thermal-julia-113` | yes | 82.66 | no: 6 passed and 2 simple-well cases errored on Julia 1.13.0 | Jutul 0.4.31 attempts to iterate a CartesianIndex in sparsity tracing; Julia 1.13 deliberately rejects that operation. |
| `thermal-julia-111` | yes | 65.21 | yes: 8 of 8 official thermal and well cases passed | - |
| `compositional-validation` | yes | 60.76 | yes: 3000 of 3000 comparisons to the shipped SIMPLE_COMP reference passed | The first run downloads the SIMPLE_COMP artifact through GeoEnergyIO. |
| `mrst-cases` | yes | 88.83 | yes: SPE1 5/5, SPE3 6/6, SPE9 97/97 and Egg 28/28 assertions passed | The DATA-path comparison obtains SPE1 through a GeoEnergyIO artifact; the MRST fixtures themselves are vendored. |
| `buckley-leverett-sensitivities` | yes | 25.08 | yes: 12 of 12 sensitivity comparisons passed | - |
| `discrete-fractures` | yes | 104.1 | yes: 202 of 202 DFM assertions passed | The first run downloads the lazy CO2Tables_CSP11 artifact. |
| `multimodel-wells` | yes | 149.11 | yes: 250 of 250 coupled reservoir/well and preconditioner assertions passed | - |
| `multisegment-wells` | yes | 21.26 | yes: 18 of 18 multisegment-well assertions passed | - |
| `afi-input-example` | yes | 46.8 | no reference; the unchanged official example completed successfully | Downloads OLYMPUS_25_AFI_RESQML and SPE9_AFI_GSG artifacts.; The parser reports unsupported GSG include and several unhandled or unused AFI/IX record types but continues successfully. |
| `compositional-clapeyron-example` | yes | 157.03 | no reference; two unchanged official compositional simulations completed successfully | Near the three-minute investigation ceiling; a check should omit plotting and grade numerical phase compositions, pressure and saturation. |
| `avgmpfa-example` | yes | 79.08 | no reference; TPFA, AvgMPFA and NTPFA official runs completed successfully | AvgMPFA and NTPFA use many rejected ministeps; grade converged fields rather than iteration counts. |
| `dfm-validation-example` | yes | 201.92 | no standalone reference; all unchanged official DFM validation runs completed successfully | Above the 180-second native-run target but below the 300-second check ceiling; shorten the time grid while preserving matrix-fracture exchange physics. |
| `co2-properties-example` | yes | 30.04 | no reference; unchanged official CO2 property example completed successfully | The task check should grade numerical property tables, not rasterized figures. |
| `optimize-simple-bl-example` | yes | 140.34 | yes qualitatively: the official objective fell from 0.676192 to 0.000423 in the adjoint/L-BFGS path and the scalar cali… | One demonstration intentionally uses a 30-second optimizer time limit and reports non-convergence despite reducing the residual; grade objective improvement an… |
| `htates-example` | yes | 151.52 | no reference; unchanged 25-year, 300-step HT-ATES workflow completed successfully | Close to the native-run target; a check can reduce the report-step count while retaining cyclic injection and thermal transport. |
| `validation-spe1-example` | yes | 60.89 | yes: the unchanged SPE1 validation simulation and reference-comparison plotting path completed | Requires the SPE1 GeoEnergyIO artifact; prefetch it during image build and grade well time series directly. |
| `tracer-workflow-example` | yes | 120.09 | no reference; unchanged two-well tracer workflow completed successfully | Grade tracer concentration and conservation observables rather than visualization objects. |
| `gpu-tests` | no | - | The investigation host is Apple Silicon without a functional NVIDIA CUDA device; the file conditionally skips all tests… | - |

Pitfalls of running the codebase: 6
- [build and thermal-julia-113] Julia 1.13.0 plus resolved Jutul 0.4.31 warns during precompilation and errors in two thermal simple-well cases because CartesianIndex iteration is unsupported. -> Pin the task environment to Julia 1.11.9 and a generated Manifest; the unchanged thermal test passes 8/8 there.
- [build] The release does not ship Manifest.toml, so Pkg resolves current compatible dependency versions and writes a scratch Manifest. -> Vendor the clean upstream pin, but generate and copy a separate locked environment into the Docker build definition.
- [multiphase] Inside the restricted macOS sandbox, automatic HYPRE loading caused MPI_Init_thread to abort when MPICH/OFI could not create an endpoint on utun4. -> Run the native investigation outside the restricted sandbox; for Docker use a Linux Julia 1.11 image with an explicit MPI implementation.
- [general] First use of standard cases downloads GeoEnergyIO or package artifacts, and primary downloads may fail before fallback URLs succeed. -> Resolve and prefetch every selected check's artifacts during the image build, then run checks without network access.
- [examples] Forty of 41 official examples import GLMakie or another Makie-facing dependency and do not run in the package's minimal Project environment; the full docs environment precompiled 510 dependencies in 614.88 seconds. -> The official scripts do run on this macOS host with docs/Project.toml, but checks should extract the unchanged computational setup into a headless driver and grade numerical state/well outputs rather than plots or carry visualization-only dependencies.
- [afi-input-example] The example completes but reports unsupported GSG includes plus unhandled or unused AFI/IX records. -> Do not grade ignored records; grade the parsed model and subsequent physical simulation quantities that the supported path actually produces.

Not run:
- full Pkg.test() suite as one command: The complete suite is longer than the three-minute per-run investigation limit; representative TestItem source files were run independently instead.
- remaining package test items: The investigation ran representative tests across mesh, properties, single/multiphase, thermal, compositional, MRST input, gradients, fractures and wells. The remaining official items are still listed exhaustively in tests.json and will be considered individually for checks.
- remaining Makie-dependent examples: The docs environment and one official example from each of the eight Makie-dependent scientific families were run; the remaining examples are still listed exhaustively in tests.json and will be deduplicated or calibrated individually after the source pin is merged.
- CUDA and AMGX execution: No NVIDIA GPU is present on the Apple Silicon investigation host; conditional execution would skip every GPU assertion.

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `jutuldarcy` | approved | There are no peer modules: this is the whole-codebase default because every physics family shares the same package loader, model/state types, discretization and nonlinear/linear s… | 313 | 63480 | 26 | unknown |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | unknown | ["jutuldarcy"] | 0 | 0 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 0 | 0 | 0 |
| owned | 313 | 7274900 | 63480 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 0 | 0 | 0 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 68 | files |
| `test_definitions` | 27 | source-level test definitions |
| `collected_items` | 26 | framework-collected items |
| `inner_cases` | 177 | inner cases |

### Gaps and warnings
- Docker calibration on Linux may expose MPI implementation differences not visible on macOS
- the fina

[PR section truncated at 12,000 characters; canonical JSON and HTML retain the full report.]
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
