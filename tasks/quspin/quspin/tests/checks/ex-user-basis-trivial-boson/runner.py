#!/usr/bin/env python3
"""Adapted from code/quspin/examples/scripts/user_basis_trivial-boson.py.

Builds a boson (sps=3) user_basis with translation+parity (T,P) symmetry
that reproduces `boson_basis_1d`, and constructs the same
hopping+interaction Hamiltonian on both. Upstream's own pass condition is
that the two agree (norm of the difference); kept as a raising assertion,
not graded, per this leaf's instructions for the user_basis_trivial-* decks.
The graded, physical content is the full sorted spectrum of H built on the
user_basis. sps=3 is hard-coded inside `get_s0_pcon`/`get_Ns_pcon` (upstream
uses the literal `3` there, not the `args`-driven value used elsewhere), so
only the site count SAB_N is a knob.
"""
from __future__ import annotations

import json
import os
import sys

import numpy as np
from numba import carray, cfunc, uint32, int32, float64
from scipy.special import comb

from quspin.basis import boson_basis_1d
from quspin.basis.user import user_basis, next_state_sig_32, op_sig_32, map_sig_32
from quspin.operators import hamiltonian

SPS = 3


@cfunc(op_sig_32, locals=dict(b=uint32, occ=int32, sps=uint32, me_offdiag=float64, me_diag=float64))
def op(op_struct_ptr, op_str, site_ind, N, args):
    op_struct = carray(op_struct_ptr, 1)[0]
    err = 0
    sps = args[0]
    me_offdiag = 1.0
    me_diag = 1.0
    site_ind = N - site_ind - 1
    occ = (op_struct.state // sps**site_ind) % sps
    b = sps**site_ind
    if op_str == 43:  # "+"
        me_offdiag *= (occ + 1) % sps
        op_struct.state += b if (occ + 1) < sps else 0
    elif op_str == 45:  # "-"
        me_offdiag *= occ
        op_struct.state -= b if occ > 0 else 0
    elif op_str == 110:  # "n"
        me_diag *= occ
    elif op_str == 73:  # "I"
        pass
    else:
        me_diag = 0.0
        err = -1
    op_struct.matrix_ele *= me_diag * np.sqrt(me_offdiag)
    return err


op_args = np.array([SPS], dtype=np.uint32)


@cfunc(next_state_sig_32, locals=dict(t=uint32, i=int32, j=int32, n=int32, sps=uint32,
                                       b1=int32, b2=int32, l=int32, n_left=int32))
def next_state(s, counter, N, args):
    t = s
    sps = args[1]
    n = 0
    for i in range(N):
        b1 = (t // args[i]) % sps
        if b1 > 0:
            n += b1
            b2 = (t / args[i + 1]) % sps
            if b2 < (sps - 1):
                n -= 1
                t -= args[i]
                t += args[i + 1]
                if n > 0:
                    l = n // (sps - 1)
                    n_left = n % (sps - 1)
                    for j in range(i + 1):
                        t -= (t // args[j]) % sps * args[j]
                        if j < l:
                            t += (sps - 1) * args[j]
                        elif j == l:
                            t += n_left * args[j]
                break
    return t


def get_s0_pcon(N, Np):
    sps = 3
    l = Np // (sps - 1)
    s = sum((sps - 1) * sps**i for i in range(l))
    s += (Np % (sps - 1)) * sps**l
    return s


def get_Ns_pcon(N, Np):
    Ns = 0
    sps = 3
    for r in range(Np // sps + 1):
        r_2 = Np - r * sps
        if r % 2 == 0:
            Ns += comb(N, r, exact=True) * comb(N + r_2 - 1, r_2, exact=True)
        else:
            Ns += -comb(N, r, exact=True) * comb(N + r_2 - 1, r_2, exact=True)
    return Ns


@cfunc(map_sig_32, locals=dict(shift=uint32, out=uint32, sps=uint32, i=int32, j=int32))
def translation(x, N, sign_ptr, args):
    out = 0
    shift = args[0]
    sps = args[1]
    for i in range(N):
        j = (i + shift + N) % N
        out += (x % sps) * sps**j
        x //= sps
    return out


@cfunc(map_sig_32, locals=dict(out=uint32, sps=uint32, i=int32, j=int32))
def parity(x, N, sign_ptr, args):
    out = 0
    sps = args[0]
    for i in range(N):
        j = (N - 1) - i
        out += (x % sps) * (sps**j)
        x //= sps
    return out


def build(N, J, U):
    Np = N // 2
    next_state_args = np.array([SPS**i for i in range(N)], dtype=np.uint32)
    T_args = np.array([1, SPS], dtype=np.uint32)
    P_args = np.array([SPS], dtype=np.uint32)

    maps = dict(T_block=(translation, N, 0, T_args), P_block=(parity, 2, 0, P_args))
    pcon_dict = dict(Np=Np, next_state=next_state, next_state_args=next_state_args,
                      get_Ns_pcon=get_Ns_pcon, get_s0_pcon=get_s0_pcon)
    op_dict = dict(op=op, op_args=op_args)
    basis = user_basis(np.uint32, N, op_dict, allowed_ops=set("+-nI"), sps=SPS,
                        pcon_dict=pcon_dict, **maps)
    basis_1d = boson_basis_1d(N, Nb=Np, sps=SPS, kblock=0, pblock=1)

    hopping = [[+J, j, (j + 1) % N] for j in range(N)]
    int_bb = [[0.5 * U, j, j] for j in range(N)]
    int_b = [[-0.5 * U, j] for j in range(N)]
    static = [["+-", hopping], ["-+", hopping], ["nn", int_bb], ["n", int_b]]
    no_checks = dict(check_symm=False, check_pcon=False, check_herm=False)
    H = hamiltonian(static, [], basis=basis, dtype=np.float64, **no_checks)
    H_1d = hamiltonian(static, [], basis=basis_1d, dtype=np.float64)

    resid = np.linalg.norm((H - H_1d).toarray())
    if resid > 1e-8:
        raise AssertionError(f"user_basis Hamiltonian disagrees with boson_basis_1d: {resid} > 1e-8")

    return np.sort(np.linalg.eigvalsh(H.toarray()))


def main() -> int:
    config_path, out_path = sys.argv[1:3]
    config = json.loads(open(config_path).read())
    N = int(os.environ.get("SAB_N", 6))
    J = float(config["J"])
    U = float(config["U"])
    spectrum = build(N, J, U)
    observable = {"spectrum": [float(x) for x in spectrum]}
    with open(out_path, "w") as fh:
        json.dump(observable, fh, sort_keys=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
