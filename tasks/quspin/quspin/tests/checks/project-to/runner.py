#!/usr/bin/env python3
"""Adapted from code/quspin/test/test_project_to.py.

Upstream builds basis.get_proj() projectors for 24 symmetry sectors of a
spin chain and checks basis.project_from / basis.project_to against the
dense projector matrix, on fixed-seed random input vectors. There is no
Hamiltonian coupling in this check: the projectors are fixed by the lattice
symmetry (discrete quantum numbers), so the only physical input this check
can vary is the seeded state itself. This check keeps a handful of
representative sectors and grades the projected-amplitude magnitudes
basis.project_from / basis.project_to produce ("projected amplitudes as
|amplitude|"). The variant shifts the first component of both seeded input
vectors (the sector-space v for project_from, and the full-basis v_full for
project_to) by v0_perturbation before renormalising and projecting -- the
leaf's usual "one physical coupling, 450 ulps" convention does not apply
here since there is no Hamiltonian, so this input-state amplitude shift
plays that role instead. It is not an overall scale (renormalisation after
the shift changes every component's relative weight, i.e. the state's
direction), so it moves every graded entry through the (generically dense)
projector map. Measured: v0_perturbation=1e-13 gave a spread only about one
order of magnitude above the confirmed-zero repeat-run floor, so the default
is raised to 1e-11 to land comfortably above it.
"""
from __future__ import annotations

import json
import os
import sys

import numpy as np
from quspin.basis import spin_basis_general


def main() -> int:
    cfg = json.loads(open(sys.argv[1]).read())
    out_path = sys.argv[2]

    L = int(os.environ.get("SAB_L", cfg.get("L", 8)))
    eps = float(cfg.get("v0_perturbation", 0.0))

    z = -(np.arange(L) + 1)
    p = np.arange(L)[::-1]
    t = (np.arange(L) + 1) % L

    bases = {
        "full": spin_basis_general(L),
        "Nup_half": spin_basis_general(L, Nup=L // 2),
        "z0": spin_basis_general(L, zb=(z, 0)),
        "z0_p0": spin_basis_general(L, zb=(z, 0), pb=(p, 0)),
        "z0_p0_t0": spin_basis_general(L, zb=(z, 0), pb=(p, 0), tb=(t, 0)),
    }

    observable = {}
    for name, basis in bases.items():
        pcon = "Nup" in name
        P = basis.get_proj(np.complex128, pcon=pcon)
        Ns_full, Ns = P.shape

        rng = np.random.RandomState(0)
        v = rng.normal(size=Ns).astype(np.complex128)
        v[0] += eps
        v /= np.linalg.norm(v)

        v_full = rng.normal(size=Ns_full).astype(np.complex128)
        v_full[0] += eps
        v_full /= np.linalg.norm(v_full)

        pf = basis.project_from(v, sparse=False, pcon=pcon)
        pt = basis.project_to(v_full, sparse=False, pcon=pcon)

        observable[f"{name}_project_from_abs_sorted"] = sorted(float(x) for x in np.abs(pf))
        observable[f"{name}_project_to_abs_sorted"] = sorted(float(x) for x in np.abs(pt))

    with open(out_path, "w") as fh:
        json.dump(observable, fh, sort_keys=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
