use std::fs;
use std::path::Path;
use std::process::Command;

use ed_workbench_rs::published_reference::rounded_published_match;
use serde::Deserialize;

#[derive(Deserialize)]
struct CommittedLevel3Series {
    energy_unit: String,
    calculation_commit: String,
    fci_energy: f64,
    ci_residual_tolerance: f64,
    ci: Vec<CommittedCi>,
    mbpt: Vec<CommittedMbpt>,
}

#[derive(Deserialize)]
struct CommittedCi {
    rank: usize,
    dimension: usize,
    energy: f64,
    method_minus_fci: f64,
    published_difference: f64,
    published_error: f64,
    residual_norm: f64,
    converged: bool,
    published_match: bool,
}

#[derive(Deserialize)]
struct CommittedMbpt {
    order: usize,
    energy: f64,
    method_minus_fci: f64,
    published_difference: f64,
    published_error: f64,
    published_match: bool,
}

#[test]
fn level3_series_cli_reports_ci_and_mbpt_orders() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h4-sto3g");
    let output = Command::new(env!("CARGO_BIN_EXE_ed_workbench_rs"))
        .arg("level3-series")
        .arg(root.join("FCIDUMP"))
        .arg(root.join("reference.json"))
        .arg("--max-ci-rank")
        .arg("4")
        .arg("--max-mbpt-order")
        .arg("4")
        .arg("--ci-residual-tolerance")
        .arg("1e-9")
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{}",
        String::from_utf8_lossy(&output.stderr)
    );
    let stdout = String::from_utf8_lossy(&output.stdout);
    assert!(stdout.contains("CI series"));
    assert!(stdout.contains("MBPT series"));
    assert!(stdout.lines().any(|line| line.starts_with("CI\t4\t")));
    assert!(stdout.lines().any(|line| line.starts_with("MBPT\t4\t")));
    assert!(stdout.contains("CI series converged: true"));
}

#[test]
fn committed_primary_level3_series_matches_every_published_order() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h2o-631g-fc");
    let committed: CommittedLevel3Series =
        serde_json::from_str(&fs::read_to_string(root.join("level3_series_results.json")).unwrap())
            .unwrap();
    assert_eq!(committed.energy_unit, "hartree");
    assert_eq!(committed.calculation_commit.len(), 40);
    assert_eq!(committed.ci.len(), 8);
    assert_eq!(committed.mbpt.len(), 20);

    let mut previous_energy = f64::INFINITY;
    let mut previous_dimension = 0;
    for (index, result) in committed.ci.iter().enumerate() {
        assert_eq!(result.rank, index + 1);
        assert!(result.dimension > previous_dimension);
        assert!(result.energy <= previous_energy + 1e-12);
        assert!(result.converged);
        assert!(result.residual_norm <= committed.ci_residual_tolerance);
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
        previous_energy = result.energy;
        previous_dimension = result.dimension;
    }
    assert!((committed.ci[7].energy - committed.fci_energy).abs() < 1e-8);

    for (index, result) in committed.mbpt.iter().enumerate() {
        assert_eq!(result.order, index + 1);
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
    }
}

#[test]
#[ignore = "live H2O CI(1)-CI(8) and MBPT(1)-MBPT(20) verification takes about three minutes"]
fn live_primary_level3_series_matches_hirata_table2() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR"));
    let fixture = root.join("fixtures/h2o-631g-fc");
    let output = Command::new(root.join("target/release/ed_workbench_rs"))
        .env("RAYON_NUM_THREADS", "10")
        .arg("level3-series")
        .arg(fixture.join("FCIDUMP"))
        .arg(fixture.join("reference.json"))
        .arg("--published-reference")
        .arg(fixture.join("hirata2000-table2.json"))
        .arg("--max-ci-rank")
        .arg("8")
        .arg("--max-mbpt-order")
        .arg("20")
        .arg("--ci-residual-tolerance")
        .arg("1e-7")
        .arg("--max-iterations")
        .arg("100")
        .arg("--max-subspace")
        .arg("24")
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{}",
        String::from_utf8_lossy(&output.stderr)
    );
    assert!(String::from_utf8_lossy(&output.stdout).contains("published verification: PASS"));
}
