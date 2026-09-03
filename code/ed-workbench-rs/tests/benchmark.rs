use std::fs;
use std::process::Command;

use ed_workbench_rs::benchmark::BoundedBenchmarkResult;
use serde::Deserialize;

#[derive(Deserialize)]
struct BenchmarkSummary {
    schema_version: u32,
    artifact_kind: String,
    measured_commit: String,
    determinants: u64,
    sources_per_run: usize,
    raw_contributions_per_run: u64,
    runs: Vec<BenchmarkRun>,
    aggregate: BenchmarkAggregate,
}

#[derive(Deserialize)]
struct BenchmarkRun {
    index: usize,
    wall_seconds: f64,
    peak_rss_bytes: u64,
    ao_integrals_seconds: f64,
    rhf_seconds: f64,
    ao_to_mo_seconds: f64,
    link_tables_seconds: f64,
    sparse_columns_seconds: f64,
    contributions_per_second: f64,
    checksum: f64,
}

#[derive(Deserialize)]
struct BenchmarkAggregate {
    median_wall_seconds: f64,
    median_peak_rss_bytes: u64,
    maximum_peak_rss_bytes: u64,
    median_ao_integrals_seconds: f64,
    median_rhf_seconds: f64,
    median_ao_to_mo_seconds: f64,
    median_link_tables_seconds: f64,
    median_sparse_columns_seconds: f64,
    median_contributions_per_second: f64,
}

fn median_f64(values: impl Iterator<Item = f64>) -> f64 {
    let mut values: Vec<_> = values.collect();
    values.sort_by(f64::total_cmp);
    values[values.len() / 2]
}

fn median_u64(values: impl Iterator<Item = u64>) -> u64 {
    let mut values: Vec<_> = values.collect();
    values.sort_unstable();
    values[values.len() / 2]
}

fn assert_close(actual: f64, expected: f64) {
    assert!(
        (actual - expected).abs() < 1e-12,
        "actual={actual:.15e}, expected={expected:.15e}"
    );
}

#[test]
fn five_process_summary_recomputes_exactly() {
    let fixture_root = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("fixtures")
        .join("h2o-ccpvdz-ae");
    let summary: BenchmarkSummary =
        serde_json::from_slice(&fs::read(fixture_root.join("benchmark-m4-summary.json")).unwrap())
            .unwrap();
    let full: BoundedBenchmarkResult =
        serde_json::from_slice(&fs::read(fixture_root.join("benchmark-m4.json")).unwrap()).unwrap();

    assert_eq!(summary.schema_version, 1);
    assert_eq!(summary.artifact_kind, "h2o-ccpvdz-five-process-summary");
    assert_eq!(
        summary.measured_commit,
        "025a6dd27836f2e9011ef63ee35630a667bdd786"
    );
    assert_eq!(summary.determinants, full.space.determinants);
    assert_eq!(summary.sources_per_run, full.sparse_kernel.sources);
    assert_eq!(
        summary.raw_contributions_per_run,
        full.sparse_kernel.raw_contributions
    );
    assert_eq!(summary.runs.len(), 5);
    assert_eq!(
        summary.runs.iter().map(|run| run.index).collect::<Vec<_>>(),
        vec![1, 2, 3, 4, 5]
    );
    for run in &summary.runs {
        assert_close(run.checksum, full.sparse_kernel.checksum);
    }

    assert_close(
        summary.aggregate.median_wall_seconds,
        median_f64(summary.runs.iter().map(|run| run.wall_seconds)),
    );
    assert_eq!(
        summary.aggregate.median_peak_rss_bytes,
        median_u64(summary.runs.iter().map(|run| run.peak_rss_bytes))
    );
    assert_eq!(
        summary.aggregate.maximum_peak_rss_bytes,
        summary
            .runs
            .iter()
            .map(|run| run.peak_rss_bytes)
            .max()
            .unwrap()
    );
    assert_close(
        summary.aggregate.median_ao_integrals_seconds,
        median_f64(summary.runs.iter().map(|run| run.ao_integrals_seconds)),
    );
    assert_close(
        summary.aggregate.median_rhf_seconds,
        median_f64(summary.runs.iter().map(|run| run.rhf_seconds)),
    );
    assert_close(
        summary.aggregate.median_ao_to_mo_seconds,
        median_f64(summary.runs.iter().map(|run| run.ao_to_mo_seconds)),
    );
    assert_close(
        summary.aggregate.median_link_tables_seconds,
        median_f64(summary.runs.iter().map(|run| run.link_tables_seconds)),
    );
    assert_close(
        summary.aggregate.median_sparse_columns_seconds,
        median_f64(summary.runs.iter().map(|run| run.sparse_columns_seconds)),
    );
    assert_close(
        summary.aggregate.median_contributions_per_second,
        median_f64(summary.runs.iter().map(|run| run.contributions_per_second)),
    );
}

#[test]
fn memory_budget_cli_is_precise_and_backward_compatible() {
    let help = Command::new(env!("CARGO_BIN_EXE_ed_workbench_rs"))
        .args(["benchmark", "--help"])
        .output()
        .unwrap();
    assert!(help.status.success());
    let help = String::from_utf8_lossy(&help.stdout);
    assert!(help.contains("--memory-budget-gib"), "{help}");
    assert!(help.contains("--max-memory-gib"), "{help}");
    assert!(
        help.contains("not an operating-system hard memory limit"),
        "{help}"
    );

    for option in ["--memory-budget-gib", "--max-memory-gib"] {
        let output = Command::new(env!("CARGO_BIN_EXE_ed_workbench_rs"))
            .args(["benchmark", "h2o-cc-pvdz", "--sources", "1", option, "0.5"])
            .output()
            .unwrap();
        assert!(!output.status.success(), "{option} unexpectedly succeeded");
        let stderr = String::from_utf8_lossy(&output.stderr);
        assert!(stderr.contains("exceeds budget"), "{option}: {stderr}");
    }
}

#[test]
#[ignore = "live cc-pVDZ integral/link benchmark; run explicitly in release mode"]
fn live_cc_pvdz_benchmark_is_bounded_and_matches_pyscf_rhf() {
    let output_path = std::env::temp_dir().join(format!(
        "ed-workbench-h2o-ccpvdz-{}.json",
        std::process::id()
    ));
    let output = Command::new(env!("CARGO_BIN_EXE_ed_workbench_rs"))
        .args([
            "benchmark",
            "h2o-cc-pvdz",
            "--sources",
            "1",
            "--memory-budget-gib",
            "2",
            "--json-output",
        ])
        .arg(&output_path)
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{}",
        String::from_utf8_lossy(&output.stderr)
    );
    let result: BoundedBenchmarkResult =
        serde_json::from_slice(&fs::read(&output_path).unwrap()).unwrap();
    fs::remove_file(output_path).unwrap();

    assert_eq!(result.norb, 24);
    assert_eq!(result.nelec, 10);
    assert_eq!(result.nalpha, 5);
    assert_eq!(result.nbeta, 5);
    assert_eq!(result.space.determinants, 1_806_590_016);
    assert!(result.rhf_converged);
    assert!(result.rhf_absolute_error < 1e-8);
    assert!(!result.point_group_symmetry);
    assert!(!result.full_fci_executed);
    assert!(result.bounded_memory.conservative_peak_bytes < result.memory_budget_bytes);
}
