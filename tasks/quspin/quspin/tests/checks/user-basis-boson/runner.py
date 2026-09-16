#!/usr/bin/env python3
"""Adapted from code/quspin/test/test_user_basis_boson.py.

Upstream test builds an sps=3 boson-chain basis two ways: a hand-written
`user_basis` (numba cfuncs for the operator action, a translation/parity
symmetry map and particle-number conservation with the boson "carry" logic
in next_state) and the production `boson_basis_1d` with the equivalent
symmetry sector, and checks the two bases and the two Hamiltonians they
support agree. Kept verbatim: the cfuncs, symmetry maps and particle-number
bookkeeping. Grades the hopping+interaction Hamiltonian the upstream file
assembles, with hopping J and on-site interaction U read from config.json.

Graded quantities (observable.json):
  spectrum -- the full sorted spectrum of H (built through user_basis).
  h_re -- the dense Ns x Ns Hamiltonian matrix (real, dtype float64), in the
    basis's documented (ascending integer state) order, flattened row-major.

Internal consistency (kept from upstream, raises on failure): user_basis and
boson_basis_1d produce the same states and the same Hamiltonian.
"""
from __future__ import annotations

import _seeded_eigsh  # noqa: F401  deterministic ARPACK start vectors (see the module)
import json
import os
import sys

import numpy as np
from numba import carray, cfunc, float64, int32, uint32
from quspin.basis import boson_basis_1d
from quspin.basis.user import (
    count_particles_sig_32,
    map_sig_32,
    next_state_sig_32,
    op_sig_32,
)
from quspin.basis.user import user_basis
from quspin.operators import hamiltonian
from scipy.special import comb


@cfunc(
    op_sig_32,
    locals=dict(s=int32, n=int32, b=uint32, occ=int32, sps=uint32, me_offdiag=float64, me_diag=float64),
)
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


@cfunc(
    next_state_sig_32,
    locals=dict(t=uint32, i=int32, j=int32, n=int32, sps=uint32, b1=int32, b2=int32, l=int32, n_left=int32),
)
def next_state(s, counter, N, args):
    t = s
    sps = args[1]
    n = 0
    for i in range(N):
        b1 = (t // args[i]) % sps
        if b1 > 0:
            n += b1
            b2 = (t // args[i + 1]) % sps
            if b2 < (sps - 1):
                n -= 1
                t -= args[i]
                t += args[i + 1]
                if n > 0:
                    l_full = n // (sps - 1)
                    n_left = n % (sps - 1)
                    for j in range(i + 1):
                        t -= (t // args[j]) % sps * args[j]
                        if j < l_full:
                            t += (sps - 1) * args[j]
                        elif j == l_full:
                            t += n_left * args[j]
                break
    return t


def get_s0_pcon(N, Np):
    sps = 3
    l_full = Np // (sps - 1)
    s = sum((sps - 1) * sps**i for i in range(l_full))
    s += (Np % (sps - 1)) * sps**l_full
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


@cfunc(count_particles_sig_32, locals=dict(s=uint32))
def count_particles(x, p_count_ptr, args):
    s = x
    for i in range(args[0]):
        p_count_ptr[0] += s % args[1]
        s /= args[1]


def main() -> None:
    cfg = json.load(open(sys.argv[1]))
    out_path = sys.argv[2]

    N = int(os.environ.get("SAB_N", cfg.get("N", 6)))
    J = float(cfg["J"])
    U = float(cfg["U"])
    sps = 3
    Np = N // 2
    n_sectors = 1

    count_particles_args = np.array([N, sps], dtype=np.int32)
    P_args = np.array([sps], dtype=np.uint32)
    T_args = np.array([1, sps], dtype=np.uint32)
    next_state_args = np.array([sps**i for i in range(N)], dtype=np.uint32)
    op_args = np.array([sps], dtype=np.uint32)

    maps = dict(T_block=(translation, N, 0, T_args), P_block=(parity, 2, 0, P_args))
    pcon_dict = dict(
        Np=Np,
        next_state=next_state,
        next_state_args=next_state_args,
        get_Ns_pcon=get_Ns_pcon,
        get_s0_pcon=get_s0_pcon,
        count_particles=count_particles,
        count_particles_args=count_particles_args,
        n_sectors=n_sectors,
    )
    op_dict = dict(op=op, op_args=op_args)
    basis = user_basis(
        np.uint32, N, op_dict, allowed_ops=set("+-nI"), sps=sps, pcon_dict=pcon_dict, **maps
    )
    basis_1d = boson_basis_1d(N, Nb=Np, sps=sps, kblock=0, pblock=1)
    if basis.Ns != basis_1d.Ns:
        raise AssertionError("basis size mismatch between user_basis and boson_basis_1d")
    if not np.allclose(basis.states - basis_1d.states, 0.0, atol=1e-5):
        raise AssertionError("user_basis and boson_basis_1d states disagree")

    hopping = [[+J, j, (j + 1) % N] for j in range(N)]
    int_bb = [[0.5 * U, j, j] for j in range(N)]
    int_b = [[-0.5 * U, j] for j in range(N)]
    static = [["+-", hopping], ["-+", hopping], ["nn", int_bb], ["n", int_b]]
    no_checks = dict(check_symm=False, check_herm=False, check_pcon=False)
    H = hamiltonian(static, [], basis=basis, dtype=np.float64, **no_checks)
    H_1d = hamiltonian(static, [], basis=basis_1d, dtype=np.float64, **no_checks)
    if not np.allclose((H - H_1d).toarray(), 0.0, atol=1e-5):
        raise AssertionError("Hamiltonians built through user_basis and boson_basis_1d disagree")

    A = H.toarray()
    E = np.linalg.eigvalsh(A)
    E.sort()

    observable = {
        "spectrum": [float(x) for x in E],
        "h_re": [float(x) for x in A.flatten()],
    }
    json.dump(observable, open(out_path, "w"), sort_keys=True)


if __name__ == "__main__":
    main()
