#!/usr/bin/env python3
"""Reverse-mode gradient of a guided-mode-expansion quantity with respect to a
parameterised geometry.

Physics: legume's distinguishing capability. With the autograd backend every
output is a differentiable function of every geometric input, so the derivative
of a photonic observable with respect to hole positions, radii and layer
thicknesses comes from one reverse-mode pass rather than one forward solve per
parameter. That is what makes inverse design of photonic crystals tractable, and
it is the workflow the upstream example this check derives from demonstrates.

The geometry is built from the initial condition: a list of layers, each with a
thickness and a list of shapes whose numeric fields may reference the free
parameters by index. The objective is named in the initial condition and is one
of the quantities an inverse-design loop actually maximises.

Graded: the objective and the analytic gradient. The finite-difference gradient
is computed as a physics anchor -- a derivative taken with no differentiation
machinery -- and the driver fails loudly if the two disagree beyond the bound,
but it is not graded pointwise because a finite difference carries its own step
error far larger than the analytic gradient's noise.
"""
import json
import os
import sys

import numpy as np
import legume
import autograd.numpy as npa
from autograd import grad


def resolve(value, params):
    """A shape field is either a literal, or {"param": i} naming a free parameter,
    or a list of either."""
    if isinstance(value, dict) and "param" in value:
        return params[value["param"]]
    if isinstance(value, list):
        return [resolve(v, params) for v in value]
    return value


def main(ic_path, out_dir):
    p = json.load(open(ic_path))
    p["gmax"] = float(os.environ.get("SAB_GMAX", p["gmax"]))
    if "numeig" in p["options"]:
        p["options"]["numeig"] = int(os.environ.get("SAB_NUMEIG", p["options"]["numeig"]))

    lat = p["lattice"]
    mk_lat = (lambda: legume.Lattice(lat)) if isinstance(lat, str) else (
        lambda: legume.Lattice(np.array(lat[0], dtype=float), np.array(lat[1], dtype=float)))

    def solve(params):
        phc = legume.PhotCryst(mk_lat(), **{k: p[k] for k in ("eps_l", "eps_u") if k in p})
        for layer in p["layers"]:
            phc.add_layer(d=resolve(layer["d"], params), eps_b=layer["eps_b"])
            for s in layer.get("shapes", []):
                s = dict(s); kind = s.pop("type")
                phc.add_shape(getattr(legume, kind)(**{k: resolve(v, params) for k, v in s.items()}))
        gme = legume.GuidedModeExp(phc, gmax=p["gmax"],
                                   **({"truncate_g": p["truncate_g"]} if "truncate_g" in p else {}))
        gme.run(kpoints=np.array(p["kpoints"]), **p["options"])
        return gme

    def q_single_mode(g):
        """Quality factor of ONE chosen eigenmode, the way upstream's cavity examples
        do it: compute_rad returns that mode's radiative linewidth on demand.

        Summing Q over every mode would be wrong physics here. At a high-symmetry
        wavevector most modes lie below the light line and do not radiate at all --
        measured in this structure, freqs_im runs down to 1e-32 -- so Q diverges for
        them and a sum over modes is dominated by numerical noise rather than by the
        cavity resonance anyone is designing. Selecting the radiating mode is what
        makes the objective the quantity an inverse-design loop actually maximises.
        """
        freq_im, _, _ = g.compute_rad(p["kind"], [p["mode"]])
        return g.freqs[p["kind"], p["mode"]] / 2 / freq_im[0]

    def linewidth_single_mode(g):
        """The radiative linewidth of one chosen mode, not its quality factor.

        For a bound state in the continuum the linewidth is the physics: it collapses
        toward zero as the wavevector approaches the BIC, and Q = freq/(2*linewidth)
        diverges there. Differentiating Q at the BIC is meaningless -- the analytic
        gradient of a divergent quantity is what upstream's own example avoids by
        working slightly off the symmetry point -- so this objective grades the
        linewidth itself, evaluated at a wavevector where the mode still radiates.
        """
        freq_im, _, _ = g.compute_rad(p["kind"], [p["mode"]])
        return freq_im[0]

    OBJECTIVES = {
        # the quality factor of one chosen cavity mode
        "q_single_mode": q_single_mode,
        # the radiative linewidth of one chosen mode
        "linewidth_single_mode": linewidth_single_mode,
        # the summed real frequencies: a band-structure target, no radiative part
        "sum_freqs": lambda g: npa.sum(g.freqs),
        # one band's width across the sampled wavevectors: a waveguide dispersion target
        "band_spread": lambda g: npa.max(g.freqs[:, 0]) - npa.min(g.freqs[:, 0]),
    }
    objective = lambda params: OBJECTIVES[p["objective"]](solve(params))

    legume.set_backend("autograd")
    params = npa.array(p["params"], dtype=float)

    value = float(objective(params))
    g_analytic = np.asarray(grad(objective)(params), dtype=np.float64)
    g_numeric = np.asarray(legume.utils.grad_num(objective, params), dtype=np.float64)

    # Compare the two gradients against the SCALE of the gradient vector, not
    # component by component. A per-component relative test is the wrong test here:
    # a component can be exactly zero for a physical reason -- translating the whole
    # crystal cannot change its spectrum, so the derivative with respect to a rigid
    # shift is identically zero in the analytic pass -- while the finite difference
    # returns its own step noise there and the ratio is meaningless.
    scale = float(np.max(np.abs(g_numeric))) or 1.0
    rel = float(np.max(np.abs(g_analytic - g_numeric)) / scale)
    if rel > p["grad_agreement_bound"]:
        raise SystemExit(f"driver.py: reverse-mode gradient disagrees with the finite-difference "
                         f"gradient by {rel:.3e} of the gradient scale, above "
                         f"{p['grad_agreement_bound']:.0e}")
    print(f"grad agreement: {rel:.3e} of scale {scale:.3e}", file=sys.stderr)
    np.ascontiguousarray(g_numeric, dtype="<f8").tofile(os.path.join(out_dir, "grad_numeric_ungraded.f64"))

    for name, arr in (("objective", np.array([value])), ("grad_analytic", g_analytic)):
        np.ascontiguousarray(arr, dtype="<f8").tofile(os.path.join(out_dir, name + ".f64"))
        print(f"{name}: {arr.ravel()}", file=sys.stderr)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
