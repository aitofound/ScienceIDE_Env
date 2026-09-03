use std::fs;
use std::path::Path;
use std::process::Command;

use ed_workbench_rs::davidson::{DavidsonConfig, lowest_eigenpairs};
use ed_workbench_rs::direct_fci::DirectFciOperator;
use ed_workbench_rs::fcidump::Fcidump;
use ed_workbench_rs::hamiltonian::build_dense_hamiltonian;
use ed_workbench_rs::problem::ElectronicProblem;
use nalgebra::linalg::SymmetricEigen;

fn h4_dump() -> Fcidump {
    let path = Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h4-sto3g/FCIDUMP");
    Fcidump::parse(&fs::read_to_string(path).unwrap()).unwrap()
}

#[test]
fn h4_three_root_davidson_matches_dense_diagonalization() {
    let dump = h4_dump();
    let operator = DirectFciOperator::new(ElectronicProblem::from_fcidump(&dump).unwrap()).unwrap();
    let dense = build_dense_hamiltonian(&dump, operator.basis());
    let exact = SymmetricEigen::new(dense);
    let mut expected = exact.eigenvalues.as_slice().to_vec();
    expected.sort_by(f64::total_cmp);

    let results = lowest_eigenpairs(
        &operator,
        3,
        &DavidsonConfig {
            residual_tolerance: 1e-10,
            energy_tolerance: 1e-12,
            max_iterations: 100,
            max_subspace: 12,
        },
    )
    .unwrap();
    let committed: serde_json::Value = serde_json::from_slice(
        &fs::read(
            Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h4-sto3g/multiroot_results.json"),
        )
        .unwrap(),
    )
    .unwrap();
    assert_eq!(committed["calculation_commit"].as_str().unwrap().len(), 40);
    assert_eq!(committed["energy_unit"], "hartree");
    for (root, (result, expected)) in results.iter().zip(expected).enumerate() {
        assert!(result.converged, "residual {}", result.residual_norm);
        assert!(
            (result.energy - expected).abs() < 1e-10,
            "Davidson {}, dense {}",
            result.energy,
            expected
        );
        assert!(
            (result.energy - committed["results"][root]["energy"].as_f64().unwrap()).abs() < 1e-12
        );
    }
    for left in 0..results.len() {
        for right in 0..left {
            let overlap: f64 = results[left]
                .eigenvector
                .iter()
                .zip(&results[right].eigenvector)
                .map(|(left, right)| left * right)
                .sum();
            assert!(overlap.abs() < 1e-10);
        }
    }
}

#[test]
fn davidson_roots_cli_reports_excitation_energies() {
    let path = Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h4-sto3g/FCIDUMP");
    let output = Command::new(env!("CARGO_BIN_EXE_ed_workbench_rs"))
        .arg("davidson-roots")
        .arg(path)
        .arg("--roots")
        .arg("3")
        .arg("--residual-tolerance")
        .arg("1e-10")
        .arg("--max-subspace")
        .arg("12")
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{}",
        String::from_utf8_lossy(&output.stderr)
    );
    let stdout = String::from_utf8_lossy(&output.stdout);
    assert!(stdout.contains(
        "root\tenergy_hartree\texcitation_energy_hartree\titerations\tresidual\tconverged"
    ));
    assert!(stdout.lines().any(|line| line.starts_with("0\t")));
    assert!(stdout.lines().any(|line| line.starts_with("1\t")));
    assert!(stdout.lines().any(|line| line.starts_with("2\t")));
}
