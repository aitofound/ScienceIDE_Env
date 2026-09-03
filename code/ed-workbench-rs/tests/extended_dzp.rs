use std::fs;
use std::path::Path;
use std::process::Command;

use ed_workbench_rs::determinant::DeterminantBasis;
use ed_workbench_rs::fcidump::Fcidump;
use ed_workbench_rs::reference::sha256_hex;
use serde::Deserialize;

#[derive(Deserialize)]
struct DavidsonRecord {
    calculation_commit: String,
    energy_unit: String,
    determinants_in_isym_sector: usize,
    fcidump_sha256: String,
    published_fci_energy: f64,
    published_printed_decimals: u32,
    rust_fci_energy: f64,
    rust_minus_published: f64,
    residual_tolerance: f64,
    residual_norm: f64,
    converged: bool,
    maximum_resident_set_bytes: usize,
    swap_count: usize,
}

#[derive(Deserialize)]
struct SigmaBenchmark {
    calculation_commit: String,
    determinants: usize,
    rayon_threads: usize,
    operator_build_seconds: f64,
    sigma_apply_seconds: f64,
    maximum_resident_set_bytes: usize,
}

fn root() -> std::path::PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h2o-dzp-fc")
}

#[test]
fn committed_dzp_evidence_matches_the_published_anchor() {
    let root = root();
    let bytes = fs::read(root.join("FCIDUMP")).unwrap();
    let dump = Fcidump::parse(std::str::from_utf8(&bytes).unwrap()).unwrap();
    let metadata: serde_json::Value =
        serde_json::from_slice(&fs::read(root.join("generation_metadata.json")).unwrap()).unwrap();
    let published: serde_json::Value =
        serde_json::from_slice(&fs::read(root.join("published-fci-anchor.json")).unwrap()).unwrap();
    let result: DavidsonRecord =
        serde_json::from_slice(&fs::read(root.join("davidson_result.json")).unwrap()).unwrap();
    let benchmark: SigmaBenchmark =
        serde_json::from_slice(&fs::read(root.join("sigma_benchmark.json")).unwrap()).unwrap();

    assert_eq!(dump.norb, 24);
    assert_eq!(dump.nelec, 8);
    assert_eq!(dump.ms2, 0);
    assert_eq!(dump.isym, 1);
    assert_eq!(metadata["frozen_orbitals"], serde_json::json!([0]));
    assert_eq!(metadata["number_of_active_molecular_orbitals"], 24);
    assert_eq!(metadata["number_of_active_electrons"], 8);
    assert_eq!(metadata["fci_status"], "skipped-size-guard");
    assert_eq!(metadata["ccsd_converged"], true);

    let basis =
        DeterminantBasis::with_symmetry(dump.norb, dump.nelec, dump.ms2, &dump.orbsym, dump.isym)
            .unwrap();
    assert_eq!(basis.len(), 28_233_466);
    assert_eq!(sha256_hex(&bytes), result.fcidump_sha256);
    assert_eq!(result.energy_unit, "hartree");
    assert_eq!(result.calculation_commit.len(), 40);
    assert_eq!(result.determinants_in_isym_sector, basis.len());
    assert!(result.converged);
    assert!(result.residual_norm <= result.residual_tolerance);
    assert_eq!(
        result.published_fci_energy,
        published["energy"].as_f64().unwrap()
    );
    assert_eq!(result.published_printed_decimals, 6);
    assert!(
        (result.rust_fci_energy - result.published_fci_energy - result.rust_minus_published).abs()
            < 1e-14
    );
    let scale = 10_f64.powi(result.published_printed_decimals as i32);
    assert_eq!(
        (result.rust_fci_energy * scale).round() as i64,
        (result.published_fci_energy * scale).round() as i64
    );
    assert!(result.maximum_resident_set_bytes < 8 * 1024 * 1024 * 1024);
    assert_eq!(result.swap_count, 0);

    assert_eq!(benchmark.calculation_commit, result.calculation_commit);
    assert_eq!(benchmark.determinants, basis.len());
    assert_eq!(benchmark.rayon_threads, 10);
    assert!(benchmark.operator_build_seconds < 5.0);
    assert!(benchmark.sigma_apply_seconds < 120.0);
    assert!(benchmark.maximum_resident_set_bytes < 4 * 1024 * 1024 * 1024);
}

#[test]
#[ignore = "release-mode H2O/DZP Davidson validation takes about 18 minutes and 7 GB"]
fn live_dzp_davidson_reproduces_the_published_digits() {
    let project = Path::new(env!("CARGO_MANIFEST_DIR"));
    let root = root();
    let output = Command::new(project.join("target/release/ed_workbench_rs"))
        .arg("davidson")
        .arg(root.join("FCIDUMP"))
        .arg("--residual-tolerance")
        .arg("1e-7")
        .arg("--max-iterations")
        .arg("40")
        .arg("--max-subspace")
        .arg("6")
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{}",
        String::from_utf8_lossy(&output.stderr)
    );
    let stdout = String::from_utf8_lossy(&output.stdout);
    assert!(stdout.contains("energy: -76.256624"));
    assert!(stdout.contains("converged: true"));
}
