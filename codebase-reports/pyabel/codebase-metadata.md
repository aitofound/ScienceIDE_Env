<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `pyabel` | CLI |
| source payload | `code/pyabel/` | CLI |
| upstream pin | `f16723713e118a2eeef9bc7ba17efd57ad074093` | human/state |
| license | `MIT` | human/state |
| source fingerprint | `7330e559d0ff5d6c77467a05073d3ccb90268538fb03076776c337dc7b03931a` | CLI |
| size | 245 files / 2309177 bytes / 26431 text lines | CLI |

### Build and run (Step 1.2: what was actually built and run natively)

Build: `setuptools/pip with optional Cython extension`, ok, 11.0321 s; commands: `python -m pip install numpy==2.3.5 scipy==1.16.3 Cython==3.2.4 matplotlib==3.10.8 pytest==8.4.2 setuptools==80.9.0 whee…`; `python -m pip install --no-deps --no-build-isolation native`.
- build pitfall: NumPy and Cython must be present before installation to compile the optional direct extension.

| family | kind | decks | how to run | references |
|---|---|---:|---|---|
| basex | test-suite | 13 | `python -m pytest --pyargs abel.tests.test_basex -q -ra --durations=3` | partial |
| benchmarks | test-suite | 4 | `python -m pytest --pyargs abel.tests.test_benchmarks -q -ra --durations=3` | partial |
| dasch_methods | test-suite | 5 | `python -m pytest --pyargs abel.tests.test_dasch_methods -q -ra --durations=3` | partial |
| daun | test-suite | 7 | `python -m pytest --pyargs abel.tests.test_daun -q -ra --durations=3` | partial |
| direct | test-suite | 5 | `python -m pytest --pyargs abel.tests.test_direct -q -ra --durations=3` | partial |
| hansenlaw | test-suite | 7 | `python -m pytest --pyargs abel.tests.test_hansenlaw -q -ra --durations=3` | partial |
| linbasex | test-suite | 7 | `python -m pytest --pyargs abel.tests.test_linbasex -q -ra --durations=3` | partial |
| math | test-suite | 1 | `python -m pytest --pyargs abel.tests.test_math -q -ra --durations=3` | partial |
| onion_bordas | test-suite | 6 | `python -m pytest --pyargs abel.tests.test_onion_bordas -q -ra --durations=3` | partial |
| rbasex | test-suite | 15 | `python -m pytest --pyargs abel.tests.test_rbasex -q -ra --durations=3` | partial |
| tools_analytical | test-suite | 5 | `python -m pytest --pyargs abel.tests.test_tools_analytical -q -ra --durations=3` | partial |
| tools_center | test-suite | 6 | `python -m pytest --pyargs abel.tests.test_tools_center -q -ra --durations=3` | partial |
| tools_circularize | test-suite | 1 | `python -m pytest --pyargs abel.tests.test_tools_circularize -q -ra --durations=3` | partial |
| tools_distributions | test-suite | 16 | `python -m pytest --pyargs abel.tests.test_tools_distributions -q -ra --durations=3` | partial |
| tools_polynomial | test-suite | 16 | `python -m pytest --pyargs abel.tests.test_tools_polynomial -q -ra --durations=3` | partial |
| tools_symmetry | test-suite | 1 | `python -m pytest --pyargs abel.tests.test_tools_symmetry -q -ra --durations=3` | partial |
| tools_transform_pairs | test-suite | 1 | `python -m pytest --pyargs abel.tests.test_tools_transform_pairs -q -ra --durations=3` | partial |
| tools_vmi | test-suite | 6 | `python -m pytest --pyargs abel.tests.test_tools_vmi -q -ra --durations=3` | partial |
| transform | test-suite | 1 | `python -m pytest --pyargs abel.tests.test_transform -q -ra --durations=3` | partial |
| standalone scientific examples | examples | 22 | `From examples, run MPLBACKEND=Agg python example_NAME.py; bundled input images are under examples/d…` | partial |
| interactive Tk examples | examples | 2 | `Run example_GUI.py or example_simple_GUI.py with Tk and an interactive display.` | none |
| documentation Python files | other | 33 | `Inspect each script before execution: this inventory includes figure generators, long timing sweeps…` | partial |

Actually run: 27 of 27 attempted.

| run | ran | wall s | reproduced / reason | pitfalls |
|---|---|---:|---|---|
| `example-all-o2` | yes | 3.7664 | Completed successfully with the upstream inputs and settings. This plotting example has no automated stored-reference v… | Agg completes plotting without opening a GUI; plt.show emits an expected non-interactive-backend warning. |
| `example-all-dribinski` | yes | 0.5423 | Completed successfully with the upstream inputs and settings. This plotting example has no automated stored-reference v… | Agg completes plotting without opening a GUI; plt.show emits an expected non-interactive-backend warning. |
| `example-basex-gaussian` | yes | 0.3831 | Completed successfully with the upstream inputs and settings. This plotting example has no automated stored-reference v… | Agg completes plotting without opening a GUI; plt.show emits an expected non-interactive-backend warning. |
| `example-daun-degree` | yes | 0.3724 | Completed successfully with the upstream inputs and settings. This plotting example has no automated stored-reference v… | Agg completes plotting without opening a GUI; plt.show emits an expected non-interactive-backend warning. |
| `example-direct-gaussian` | yes | 0.3654 | Completed successfully with the upstream inputs and settings. This plotting example has no automated stored-reference v… | Agg completes plotting without opening a GUI; plt.show emits an expected non-interactive-backend warning. |
| `example-onion-bordas` | yes | 0.426 | Completed successfully with the upstream inputs and settings. This plotting example has no automated stored-reference v… | Agg completes plotting without opening a GUI; plt.show emits an expected non-interactive-backend warning. |
| `example-rbasex-multimass` | yes | 0.6148 | Completed successfully with the upstream inputs and settings. This plotting example has no automated stored-reference v… | Agg completes plotting without opening a GUI; plt.show emits an expected non-interactive-backend warning. |
| `example-simple` | yes | 20.7941 | Completed successfully with the upstream inputs and settings. This plotting example has no automated stored-reference v… | Agg completes plotting without opening a GUI; plt.show emits an expected non-interactive-backend warning.; First-use Matplotlib font-cache initialization is included in the measured wall time. |
| `test-basex` | yes | 11.3919 | 13 official tests passed; zero failures and zero skips. Upstream assertions use case-specific analytic, structural or c… | - |
| `test-benchmarks` | yes | 0.8594 | 4 official tests passed; zero failures and zero skips. Upstream assertions use case-specific analytic, structural or cr… | - |
| `test-dasch-methods` | yes | 0.3192 | 5 official tests passed; zero failures and zero skips. Upstream assertions use case-specific analytic, structural or cr… | - |
| `test-daun` | yes | 0.4119 | 7 official tests passed; zero failures and zero skips. Upstream assertions use case-specific analytic, structural or cr… | - |
| `test-direct` | yes | 0.3188 | 5 official tests passed; zero failures and zero skips. Upstream assertions use case-specific analytic, structural or cr… | - |
| `test-hansenlaw` | yes | 0.3799 | 7 official tests passed; zero failures and zero skips. Upstream assertions use case-specific analytic, structural or cr… | - |
| `test-linbasex` | yes | 0.8192 | 7 official tests passed; zero failures and zero skips. Upstream assertions use case-specific analytic, structural or cr… | - |
| `test-math` | yes | 0.3181 | 1 official tests passed; zero failures and zero skips. Upstream assertions use case-specific analytic, structural or cr… | - |
| `test-onion-bordas` | yes | 0.5632 | 6 official tests passed; zero failures and zero skips. Upstream assertions use case-specific analytic, structural or cr… | - |
| `test-rbasex` | yes | 1.7722 | 15 official tests passed; zero failures and zero skips. Upstream assertions use case-specific analytic, structural or c… | - |
| `test-tools-analytical` | yes | 1.5144 | 5 official tests passed; zero failures and zero skips. Upstream assertions use case-specific analytic, structural or cr… | - |
| `test-tools-center` | yes | 0.7266 | 6 official tests passed; zero failures and zero skips. Upstream assertions use case-specific analytic, structural or cr… | - |
| `test-tools-circularize` | yes | 0.4226 | 1 official tests passed; zero failures and zero skips. Upstream assertions use case-specific analytic, structural or cr… | - |
| `test-tools-distributions` | yes | 1.8931 | 16 official tests passed; zero failures and zero skips. Upstream assertions use case-specific analytic, structural or c… | - |
| `test-tools-polynomial` | yes | 0.4794 | 16 official tests passed; zero failures and zero skips. Upstream assertions use case-specific analytic, structural or c… | - |
| `test-tools-symmetry` | yes | 0.2934 | 1 official tests passed; zero failures and zero skips. Upstream assertions use case-specific analytic, structural or cr… | - |
| `test-tools-transform-pairs` | yes | 0.3155 | 1 official tests passed; zero failures and zero skips. Upstream assertions use case-specific analytic, structural or cr… | - |
| `test-tools-vmi` | yes | 0.3833 | 6 official tests passed; zero failures and zero skips. Upstream assertions use case-specific analytic, structural or cr… | - |
| `test-transform` | yes | 0.3129 | 1 official tests passed; zero failures and zero skips. Upstream assertions use case-specific analytic, structural or cr… | - |

Pitfalls of running the codebase: 5
- [build] The direct C implementation is optional; a Python-only installation can appear successful while its two backend-comparison tests are skipped. -> Preinstalled NumPy and Cython, built with --no-build-isolation, and verified abel.direct.cython_ext is True and abel.lib.direct imports from a compiled extension.
- [general] Tests and transform methods write basis caches; running from the vendored source could contaminate its exact upstream copy. -> Built an independent scratch copy, tested the installed package with --pyargs from outside the checkout, and retained code/pyabel unchanged.
- [general] Examples load data relative to their working directory and open plots. -> Ran from the scratch examples directory with MPLBACKEND=Agg and a separate Matplotlib cache; no image inputs were downloaded.
- [general] Native library thread pools and first-use caches affect wall-clock observations. -> Set OMP_NUM_THREADS, OPENBLAS_NUM_THREADS, MKL_NUM_THREADS and VECLIB_MAXIMUM_THREADS to 1. Reported times include process startup and any cache initialization and are build/run evidence, not comparative performance results.
- [general] Some upstream tests and example noise generators are unseeded. -> Recorded this behavior without modifying upstream. These native passes do not establish cross-run identity; task input reproducibility will require separate design after source merge.

Not run:
- examples/example_GUI.py: Interactive Tk application; no display-based investigation was performed.
- examples/example_O2_PES_PAD.py: Not selected for this representative source-stage example run. All official unit-test files ran; complete task coverage is a later source-merged phase.
- examples/example_all_Ominus.py: Not selected for this representative source-stage example run. All official unit-test files ran; complete task coverage is a later source-merged phase.
- examples/example_anisotropy_parameter.py: Not selected for this representative source-stage example run. All official unit-test files ran; complete task coverage is a later source-merged phase.
- examples/example_basex_photoelectron.py: Not selected for this representative source-stage example run. All official unit-test files ran; complete task coverage is a later source-merged phase.
- examples/example_basex_step.py: Not selected for this representative source-stage example run. All official u

[PR section truncated at 12,000 characters; canonical JSON and HTML retain the full report.]
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
