use std::fs;
use std::path::Path;

use ed_workbench_rs::davidson::DavidsonConfig;
use ed_workbench_rs::direct_fci::DirectFciOperator;
use ed_workbench_rs::fcidump::Fcidump;
use ed_workbench_rs::mbpt::solve_mbpt;
use ed_workbench_rs::optimizer::BfgsConfig;
use ed_workbench_rs::problem::ElectronicProblem;
use ed_workbench_rs::reference::Reference;
use ed_workbench_rs::truncated_ci::{TruncatedCiError, solve_ci, solve_ci_series};
use ed_workbench_rs::unitary_cc::UnitaryCcModel;

fn load(slug: &str) -> (DirectFciOperator, Reference) {
    let root = Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("fixtures")
        .join(slug);
    let dump = Fcidump::parse(&fs::read_to_string(root.join("FCIDUMP")).unwrap()).unwrap();
    let reference = Reference::load(&root.join("reference.json")).unwrap();
    let operator = DirectFciOperator::new(ElectronicProblem::from_fcidump(&dump).unwrap()).unwrap();
    (operator, reference)
}

#[test]
fn h4_ci_is_variational_and_full_rank_is_fci() {
    let (operator, reference) = load("h4-sto3g");
    let config = DavidsonConfig {
        residual_tolerance: 1e-10,
        energy_tolerance: 1e-12,
        max_iterations: 100,
        max_subspace: 36,
    };
    let mut energies = Vec::new();
    for rank in 1..=4 {
        let result = solve_ci(&operator, rank, &config).unwrap();
        assert!(result.converged);
        energies.push(result.energy);
    }
    for pair in energies.windows(2) {
        assert!(pair[1] <= pair[0] + 1e-10);
    }
    assert!((energies[3] - reference.fci_energy).abs() < 1e-10);
}

#[test]
fn h2_mbpt_second_order_matches_pyscf_mp2() {
    let (operator, reference) = load("h2-sto3g");
    let result = solve_mbpt(&operator, &reference.active_orbital_energies, 6).unwrap();
    assert!(result.corrections[0].abs() < 1e-12);
    assert!((result.partial_sums[1] - reference.mp2_total_energy.unwrap()).abs() < 1e-10);
}

#[test]
fn h2_full_rank_ucc_reaches_fci_variationally() {
    let (operator, reference) = load("h2-sto3g");
    let model = UnitaryCcModel::new(&operator, 2).unwrap();
    let hf_energy = model.energy(&vec![0.0; model.parameter_count()]).unwrap();
    let result = model.optimize(&BfgsConfig {
        gradient_tolerance: 1e-7,
        max_iterations: 100,
        finite_difference_step: 1e-5,
    });
    assert!(result.converged, "gradient {}", result.gradient_norm);
    assert!(result.value <= hf_energy + 1e-10);
    assert!(result.value >= reference.fci_energy - 1e-10);
    assert!((result.value - reference.fci_energy).abs() < 1e-8);
}

#[test]
fn h4_full_rank_ucc_reaches_fci_variationally() {
    let (operator, reference) = load("h4-sto3g");
    let model = UnitaryCcModel::new(&operator, 4).unwrap();
    let hf_energy = model.energy(&vec![0.0; model.parameter_count()]).unwrap();
    let result = model.optimize(&BfgsConfig {
        gradient_tolerance: 1e-7,
        max_iterations: 100,
        finite_difference_step: 1e-5,
    });
    assert_eq!(model.parameter_count(), 35);
    assert!(result.converged, "gradient {}", result.gradient_norm);
    assert!(result.value <= hf_energy + 1e-10);
    assert!(result.value >= reference.fci_energy - 1e-10);
    assert!((result.value - reference.fci_energy).abs() < 1e-8);
    let committed: serde_json::Value = serde_json::from_slice(
        &fs::read(Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h4-sto3g/ucc4_result.json"))
            .unwrap(),
    )
    .unwrap();
    assert_eq!(committed["calculation_commit"].as_str().unwrap().len(), 40);
    assert_eq!(committed["energy_unit"], "hartree");
    assert!((result.value - committed["ucc_energy"].as_f64().unwrap()).abs() < 1e-12);
}

#[test]
fn warm_started_ci_series_is_variational_and_reaches_fci() {
    let (operator, reference) = load("h4-sto3g");
    let config = DavidsonConfig {
        residual_tolerance: 1e-10,
        energy_tolerance: 1e-12,
        max_iterations: 100,
        max_subspace: 36,
    };
    let series = solve_ci_series(&operator, 4, &config).unwrap();
    assert_eq!(series.len(), 4);
    for (index, entry) in series.iter().enumerate() {
        assert_eq!(entry.rank, index + 1);
        assert!(entry.result.converged);
    }
    for pair in series.windows(2) {
        assert!(pair[1].result.energy <= pair[0].result.energy + 1e-10);
    }
    assert!((series[3].result.energy - reference.fci_energy).abs() < 1e-10);
}

#[test]
fn ci_series_rejects_invalid_maximum_rank() {
    let (operator, _) = load("h2-sto3g");
    assert!(matches!(
        solve_ci_series(&operator, 0, &DavidsonConfig::default()),
        Err(TruncatedCiError::InvalidRank {
            requested: 0,
            maximum: 2
        })
    ));
    assert!(matches!(
        solve_ci_series(&operator, 3, &DavidsonConfig::default()),
        Err(TruncatedCiError::InvalidRank {
            requested: 3,
            maximum: 2
        })
    ));
}
