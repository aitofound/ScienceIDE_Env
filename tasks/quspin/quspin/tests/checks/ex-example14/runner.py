#!/usr/bin/env python3
"""runner.py for ex-example14: adapted from examples/scripts/example14.py.

Builds the PXP-constrained user_basis Hamiltonian H = sum_j P_{j-1} sigma^x_j
P_{j+1} exactly as the upstream deck does (numba pre_check_state forbidding
two adjacent up-spins, the same op() callback), with the field strength h
exposed as a tunable coupling (upstream hardcodes h_list=[[1.0,i],...]; this
check reads it from config.json/SAB_ so the leaf's coupling-variant
convention applies). Upstream builds H and never diagonalises or computes
any numeric observable (it only prints the basis object) -- consistent with
how the other workers on this leaf treated the same situation (example16.py,
FHM.py), this check adds one minimal calibration observable on the same
production path: the full sorted spectrum of H (the constrained basis is
small -- Lucas-number growth with N -- so a dense eigvalsh is cheap).
"""
from __future__ import annotations

import json
import os
import sys
from pathlib import Path

import numpy as np
from numba import carray, cfunc
from numba import uint32, int32
from quspin.basis.user import user_basis
from quspin.basis.user import pre_check_state_sig_32, op_sig_32
from quspin.operators import hamiltonian


@cfunc(op_sig_32, locals=dict(s=int32, b=uint32))
def op(op_struct_ptr, op_str, ind, N, args):
    op_struct = carray(op_struct_ptr, 1)[0]
    err = 0
    ind = N - ind - 1  # QuSpin convention for mapping bits to sites
    s = (((op_struct.state >> ind) & 1) << 1) - 1
    b = 1 << ind
    if op_str == 120:  # "x"
        op_struct.state ^= b
    elif op_str == 121:  # "y"
        op_struct.state ^= b
        op_struct.matrix_ele *= 1.0j * s
    elif op_str == 122:  # "z"
        op_struct.matrix_ele *= s
    else:
        op_struct.matrix_ele = 0
        err = -1
    return err


@cfunc(pre_check_state_sig_32, locals=dict(s_shift_left=uint32, s_shift_right=uint32))
def pre_check_state(s, N, args):
    """A spin-up on a given site must have empty neighbouring sites (up to N=32)."""
    mask = 0xFFFFFFFF >> (32 - N)
    s_shift_left = ((s << 1) & mask) | ((s >> (N - 1)) & mask)
    s_shift_right = ((s >> 1) & mask) | ((s << (N - 1)) & mask)
    return (((s_shift_right | s_shift_left) & s)) == 0


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: runner.py <config.json> <observable.json>")
    cfg = json.loads(Path(sys.argv[1]).read_text())
    N = int(os.environ.get("SAB_N", cfg.get("N", 10)))
    h = float(cfg.get("h", 1.0))

    op_args = np.array([], dtype=np.uint32)
    op_dict = dict(op=op, op_args=op_args)
    pre_check_state_t = (pre_check_state, None)
    basis = user_basis(np.uint32, N, op_dict, allowed_ops=set("xyz"), sps=2,
                        pre_check_state=pre_check_state_t, Ns_block_est=300000)

    h_list = [[h, i] for i in range(N)]
    static = [["x", h_list]]
    no_checks = dict(check_symm=False, check_pcon=False, check_herm=False)
    H = hamiltonian(static, [], basis=basis, dtype=np.float64, **no_checks)

    E = np.sort(H.eigvalsh())
    obs = {"spectrum": [float(x) for x in E]}
    Path(sys.argv[2]).write_text(json.dumps(obs, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
