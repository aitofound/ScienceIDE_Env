"""Adapted from code/quspin/test/test_FHM_energies_symm_adv.py: the same
square-lattice Fermi-Hubbard ground energy vs. U as fhm-energies, but built
through spinful_fermion_basis_general with the kx=ky=0 translation sectors
and simple_symm=False (the "advanced" symmetry path this file exercises).

Deterministic model, no random draws. Sizes/points from SAB_LX/SAB_LY/SAB_NU
(config defaults); the active hopping J from config.json.
"""
from __future__ import annotations

import _seeded_eigsh  # noqa: F401  deterministic ARPACK start vectors (see the module)
import json
import os
import sys

import numpy as np
from quspin.basis import spinful_fermion_basis_general
from quspin.basis.transformations import square_lattice_trans
from quspin.operators import quantum_operator


def main() -> None:
    cfg = json.load(open(sys.argv[1]))
    out_path = sys.argv[2]

    Lx = int(os.environ.get("SAB_LX", cfg.get("Lx", 4)))
    Ly = int(os.environ.get("SAB_LY", cfg.get("Ly", 4)))
    n_U = int(os.environ.get("SAB_NU", cfg.get("n_U", 9)))
    J = float(cfg.get("J", 1.0))
    N = Lx * Ly

    tr = square_lattice_trans(Lx, Ly)
    T_x = np.hstack((tr.T_x, tr.T_x + N))
    T_y = np.hstack((tr.T_y, tr.T_y + N))

    Jp = [[J, i, T_x[i]] for i in range(2 * N)]
    Jp.extend([[J, i, T_y[i]] for i in range(2 * N)])
    Jm = [[-J, i, T_x[i]] for i in range(2 * N)]
    Jm.extend([[-J, i, T_y[i]] for i in range(2 * N)])
    U_onsite = [[1.0, i, i + N] for i in range(N)]

    operator_list_0 = [["+-", Jp], ["-+", Jm]]
    operator_list_1 = [["nn", U_onsite]]
    operator_dict = dict(H0=operator_list_0, H1=operator_list_1)

    basis = spinful_fermion_basis_general(
        N, Nf=(2, 2), ky=(T_y, 0), kx=(T_x, 0), simple_symm=False
    )
    H_U = quantum_operator(
        operator_dict,
        basis=basis,
        dtype=np.float64,
        check_pcon=False,
        check_symm=False,
        check_herm=False,
    )

    Us = np.linspace(0.0, 4.0, n_U)
    ground_energy = np.zeros(n_U)
    for j, U in enumerate(Us):
        H = H_U.tohamiltonian(dict(H0=1.0, H1=float(U)))
        ground_energy[j] = H.eigsh(k=1, which="SA", maxiter=int(1e4), return_eigenvectors=False)[0]

    obs = {"ground_energy": ground_energy.tolist()}
    with open(out_path, "w") as fh:
        json.dump(obs, fh, sort_keys=True)


if __name__ == "__main__":
    main()
