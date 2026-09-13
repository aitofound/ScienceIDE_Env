# QuSpin upstream-test coverage matrix

This matrix is the steward gate for full-codebase coverage. A file is not
counted as covered until it has a named check, a physical observable, a
nominal/variant input pair, and a passing self-validation record. The current
PR baseline covers only the eight rows marked `baseline`.

## Basis and symmetry

| upstream test | status |
|---|---|
| test_array_ints_conversion.py | pending |
| test_basis_particle_sectors.py | baseline |
| test_general_bitops.py | pending |
| test_general_spin_get_vec.py | pending |
| test_general_spin_opstr.py | pending |
| test_general_spinless_fermion_opstr.py | pending |
| test_general_spinless_majorana_opstr.py | pending |
| test_higher_spin.py | pending |
| test_pauli.py | pending |
| test_representative.py | pending |
| test_user_basis_boson.py | pending |
| test_user_basis_spin.py | pending |
| test_user_basis_spinless_fermion.py | pending |

## Operators and Hamiltonians

| upstream test | status |
|---|---|
| test_Op_bra_ket.py | pending |
| test_Op_shift_sector.py | pending |
| test_Op_shift_sector_corr.py | pending |
| test_get_amp.py | pending |
| test_ham_project_to.py | pending |
| test_hamiltonian.py | baseline |
| test_inplace_op.py | pending |
| test_operator_shape.py | pending |
| test_project_from_boson.py | pending |
| test_project_from_fermion.py | pending |
| test_project_from_spin.py | pending |
| test_project_op.py | pending |
| test_project_to.py | pending |
| test_quantum_LinearOperator.py | pending |
| test_quantum_operator.py | pending |
| test_recursive_tensor.py | pending |
| test_save_zip.py | pending |
| test_sent_wrapper.py | pending |

## Spectral solvers and dynamics

| upstream test | status |
|---|---|
| test_ED.py | pending |
| test_ED_fermions.py | baseline |
| test_ED_spin.py | baseline |
| test_ED_spinful_fermions.py | baseline |
| test_FHM_energies.py | pending |
| test_FHM_energies_symm_adv.py | pending |
| test_Floquet.py | baseline |
| test_Floquet_t_vec.py | pending |
| test_Lanczos.py | baseline |
| test_diag_ensemble.py | pending |
| test_evolve.py | baseline |
| test_expm_multiply_parallel.py | pending |
| test_expm_multiply_parallel_batch.py | pending |
| test_gen_evolve.py | pending |
| test_mean_level_spacing.py | pending |
| test_obs_vs_time.py | pending |

## Entanglement and observables

| upstream test | status |
|---|---|
| test_basis_entropy.py | pending |
| test_basis_entropy_sparse.py | pending |
| test_boson_vs_ho.py | pending |
| test_boson_vs_spin.py | pending |
| test_diag_ensemble.py | pending (also spectral observable) |
| test_ent_basis_vs_tools.py | pending |
| test_entropy_pure.py | pending |
| test_local_entropy.py | pending |
| test_multispecies_ent.py | pending |
| test_onsite_ent.py | pending |
| test_partial_trace.py | pending |
| test_partial_trace_fermion.py | pending |
| test_partial_trace_user_basis.py | pending |
| test_photon_entropy.py | pending |
| test_spinful_fermion_entropy.py | pending |
| test_tensor_entropy.py | pending |
| test_spinful_fermion_tensor.py | pending |

## Models and cross-checks

| upstream test | status |
|---|---|
| test_Jordan_Wigner.py | pending |
| test_block_tools.py | pending |
| test_project_from_boson.py | pending (also operator projection) |
| test_project_from_fermion.py | pending (also operator projection) |
| test_project_from_spin.py | pending (also operator projection) |
| test_sq_lat_Bose_Hubbard.py | pending |
| test_sq_lat_Fermi_Hubbard_spinful.py | pending |
| test_sq_lat_Fermi_Hubbard_spinless.py | pending |
| test_sq_lat_Heis.py | pending |
| test_sq_lat_Heis_double_occupancy.py | pending |
| test_tilted_sq_lat_Heis.py | pending |

## Package/utility gate

| upstream test | status |
|---|---|
| test_reshape_pure.py | pending |
| test_version.py | pending |

There are 73 upstream test files in total. Some appear in more than one
conceptual group; the implementation must assign each file to one owner and
record any deliberate merge or exclusion before the steward can be called
complete. The duplicate rows above are intentional cross-reference markers,
not additional tests.
