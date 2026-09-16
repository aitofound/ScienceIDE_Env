import json, os, sys
import numpy as np
from quspin.operators import hamiltonian
from quspin.basis import tensor_basis, spinless_fermion_basis_1d
from quspin.tools.measurements import obs_vs_time

np.random.seed(0)
cfg = json.loads(open(sys.argv[1]).read())
out_path = sys.argv[2]
L = int(os.environ.get("SAB_L", cfg.get("L", 8)))
J = float(cfg["J"]); U = float(cfg["U"])
nt = int(os.environ.get("SAB_NT", cfg.get("nt", 6)))
N = L // 2
N_up = N // 2 + N % 2
N_down = N // 2

basis_up = spinless_fermion_basis_1d(L, Nf=N_up)
basis_down = spinless_fermion_basis_1d(L, Nf=N_down)
basis = tensor_basis(basis_up, basis_down)

hop_right = [[-J, i, i + 1] for i in range(L - 1)]
hop_left = [[J, i, i + 1] for i in range(L - 1)]
int_list = [[U, i, i] for i in range(L)]
static = [["+-|", hop_left], ["-+|", hop_right], ["|+-", hop_left], ["|-+", hop_right], ["n|n", int_list]]
no_checks = dict(check_pcon=False, check_symm=False, check_herm=False)
H = hamiltonian(static, [], basis=basis, **no_checks)
v0 = np.random.RandomState(0).standard_normal(basis.Ns)
E, V = H.eigsh(k=4, which="SA", v0=v0)

# imbalance-style observable: density on the odd sublattice (up species)
n_odd = [[1.0, i] for i in range(1, L, 2)]
I_op = hamiltonian([["n|", n_odd]], [], basis=basis, dtype=np.float64, **no_checks)

up_str = "1" * N_up + "0" * (L - N_up)
down_str = "0" * (L - N_down) + "1" * N_down
i0 = basis.index(up_str, down_str)
psi0 = np.zeros(basis.Ns, dtype=np.complex128)
psi0[i0] = 1.0
times = np.linspace(0.0, 2.0, nt)
psi_t = H.evolve(psi0, 0.0, times, iterate=True)
Obs_time = obs_vs_time(psi_t, times, dict(I=I_op))

observable = {
    "low_spectrum": np.sort(E).tolist(),
    "imbalance_t": np.asarray(Obs_time["I"], dtype=float).tolist(),
}
json.dump(observable, open(out_path, "w"), sort_keys=True)
