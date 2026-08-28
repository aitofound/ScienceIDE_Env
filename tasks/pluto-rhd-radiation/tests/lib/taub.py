"""Independent primitive/conserved reconstruction used by RHD checks.

This is a verifier-side calculation, not an import of PLUTO code.  The staged
fixtures use the same equations only to create a self-contained contract test;
measured scientific bounds remain pending CPU repeats.
"""
from __future__ import annotations

import math


def reconstruct(fields: dict[str, list[float]], eos: str, gamma: float) -> dict[str, list[float]]:
    rho = fields["rho"]
    vx = fields["vx1"]
    vy = fields["vx2"]
    vz = fields["vx3"]
    prs = fields["prs"]
    result = {"cons_rho": [], "cons_mom1": [], "cons_mom2": [],
              "cons_mom3": [], "cons_energy": []}
    for r, x, y, z, p in zip(rho, vx, vy, vz, prs):
        speed2 = x * x + y * y + z * z
        if eos == "taub":
            theta = p / r
            enthalpy = 0.5 * (5.0 * theta + math.sqrt(9.0 * theta * theta + 4.0))
            lorentz2 = 1.0 / max(1.0 - speed2, 1.0e-300)
            inertia = r * enthalpy * lorentz2
            energy = inertia - p
        else:
            inertia = r
            energy = p / (gamma - 1.0) + 0.5 * r * speed2
        result["cons_rho"].append(r)
        result["cons_mom1"].append(inertia * x)
        result["cons_mom2"].append(inertia * y)
        result["cons_mom3"].append(inertia * z)
        result["cons_energy"].append(energy)
    return result


def max_residual(fields: dict[str, list[float]], eos: str, gamma: float) -> float:
    expected = reconstruct(fields, eos, gamma)
    worst = 0.0
    for name, values in expected.items():
        actual = fields[name]
        for left, right in zip(actual, values):
            scale = max(1.0, abs(left), abs(right))
            worst = max(worst, abs(left - right) / scale)
    return worst
