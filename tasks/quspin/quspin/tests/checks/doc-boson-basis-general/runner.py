import json, os, sys
import numpy as np
from quspin.operators import hamiltonian
from quspin.basis import boson_basis_general

np.random.seed(0)
cfg = json.loads(open(sys.argv[1]).read())
out_path = sys.argv[2]
Lx = int(os.environ.get("SAB_LX", cfg.get("Lx", 3)))
Ly = int(os.environ.get("SAB_LY", cfg.get("Ly", 3)))
J = float(cfg["J"]); U = float(cfg["U"]); mu = float(cfg["mu"])
N_2d = Lx * Ly

s = np.arange(N_2d)
x = s % Lx; y = s // Lx
T_x = (x + 1) % Lx + Lx * y
T_y = x + Lx * ((y + 1) % Ly)
P_x = x + Lx * (Ly - y - 1)
P_y = (Lx - x - 1) + Lx * y
basis = boson_basis_general(N_2d, sps=3, kxblock=(T_x, 0), kyblock=(T_y, 0), pxblock=(P_x, 0), pyblock=(P_y, 0))
hopping = [[-J, i, T_x[i]] for i in range(N_2d)] + [[-J, i, T_y[i]] for i in range(N_2d)]
potential = [[-mu - U / 2.0, i] for i in range(N_2d)]
interaction = [[U / 2.0, i, i] for i in range(N_2d)]
static = [["+-", hopping], ["-+", hopping], ["n", potential], ["nn", interaction]]
H = hamiltonian(static, [], basis=basis, dtype=np.float64)
E = np.sort(H.eigvalsh())
observable = {"spectrum": E.tolist()}
json.dump(observable, open(out_path, "w"), sort_keys=True)
