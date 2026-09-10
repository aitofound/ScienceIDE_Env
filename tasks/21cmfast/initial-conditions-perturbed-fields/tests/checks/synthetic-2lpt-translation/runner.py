#!/usr/bin/env python3
"""Run one self-contained 21cmFAST initial/perturbed-field check."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
from powerbox import get_power

import py21cmfast as p21c
from py21cmfast.wrapper import cfuncs as cf


def array(box, name: str) -> np.ndarray:
    """Return one computed box field as an ordinary NumPy array."""
    value = box.get(name)
    return np.asarray(value.value if hasattr(value, "value") else value)


def save(out: Path, name: str, values: np.ndarray) -> None:
    np.save(out / name, np.asarray(values, dtype=np.float64), allow_pickle=False)


def spectrum(values: np.ndarray, box_len: float) -> np.ndarray:
    power = get_power(
        np.asarray(values), boxlength=box_len, bins_upto_boxlen=True
    )[0]
    return np.asarray(power, dtype=np.float64)


def distribution(values: np.ndarray, xmin: float, xmax: float) -> np.ndarray:
    hist, _ = np.histogram(
        np.asarray(values), bins=np.linspace(xmin, xmax, 50), density=True
    )
    return np.repeat(hist, 2).astype(np.float64)


def inputs_from_cfg(cfg: dict, **extra) -> p21c.InputParameters:
    options = {
        "HII_DIM": int(cfg["hii_dim"]),
        "DIM": int(cfg["dim"]),
        "BOX_LEN": float(cfg["box_len"]),
        "N_THREADS": int(cfg["threads"]),
        **extra,
    }
    return p21c.InputParameters(
        random_seed=int(cfg["seed"]), node_redshifts=()
    ).evolve_input_structs(**options)


def run_integration(cfg: dict, out: Path) -> None:
    inputs = inputs_from_cfg(
        cfg,
        SOURCE_MODEL="E-INTEGRAL",
        USE_EXP_FILTER=False,
        CELL_RECOMB=False,
        USE_TS_FLUCT=False,
        USE_UPPER_STELLAR_TURNOVER=False,
        PERTURB_ALGORITHM=cfg["algorithm"],
        PERTURB_ON_HIGH_RES=bool(cfg["highres"]),
    )
    initial = p21c.compute_initial_conditions(inputs=inputs, write=False)
    perturbed = p21c.perturb_field(
        redshift=float(cfg["redshift"]),
        initial_conditions=initial,
        write=False,
    )
    density = array(perturbed, "density")
    velocity = array(perturbed, "velocity_z") * 1.0e16
    save(out, "power-density.npy", spectrum(density, float(cfg["box_len"])))
    save(out, "power-velocity.npy", spectrum(velocity, float(cfg["box_len"])))
    save(out, "pdf-density.npy", distribution(density, -0.8, 2.0))
    save(out, "pdf-velocity.npy", distribution(velocity, -2.0, 2.0))


def run_injected(cfg: dict, out: Path) -> None:
    inputs = inputs_from_cfg(cfg, KEEP_3D_VELOCITIES=True)
    dim = int(cfg["dim"])
    supplied = -np.ones((dim, dim, dim), dtype=np.float64)
    perturbed_cell = np.float32(supplied[0, 0, 1])
    for _ in range(int(cfg["density_perturb_ulps"])):
        perturbed_cell = np.nextafter(perturbed_cell, np.float32(-np.inf))
    supplied[0, 0, 1] = perturbed_cell
    centre = (dim // 2,) * 3
    supplied[centre] = supplied[0, 0, 0] - supplied.sum()
    initial = p21c.compute_initial_conditions(
        inputs=inputs, initial_density=supplied, write=False
    )
    for name in (
        "hires_density",
        "lowres_density",
        "lowres_vx",
        "lowres_vy",
        "lowres_vz",
        "lowres_vx_2LPT",
        "lowres_vy_2LPT",
        "lowres_vz_2LPT",
    ):
        save(out, f"{name}.npy", array(initial, name))


def moments(values: np.ndarray) -> np.ndarray:
    values = np.asarray(values, dtype=np.float64)
    return np.array(
        [
            values.mean(),
            values.std(),
            np.sqrt(np.mean(values * values)),
            *np.quantile(values, [0.1, 0.5, 0.9]),
        ],
        dtype=np.float64,
    )


def run_class_transfer(cfg: dict, out: Path) -> None:
    inputs = inputs_from_cfg(cfg, POWER_SPECTRUM="CLASS", KEEP_3D_VELOCITIES=True)
    initial = p21c.compute_initial_conditions(inputs=inputs, write=False)
    for name in ("hires_density", "lowres_density", "lowres_vx", "lowres_vx_2LPT"):
        values = array(initial, name)
        save(out, f"power-{name}.npy", spectrum(values, float(cfg["box_len"])))
        save(out, f"moments-{name}.npy", moments(values))


def run_relative_velocity(cfg: dict, out: Path, tutorial: bool) -> None:
    if tutorial:
        inputs = p21c.InputParameters.from_template(
            "Munoz21",
            random_seed=int(cfg["seed"]),
            node_redshifts=p21c.get_logspaced_redshifts(
                min_redshift=5.0, max_redshift=35.0, z_step_factor=1.06
            ),
        ).evolve_input_structs(
            HII_DIM=int(cfg["hii_dim"]),
            DIM=int(cfg["dim"]),
            BOX_LEN=float(cfg["box_len"]),
            N_THREADS=int(cfg["threads"]),
            USE_RELATIVE_VELOCITIES=True,
        )
    else:
        inputs = inputs_from_cfg(
            cfg, POWER_SPECTRUM="CLASS", USE_RELATIVE_VELOCITIES=True
        )
    initial = p21c.compute_initial_conditions(inputs=inputs, write=False)
    vcb = array(initial, "lowres_vcb")
    save(out, "vcb-moments.npy", moments(vcb))
    save(out, "power-vcb.npy", spectrum(vcb, float(cfg["box_len"])))
    if tutorial:
        density = array(initial, "lowres_density")
        save(out, "density-moments.npy", moments(density))
        save(out, "power-density.npy", spectrum(density, float(cfg["box_len"])))


def fake_initial_conditions(
    inputs: p21c.InputParameters, redshift: float, velocity_perturb_ulps: int
):
    initial = p21c.InitialConditions.new(inputs=inputs)
    growth = cf.get_growth_factor(inputs=inputs, redshift=redshift)
    initial_growth = cf.get_growth_factor(
        inputs=inputs, redshift=inputs.simulation_options.INITIAL_REDSHIFT
    )
    factor = int(inputs.simulation_options.HIRES_TO_LOWRES_FACTOR)
    low_dim = inputs.simulation_options.HII_DIM
    high_dim = inputs.simulation_options.DIM
    fac_1lpt = inputs.simulation_options.cell_size / (growth - initial_growth)
    fac_2lpt = inputs.simulation_options.cell_size / (
        (-3.0 / 7.0) * (growth**2 - initial_growth**2)
    )
    for name, field in initial.arrays.items():
        setattr(initial, name, field.initialize().computed())
    velocity = np.ones_like(initial.get("lowres_vx"))
    initial.set("lowres_vx", np.zeros_like(velocity))
    initial.set("lowres_vy", fac_1lpt * velocity)
    initial.set("lowres_vz", np.zeros_like(velocity))
    if inputs.matter_options.PERTURB_ALGORITHM == "2LPT":
        initial.set("lowres_vx_2LPT", np.zeros_like(velocity))
        initial.set("lowres_vy_2LPT", np.zeros_like(velocity))
        initial.set("lowres_vz_2LPT", fac_2lpt * velocity)
    if velocity_perturb_ulps:
        field = (
            "lowres_vz_2LPT"
            if inputs.matter_options.PERTURB_ALGORITHM == "2LPT"
            else "lowres_vy"
        )
        displaced = np.asarray(initial.get(field)).copy()
        direction = np.asarray(np.inf, dtype=displaced.dtype)
        for _ in range(velocity_perturb_ulps):
            displaced = np.nextafter(displaced, direction)
        initial.set(field, displaced)
    low_density = np.zeros_like(initial.get("lowres_density"))
    low_density[0, 0, 0] = 1
    low_density[low_dim // 2, low_dim // 2, low_dim // 2] = -1
    initial.set("lowres_density", low_density)
    high_density = np.zeros_like(initial.get("hires_density"))
    high_density[0, 0, 0] = factor**3
    high_density[high_dim // 2, high_dim // 2, high_dim // 2] = -(factor**3)
    initial.set("hires_density", high_density)
    return initial


def run_synthetic(cfg: dict, out: Path) -> None:
    inputs = inputs_from_cfg(
        cfg,
        SOURCE_MODEL="L-INTEGRAL",
        USE_UPPER_STELLAR_TURNOVER=False,
        PERTURB_ON_HIGH_RES=False,
        R_BUBBLE_MAX=1.0,
        PERTURB_ALGORITHM=cfg["algorithm"],
    )
    redshift = float(cfg["redshift"])
    initial = fake_initial_conditions(
        inputs, redshift, int(cfg.get("velocity_perturb_ulps", 0))
    )
    perturbed = p21c.perturb_field(
        initial_conditions=initial,
        redshift=redshift,
        regenerate=True,
        write=False,
    )
    save(out, "density.npy", array(perturbed, "density"))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--dim", type=int)
    parser.add_argument("--hii-dim", type=int)
    parser.add_argument("--threads", type=int)
    args = parser.parse_args()
    cfg = json.loads(Path(args.input).read_text(encoding="utf-8"))
    for key, value in (
        ("dim", args.dim),
        ("hii_dim", args.hii_dim),
        ("threads", args.threads),
    ):
        if value is not None:
            cfg[key] = value
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    mode = cfg["mode"]
    if mode == "integration":
        run_integration(cfg, out)
    elif mode == "injected-density":
        run_injected(cfg, out)
    elif mode == "class-transfer":
        run_class_transfer(cfg, out)
    elif mode == "relative-velocity":
        run_relative_velocity(cfg, out, tutorial=False)
    elif mode == "munoz21-relative-velocity":
        run_relative_velocity(cfg, out, tutorial=True)
    elif mode == "synthetic-translation":
        run_synthetic(cfg, out)
    else:
        raise ValueError(f"unknown mode: {mode}")


if __name__ == "__main__":
    main()
