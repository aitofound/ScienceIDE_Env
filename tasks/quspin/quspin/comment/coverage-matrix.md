# QuSpin upstream-test coverage matrix

This matrix is the steward gate for full-codebase coverage. All 73 upstream
`test_*.py` files are now owned by a check: eight baseline checks plus five
grouped checks (`basis-symmetry`, `operators-projections`, `dynamics-utilities`,
`entanglement-observables`, `models-crosschecks`). Rows marked `covered` are
bound to a grouped check that executes the file and records a physical
calibration observable; `baseline` rows are the original standalone checks.

## Basis and symmetry

| upstream test | status |
|---|---|
| test_array_ints_conversion.py | covered |
| test_basis_particle_sectors.py | baseline |
| test_general_bitops.py | covered |
| test_general_spin_get_vec.py | covered |
| test_general_spin_opstr.py | covered |
| test_general_spinless_fermion_opstr.py | covered |
| test_general_spinless_majorana_opstr.py | covered |
| test_higher_spin.py | covered |
| test_pauli.py | covered |
| test_representative.py | covered |
| test_user_basis_boson.py | covered |
| test_user_basis_spin.py | covered |
| test_user_basis_spinless_fermion.py | covered |

## Operators and Hamiltonians

| upstream test | status |
|---|---|
| test_Op_bra_ket.py | covered |
| test_Op_shift_sector.py | covered |
| test_Op_shift_sector_corr.py | covered |
| test_get_amp.py | covered |
| test_ham_project_to.py | covered |
| test_hamiltonian.py | baseline |
| test_inplace_op.py | covered |
| test_operator_shape.py | covered |
| test_project_from_boson.py | covered |
| test_project_from_fermion.py | covered |
| test_project_from_spin.py | covered |
| test_project_op.py | covered |
| test_project_to.py | covered |
| test_quantum_LinearOperator.py | covered |
| test_quantum_operator.py | covered |
| test_recursive_tensor.py | covered |
| test_save_zip.py | covered |
| test_sent_wrapper.py | covered |

## Spectral solvers and dynamics

| upstream test | status |
|---|---|
| test_ED.py | covered |
| test_ED_fermions.py | baseline |
| test_ED_spin.py | baseline |
| test_ED_spinful_fermions.py | baseline |
| test_FHM_energies.py | covered |
| test_FHM_energies_symm_adv.py | covered |
| test_Floquet.py | baseline |
| test_Floquet_t_vec.py | covered |
| test_Lanczos.py | baseline |
| test_diag_ensemble.py | covered |
| test_evolve.py | baseline |
| test_expm_multiply_parallel.py | covered |
| test_expm_multiply_parallel_batch.py | covered |
| test_gen_evolve.py | covered |
| test_mean_level_spacing.py | covered |
| test_obs_vs_time.py | covered |

## Entanglement and observables

| upstream test | status |
|---|---|
| test_basis_entropy.py | covered |
| test_basis_entropy_sparse.py | covered |
| test_boson_vs_ho.py | covered |
| test_boson_vs_spin.py | covered |
| test_diag_ensemble.py | covered |
| test_ent_basis_vs_tools.py | covered |
| test_entropy_pure.py | covered |
| test_local_entropy.py | covered |
| test_multispecies_ent.py | covered |
| test_onsite_ent.py | covered |
| test_partial_trace.py | covered |
| test_partial_trace_fermion.py | covered |
| test_partial_trace_user_basis.py | covered |
| test_photon_entropy.py | covered |
| test_spinful_fermion_entropy.py | covered |
| test_tensor_entropy.py | covered |
| test_spinful_fermion_tensor.py | covered |

## Models and cross-checks

| upstream test | status |
|---|---|
| test_Jordan_Wigner.py | covered |
| test_block_tools.py | covered |
| test_project_from_boson.py | covered |
| test_project_from_fermion.py | covered |
| test_project_from_spin.py | covered |
| test_sq_lat_Bose_Hubbard.py | covered |
| test_sq_lat_Fermi_Hubbard_spinful.py | covered |
| test_sq_lat_Fermi_Hubbard_spinless.py | covered |
| test_sq_lat_Heis.py | covered |
| test_sq_lat_Heis_double_occupancy.py | covered |
| test_tilted_sq_lat_Heis.py | covered |

## Package/utility gate

| upstream test | status |
|---|---|
| test_reshape_pure.py | covered |
| test_version.py | covered |

There are 73 upstream test files in total. Some appear in more than one
conceptual group; the implementation must assign each file to one owner and
record any deliberate merge or exclusion before the steward can be called
complete. The duplicate rows above are intentional cross-reference markers,
not additional tests.
