# Official-test survey

The pinned source ships 257 test definitions across 49 test files. The example inventory identifies 38 independent scientific examples and 30 supporting files. `test-survey.json` contains one row per definition or independent example: 295 rows, 277 proposed checks and 18 omissions with reasons. The proposal retains every collected parameter case associated with a definition, including inherited implementations.

This is a source-phase proposal. No benchmark checks or calibrated tolerances exist yet. Missing optional solvers, legacy examples and the recorded Birch-coefficient failure remain explicit work items on proposed checks. Suitability does not mean execution succeeded.

## Execution and resources

Seven examples have measured attempt times; six completed and one failed to import FEniCS. The other 31 examples have not run. All 49 test files were attempted, followed by compatibility and class-level reruns. File-level results are not individual check timings. Their wall times are planning estimates in this survey, not independently measured runtimes for the 257 definitions. The 2 CPU and 4 GB entries describe the requested allocation; peak memory has not been measured. Summing file-level estimates across definitions would count shared work repeatedly.

## Scientific comparison

Policies are provisional. Compare fields using physical atom, species, tensor or spatial-grid identities. Canonicalize connectivity and periodic image keys across all arrays. Sampled particles and Langevin trajectories require distributions, conserved quantities or physical response statistics rather than random draws or storage slots. Initial configurations, numerical sensitivity and tolerances must be reviewed during task authoring.

One upstream MCFM clustering assertion compares the return values of two `list.sort()` calls, both `None`. Its proposed check must compare canonical cluster memberships directly. The original assertion is preserved in the vendored source.

## Omitted definitions

| Official definition | Reason |
|---|---|
| `tests/test_cluster_stable_sort.py::TestStableSortCluster::test_stable_sort_silicon` | The assertion requires a storage order. A correct port may permute atoms; geometry is covered by physical cluster and dislocation tests. |
| `tests/test_cluster_stable_sort.py::TestStableSortCluster::test_stable_sort_iron` | The assertion requires a storage order. A correct port may permute atoms; geometry is covered by physical cluster and dislocation tests. |
| `tests/test_committee.py::test_committeemember_initialize` | This definition checks initialization, membership, setters, validation-set selection or API errors. It produces no energy, force or uncertainty result for grading. |
| `tests/test_committee.py::test_committeemember_set_training_data` | This definition checks initialization, membership, setters, validation-set selection or API errors. It produces no energy, force or uncertainty result for grading. |
| `tests/test_committee.py::test_committeemember_is_sample_in_atoms` | This definition checks initialization, membership, setters, validation-set selection or API errors. It produces no energy, force or uncertainty result for grading. |
| `tests/test_committee.py::test_committeemember_setter` | This definition checks initialization, membership, setters, validation-set selection or API errors. It produces no energy, force or uncertainty result for grading. |
| `tests/test_committee.py::test_committee_initialize` | This definition checks initialization, membership, setters, validation-set selection or API errors. It produces no energy, force or uncertainty result for grading. |
| `tests/test_committee.py::test_committee_member` | This definition checks initialization, membership, setters, validation-set selection or API errors. It produces no energy, force or uncertainty result for grading. |
| `tests/test_committee.py::test_committee_set_internal_validation_set` | This definition checks initialization, membership, setters, validation-set selection or API errors. It produces no energy, force or uncertainty result for grading. |
| `tests/test_committee.py::test_committeeuncertainty_initialize` | This definition checks initialization, membership, setters, validation-set selection or API errors. It produces no energy, force or uncertainty result for grading. |
| `tests/test_ffi.py::test_ffi` | This test imports the extension and computes no scientific output. Keep import validation in the build smoke test. |
| `tests/test_io.py::TestEAMIO::test_savetbl_loadtbl` | This definition round-trips arbitrary random columns and text labels. It does not calculate a material property. |
| `tests/test_io.py::TestEAMIO::test_savetbl_loadtbl_text` | This definition round-trips arbitrary random columns and text labels. It does not calculate a material property. |
| `tests/test_neighbours.py::TestNeighbours::test_first_neighbours` | The expected output is an offset into the neighbour storage layout. Grade canonical physical connectivity in the neighbour and triplet checks instead. |
| `tests/test_neighbours.py::TestNeighbours::test_wrong_number_of_cutoffs` | This definition is an empty placeholder or an argument-error test; it supplies no scientific output. |
| `tests/test_neighbours.py::TestTriplets::test_get_jump_indicies` | The expected output is an offset into the neighbour storage layout. Grade canonical physical connectivity in the neighbour and triplet checks instead. |
| `tests/test_neighbours.py::TestNeighbourhood::test_pair_types` | This definition is an empty placeholder or an argument-error test; it supplies no scientific output. |
| `tests/test_numpy_tricks.py::test_mabincount` | The inputs are generic bin indices and weights, with no material system or physical observable. Retain the utility test upstream. |

## Remaining work

- Execute the unrun examples with their actual drivers and record dependencies, missing inputs and results.
- Resolve the Birch finite-difference comparison without treating the separate diagnostic as the official result.
- Measure each authored check, including its retained parameter cases, and peak memory on the chartered host.
- Derive and calibrate each physical comparison after source merge.
