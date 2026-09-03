use thiserror::Error;

use crate::determinant::{DeterminantBasis, apply_annihilation, apply_creation};

#[derive(Debug, Clone)]
pub struct Excitation {
    pub determinant_index: usize,
    pub rank: usize,
    pub holes: Vec<usize>,
    pub particles: Vec<usize>,
    reference_phase: f64,
}

#[derive(Debug, Error)]
pub enum ExcitationError {
    #[error("reference determinant is not in the basis")]
    MissingReference,
    #[error("failed to normalize excitation operator")]
    InvalidOperator,
}

impl Excitation {
    pub fn apply(&self, determinant: u64) -> Option<(u64, f64)> {
        let mut state = determinant;
        let mut sign = 1.0;
        for &hole in &self.holes {
            let (next, phase) = apply_annihilation(state, hole)?;
            state = next;
            sign *= phase;
        }
        for &particle in self.particles.iter().rev() {
            let (next, phase) = apply_creation(state, particle)?;
            state = next;
            sign *= phase;
        }
        Some((state, sign / self.reference_phase))
    }
}

#[derive(Debug, Clone)]
pub struct ExcitationSpace {
    pub reference: u64,
    pub reference_index: usize,
    pub max_rank: usize,
    pub excitations: Vec<Excitation>,
}

impl ExcitationSpace {
    pub fn new(
        basis: &DeterminantBasis,
        reference: u64,
        max_rank: usize,
    ) -> Result<Self, ExcitationError> {
        let reference_index = basis
            .address(reference)
            .ok_or(ExcitationError::MissingReference)?;
        let mut excitations = Vec::new();
        for (determinant_index, target) in basis.determinants().enumerate() {
            if target == reference {
                continue;
            }
            let holes_bits = reference & !target;
            let particles_bits = target & !reference;
            let rank = holes_bits.count_ones() as usize;
            if rank == 0 || rank > max_rank || rank != particles_bits.count_ones() as usize {
                continue;
            }
            let holes = bit_positions(holes_bits);
            let particles = bit_positions(particles_bits);
            let mut excitation = Excitation {
                determinant_index,
                rank,
                holes,
                particles,
                reference_phase: 1.0,
            };
            let (mapped, phase) = excitation
                .apply(reference)
                .ok_or(ExcitationError::InvalidOperator)?;
            if mapped != target {
                return Err(ExcitationError::InvalidOperator);
            }
            excitation.reference_phase = phase;
            excitations.push(excitation);
        }
        excitations.sort_by_key(|excitation| (excitation.rank, excitation.determinant_index));
        Ok(Self {
            reference,
            reference_index,
            max_rank,
            excitations,
        })
    }

    pub fn full_rank(basis: &DeterminantBasis, reference: u64) -> Result<Self, ExcitationError> {
        Self::new(basis, reference, basis.nalpha + basis.nbeta)
    }
}

pub fn hartree_fock_reference(norb: usize, nalpha: usize, nbeta: usize) -> u64 {
    let alpha = if nalpha == 0 {
        0
    } else {
        (1_u64 << nalpha) - 1
    };
    let beta = if nbeta == 0 {
        0
    } else {
        ((1_u64 << nbeta) - 1) << norb
    };
    alpha | beta
}

fn bit_positions(bits: u64) -> Vec<usize> {
    (0..64)
        .filter(|&position| bits & (1_u64 << position) != 0)
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn normalized_operators_map_reference_to_their_determinants() {
        let basis = DeterminantBasis::new(4, 4, 0).unwrap();
        let reference = hartree_fock_reference(4, 2, 2);
        let space = ExcitationSpace::full_rank(&basis, reference).unwrap();
        for excitation in &space.excitations {
            let (target, phase) = excitation.apply(reference).unwrap();
            assert_eq!(basis.address(target), Some(excitation.determinant_index));
            assert_eq!(phase, 1.0);
        }
    }
}
