#!/usr/bin/env python3
"""Adapted from code/quspin/test/test_sq_lat_Fermi_Hubbard_spinless.py.

An Lx-by-Ly square-lattice spinless-fermion model (nearest-neighbour
interaction V, hopping J), built with spinless_fermion_basis_general. For
one particle sector the upstream test decomposes the Hilbert space into the
square lattice's translation+parity symmetry blocks and checks the
block-decomposed spectrum reproduces the particle-conserving (pcon) basis
spectrum exactly (kept here as an internal consistency check, raised on
failure). Writes that sector's spectrum, and the ground-state energy for
every filling Nf=0..N (ascending), to observable.json.
"""
from __future__ import annotations

import _seeded_eigsh  # noqa: F401  deterministic ARPACK start vectors (see the module)
import json
import os
import sys

import numpy as np
from quspin.basis import spinless_fermion_basis_general
from quspin.basis.transformations import square_lattice_trans
from quspin.operators import hamiltonian


def main() -> None:
    cfg_path, out_path = sys.argv[1], sys.argv[2]
    cfg = json.loads(open(cfg_path).read())
    Lx = int(os.environ.get("SAB_LX", cfg.get("Lx", 2)))
    Ly = int(os.environ.get("SAB_LY", cfg.get("Ly", 2)))
    V = float(cfg.get("V", np.sqrt(2.0)))
    J = float(cfg.get("J", 1.0))

    N = Lx * Ly
    tr = square_lattice_trans(Lx, Ly)
    Vl = [[V, i, tr.T_x[i]] for i in range(N)] + [[V, i, tr.T_y[i]] for i in range(N)]
    Jp = [[J, i, tr.T_x[i]] for i in range(N)] + [[J, i, tr.T_y[i]] for i in range(N)]
    Jm = [[-J, i, tr.T_x[i]] for i in range(N)] + [[-J, i, tr.T_y[i]] for i in range(N)]
    static = [["nn", Vl], ["+-", Jp], ["-+", Jm]]

    Nf_check = N // 2
    pcon = spinless_fermion_basis_general(N, Nf=Nf_check)
    blocks_sp, Ns_block = [], 0
    for blocks in tr.allowed_blocks_iter_parity():
        b = spinless_fermion_basis_general(N, Nf=Nf_check, **blocks)
        Ns_block += b.Ns
        blocks_sp.append(b)
    assert Ns_block == pcon.Ns

    H_pcon = hamiltonian(static, [], basis=pcon, dtype=np.float64)
    E_pcon = np.linalg.eigvalsh(H_pcon.todense()) if H_pcon.Ns > 0 else np.array([])

    E_block = []
    for b in blocks_sp:
        H = hamiltonian(static, [], basis=b, dtype=np.complex128)
        if H.Ns > 0:
            E_block.append(np.linalg.eigvalsh(H.todense()))
    E_block = np.hstack(E_block)
    E_block.sort()
    # Upstream consistency: symmetry-block ED reproduces the particle-conserving basis.
    np.testing.assert_allclose(E_pcon, E_block, atol=1e-10)

    ground_by_Nf = []
    for Nf in range(N + 1):
        b = spinless_fermion_basis_general(N, Nf=Nf)
        H = hamiltonian(static, [], basis=b, dtype=np.float64)
        E = np.linalg.eigvalsh(H.todense()) if H.Ns > 0 else np.array([0.0])
        ground_by_Nf.append(float(E.min()))

    result = {
        "spectrum_sector": sorted(float(e) for e in E_pcon),
        "ground_energy_by_Nf": ground_by_Nf,
    }
    with open(out_path, "w") as f:
        json.dump(result, f, sort_keys=True)


if __name__ == "__main__":
    main()
