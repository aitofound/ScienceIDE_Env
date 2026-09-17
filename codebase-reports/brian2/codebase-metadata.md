<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `brian2` | CLI |
| source payload | `code/brian2/` | CLI |
| upstream pin | `2.10.1` | human/state |
| license | `CeCILL-2.1` | human/state |
| source fingerprint | `581933e2e03f627898c8ccdfaa08a876eb74680653fea69ff57927395ba4a50a` | CLI |
| size | 552 files / 5337228 bytes / 144010 text lines | CLI |

### Build and run (Step 1.2: what was actually built and run natively)

Build: `pip/setuptools (setup.py compiles 2 Cython extensions: synapses/cythonspikequeue.pyx, memory/cythondynamicarray.pyx)`, ok, 26 s; commands: `# host: ubuntu@EC2 (8 vCPU, 30 GB RAM, Ubuntu 26.04, gcc 15.2.0, python 3.14.4)`; `git clone --branch 2.10.1 https://github.com/brian-team/brian2 brian2-2.10.1  # HEAD 3fe5ac9a9898210319a159160fd880d5a4…`; `cp -r brian2-2.10.1 brian2-build  # scratch copy, pristine checkout untouched`; `python3 -m venv brian2-venv && source brian2-venv/bin/activate`; `pip install cython pytest pytest-timeout matplotlib scipy`; `pip install -e brian2-build  # 26 s wall; resolves numpy 2.5.3, cython 3.1.3 (pin is <3.1.4), sympy 1.14.0, jinja2, pyp…`; `python -c "import brian2; print(brian2.__version__)"  # 2.10.1`.
- build pitfall: pyproject pins cython>=0.29.21,<3.1.4; pip resolved 3.1.3, fine today, but a future cython release cannot be picked up
- build pitfall: <local path redacted>

| family | kind | decks | how to run | references |
|---|---|---:|---|---|
| official pytest suite | test-suite | 686 | `python -m pytest brian2/tests/<file> (config brian2/tests/pytest.ini); official driver brian2.test(…` | none |
| examples | examples | 100 | `python examples/<name>.py with MPLBACKEND=Agg (all end in matplotlib show())` | none |

Actually run: 7 of 7 attempted.

| run | ran | wall s | reproduced / reason | pitfalls |
|---|---|---:|---|---|
| `pytest-test-equations` | yes | 0.9 | 14 passed, 1 skipped | - |
| `pytest-test-units` | yes | 1.0 | 38 passed | - |
| `pytest-test-neurongroup` | yes | 115.9 | 73 passed, 4 skipped, 1 failed (test_namespace_warnings, order-dependent: passes alone) | test_namespace_warnings fails in full-file order, passes in isolation |
| `pytest-test-network-perfile` | yes | 120 | 12 passed then 4 schedule tests FAILED (test_network_schedule_change, test_schedule_warning, test_scheduling_summary_ma… | full-file run leaks scheduling state across tests in this environment; run affected tests in their own pytest process |
| `pytest-test-synapses-head` | yes | 180 | 12 tests passed before the 180 s cap cut the run; no failures seen | largest test file (3836 lines); cython codegen compile cost dominates early tests |
| `example-cuba` | yes | 15 | runs clean, silent (plot suppressed by Agg); Brette 2007 CUBA benchmark, 4000 LIF neurons, 1 s biological time | - |
| `example-cobahh` | yes | 20 | runs clean: reports "1. s (100%) simulated in 6s" after cython compile; Brette 2007 COBAHH benchmark, 4000 HH neurons | - |

Pitfalls of running the codebase: 5
- [brian2/tests/test_network.py full-file run] 4 schedule-related tests fail and test_magic_network hangs >60 s when the whole file runs in one pytest process (python 3.14.4, numpy 2.5.3); all 5 pass together in isolation in 21 s -> run tests in per-file or per-test pytest processes; do not chain the whole file in one process in this environment
- [brian2/tests/test_neurongroup.py::test_namespace_warnings] AssertionError on warning count in full-file order, passes alone (warning registry leaks across tests) -> own pytest process
- [cython runtime target (default when a compiler exists)] first run of any model pays gcc compilation of generated code objects; silent tens of seconds -> warm the on-disk cache, or set prefs.codegen.target='numpy' for compile-free runs
- [cross-target reproducibility] same brian2.seed() gives different results on runtime vs cpp_standalone device (different RNG streams) -> grade within one target only, or use conftest fake_randn pattern for cross-target checks
- [examples] all end in matplotlib show(); block under a display, silent under Agg -> MPLBACKEND=Agg and grade numeric quantities, not figures

Not run:
- brian2.test() full official driver (all targets): runs the 686-test suite up to 4 times (numpy, cython, standalone phases); far beyond the 3-minute-per-run investigation cap; deferred to check authoring
- gsl-marked tests: need the GNU Scientific Library headers; excluded by the official driver by default
- openmp standalone tests: threading mode off by default; grading will pin single thread for determinism
- frompapers/ long examples and multiprocessing/ examples: minutes-long or multi-process; smoke coverage via CUBA/COBAHH is representative for Step 1.2

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `brian2` | approved | single-module codebase; no sibling modules to differ from | 552 | 144010 | 769 | unknown |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 0 | 0 | 0 |
| owned | 552 | 5337228 | 144010 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 0 | 0 | 0 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 36 | files |
| `test_definitions` | 686 | source-level test definitions |
| `collected_items` | 769 | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- CLI: build_and_run.build.pitfalls[1]: local/private absolute path redacted
- CLI: repository/cache directory excluded from source accounting: .git

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
