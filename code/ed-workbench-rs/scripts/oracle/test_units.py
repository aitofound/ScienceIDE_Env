"""Regression tests for geometry and unit metadata in oracle systems."""

from __future__ import annotations

import json
import unittest
from pathlib import Path

import numpy
from pyscf import gto

from scripts.oracle.generate import (
    SYSTEMS,
    System,
    basis_for_system,
    bauschlicher_1986_basis,
)


class GeometryUnitTests(unittest.TestCase):
    def molecule(self, slug: str) -> tuple[System, numpy.ndarray]:
        system = SYSTEMS[slug]
        molecule = gto.M(
            atom=system.atom,
            basis=basis_for_system(system),
            charge=system.charge,
            spin=system.spin,
            unit=system.coordinate_unit,
            verbose=0,
        )
        return system, molecule.atom_coords(unit="Angstrom")

    def test_h2_coordinates_mean_a_1_4_angstrom_bond(self) -> None:
        system, coordinates = self.molecule("h2-sto3g")
        self.assertEqual(system.coordinate_unit, "Angstrom")
        distance = numpy.linalg.norm(coordinates[1] - coordinates[0])
        self.assertAlmostEqual(distance, 1.4, places=12)
        self.assertEqual(
            system.geometry_parameters,
            (("R(H-H)", 1.4, "angstrom"),),
        )

    def test_equilibrium_h2_has_a_0_7414_angstrom_bond(self) -> None:
        system, coordinates = self.molecule("h2-equilibrium-sto3g")
        self.assertEqual(system.coordinate_unit, "Angstrom")
        distance = numpy.linalg.norm(coordinates[1] - coordinates[0])
        self.assertAlmostEqual(distance, 0.7414, places=12)
        self.assertEqual(
            system.geometry_parameters,
            (("R(H-H)", 0.7414, "angstrom"),),
        )

    def test_h4_has_one_angstrom_adjacent_spacing(self) -> None:
        system, coordinates = self.molecule("h4-sto3g")
        self.assertEqual(system.coordinate_unit, "Angstrom")
        distances = [
            numpy.linalg.norm(coordinates[index + 1] - coordinates[index])
            for index in range(3)
        ]
        for distance in distances:
            self.assertAlmostEqual(distance, 1.0, places=12)
        self.assertEqual(
            system.geometry_parameters,
            (("adjacent R(H-H)", 1.0, "angstrom"),),
        )

    def test_water_geometry_is_0_967_angstrom_and_107_6_degrees(self) -> None:
        for slug in ("h2o-sto3g", "h2o-631g-fc", "h2o-ccpvdz-ae"):
            system, coordinates = self.molecule(slug)
            self.assertEqual(system.coordinate_unit, "Angstrom")
            first_bond = coordinates[1] - coordinates[0]
            second_bond = coordinates[2] - coordinates[0]
            first_length = numpy.linalg.norm(first_bond)
            second_length = numpy.linalg.norm(second_bond)
            cosine = numpy.dot(first_bond, second_bond) / (first_length * second_length)
            angle_degrees = numpy.degrees(numpy.arccos(cosine))
            self.assertAlmostEqual(first_length, 0.967, places=12)
            self.assertAlmostEqual(second_length, 0.967, places=12)
            self.assertAlmostEqual(angle_degrees, 107.6, places=12)
            self.assertEqual(
                system.geometry_parameters,
                (
                    ("R(O-H)", 0.967, "angstrom"),
                    ("angle(H-O-H)", 107.6, "degree"),
                ),
            )

    def test_stretched_water_scales_both_bonds_and_preserves_the_angle(self) -> None:
        for slug, scale in (
            ("h2o-631g-fc-r1p5", 1.5),
            ("h2o-631g-fc-r2p0", 2.0),
        ):
            system, coordinates = self.molecule(slug)
            first_bond = coordinates[1] - coordinates[0]
            second_bond = coordinates[2] - coordinates[0]
            first_length = numpy.linalg.norm(first_bond)
            second_length = numpy.linalg.norm(second_bond)
            cosine = numpy.dot(first_bond, second_bond) / (
                first_length * second_length
            )
            angle_degrees = numpy.degrees(numpy.arccos(cosine))
            self.assertAlmostEqual(first_length, scale * 0.967, places=12)
            self.assertAlmostEqual(second_length, scale * 0.967, places=12)
            self.assertAlmostEqual(angle_degrees, 107.6, places=12)
            self.assertEqual(system.frozen_orbitals, (0,))

    def test_committed_references_expose_units_and_geometry_parameters(self) -> None:
        fixtures_root = Path(__file__).resolve().parents[2] / "fixtures"
        for slug, system in SYSTEMS.items():
            filename = "reference.json" if system.compute_fci else "generation_metadata.json"
            reference_path = fixtures_root / slug / filename
            reference = json.loads(reference_path.read_text(encoding="utf-8"))
            self.assertEqual(
                reference["coordinate_unit"], system.coordinate_unit.lower()
            )
            self.assertEqual(reference["energy_unit"], "hartree")
            expected_parameters = [
                {"name": name, "value": value, "unit": unit}
                for name, value, unit in system.geometry_parameters
            ]
            self.assertEqual(reference["geometry_parameters"], expected_parameters)

    def test_bauschlicher_dz_uses_the_printed_basis_and_bohr_geometry(self) -> None:
        system = SYSTEMS["h2o-dz-ae"]
        self.assertEqual(system.coordinate_unit, "Bohr")
        self.assertTrue(system.symmetry)
        basis = bauschlicher_1986_basis()
        self.assertEqual(basis["H"][0][1], [19.2384, 0.032828])
        self.assertEqual(basis["H"][-1], [0, [0.177552, 1.0]])
        self.assertEqual(basis["O"][0][1], [7817.0, 0.002031])
        self.assertEqual(basis["O"][-1], [1, [0.2137, 1.0]])

        molecule = gto.M(
            atom=system.atom,
            basis=basis_for_system(system),
            unit=system.coordinate_unit,
            verbose=0,
        )
        coordinates = molecule.atom_coords(unit="Bohr")
        first_bond = coordinates[1] - coordinates[0]
        second_bond = coordinates[2] - coordinates[0]
        distance = numpy.linalg.norm(first_bond)
        cosine = numpy.dot(first_bond, second_bond) / distance**2
        angle_degrees = numpy.degrees(numpy.arccos(cosine))
        self.assertAlmostEqual(distance, 1.889726334392893, places=12)
        self.assertAlmostEqual(angle_degrees, 104.50000893084858, places=12)
        self.assertEqual(molecule.nao_nr(), 14)

    def test_bauschlicher_dzp_adds_the_printed_polarization_and_freezes_core(
        self,
    ) -> None:
        system = SYSTEMS["h2o-dzp-fc"]
        basis = bauschlicher_1986_basis(polarized=True)
        self.assertEqual(basis["H"][-1], [1, [0.8, 1.0]])
        self.assertEqual(basis["O"][-1], [2, [1.2, 1.0]])
        self.assertEqual(system.frozen_orbitals, (0,))
        self.assertTrue(system.symmetry)
        self.assertFalse(system.compute_fci)
        self.assertEqual(system.published_fci_energy, -76.256624)

        molecule = gto.M(
            atom=system.atom,
            basis=basis_for_system(system),
            unit=system.coordinate_unit,
            symmetry=system.symmetry,
            verbose=0,
        )
        self.assertEqual(molecule.nao_nr(), 25)

    def test_ccpvdz_benchmark_reference_does_not_run_full_fci(self) -> None:
        fixtures_root = Path(__file__).resolve().parents[2] / "fixtures"
        reference = json.loads(
            (fixtures_root / "h2o-ccpvdz-ae" / "reference.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertEqual(reference["number_of_molecular_orbitals"], 24)
        self.assertEqual(reference["number_of_electrons"], 10)
        self.assertEqual(reference["determinants"], 1_806_590_016)
        self.assertFalse(reference["point_group_symmetry"])
        self.assertFalse(reference["full_fci_executed"])


if __name__ == "__main__":
    unittest.main()
