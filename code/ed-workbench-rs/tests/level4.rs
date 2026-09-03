use std::fs;
use std::path::Path;
use std::process::Command;

use ed_workbench_rs::ao2mo::transform_to_mo;
use ed_workbench_rs::davidson::{DavidsonConfig, lowest_eigenpair};
use ed_workbench_rs::direct_fci::DirectFciOperator;
use ed_workbench_rs::libcint_frontend::compute_ao_integrals;
use ed_workbench_rs::molecule::{CoordinateUnit, Molecule};
use ed_workbench_rs::operator::LinearOperator;
use ed_workbench_rs::rhf::{RhfConfig, solve_rhf};
use serde::Deserialize;

#[derive(Deserialize)]
struct AoReference {
    nao: usize,
    coordinate_unit: String,
    energy_unit: String,
    overlap_unit: String,
    nuclear_repulsion_energy: f64,
    overlap: Vec<f64>,
    hcore: Vec<f64>,
    eri_ao: Vec<f64>,
    rhf_total_energy: f64,
    fci_energy: f64,
    orbital_energies: Vec<f64>,
    mo_coefficients: Vec<f64>,
    h1_mo: Vec<f64>,
    eri_mo: Vec<f64>,
}

fn reference(slug: &str) -> AoReference {
    let path = Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("fixtures")
        .join(slug)
        .join("ao_reference.json");
    serde_json::from_str(&fs::read_to_string(path).unwrap()).unwrap()
}

fn max_error(left: &[f64], right: &[f64]) -> f64 {
    left.iter()
        .zip(right)
        .map(|(a, b)| (a - b).abs())
        .fold(0.0, f64::max)
}

fn align_orbital_signs(rhf: &mut ed_workbench_rs::rhf::RhfResult, expected: &AoReference) {
    for orbital in 0..expected.nao {
        let overlap: f64 = (0..expected.nao)
            .map(|ao| {
                rhf.coefficients[(ao, orbital)]
                    * expected.mo_coefficients[ao * expected.nao + orbital]
            })
            .sum();
        if overlap < 0.0 {
            for ao in 0..expected.nao {
                rhf.coefficients[(ao, orbital)] *= -1.0;
            }
        }
    }
}

fn direct_fci(problem: ed_workbench_rs::problem::ElectronicProblem) -> f64 {
    let operator = DirectFciOperator::new(problem).unwrap();
    let mut initial = vec![0.0; operator.dimension()];
    let index = operator
        .diagonal()
        .iter()
        .enumerate()
        .min_by(|(_, a), (_, b)| a.total_cmp(b))
        .unwrap()
        .0;
    initial[index] = 1.0;
    let result = lowest_eigenpair(&operator, &initial, &DavidsonConfig::default()).unwrap();
    assert!(
        result.converged,
        "FCI residual {} after {} iterations",
        result.residual_norm, result.iterations
    );
    result.energy
}

#[test]
fn h2_libcint_rhf_ao2mo_and_fci_match_pyscf() {
    let expected = reference("h2-sto3g");
    assert_eq!(expected.coordinate_unit, "angstrom");
    assert_eq!(expected.energy_unit, "hartree");
    assert_eq!(expected.overlap_unit, "dimensionless");
    let integrals = compute_ao_integrals(&Molecule::h2_sto3g()).unwrap();
    assert_eq!(integrals.coordinate_unit, CoordinateUnit::Angstrom);
    assert_eq!(integrals.nao, expected.nao);
    assert!(
        (integrals.nuclear_repulsion - expected.nuclear_repulsion_energy).abs() < 1e-9,
        "nuclear repulsion: Rust {}, PySCF {}",
        integrals.nuclear_repulsion,
        expected.nuclear_repulsion_energy
    );
    assert!(
        max_error(&integrals.overlap, &expected.overlap) < 1e-8,
        "overlap error {}",
        max_error(&integrals.overlap, &expected.overlap)
    );
    assert!(
        max_error(&integrals.hcore, &expected.hcore) < 1e-8,
        "hcore error {}",
        max_error(&integrals.hcore, &expected.hcore)
    );
    assert!(
        max_error(&integrals.eri, &expected.eri_ao) < 1e-8,
        "ERI error {}",
        max_error(&integrals.eri, &expected.eri_ao)
    );

    let mut rhf = solve_rhf(&integrals, &RhfConfig::default()).unwrap();
    assert!(rhf.converged);
    assert!(
        (rhf.total_energy - expected.rhf_total_energy).abs() < 1e-8,
        "RHF energy: Rust {}, PySCF {}",
        rhf.total_energy,
        expected.rhf_total_energy
    );
    assert!(max_error(&rhf.orbital_energies, &expected.orbital_energies) < 1e-8);

    align_orbital_signs(&mut rhf, &expected);
    let problem = transform_to_mo(&integrals, &rhf).unwrap();
    let h1_error = max_error(problem.h1_data(), &expected.h1_mo);
    let mo_eri_error = max_error(problem.eri_data(), &expected.eri_mo);
    assert!(h1_error < 1e-8);
    assert!(mo_eri_error < 1e-8);
    let fci_energy = direct_fci(problem);
    assert!((fci_energy - expected.fci_energy).abs() < 1e-8);
    println!(
        "H2 max errors: S={:.3e}, hAO={:.3e}, eriAO={:.3e}, RHF={:.3e}, eps={:.3e}, hMO={:.3e}, eriMO={:.3e}, FCI={:.3e}",
        max_error(&integrals.overlap, &expected.overlap),
        max_error(&integrals.hcore, &expected.hcore),
        max_error(&integrals.eri, &expected.eri_ao),
        (rhf.total_energy - expected.rhf_total_energy).abs(),
        max_error(&rhf.orbital_energies, &expected.orbital_energies),
        h1_error,
        mo_eri_error,
        (fci_energy - expected.fci_energy).abs()
    );
}

#[test]
fn h2o_libcint_rhf_ao2mo_and_fci_match_pyscf() {
    let expected = reference("h2o-sto3g");
    assert_eq!(expected.coordinate_unit, "angstrom");
    assert_eq!(expected.energy_unit, "hartree");
    assert_eq!(expected.overlap_unit, "dimensionless");
    let integrals = compute_ao_integrals(&Molecule::h2o_sto3g()).unwrap();
    assert_eq!(integrals.coordinate_unit, CoordinateUnit::Angstrom);
    assert!(
        max_error(&integrals.overlap, &expected.overlap) < 1e-8,
        "overlap error {}",
        max_error(&integrals.overlap, &expected.overlap)
    );
    assert!(
        max_error(&integrals.hcore, &expected.hcore) < 1e-8,
        "hcore error {}",
        max_error(&integrals.hcore, &expected.hcore)
    );
    assert!(
        max_error(&integrals.eri, &expected.eri_ao) < 1e-8,
        "ERI error {}",
        max_error(&integrals.eri, &expected.eri_ao)
    );
    let mut rhf = solve_rhf(&integrals, &RhfConfig::default()).unwrap();
    assert!(rhf.converged, "density RMS {}", rhf.density_rms);
    assert!(
        (rhf.total_energy - expected.rhf_total_energy).abs() < 1e-8,
        "RHF energy: Rust {}, PySCF {}",
        rhf.total_energy,
        expected.rhf_total_energy
    );
    assert!(
        max_error(&rhf.orbital_energies, &expected.orbital_energies) < 1e-7,
        "orbital energy error {}",
        max_error(&rhf.orbital_energies, &expected.orbital_energies)
    );
    align_orbital_signs(&mut rhf, &expected);
    let problem = transform_to_mo(&integrals, &rhf).unwrap();
    let h1_error = max_error(problem.h1_data(), &expected.h1_mo);
    let mo_eri_error = max_error(problem.eri_data(), &expected.eri_mo);
    assert!(h1_error < 1e-7, "MO h1 error {}", h1_error);
    assert!(mo_eri_error < 1e-7, "MO ERI error {}", mo_eri_error);
    let fci_energy = direct_fci(problem);
    assert!(
        (fci_energy - expected.fci_energy).abs() < 1e-7,
        "FCI energy: Rust {}, PySCF {}",
        fci_energy,
        expected.fci_energy
    );
    println!(
        "H2O max errors: S={:.3e}, hAO={:.3e}, eriAO={:.3e}, RHF={:.3e}, eps={:.3e}, hMO={:.3e}, eriMO={:.3e}, FCI={:.3e}",
        max_error(&integrals.overlap, &expected.overlap),
        max_error(&integrals.hcore, &expected.hcore),
        max_error(&integrals.eri, &expected.eri_ao),
        (rhf.total_energy - expected.rhf_total_energy).abs(),
        max_error(&rhf.orbital_energies, &expected.orbital_energies),
        h1_error,
        mo_eri_error,
        (fci_energy - expected.fci_energy).abs()
    );
}

#[test]
fn direct_integrals_cli_runs_without_python() {
    let output = Command::new(env!("CARGO_BIN_EXE_ed_workbench_rs"))
        .args(["direct-integrals-fci", "h2-sto3g"])
        .env_remove("PYTHONPATH")
        .env_remove("VIRTUAL_ENV")
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{}",
        String::from_utf8_lossy(&output.stderr)
    );
    let stdout = String::from_utf8_lossy(&output.stdout);
    assert!(stdout.contains("FCI total energy: -1.015468249"));
    assert!(stdout.contains("coordinate unit: angstrom"));
    assert!(stdout.contains("energy unit: hartree"));
    assert!(stdout.contains("converged: true"));
}
