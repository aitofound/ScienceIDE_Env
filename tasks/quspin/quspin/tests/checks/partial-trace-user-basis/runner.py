#!/usr/bin/env python3
"""Adapted from code/quspin/test/test_partial_trace_user_basis.py.

Upstream builds a custom `user_basis` mixing hardcore bosons and spinless
fermions on two N_half-site legs (via numba cfuncs implementing the fermion
sign), draws one random pure state, and for every 3+3-site subsystem
combination checks that a local hop operator's expectation value on the
subsystem, from basis.partial_trace, agrees with its expectation on the full
state. We keep that construction and the full-vs-reduced consistency check
as an internal guard, but fix a single subsystem instead of looping over
every combination, and replace the un-parameterised random draw with a
two-state mixing angle theta (both source states seeded) so the variant has
a physical knob that moves every entry (see rubric "variant").
Graded: the sorted reduced-DM spectrum of that fixed subsystem.
"""
from __future__ import annotations

import json
import os
import sys

import numpy as np
from numba import carray, cfunc, jit, uint32, int32
from quspin.basis import spin_basis_general
from quspin.basis.user import next_state_sig_32, op_sig_32, user_basis
from quspin.operators import hamiltonian


def make_basis(N_half):
    old_basis = spin_basis_general(N_half, m=0)
    states = old_basis.states
    shift_states = np.left_shift(states, N_half)
    shape = states.shape + states.shape
    states_b = np.broadcast_to(states, shape)
    shift_states_b = np.broadcast_to(shift_states, shape)
    return (states_b + shift_states_b.T).ravel()


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
    if 2 * site_ind < N:
        f_count = _count_particles_32(op_struct.state, site_ind)
    else:
        f_count = 0
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


@cfunc(next_state_sig_32)
def next_state(s, counter, N, args):
    return args[counter + 1]


class function_wrapper(object):
    def __init__(self, basis):
        self.basis = basis

    def get_s0_pcon(self, N, Np):
        return self.basis[0]

    def get_Ns_pcon(self, N, Np):
        return self.basis.size


def build(N_half):
    N = 2 * N_half
    external_basis = make_basis(N_half)
    op_args = np.array([], dtype=np.uint32)
    FW = function_wrapper(external_basis)
    noncommuting_bits = [(np.arange(N_half), -1)]
    pcon_dict = dict(
        Np=(), next_state=next_state, next_state_args=external_basis,
        get_Ns_pcon=FW.get_Ns_pcon, get_s0_pcon=FW.get_s0_pcon,
    )
    op_dict = dict(op=op, op_args=op_args)
    basis = user_basis(
        np.uint32, N, op_dict, allowed_ops=set("n+-"), sps=2,
        pcon_dict=pcon_dict, noncommuting_bits=noncommuting_bits,
    )
    subsys_basis = user_basis(
        np.uint32, N_half, op_dict, allowed_ops=set("n+-"), sps=2,
        noncommuting_bits=[(np.arange(N_half), -1)],
    )
    return basis, subsys_basis


def main() -> int:
    config_path, out_path = sys.argv[1], sys.argv[2]
    cfg = json.load(open(config_path))
    N_half = int(os.environ.get("SAB_NHALF", cfg.get("N_half", 6)))
    theta = float(cfg["theta"])
    seed = int(cfg.get("seed", 0))
    n_A = int(cfg.get("n_A", 3))

    basis, subsys_basis = build(N_half)
    rng = np.random.default_rng(seed)
    a = rng.normal(0, 1, size=(basis.Ns,)); a /= np.linalg.norm(a)
    b = rng.normal(0, 1, size=(basis.Ns,)); b /= np.linalg.norm(b)
    psi = np.cos(theta) * a + np.sin(theta) * b
    psi /= np.linalg.norm(psi)

    spin_part = list(range(n_A))
    fermion_part = list(range(N_half, N_half + n_A))
    sub_sys_A = np.hstack((spin_part, fermion_part))

    rho = basis.partial_trace(
        psi, sub_sys_A=sub_sys_A, return_rdm="A", subsys_ordering=False, enforce_pure=False,
    )

    coef = 1.0
    i, j = 0, 1
    static_ss = [["+-", [[coef, i, j], [coef, j, i]]]]
    static_full = [["+-", [[coef, int(sub_sys_A[i]), int(sub_sys_A[j])],
                            [coef, int(sub_sys_A[j]), int(sub_sys_A[i])]]]]
    O_ss = hamiltonian(
        static_ss, [], basis=subsys_basis, dtype=np.float64,
        check_symm=False, check_herm=False, check_pcon=False,
    )
    O_full = hamiltonian(
        static_full, [], basis=basis, dtype=np.float64,
        check_symm=False, check_herm=False, check_pcon=False,
    )
    ess = float(np.real(O_ss.expt_value(rho)))
    efull = float(np.real(O_full.expt_value(psi)))

    guard = abs(ess - efull)
    if guard > 1e-8:
        raise AssertionError(f"reduced vs full hop expectation mismatch: {guard}")

    observable = {
        "rdm_A_eigenvalues": [float(x) for x in np.sort(np.linalg.eigvalsh(np.asarray(rho)))],
        "reduced_hop_expectation": ess,
    }
    with open(out_path, "w") as f:
        json.dump(observable, f, sort_keys=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
