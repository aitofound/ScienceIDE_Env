#!/usr/bin/env python3
"""Build an independent H2O/cc-pVDZ correlation-energy cross-check.

This script does not run FCI.  It recomputes RHF, MP2, CISD, CCSD, and
CCSD(T) with PySCF for the exact geometry used by the symmetry-adapted Rust
FCI calculation.  The resulting hierarchy is a compact independent check of
the FCIDUMP Hamiltonian and of the scale of the converged FCI energy.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import platform

import pyscf
from pyscf import cc, ci, gto, lib, mp, scf
from pyscf.tools import fcidump


GEOMETRY = (
    "O 0 0 0; H 0.967 0 0; "
    "H -0.2923916843556798 0.9217353757557798 0"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        type=Path,
        help="optional JSON output path; the record is always printed",
    )
    parser.add_argument(
        "--fcidump-output",
        type=Path,
        help="optional symmetry-adapted FCIDUMP output path",
    )
    parser.add_argument("--threads", type=int, default=1)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.threads < 1:
        raise SystemExit("--threads must be positive")
    lib.num_threads(args.threads)

    mol = gto.M(
        atom=GEOMETRY,
        basis="cc-pvdz",
        unit="Angstrom",
        charge=0,
        spin=0,
        symmetry="C2v",
        cart=False,
        verbose=0,
    )

    mf = scf.RHF(mol)
    mf.conv_tol = 1e-12
    mf.conv_tol_grad = 1e-8
    mf.max_cycle = 100
    hf_energy = mf.kernel()
    if args.fcidump_output is not None:
        # The production input was generated from the default PySCF-quality
        # RHF orbitals.  Keep this tolerance explicit because tighter SCF
        # convergence rotates the virtual orbitals slightly and changes the
        # FCIDUMP bytes, although full-CI observables remain invariant.
        input_mf = scf.RHF(mol)
        input_mf.conv_tol = 1e-10
        input_mf.max_cycle = 100
        input_mf.kernel()
        fcidump.from_scf(
            input_mf,
            str(args.fcidump_output),
            tol=1e-15,
            float_format=" %.16g",
            molpro_orbsym=True,
        )

    pt2 = mp.MP2(mf)
    pt2.conv_tol = 1e-12
    mp2_correlation, _ = pt2.kernel()

    cisd = ci.CISD(mf)
    cisd.conv_tol = 1e-10
    cisd.max_cycle = 100
    cisd_correlation, _ = cisd.kernel()

    ccsd = cc.CCSD(mf)
    ccsd.conv_tol = 1e-10
    ccsd.conv_tol_normt = 1e-8
    ccsd.max_cycle = 100
    ccsd_correlation, _, _ = ccsd.kernel()
    triples_correction = ccsd.ccsd_t()

    result = {
        "schema_version": 1,
        "artifact_kind": "independent-ccpvdz-fci-cross-check",
        "generator": "scripts/oracle/validate_ccpvdz_fci.py",
        "python_version": platform.python_version(),
        "pyscf_version": pyscf.__version__,
        "threads": lib.num_threads(),
        "system": "H2O/cc-pVDZ all-electron",
        "geometry_angstrom": GEOMETRY,
        "basis": "cc-pvdz",
        "spherical_basis_functions": True,
        "symmetry": mol.topgroup,
        "nuclear_repulsion_energy": float(mol.energy_nuc()),
        "number_of_atomic_orbitals": mol.nao_nr(),
        "number_of_electrons": mol.nelectron,
        "rhf": {
            "converged": bool(mf.converged),
            "total_energy_hartree": float(hf_energy),
        },
        "mp2": {
            "correlation_energy_hartree": float(mp2_correlation),
            "total_energy_hartree": float(hf_energy + mp2_correlation),
        },
        "cisd": {
            "converged": bool(cisd.converged),
            "correlation_energy_hartree": float(cisd_correlation),
            "total_energy_hartree": float(hf_energy + cisd_correlation),
        },
        "ccsd": {
            "converged": bool(ccsd.converged),
            "correlation_energy_hartree": float(ccsd_correlation),
            "total_energy_hartree": float(ccsd.e_tot),
        },
        "ccsd_t": {
            "triples_correction_hartree": float(triples_correction),
            "total_energy_hartree": float(ccsd.e_tot + triples_correction),
        },
    }
    text = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output is not None:
        args.output.write_text(text, encoding="utf-8")
    print(text, end="")


if __name__ == "__main__":
    main()
