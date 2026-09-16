<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `legume` | CLI |
| source payload | `code/legume/` | CLI |
| upstream pin | `edef021a98d5972ef52e3a65bd36ccfb742381be` | human/state |
| license | `MIT` | human/state |
| source fingerprint | `bd74c2205cbdc91611d447d70d65ac69a5268bce76c5e75fa4649ff264b9ff58` | CLI |
| size | 93 files / 11790565 bytes / 27661 text lines | CLI |

### Build and run (Step 1.2: what was actually built and run natively)

Build: `pip`, ok, 17.6 s; commands: `python3 -m venv venv`; `venv/bin/pip install -e ".[test,autodiff]"`.
- build pitfall: Pure Python: there is no compile step, so the 17.6 s is dependency resolution and wheel install, not a build. Two environments were built to separate version effects from real defects: python 3.14.6, numpy 2.5.3, scipy 1.18.1 (nominal) and python 3.11.10, numpy 1.26.4, scipy 1.13.1 (control).
- build pitfall: The autodiff extra installs autograd 1.9.1, which exposes no __version__ attribute; probing it with autograd.__version__ raises AttributeError and is not a sign of a broken install.
- build pitfall: Headless runs need MPLBACKEND=Agg: legume/viz.py and every example notebook import matplotlib.pyplot at module scope.

| family | kind | decks | how to run | references |
|---|---|---:|---|---|
| pytest suite | test-suite | 13 | `python -m pytest tests   (from the repository root; NOT bare `pytest`, see pitfalls)` | shipped |
| example notebooks | examples | 14 | `jupyter nbconvert --to notebook --execute <nb>.ipynb   (from a directory that is NOT docs/examples/…` | none |
| stale duplicate test tree | other | 9 | `not run: not an official test family` | partial |

Actually run: 25 of 26 attempted.

| run | ran | wall s | reproduced / reason | pitfalls |
|---|---|---:|---|---|
| `pytest-gme-te` | yes | 0.12 | yes: 6.5 digits (max relative 2.844e-07 against gme_square_te.mat, bands 1..9; upstream's own bound is 1e-4) | - |
| `pytest-gme-tm` | yes | 0.12 | yes: 6.5 digits (max relative 3.091e-07 against gme_square_tm.mat) | - |
| `pytest-gme-te-tm` | yes | 0.23 | yes: 6.5 digits (max relative 3.119e-07 against gme_square_te-tm.mat) | - |
| `pytest-gme-1layer-square` | yes | 0.11 | yes: within upstream's 1e-4 real and 1e-3 imaginary summed bound against gme_square.mat | - |
| `pytest-gme-1layer-rect` | yes | 0.07 | yes: within upstream's 1e-4 and 1e-3 bound against gme_rect.mat | - |
| `pytest-gme-1layer-hex` | yes | 0.64 | yes: within upstream's looser 1e-3 and 1e-1 bound against gme_hex.mat | Upstream's own imaginary-part bound here is 1e-1, two orders looser than the other decks; the hexagonal reference agrees far less well than the square ones. |
| `pytest-guided-modes` | yes | 3.15 | yes: summed absolute difference under 1e-5 against guided_modes_grating.npy | Exercises the scipy brentq root-find in legume/gme/slab_modes.py with gmode_tol 1e-12; see the tolerance-floor pitfall below. |
| `pytest-even-odd-separation` | yes | 35.31 | yes: 4,000 mode frequencies against the Pavia Fortran GME code, summed absolute difference under 5e-8 real and 5e-7 ima… | The longest test in the suite and the strongest anchor: it runs the same structure twice, once with use_sparse=False and once True, comparing both to the same … |
| `pytest-polariton` | yes | 12.02 | yes: against Polariton_en.npy, Polariton_fr.npy and Polariton_im.npy | - |
| `pytest-primitives` | yes | 0.03 | no reference: compares the autograd vjp of eigh and eigsh against a finite-difference gradient | These two pass, and they are the only autograd tests that do; they exercise legume/primitives.py directly rather than through GuidedModeExp. |
| `pytest-shapes` | yes | 0.16 | no reference: compares a polygon's Fourier transform against an analytic expression | - |
| `pytest-gme-grad` | yes | 0.45 | no: the test FAILS at this pin | UPSTREAM DEFECT. TypeError at legume/gme/gme.py:1720 in _compute_rad_components. Reproduced identically on python 3.14/numpy 2.5.3 and python 3.11/numpy 1.26.4… |
| `nb-01-shapes-layers` | yes | 4.6 | no reference | Run from a copy outside docs/examples/; see the shadow pitfall. |
| `nb-02-pwe-2d` | yes | 3.0 | no reference | The only example that exercises legume/pwe/ (2D plane-wave expansion); no test in tests/ covers that module. |
| `nb-03-multilayer-grating` | yes | 7.1 | no reference | - |
| `nb-04-phc-slab-bands` | yes | 5.7 | no reference | Reproduces Fig. 2(b) of chapter 8 of Joannopoulos et al., the comparison the README leads with. |
| `nb-11-xy-symmetry` | yes | 8.9 | no reference | - |
| `nb-12-kz-symmetry` | yes | 71.6 | no reference | The longest example that runs; still well under the three-minute cap. |
| `nb-13-bic-q-factor` | yes | 26.3 | no reference | - |
| `nb-14-polarization-mixing` | yes | 10.0 | no reference | - |
| `nb-07-enhancing-optimization` | no | - | needs the memory_profiler package, which is in no dependency list of the project (not in pyproject.toml dependencies no… | - |
| `nb-06-gme-autograd` | yes | 45.5 | no: the notebook FAILS at this pin | Same upstream defect as tests/test_gme_grad.py: TypeError at legume/gme/gme.py:1720, reached here through GuidedModeExp.compute_rad (gme.py:1808). This is the … |
| `nb-08-zero-index-bics` | yes | 7.7 | no: the notebook FAILS at this pin | Same upstream defect, reached through GuidedModeExp.compute_rad_sp (gme.py:1849) -> _compute_rad_components (gme.py:1720). |
| `nb-05-pwe-autograd` | yes | 38.3 | no: the notebook FAILS on numpy 2, and the failure is in the notebook's own printing, not in legume | numpy 2 incompatibility in the notebook, not a library defect. The cell computes grad_a = grad(of_sc)(pstart) successfully and then fails on the next line, '%1… |
| `nb-16-w1-waveguide-optimization` | yes | 25.8 | no: the notebook FAILS on numpy 2, same printing incompatibility as notebook 05 | Same numpy 2 size-1-array formatting incompatibility as notebook 05. |
| `nb-15-excitons-polaritons` | yes | 63.3 | no: the notebook FAILS on numpy 2, in its own input line | numpy 2 incompatibility in the notebook, not a library defect. The line osc_str_x = 2*10**19*np.array([1,0,0]) overflows: 2e19 exceeds the int64 maximum 9.223e… |

Pitfalls of running the codebase: 7
- [general] Every example notebook fails at its first legume call with AttributeError: module 'legume' has no attribute 'Lattice', and bare `pytest` at the repository root dies with 9 collection errors ('import file mismatch' on duplicate test basenames). -> Both symptoms have one cause: docs/examples/legume/ is a committed stale copy of an older repository root with no __init__.py, so it is picked up as a namespace package that shadows the installed legume whenever the working directory is docs/examples/, and its 9 test_*.py files collide by basename …
- [general] Two runs of the same input on the same build give different bits when the BLAS thread count differs; the graded values are stable for a fixed thread count. -> Pin OPENBLAS_NUM_THREADS, OMP_NUM_THREADS, MKL_NUM_THREADS, NUMEXPR_NUM_THREADS and VECLIB_MAXIMUM_THREADS to a fixed value in the check, never read from the host. Measured at gmax=7: 1 thread versus 8 threads leaves every frequency above 1e-6 agreeing to 9.6e-14 relative, but the bits differ. Matc…
- [general] Relative error on gme.freqs explodes to 34-51% while the absolute difference is only 4e-08. -> At the Gamma point the two lowest bands are numerically zero (1.94e-08 and 7.40e-08 at gmax=7), so relative error on them is meaningless noise. Restricted to |freq| > 1e-6, the same comparison is 9.6e-14. Upstream already works around this in its own tests by slicing off band 0 (gme.freqs[0, 1:]) a…
- [general] Perturbing an input by two ulps moves the output by ~1e-14 relative, and perturbing by 2000 ulps moves it by only ~1.2e-13: the response does not scale with the perturbation. -> legume/gme/slab_modes.py finds guided modes with scipy brentq under gmode_tol (default 1e-12), so the output carries a root-finder tolerance floor of about 1e-14 relative rather than propagating the input smoothly. Measured ratios of output move to input perturbation: 28.8x at 2 ulps, 2.4x at 20, 0…
- [nb-07-enhancing-optimization] ModuleNotFoundError: No module named 'memory_profiler'. -> Install memory_profiler. It is used by one example notebook but appears in no dependency list of the project, neither in pyproject.toml dependencies nor in the test, autodiff or docs extras.
- [general] Tests fail with FileNotFoundError on tests/data/*.mat when run from any directory other than the repository root. -> Every reference-loading test hardcodes a relative path of the form 'dot-slash tests/data/...'. The suite must be invoked with the repository root as the working directory.
- [general] Three of the fourteen example notebooks fail on numpy 2 while the pytest suite is unaffected. -> Upstream removed its numpy<2 constraint at 1.0.3 (commit 14b7b7e) and its CI runs only `python -m pytest tests`, so the examples were never exercised against numpy 2. Notebooks 05 and 16 fail on '%f' % size-1-array formatting; notebook 15 fails on an int64 overflow in 2*10**19*np.array([1,0,0]). Al…

Not run:
- docs/examples/legume/tests/ (9 stale test files): Not an official test family: a committed stale copy of an older repository root, divergent from tests/ and missing test_polariton.py and the three Polariton_*.npy references. Excluded from the survey and counted separately in the report.
- docs/examples/07_Enhancing_your_GME_optimization.ipynb: Needs memory_profiler, which is in no dependency list of the project; recorded as not run rather than run with an ad-hoc install.

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `legume` | approved | Single module: there is nothing to differentiate it from. The cut is the whole codebase over paths ['.'] under the skill's whole-codebase default. legume is one pip-installable pa… | 93 | 27661 | 27 | unknown |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 0 | 0 | 0 |
| owned | 93 | 11790565 | 27661 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 0 | 0 | 0 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 24 | files |
| `test_definitions` | 27 | source-level test definitions |
| `collected_items` | 27 | framework-collected items |
| `inner_cases` | 8 | inner cases |

### Gaps and warnings
- DEVIATION FROM THE PIN — READ codebase-reports/legume/DEVIATIONS.md FIRST. code/legume/ is NOT byte-for-byte upstream and is the first vendored tree in this benchmark that is not. The complete deviation is 93 files where the pin has 115: four in-place accumulations rewritten out-of-place (legume/gme/gme.py:1720 and :1743, legume/utils.py:28, legume/phc/layer.py:175 — six lines changed, four removed), and the deletion of docs/examples/legume/, a 22-file 3.7 MB fossil copy of an older repository …
- Two of the four upstream defects found at this pin are fixed by the deletion above rather than described as live: docs/examples/legume/ shadowed the installed package for notebooks run from their documented location, and its nine stale test basenames broke bare pytest at the repository root. Its README also carried the payload's only credential-shaped string, a defunct read-only Travis badge token that upstream had already removed from the real README in their PR #73. A

[PR section truncated at 12,000 characters; canonical JSON and HTML retain the full report.]
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
