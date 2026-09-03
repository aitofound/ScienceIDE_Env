use std::collections::HashSet;

use thiserror::Error;

use crate::problem::{ElectronicProblem, ProblemError};

#[derive(Debug, Error)]
pub enum ActiveSpaceError {
    #[error("frozen orbital {0} is out of range")]
    InvalidOrbital(usize),
    #[error("frozen occupied orbital {0} is listed more than once")]
    DuplicateOccupied(usize),
    #[error("frozen virtual orbital {0} is listed more than once")]
    DuplicateVirtual(usize),
    #[error("orbital {0} cannot be both frozen occupied and frozen virtual")]
    OccupiedVirtualOverlap(usize),
    #[error("cannot freeze {frozen} doubly occupied orbitals with only {nelec} electrons")]
    TooManyFrozen { frozen: usize, nelec: usize },
    #[error("active-space selection removes every orbital")]
    NoActiveOrbitals,
    #[error(transparent)]
    Problem(#[from] ProblemError),
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct ActiveSpaceSpec {
    pub frozen_occupied: Vec<usize>,
    pub frozen_virtual: Vec<usize>,
}

#[derive(Debug, Clone)]
pub struct ActiveSpaceResult {
    pub problem: ElectronicProblem,
    pub active_to_original: Vec<usize>,
    pub original_to_active: Vec<Option<usize>>,
}

pub fn build_active_space(
    problem: &ElectronicProblem,
    spec: &ActiveSpaceSpec,
) -> Result<ActiveSpaceResult, ActiveSpaceError> {
    let mut frozen_occupied = spec.frozen_occupied.clone();
    let mut frozen_virtual = spec.frozen_virtual.clone();
    frozen_occupied.sort_unstable();
    frozen_virtual.sort_unstable();

    if let Some(duplicate) = first_duplicate(&frozen_occupied) {
        return Err(ActiveSpaceError::DuplicateOccupied(duplicate));
    }
    if let Some(duplicate) = first_duplicate(&frozen_virtual) {
        return Err(ActiveSpaceError::DuplicateVirtual(duplicate));
    }
    if let Some(&orbital) = frozen_occupied
        .iter()
        .chain(&frozen_virtual)
        .find(|&&orbital| orbital >= problem.norb)
    {
        return Err(ActiveSpaceError::InvalidOrbital(orbital));
    }
    let occupied_set: HashSet<_> = frozen_occupied.iter().copied().collect();
    if let Some(&orbital) = frozen_virtual
        .iter()
        .find(|&&orbital| occupied_set.contains(&orbital))
    {
        return Err(ActiveSpaceError::OccupiedVirtualOverlap(orbital));
    }
    if 2 * frozen_occupied.len() > problem.nelec {
        return Err(ActiveSpaceError::TooManyFrozen {
            frozen: frozen_occupied.len(),
            nelec: problem.nelec,
        });
    }
    let frozen_virtual_set: HashSet<_> = frozen_virtual.iter().copied().collect();
    let active_to_original: Vec<_> = (0..problem.norb)
        .filter(|orbital| !occupied_set.contains(orbital) && !frozen_virtual_set.contains(orbital))
        .collect();
    if active_to_original.is_empty() {
        return Err(ActiveSpaceError::NoActiveOrbitals);
    }
    let mut original_to_active = vec![None; problem.norb];
    for (active, &original) in active_to_original.iter().enumerate() {
        original_to_active[original] = Some(active);
    }

    let mut ecore = problem.ecore;
    for &i in &frozen_occupied {
        ecore += 2.0 * problem.h1(i, i);
        for &j in &frozen_occupied {
            ecore += 2.0 * problem.eri(i, i, j, j) - problem.eri(i, j, j, i);
        }
    }
    let nactive = active_to_original.len();
    let mut h1 = vec![0.0; nactive * nactive];
    let mut eri = vec![0.0; nactive.pow(4)];
    for (p_new, &p) in active_to_original.iter().enumerate() {
        for (q_new, &q) in active_to_original.iter().enumerate() {
            let mut value = problem.h1(p, q);
            for &i in &frozen_occupied {
                value += 2.0 * problem.eri(p, q, i, i) - problem.eri(p, i, i, q);
            }
            h1[p_new * nactive + q_new] = value;
            for (r_new, &r) in active_to_original.iter().enumerate() {
                for (s_new, &s) in active_to_original.iter().enumerate() {
                    eri[((p_new * nactive + q_new) * nactive + r_new) * nactive + s_new] =
                        problem.eri(p, q, r, s);
                }
            }
        }
    }
    let active_orbsym = active_to_original
        .iter()
        .map(|&index| problem.orbsym[index])
        .collect();
    let mut active_problem = ElectronicProblem::new(
        nactive,
        problem.nelec - 2 * frozen_occupied.len(),
        problem.ms2,
        ecore,
        h1,
        eri,
    )?
    .with_symmetry(active_orbsym, problem.isym)?;
    if let Some(energies) = &problem.orbital_energies {
        active_problem.orbital_energies = Some(
            active_to_original
                .iter()
                .map(|&index| energies[index])
                .collect(),
        );
    }
    Ok(ActiveSpaceResult {
        problem: active_problem,
        active_to_original,
        original_to_active,
    })
}

pub fn freeze_core(
    problem: &ElectronicProblem,
    frozen: &[usize],
) -> Result<ElectronicProblem, ActiveSpaceError> {
    Ok(build_active_space(
        problem,
        &ActiveSpaceSpec {
            frozen_occupied: frozen.to_vec(),
            frozen_virtual: Vec::new(),
        },
    )?
    .problem)
}

fn first_duplicate(sorted: &[usize]) -> Option<usize> {
    sorted
        .windows(2)
        .find(|pair| pair[0] == pair[1])
        .map(|pair| pair[0])
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn no_frozen_orbitals_is_identity() {
        let problem = ElectronicProblem::new(1, 2, 0, 0.5, vec![-1.0], vec![0.7]).unwrap();
        let active = freeze_core(&problem, &[]).unwrap();
        assert_eq!(active.ecore, problem.ecore);
        assert_eq!(active.h1(0, 0), problem.h1(0, 0));
    }

    #[test]
    fn folds_a_doubly_occupied_core_into_ecore() {
        let mut eri = vec![0.0; 16];
        eri[0] = 0.7;
        let problem =
            ElectronicProblem::new(2, 4, 0, 0.5, vec![-1.0, 0.0, 0.0, -0.5], eri).unwrap();
        let active = freeze_core(&problem, &[0]).unwrap();
        assert_eq!(active.norb, 1);
        assert_eq!(active.nelec, 2);
        assert!((active.ecore - (-0.8)).abs() < 1e-12);
    }

    #[test]
    fn freezing_a_doubly_occupied_orbital_preserves_total_symmetry() {
        let problem = ElectronicProblem::new(2, 4, 0, 0.0, vec![0.0; 4], vec![0.0; 16])
            .unwrap()
            .with_symmetry(vec![4, 1], 1)
            .unwrap();
        let active = freeze_core(&problem, &[0]).unwrap();
        assert_eq!(active.orbsym, vec![1]);
        assert_eq!(active.isym, 1);
    }

    #[test]
    fn removing_a_virtual_orbital_preserves_remaining_symmetry_labels() {
        let problem = ElectronicProblem::new(3, 2, 0, 0.0, vec![0.0; 9], vec![0.0; 81])
            .unwrap()
            .with_symmetry(vec![1, 2, 3], 1)
            .unwrap();
        let active = build_active_space(
            &problem,
            &ActiveSpaceSpec {
                frozen_occupied: Vec::new(),
                frozen_virtual: vec![2],
            },
        )
        .unwrap();
        assert_eq!(active.problem.orbsym, vec![1, 2]);
        assert_eq!(active.problem.isym, 1);
        assert_eq!(active.active_to_original, vec![0, 1]);
    }
}
