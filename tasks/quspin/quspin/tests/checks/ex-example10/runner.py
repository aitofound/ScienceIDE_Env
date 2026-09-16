#!/usr/bin/env python3
"""runner.py for ex-example10: adapted from examples/scripts/example10.py.

Builds the driven Bose-Fermi mixture Hamiltonian on a tensor_basis (boson x
fermion), prepares a fixed Fock product state, evolves it under the driven
Hamiltonian, and measures the boson-fermion entanglement entropy vs time.
Plots dropped; SAB_L shrinks the chain (upstream: 6, kept as default here
since it is already small), SAB_NCYC shrinks the number of drive cycles
(upstream: 10) at a fixed 5 points/cycle. Entropy_t(t=0) is exactly 0 for
every parameter choice (the initial state is an unentangled Fock product
state), so t starts after 0.
"""
from __future__ import annotations

import _seeded_eigsh  # noqa: F401  deterministic ARPACK start vectors (see the module)
import json
import os
import sys
from pathlib import Path

import numpy as np
from quspin.basis import tensor_basis, spinless_fermion_basis_1d, boson_basis_1d
from quspin.operators import hamiltonian
from quspin.tools.Floquet import Floquet_t_vec
from quspin.tools.measurements import obs_vs_time


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: runner.py <config.json> <observable.json>")
    cfg = json.loads(Path(sys.argv[1]).read_text())
    L = int(os.environ.get("SAB_L", cfg.get("L", 6)))
    n_cyc = int(os.environ.get("SAB_NCYC", cfg.get("n_cycles", 3)))
    Jb = float(cfg.get("Jb", 1.0))
    Jf = float(cfg.get("Jf", 1.0))
    Uff = float(cfg.get("Uff", -2.0))
    Ubb = float(cfg.get("Ubb", 0.5))
    Ubf = float(cfg.get("Ubf", 5.0))
    A = float(cfg.get("A", 2.0))
    Omega = float(cfg.get("Omega", 1.0))
    Nf, Nb = L // 2, L

    def drive(t, Omega):
        return np.sin(Omega * t)

    drive_args = [Omega]
    basis_b = boson_basis_1d(L, Nb=Nb, sps=3)
    basis_f = spinless_fermion_basis_1d(L, Nf=Nf)
    basis = tensor_basis(basis_b, basis_f)

    hop_b = [[-Jb, i, (i + 1) % L] for i in range(L)]
    int_list_bb = [[Ubb / 2.0, i, i] for i in range(L)]
    int_list_bb_lin = [[-Ubb / 2.0, i] for i in range(L)]
    hop_f_right = [[-Jf, i, (i + 1) % L] for i in range(L)]
    hop_f_left = [[Jf, i, (i + 1) % L] for i in range(L)]
    int_list_ff = [[Uff, i, (i + 1) % L] for i in range(L)]
    drive_f = [[A * (-1.0) ** i, i] for i in range(L)]
    int_list_bf = [[Ubf, i, i] for i in range(L)]
    static = [["+-|", hop_b], ["-+|", hop_b], ["n|", int_list_bb_lin], ["nn|", int_list_bb],
              ["|+-", hop_f_left], ["|-+", hop_f_right], ["|nn", int_list_ff], ["n|n", int_list_bf]]
    dynamic = [["|n", drive_f, drive, drive_args]]
    no_checks = dict(check_pcon=False, check_symm=False, check_herm=False)
    H_BFM = hamiltonian(static, dynamic, basis=basis, **no_checks)

    s_f = "".join("1" for _ in range(Nf)) + "".join("0" for _ in range(L - Nf))
    s_b = "".join("1" for _ in range(Nb))
    i_0 = basis.index(s_b, s_f)
    psi_0 = np.zeros(basis.Ns)
    psi_0[i_0] = 1.0

    t = Floquet_t_vec(Omega, n_cyc, len_T=5)
    psi_t = H_BFM.evolve(psi_0, t.i, t.vals, iterate=True, rtol=1e-12, atol=1e-12)
    Sent_args = dict(basis=basis, sub_sys_A="left")
    meas = obs_vs_time(psi_t, t.vals, {}, Sent_args=Sent_args)
    Entropy_t = meas["Sent_time"]["Sent_A"]

    obs = {"Entropy_t": [float(x) for x in Entropy_t[1:]]}  # drop t=0: exactly 0 by construction
    Path(sys.argv[2]).write_text(json.dumps(obs, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
