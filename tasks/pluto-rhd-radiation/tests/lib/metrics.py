"""Deterministic geometry-aware observables for the active 18-row validators."""
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
        elif metric == "material_pressure_norm":
            # Honest dimensionful diagnostic: Euclidean norm of the emitted gas
            # pressure field. It carries no invented EOS/body-force constant.
            result[metric] = math.sqrt(sum(value * value for value in data["prs"]))
        elif metric == "radiation_moment_scale":
            required = ("enr", "fr1", "fr2", "fr3")
            if any(name not in data for name in required):
                raise ValueError("radiation_moment_scale requires enr and fr1-fr3")
            if any(not math.isfinite(value) for name in required for value in data[name]):
                raise ValueError("radiation moments must be finite")
            # Dimensionful Euclidean norm of the emitted radiation moments;
            # unlike a flux factor it introduces no unavailable c or reduced-c.
            result[metric] = math.sqrt(sum(value * value for name in required for value in data[name]))
            if result[metric] <= 0.0:
                raise ValueError("radiation moment scale is zero")
        elif metric == "radiation_energy_norm":
            if "enr" not in data or any(value < 0.0 or not math.isfinite(value) for value in data["enr"]):
                raise ValueError("radiation energy must be finite and nonnegative")
            result[metric] = math.sqrt(sum(value * value for value in data["enr"]))
        elif metric == "radiation_flux_norm":
            required = ("fr1", "fr2", "fr3")
            if any(name not in data for name in required):
                raise ValueError("radiation_flux_norm requires fr1-fr3")
            if any(not math.isfinite(value) for name in required for value in data[name]):
                raise ValueError("radiation flux must be finite")
            # A dimensionful Euclidean flux norm; this is not the dimensionless
            # |F|/(c E) flux factor and intentionally introduces no c constant.
            result[metric] = math.sqrt(sum(value * value for name in required for value in data[name]))
        elif metric == "magnetic_field_scale":
            required = ("Bx1", "Bx2", "Bx3")
            if any(name not in data for name in required):
                raise ValueError("magnetic_field_scale requires Bx1-Bx3")
            result[metric] = math.sqrt(sum(value * value
                                          for name in required for value in data[name]))
            if result[metric] <= 0.0:
                raise ValueError("magnetic field scale is zero")
        elif metric == "magnetic_divergence":
            required = ("Bx1", "Bx2", "Bx3")
            if any(name not in data for name in required):
                raise ValueError("magnetic_divergence requires Bx1-Bx3")
            # Cell-index finite-difference witness for a CT consumer. The exact
            # MHD flux/CT implementation is still tested by the coupled output
            # fields; this independent scalar catches field omission or
            # dimensional collapse in a candidate result.
            result[metric] = sum(abs(data["Bx1"][i] - data["Bx1"][i - 1])
                                 + abs(data["Bx2"][i] - data["Bx2"][i - 1])
                                 + abs(data["Bx3"][i] - data["Bx3"][i - 1])
                                 for i in range(1, len(rho)))
        elif metric == "tracer_scale":
            if "tr1" not in data:
                raise ValueError("tracer_scale requires tr1")
            # A configured tracer may be physically zero in an official deck
            # (RMHD_Blast/01 sets NTRACER=1 but does not seed tr1). Presence,
            # finiteness, and frame-to-frame comparison remain strict; zero is
            # not a missing-interface signal.
            result[metric] = math.sqrt(sum(value * value for value in data["tr1"]))
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
