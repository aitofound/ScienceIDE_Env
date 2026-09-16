import json, os, sys
import numpy as np
from quspin.operators import hamiltonian
from quspin.basis import spin_basis_1d

np.random.seed(0)
cfg = json.loads(open(sys.argv[1]).read())
out_path = sys.argv[2]
L = int(os.environ.get("SAB_L", cfg.get("L", 4)))
J = float(cfg["J"]); g = float(cfg["g"]); h = float(cfg["h"]); Omega = float(cfg["Omega"])

def drive(t, Omega):
    return np.cos(Omega * t)

basis = spin_basis_1d(L=L, a=1, kblock=0, pblock=1)
x_field = [[g, i] for i in range(L)]
z_field = [[h, i] for i in range(L)]
J_nn = [[J, i, (i + 1) % L] for i in range(L)]
static = [["zz", J_nn], ["z", z_field]]
dynamic = [["x", x_field, drive, [Omega]]]
H = hamiltonian(static, dynamic, static_fmt="dia", dtype=np.float64, basis=basis)
E = np.sort(H.eigvalsh(time=0.0))
observable = {"spectrum_t0": E.tolist()}
json.dump(observable, open(out_path, "w"), sort_keys=True)
