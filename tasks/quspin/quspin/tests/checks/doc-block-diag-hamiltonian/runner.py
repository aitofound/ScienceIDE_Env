import _seeded_eigsh  # noqa: F401  deterministic ARPACK start vectors (see the module)
import json, os, sys
import numpy as np
from quspin.operators import hamiltonian
from quspin.basis import boson_basis_1d
from quspin.tools.block_tools import block_diag_hamiltonian

np.random.seed(0)
cfg = json.loads(open(sys.argv[1]).read())
out_path = sys.argv[2]
L = int(os.environ.get("SAB_L", cfg.get("L", 6)))
J = float(cfg["J"]); deltaJ = float(cfg["deltaJ"]); Delta = float(cfg["Delta"])

hop = [[-J - deltaJ * (-1) ** i, i, (i + 1) % L] for i in range(L)]
stagg_pot = [[Delta * (-1) ** i, i] for i in range(L)]
static = [["+-", hop], ["-+", hop], ["n", stagg_pot]]
basis = boson_basis_1d(L, Nb=1, sps=2)
H = hamiltonian(static, [], basis=basis, dtype=np.float64)
E, V = H.eigh()

blocks = [dict(Nb=1, sps=2, kblock=i, a=2) for i in range(L // 2)]
basis_args = (L,)
FT, Hblock = block_diag_hamiltonian(blocks, static, [], boson_basis_1d, basis_args,
                                     np.complex128, get_proj_kwargs=dict(pcon=True))
Eblock, Vblock = Hblock.eigh()

observable = {
    "spectrum_real_space": np.sort(E).tolist(),
    "spectrum_block_diag": np.sort(np.real(Eblock)).tolist(),
}
json.dump(observable, open(out_path, "w"), sort_keys=True)
