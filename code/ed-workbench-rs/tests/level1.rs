use std::fs;
use std::path::Path;
use std::process::Command;

use ed_workbench_rs::davidson::{
    DavidsonConfig, DavidsonRunConfig, DavidsonWorkspaceConfig, lowest_eigenpair,
    lowest_eigenpair_with_run_config,
};
use ed_workbench_rs::direct_fci::{DirectFciOperator, ExecutionPolicy};
use ed_workbench_rs::fcidump::Fcidump;
use ed_workbench_rs::operator::LinearOperator;
use ed_workbench_rs::problem::ElectronicProblem;
use ed_workbench_rs::reference::Reference;

fn davidson_fixture(slug: &str, tolerance: f64) {
    let root = Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("fixtures")
        .join(slug);
    let dump = Fcidump::parse(&fs::read_to_string(root.join("FCIDUMP")).unwrap()).unwrap();
    let reference = Reference::load(&root.join("reference.json")).unwrap();
    let operator = DirectFciOperator::new(ElectronicProblem::from_fcidump(&dump).unwrap()).unwrap();
    let mut initial = vec![0.0; operator.dimension()];
    let index = operator
        .diagonal()
        .iter()
        .enumerate()
        .min_by(|(_, left), (_, right)| left.total_cmp(right))
        .unwrap()
        .0;
    initial[index] = 1.0;
    let result = lowest_eigenpair(
        &operator,
        &initial,
        &DavidsonConfig {
            residual_tolerance: tolerance,
            energy_tolerance: tolerance * 0.01,
            max_iterations: 100,
            max_subspace: operator.dimension().clamp(2, 24),
        },
    )
    .unwrap();
    assert!(result.converged, "{slug} residual {}", result.residual_norm);
    assert!(
        (result.energy - reference.fci_energy).abs() < tolerance * 10.0,
        "{slug}: Rust {}, PySCF {}",
        result.energy,
        reference.fci_energy
    );
}

#[test]
fn h2_davidson_matches_pyscf() {
    davidson_fixture("h2-sto3g", 1e-11);
}

#[test]
fn equilibrium_h2_davidson_matches_pyscf() {
    davidson_fixture("h2-equilibrium-sto3g", 1e-11);
}

#[test]
fn h4_davidson_matches_pyscf() {
    davidson_fixture("h4-sto3g", 1e-10);
}

#[test]
fn h2o_sto3g_davidson_matches_pyscf() {
    davidson_fixture("h2o-sto3g", 1e-9);
}

#[test]
fn sigma_benchmark_cli_reports_timing_and_checksum() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h4-sto3g");
    let output = Command::new(env!("CARGO_BIN_EXE_ed_workbench_rs"))
        .arg("sigma-benchmark")
        .arg(root.join("FCIDUMP"))
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{}",
        String::from_utf8_lossy(&output.stderr)
    );
    let stdout = String::from_utf8_lossy(&output.stdout);
    assert!(stdout.contains("determinants: 36"));
    assert!(stdout.contains("sigma apply seconds:"));
    assert!(stdout.contains("weighted checksum:"));
}

#[test]
fn open_shell_doublet_matches_across_memory_disk_and_parallel_paths() {
    let problem = ElectronicProblem::new(
        3,
        1,
        1,
        0.1,
        vec![-1.0, 0.0, 0.0, 0.0, -0.5, 0.0, 0.0, 0.0, 0.2],
        vec![0.0; 3_usize.pow(4)],
    )
    .unwrap();
    let config = DavidsonConfig {
        residual_tolerance: 1e-12,
        energy_tolerance: 1e-14,
        max_iterations: 20,
        max_subspace: 3,
    };
    let initial = [1.0, 0.1, 0.1];

    let serial = DirectFciOperator::new(problem.clone()).unwrap();
    let memory = lowest_eigenpair(&serial, &initial, &config).unwrap();
    assert!(memory.converged);
    assert!((memory.energy - (-0.9)).abs() < 1e-12);

    let parallel = DirectFciOperator::new(problem)
        .unwrap()
        .with_execution_policy(ExecutionPolicy::Parallel {
            blocks: 2,
            memory_budget_bytes: 1 << 20,
            allow_serial_fallback: false,
        })
        .unwrap();
    let workspace =
        std::env::temp_dir().join(format!("ed-workbench-open-shell-{}", std::process::id()));
    if workspace.exists() {
        fs::remove_dir_all(&workspace).unwrap();
    }
    let disk = lowest_eigenpair_with_run_config(
        &parallel,
        &initial,
        &DavidsonRunConfig {
            algorithm: config,
            workspace: Some(DavidsonWorkspaceConfig {
                path: workspace.clone(),
                resume: false,
                checkpoint_every: 1,
                operator_fingerprint: "open-shell-doublet-v1".to_string(),
            }),
        },
    )
    .unwrap();
    fs::remove_dir_all(workspace).unwrap();
    assert!(disk.converged);
    assert!((disk.energy - memory.energy).abs() < 1e-13);
    assert!((disk.residual_norm - memory.residual_norm).abs() < 1e-12);
}
