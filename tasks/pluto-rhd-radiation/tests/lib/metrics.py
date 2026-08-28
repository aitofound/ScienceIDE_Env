"""Deterministic geometry-aware observables for staged RHD validators."""
from __future__ import annotations

import math


def fields(frame: list[list[float]], names: list[str]) -> dict[str, list[float]]:
    return {name: frame[index] for index, name in enumerate(names)}


def state_scale(frame: list[list[float]]) -> float:
    total = sum(value * value for field in frame for value in field)
    scale = math.sqrt(total)
    if not math.isfinite(scale) or scale <= 0.0:
        raise ValueError("whole-state normalization scale is zero or nonfinite")
    return scale


def _weights(dims: tuple[int, int, int], geometry: str) -> list[float]:
    nx, ny, nz = dims
    weights = []
    for index in range(nx * ny * nz):
        i = index % nx
        j = (index // nx) % ny
        k = index // (nx * ny)
        if geometry == "cylindrical":
            weights.append(float(i + 1))
        elif geometry == "spherical":
            weights.append(float((i + 1) * (j + 1)))
        else:
            weights.append(1.0 + 0.0 * k)
    return weights


def signature(frame: list[list[float]], names: list[str], dims: tuple[int, int, int],
              geometry: str, requested: list[str]) -> dict[str, float]:
    data = fields(frame, names)
    weights = _weights(dims, geometry)
    rho = data["rho"]
    result = {}
    for metric in requested:
        if metric == "state_scale":
            result[metric] = state_scale(frame)
        elif metric == "shock_contact_positions":
            result[metric] = sum((i + 0.5) * abs(rho[i] - rho[i - 1])
                                 for i in range(1, len(rho)))
        elif metric == "radial_profile":
            result[metric] = sum(w * r for w, r in zip(weights, rho))
        elif metric == "angular_scatter":
            mean = sum(w * r for w, r in zip(weights, rho)) / sum(weights)
            result[metric] = math.sqrt(sum(w * (r - mean) ** 2 for w, r in zip(weights, rho)))
        elif metric == "geometry_weighted_fields":
            result[metric] = sum(w * r for w, r in zip(weights, rho))
        elif metric == "jet_head_and_cocoon":
            result[metric] = max(rho) - min(rho)
        elif metric == "body_force_budget":
            # Fixed Newtonian-approximation constant, deliberately NOT
            # rubric.physics.gamma: this metric's only current user
            # (rhd-blast3d-03-taub) is EOS==TAUB, which has no compiled
            # g_gamma (Src/globals.h:117-121) -- there is no row-derived
            # gamma to thread through here, only a scientifically-pending
            # placeholder observable (see rubric.json's "body_force" note).
            # Threading physics.gamma through would pass None/NaN instead of
            # fixing anything; if a future ideal-EOS row adopts this
            # observable, its real gamma must be threaded in explicitly then.
            newtonian_gamma = 5.0 / 3.0
            energy = [p / (newtonian_gamma - 1.0) + 0.5 * r * (x * x + y * y + z * z)
                      for r, x, y, z, p in zip(data["rho"], data["vx1"], data["vx2"],
                                                data["vx3"], data["prs"])]
            result[metric] = sum(w * e for w, e in zip(weights, energy))
        elif metric == "quadrant_wave_cuts":
            mid = len(rho) // 2
            result[metric] = sum(rho[:mid]) - sum(rho[mid:])
        elif metric == "entropy_recovery":
            entropy = data.get("entropy")
            if entropy is None:
                raise ValueError("entropy_recovery requires entropy output")
            result[metric] = sum(entropy) / len(entropy)
        else:
            raise ValueError(f"unknown metric {metric!r}")
        if not math.isfinite(result[metric]):
            raise ValueError(f"metric {metric} is nonfinite")
    return result
