#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

import numpy as np
import soxs
from numpy.testing import assert_allclose

from pyxsim.spectral_models import TableCIEModel


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--bins", required=True, type=int)
    args = parser.parse_args()

    config = json.loads(Path(args.input).read_text(encoding="utf-8"))
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=True)

    model = TableCIEModel("apec", 0.1, 10.0, args.bins, 1.0, 10.0, thermal_broad=True)
    model.prepare_spectrum(config["redshift"])
    cosmic, metal, _ = model.get_spectrum(config["temperature_keV"])
    spectrum = cosmic + config["abundance"] * metal

    generator = soxs.ApecGenerator(0.1, 10.0, args.bins, broadening=True)
    expected = generator.get_spectrum(
        config["temperature_keV"],
        config["abundance"],
        config["redshift"],
        1.0e-14,
    )
    assert_allclose(spectrum[0], expected.flux.value * expected.de.value)

    photon_flux = model.make_fluxf(config["band_min_keV"], config["band_max_keV"], energy=False)
    energy_flux = model.make_fluxf(config["band_min_keV"], config["band_max_keV"], energy=True)
    continuum_photon, metal_photon, _ = photon_flux(config["temperature_keV"])
    continuum_energy, metal_energy, _ = energy_flux(config["temperature_keV"])
    integrals = np.array(
        [
            continuum_photon + config["abundance"] * metal_photon,
            continuum_energy + config["abundance"] * metal_energy,
        ],
        dtype=np.float64,
    )

    np.save(output / "spectrum.npy", np.asarray(spectrum[0], dtype=np.float64))
    np.save(output / "band-integrals.npy", integrals)


if __name__ == "__main__":
    main()
