#!/usr/bin/env python3
"""Adapted from code/quspin/examples/scripts/example23.py.

Builds a boson user_basis (sps=3, "qutrit" per site) implementing the 8
SU(3) Gell-Mann generators as custom operator strings "1".."8", plus the
usual ladder/number operators, and constructs
    H = J*sum_j (lambda1_j lambda1_{j+1} + lambda2_j lambda2_{j+1})
        + U*sum_j lambda8_j^2 + h*sum_j lambda5_j
on a ring of SAB_N sites. Plots/prints of the basis and dense matrix are
dropped; the physical content kept is the sorted lowest-lying eigenvalues of
H (full spectrum since the Hilbert space is small).
"""
from __future__ import annotations

import _seeded_eigsh  # noqa: F401  deterministic ARPACK start vectors (see the module)
import json
import os
import sys

import numpy as np
from numba import carray, cfunc, uint32, int32, float64, complex128

from quspin.basis.user import user_basis, op_sig_32
from quspin.operators import hamiltonian


@cfunc(op_sig_32, locals=dict(b=uint32, occ=int32, sps=uint32, me_offdiag=complex128, me_diag=float64))
def op(op_struct_ptr, op_str, site_ind, N, args):
    op_struct = carray(op_struct_ptr, 1)[0]
    err = 0
    sps = 3
    me_offdiag = 1.0
    me_diag = 1.0
    site_ind = N - site_ind - 1
    occ = (op_struct.state // sps**site_ind) % sps
    b = sps**site_ind
    if op_str == 43:  # "+"
        if (occ + 1) % sps != 1:
            me_offdiag *= np.sqrt((occ + 1) % sps)
        else:
            me_offdiag *= np.sqrt(2.0)
        op_struct.state += b if (occ + 1) < sps else 0
    elif op_str == 45:  # "-"
        if occ != 1:
            me_offdiag *= np.sqrt(occ)
        else:
            me_offdiag *= np.sqrt(2.0)
        op_struct.state -= b if occ > 0 else 0
    elif op_str == 122:  # "z"
        me_diag *= occ - 1
    elif op_str == 49:  # "1"
        if occ == 2:
            op_struct.state -= b
        elif occ == 1:
            op_struct.state += b
        else:
            me_offdiag *= 0.0
    elif op_str == 50:  # "2"
        if occ == 2:
            op_struct.state -= b
            me_offdiag *= 1.0j
        elif occ == 1:
            op_struct.state += b
            me_offdiag *= -1.0j
        else:
            me_offdiag *= 0.0
    elif op_str == 51:  # "3"
        if occ == 1:
            me_diag *= -1.0
        elif occ == 0:
            me_diag *= 0.0
    elif op_str == 52:  # "4"
        if occ == 2:
            op_struct.state -= 2 * b
        elif occ == 0:
            op_struct.state += 2 * b
        else:
            me_offdiag *= 0.0
    elif op_str == 53:  # "5"
        if occ == 2:
            op_struct.state -= 2 * b
            me_offdiag *= 1.0j
        elif occ == 0:
            op_struct.state += 2 * b
            me_offdiag *= -1.0j
        else:
            me_offdiag *= 0.0
    elif op_str == 54:  # "6"
        if occ == 1:
            op_struct.state -= b
        elif occ == 0:
            op_struct.state += b
        else:
            me_offdiag *= 0.0
    elif op_str == 55:  # "7"
        if occ == 1:
            op_struct.state -= b
            me_offdiag *= 1.0j
        elif occ == 0:
            op_struct.state += b
            me_offdiag *= -1.0j
        else:
            me_offdiag *= 0.0
    elif op_str == 56:  # "8"
        if occ == 2:
            me_diag *= 1.0 / np.sqrt(3.0)
        elif occ == 1:
            me_diag *= 1.0 / np.sqrt(3.0)
        else:
            me_diag *= -2.0 / np.sqrt(3.0)
    elif op_str == 73:  # "I"
        pass
    else:
        me_diag = 0.0
        err = -1
    op_struct.matrix_ele *= me_diag * me_offdiag
    return err


op_args = np.array([3], dtype=np.uint32)


def main() -> int:
    config_path, out_path = sys.argv[1:3]
    config = json.loads(open(config_path).read())
    N = int(os.environ.get("SAB_N", 2))
    J = float(config["J"])
    U = float(config["U"])
    h = float(config["h"])

    op_dict = dict(op=op, op_args=op_args)
    basis = user_basis(np.uint32, N, op_dict, allowed_ops=set("+-zI12345678"), sps=3)

    nn_int = [[+J, j, (j + 1) % N] for j in range(N)]
    onsite_int = [[U, j, j] for j in range(N)]
    onsite_field = [[h, j] for j in range(N)]
    static = [["11", nn_int], ["22", nn_int], ["88", onsite_int], ["5", onsite_field]]
    no_checks = dict(check_symm=False, check_pcon=False, check_herm=False)
    H = hamiltonian(static, [], basis=basis, dtype=np.complex128, **no_checks)
    E = H.eigvalsh()

    observable = {"spectrum": [float(x) for x in np.sort(np.real(E))]}
    with open(out_path, "w") as fh:
        json.dump(observable, fh, sort_keys=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
