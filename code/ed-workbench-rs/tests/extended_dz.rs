use std::fs;
use std::path::Path;

use ed_workbench_rs::davidson::{DavidsonConfig, lowest_eigenpair};
use ed_workbench_rs::direct_fci::DirectFciOperator;
use ed_workbench_rs::fcidump::Fcidump;
use ed_workbench_rs::operator::LinearOperator;
use ed_workbench_rs::problem::ElectronicProblem;
use ed_workbench_rs::reference::{Reference, sha256_hex};

fn root() -> std::path::PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h2o-dz-ae")
}

#[test]
fn committed_dz_oracle_matches_the_published_fci_anchor() {
    let root = root();
    let bytes = fs::read(root.join("FCIDUMP")).unwrap();
    let dump = Fcidump::parse(std::str::from_utf8(&bytes).unwrap()).unwrap();
    let reference = Reference::load(&root.join("reference.json")).unwrap();
    let published: serde_json::Value =
        serde_json::from_slice(&fs::read(root.join("published-fci-anchor.json")).unwrap()).unwrap();

    assert_eq!(dump.norb, 14);
    assert_eq!(dump.nelec, 10);
    assert_eq!(dump.ms2, 0);
    assert_eq!(dump.isym, 1);
    assert_eq!(reference.frozen_orbitals, Vec::<usize>::new());
    assert_eq!(sha256_hex(&bytes), reference.fcidump_sha256);

    let published_energy = published["energy"].as_f64().unwrap();
    assert_eq!(
        (reference.fci_energy * 1e6).round() as i64,
        (published_energy * 1e6).round() as i64
    );
}

#[test]
#[ignore = "release-mode H2O/DZ Davidson validation takes about 150 seconds"]
fn live_dz_davidson_matches_pyscf() {
    let root = root();
    let dump = Fcidump::parse(&fs::read_to_string(root.join("FCIDUMP")).unwrap()).unwrap();
    let reference = Reference::load(&root.join("reference.json")).unwrap();
    let operator = DirectFciOperator::new(ElectronicProblem::from_fcidump(&dump).unwrap()).unwrap();
    assert_eq!(operator.dimension(), 1_002_708);

    let mut initial = vec![0.0; operator.dimension()];
    let reference_index = operator
        .diagonal()
        .iter()
        .enumerate()
        .min_by(|(_, left), (_, right)| left.total_cmp(right))
        .unwrap()
        .0;
    initial[reference_index] = 1.0;
    let result = lowest_eigenpair(
        &operator,
        &initial,
        &DavidsonConfig {
            residual_tolerance: 1e-7,
            energy_tolerance: 1e-9,
            max_iterations: 40,
            max_subspace: 20,
        },
    )
    .unwrap();
    assert!(result.converged, "residual {}", result.residual_norm);
    assert!((result.energy - reference.fci_energy).abs() < 1e-10);
}
