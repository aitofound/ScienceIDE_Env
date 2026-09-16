import json, os, sys
import numpy as np
from quspin.operators import hamiltonian
from quspin.basis import boson_basis_1d
from quspin.tools.evolution import evolve
from quspin.tools.Floquet import Floquet_t_vec

np.random.seed(0)
cfg = json.loads(open(sys.argv[1]).read())
out_path = sys.argv[2]
L = int(os.environ.get("SAB_L", cfg.get("L", 20)))
J = float(cfg["J"]); kappa_trap = float(cfg["kappa_trap"]); U = float(cfg["U"])
A = float(cfg["A"]); Omega = float(cfg["Omega"])
periods = int(os.environ.get("SAB_PERIODS", cfg.get("periods", 5)))
i_CM = L / 2.0 - 0.5

def drive(t, Omega):
    return np.exp(-1j * A * np.sin(Omega * t))

def drive_conj(t, Omega):
    return np.exp(+1j * A * np.sin(Omega * t))

t = Floquet_t_vec(Omega, periods, len_T=1)
hopping = [[-J, i, (i + 1) % L] for i in range(L)]
trap = [[kappa_trap * (i - i_CM) ** 2, i] for i in range(L)]
static = [["n", trap]]
dynamic = [["+-", hopping, drive, [Omega]], ["-+", hopping, drive_conj, [Omega]]]
basis = boson_basis_1d(L, Nb=1, sps=2)
H = hamiltonian(static, dynamic, basis=basis, dtype=np.complex128)
E, V = H.eigh(time=0)
phi0 = V[:, 0] * np.sqrt(L)

def GPE(time, phi):
    phi_dot = -1j * (H.static.dot(phi) + U * np.abs(phi) ** 2 * phi)
    for fun, Hdyn in H.dynamic.items():
        phi_dot += -1j * fun(time) * Hdyn.dot(phi)
    return phi_dot

phi_t = evolve(phi0, t.i, t.vals, GPE)
density_complex = (np.abs(phi_t) ** 2).T

def GPE_real(time, phi, H, U):
    phi_dot = np.zeros_like(phi)
    Ns = H.Ns
    phi_dot[:Ns] = H.static.dot(phi[Ns:]).real
    phi_dot[Ns:] = -H.static.dot(phi[:Ns]).real
    phi_dot_2 = np.abs(phi[:Ns]) ** 2 + np.abs(phi[Ns:]) ** 2
    phi_dot[:Ns] += U * phi_dot_2 * phi[Ns:]
    phi_dot[Ns:] -= U * phi_dot_2 * phi[:Ns]
    for func, Hdyn in H.dynamic.items():
        fun = func(time)
        phi_dot[:Ns] += (+(fun.real) * Hdyn.dot(phi[Ns:]) + (fun.imag) * Hdyn.dot(phi[:Ns])).real
        phi_dot[Ns:] += (-(fun.real) * Hdyn.dot(phi[:Ns]) + (fun.imag) * Hdyn.dot(phi[Ns:])).real
    return phi_dot

GPE_params = (H, U)
phi_t_real = evolve(phi0, t.i, t.vals, GPE_real, stack_state=True, f_params=GPE_params)
density_real = (np.abs(phi_t_real) ** 2).T

observable = {
    "density_complex_form": density_complex.tolist(),
    "density_real_form": density_real.tolist(),
}
json.dump(observable, open(out_path, "w"), sort_keys=True)
