#!/usr/bin/env python3
"""Adapted from code/quspin/test/test_higher_spin.py.

Upstream test sweeps every spin length S in {1/2,1,3/2,2}, every chain
length L in 1..4 and every product operator string built from the site
operators {I,x,y,z}, checking that the QuSpin-assembled operator (coupling
1.0) equals the explicit Kronecker product of the dense single-site
matrices. This check fixes S=1 (the smallest genuinely "higher spin" case,
sps=3) and the multi-site string opstr="xyz" acting on all L sites with an
explicit coupling J read from config.json, and keeps the Kronecker-product
identity as an internal consistency assertion (raises on failure).

Graded quantities (observable.json):
  spectrum -- the moving eigenvalues of the operator (|eigenvalue| = |J|,
    sorted ascending); the 19 of 27 eigenvalues that are exactly zero by
    rank deficiency of the single-site factors are excluded (they never
    move under any J).
  h_im -- the imaginary part of the dense Ns x Ns (Ns = 3^L) matrix
    J * (x tensor y tensor z tensor ...), in the basis's documented
    (ascending integer state) order, flattened row-major.
"""
from __future__ import annotations

import json
import os
import sys
from functools import reduce

import numpy as np
from quspin.basis import spin_basis_1d
from quspin.operators import hamiltonian

X1 = (1.0 / np.sqrt(2)) * np.array([[0, 1, 0], [1, 0, 1], [0, 1, 0]]) + 0.0j
Y1 = (1.0j / np.sqrt(2)) * np.array([[0, -1, 0], [1, 0, -1], [0, 1, 0]]) + 0.0j
Z1 = np.array([[1, 0, 0], [0, 0, 0], [0, 0, -1]]) + 0.0j
OPS = {"x": X1, "y": Y1, "z": Z1}


def main() -> None:
    cfg = json.load(open(sys.argv[1]))
    out_path = sys.argv[2]

    L = int(os.environ.get("SAB_L", cfg.get("L", 3)))
    J = float(cfg["J"])

    basis = spin_basis_1d(L, S="1", pauli=False)

    opstr = "".join(["x", "y", "z"][i % 3] for i in range(L))
    Jc = [J] + list(range(L))
    static = [[opstr, [Jc]]]
    static_expanded, _ = basis.expanded_form(static, [])
    quspin_op = hamiltonian(static_expanded, [], basis=basis, check_symm=False, check_herm=False)

    ops = [OPS[c] for c in opstr]
    ref = J * reduce(np.kron, ops)
    if not np.allclose(quspin_op.toarray(), ref, atol=1e-12):
        raise AssertionError("QuSpin operator disagrees with explicit Kronecker-product reference")

    A = quspin_op.toarray()
    # the x*y*z product operator is purely imaginary (the single y-factor
    # carries the only factor of i); the real part is exactly zero by
    # construction and is not graded.
    if not np.allclose(np.real(A), 0.0, atol=1e-12):
        raise AssertionError("operator unexpectedly has a non-zero real part")

    if not np.allclose(A, A.conj().T, atol=1e-12):
        raise AssertionError("operator is not Hermitian")
    E = np.linalg.eigvalsh(A)
    E.sort()

    # eig(X1 kron Y1 kron Z1) is the product of the single-site eigenvalues
    # (-1,0,1) of each factor, so 19 of the 27 eigenvalues are exactly zero
    # by rank deficiency (whenever any one factor contributes its m=0
    # eigenvalue) and never move under any J -- they are excluded, keeping
    # only the |eigenvalue|=|J| entries that do move.
    moving = E[np.abs(E) > 0.5 * abs(J)]

    observable = {
        "spectrum": [float(x) for x in moving],
        "h_im": [float(x) for x in np.imag(A).flatten()],
    }
    json.dump(observable, open(out_path, "w"), sort_keys=True)


if __name__ == "__main__":
    main()
