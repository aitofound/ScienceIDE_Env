use thiserror::Error;

use crate::determinant::{DeterminantBasis, DeterminantError};
use crate::direct_fci::DirectFciOperator;
use crate::excitation::hartree_fock_reference;
use crate::operator::{LinearOperator, OperatorError};

#[derive(Debug, Clone)]
pub struct MbptResult {
    pub reference_energy: f64,
    pub corrections: Vec<f64>,
    pub partial_sums: Vec<f64>,
}

#[derive(Debug, Error)]
pub enum MbptError {
    #[error("orbital energy length is {actual}, expected {expected}")]
    OrbitalEnergyLength { actual: usize, expected: usize },
    #[error("zero MBPT denominator for determinant {0}")]
    ZeroDenominator(usize),
    #[error(transparent)]
    Determinant(#[from] DeterminantError),
    #[error(transparent)]
    Operator(#[from] OperatorError),
}

pub fn solve_mbpt(
    operator: &DirectFciOperator,
    orbital_energies: &[f64],
    max_order: usize,
) -> Result<MbptResult, MbptError> {
    let problem = operator.problem();
    if orbital_energies.len() != problem.norb {
        return Err(MbptError::OrbitalEnergyLength {
            actual: orbital_energies.len(),
            expected: problem.norb,
        });
    }
    let basis = DeterminantBasis::from_problem(problem)?;
    let reference = hartree_fock_reference(problem.norb, basis.nalpha, basis.nbeta);
    let reference_index = basis
        .address(reference)
        .expect("Hartree-Fock determinant is in basis");
    let reference_energy = operator.diagonal()[reference_index];
    let mut h0 = vec![0.0; basis.len()];
    let mut denominators = vec![0.0; basis.len()];
    for (index, determinant) in basis.determinants().enumerate() {
        let holes = reference & !determinant;
        let particles = determinant & !reference;
        let hole_sum: f64 = bit_positions(holes)
            .map(|spin_orbital| orbital_energies[spin_orbital % problem.norb])
            .sum();
        let particle_sum: f64 = bit_positions(particles)
            .map(|spin_orbital| orbital_energies[spin_orbital % problem.norb])
            .sum();
        denominators[index] = hole_sum - particle_sum;
        h0[index] = reference_energy - denominators[index];
        if index != reference_index && denominators[index].abs() < 1e-12 {
            return Err(MbptError::ZeroDenominator(index));
        }
    }

    let mut wavefunctions = Vec::with_capacity(max_order + 1);
    let mut psi0 = vec![0.0; basis.len()];
    psi0[reference_index] = 1.0;
    wavefunctions.push(psi0);
    let mut corrections = Vec::with_capacity(max_order);
    let mut partial_sums = Vec::with_capacity(max_order);
    let mut running_energy = reference_energy;

    for order in 1..=max_order {
        let previous = &wavefunctions[order - 1];
        let mut h_previous = vec![0.0; basis.len()];
        operator.apply(previous, &mut h_previous)?;
        let v_previous: Vec<_> = h_previous
            .iter()
            .zip(&h0)
            .zip(previous)
            .map(|((&h_value, &h0_value), &coefficient)| h_value - h0_value * coefficient)
            .collect();
        let energy_correction = v_previous[reference_index];
        corrections.push(energy_correction);
        running_energy += energy_correction;
        partial_sums.push(running_energy);

        let mut next = vec![0.0; basis.len()];
        for determinant in 0..basis.len() {
            if determinant == reference_index {
                continue;
            }
            let folded: f64 = (1..order)
                .map(|k| corrections[k - 1] * wavefunctions[order - k][determinant])
                .sum();
            next[determinant] = (v_previous[determinant] - folded) / denominators[determinant];
        }
        wavefunctions.push(next);
    }
    Ok(MbptResult {
        reference_energy,
        corrections,
        partial_sums,
    })
}

fn bit_positions(bits: u64) -> impl Iterator<Item = usize> {
    (0..64).filter(move |&position| bits & (1_u64 << position) != 0)
}
