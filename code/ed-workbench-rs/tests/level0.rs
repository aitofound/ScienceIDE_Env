use std::fs;
use std::path::Path;
use std::process::Command;

use ed_workbench_rs::dense_fci::ground_state_energy;
use ed_workbench_rs::fcidump::Fcidump;
use ed_workbench_rs::reference::Reference;

fn verify_fixture(slug: &str) {
    let root = Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("fixtures")
        .join(slug);
    let dump = Fcidump::parse(&fs::read_to_string(root.join("FCIDUMP")).unwrap()).unwrap();
    let reference = Reference::load(&root.join("reference.json")).unwrap();
    let energy = ground_state_energy(&dump).unwrap();
    assert!(
        (energy - reference.fci_energy).abs() < 1e-10,
        "{slug}: Rust {energy:.15}, PySCF {:.15}",
        reference.fci_energy
    );
}

#[test]
fn h2_matches_pyscf_fci() {
    verify_fixture("h2-sto3g");
}

#[test]
fn equilibrium_h2_matches_pyscf_fci() {
    verify_fixture("h2-equilibrium-sto3g");
}

#[test]
fn h4_matches_pyscf_fci() {
    verify_fixture("h4-sto3g");
}

#[test]
fn verify_cli_passes_for_h2() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h2-sto3g");
    let output = Command::new(env!("CARGO_BIN_EXE_ed_workbench_rs"))
        .arg("verify")
        .arg(root.join("FCIDUMP"))
        .arg(root.join("reference.json"))
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{}",
        String::from_utf8_lossy(&output.stderr)
    );
    assert!(String::from_utf8_lossy(&output.stdout).contains("verification: PASS"));
}

#[test]
fn inspect_cli_reports_basis_size() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h4-sto3g");
    let output = Command::new(env!("CARGO_BIN_EXE_ed_workbench_rs"))
        .arg("inspect")
        .arg(root.join("FCIDUMP"))
        .output()
        .unwrap();
    assert!(output.status.success());
    let stdout = String::from_utf8_lossy(&output.stdout);
    assert!(stdout.contains("NORB: 4"));
    assert!(stdout.contains("determinants: 36"));
}
