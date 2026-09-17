#!/usr/bin/env python3
"""Run one legume exciton-polariton calculation and write the graded arrays.

Physics: a quantum well embedded in a photonic crystal slab supports excitons,
bound electron-hole pairs, which couple to the slab's photonic modes. legume
solves the exciton Schroedinger equation on the lattice (legume/exc/) and then
diagonalises the Hopfield matrix that mixes excitons with photons
(legume/pol/). The eigenvalues are the polariton energies in eV; eners_im are
their linewidths; fractions_ex is how excitonic each polariton branch is, a
number between 0 and 1 that is the physical content of the mixing.
"""
import json
import os
import sys

import numpy as np
import scipy.constants as cs
import legume


def main(ic_path, out_dir):
    p = json.load(open(ic_path))
    p["gmax"] = float(os.environ.get("SAB_GMAX", p["gmax"]))
    p["num_k"] = int(os.environ.get("SAB_NPTS", p["num_k"]))

    a = p["a"]
    dqw = p["dqw_m"] / a
    lattice = legume.Lattice("square")
    lim = p["lim_k"]
    path = lattice.bz_path([[lim * np.pi, lim * np.pi], "g", [lim * np.pi, 0]], [p["num_k"]])

    phc = legume.PhotCryst(lattice, eps_l=p["eps_SiO2"])
    unetched = (p["d_unetch"] - dqw * 3) / 2
    phc.add_layer(d=unetched, eps_b=p["eps_GaAs"])
    phc.add_layer(d=dqw, eps_b=p["eps_AlGaAs"])
    phc.add_layer(d=dqw, eps_b=p["eps_GaAs"])
    phc.add_layer(d=dqw, eps_b=p["eps_AlGaAs"])
    phc.add_layer(d=unetched, eps_b=p["eps_GaAs"])
    phc.add_layer(d=p["d_etched"], eps_b=p["eps_GaAs"])
    phc.add_shape(legume.Circle(r=p["R"], eps=1, x_cent=0.0, y_cent=0.0))

    phc.add_qw(z=phc.layers[3].z_mid, a=a, M=cs.m_e * p["M_eff"], E0=p["E_x"],
               V_shapes=1, loss=p["loss"], osc_str=np.array(p["osc_str"], dtype=float))

    pol = legume.HopfieldPol(phc, p["gmax"], truncate_g="abs")
    gme_options = dict(p["gme_options"]); gme_options["angles"] = path["angles"]
    pol.run(kpoints=path["kpoints"], gme_options=gme_options,
            exc_options=p["exc_options"], verbose=False)

    for name, arr in (("eners", pol.eners), ("eners_im", pol.eners_im),
                      ("fractions_ex", pol.fractions_ex)):
        a2 = np.asarray(arr, dtype=np.float64)
        np.ascontiguousarray(a2, dtype="<f8").tofile(os.path.join(out_dir, name + ".f64"))
        print(f"{name}: shape {a2.shape}", file=sys.stderr)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
