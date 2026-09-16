<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `neuron` | CLI |
| source payload | `code/neuron/` | CLI |
| upstream pin | `9.0.2` | human/state |
| license | `BSD-3-Clause` | human/state |
| source fingerprint | `3a600f98d5a30679c3d5da1a8a9af7791708759f4a54e626083d87ec46afda62` | CLI |
| size | 9161 files / 143318806 bytes / 2016827 text lines | CLI |

### Build and run (Step 1.2: what was actually built and run natively)

Build: `cmake`, ok, 149 s; commands: `# host: ubuntu@EC2 (8 vCPU, 30 GB RAM, Ubuntu 26.04, gcc 15.2.0, python 3.14); same recipe passed on RHEL8 + gcc 13.2 +…`; `sudo apt-get install -y build-essential cmake bison flex libreadline-dev python3-venv python3-dev git`; `git clone --branch 9.0.2 --recurse-submodules https://github.com/neuronsimulator/nrn nrn-9.0.2  # HEAD ba5783378c0aa1d9…`; `python3 -m venv venv && pip install numpy 'cython<3.1' pytest plotly matplotlib anywidget`; `cmake $SRC -DCMAKE_INSTALL_PREFIX=$PREFIX -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNRN_ENABLE_INTERVIEWS=OFF -DNRN_ENABLE_MP…`; `cmake --build . --parallel 8 --target install  # 142 s on 8 cores; 114 s on 16 cluster cores`; `export PATH=$PREFIX/bin:$PATH; export PYTHONPATH=$PREFIX/lib/python; python3 -c 'import neuron'  # prints 9.0.2`.
- build pitfall: Cython >= 3.1 crashes compiling share/lib/python/neuron/rxd/geometry3d/ctng.pyx ('Compiler crash in MarkParallelAssignments, AssertionError: <NOT CONSTANT>' with Cython 3.3.0); pin cython<3.1 (3.0.12 works)
- build pitfall: NMODL_ENABLE_PYTHON_BINDINGS must stay OFF: compiling the bindings exhausts RAM
- build pitfall: -DNRN_ENABLE_TESTS=ON does FetchContent at configure time: it needs network AND clones 5 test repos INTO the source tree at external/tests/ (nrntest, ringtest, reduced_dentate, testcorenrn, tqperf), so a tests-enabled configure dirties the checkout; build from a copy
- build pitfall: capture the build exit code directly; piping the build through tail/tee and reading $? reports the pipe's exit and can hide a failed compile
- build pitfall: the install is not relocatable and nrnivmodl needs a C compiler at runtime

| family | kind | decks | how to run | references |
|---|---|---:|---|---|
| pyinit | test-suite | 88 | `ctest -R '^pyinit'` | none |
| hoctests | test-suite | 39 | `ctest -R '^hoctests'` | partial |
| example_nmodl | examples | 36 | `ctest -R '^example_nmodl'` | none |
| api | test-suite | 6 | `ctest -R '^api::'` | none |
| nmodl_tests | test-suite | 5 | `ctest -R '^nmodl_tests'` | none |
| modlunit | test-suite | 5 | `ctest -R '^modlunit_'` | none |
| mpi_init | test-suite | 4 | `ctest -R '^mpi_init'` | none |
| unit_tests | test-suite | 3 | `ctest -R '^unit_tests'` | none |
| rxd | test-suite | 129 | `ctest -R 'rxdmod_tests' (one ctest entry running 129 pytest tests)` | shipped |
| external_nrntest | regression | 1 | `ctest -R '^external_nrntest' (one entry, ~90 HOC decks via cmpdatfile)` | shipped |
| external_ringtest | regression | 1 | `ctest -R '^external_ringtest'` | shipped |
| ringtest | regression | 1 | `ctest -R '^ringtest'` | shipped |
| pytest_basic | test-suite | 1 | `ctest -R '^pytest::'` | none |
| pytest_coreneuron | test-suite | 1 | `ctest -R '^pytest_coreneuron'` | none |
| connect_dend | regression | 1 | `ctest -R '^connect_dend'` | shipped |
| coverage_tests | test-suite | 1 | `ctest -R '^coverage_tests'` | none |
| datahandle | test-suite | 1 | `ctest -R '^datahandle'` | none |
| nocmodl | test-suite | 1 | `ctest -R '^nocmodl'` | none |
| benchmarks | benchmarks | 1 | `requires -DNRN_ENABLE_PERFORMANCE_TESTS=ON` | none |

Actually run: 11 of 11 attempted.

| run | ran | wall s | reproduced / reason | pitfalls |
|---|---|---:|---|---|
| `ctest-full-suite` | yes | 36.4 | 195 of 196 ctest entries pass on EC2 (gcc 15.2); the 1 failure is a reference mismatch detailed in run external-nrntest… | - |
| `rxd-pytest` | yes | 32.8 | yes: 129/129 pytest tests pass against the stored arrays in test/rxd/testdata (upstream's own comparison) | needs plotly, matplotlib AND anywidget in the python env; a missing plotly or matplotlib fails ALL 129 tests at pytest collection, missing anywidget fails test… |
| `external-nrntest` | yes | 21.5 | no: 1 of ~90 HOC decks (nmodl/CONDUCTANCE/test.hoc) differs from its shipped .cmp reference on gcc 15.2/Ubuntu 26.04 (a… | the deck runner writes .temp files into the fetched external/tests tree inside the source checkout |
| `hoctests` | yes | 12.6 | yes: upstream's own JSON/dat comparisons pass (39/39) | - |
| `example-nmodl` | yes | 8.5 | no reference (smoke runs; 36/36 pass) | nrnivmodl needs a working C compiler at test time |
| `pyinit` | yes | 5.3 | no reference (assert-based; 88/88 pass) | - |
| `nmodl-tests` | yes | 3.4 | no reference (assert-based; 5/5 pass) | - |
| `pytest-coreneuron-fallback` | yes | 3.3 | no reference (assert-based; passes in NEURON-only mode) | - |
| `unit-tests` | yes | 2.1 | no reference (Catch2 asserts; 3/3 pass) | - |
| `external-ringtest` | yes | 1.9 | yes: spike raster matches the shipped reference (upstream comparison) | - |
| `api-connect-cover-data-modlunit-mpi-nocmodl-ringtest-pytest` | yes | 2.7 | yes where a reference exists (connect_dend .ref, ringtest raster), no reference for the rest; all pass | - |

Pitfalls of running the codebase: 5
- [build] Cython 3.3.0: 'Compiler crash in MarkParallelAssignments' on rxd geometry3d/ctng.pyx -> pin cython<3.1 in the build env
- [rxd-pytest] all 129 rxd tests fail at pytest collection with ModuleNotFoundError (plotly, then matplotlib), and test_pltvar fails with 'Please install anywidget' -> pip install plotly matplotlib anywidget before running the suite
- [build] configure with NRN_ENABLE_TESTS=ON needs network (FetchContent of 5 GitHub test repos) and clones them into external/tests/ INSIDE the source tree -> build from a disposable copy of the checkout; pre-clone and set FETCHCONTENT_SOURCE_DIR_<NAME> on offline machines
- [external-nrntest] bytewise .cmp comparison fails on one deck (nmodl/CONDUCTANCE) under gcc 15.2 while passing under gcc 13.2 -> treat old nrntest decks as compiler-sensitive; pin the toolchain or exclude bytewise decks from cross-platform checks
- [general] python version drift changes nothing observable: suite passes on python 3.12.5 and 3.14 -> none needed; noted for reproducibility

Not run:
- test/benchmarks (performance suite): NRN_ENABLE_PERFORMANCE_TESTS=OFF in this build; benchmarks are not correctness tests
- MPI-dependent tests (test/parallel_tests, gjtests, music_tests, mpiexec variants): NRN_ENABLE_MPI=OFF; single-process build
- CoreNEURON/GPU test variants and external suites testcorenrn, tqperf, reduced_dentate, olfactory-bulb-3d, channel-bench…: NRN_ENABLE_CORENEURON=OFF (CPU-only headless build); these decks target the CoreNEURON backend or are long benchmarks
- InterViews GUI paths: NRN_ENABLE_INTERVIEWS=OFF; headless

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `neuron` | approved | {"note": "not supplied", "status": "unknown"} | 9161 | 2016827 | 196 | unknown |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 0 | 0 | 0 |
| owned | 9161 | 143318806 | 2016827 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 0 | 0 | 0 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | unknown | files |
| `test_definitions` | unknown | source-level test definitions |
| `collected_items` | 196 | framework-collected items |
| `inner_cases` | 219 | inner cases |

### Gaps and warnings
- CLI: repository/cache directory excluded from source accounting: .git

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
