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

import _seeded_eigsh  # noqa: F401  deterministic ARPACK start vectors (see the module)
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

    # The top of this spectrum is the fully polarised S = N/2 multiplet, an
    # exactly (N+1)-fold degenerate level (10 states at 3x3): ARPACK with k=10
    # and which="LA" sometimes returns nine of them plus the next level down,
    # depending on the start vector (measured: one graded value off by 3.0
    # between two solves). The dense solver is exact here (Ns = 512), so the
    # top n_top+2 eigenvalues come from numpy.linalg.eigh, which also reaches
    # the first level below the multiplet; the initial state is the dense
    # eigenvector of the largest eigenvalue (any state of the multiplet gives
    # the same energy trace, both Hamiltonian terms being SU(2) symmetric).
    E_all, V_all = np.linalg.eigh(np.asarray(H.toarray(time=0.0)))
    E = np.sort(E_all)[-(n_top + 2):]
    psi_0 = np.asarray(V_all)[:, int(np.argmax(E_all))].ravel()

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
