#!/usr/bin/env python3
"""Adapted from code/quspin/examples/scripts/example18.py.

Builds a honeycomb (hexagonal) lattice graph with networkx (SAB_M rows,
SAB_N columns of hexagons) and the spinful Fermi-Hubbard Hamiltonian on it,
    H = -t sum_{sigma,<i,j>} a^dagger_{i,sigma} a_{j,sigma} + U sum_i n_iu n_id,
diagonalized with eigsh. The graph-drawing/plot call is dropped; the physical
content kept is the sorted lowest SAB_K eigenvalues.
"""
from __future__ import annotations

import _seeded_eigsh  # noqa: F401  deterministic ARPACK start vectors (see the module)
import json
import os
import sys

import numpy as np
import networkx as nx

from quspin.basis import spinful_fermion_basis_general
from quspin.operators import hamiltonian


def main() -> int:
    config_path, out_path = sys.argv[1:3]
    config = json.loads(open(config_path).read())
    m = int(os.environ.get("SAB_M", 2))
    n = int(os.environ.get("SAB_N", 2))
    k = int(os.environ.get("SAB_K", 4))
    N_up = int(config["Nup"])
    N_down = int(config["Ndown"])
    t = float(config["t"])
    U = float(config["U"])

    hex_graph = nx.generators.lattice.hexagonal_lattice_graph(m, n, periodic=False)
    hex_graph = nx.convert_node_labels_to_integers(hex_graph)
    N = hex_graph.number_of_nodes()

    basis = spinful_fermion_basis_general(N, Nf=(N_up, N_down))
    tunnelling = [[-t, i, j] for i in range(N) for j in hex_graph.adj[i]]
    interactions = [[U, i, i] for i in range(N)]
    static = [["n|n", interactions], ["+-|", tunnelling], ["|+-", tunnelling]]
    H = hamiltonian(static, [], basis=basis, dtype=np.float64, check_pcon=False)
    E, V = H.eigsh(k=min(k, basis.Ns - 1), which="SA", maxiter=1e4)

    observable = {"spectrum": [float(x) for x in np.sort(E)]}
    with open(out_path, "w") as fh:
        json.dump(observable, fh, sort_keys=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
