<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `tmm` | CLI |
| source payload | `code/tmm/` | CLI |
| upstream pin | `462b63b517472b3b18245c98d70e700f4d312866` | human/state |
| license | `MIT` | human/state |
| source fingerprint | `aeac557f25691704a2e48cb30844cec204c6296ed6fd6b011c260a26697a3773` | CLI |
| size | 14 files / 425905 bytes / 3158 text lines | CLI |

### Build and run (Step 1.2: what was actually built and run natively)

Build: `pip (setuptools via pyproject.toml; pure Python, nothing compiles; the package 'tmm' is mapped onto the repository root)`, ok, 3.8 s; commands: `python3.12 -m venv venv-tmm && venv-tmm/bin/pip install numpy scipy matplotlib   # numpy 2.5.3, scipy 1.18.1, matplotli…`; `SRC=<scratch copy of the checkout, directory named tmm>`; `python -m pip install --no-deps --target $SITE $SRC   # SRC = the scratch copy of the checkout, SITE = an empty target …`; `PKG=<a directory holding a symlink named tmm to the checkout>   # the alternative: the checkout itself as the package d…`.
- build pitfall: pip install --no-build-isolation fails in a bare Python 3.12 venv with ModuleNotFoundError: No module named 'setuptools' (venv no longer seeds setuptools); use the default build isolation or pip install setuptools first
- build pitfall: the package is flat: pyproject.toml maps package 'tmm' to '.', so the checkout directory must be named tmm (or symlinked as such) to be importable without installing; the vendored tree under code/tmm/ has the right name by construction

| family | kind | decks | how to run | references |
|---|---|---:|---|---|
| tests.py | test-suite | 8 | `python -c "import tmm.tests as t; t.run_all()"   (no pytest, no assertions: each test prints differ…` | partial |
| examples.py | examples | 6 | `python -c "import tmm.examples as ex; ex.sample1()"  (each sample builds a Matplotlib figure and ca…` | none |
| examples.ipynb | tutorials | 6 | `jupyter nbconvert --execute examples.ipynb` | none |
| manual | other | 0 | `not runnable (documentation; manual_source/manual.lyx is its LyX source)` | none |

Actually run: 15 of 16 attempted.

| run | ran | wall s | reproduced / reason | pitfalls |
|---|---|---:|---|---|
| `run-all` | yes | 0.37 | yes: 15 digits (basic_test and position_resolved_test against the embedded Mathematica values, largest difference fract… | nothing asserts and the exit status is always 0: a check must recompute the graded quantities itself, never parse pass/fail; the opacity warning is printed once per process through a module global, so stdout depends on which tests ran earlier in the same process; grade numbers, never… |
| `basic-test` | yes | 0.0012 | yes: 15 digits (r, t, R, T for s and p, psi and Delta against Mathematica; largest difference fraction 1.0e-15 on t_s, … | - |
| `position-resolved-test` | yes | 0.0021 | yes: 15 digits (kz, Poynting vector and absorption at one depth against Mathematica, 2e-16 to 4e-16); absorption sums t… | - |
| `position-resolved-test2` | yes | 0.002 | no reference (identities only): absorption sums to 1 exactly with complex incident and final media; continuity and end-… | - |
| `absorp-analytic-fn-test` | yes | 0.0004 | no reference (identities only): the analytic absorption function equals position_resolved to 0 and 1.7e-16, and its fli… | - |
| `incoherent-test` | yes | 0.25 | no reference (identities only): inc_tmm against the closed-form three-layer sums and against coh_tmm at 1e-16 or exactl… | the 'discrepancy' lines are 1e-5 by design (finite averaging), not failures; a check grades the computed values, not their closeness to zero |
| `rt-test` | yes | 0.0002 | no reference (identities only): R+T=1 at a single interface to 4.4e-16, power_entering equals T to 3.5e-16 | - |
| `coh-overflow-test` | yes | 0.0011 | no reference (identity only): the vw_list of the five-layer stack with a layer of imag(delta)=18850 agrees with the tru… | imag(delta) > 35 is clamped to 35 in coh_tmm and a warning is printed once per process; amplitudes behind the opaque layer come out at 1e-16 to 1e-32, so a poi… |
| `inc-overflow-test` | yes | 0.0005 | no reference (identity only): power_entering_list of the five-layer stack agrees with the three-layer stack in its firs… | single-pass transmission P < 1e-30 is floored to 1e-30 in inc_tmm, which sets the 1.97e-30 entries behind the opaque layer; the printed list format depends on … |
| `sample1` | yes | 0.166 | no reference (a plot): reflection versus wavenumber, 400 points, normal incidence and 45-degree unpolarised; 1200 coh_t… | every sample ends in plt.show(), which blocks under an interactive backend; run with MPLBACKEND=Agg and a stubbed plt.show; Python 3.12 prints SyntaxWarning: invalid escape sequence '\c' for the '$^\circ$' titles; harmless |
| `sample2` | yes | 0.071 | no reference (a plot): transmission of a 300 nm film whose complex index is a scipy interp1d quadratic through five tab… | depends on scipy.interpolate.interp1d(kind='quadratic') on complex data; scipy calls interp1d legacy but it still runs on 1.18.1 |
| `sample3` | yes | 0.032 | not compared (a plot against Handbook of Ellipsometry Fig. 1.14): psi and Delta of air/SiO2/Si at 70 degrees and 633 nm… | - |
| `sample4` | yes | 0.02 | no reference (a plot): Poynting vector and absorption versus depth at 1000 positions across a two-film stack, one coh_t… | - |
| `sample5` | yes | 8.43 | not compared (printed rgb=[0.0728 0.1514 0.4324], xyY=[0.2170 0.2073 0.1549] for 300 nm SiO2 on Si and a colour strip a… | colorpy 0.1.1 from PyPI is Python 2 only: import fails with ModuleNotFoundError: No module named 'colormodels'; the fish2000/ColorPy fork (LGPL-3.0) at commit …; colorpy.plots.spectrum_plot saves temp_plot.png into the current working directory; run in a scratch cwd; the irgb values are gamma-corrected integers 0-255, a discrete output; the linear rgb and xyY are the continuous quantities |
| `sample6` | yes | 0.046 | not compared (a plot against Mater. Trans. 51 (2010) Fig. 6a): p-polarised reflection of glass/Cr 5 nm/Au 30 nm/air at … | - |
| `examples-notebook` | no | - | its code cells are the six samples of examples.py pasted with their plots; the same code paths were run through example… | - |

Pitfalls of running the codebase: 8
- [build] pip install --no-build-isolation: ModuleNotFoundError: No module named 'setuptools' -> let pip build in isolation (it fetches setuptools) or pip install setuptools first; or skip installing and put a directory named tmm on PYTHONPATH
- [run-all] the tests print difference fractions and always exit 0; nothing asserts -> a check recomputes the graded quantities (r, t, R, T, psi, Delta, kz, Poynting, absorption, vw_list, power_entering) and grades those; the printed text is never the observable
- [run-all] stdout differs between processes depending on whether coh_overflow_test ran first (the opacity warning is printed once per process via a module global) and on the numpy major version (np.float64(...) reprs) -> grade numbers written to files, not stdout
- [sample1] plt.show() blocks under an interactive backend -> MPLBACKEND=Agg and stub plt.show (or save figures instead)
- [sample5] PyPI colorpy 0.1.1 fails to import on Python 3 (No module named 'colormodels') -> pip install the fish2000 ColorPy fork from GitHub at commit 4c9e5b2e334851b42626f9a1eda31197739b097e (github.com/fish2000/ColorPy) (LGPL-3.0); then examples.colors_were_imported is True and sample5 runs in 8.4 s
- [sample5] temp_plot.png appears in the working directory -> run in a scratch cwd; colorpy.plots.spectrum_plot saves its figure unconditionally
- [general] Python 3.12 SyntaxWarning: invalid escape sequence '\c' when examples.py is compiled -> harmless; comes from '$^\circ$' in plot titles
- [general] is_forward_angle decides the forward wave by comparing imag(n cos theta) with 100*EPSILON, and coh_tmm clamps imag(delta) > 35 while inc_tmm floors P < 1e-30 -> discrete choices in the source; the official decks sit far from the thresholds (imag(delta)=18850 in the overflow tests), which a rubric warrant should say

Not run:
- examples.ipynb: the same six samples as examples.py; running the .py exercised every code path the notebook holds
- manual.pdf and manual_source/manual.lyx: documentation, nothing to run

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `tmm` | approved | single whole-codebase module | 14 | 3158 | 14 | unknown |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `shared-infrastructure` | unknown | ["tmm"] | 0 | 0 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 0 | 0 | 0 |
| owned | 14 | 425905 | 3158 |
| overlapping_owned | 0 | 0 | 0 |
| unclassified | 0 | 0 | 0 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 3 | files |
| `test_definitions` | 14 | source-level test definitions |
| `collected_items` | 14 | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- upstream ships reference numbers only inside basic_test and position_resolved_test (Mathematica values); every other check is anchored by the pinned build's own output
- the repository has no tags: the pin is the commit that set version 0.2.0, verified against the PyPI sdist
- whether the colour example (sample5) should become a check, since it needs the LGPL-3.0 fish2000 ColorPy fork installed in the images (the packager's default: yes, installed from its pinned commit, never vendored)

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
