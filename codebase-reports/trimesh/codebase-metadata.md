<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `trimesh` | CLI |
| source payload | `code/trimesh/` | CLI |
| upstream pin | `fcf660feb0a14c68fd3945789e8ed77e260f9167` | human/state |
| license | `MIT` | human/state |
| source fingerprint | `91ed71a0011d8e51080fde6b466b07b688c1065f2485e153ff0ec2f5fd074a73` | CLI |
| size | 564 files / 33516694 bytes / 733924 text lines | CLI |

### Build and run (Step 1.2: what was actually built and run natively)

Build: `setuptools build backend declared in pyproject.toml; pip editable install`, ok, 2.502 s; commands: `python -m pip install --disable-pip-version-check -e .[easy,test]`; `python -m pip install --disable-pip-version-check -e . --no-deps --no-build-isolation --quiet`.
- build pitfall: The pipeline CLI requires Python >=3.11; the default Python 3.9.7 lacks tomllib.
- build pitfall: The pinned pipeline has a SyntaxError under Python 3.11 in reviewer.py due to a backslash inside an f-string expression; Python 3.12 parses it, so the isolated environment uses Python 3.12.14.

| family | kind | decks | how to run | references |
|---|---|---:|---|---|
| pytest suite | test-suite | 757 | `python -m pytest tests` | none |
| examples and notebooks | examples | 29 | `Run individual Python scripts or notebooks with their documented optional dependencies and interact…` | none |

Actually run: 8 of 8 attempted.

| run | ran | wall s | reproduced / reason | pitfalls |
|---|---|---:|---|---|
| `proximity-suite` | yes | 5.899 | official assertions passed: 12/12; no shipped numeric reference | - |
| `proximity-naive-comparison` | yes | 5.322 | official assertion passed: 1/1; indexed and naive results agree within upstream tolerance | - |
| `ray-functional-subset` | yes | 8.805 | official assertions passed: 9/9; 2 performance-oriented tests deselected | - |
| `ray-throughput` | yes | 3.694 | official assertion passed: 1/1; no shipped numeric reference | - |
| `ray-multiple-hits` | yes | 2.798 | official assertion passed: 1/1; no shipped numeric reference | - |
| `thickness-suite` | yes | 2.377 | official assertions passed: 5/5; no shipped numeric reference | - |
| `triangle-closest-subset` | yes | 2.029 | official assertions passed: 2/2; 4 unrelated tests deselected | - |
| `sample-surface-distance` | yes | 1.927 | official assertion passed: 1/1; signed distance of sampled surface points within upstream 1e-4 bound | - |

Pitfalls of running the codebase: 3
- [build] Default Python 3.9.7 cannot import pipeline tomllib dependency. -> Created a dedicated project-local CPython 3.12.14 Conda environment; no global environment was modified.
- [general] Tests import optional spatial and geometry packages and some ray tests select Embree when available. -> Installed the upstream easy and test extras only inside the dedicated environment and recorded exact resolved versions.
- [ray-functional-subset] The functional subset deliberately excludes the two explicit throughput/many-ray tests. -> Ran test_rps and test_multiple_hits separately under their own 180-second limits.

Not run:
- remaining 100+ upstream test files and interactive examples: Initial native probe was intentionally bounded to the requested closest-point/distance geometry and its direct ray/thickness/triangle/sample dependencies; exhaustive survey is a later pipeline step and is forbidden before the source PR is merged.
- registration tests that call closest_point: They exercise a broader non-rigid registration workflow rather than the core proximity contract and were not needed for this <=15-minute initial probe.

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `trimesh` | approved | This is the only module. Internal directories and optional backends are not separate modules because they share one distribution, object model, build, and test suite. | 564 | 733924 | unknown | unknown |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | unknown | ["trimesh"] | 0 | 0 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 0 | 0 | 0 |
| owned | 564 | 33516694 | 733924 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 0 | 0 | 0 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 116 | files |
| `test_definitions` | 757 | source-level test definitions |
| `collected_items` | unknown | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- No full-suite run or exhaustive official-test/example survey has been performed; that belongs after source merge.
- Optional dependencies and platform-specific backends will need per-check treatment in the task phase.

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
