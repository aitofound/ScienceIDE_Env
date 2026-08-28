"""Physics-selected verifier-side primitive-to-conserved maps.

The maps below are independently written from the production equations in
Src/RHD/mappers.c, Src/RHD/mappers_loc.c, Src/EOS/{Ideal,Taub}/eos.c, and
Src/HD/mappers.c.  They are used only for HD and RHD rows, where the complete
material map is verified.  MHD/RMHD rows deliberately do not claim an
independent conserved-state reconstruction: their strict raw-field/schema and
honest finite diagnostics remain the acceptance evidence until a complete
magnetic map is verified against production.
"""
from __future__ import annotations

import math


def _enthalpy(rho: float, prs: float, eos: str, gamma: float | None) -> float:
    theta = prs / rho
    if eos == "ideal":
        if gamma is None or not math.isfinite(gamma) or gamma <= 1.0:
            raise ValueError("ideal EOS requires finite gamma > 1")
        # PLUTO's ideal relativistic enthalpy, with c=1.
        return 1.0 + gamma / (gamma - 1.0) * theta
    if eos == "taub":
        # Exact Taub-Mathews closure used by Src/EOS/Taub/eos.c.
        return 2.5 * theta + math.sqrt(2.25 * theta * theta + 1.0)
    raise ValueError(f"no verified RHD enthalpy for eos {eos!r}")


def reconstruct(fields: dict[str, list[float]], eos: str, gamma: float | None,
                physics: str) -> dict[str, list[float]]:
    """Return a complete verified HD or RHD material conserved map.

    HD uses D=rho, M_i=rho*v_i, E=p/(gamma-1)+rho*v^2/2.  RHD uses
    D=rho*Lorentz, M_i=rho*h*Lorentz^2*v_i, E=rho*h*Lorentz^2-p.  Radiation
    moments are already conserved in PLUTO's interface and are intentionally
    not folded into the material map.
    """
    if physics not in {"HD", "RHD"}:
        raise ValueError(f"independent conserved map is intentionally unavailable for {physics}")
    rho, vx, vy, vz, prs = (fields[name] for name in ("rho", "vx1", "vx2", "vx3", "prs"))
    result = {"cons_rho": [], "cons_mom1": [], "cons_mom2": [],
              "cons_mom3": [], "cons_energy": []}
    for r, x, y, z, p in zip(rho, vx, vy, vz, prs):
        speed2 = x * x + y * y + z * z
        if physics == "HD":
            inertia = r
            energy = p / (gamma - 1.0) + 0.5 * r * speed2 if gamma is not None and gamma > 1.0 else float("nan")
            density = r
        else:
            if speed2 >= 1.0:
                raise ValueError("RHD primitive velocity is superluminal")
            lorentz2 = 1.0 / (1.0 - speed2)
            lorentz = math.sqrt(lorentz2)
            enthalpy = _enthalpy(r, p, eos, gamma)
            inertia = r * enthalpy * lorentz2
            energy = inertia - p
            density = r * lorentz
        result["cons_rho"].append(density)
        result["cons_mom1"].append(inertia * x)
        result["cons_mom2"].append(inertia * y)
        result["cons_mom3"].append(inertia * z)
        result["cons_energy"].append(energy)
    return result


def max_residual(fields: dict[str, list[float]], eos: str, gamma: float | None,
                 physics: str) -> float:
    """Measure residual only when caller supplies actual conserved fields."""
    expected = reconstruct(fields, eos, gamma, physics)
    worst = 0.0
    for name, values in expected.items():
        if name not in fields:
            raise ValueError(f"actual conserved field {name} is required for residual")
        for left, right in zip(fields[name], values):
            scale = max(1.0, abs(left), abs(right))
            worst = max(worst, abs(left - right) / scale)
    return worst
