#!/usr/bin/env python3
"""Run one pinned official MRSimulator source (a gallery script or a pytest file)
unchanged and record every spectrum its Simulator.run calls emit.

Modes (chosen by ic/<ic>/input.json, key "mode"):
  scenario  execute official_source.py with runpy, as the gallery does;
  pytest    execute official_source.py under pytest from the pinned source tree, so
            the upstream assertions (SIMPSON, RMNSIM, brute-force and self-consistency
            references) must pass as well; a failing test fails the run.

The graded output is the ordered stream of every complex spectrum, real samples then
imaginary samples per dependent variable, in execution order (spectrum.bin, f64).

Variant: before the first run of each Simulator object, one active physical input is
moved upward by N binary64 ulps (input.json "ulps", default 2); the nominal source is
never edited. Knobs (environment, see run.sh --help): SAB_MRSIM_INTEGRATION_DENSITY,
SAB_MRSIM_GAMMA_ANGLES (runtime; unset = upstream), SAB_THREADS (joblib n_jobs when the
source passes none; the graded default is 1, the upstream default).
"""
from __future__ import annotations

import argparse
import json
import os
import random
import runpy
import sys
from pathlib import Path

os.environ.setdefault("MPLBACKEND", "Agg")
os.environ.setdefault("MPLCONFIGDIR", "/tmp/matplotlib")

import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from mrsimulator import Simulator  # noqa: E402


def move_ulps(value: float, ulps: int) -> float:
    moved = np.float64(value)
    for _ in range(ulps):
        moved = np.nextafter(moved, np.float64(np.inf))
    return float(moved)


def set_ulps(obj, attr: str, ulps: int) -> str | None:
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
    new = move_ulps(old, ulps)
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


def perturb(sim: Simulator, ulps: int, order: str) -> str:
    """Perturb the first active physical input reached by this official source."""
    systems = [item for item in sim.spin_systems if has_nonzero_abundance(item)]
    systems.sort(key=lambda item: not bool(item.couplings))

    def coupling_targets(system):
        for coupling in system.couplings:
            yield getattr(coupling, "dipolar", None), "D"
            yield coupling, "isotropic_j"

    def site_targets(system):
        for site in system.sites:
            yield getattr(site, "shielding_symmetric", None), "zeta"
            yield getattr(site, "quadrupolar", None), "Cq"
            yield site, "isotropic_chemical_shift"

    for system in systems:
        groups = (coupling_targets, site_targets)
        if order == "sites-first":
            groups = (site_targets, coupling_targets)
        for group in groups:
            for obj, attr in group(system):
                changed = set_ulps(obj, attr, ulps)
                if changed:
                    return changed
    for method in sim.methods:
        for attr in ("magnetic_flux_density", "rotor_frequency", "rotor_angle"):
            changed = set_ulps(method, attr, ulps)
            if changed:
                return changed
    raise RuntimeError("variant could not find a finite nonzero active physical input")


def spectra(sim: Simulator) -> list[np.ndarray]:
    arrays: list[np.ndarray] = []
    for method in sim.methods:
        dataset = getattr(method, "simulation", None)
        if dataset is None:
            continue
        dependents = getattr(dataset, "dependent_variables", None)
        if dependents is None:  # run(pack_as_csdm=False) returns a bare ndarray
            values = np.asarray(dataset, dtype=np.complex128).ravel()
            arrays.append(np.concatenate((values.real, values.imag)).astype("<f8"))
            continue
        for dependent in dependents:
            values = np.asarray(dependent.components, dtype=np.complex128).ravel()
            arrays.append(np.concatenate((values.real, values.imag)).astype("<f8"))
    return arrays


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--source", required=True, help="the pinned official source copy")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    spec = json.loads(Path(args.input).read_text())
    is_variant = spec.get("initial_condition") == "variant"
    ulps = int(spec.get("ulps", 2))
    order = spec.get("perturb_order", "couplings-first")
    mode = spec.get("mode", "scenario")
    fixtures = spec.get("offline_fixtures", {})
    check_dir = Path(args.source).resolve().parent

    captured: list[np.ndarray] = []
    changes: list[str] = []
    seen: list[Simulator] = []  # strong references: no id reuse while the run lasts
    seen_ids: set[int] = set()
    original_run = Simulator.run
    original_load = Simulator.load_spin_systems
    density = os.environ.get("SAB_MRSIM_INTEGRATION_DENSITY", "").strip()
    gamma = os.environ.get("SAB_MRSIM_GAMMA_ANGLES", "").strip()
    threads = int(os.environ.get("SAB_THREADS", "1") or "1")

    def recorded_run(self, *run_args, **run_kwargs):
        if is_variant and id(self) not in seen_ids:
            changes.append(perturb(self, ulps, order))
            seen_ids.add(id(self))
            seen.append(self)
        if density and density != "upstream":
            self.config.integration_density = int(density)
        if gamma and gamma != "upstream":
            self.config.number_of_gamma_angles = int(gamma)
        if threads > 1 and "n_jobs" not in run_kwargs and len(run_args) < 2:
            run_kwargs["n_jobs"] = threads
        result = original_run(self, *run_args, **run_kwargs)
        captured.extend(array.copy() for array in spectra(self))
        return result

    def offline_load(self, filename):
        name = Path(str(filename)).name
        if name in fixtures:
            return original_load(self, str(check_dir / fixtures[name]))
        return original_load(self, filename)

    Simulator.run = recorded_run
    Simulator.load_spin_systems = offline_load
    plt.show = lambda *a, **k: None
    exit_code = 0
    try:
        if "seed" in spec:  # sources that draw inputs from numpy's global stream (Czjzek pdf
            np.random.seed(int(spec["seed"]))  # sampling, random tensor orientations, synthetic
            random.seed(int(spec["seed"]))  # datasets) run on a fixed seed so the inputs are pinned
        if mode == "pytest":
            import pytest

            source_dir = Path(os.environ["SOURCE_DIR"]).resolve()
            # The test file runs at its own place in the pinned tree (its package-relative
            # imports need that); the check's copy is the pinned reference it must equal.
            in_tree = source_dir / spec["upstream"]
            if in_tree.read_bytes() != Path(args.source).read_bytes():
                raise SystemExit(f"{in_tree} differs from the pinned official copy {args.source}")
            os.chdir(source_dir)
            sys.dont_write_bytecode = True
            pytest_args = [
                "-p", "no:cacheprovider", "-o", "addopts=",
                "-c", str(source_dir / "setup.cfg"), "--rootdir", str(source_dir),
                "-q", "-rA", "--no-header", str(in_tree),
            ]
            exit_code = int(pytest.main(pytest_args))
        else:
            runpy.run_path(args.source, run_name="__main__")
    finally:
        Simulator.run = original_run
        Simulator.load_spin_systems = original_load
        plt.close("all")

    if exit_code != 0:
        raise SystemExit(f"upstream test file failed under pytest (exit {exit_code})")
    if not captured:
        raise RuntimeError("official source completed without a Simulator spectrum")
    if is_variant and not changes:
        raise RuntimeError("variant completed without perturbing an active input")
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    np.concatenate(captured).astype("<f8", copy=False).tofile(args.out)
    print(f"SAB_SPECTRA={len(captured)} SAB_VALUES={sum(a.size for a in captured)}")
    if changes:
        print("SAB_VARIANT=" + "; ".join(changes))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
