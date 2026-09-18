<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `matscipy` | CLI |
| source payload | `code/matscipy/` | CLI |
| upstream pin | `b9530066f61223b3e4879aeb7424642932baeaab` | human/state |
| license | `LGPL-2.1` | human/state |
| source fingerprint | `95f2def17966228a767b8123120b3334689205aafe3d081e8c14e30d1f88b0e3` | CLI |
| size | 416 files / 30207912 bytes / 772423 text lines | CLI |

### Build and run (Step 1.2: what was actually built and run natively)

Build: `Meson through meson-python and pip wheel`, ok, 14.960079431000395 s; commands: `<local path redacted>`.
- build pitfall: Windows-to-WSL Git mode and newline interpretation generated invalid version 1.2.0.dirty; local core.filemode=false and core.autocrlf=true restored clean detection.
- build pitfall: ASE 3.29.0 removed legacy constraint filter imports used by official tests; ASE 3.26.0 collected all 873 items.
- build pitfall: Generated PKG-INFO with Name matscipy and Version 1.2.0 in the isolated build copy. The vendored source is unchanged; its git archive has neither .git nor PKG-INFO.

| family | kind | decks | how to run | references |
|---|---|---:|---|---|
| test_manybody_molecules | test-suite | 2 | `From tests/: python -m pytest manybody/test_manybody_molecules.py` | partial |
| test_manybody_potentials | test-suite | 4 | `From tests/: python -m pytest manybody/test_manybody_potentials.py` | partial |
| test_newmb | test-suite | 10 | `From tests/: python -m pytest manybody/test_newmb.py` | partial |
| test_angle_distribution | test-suite | 4 | `From tests/: python -m pytest test_angle_distribution.py` | partial |
| test_atomic_strain | test-suite | 1 | `From tests/: python -m pytest test_atomic_strain.py` | partial |
| test_bop | test-suite | 11 | `From tests/: python -m pytest test_bop.py` | partial |
| test_build_3D_crack | test-suite | 2 | `From tests/: python -m pytest test_build_3D_crack.py` | partial |
| test_bulk_properties | test-suite | 1 | `From tests/: python -m pytest test_bulk_properties.py` | partial |
| test_cauchy_born_corrector | test-suite | 10 | `From tests/: python -m pytest test_cauchy_born_corrector.py` | partial |
| test_cluster_stable_sort | test-suite | 2 | `From tests/: python -m pytest test_cluster_stable_sort.py` | partial |
| test_committee | test-suite | 12 | `From tests/: python -m pytest test_committee.py` | partial |
| test_crack | test-suite | 2 | `From tests/: python -m pytest test_crack.py` | partial |
| test_cubic_crystal_crack | test-suite | 6 | `From tests/: python -m pytest test_cubic_crystal_crack.py` | partial |
| test_dislocation | test-suite | 33 | `From tests/: python -m pytest test_dislocation.py` | partial |
| test_eam_average_atom | test-suite | 2 | `From tests/: python -m pytest test_eam_average_atom.py` | partial |
| test_eam_calculator | test-suite | 7 | `From tests/: python -m pytest test_eam_calculator.py` | partial |
| test_eam_calculator_forces_and_hessian | test-suite | 6 | `From tests/: python -m pytest test_eam_calculator_forces_and_hessian.py` | partial |
| test_eam_io | test-suite | 4 | `From tests/: python -m pytest test_eam_io.py` | partial |
| test_elastic_moduli | test-suite | 2 | `From tests/: python -m pytest test_elastic_moduli.py` | partial |
| test_electrochemistry_cli | test-suite | 6 | `From tests/: python -m pytest test_electrochemistry_cli.py` | partial |
| test_energy_release | test-suite | 1 | `From tests/: python -m pytest test_energy_release.py` | partial |
| test_ewald | test-suite | 12 | `From tests/: python -m pytest test_ewald.py` | partial |
| test_ffi | test-suite | 1 | `From tests/: python -m pytest test_ffi.py` | partial |
| test_fit_elastic_constants | test-suite | 11 | `From tests/: python -m pytest test_fit_elastic_constants.py` | partial |
| test_full_to_Voigt | test-suite | 4 | `From tests/: python -m pytest test_full_to_Voigt.py` | partial |
| test_gamma_surface | test-suite | 3 | `From tests/: python -m pytest test_gamma_surface.py` | partial |
| test_hessian_finite_differences | test-suite | 3 | `From tests/: python -m pytest test_hessian_finite_differences.py` | partial |
| test_hessian_precon | test-suite | 3 | `From tests/: python -m pytest test_hessian_precon.py` | partial |
| test_hydrogenate | test-suite | 1 | `From tests/: python -m pytest test_hydrogenate.py` | partial |
| test_idealbrittlesolid | test-suite | 3 | `From tests/: python -m pytest test_idealbrittlesolid.py` | partial |

Actually run: 64 of 64 attempted.

| run | ran | wall s | reproduced / reason | pitfalls |
|---|---|---:|---|---|
| `tests-manybody-test-manybody-molecules` | yes | 15.359504560000005 | Official assertions completed; JUnit counts: {"tests": 18, "failures": 0, "errors": 0, "skipped": 9}. Decimal agreement… | - |
| `tests-manybody-test-manybody-potentials` | yes | 9.945053729999927 | Official assertions completed; JUnit counts: {"tests": 19, "failures": 0, "errors": 0, "skipped": 4}. Decimal agreement… | - |
| `tests-manybody-test-newmb` | yes | 35.41662078499985 | Official run failed or timed out: FAILED manybody/test_newmb.py::test_birch_constants[diamond-distance=5.431-rattle=0-S… | FAILED manybody/test_newmb.py::test_birch_constants[diamond-distance=5.431-rattle=0-SimpleAngle~cutoff]; 1 failed, 194 passed, 42 xfailed, 28 xpassed, 22655 warnings in 33.52s |
| `tests-test-angle-distribution` | yes | 3.326273280000123 | Official assertions completed; JUnit counts: {"tests": 4, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement … | - |
| `tests-test-atomic-strain` | yes | 3.4764132650000192 | Official assertions completed; JUnit counts: {"tests": 1, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement … | - |
| `tests-test-bop` | yes | 160.47519797699988 | Official assertions completed; JUnit counts: {"tests": 117, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreemen… | - |
| `tests-test-build-3d-crack` | yes | 4.780660490000173 | Official assertions completed; JUnit counts: {"tests": 2, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement … | - |
| `tests-test-bulk-properties` | yes | 6.935311617000025 | Official assertions completed; JUnit counts: {"tests": 13, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement… | - |
| `tests-test-cauchy-born-corrector` | yes | 4.978242676000264 | Official run failed or timed out: FAILED test_cauchy_born_corrector.py::TestPredictCauchyBornShifts::test_taylor_model_… | FAILED test_cauchy_born_corrector.py::TestPredictCauchyBornShifts::test_taylor_model_E; FAILED test_cauchy_born_corrector.py::TestPredictCauchyBornShifts::test_taylor_model_F; 9 failed, 1 passed, 82 warnings in 4.15s |
| `tests-test-cluster-stable-sort` | yes | 2.87301579699988 | Official assertions completed; JUnit counts: {"tests": 2, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement … | - |
| `tests-test-committee` | yes | 3.3737278280000282 | Official assertions completed; JUnit counts: {"tests": 12, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement… | - |
| `tests-test-crack` | yes | 4.476157465000142 | No tests collected; pytest exit code 5. The optional Atomistica dependency is absent. | The upstream module conditionally defines its test classes only when Atomistica is available. |
| `tests-test-cubic-crystal-crack` | yes | 6.535898968999845 | Official assertions completed; JUnit counts: {"tests": 6, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement … | - |
| `tests-test-dislocation` | yes | 180.04082355300034 | Official run failed or timed out: | File exceeded the 180-second investigation limit; split by official test item for follow-up. |
| `tests-test-eam-average-atom` | yes | 6.483876232000057 | Official assertions completed; JUnit counts: {"tests": 2, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement … | - |
| `tests-test-eam-calculator` | yes | 23.125264129000243 | Official assertions completed; JUnit counts: {"tests": 7, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement … | - |
| `tests-test-eam-calculator-forces-and-hessian` | yes | 35.102884942999935 | Official assertions completed; JUnit counts: {"tests": 6, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement … | - |
| `tests-test-eam-io` | yes | 6.33552739400011 | Official run failed or timed out: FAILED test_eam_io.py::test_eam_read_write - TypeError: only 0-dimensional ar...; 1 f… | FAILED test_eam_io.py::test_eam_read_write - TypeError: only 0-dimensional ar...; 1 failed, 2 passed, 1 skipped in 4.54s |
| `tests-test-elastic-moduli` | yes | 3.4262715669997306 | Official assertions completed; JUnit counts: {"tests": 2, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement … | - |
| `tests-test-electrochemistry-cli` | yes | 5.232526159999907 | Official run failed or timed out: FAILED test_electrochemistry_cli.py::ElectrochemistryCliTest::test_pnp_c2d_pipeline_m… | FAILED test_electrochemistry_cli.py::ElectrochemistryCliTest::test_pnp_c2d_pipeline_mode; FAILED test_electrochemistry_cli.py::ElectrochemistryCliTest::test_pnp_output_format_npz; FAILED test_electrochemistry_cli.py::ElectrochemistryCliTest::test_pnp_output_format_txt |
| `tests-test-energy-release` | yes | 4.630473885999891 | The only collected test was skipped; no scientific assertion was executed. | - |
| `tests-test-ewald` | yes | 50.523844500999985 | Official assertions completed; JUnit counts: {"tests": 27, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement… | - |
| `tests-test-ffi` | yes | 1.6218094659998314 | Official assertions completed; JUnit counts: {"tests": 1, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement … | - |
| `tests-test-fit-elastic-constants` | yes | 3.978288243999941 | No tests collected; pytest exit code 5. The optional QUIP dependency is absent. | The upstream module conditionally defines its test classes only when QUIP is available. |
| `tests-test-full-to-voigt` | yes | 2.924974488999851 | Official assertions completed; JUnit counts: {"tests": 4, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement … | - |
| `tests-test-gamma-surface` | yes | 5.129984995000086 | Official assertions completed; JUnit counts: {"tests": 3, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement … | - |
| `tests-test-hessian-finite-differences` | yes | 5.432464516999971 | Official assertions completed; JUnit counts: {"tests": 3, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement … | - |
| `tests-test-hessian-precon` | yes | 14.211243806999846 | Official assertions completed; JUnit counts: {"tests": 3, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement … | - |
| `tests-test-hydrogenate` | yes | 3.8279241219997857 | Official assertions completed; JUnit counts: {"tests": 1, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement … | - |
| `tests-test-idealbrittlesolid` | yes | 5.683581807999872 | Official assertions completed; JUnit counts: {"tests": 3, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement … | - |
| `tests-test-invariants` | yes | 4.580557944999782 | Official assertions completed; JUnit counts: {"tests": 1, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement … | - |
| `tests-test-io` | yes | 4.029585718999897 | Official assertions completed; JUnit counts: {"tests": 5, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement … | - |
| `tests-test-mcfm` | yes | 4.279175599000155 | Official assertions completed; JUnit counts: {"tests": 3, "failures": 0, "errors": 0, "skipped": 0}. Decimal agreement … | - |
| `tests-test-neighbours` | yes | 5.333333606999986 | Official assertions completed; JUnit counts: {"tests": 19, "f

[PR section truncated at 12,000 characters; canonical JSON and HTML retain the full report.]
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
