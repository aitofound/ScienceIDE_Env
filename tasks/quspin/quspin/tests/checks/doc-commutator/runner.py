import _seeded_eigsh  # noqa: F401  deterministic ARPACK start vectors (see the module)
import json, os, sys
import numpy as np
from quspin.operators import hamiltonian
from quspin.basis import spin_basis_1d
from quspin.tools.measurements import diag_ensemble

np.random.seed(0)
cfg = json.loads(open(sys.argv[1]).read())
out_path = sys.argv[2]
L = int(os.environ.get("SAB_L", cfg.get("L", 12)))
h = float(cfg["h"]); g = float(cfg["g"]); J = float(cfg.get("J", 1.0))
J1 = float(cfg.get("J1", 0.1))
J_zz = [[J, i, (i + 1) % L] for i in range(L)]
J1_zz = [[J1, i, (i + 1) % L] for i in range(L)]
x_field = [[h, i] for i in range(L)]
z_field = [[g, i] for i in range(L)]
# H1 is x(h)+z(g) upstream, a sum of decoupled single-site terms with a
# combinatorially degenerate spectrum: any single eigenvector picked from a
# degenerate manifold is arbitrary and not reproducible between two correct
# solves, so a weak zz(J1) bond is added to lift the degeneracy (see
# default_vs_upstream). H2 keeps the original zz(J)+x(h)+z(g) upstream form.
static_1 = [["zz", J1_zz], ["x", x_field], ["z", z_field]]
static_2 = [["zz", J_zz], ["x", x_field], ["z", z_field]]
basis = spin_basis_1d(L, kblock=0, pblock=1)
H1 = hamiltonian(static_1, [], basis=basis, dtype=np.float64)

static_2 = [["zz", J_zz], ["x", x_field], ["z", z_field]]
H2 = hamiltonian(static_2, [], basis=basis, dtype=np.float64)
from quspin.operators import commutator
comm = commutator(H1, H2)
M = comm.toarray()
observable = {
    "comm_real": M.real.tolist(),
    "comm_imag": M.imag.tolist(),
}
json.dump(observable, open(out_path, "w"), sort_keys=True)
