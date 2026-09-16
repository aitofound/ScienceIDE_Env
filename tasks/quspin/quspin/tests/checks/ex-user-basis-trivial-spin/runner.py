#!/usr/bin/env python3
"""Adapted from code/quspin/examples/scripts/user_basis_trivial-spin.py.

Builds a spin-1/2 user_basis with translation+parity+spin-inversion (T,P,Z)
symmetry (generic bit-count/shift routines, not hard-coded to N=6, so
SAB_N is a knob) that reproduces `spin_basis_1d`, and constructs the same
Heisenberg Hamiltonian on both. Upstream's own pass condition is that the
two bases give the same H (norm of the difference); that residual is a
round-trip check, not a physical quantity, so it is kept as a raising
assertion rather than graded, per this leaf's instructions for the
user_basis_trivial-* decks. The graded, physical content is the full sorted
spectrum of H built on the user_basis.
"""
from __future__ import annotations

import json
import os
import sys

import numpy as np
from numba import carray, cfunc, uint32, int32

from quspin.basis import spin_basis_1d
from quspin.basis.user import user_basis, next_state_sig_32, op_sig_32, map_sig_32, count_particles_sig_32
from quspin.operators import hamiltonian


@cfunc(op_sig_32, locals=dict(s=int32, n=int32, b=uint32))
def op(op_struct_ptr, op_str, site_ind, N, args):
    op_struct = carray(op_struct_ptr, 1)[0]
    err = 0
    site_ind = N - site_ind - 1
    n = (op_struct.state >> site_ind) & 1
    s = (((op_struct.state >> site_ind) & 1) << 1) - 1
    b = 1 << site_ind
    if op_str == 120:  # "x"
        op_struct.state ^= b
    elif op_str == 121:  # "y"
        op_struct.state ^= b
        op_struct.matrix_ele *= 1.0j * s
    elif op_str == 43:  # "+"
        if n:
            op_struct.matrix_ele = 0
        else:
            op_struct.state ^= b
    elif op_str == 45:  # "-"
        if n:
            op_struct.state ^= b
        else:
            op_struct.matrix_ele = 0
    elif op_str == 122:  # "z"
        op_struct.matrix_ele *= s
    elif op_str == 110:  # "n"
        op_struct.matrix_ele *= n
    elif op_str == 73:  # "I"
        pass
    else:
        op_struct.matrix_ele = 0
        err = -1
    return err


op_args = np.array([], dtype=np.uint32)


@cfunc(next_state_sig_32, locals=dict(t=uint32))
def next_state(s, counter, N, args):
    if s == 0:
        return s
    t = (s | (s - 1)) + 1
    return t | ((((t & (0 - t)) // (s & (0 - s))) >> 1) - 1)


next_state_args = np.array([], dtype=np.uint32)


def get_s0_pcon(N, Np):
    return sum(1 << i for i in range(Np))


from scipy.special import comb


def get_Ns_pcon(N, Np):
    return comb(N, Np, exact=True)


@cfunc(map_sig_32, locals=dict(shift=uint32, xmax=uint32, x1=uint32, x2=uint32, period=int32, l=int32))
def translation(x, N, sign_ptr, args):
    shift = args[0]
    period = N
    xmax = args[1]
    l = (shift + period) % period
    x1 = x >> (period - l)
    x2 = (x << l) & xmax
    return x2 | x1


@cfunc(map_sig_32, locals=dict(out=uint32, s=int32))
def parity(x, N, sign_ptr, args):
    out = 0
    s = args[0]
    out ^= x & 1
    x >>= 1
    while x:
        out <<= 1
        out ^= x & 1
        x >>= 1
        s -= 1
    out <<= s
    return out


@cfunc(map_sig_32, locals=dict(xmax=uint32))
def spin_inversion(x, N, sign_ptr, args):
    xmax = args[0]
    return x ^ xmax


@cfunc(count_particles_sig_32, locals=dict(s_count=uint32))
def count_particles(x, p_number_ptr, args):
    s_count = x & ((0x7FFFFFFF) >> (31 - args[0]))
    s_count = s_count - ((s_count >> 1) & 0x55555555)
    s_count = (s_count & 0x33333333) + ((s_count >> 2) & 0x33333333)
    s_count = (((s_count + (s_count >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24
    p_number_ptr[0] = s_count


def build(N, J):
    Np = N // 2
    T_args = np.array([1, (1 << N) - 1], dtype=np.uint32)
    P_args = np.array([N - 1], dtype=np.uint32)
    Z_args = np.array([(1 << N) - 1], dtype=np.uint32)
    count_particles_args = np.array([N], dtype=np.int32)

    maps = dict(T_block=(translation, N, 0, T_args), P_block=(parity, 2, 0, P_args),
                Z_block=(spin_inversion, 2, 0, Z_args))
    pcon_dict = dict(Np=Np, next_state=next_state, next_state_args=next_state_args,
                      get_Ns_pcon=get_Ns_pcon, get_s0_pcon=get_s0_pcon,
                      count_particles=count_particles, count_particles_args=count_particles_args,
                      n_sectors=1)
    op_dict = dict(op=op, op_args=op_args)
    basis = user_basis(np.uint32, N, op_dict, allowed_ops=set("+-xyznI"), sps=2,
                        pcon_dict=pcon_dict, **maps)
    basis_1d = spin_basis_1d(N, Nup=Np, pauli=True, kblock=0, pblock=1, zblock=1)

    spin_spin = [[J, j, (j + 1) % N] for j in range(N)]
    static = [["xx", spin_spin], ["yy", spin_spin], ["zz", spin_spin]]
    no_checks = dict(check_symm=False, check_pcon=False, check_herm=False)
    H = hamiltonian(static, [], basis=basis, dtype=np.float64, **no_checks)
    H_1d = hamiltonian(static, [], basis=basis_1d, dtype=np.float64)

    # upstream's own consistency check: the two bases must give the same H.
    # a round-trip residual, not a physical quantity, so not graded -- but a
    # failure here means the deck's physics is broken, so it raises.
    resid = np.linalg.norm((H - H_1d).toarray())
    if resid > 1e-8:
        raise AssertionError(f"user_basis Hamiltonian disagrees with spin_basis_1d: {resid} > 1e-8")

    return np.sort(np.linalg.eigvalsh(H.toarray()))


def main() -> int:
    config_path, out_path = sys.argv[1:3]
    config = json.loads(open(config_path).read())
    N = int(os.environ.get("SAB_N", 6))
    J = float(config["J"])
    spectrum = build(N, J)
    observable = {"spectrum": [float(x) for x in spectrum]}
    with open(out_path, "w") as fh:
        json.dump(observable, fh, sort_keys=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
