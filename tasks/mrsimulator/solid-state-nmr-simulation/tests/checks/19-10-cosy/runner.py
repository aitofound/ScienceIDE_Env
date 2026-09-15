#!/usr/bin/env python3
"""Execute an exact pinned official MRSimulator example and record every spectrum."""
from __future__ import annotations

import argparse
import json
import os
import runpy
from pathlib import Path

os.environ.setdefault("MPLBACKEND", "Agg")
os.environ.setdefault("MPLCONFIGDIR", "/tmp/matplotlib")

import matplotlib.pyplot as plt
import numpy as np
from mrsimulator import Simulator


def two_ulps(value: float) -> float:
    """Move a finite binary64 value two representable numbers toward +infinity."""
    first = np.nextafter(np.float64(value), np.float64(np.inf))
    return float(np.nextafter(first, np.float64(np.inf)))


def set_two_ulp(obj, attr: str) -> str | None:
    if obj is None or not hasattr(obj, attr):
        return None
    value = getattr(obj, attr)
    if isinstance(value, (bool, str)) or value is None:
        return None
    try:
        old = float(value)
    except (TypeError, ValueError):
        return None
    if not np.isfinite(old) or old == 0.0:
        return None
    new = two_ulps(old)
    try:
        setattr(obj, attr, new)
    except Exception:
        return None
    return f"{type(obj).__name__}.{attr}: {old:.17g} -> {new:.17g}"


def has_nonzero_abundance(system) -> bool:
    try:
        return float(system.abundance) != 0.0
    except (AttributeError, TypeError, ValueError):
        return True


def perturb(sim: Simulator) -> str:
    """Perturb the first active physical input reached by this official scenario."""
    systems = [item for item in sim.spin_systems if has_nonzero_abundance(item)]
    systems.sort(key=lambda item: not bool(item.couplings))
    for system in systems:
        for site in system.sites:
            for obj, attr in (
                (getattr(site, "shielding_symmetric", None), "zeta"),
                (getattr(site, "quadrupolar", None), "Cq"),
                (site, "isotropic_chemical_shift"),
            ):
                changed = set_two_ulp(obj, attr)
                if changed:
                    return changed
        for coupling in system.couplings:
            for obj, attr in (
                (getattr(coupling, "dipolar", None), "D"),
                (coupling, "isotropic_j"),
            ):
                changed = set_two_ulp(obj, attr)
                if changed:
                    return changed
    for method in sim.methods:
        for attr in ("magnetic_flux_density", "rotor_frequency", "rotor_angle"):
            changed = set_two_ulp(method, attr)
            if changed:
                return changed
    raise RuntimeError("variant could not find a finite nonzero active physical input")


def spectra(sim: Simulator) -> list[np.ndarray]:
    arrays: list[np.ndarray] = []
    for method in sim.methods:
        dataset = getattr(method, "simulation", None)
        if dataset is None:
            continue
        for dependent in dataset.dependent_variables:
            values = np.asarray(dependent.components, dtype=np.complex128).ravel()
            arrays.append(np.concatenate((values.real, values.imag)).astype("<f8"))
    return arrays


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--scenario", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    spec = json.loads(Path(args.input).read_text())
    is_variant = spec.get("initial_condition") == "variant"

    captured: list[np.ndarray] = []
    changes: list[str] = []
    seen: set[int] = set()
    original_run = Simulator.run

    def recorded_run(self, *run_args, **run_kwargs):
        if is_variant and id(self) not in seen:
            changes.append(perturb(self))
            seen.add(id(self))
        density = os.environ.get("SAB_MRSIM_INTEGRATION_DENSITY")
        gamma = os.environ.get("SAB_MRSIM_GAMMA_ANGLES")
        if density:
            self.config.integration_density = int(density)
        if gamma:
            self.config.number_of_gamma_angles = int(gamma)
        result = original_run(self, *run_args, **run_kwargs)
        captured.extend(array.copy() for array in spectra(self))
        return result

    Simulator.run = recorded_run
    plt.show = lambda *args, **kwargs: None
    try:
        runpy.run_path(args.scenario, run_name="__main__")
    finally:
        Simulator.run = original_run
        plt.close("all")

    if not captured:
        raise RuntimeError("official example completed without a Simulator spectrum")
    if is_variant and not changes:
        raise RuntimeError("variant completed without perturbing an active input")
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    np.concatenate(captured).astype("<f8", copy=False).tofile(args.out)
    if changes:
        print("SAB_VARIANT=" + "; ".join(changes))


if __name__ == "__main__":
    main()
