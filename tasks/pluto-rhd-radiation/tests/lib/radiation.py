"""Radiation-side helpers reserved for the explicitly staged radiation rows."""
from __future__ import annotations

import math


def flux_factor(enr: list[float], fr1: list[float], fr2: list[float], fr3: list[float],
                light_speed: float) -> list[float]:
    if light_speed <= 0.0:
        raise ValueError("light speed must be positive")
    values = []
    for e, x, y, z in zip(enr, fr1, fr2, fr3):
        if e <= 0.0 or not all(math.isfinite(v) for v in (e, x, y, z)):
            raise ValueError("radiation moment is nonpositive or nonfinite")
        values.append(math.sqrt(x * x + y * y + z * z) / (light_speed * e))
    return values


def exchange_residual(material_before: list[float], material_after: list[float],
                      radiation_before: list[float], radiation_after: list[float],
                      geometry_scale: float) -> float:
    if geometry_scale <= 0.0:
        raise ValueError("geometry scale must be positive")
    residual = 0.0
    for mb, ma, rb, ra in zip(material_before, material_after,
                              radiation_before, radiation_after):
        residual = max(residual, abs((ma - mb) + (ra - rb)) / geometry_scale)
    return residual
