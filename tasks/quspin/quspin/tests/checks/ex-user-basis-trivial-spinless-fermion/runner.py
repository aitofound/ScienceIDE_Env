#!/usr/bin/env python3
"""Adapted from
code/quspin/examples/scripts/user_basis_trivial-spinless_fermion.py.

Builds a spinless-fermion user_basis with translation+parity (T,P) symmetry
and fermion signs (generic bit-count routines, not hard-coded to N=8, so
SAB_N is a knob) that reproduces `spinless_fermion_basis_1d`, and constructs
the same hopping+interaction Hamiltonian on both. Upstream's own pass
condition is that the two agree (norm of the difference); kept as a raising
assertion, not graded, per this leaf's instructions for the
user_basis_trivial-* decks. The graded, physical content is the full sorted
spectrum of H built on the user_basis.
"""
from __future__ import annotations

import json
import os
import sys

import numpy as np
from numba import carray, cfunc, jit, uint32, int32
from scipy.special import comb

from quspin.basis import spinless_fermion_basis_1d
from quspin.basis.user import user_basis, next_state_sig_32, op_sig_32, map_sig_32, count_particles_sig_32
from quspin.operators import hamiltonian


@jit(uint32(uint32, uint32), locals=dict(f_count=uint32), nopython=True, nogil=True)
def _count_particles_32(state, site_ind):
    f_count = state & ((0x7FFFFFFF) >> (31 - site_ind))
    f_count = f_count - ((f_count >> 1) & 0x55555555)
    f_count = (f_count & 0x33333333) + ((f_count >> 2) & 0x33333333)
    return (((f_count + (f_count >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24


@cfunc(op_sig_32, locals=dict(s=int32, sign=int32, n=int32, b=uint32, f_count=uint32))
def op(op_struct_ptr, op_str, site_ind, N, args):
    op_struct = carray(op_struct_ptr, 1)[0]
    err = 0
    site_ind = N - site_ind - 1
    f_count = _count_particles_32(op_struct.state, site_ind)
    sign = -1 if f_count & 1 else 1
    n = (op_struct.state >> site_ind) & 1
    b = 1 << site_ind
    if op_str == 43:  # "+"
        op_struct.matrix_ele *= 0.0 if n else sign
        op_struct.state ^= b
    elif op_str == 45:  # "-"
        op_struct.matrix_ele *= sign if n else 0.0
        op_struct.state ^= b
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


def get_Ns_pcon(N, Np):
    return comb(N, Np, exact=True)


@cfunc(map_sig_32, locals=dict(shift=uint32, xmax=uint32, x1=uint32, x2=uint32, period=int32,
                                l=int32, f_count1=int32, f_count2=int32))
def translation(x, N, sign_ptr, args):
    shift = args[0]
    period = N
    xmax = args[1]
    l = (shift + period) % period
    x1 = x >> (period - l)
    x2 = (x << l) & xmax
    f_count1 = _count_particles_32(x1, period)
    f_count2 = _count_particles_32(x2, period)
    sign_ptr[0] *= -1 if ((f_count1 & 1) & (f_count2 & 1) & 1) else 1
    return x2 | x1


@cfunc(map_sig_32, locals=dict(out=uint32, s=uint32, f_count=int32))
def parity(x, N, sign_ptr, args):
    out = 0
    s = args[0]
    f_count = _count_particles_32(x, N)
    sign_ptr[0] *= -1 if ((f_count & 2) and 1) else 1
    out ^= x & 1
    x >>= 1
    while x:
        out <<= 1
        out ^= x & 1
        x >>= 1
        s -= 1
    out <<= s
    return out


@cfunc(count_particles_sig_32, locals=dict(f_count=uint32))
def count_particles(x, p_number_ptr, args):
    p_number_ptr[0] = _count_particles_32(x, args[0])


def build(N, J, U):
    Np = N // 2
    T_args = np.array([1, (1 << N) - 1], dtype=np.uint32)
    P_args = np.array([N - 1], dtype=np.uint32)
    count_particles_args = np.array([N], dtype=np.int32)
    noncommuting_bits = [(np.arange(N), -1)]

    maps = dict(T_block=(translation, N, 0, T_args), P_block=(parity, 2, 0, P_args))
    pcon_dict = dict(Np=Np, next_state=next_state, next_state_args=next_state_args,
                      get_Ns_pcon=get_Ns_pcon, get_s0_pcon=get_s0_pcon,
                      count_particles=count_particles, count_particles_args=count_particles_args,
                      n_sectors=1)
    op_dict = dict(op=op, op_args=op_args)
    basis = user_basis(np.uint32, N, op_dict, allowed_ops=set("+-nI"), sps=2,
                        pcon_dict=pcon_dict, noncommuting_bits=noncommuting_bits, **maps)
    basis_1d = spinless_fermion_basis_1d(N, Nf=Np, kblock=0, pblock=1)

    hopping_pm = [[+J, j, (j + 1) % N] for j in range(N)]
    hopping_mp = [[-J, j, (j + 1) % N] for j in range(N)]
    nn_int = [[U, j, (j + 1) % N] for j in range(N)]
    static = [["+-", hopping_pm], ["-+", hopping_mp], ["nn", nn_int]]
    no_checks = dict(check_symm=False, check_pcon=False, check_herm=False)
    H = hamiltonian(static, [], basis=basis, dtype=np.float64, **no_checks)
    H_1d = hamiltonian(static, [], basis=basis_1d, dtype=np.float64)

    resid = np.linalg.norm((H - H_1d).toarray())
    if resid > 1e-8:
        raise AssertionError(f"user_basis Hamiltonian disagrees with spinless_fermion_basis_1d: {resid} > 1e-8")

    return np.sort(np.linalg.eigvalsh(H.toarray()))


def main() -> int:
    config_path, out_path = sys.argv[1:3]
    config = json.loads(open(config_path).read())
    N = int(os.environ.get("SAB_N", 8))
    J = float(config["J"])
    U = float(config["U"])
    spectrum = build(N, J, U)
    observable = {"spectrum": [float(x) for x in spectrum]}
    with open(out_path, "w") as fh:
        json.dump(observable, fh, sort_keys=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
