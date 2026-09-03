use std::fs;
use std::path::Path;

use ed_workbench_rs::coupled_cluster::{
    CcConfig, CcError, CcTermination, solve_cc, solve_cc_series,
};
use ed_workbench_rs::direct_fci::DirectFciOperator;
use ed_workbench_rs::fcidump::Fcidump;
use ed_workbench_rs::problem::ElectronicProblem;
use ed_workbench_rs::reference::Reference;

fn cc_fixture(slug: &str, rank: usize, tolerance: f64) {
    let root = Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("fixtures")
        .join(slug);
    let dump = Fcidump::parse(&fs::read_to_string(root.join("FCIDUMP")).unwrap()).unwrap();
    let reference = Reference::load(&root.join("reference.json")).unwrap();
    let operator = DirectFciOperator::new(ElectronicProblem::from_fcidump(&dump).unwrap()).unwrap();
    let result = solve_cc(
        &operator,
        rank,
        &reference.active_orbital_energies,
        &CcConfig {
            residual_tolerance: tolerance,
            energy_tolerance: tolerance * 0.01,
            max_iterations: 100,
            diis_history: 6,
            exponential_threshold: 1e-14,
        },
    )
    .unwrap();
    assert!(result.converged, "{slug} residual {}", result.residual_norm);
    let expected = if rank >= dump.nelec {
        reference.fci_energy
    } else {
        reference.ccsd_total_energy.unwrap()
    };
    assert!(
        (result.energy - expected).abs() < tolerance * 10.0,
        "{slug} CC({rank}): Rust {}, expected {}",
        result.energy,
        expected
    );
}

#[test]
fn h2_cc2_matches_fci_and_ccsd() {
    cc_fixture("h2-sto3g", 2, 1e-9);
}

#[test]
fn equilibrium_h2_cc2_matches_fci_and_ccsd() {
    cc_fixture("h2-equilibrium-sto3g", 2, 1e-9);
}

#[test]
fn h4_cc2_matches_pyscf_ccsd() {
    cc_fixture("h4-sto3g", 2, 1e-8);
}

#[test]
fn h4_full_rank_cc_matches_fci() {
    cc_fixture("h4-sto3g", 4, 1e-8);
}

#[test]
fn h2o_sto3g_cc2_matches_pyscf_ccsd() {
    cc_fixture("h2o-sto3g", 2, 1e-7);
}

#[test]
fn warm_started_cc_series_matches_single_rank_solver() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h4-sto3g");
    let dump = Fcidump::parse(&fs::read_to_string(root.join("FCIDUMP")).unwrap()).unwrap();
    let reference = Reference::load(&root.join("reference.json")).unwrap();
    let operator = DirectFciOperator::new(ElectronicProblem::from_fcidump(&dump).unwrap()).unwrap();
    let config = CcConfig {
        residual_tolerance: 1e-8,
        energy_tolerance: 1e-10,
        max_iterations: 100,
        diis_history: 6,
        exponential_threshold: 1e-14,
    };
    let single = solve_cc(&operator, 2, &reference.active_orbital_energies, &config).unwrap();
    let series =
        solve_cc_series(&operator, 2, &reference.active_orbital_energies, &config).unwrap();
    assert_eq!(series.len(), 2);
    assert_eq!(series[0].rank, 1);
    assert_eq!(series[1].rank, 2);
    assert!(series.iter().all(|entry| entry.result.converged));
    assert!((series[1].result.energy - single.energy).abs() < 1e-11);
    assert!(series[1].result.iterations.len() <= single.iterations.len());
}

#[test]
fn cc_series_rejects_ranks_outside_the_active_electron_space() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h2-sto3g");
    let dump = Fcidump::parse(&fs::read_to_string(root.join("FCIDUMP")).unwrap()).unwrap();
    let reference = Reference::load(&root.join("reference.json")).unwrap();
    let operator = DirectFciOperator::new(ElectronicProblem::from_fcidump(&dump).unwrap()).unwrap();
    let config = CcConfig::default();
    assert!(matches!(
        solve_cc_series(&operator, 0, &reference.active_orbital_energies, &config),
        Err(CcError::InvalidRank {
            requested: 0,
            maximum: 2
        })
    ));
    assert!(matches!(
        solve_cc_series(&operator, 3, &reference.active_orbital_energies, &config),
        Err(CcError::InvalidRank {
            requested: 3,
            maximum: 2
        })
    ));
}

#[test]
fn rejects_invalid_cc_configuration_before_iteration() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h2-sto3g");
    let dump = Fcidump::parse(&fs::read_to_string(root.join("FCIDUMP")).unwrap()).unwrap();
    let reference = Reference::load(&root.join("reference.json")).unwrap();
    let operator = DirectFciOperator::new(ElectronicProblem::from_fcidump(&dump).unwrap()).unwrap();

    for (field, config) in [
        (
            "residual_tolerance",
            CcConfig {
                residual_tolerance: f64::NAN,
                ..Default::default()
            },
        ),
        (
            "energy_tolerance",
            CcConfig {
                energy_tolerance: 0.0,
                ..Default::default()
            },
        ),
        (
            "max_iterations",
            CcConfig {
                max_iterations: 0,
                ..Default::default()
            },
        ),
        (
            "diis_history",
            CcConfig {
                diis_history: 0,
                ..Default::default()
            },
        ),
        (
            "exponential_threshold",
            CcConfig {
                exponential_threshold: -1.0,
                ..Default::default()
            },
        ),
    ] {
        assert!(matches!(
            solve_cc(
                &operator,
                2,
                &reference.active_orbital_energies,
                &config
            ),
            Err(CcError::InvalidConfig {
                field: actual,
                ..
            }) if actual == field
        ));
    }
}

#[test]
fn reports_explicit_cc_termination() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h2-sto3g");
    let dump = Fcidump::parse(&fs::read_to_string(root.join("FCIDUMP")).unwrap()).unwrap();
    let reference = Reference::load(&root.join("reference.json")).unwrap();
    let operator = DirectFciOperator::new(ElectronicProblem::from_fcidump(&dump).unwrap()).unwrap();

    let converged = solve_cc(
        &operator,
        2,
        &reference.active_orbital_energies,
        &CcConfig::default(),
    )
    .unwrap();
    assert_eq!(converged.termination, CcTermination::Converged);

    let stopped = solve_cc(
        &operator,
        2,
        &reference.active_orbital_energies,
        &CcConfig {
            max_iterations: 1,
            ..Default::default()
        },
    )
    .unwrap();
    assert_eq!(stopped.termination, CcTermination::MaximumIterations);
    assert!(!stopped.converged);
}
