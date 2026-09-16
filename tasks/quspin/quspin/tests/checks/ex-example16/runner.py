#!/usr/bin/env python3
"""Adapted from code/quspin/examples/scripts/example16.py.

Upstream builds a user-imported basis (a plain array of integer basis states
for a two-legged ladder) and applies QuSpin's translation (T, period
SAB_NHALF) and parity/reflection (P) symmetries to it via `user_basis`, then
only prints the resulting basis object -- it computes no numeric observable.
The physical content of the example is that the reduced basis is a valid
Hilbert space on which QuSpin operators can be built, so this check builds a
Heisenberg-ladder Hamiltonian on top of it (intra-leg XX+YY hopping J, rung
ZZ coupling U_rung) and grades its sorted low spectrum. The symmetry bit-masks
in `translation`/`parity` are hard-coded for N_half=10 (see upstream's own
"works only for N=10" guard), so this check keeps that fixed size; there is no
runtime knob for it.
"""
from __future__ import annotations

import json
import os
import sys

import numpy as np
from numba import carray, cfunc, uint32, int32
from scipy.special import comb

from quspin.basis import spin_basis_general
from quspin.basis.user import user_basis, next_state_sig_32, op_sig_32, map_sig_32
from quspin.operators import hamiltonian

N_HALF = 10
N = 2 * N_HALF


def make_external_basis():
    old_basis = spin_basis_general(N_HALF, m=0)
    states = old_basis.states
    shift_states = np.left_shift(states, N_HALF)
    shape = states.shape + states.shape
    states_b = np.broadcast_to(states, shape)
    shift_states_b = np.broadcast_to(shift_states, shape)
    return (states_b + shift_states_b.T).ravel()


@cfunc(op_sig_32, locals=dict(s=int32, b=uint32))
def op(op_struct_ptr, op_str, ind, N, args):
    op_struct = carray(op_struct_ptr, 1)[0]
    err = 0
    ind = N - ind - 1
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


op_args = np.array([], dtype=np.uint32)


@cfunc(next_state_sig_32)
def next_state(s, counter, N, args):
    return args[counter + 1]


@cfunc(map_sig_32)
def translation(x, N, sign_ptr, args):
    return ((x & 0x0007FDFF) << 1) | ((x & 0x00080200) >> 9)


T_args = np.array([], dtype=np.uint32)


@cfunc(map_sig_32)
def parity(x, N, sign_ptr, args):
    return (
        ((x & 0x00004010) << 1)
        | ((x & 0x00002008) << 3)
        | ((x & 0x00001004) << 5)
        | ((x & 0x00000802) << 7)
        | ((x & 0x00000401) << 9)
        | ((x & 0x00080200) >> 9)
        | ((x & 0x00040100) >> 7)
        | ((x & 0x00020080) >> 5)
        | ((x & 0x00010040) >> 3)
        | ((x & 0x00008020) >> 1)
    )


P_args = np.array([], dtype=np.uint32)


class function_wrapper:
    def __init__(self, basis):
        self.basis = basis

    def get_s0_pcon(self, N, Np):
        return self.basis[0]

    def get_Ns_pcon(self, N, Np):
        return self.basis.size


def build(J: float, U_rung: float, k: int):
    external_basis = make_external_basis()
    FW = function_wrapper(external_basis)
    pcon_dict = dict(Np=(), next_state=next_state, next_state_args=external_basis,
                      get_Ns_pcon=FW.get_Ns_pcon, get_s0_pcon=FW.get_s0_pcon)
    maps = dict(T_block=(translation, N_HALF, 0, T_args), P_block=(parity, 2, 0, P_args))
    op_dict = dict(op=op, op_args=op_args)
    basis = user_basis(np.uint32, N, op_dict, allowed_ops=set("xyz"), sps=2,
                        pcon_dict=pcon_dict, **maps)

    leg1 = [[J, i, (i + 1) % N_HALF] for i in range(N_HALF)]
    leg2 = [[J, N_HALF + i, N_HALF + (i + 1) % N_HALF] for i in range(N_HALF)]
    rung = [[U_rung, i, N_HALF + i] for i in range(N_HALF)]
    static = [["xx", leg1], ["yy", leg1], ["xx", leg2], ["yy", leg2], ["zz", rung]]
    no_checks = dict(check_symm=False, check_pcon=False, check_herm=False)
    H = hamiltonian(static, [], basis=basis, dtype=np.complex128, **no_checks)
    E = H.eigsh(k=min(k, basis.Ns - 1), which="SA", return_eigenvectors=False)
    return np.sort(np.real(E))


def main() -> int:
    config_path, out_path = sys.argv[1:3]
    config = json.loads(open(config_path).read())
    k = int(os.environ.get("SAB_K", 4))
    J = float(config["J"])
    U_rung = float(config["U_rung"])
    spectrum = build(J, U_rung, k)
    observable = {"spectrum": [float(x) for x in spectrum]}
    with open(out_path, "w") as fh:
        json.dump(observable, fh, sort_keys=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
