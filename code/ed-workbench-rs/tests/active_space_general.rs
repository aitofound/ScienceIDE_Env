use ed_workbench_rs::active_space::{
    ActiveSpaceError, ActiveSpaceSpec, build_active_space, freeze_core,
};
use ed_workbench_rs::problem::ElectronicProblem;

fn diagonal_problem(norb: usize, nelec: usize, ms2: isize) -> ElectronicProblem {
    let mut h1 = vec![0.0; norb * norb];
    for orbital in 0..norb {
        h1[orbital * norb + orbital] = -1.0 + 0.5 * orbital as f64;
    }
    ElectronicProblem::new(norb, nelec, ms2, 0.3, h1, vec![0.0; norb.pow(4)])
        .unwrap()
        .with_orbital_energies((0..norb).map(|index| index as f64 * 0.25).collect())
}

#[test]
fn selects_frozen_occupied_and_virtual_orbitals_with_maps() {
    let result = build_active_space(
        &diagonal_problem(4, 4, 0),
        &ActiveSpaceSpec {
            frozen_occupied: vec![0],
            frozen_virtual: vec![3],
        },
    )
    .unwrap();

    assert_eq!(result.problem.norb, 2);
    assert_eq!(result.problem.nelec, 2);
    assert_eq!(result.problem.ms2, 0);
    assert_eq!(result.active_to_original, vec![1, 2]);
    assert_eq!(
        result.original_to_active,
        vec![None, Some(0), Some(1), None]
    );
    assert!((result.problem.ecore - (-1.7)).abs() < 1e-12);
    assert_eq!(result.problem.h1(0, 0), -0.5);
    assert_eq!(result.problem.h1(1, 1), 0.0);
    assert_eq!(
        result.problem.orbital_energies.as_deref(),
        Some(&[0.25, 0.5][..])
    );
}

#[test]
fn preserves_open_shell_spin_when_freezing_a_doubly_occupied_orbital() {
    let result = build_active_space(
        &diagonal_problem(3, 3, 1),
        &ActiveSpaceSpec {
            frozen_occupied: vec![0],
            frozen_virtual: Vec::new(),
        },
    )
    .unwrap();

    assert_eq!(result.problem.norb, 2);
    assert_eq!(result.problem.nelec, 1);
    assert_eq!(result.problem.ms2, 1);
}

#[test]
fn compatibility_wrapper_matches_general_transformation() {
    let problem = diagonal_problem(3, 4, 0);
    let legacy = freeze_core(&problem, &[0]).unwrap();
    let general = build_active_space(
        &problem,
        &ActiveSpaceSpec {
            frozen_occupied: vec![0],
            frozen_virtual: Vec::new(),
        },
    )
    .unwrap()
    .problem;

    assert_eq!(legacy.norb, general.norb);
    assert_eq!(legacy.nelec, general.nelec);
    assert_eq!(legacy.ecore, general.ecore);
    assert_eq!(legacy.h1_data(), general.h1_data());
    assert_eq!(legacy.eri_data(), general.eri_data());
}

#[test]
fn rejects_ambiguous_or_impossible_selections() {
    let problem = diagonal_problem(4, 4, 0);

    assert!(matches!(
        build_active_space(
            &problem,
            &ActiveSpaceSpec {
                frozen_occupied: vec![0, 0],
                frozen_virtual: Vec::new(),
            }
        ),
        Err(ActiveSpaceError::DuplicateOccupied(0))
    ));
    assert!(matches!(
        build_active_space(
            &problem,
            &ActiveSpaceSpec {
                frozen_occupied: Vec::new(),
                frozen_virtual: vec![3, 3],
            }
        ),
        Err(ActiveSpaceError::DuplicateVirtual(3))
    ));
    assert!(matches!(
        build_active_space(
            &problem,
            &ActiveSpaceSpec {
                frozen_occupied: vec![1],
                frozen_virtual: vec![1],
            }
        ),
        Err(ActiveSpaceError::OccupiedVirtualOverlap(1))
    ));
    assert!(matches!(
        build_active_space(
            &problem,
            &ActiveSpaceSpec {
                frozen_occupied: Vec::new(),
                frozen_virtual: vec![4],
            }
        ),
        Err(ActiveSpaceError::InvalidOrbital(4))
    ));
    assert!(matches!(
        build_active_space(
            &problem,
            &ActiveSpaceSpec {
                frozen_occupied: vec![0, 1, 2],
                frozen_virtual: Vec::new(),
            }
        ),
        Err(ActiveSpaceError::TooManyFrozen {
            frozen: 3,
            nelec: 4
        })
    ));
    assert!(matches!(
        build_active_space(
            &problem,
            &ActiveSpaceSpec {
                frozen_occupied: vec![0, 1],
                frozen_virtual: vec![2, 3],
            }
        ),
        Err(ActiveSpaceError::NoActiveOrbitals)
    ));
}
