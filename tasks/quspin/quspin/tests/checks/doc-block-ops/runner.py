import json, os, sys
import numpy as np
from quspin.operators import hamiltonian
from quspin.basis import boson_basis_1d
from quspin.tools.block_tools import block_ops

np.random.seed(0)
cfg = json.loads(open(sys.argv[1]).read())
out_path = sys.argv[2]
L = int(os.environ.get("SAB_L", cfg.get("L", 6)))
J = float(cfg["J"]); U = float(cfg["U"])
num = int(os.environ.get("SAB_NUM_T", cfg.get("num", 31)))

int_list_1 = [[-0.5 * U, i] for i in range(L)]
int_list_2 = [[0.5 * U, i, i] for i in range(L)]
hop_list = [[-J, i, (i + 1) % L] for i in range(L)]
static = [["+-", hop_list], ["-+", hop_list], ["nn", int_list_2], ["n", int_list_1]]
blocks = [dict(kblock=kblock) for kblock in range(L)]
basis_args = (L,)
basis_kwargs = dict(Nb=L // 2, sps=3)
H_block = block_ops(blocks, static, [], boson_basis_1d, basis_args, np.complex128,
                     basis_kwargs=basis_kwargs, get_proj_kwargs=dict(pcon=True))

basis = boson_basis_1d(L, Nb=L // 2, sps=3)
no_checks = dict(check_herm=False, check_symm=False, check_pcon=False)
n_list = [hamiltonian([["n", [[1.0, i]]]], [], basis=basis, dtype=np.float64, **no_checks) for i in range(L)]

i0 = basis.index("111000") if L == 6 else basis.index("1" * (L // 2) + "0" * (L - L // 2))
psi = np.zeros(basis.Ns, dtype=np.float64)
psi[i0] = 1.0

H_block.compute_all_blocks()
start, stop = 0, 10
times = np.linspace(start, stop, num)
psi_t = H_block.evolve(psi, times[0], times)
n_t = np.vstack([n.expt_value(psi_t).real for n in n_list]).T

observable = {"density_profile": n_t.tolist()}
json.dump(observable, open(out_path, "w"), sort_keys=True)
