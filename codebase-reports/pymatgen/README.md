# pymatgen: pinned source contribution

Upstream: https://github.com/materialsproject/pymatgen/tree/0428f232a569ffe6b16fa030d38ea35a56d70fd6

This is a filtered source snapshot, not a full upstream checkout. Every retained
file is an unchanged upstream Git blob. `source-manifest.json` records all
retained and omitted paths, original Git modes, object IDs and retained SHA-256
digests. It includes all tracked `src/` files, build metadata, the original MIT
license, and only the official tests and external fixtures listed below.
Unrelated test datasets, documentation, developer automation and submodules are
omitted. No raw VASP POTCAR test fixture is included. Runtime package data,
including core's potential summary statistics, is retained unchanged.

## Scope and attribution

The source remains under its upstream MIT license; preserve `code/pymatgen/LICENSE`
and source notices when redistributing. This contribution does not relicense
upstream source under the benchmark's task-content license.

Selected fixture paths:

- `test-files/analysis/pourbaix_diagram/pourbaix_test_data.json`

The fixtures come directly from the upstream revision above. They contain
composition/energy records and, for Pourbaix, solid/ionic entries with Materials
Project identifiers. No Materials Project API query or fresh data download was
performed. Credit: Pymatgen Development Team and the Materials Project.
Materials Project publishes its data under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/);
see its [data page](https://doi.org/10.17188/1281376) and
[citation guidance](https://materialsproject.org/about/cite).
The upstream fixtures do not identify a database release; none is invented here.
Fixture bytes and existing identifiers are unchanged. This is a provenance record
for the selected payload, not a determination about all datasets upstream.

## Reproducing the native check

Use Python 3.12, a C compiler and development headers. Build from clean copies
outside `code/` so generated artifacts cannot enter the vendored snapshot.
Install both approved source payloads in the same virtual environment, using the
versions below to replace missing Git-derived version metadata. The umbrella
version is a local development label, not a claimed upstream release tag.

```sh
export SETUPTOOLS_SCM_PRETEND_VERSION_FOR_PYMATGEN_CORE=2026.8.30
export SETUPTOOLS_SCM_PRETEND_VERSION_FOR_PYMATGEN=2026.8.30.dev1
python -m pip install ./core-copy ./pymatgen-copy pytest pytest-timeout
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 MPLBACKEND=Agg
export PMG_TEST_FILES_DIR="$(realpath core-copy/test-files)"
```

`native-requirements.txt` records the resolved native environment. Both wheels
built successfully from these filtered sources on Ubuntu. Run each test file
separately from its own source root, with a 180-second wall-clock limit:

```sh
timeout 180 python -m pytest -o addopts= --timeout=150 tests/analysis/interfaces/test_zsl.py
timeout 180 python -m pytest -o addopts= --timeout=150 tests/analysis/interfaces/test_substrate_analyzer.py
timeout 180 python -m pytest -o addopts= --timeout=150 tests/analysis/interfaces/test_coherent_interface.py
timeout 180 python -m pytest -o addopts= --timeout=150 tests/analysis/test_pourbaix_diagram.py -k "not test_multielement_parallel"
```

The native results appear in `codebase-metadata.json` and the HTML report.
Across the two sources, 137 tests passed. One CPU-wide multiprocessing test was
deselected. The phase-diagram suite includes plot tests, but plot appearance is
outside the approved scientific task scope. No Docker or accelerator test has
run, and no task checks or scientific thresholds are proposed by this source PR.

The two source PRs can be reviewed independently; subsequent interface and
Pourbaix tasks depend on the separately pinned pymatgen-core source.
