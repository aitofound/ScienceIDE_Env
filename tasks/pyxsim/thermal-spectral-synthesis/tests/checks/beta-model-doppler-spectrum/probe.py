#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

import numpy as np
import unyt as u
from yt import YTQuantity
from yt.loaders import load_uniform_grid
from yt.utilities.cosmology import Cosmology

from pyxsim import CIESourceModel


K_PER_KEV = (1.0 * u.keV).to_value("K", "thermal")
MASS_HYDROGEN = u.mass_hydrogen.to_value("g")


def make_dataset(grid, config):
    radius_mpc = 1.0
    core_radius_mpc = 0.05
    central_density = 0.04 * MASS_HYDROGEN * config["density_scale"]
    coordinates = np.mgrid[
        -radius_mpc : radius_mpc : grid * 1j,
        -radius_mpc : radius_mpc : grid * 1j,
        -radius_mpc : radius_mpc : grid * 1j,
    ]
    radius = np.sqrt(np.sum(coordinates**2, axis=0))
    density = np.zeros((grid, grid, grid), dtype=np.float64)
    inside = radius <= radius_mpc
    density[inside] = central_density * (1.0 + (radius[inside] / core_radius_mpc) ** 2) ** -1.0
    shape = density.shape
    velocity = YTQuantity(config["velocity_fraction_c"], "c").to_value("cm/s")
    data = {
        "density": (density, "g/cm**3"),
        "temperature": (np.full(shape, config["temperature_keV"] * K_PER_KEV), "K"),
        "velocity_x": (np.zeros(shape), "cm/s"),
        "velocity_y": (np.zeros(shape), "cm/s"),
        "velocity_z": (np.full(shape, velocity), "cm/s"),
        "metallicity": (np.full(shape, config["metallicity"]), "Zsun"),
    }
    return load_uniform_grid(
        data,
        shape,
        length_unit=(2.0 * radius_mpc, "Mpc"),
        bbox=np.array([[-0.5, 0.5], [-0.5, 0.5], [-0.5, 0.5]]),
        nprocs=1,
        default_species_fields="ionized",
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--grid", required=True, type=int)
    parser.add_argument("--model-bins", required=True, type=int)
    parser.add_argument("--output-bins", required=True, type=int)
    args = parser.parse_args()

    config = json.loads(Path(args.input).read_text(encoding="utf-8"))
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    dataset = make_dataset(args.grid, config)
    sphere = dataset.sphere("c", (0.5, "Mpc"))
    model = CIESourceModel("apec", 0.1, 11.0, args.model_bins, config["metallicity"])
    cosmology = Cosmology()

    cosmological = model.make_spectrum(
        sphere,
        0.2,
        6.0,
        args.output_bins,
        redshift=config["redshift"],
        cosmology=cosmology,
    )
    rest_frame = model.make_spectrum(sphere, 0.2, 6.0, args.output_bins)
    line_of_sight = model.make_spectrum(
        sphere,
        0.2,
        6.0,
        args.output_bins,
        redshift=config["redshift"],
        cosmology=cosmology,
        normal="z",
    )
    transverse = model.make_spectrum(
        sphere,
        0.2,
        6.0,
        args.output_bins,
        redshift=config["redshift"],
        cosmology=cosmology,
        normal="x",
    )

    np.save(output / "cosmological-flux.npy", np.asarray(cosmological.flux.value, dtype=np.float64))
    np.save(output / "rest-frame-rate.npy", np.asarray(rest_frame.rate.value, dtype=np.float64))
    np.save(output / "line-of-sight-flux.npy", np.asarray(line_of_sight.flux.value, dtype=np.float64))
    np.save(output / "transverse-flux.npy", np.asarray(transverse.flux.value, dtype=np.float64))


if __name__ == "__main__":
    main()
