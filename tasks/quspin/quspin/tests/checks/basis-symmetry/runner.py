#!/usr/bin/env python3
"""Run the upstream QuSpin regression files owned by one grouped check.

The grouped checks preserve the upstream assertions while emitting one small,
deterministic physical observable.  This lets the steward cover the broad
official suite without copying every implementation detail into a checker.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import time
from pathlib import Path

GROUPS = {
    "basis-symmetry": [
        "test_array_ints_conversion.py", "test_basis_particle_sectors.py",
        "test_general_bitops.py", "test_general_spin_get_vec.py",
        "test_general_spin_opstr.py", "test_general_spinless_fermion_opstr.py",
        "test_general_spinless_majorana_opstr.py", "test_higher_spin.py",
        "test_pauli.py", "test_representative.py", "test_user_basis_boson.py",
        "test_user_basis_spin.py", "test_user_basis_spinless_fermion.py",
    ],
    "operators-projections": [
        "test_Op_bra_ket.py", "test_Op_shift_sector.py", "test_Op_shift_sector_corr.py",
        "test_get_amp.py", "test_ham_project_to.py", "test_inplace_op.py",
        "test_operator_shape.py", "test_project_from_boson.py",
        "test_project_from_fermion.py", "test_project_from_spin.py", "test_project_op.py",
        "test_project_to.py", "test_quantum_LinearOperator.py", "test_quantum_operator.py",
        "test_recursive_tensor.py", "test_save_zip.py", "test_sent_wrapper.py",
    ],
    "dynamics-utilities": [
        "test_ED.py", "test_FHM_energies.py", "test_FHM_energies_symm_adv.py",
        "test_Floquet_t_vec.py", "test_diag_ensemble.py", "test_expm_multiply_parallel.py",
        "test_expm_multiply_parallel_batch.py", "test_gen_evolve.py",
        "test_mean_level_spacing.py", "test_obs_vs_time.py",
    ],
    "entanglement-observables": [
        "test_basis_entropy.py", "test_basis_entropy_sparse.py", "test_ent_basis_vs_tools.py",
        "test_entropy_pure.py", "test_local_entropy.py", "test_multispecies_ent.py",
        "test_onsite_ent.py", "test_partial_trace.py", "test_partial_trace_fermion.py",
        "test_partial_trace_user_basis.py", "test_photon_entropy.py",
        "test_spinful_fermion_entropy.py", "test_spinful_fermion_tensor.py",
        "test_tensor_entropy.py",
    ],
    "models-crosschecks": [
        "test_Jordan_Wigner.py", "test_boson_vs_ho.py", "test_boson_vs_spin.py",
        "test_block_tools.py", "test_sq_lat_Bose_Hubbard.py",
        "test_sq_lat_Fermi_Hubbard_spinful.py", "test_sq_lat_Fermi_Hubbard_spinless.py",
        "test_sq_lat_Heis.py", "test_sq_lat_Heis_double_occupancy.py",
        "test_tilted_sq_lat_Heis.py", "test_reshape_pure.py", "test_version.py",
    ],
}


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit("usage: upstream_group.py <group> <source> <out> <config>")
    group, source, out, config_path = sys.argv[1:]
    files = GROUPS[group]
    config = json.loads(Path(config_path).read_text())
    test_root = Path(source) / "test"
    env = os.environ.copy()
    env.update({"PYTHONDONTWRITEBYTECODE": "1", "OMP_NUM_THREADS": "1", "OPENBLAS_NUM_THREADS": "1"})
    command = [sys.executable, "-m", "pytest", "-q", *(str(test_root / f) for f in files)]
    started = time.monotonic()
    proc = subprocess.run(command, env=env, text=True, capture_output=True)
    elapsed = time.monotonic() - started
    if proc.returncode:
        sys.stderr.write(proc.stdout + proc.stderr)
        return proc.returncode

    # A compact physical calibration observable: the finite spin-chain ground
    # energy at the configured size/field.  The variant changes L, so it cannot
    # silently reuse the nominal result while the upstream suite still runs.
    import numpy as np
    from quspin.basis import spin_basis_1d
    from quspin.operators import hamiltonian

    L = int(config.get("L", 8))
    h = float(config.get("h", 0.5))
    basis = spin_basis_1d(L, Nup=L // 2)
    bonds = [[1.0, i, i + 1] for i in range(L - 1)]
    field = [[h, i] for i in range(L)]
    H = hamiltonian([["xx", bonds], ["yy", bonds], ["zz", bonds], ["z", field]], [], basis=basis, dtype=np.float64)
    ground = float(H.eigsh(k=1, which="SA", return_eigenvectors=False)[0])
    Path(out).write_text(json.dumps({
        "group": group,
        "upstream_files": files,
        "upstream_passed": len(files),
        "ground_energy": ground,
        "L": L,
        "h": h,
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
