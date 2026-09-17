import _seeded_eigsh  # noqa: F401  deterministic ARPACK start vectors (see the module)
import json, os, sys
import numpy as np
from quspin.basis import spin_basis_1d, photon_basis
from quspin.operators import hamiltonian
from quspin.basis.photon import coherent_state

np.random.seed(0)
cfg = json.loads(open(sys.argv[1]).read())
out_path = sys.argv[2]
Nph_tot = int(os.environ.get("SAB_NPH_TOT", cfg.get("Nph_tot", 30)))
Nph = float(cfg["Nph"])
Omega = float(cfg["Omega"]); A = float(cfg["A"]); Delta = float(cfg["Delta"])

ph_energy = [[Omega]]
at_energy = [[Delta, 0]]
absorb = [[A / (2.0 * np.sqrt(Nph)), 0]]
emit = [[A / (2.0 * np.sqrt(Nph)), 0]]
static = [["|n", ph_energy], ["x|-", absorb], ["x|+", emit], ["z|", at_energy]]
basis = photon_basis(spin_basis_1d, L=1, Nph=Nph_tot)
H = hamiltonian(static, [], dtype=np.float64, basis=basis)

psi_at_i = np.array([1.0, 0.0])
psi_ph_i = coherent_state(np.sqrt(Nph), Nph_tot + 1)
psi_i = np.kron(psi_at_i, psi_ph_i)

k = min(4, H.Ns - 1)
v0 = np.random.RandomState(0).standard_normal(H.Ns)
E = np.sort(H.eigsh(k=k, which="SA", return_eigenvectors=False, v0=v0))

observable = {
    "psi_i_abs": np.abs(psi_i).tolist(),
    "low_spectrum": E.tolist(),
}
json.dump(observable, open(out_path, "w"), sort_keys=True)
