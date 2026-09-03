use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

use ed_workbench_rs::davidson::{
    DavidsonConfig, DavidsonError, DavidsonRunConfig, DavidsonWorkspaceConfig, lowest_eigenpair,
    lowest_eigenpair_with_run_config,
};
use ed_workbench_rs::operator::{LinearOperator, OperatorError};
use nalgebra::{DMatrix, DVector};
use serde_json::Value;

struct MatrixOperator {
    matrix: DMatrix<f64>,
    diagonal: Vec<f64>,
}

impl LinearOperator for MatrixOperator {
    fn dimension(&self) -> usize {
        self.matrix.nrows()
    }

    fn diagonal(&self) -> &[f64] {
        &self.diagonal
    }

    fn apply(&self, input: &[f64], output: &mut [f64]) -> Result<(), OperatorError> {
        let result = &self.matrix * DVector::from_column_slice(input);
        output.copy_from_slice(result.as_slice());
        Ok(())
    }
}

fn operator() -> MatrixOperator {
    let matrix = DMatrix::from_row_slice(
        5,
        5,
        &[
            1.0, 0.2, 0.0, 0.0, 0.0, 0.2, 1.7, 0.3, 0.0, 0.0, 0.0, 0.3, 2.5, 0.4, 0.0, 0.0, 0.0,
            0.4, 3.6, 0.5, 0.0, 0.0, 0.0, 0.5, 5.0,
        ],
    );
    MatrixOperator {
        diagonal: matrix.diagonal().iter().copied().collect(),
        matrix,
    }
}

fn workspace(label: &str) -> PathBuf {
    std::env::temp_dir().join(format!(
        "ed-workbench-{label}-{}-{}",
        std::process::id(),
        std::thread::current().name().unwrap_or("test")
    ))
}

fn clean_workspace(path: &Path) {
    if path.exists() {
        fs::remove_dir_all(path).unwrap();
    }
}

fn run_config(
    path: &Path,
    resume: bool,
    max_iterations: usize,
    fingerprint: &str,
) -> DavidsonRunConfig {
    DavidsonRunConfig {
        algorithm: DavidsonConfig {
            residual_tolerance: 1e-12,
            energy_tolerance: 1e-14,
            max_iterations,
            max_subspace: 4,
        },
        workspace: Some(DavidsonWorkspaceConfig {
            path: path.to_path_buf(),
            resume,
            checkpoint_every: 1,
            operator_fingerprint: fingerprint.to_string(),
        }),
    }
}

#[test]
fn interrupted_disk_run_resumes_to_the_in_memory_result() {
    let operator = operator();
    let initial = [1.0, 0.1, 0.1, 0.1, 0.1];
    let baseline = lowest_eigenpair(
        &operator,
        &initial,
        &DavidsonConfig {
            residual_tolerance: 1e-12,
            energy_tolerance: 1e-14,
            max_iterations: 100,
            max_subspace: 4,
        },
    )
    .unwrap();
    let workspace = workspace("resume");
    clean_workspace(&workspace);

    let interrupted = lowest_eigenpair_with_run_config(
        &operator,
        &initial,
        &run_config(&workspace, false, 1, "matrix-v1"),
    )
    .unwrap();
    assert!(!interrupted.converged);
    assert_eq!(interrupted.iterations, 1);
    assert!(workspace.join("checkpoint.json").is_file());

    let resumed = lowest_eigenpair_with_run_config(
        &operator,
        &initial,
        &run_config(&workspace, true, 100, "matrix-v1"),
    )
    .unwrap();
    clean_workspace(&workspace);

    assert!(resumed.converged);
    assert!((resumed.energy - baseline.energy).abs() < 1e-13);
    assert!((resumed.residual_norm - baseline.residual_norm).abs() < 1e-12);
    let phase = if resumed.eigenvector[0] * baseline.eigenvector[0] < 0.0 {
        -1.0
    } else {
        1.0
    };
    let vector_error = resumed
        .eigenvector
        .iter()
        .zip(&baseline.eigenvector)
        .map(|(actual, expected)| (actual - phase * expected).abs())
        .fold(0.0, f64::max);
    assert!(vector_error < 1e-11, "eigenvector error {vector_error:e}");
}

#[test]
fn resume_rejects_an_operator_fingerprint_mismatch() {
    let operator = operator();
    let workspace = workspace("fingerprint");
    clean_workspace(&workspace);
    lowest_eigenpair_with_run_config(
        &operator,
        &[1.0, 0.1, 0.1, 0.1, 0.1],
        &run_config(&workspace, false, 1, "matrix-v1"),
    )
    .unwrap();

    let error = lowest_eigenpair_with_run_config(
        &operator,
        &[1.0, 0.1, 0.1, 0.1, 0.1],
        &run_config(&workspace, true, 100, "other-matrix"),
    )
    .unwrap_err();
    clean_workspace(&workspace);
    assert!(matches!(
        error,
        DavidsonError::CheckpointMismatch {
            field: "operator_fingerprint",
            ..
        }
    ));
}

#[test]
fn resume_rejects_a_truncated_vector_file() {
    let operator = operator();
    let workspace = workspace("truncated");
    clean_workspace(&workspace);
    lowest_eigenpair_with_run_config(
        &operator,
        &[1.0, 0.1, 0.1, 0.1, 0.1],
        &run_config(&workspace, false, 1, "matrix-v1"),
    )
    .unwrap();

    let manifest: Value =
        serde_json::from_slice(&fs::read(workspace.join("checkpoint.json")).unwrap()).unwrap();
    let generation = manifest["basis_generation"].as_u64().unwrap();
    let vector = workspace
        .join("basis")
        .join(format!("generation-{generation:06}"))
        .join("vector-000000.bin");
    fs::write(&vector, [0_u8]).unwrap();

    let error = lowest_eigenpair_with_run_config(
        &operator,
        &[1.0, 0.1, 0.1, 0.1, 0.1],
        &run_config(&workspace, true, 100, "matrix-v1"),
    )
    .unwrap_err();
    clean_workspace(&workspace);
    assert!(matches!(error, DavidsonError::InvalidVectorFile { .. }));
}

#[test]
fn fresh_run_refuses_a_nonempty_workspace() {
    let operator = operator();
    let workspace = workspace("nonempty");
    clean_workspace(&workspace);
    fs::create_dir_all(&workspace).unwrap();
    fs::write(workspace.join("unrelated.txt"), b"preserve me").unwrap();

    let error = lowest_eigenpair_with_run_config(
        &operator,
        &[1.0, 0.1, 0.1, 0.1, 0.1],
        &run_config(&workspace, false, 1, "matrix-v1"),
    )
    .unwrap_err();
    assert!(workspace.join("unrelated.txt").is_file());
    clean_workspace(&workspace);
    assert!(matches!(error, DavidsonError::WorkspaceNotEmpty { .. }));
}

#[test]
fn davidson_cli_documents_and_resumes_a_disk_workspace() {
    let help = Command::new(env!("CARGO_BIN_EXE_ed_workbench_rs"))
        .args(["davidson", "--help"])
        .output()
        .unwrap();
    assert!(help.status.success());
    let help = String::from_utf8_lossy(&help.stdout);
    for option in [
        "--workspace",
        "--resume",
        "--checkpoint-every",
        "--memory-budget-gib",
        "--operator-fingerprint",
    ] {
        assert!(help.contains(option), "missing {option} in {help}");
    }

    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h4-sto3g");
    let workspace = workspace("cli");
    clean_workspace(&workspace);
    let interrupted = Command::new(env!("CARGO_BIN_EXE_ed_workbench_rs"))
        .arg("davidson")
        .arg(root.join("FCIDUMP"))
        .args(["--max-iterations", "1", "--workspace"])
        .arg(&workspace)
        .output()
        .unwrap();
    assert!(!interrupted.status.success());
    assert!(workspace.join("checkpoint.json").is_file());

    let resumed = Command::new(env!("CARGO_BIN_EXE_ed_workbench_rs"))
        .arg("davidson")
        .arg(root.join("FCIDUMP"))
        .args(["--max-iterations", "100", "--workspace"])
        .arg(&workspace)
        .arg("--resume")
        .output()
        .unwrap();
    clean_workspace(&workspace);
    assert!(
        resumed.status.success(),
        "{}",
        String::from_utf8_lossy(&resumed.stderr)
    );
    let stdout = String::from_utf8_lossy(&resumed.stdout);
    assert!(stdout.contains("storage: disk workspace"));
    assert!(stdout.contains("converged: true"));
}
