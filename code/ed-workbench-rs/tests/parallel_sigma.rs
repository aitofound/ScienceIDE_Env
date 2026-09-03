use std::fs;
use std::path::Path;
use std::process::Command;

use ed_workbench_rs::direct_fci::{
    DirectFciError, DirectFciOperator, ExecutionMode, ExecutionPolicy,
};
use ed_workbench_rs::fcidump::Fcidump;
use ed_workbench_rs::operator::LinearOperator;
use ed_workbench_rs::problem::ElectronicProblem;
use serde::Deserialize;

#[derive(Deserialize)]
struct ParallelMeasurement {
    schema_version: u32,
    artifact_kind: String,
    measured_commit: String,
    problem: MeasuredProblem,
    parallel_policy: MeasuredPolicy,
    runs: Vec<MeasuredRun>,
    aggregate: MeasuredAggregate,
}

#[derive(Deserialize)]
struct MeasuredProblem {
    determinants: usize,
}

#[derive(Deserialize)]
struct MeasuredPolicy {
    source_blocks: usize,
    preflight_workspace_bytes: u64,
}

#[derive(Deserialize)]
struct MeasuredRun {
    index: usize,
    serial_seconds: f64,
    parallel_seconds: f64,
    maximum_serial_parallel_error: f64,
}

#[derive(Deserialize)]
struct MeasuredAggregate {
    median_serial_seconds: f64,
    median_parallel_seconds: f64,
    ratio_of_medians: f64,
    maximum_serial_parallel_error: f64,
}

fn operator(slug: &str) -> DirectFciOperator {
    let path = Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("fixtures")
        .join(slug)
        .join("FCIDUMP");
    let dump = Fcidump::parse(&fs::read_to_string(path).unwrap()).unwrap();
    DirectFciOperator::new(ElectronicProblem::from_fcidump(&dump).unwrap()).unwrap()
}

fn compare(slug: &str) {
    let serial = operator(slug);
    let input: Vec<_> = (0..serial.dimension())
        .map(|index| ((index * 31 + 7) as f64).sin())
        .collect();
    let mut expected = vec![0.0; input.len()];
    serial.apply(&input, &mut expected).unwrap();

    let parallel = operator(slug)
        .with_execution_policy(ExecutionPolicy::Parallel {
            blocks: 4,
            memory_budget_bytes: 1 << 30,
            allow_serial_fallback: false,
        })
        .unwrap();
    let mut first = vec![0.0; input.len()];
    let first_report = parallel.apply_with_report(&input, &mut first).unwrap();
    let mut second = vec![0.0; input.len()];
    let second_report = parallel.apply_with_report(&input, &mut second).unwrap();

    assert_eq!(first_report.effective_mode, ExecutionMode::Parallel);
    assert_eq!(first_report.effective_blocks, input.len().min(4));
    assert!(first_report.workspace_bytes > 0);
    assert_eq!(first_report.fallback_reason, None);
    assert_eq!(first_report.workspace_bytes, second_report.workspace_bytes);
    assert!(
        first
            .iter()
            .zip(&second)
            .all(|(left, right)| left.to_bits() == right.to_bits()),
        "{slug} parallel sigma was not bitwise repeatable"
    );
    let maximum_error = first
        .iter()
        .zip(&expected)
        .map(|(actual, expected)| (actual - expected).abs())
        .fold(0.0, f64::max);
    assert!(
        maximum_error < 1e-11,
        "{slug} serial/parallel sigma error {maximum_error:e}"
    );
}

#[test]
fn fixed_block_parallel_sigma_matches_serial_and_repeats() {
    compare("h2-sto3g");
    compare("h4-sto3g");
    compare("h2o-sto3g");
}

#[test]
fn parallel_memory_preflight_can_reject_or_fallback() {
    let strict = operator("h4-sto3g").with_execution_policy(ExecutionPolicy::Parallel {
        blocks: 4,
        memory_budget_bytes: 1,
        allow_serial_fallback: false,
    });
    assert!(matches!(
        strict,
        Err(DirectFciError::ParallelMemoryBudget {
            required_bytes: _,
            budget_bytes: 1
        })
    ));

    let fallback = operator("h4-sto3g")
        .with_execution_policy(ExecutionPolicy::Parallel {
            blocks: 4,
            memory_budget_bytes: 1,
            allow_serial_fallback: true,
        })
        .unwrap();
    let input = vec![1.0; fallback.dimension()];
    let mut output = vec![0.0; fallback.dimension()];
    let report = fallback.apply_with_report(&input, &mut output).unwrap();
    assert_eq!(report.requested_mode, ExecutionMode::Parallel);
    assert_eq!(report.effective_mode, ExecutionMode::Serial);
    assert!(report.fallback_reason.unwrap().contains("exceeds budget"));
}

#[test]
fn parallel_policy_rejects_zero_blocks() {
    assert!(matches!(
        operator("h2-sto3g").with_execution_policy(ExecutionPolicy::Parallel {
            blocks: 0,
            memory_budget_bytes: 1 << 20,
            allow_serial_fallback: false,
        }),
        Err(DirectFciError::InvalidParallelBlocks)
    ));
}

#[test]
fn davidson_cli_exposes_parallel_preflight_and_strict_rejection() {
    let help = Command::new(env!("CARGO_BIN_EXE_ed_workbench_rs"))
        .args(["davidson", "--help"])
        .output()
        .unwrap();
    assert!(help.status.success());
    let help = String::from_utf8_lossy(&help.stdout);
    for option in [
        "--parallel-blocks",
        "--parallel-memory-budget-gib",
        "--strict-parallel-memory",
    ] {
        assert!(help.contains(option), "missing {option} in {help}");
    }

    let fixture = Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h4-sto3g/FCIDUMP");
    let parallel = Command::new(env!("CARGO_BIN_EXE_ed_workbench_rs"))
        .arg("davidson")
        .arg(&fixture)
        .args(["--parallel-blocks", "2"])
        .output()
        .unwrap();
    assert!(
        parallel.status.success(),
        "{}",
        String::from_utf8_lossy(&parallel.stderr)
    );
    let stdout = String::from_utf8_lossy(&parallel.stdout);
    assert!(stdout.contains("requested sigma mode: Parallel"));
    assert!(stdout.contains("effective sigma mode: Parallel"));
    assert!(stdout.contains("sigma source blocks: 2"));

    let rejected = Command::new(env!("CARGO_BIN_EXE_ed_workbench_rs"))
        .arg("davidson")
        .arg(fixture)
        .args([
            "--parallel-blocks",
            "2",
            "--parallel-memory-budget-gib",
            "0.000000001",
            "--strict-parallel-memory",
        ])
        .output()
        .unwrap();
    assert!(!rejected.status.success());
    assert!(
        String::from_utf8_lossy(&rejected.stderr).contains("exceeds budget"),
        "{}",
        String::from_utf8_lossy(&rejected.stderr)
    );
}

#[test]
#[ignore = "release-mode primary H2O/6-31G serial/parallel sigma measurement"]
fn primary_water_parallel_sigma_measurement() {
    let serial = operator("h2o-631g-fc");
    let input: Vec<_> = (0..serial.dimension())
        .map(|index| ((index * 31 + 7) as f64).sin())
        .collect();
    let mut serial_output = vec![0.0; input.len()];
    let serial_report = serial
        .apply_with_report(&input, &mut serial_output)
        .unwrap();

    let parallel = operator("h2o-631g-fc")
        .with_execution_policy(ExecutionPolicy::Parallel {
            blocks: 4,
            memory_budget_bytes: 2 * 1024_u64.pow(3),
            allow_serial_fallback: false,
        })
        .unwrap();
    let mut parallel_output = vec![0.0; input.len()];
    let parallel_report = parallel
        .apply_with_report(&input, &mut parallel_output)
        .unwrap();
    let maximum_error = serial_output
        .iter()
        .zip(&parallel_output)
        .map(|(serial, parallel)| (serial - parallel).abs())
        .fold(0.0, f64::max);
    assert!(maximum_error < 1e-10, "maximum error {maximum_error:e}");

    println!(
        "{}",
        serde_json::to_string_pretty(&serde_json::json!({
            "dimension": input.len(),
            "maximum_serial_parallel_error": maximum_error,
            "serial": serial_report,
            "parallel": parallel_report,
        }))
        .unwrap()
    );
}

#[test]
fn committed_primary_parallel_measurement_recomputes() {
    let path =
        Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h2o-631g-fc/parallel-sigma-m4.json");
    let measurement: ParallelMeasurement =
        serde_json::from_slice(&fs::read(path).unwrap()).unwrap();
    assert_eq!(measurement.schema_version, 1);
    assert_eq!(measurement.artifact_kind, "h2o-631g-fc-parallel-sigma");
    assert_eq!(measurement.measured_commit.len(), 40);
    assert_eq!(measurement.problem.determinants, 245_025);
    assert_eq!(measurement.parallel_policy.source_blocks, 4);
    assert_eq!(
        measurement.parallel_policy.preflight_workspace_bytes,
        7_840_800
    );
    assert_eq!(measurement.runs.len(), 5);
    assert_eq!(
        measurement
            .runs
            .iter()
            .map(|run| run.index)
            .collect::<Vec<_>>(),
        vec![1, 2, 3, 4, 5]
    );
    let median = |mut values: Vec<f64>| {
        values.sort_by(f64::total_cmp);
        values[values.len() / 2]
    };
    let serial = median(
        measurement
            .runs
            .iter()
            .map(|run| run.serial_seconds)
            .collect(),
    );
    let parallel = median(
        measurement
            .runs
            .iter()
            .map(|run| run.parallel_seconds)
            .collect(),
    );
    let maximum_error = measurement
        .runs
        .iter()
        .map(|run| run.maximum_serial_parallel_error)
        .fold(0.0, f64::max);
    assert_eq!(serial, measurement.aggregate.median_serial_seconds);
    assert_eq!(parallel, measurement.aggregate.median_parallel_seconds);
    assert!((serial / parallel - measurement.aggregate.ratio_of_medians).abs() < 1e-6);
    assert_eq!(
        maximum_error,
        measurement.aggregate.maximum_serial_parallel_error
    );
    assert!(maximum_error < 1e-10);
}
