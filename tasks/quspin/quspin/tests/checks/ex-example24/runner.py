#!/usr/bin/env python3
"""Adapted from code/quspin/examples/scripts/example24.py.

Builds a spinless-fermion user_basis (with translation+parity symmetry, and
fermion signs via `noncommuting_bits`) and two equivalent Hamiltonians on it:
one built from Majorana operators ("xy"/"yx"/"I"/"xyxy" strings) and one from
ordinary complex fermion operators ("+-"/"-+"/"nn"). Upstream's own pass
condition is that the two agree (norm of the difference); that residual is
not graded (it is a round-trip check, not a physical quantity, and is exactly
zero only in exact arithmetic), but is kept as a raising assertion. The
graded, physical content is the sorted spectrum of the fermion-operator
Hamiltonian H, computed via dense `eigvalsh` since the Hilbert space is
small. The symmetry maps (`translation`/`parity`) use a generic fermion
bit-counting routine, not a hard-coded mask, so the system size SAB_N is a
knob (upstream's default N=6).
"""
from __future__ import annotations

import _seeded_eigsh  # noqa: F401  deterministic ARPACK start vectors (see the module)
import json
import os
import sys

import numpy as np
from numba import carray, cfunc, jit, uint32, int32

from quspin.basis.user import user_basis, op_sig_32, map_sig_32
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
    if op_str == 120:  # "x"
        op_struct.state ^= b
        op_struct.matrix_ele *= sign
    elif op_str == 121:  # "y"
        op_struct.state ^= b
        op_struct.matrix_ele *= -1.0j * sign * ((n << 1) - 1)
    elif op_str == 43:  # "+"
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


def make_maps(N):
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

    T_args = np.array([1, (1 << N) - 1], dtype=np.uint32)

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

    P_args = np.array([N - 1], dtype=np.uint32)
    return translation, T_args, parity, P_args


def build(N, J, U, k=None):
    translation, T_args, parity, P_args = make_maps(N)
    noncommuting_bits = [(np.arange(N), -1)]
    maps = dict(T_block=(translation, N, 0, T_args), P_block=(parity, 2, 0, P_args))
    op_dict = dict(op=op, op_args=op_args)
    basis = user_basis(np.uint32, N, op_dict, allowed_ops=set("xy+-nI"), sps=2,
                        noncommuting_bits=noncommuting_bits, **maps)

    hop_term_p = [[+0.5j * J, j, (j + 1) % N] for j in range(N)]
    hop_term_m = [[-0.5j * J, j, (j + 1) % N] for j in range(N)]
    density_term = [[+0.5j * U, j, j] for j in range(N)]
    int_term = [[-0.25 * U, j, j, (j + 1) % N, (j + 1) % N] for j in range(N)]
    id_term = [[0.25 * U, j] for j in range(N)]
    static_majorana = [["xy", hop_term_p], ["yx", hop_term_m], ["I", id_term],
                        ["xy", density_term], ["xyxy", int_term]]
    no_checks = dict(check_symm=False, check_pcon=False, check_herm=False)
    H_majorana = hamiltonian(static_majorana, [], basis=basis, dtype=np.float64, **no_checks)

    hopping_pm = [[+J, j, (j + 1) % N] for j in range(N)]
    hopping_mp = [[-J, j, (j + 1) % N] for j in range(N)]
    nn_int = [[U, j, (j + 1) % N] for j in range(N)]
    static = [["+-", hopping_pm], ["-+", hopping_mp], ["nn", nn_int]]
    H = hamiltonian(static, [], basis=basis, dtype=np.float64, **no_checks)

    # upstream's own consistency check: the two representations must agree.
    # a round-trip residual, not a physical quantity, so not graded -- but a
    # failure here means the deck's physics is broken, so it raises.
    resid = np.linalg.norm((H - H_majorana).toarray())
    if resid > 1e-8:
        raise AssertionError(f"Majorana and complex-fermion Hamiltonians disagree: {resid} > 1e-8")

    E = np.linalg.eigvalsh(H.toarray())
    return np.sort(E)


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
