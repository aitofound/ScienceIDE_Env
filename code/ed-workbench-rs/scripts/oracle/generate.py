#!/usr/bin/env python3
"""Generate deterministic Level 0 FCIDUMP and reference fixtures with PySCF."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from dataclasses import dataclass
from pathlib import Path
import platform
import sys

import pyscf
import numpy
from pyscf import ao2mo, cc, fci, gto, mp, scf
from pyscf.tools import fcidump


@dataclass(frozen=True)
class System:
    slug: str
    name: str
    atom: str
    basis: str = "sto-3g"
    basis_definition: str | None = None
    basis_provenance: str | None = None
    charge: int = 0
    spin: int = 0
    coordinate_unit: str = "Angstrom"
    geometry_parameters: tuple[tuple[str, float, str], ...] = ()
    frozen_orbitals: tuple[int, ...] = ()
    symmetry: bool = False
    compute_fci: bool = True
    published_fci_energy: float | None = None
    benchmark_only: bool = False


def bauschlicher_1986_basis(polarized: bool = False) -> dict[str, list[list[object]]]:
    """Return the H2O basis printed in Bauschlicher and Taylor, JCP 85, 2779."""

    hydrogen: list[list[object]] = [
        [
            0,
            [19.2384, 0.032828],
            [2.89872, 0.231204],
            [0.653472, 0.817226],
        ],
        [0, [0.177552, 1.0]],
    ]
    oxygen: list[list[object]] = [
        [
            0,
            [7817.0, 0.002031],
            [1176.0, 0.015436],
            [273.2, 0.073771],
            [81.17, 0.247606],
            [27.18, 0.611832],
            [3.414, 0.241205],
        ],
        [0, [9.532, 1.0]],
        [0, [0.9398, 1.0]],
        [0, [0.2846, 1.0]],
        [
            1,
            [35.18, 0.019580],
            [7.904, 0.124200],
            [2.305, 0.394714],
            [0.7171, 0.627375],
        ],
        [1, [0.2137, 1.0]],
    ]
    if polarized:
        hydrogen.append([1, [0.8, 1.0]])
        oxygen.append([2, [1.2, 1.0]])
    return {"H": hydrogen, "O": oxygen}


def basis_for_system(system: System) -> str | dict[str, list[list[object]]]:
    if system.basis_definition == "bauschlicher-1986-dz":
        return bauschlicher_1986_basis()
    if system.basis_definition == "bauschlicher-1986-dzp":
        return bauschlicher_1986_basis(polarized=True)
    if system.basis_definition is not None:
        raise ValueError(f"unknown basis definition {system.basis_definition}")
    return system.basis


SYSTEMS = {
    "h2-sto3g": System(
        slug="h2-sto3g",
        name="H2",
        # Cartesian coordinates in Angstrom: R(H-H) = 1.4 Angstrom.
        atom="H 0 0 -0.7; H 0 0 0.7",
        geometry_parameters=(("R(H-H)", 1.4, "angstrom"),),
    ),
    "h2-equilibrium-sto3g": System(
        slug="h2-equilibrium-sto3g",
        name="H2 equilibrium",
        # Cartesian coordinates in Angstrom: R(H-H) = 0.7414 Angstrom.
        atom="H 0 0 -0.3707; H 0 0 0.3707",
        geometry_parameters=(("R(H-H)", 0.7414, "angstrom"),),
    ),
    "h4-sto3g": System(
        slug="h4-sto3g",
        name="linear H4",
        # Cartesian coordinates in Angstrom: adjacent R(H-H) = 1.0 Angstrom.
        atom="H 0 0 -1.5; H 0 0 -0.5; H 0 0 0.5; H 0 0 1.5",
        geometry_parameters=(("adjacent R(H-H)", 1.0, "angstrom"),),
    ),
    "h2o-sto3g": System(
        slug="h2o-sto3g",
        name="H2O",
        # Cartesian Angstrom: R(O-H) = 0.967, angle(H-O-H) = 107.6 degree.
        atom="O 0 0 0; H 0.967 0 0; H -0.2923916843556798 0.9217353757557798 0",
        geometry_parameters=(
            ("R(O-H)", 0.967, "angstrom"),
            ("angle(H-O-H)", 107.6, "degree"),
        ),
    ),
    "h2o-631g-fc": System(
        slug="h2o-631g-fc",
        name="H2O frozen core",
        # Same equilibrium water geometry as h2o-sto3g; Cartesian Angstrom.
        atom="O 0 0 0; H 0.967 0 0; H -0.2923916843556798 0.9217353757557798 0",
        basis="6-31g",
        geometry_parameters=(
            ("R(O-H)", 0.967, "angstrom"),
            ("angle(H-O-H)", 107.6, "degree"),
        ),
        frozen_orbitals=(0,),
    ),
    "h2o-631g-fc-r1p5": System(
        slug="h2o-631g-fc-r1p5",
        name="H2O frozen core at 1.5 times the equilibrium bond length",
        # Both O-H vectors are scaled by 1.5; the 107.6 degree angle is fixed.
        atom=(
            "O 0 0 0; H 1.4505 0 0; "
            "H -0.43858752653351970 1.38260306363366970 0"
        ),
        basis="6-31g",
        geometry_parameters=(
            ("R(O-H)", 1.4505, "angstrom"),
            ("angle(H-O-H)", 107.6, "degree"),
            ("R/R(e)", 1.5, "dimensionless"),
        ),
        frozen_orbitals=(0,),
    ),
    "h2o-631g-fc-r2p0": System(
        slug="h2o-631g-fc-r2p0",
        name="H2O frozen core at 2.0 times the equilibrium bond length",
        # Both O-H vectors are scaled by 2.0; the 107.6 degree angle is fixed.
        atom=(
            "O 0 0 0; H 1.9340 0 0; "
            "H -0.58478336871135960 1.84347075151155960 0"
        ),
        basis="6-31g",
        geometry_parameters=(
            ("R(O-H)", 1.934, "angstrom"),
            ("angle(H-O-H)", 107.6, "degree"),
            ("R/R(e)", 2.0, "dimensionless"),
        ),
        frozen_orbitals=(0,),
    ),
    "h2o-dz-ae": System(
        slug="h2o-dz-ae",
        name="H2O Bauschlicher DZ all-electron",
        # Exact Table II Cartesian coordinates in Bohr.
        atom="O 0 0 0; H 1.494187 0 1.156923; H -1.494187 0 1.156923",
        basis="Bauschlicher 1986 DZ",
        basis_definition="bauschlicher-1986-dz",
        basis_provenance=(
            "Bauschlicher and Taylor, J. Chem. Phys. 85, 2779 (1986), "
            "Table I; DOI 10.1063/1.451034"
        ),
        coordinate_unit="Bohr",
        geometry_parameters=(
            ("R(O-H)", 1.889726334392893, "bohr"),
            ("angle(H-O-H)", 104.50000893084858, "degree"),
        ),
        symmetry=True,
    ),
    "h2o-dzp-fc": System(
        slug="h2o-dzp-fc",
        name="H2O Bauschlicher DZP frozen core",
        # Exact Table II Cartesian coordinates in Bohr.
        atom="O 0 0 0; H 1.494187 0 1.156923; H -1.494187 0 1.156923",
        basis="Bauschlicher 1986 DZP",
        basis_definition="bauschlicher-1986-dzp",
        basis_provenance=(
            "Bauschlicher and Taylor, J. Chem. Phys. 85, 2779 (1986), "
            "Table I; oxygen d exponent 1.2 and hydrogen p exponent 0.8; "
            "DOI 10.1063/1.451034"
        ),
        coordinate_unit="Bohr",
        geometry_parameters=(
            ("R(O-H)", 1.889726334392893, "bohr"),
            ("angle(H-O-H)", 104.50000893084858, "degree"),
        ),
        frozen_orbitals=(0,),
        symmetry=True,
        # The C2v block has 28,233,466 determinants. Do not launch a
        # full-space PySCF FCI implicitly while regenerating fixtures.
        compute_fci=False,
        published_fci_energy=-76.256624,
    ),
    "h2o-ccpvdz-ae": System(
        slug="h2o-ccpvdz-ae",
        name="H2O/cc-pVDZ all-electron",
        atom="O 0 0 0; H 0.967 0 0; H -0.2923916843556798 0.9217353757557798 0",
        basis="cc-pvdz",
        geometry_parameters=(
            ("R(O-H)", 0.967, "angstrom"),
            ("angle(H-O-H)", 107.6, "degree"),
        ),
        benchmark_only=True,
    ),
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def generate(system: System, fixtures_root: Path) -> dict[str, object]:
    output_dir = fixtures_root / system.slug
    output_dir.mkdir(parents=True, exist_ok=True)
    dump_path = output_dir / "FCIDUMP"

    mol = gto.M(
        atom=system.atom,
        basis=basis_for_system(system),
        charge=system.charge,
        spin=system.spin,
        unit=system.coordinate_unit,
        symmetry=system.symmetry,
        verbose=0,
    )
    mf = scf.RHF(mol)
    mf.conv_tol = 1e-12
    hf_energy = float(mf.kernel())
    if not mf.converged:
        raise RuntimeError(f"RHF did not converge for {system.slug}")

    nmo = mf.mo_coeff.shape[1]
    if system.benchmark_only:
        nalpha = (mol.nelectron + system.spin) // 2
        nbeta = mol.nelectron - nalpha
        alpha_strings = math.comb(nmo, nalpha)
        beta_strings = math.comb(nmo, nbeta)
        reference: dict[str, object] = {
            "schema_version": 1,
            "system": system.name,
            "slug": system.slug,
            "geometry_angstrom": system.atom,
            "coordinate_unit": system.coordinate_unit.lower(),
            "geometry_parameters": [
                {"name": name, "value": value, "unit": unit}
                for name, value, unit in system.geometry_parameters
            ],
            "basis": system.basis,
            "charge": system.charge,
            "spin": system.spin,
            "all_electron": True,
            "point_group_symmetry": False,
            "number_of_atomic_orbitals": int(mol.nao_nr()),
            "number_of_molecular_orbitals": int(nmo),
            "number_of_electrons": int(mol.nelectron),
            "nalpha": nalpha,
            "nbeta": nbeta,
            "alpha_strings": alpha_strings,
            "beta_strings": beta_strings,
            "determinants": alpha_strings * beta_strings,
            "nuclear_repulsion_energy": float(mol.energy_nuc()),
            "hf_converged": bool(mf.converged),
            "hf_energy": hf_energy,
            "full_fci_executed": False,
            "pyscf_version": pyscf.__version__,
            "python_version": platform.python_version(),
            "generator": "scripts/oracle/generate.py",
            "energy_unit": "hartree",
        }
        reference_path = output_dir / "reference.json"
        reference_path.write_text(
            json.dumps(reference, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        return reference

    mo_h1 = mf.mo_coeff.T @ mf.get_hcore() @ mf.mo_coeff
    mo_eri = ao2mo.restore(1, ao2mo.kernel(mol, mf.mo_coeff), nmo)
    frozen = tuple(system.frozen_orbitals)
    active = tuple(index for index in range(nmo) if index not in frozen)
    if frozen:
        ecore = float(mol.energy_nuc())
        for i in frozen:
            ecore += 2.0 * mo_h1[i, i]
            for j in frozen:
                ecore += 2.0 * mo_eri[i, i, j, j] - mo_eri[i, j, j, i]
        active_h1 = mo_h1[numpy.ix_(active, active)].copy()
        for p_new, p in enumerate(active):
            for q_new, q in enumerate(active):
                for i in frozen:
                    active_h1[p_new, q_new] += (
                        2.0 * mo_eri[p, q, i, i] - mo_eri[p, i, i, q]
                    )
        active_eri = mo_eri[numpy.ix_(active, active, active, active)]
        active_nelec = mol.nelectron - 2 * len(frozen)
        fcidump.from_integrals(
            str(dump_path),
            active_h1,
            active_eri,
            len(active),
            active_nelec,
            nuc=ecore,
            ms=system.spin,
            orbsym=(
                fcidump._convert_orbsym(
                    mol,
                    mf.get_orbsym()[list(active)],
                    molpro_orbsym=True,
                )
                if system.symmetry
                else None
            ),
            tol=1e-15,
            float_format=" %.16g",
        )
        if system.compute_fci:
            cisolver = fci.direct_spin1.FCI()
            cisolver.conv_tol = 1e-12
            fci_energy, _ = cisolver.kernel(
                active_h1,
                active_eri,
                len(active),
                active_nelec,
                ecore=ecore,
            )
            fci_converged = bool(getattr(cisolver, "converged", True))
        else:
            fci_energy = None
            fci_converged = False
    else:
        fcidump.from_scf(
            mf,
            str(dump_path),
            tol=1e-15,
            float_format=" %.16g",
            molpro_orbsym=system.symmetry,
        )
        if system.compute_fci:
            cisolver = fci.FCI(mf)
            cisolver.conv_tol = 1e-12
            fci_energy, _ = cisolver.kernel()
            fci_converged = bool(getattr(cisolver, "converged", True))
        else:
            fci_energy = None
            fci_converged = False

    ccsd = cc.CCSD(mf, frozen=list(frozen) or None)
    ccsd.conv_tol = 1e-12
    # Stretched bonds need more than PySCF's default 50 CCSD iterations.
    ccsd.max_cycle = 200
    correlation_energy, _, _ = ccsd.kernel()
    ccsd_total_energy = hf_energy + float(correlation_energy)
    mp2 = mp.MP2(mf, frozen=list(frozen) or None)
    mp2_correlation_energy, _ = mp2.kernel()
    mp2_total_energy = hf_energy + float(mp2_correlation_energy)

    reference: dict[str, object] = {
        "schema_version": 1,
        "system": system.name,
        "slug": system.slug,
        "geometry": system.atom,
        "coordinate_unit": system.coordinate_unit.lower(),
        "geometry_parameters": [
            {"name": name, "value": value, "unit": unit}
            for name, value, unit in system.geometry_parameters
        ],
        "basis": system.basis,
        "basis_definition": system.basis_definition,
        "basis_provenance": system.basis_provenance,
        "charge": system.charge,
        "spin": system.spin,
        "point_group": mol.groupname,
        "symmetry_enabled": system.symmetry,
        "fcidump_orbsym_convention": "molpro-1-based" if system.symmetry else "all-1",
        "frozen_orbitals": list(frozen),
        "nuclear_repulsion_energy": float(mol.energy_nuc()),
        "number_of_atomic_orbitals": int(mol.nao_nr()),
        "number_of_molecular_orbitals": int(mf.mo_coeff.shape[1]),
        "number_of_active_molecular_orbitals": len(active),
        "number_of_electrons": int(mol.nelectron),
        "number_of_active_electrons": int(mol.nelectron - 2 * len(frozen)),
        "active_orbital_energies": [float(mf.mo_energy[index]) for index in active],
        "pyscf_version": pyscf.__version__,
        "python_version": platform.python_version(),
        "hf_converged": bool(mf.converged),
        "fci_converged": fci_converged,
        "ccsd_converged": bool(ccsd.converged),
        "hf_energy": hf_energy,
        "ccsd_correlation_energy": float(correlation_energy),
        "ccsd_total_energy": ccsd_total_energy,
        "mp2_correlation_energy": float(mp2_correlation_energy),
        "mp2_total_energy": mp2_total_energy,
        "fcidump_sha256": sha256(dump_path),
        "generator": "scripts/oracle/generate.py",
        "energy_unit": "hartree",
        "fci_status": "computed" if system.compute_fci else "skipped-size-guard",
        "published_fci_energy": system.published_fci_energy,
    }
    if fci_energy is not None:
        reference["fci_energy"] = float(fci_energy)
    reference[f"geometry_{system.coordinate_unit.lower()}"] = system.atom
    reference_path = output_dir / (
        "reference.json" if system.compute_fci else "generation_metadata.json"
    )
    reference_path.write_text(
        json.dumps(reference, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    ao_reference = {
        "schema_version": 1,
        "system": system.name,
        "basis": system.basis,
        "coordinate_unit": system.coordinate_unit.lower(),
        "energy_unit": "hartree",
        "overlap_unit": "dimensionless",
        "nao": int(mol.nao_nr()),
        "nelec": int(mol.nelectron),
        "nuclear_repulsion_energy": float(mol.energy_nuc()),
        "overlap": [float(value) for value in mol.intor("int1e_ovlp").ravel()],
        "hcore": [float(value) for value in mf.get_hcore().ravel()],
        "eri_ao": [float(value) for value in mol.intor("int2e").ravel()],
        "rhf_total_energy": hf_energy,
        "fci_energy": float(fci_energy) if fci_energy is not None else None,
        "orbital_energies": [float(value) for value in mf.mo_energy],
        "mo_coefficients": [float(value) for value in mf.mo_coeff.ravel()],
        "h1_mo": [float(value) for value in mo_h1.ravel()],
        "eri_mo": [float(value) for value in mo_eri.ravel()],
        "pyscf_version": pyscf.__version__,
    }
    (output_dir / "ao_reference.json").write_text(
        json.dumps(ao_reference, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return reference


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "systems",
        nargs="*",
        choices=sorted(SYSTEMS),
    )
    parser.add_argument(
        "--fixtures-root",
        type=Path,
        default=Path(__file__).resolve().parents[2] / "fixtures",
    )
    args = parser.parse_args()

    selected_systems = args.systems or sorted(SYSTEMS)
    for slug in selected_systems:
        reference = generate(SYSTEMS[slug], args.fixtures_root)
        if SYSTEMS[slug].benchmark_only:
            print(
                f"{slug}: HF={reference['hf_energy']:.12f} "
                f"determinants={reference['determinants']} "
                "FCI=not executed"
            )
        else:
            fci_display = (
                f"{reference['fci_energy']:.12f}"
                if "fci_energy" in reference
                else "skipped-size-guard"
            )
            print(
                f"{slug}: HF={reference['hf_energy']:.12f} "
                f"FCI={fci_display} "
                f"CCSD={reference['ccsd_total_energy']:.12f}"
            )
    return 0


if __name__ == "__main__":
    sys.exit(main())
