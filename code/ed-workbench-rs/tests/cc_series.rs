use std::fs;
use std::path::Path;
use std::process::Command;

use ed_workbench_rs::published_reference::rounded_published_match;
use ed_workbench_rs::reference::Reference;
use serde::Deserialize;

#[derive(Deserialize)]
struct CommittedSeries {
    energy_unit: String,
    fci_energy: f64,
    residual_tolerance: f64,
    calculation_commit: String,
    results: Vec<CommittedRank>,
}

#[derive(Deserialize)]
struct CommittedRank {
    rank: usize,
    amplitudes: usize,
    energy: f64,
    method_minus_fci: f64,
    published_difference: f64,
    published_error: f64,
    residual_norm: f64,
    converged: bool,
    published_match: bool,
}

#[derive(Deserialize)]
struct GeneratedSeries {
    schema_version: u32,
    artifact_kind: String,
    system: String,
    energy_unit: String,
    fci_reference_energy: f64,
    results: Vec<GeneratedRank>,
    published_verification: Option<bool>,
}

#[derive(Deserialize)]
struct GeneratedRank {
    rank: usize,
    energy: f64,
    method_minus_fci: f64,
    iterations: usize,
    residual_norm: f64,
    converged: bool,
    termination: String,
    published_difference: Option<f64>,
}

#[test]
fn cc_series_cli_reports_every_requested_rank() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h4-sto3g");
    let output = Command::new(env!("CARGO_BIN_EXE_ed_workbench_rs"))
        .arg("cc-series")
        .arg(root.join("FCIDUMP"))
        .arg(root.join("reference.json"))
        .arg("--max-rank")
        .arg("2")
        .arg("--residual-tolerance")
        .arg("1e-8")
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{}",
        String::from_utf8_lossy(&output.stderr)
    );
    let stdout = String::from_utf8_lossy(&output.stdout);
    assert!(stdout.contains(
        "rank\tenergy_hartree\tmethod_minus_fci_hartree\titerations\tresidual\telapsed_seconds\tconverged"
    ));
    assert!(stdout.lines().any(|line| line.starts_with("1\t")));
    assert!(stdout.lines().any(|line| line.starts_with("2\t")));
    assert!(stdout.contains("series converged: true"));
}

#[test]
fn committed_primary_cc_series_covers_octuple_excitations() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h2o-631g-fc");
    let committed: CommittedSeries =
        serde_json::from_str(&fs::read_to_string(root.join("cc_series_results.json")).unwrap())
            .unwrap();
    let reference = Reference::load(&root.join("reference.json")).unwrap();
    assert_eq!(committed.energy_unit, "hartree");
    assert_eq!(committed.calculation_commit.len(), 40);
    assert_eq!(committed.fci_energy, reference.fci_energy);
    assert_eq!(committed.results.len(), 8);
    let mut previous_amplitudes = 0;
    for (index, result) in committed.results.iter().enumerate() {
        assert_eq!(result.rank, index + 1);
        assert!(result.amplitudes > previous_amplitudes);
        assert!(result.converged);
        assert!(result.residual_norm <= committed.residual_tolerance);
        assert!(result.published_match);
        assert!(rounded_published_match(
            result.method_minus_fci,
            result.published_difference,
            6
        ));
        assert!((result.energy - committed.fci_energy - result.method_minus_fci).abs() < 1e-12);
        assert!(
            (result.method_minus_fci - result.published_difference - result.published_error).abs()
                < 1e-12
        );
        previous_amplitudes = result.amplitudes;
    }
    assert!((committed.results[1].energy - reference.ccsd_total_energy.unwrap()).abs() < 1e-8);
    assert!((committed.results[7].energy - reference.fci_energy).abs() < 1e-6);
}

#[test]
fn cc_series_json_has_a_stable_numerical_contract() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h4-sto3g");
    let output_path = std::env::temp_dir().join(format!(
        "ed-workbench-cc-series-{}.json",
        std::process::id()
    ));
    let output = Command::new(env!("CARGO_BIN_EXE_ed_workbench_rs"))
        .arg("cc-series")
        .arg(root.join("FCIDUMP"))
        .arg(root.join("reference.json"))
        .arg("--max-rank")
        .arg("2")
        .arg("--residual-tolerance")
        .arg("1e-8")
        .arg("--json-output")
        .arg(&output_path)
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{}",
        String::from_utf8_lossy(&output.stderr)
    );

    let generated: GeneratedSeries =
        serde_json::from_slice(&fs::read(&output_path).unwrap()).unwrap();
    fs::remove_file(output_path).unwrap();
    let reference = Reference::load(&root.join("reference.json")).unwrap();

    assert_eq!(generated.schema_version, 1);
    assert_eq!(generated.artifact_kind, "cc-series");
    assert_eq!(generated.system, reference.system);
    assert_eq!(generated.energy_unit, "hartree");
    assert_eq!(generated.fci_reference_energy, reference.fci_energy);
    assert_eq!(generated.results.len(), 2);
    assert_eq!(generated.published_verification, None);
    for (index, rank) in generated.results.iter().enumerate() {
        assert_eq!(rank.rank, index + 1);
        assert!(rank.energy.is_finite());
        assert!((rank.energy - reference.fci_energy - rank.method_minus_fci).abs() < 1e-12);
        assert!(rank.iterations > 0);
        assert!(rank.residual_norm.is_finite());
        assert!(rank.converged);
        assert_eq!(rank.termination, "converged");
        assert_eq!(rank.published_difference, None);
    }
}

#[test]
#[ignore = "live H2O CC(1)-CC(8) verification takes about three minutes in release mode"]
fn live_primary_cc_series_matches_hirata_table2() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR"));
    let fixture = root.join("fixtures/h2o-631g-fc");
    let output = Command::new(root.join("target/release/ed_workbench_rs"))
        .env("RAYON_NUM_THREADS", "10")
        .arg("cc-series")
        .arg(fixture.join("FCIDUMP"))
        .arg(fixture.join("reference.json"))
        .arg("--published-reference")
        .arg(fixture.join("hirata2000-table2.json"))
        .arg("--max-rank")
        .arg("8")
        .arg("--residual-tolerance")
        .arg("1e-6")
        .arg("--max-iterations")
        .arg("100")
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{}",
        String::from_utf8_lossy(&output.stderr)
    );
    assert!(String::from_utf8_lossy(&output.stdout).contains("published verification: PASS"));
}
