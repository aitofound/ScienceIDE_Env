<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `mujoco` | CLI |
| source payload | `code/mujoco/` | CLI |
| upstream pin | `25f0ac2b57ed510d2f004f0fca1cb260b51eb5fa` | human/state |
| license | `Apache-2.0` | human/state |
| source fingerprint | `02c78bcc4412f753cc4e848c1fde987b2b5009280080facdeccacbf553bedfc1` | CLI |
| size | 2191 files / 114073515 bytes / 981525 text lines | CLI |

### Build and run (Step 1.2: what was actually built and run natively)

Build: `CMake 4.4.3 with Ninja and GCC 13.4.0 in an isolated Conda toolchain`, ok, 131.17 s; commands: `rsync -a --delete --exclude=.git UPSTREAM_CHECKOUT source`; `cmake -S source -B build-gcc13 -G Ninja -DCMAKE_BUILD_TYPE=Release -DMUJOCO_BUILD_TESTS=ON -DMUJOCO_BUILD_EXAMPLES=ON -…`; `cmake --build build-gcc13 --parallel 8 --target compile testspeed`; `cmake -S source -B build-gcc13 -DMUJOCO_BUILD_EXAMPLES=OFF -DMUJOCO_BUILD_SIMULATE=OFF -DMUJOCO_BUILD_TESTS=ON -DPython…`; `cmake --build build-gcc13 --parallel 16`.
- build pitfall: The pipeline state filesystem had insufficient quota for the scratch source; moving scratch and build artifacts to a large local workspace resolved it.
- build pitfall: FetchContent checkouts on the NAS triggered Git dubious-ownership protection; a command-scoped safe.directory=* setting was required for dependency fetches.
- build pitfall: The pinned main branch uses C++20 and dependencies requiring GCC 10+, while actual source compilation also failed with GCC 11; an isolated GCC 13.4 toolchain matching upstream CI was used.
- build pitfall: Building every graphical sample requires complete X11/OpenGL development headers. Headless compile and testspeed were built explicitly, then GUI examples were disabled for the test build.
- build pitfall: CMake initially selected system Python 3.8 for documentation tests; Python3_EXECUTABLE was set to the verl Python 3.12 interpreter.

| family | kind | decks | how to run | references |
|---|---|---:|---|---|
| C++ CTest and GoogleTest suite | test-suite | 2181 | `ctest --test-dir build --parallel <n> --output-on-failure` | partial |
| Native microbenchmarks | benchmarks | 16 | `build/bin/<benchmark> --benchmark_min_time=<duration> --benchmark_format=json` | none |
| Command-line samples | examples | 6 | `build/bin/compile <model> <output>; build/bin/testspeed [options] <model>; graphical samples requir…` | partial |
| Shipped MJCF model decks | examples | 86 | `load with a sample or API client; many are also exercised by parameterized C++ tests` | partial |
| Python bindings and utilities | test-suite | 28 | `pytest -v --pyargs mujoco after building and installing the source wheel` | partial |
| MJX JAX and Warp backends | test-suite | 28 | `pytest -n auto -v -k 'not IntegrationTest' --pyargs mujoco.mjx after packaging and installing MJX` | partial |
| WebAssembly bindings | test-suite | 4 | `npm --prefix wasm test after an Emscripten build` | partial |
| Unity integration | test-suite | 54 | `Unity editor and runtime test assemblies` | partial |
| Experimental OpenUSD integration | test-suite | 3 | `configure an OpenUSD-enabled native build and run through CTest` | partial |

Actually run: 5 of 5 attempted.

| run | ran | wall s | reproduced / reason | pitfalls |
|---|---|---:|---|---|
| `full-native-ctest` | yes | 35.22 | yes: all 2181 discovered tests passed; five were skipped by upstream conditions | Two documentation tests failed when CMake selected Python 3.8; setting Python3_EXECUTABLE to Python 3.12 fixed them. |
| `compile-humanoid` | yes | 0.45 | yes: two runs produced byte-identical MJB and MJZ outputs | - |
| `testspeed-dominos` | yes | 0.11 | yes: option parsing, 12.36 average PGS iterations, 382 contacts per step, and PASS matched across two runs | - |
| `iszero-microbenchmark` | yes | 0.58 | no reference | - |
| `python-introspection-direct` | yes | 18.75 | no: collection failed before tests because the pinned native _specs extension had not been built and installed | Importing any source-tree mujoco submodule executes package __init__ and requires the generated native extension; use the official wheel build/install workflow… |

Pitfalls of running the codebase: 7
- [build] scratch copy under the pipeline state root failed with disk quota exceeded -> place large scratch, dependency and build trees in a high-capacity workspace and keep only JSON/Markdown records in the state directory
- [build] Git rejected FetchContent repositories on the NAS as dubious ownership -> set safe.directory=* only in the environment of the CMake dependency-fetch command
- [build] GCC 9 failed the Abseil minimum-version check and GCC 11 failed pinned C++20 source syntax -> use GCC 13 or another compiler represented in the upstream CI matrix
- [build] graphical samples required Wayland/X11/OpenGL development headers not present in the initial environment -> build headless compile/testspeed targets explicitly and disable GUI examples for the core test build; install the full rendering development stack when rendering checks are needed
- [full-native-ctest] doc_test and mjcf_schema_test failed under automatically selected Python 3.8 -> configure with Python3_EXECUTABLE pointing to Python 3.12; both tests then pass
- [python-introspection-direct] pytest collection imported mujoco.__init__ and failed because _specs was absent -> build and install the source wheel before running the official Python binding suite
- [general] testspeed profiler timings differ across repeated runs -> grade deterministic physical/solver statistics or invariants, not wall-clock profiler fields

Not run:
- sample/render.cc and other graphical samples: the host lacked the complete OpenGL development stack and no display/headless rendering context was configured
- full Python binding pytest suite: the pinned Python extension wheel was not built and installed during this C++-focused native pass; the direct shortest test attempt documented the dependency
- MJX and Warp test suites: JAX and Warp are not installed in the authorized environment
- WebAssembly tests: Emscripten is not installed
- Unity editor and runtime tests: Unity and a C# runtime are not installed
- experimental OpenUSD tests: OpenUSD support and dependencies were disabled in the native build
- single-precision full CTest build: the repository requires single-precision coverage for upstream development, but no documented top-level CMake option was found; reproducing the CI-specific flag path is deferred to task check authoring

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `mujoco` | approved | There are no sibling modules. Language bindings, renderers, plugins and accelerator backends are retained inside this module because they use the same model schema, state and phys… | 2191 | 981525 | 2181 | unknown |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | unknown | ["mujoco"] | 0 | 0 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 0 | 0 | 0 |
| owned | 2191 | 114073515 | 981525 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 0 | 0 | 0 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 211 | files |
| `test_definitions` | 2644 | source-level test definitions |
| `collected_items` | 2181 | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- A later test survey must decide which of the 2,181 native collected items represent distinct scientific checks rather than API bookkeeping or duplicated parameterizations.
- Single-precision CTest coverage must be reproduced or explicitly ruled on before final task checks are selected.
- Optional Python, MJX/Warp, WASM, Unity, OpenUSD and rendering suites need separate dependency and runtime evaluation if any are selected as checks.
- Benchmark wall-clock measurements are host-dependent and should not become reward values.
- Which official native tests provide the strongest solver-visible numerical contracts while remaining self-contained under the eventual Docker charter?
- Can single-precision and alternate-build behavior be exercised without doubling task image size or suite runtime?
- CLI: repository/cache directory excluded from source accounting: .git

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
