#!/usr/bin/env python3
"""Adapted from code/quspin/test/test_general_spin_get_vec.py.

Upstream test builds a symmetry-reduced spin_basis_general and the matching
spin_basis_1d for the same (translation, parity) block, and checks that
get_vec (dense and sparse, single and batched) maps a vector from the
symmetry-reduced basis into the same full 2^L representation regardless of
which basis object performed the mapping. Here the vector mapped is the
ground state of a physical spin-1/2 XX(Z)+field Hamiltonian (production
path), built independently through both basis objects, rather than a random
test vector: this keeps the check's graded output tied to a physical input
(the bond coupling J) instead of an arbitrary numpy draw.

Graded quantities (observable.json):
  spectrum -- two lowest eigenvalues of H, sorted ascending.
  amp      -- |amplitude| of the ground state mapped by basis_general.get_vec
              into the full (non-symmetry-reduced) 2^L Hilbert space, read at
              the L-choose-Nup positions with exactly Nup up spins (every
              other position is exactly zero by particle-number conservation
              and is not graded), in ascending integer-index order.

Internal consistency (kept from upstream, raises on failure): the dense
get_vec image from spin_basis_1d and spin_basis_general agree, and the two
Hamiltonians (built through basis_1d and basis_general) agree.
"""
from __future__ import annotations

import _seeded_eigsh  # noqa: F401  deterministic ARPACK start vectors (see the module)
import json
import os
import sys

import numpy as np
from quspin.basis import spin_basis_1d, spin_basis_general
from quspin.operators import hamiltonian


def main() -> None:
    cfg = json.load(open(sys.argv[1]))
    out_path = sys.argv[2]

    L = int(os.environ.get("SAB_L", cfg.get("L", 8)))
    J = float(cfg["J"])
    h = float(cfg["h"])
    Nup = L // 2

    t = np.array([(i + 1) % L for i in range(L)])
    p = np.array([L - i - 1 for i in range(L)])

    basis_1d = spin_basis_1d(L, Nup=Nup, pauli=False, kblock=0, pblock=1)
    gen_basis = spin_basis_general(L, Nup=Nup, pauli=False, kblock=(t, 0), pblock=(p, 0))
    if basis_1d.Ns != gen_basis.Ns:
        raise AssertionError("basis size mismatch between spin_basis_1d and spin_basis_general")

    Jlist = [[J, i, (i + 1) % L] for i in range(L)]
    hlist = [[h, i] for i in range(L)]
    static = [["xx", Jlist], ["yy", Jlist], ["zz", Jlist], ["z", hlist]]

    H1 = hamiltonian(static, [], basis=basis_1d, dtype=np.float64, check_symm=False)
    H2 = hamiltonian(static, [], basis=gen_basis, dtype=np.float64, check_symm=False)
    if not np.allclose(H1.toarray(), H2.toarray(), atol=1e-12):
        raise AssertionError("Hamiltonian built through spin_basis_1d and spin_basis_general disagree")

    E1, V1 = H1.eigsh(k=2, which="SA")
    order = np.argsort(E1)
    E1 = E1[order]
    v0 = V1[:, order[0]]

    v1 = basis_1d.get_vec(v0, sparse=False)
    v2 = gen_basis.get_vec(v0, sparse=False)
    if v1.shape != v2.shape or not np.allclose(np.abs(v1), np.abs(v2), atol=1e-12):
        raise AssertionError("get_vec images from spin_basis_1d and spin_basis_general disagree")

    # positions with exactly Nup up-spins (bit population count), independent of any numerics
    idx = np.arange(1 << L, dtype=np.int64)
    popcount = np.zeros_like(idx)
    tmp = idx.copy()
    while np.any(tmp):
        popcount += (tmp & 1)
        tmp >>= 1
    mask = popcount == Nup
    amp = np.abs(v2)[mask]

    observable = {
        "spectrum": [float(x) for x in E1],
        "amp": [float(x) for x in amp],
    }
    json.dump(observable, open(out_path, "w"), sort_keys=True)


if __name__ == "__main__":
    main()
