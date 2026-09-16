#!/usr/bin/env python3
"""runner.py for ex-example12: adapted from examples/scripts/example12.py.

Builds the driven 2D J1-J2 spin model on an Lx x Ly lattice (full basis, no
symmetries, matching upstream's own choice to exercise plain OpenMP/MKL
parallelism), diagonalises for the top of the spectrum, and stroboscopically
evolves the top eigenstate, measuring its energy at each period. Upstream's
demonstration is the thread-count speedup itself (argv-controlled
OMP_NUM_THREADS/MKL_NUM_THREADS); that is a performance knob (SAB_THREADS in
run.sh), not a graded input. SAB_L2D shrinks the lattice (upstream: 4x4).
Note: at this model's couplings the top-of-spectrum ("LA") eigenvalues form
an exact SU(2)-symmetric maximal-total-spin multiplet (measured: k=10
eigenvalues agree to ~1e-14, the S=N/2 multiplet size for N=Lx*Ly=9 sites)
-- a real physical degeneracy, not solver noise; sorting keeps the pointwise
comparison well-defined regardless.
"""
from __future__ import annotations

import json
import os
import sys
from pathlib import Path

import numpy as np
from quspin.basis import spin_basis_general
from quspin.operators import hamiltonian


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: runner.py <config.json> <observable.json>")
    cfg = json.loads(Path(sys.argv[1]).read_text())
    Lx = int(os.environ.get("SAB_L2D", cfg.get("Lx", 3)))
    Ly = int(os.environ.get("SAB_L2D", cfg.get("Ly", 3)))
    J1 = float(cfg.get("J1", 1.0))
    J2 = float(cfg.get("J2", 0.5))
    Omega = float(cfg.get("Omega", 8.0))
    n_top = int(cfg.get("n_top", 10))
    n_periods = int(cfg.get("n_periods", 5))
    N_2d = Lx * Ly

    sites = np.arange(N_2d)
    x, y = sites % Lx, sites // Lx
    T_x = (x + 1) % Lx + Lx * y
    T_y = x + Lx * ((y + 1) % Ly)
    T_a = (x + 1) % Lx + Lx * ((y + 1) % Ly)
    T_d = (x - 1) % Lx + Lx * ((y + 1) % Ly)

    basis_2d = spin_basis_general(N_2d, pauli=False)

    def drive(t, Omega):
        return np.cos(Omega * t)

    drive_args = [Omega]
    J1_list = [[J1, i, T_x[i]] for i in range(N_2d)] + [[J1, i, T_y[i]] for i in range(N_2d)]
    J2_list = [[J2, i, T_d[i]] for i in range(N_2d)] + [[J2, i, T_a[i]] for i in range(N_2d)]
    static = [["xx", J1_list], ["yy", J1_list], ["zz", J1_list]]
    dynamic = [["xx", J2_list, drive, drive_args], ["yy", J2_list, drive, drive_args],
               ["zz", J2_list, drive, drive_args]]
    H = hamiltonian(static, dynamic, basis=basis_2d, dtype=np.float64, check_symm=False, check_herm=False)

    E_raw, V = H.eigsh(time=0.0, k=n_top, which="LA")
    E = np.sort(E_raw)
    psi_0 = V[:, 0]  # upstream's own choice: the first returned eigenvector

    t = np.linspace(0.0, n_periods * 2 * np.pi / Omega, n_periods + 1)
    psi_t = H.evolve(psi_0, t[0], t, iterate=True, rtol=1e-12, atol=1e-12)
    Et = []
    for j, psi in enumerate(psi_t):
        Et.append(float(H.expt_value(psi, time=t[j]).real))

    obs = {
        "E_top": [float(x) for x in E],
        "Et": Et,
    }
    Path(sys.argv[2]).write_text(json.dumps(obs, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
