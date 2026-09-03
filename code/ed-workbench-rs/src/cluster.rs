use std::collections::HashMap;

use rayon::prelude::*;
use thiserror::Error;

use crate::amplitudes::Amplitudes;
use crate::determinant::{DeterminantBasis, apply_annihilation, apply_creation};
use crate::excitation::ExcitationSpace;

#[derive(Debug, Error)]
pub enum ClusterError {
    #[error("amplitude length is {actual}, expected {expected}")]
    AmplitudeLength { actual: usize, expected: usize },
    #[error("vector length is {actual}, expected {expected}")]
    VectorLength { actual: usize, expected: usize },
    #[error("reference string is not present in the determinant string space")]
    MissingReferenceString,
    #[error("a reference-relative string partition is outside the determinant string space")]
    MissingPartitionString,
    #[error("no retained amplitude corresponds to determinant index {determinant}")]
    MissingAmplitude { determinant: usize },
}

#[derive(Debug, Clone, Copy)]
struct SpinPartition {
    amplitude_string: usize,
    source_string: usize,
    rank: usize,
    phase: i8,
}

pub struct ClusterExpansionPlan<'a> {
    basis: &'a DeterminantBasis,
    space: &'a ExcitationSpace,
    amplitude_by_determinant: Vec<Option<usize>>,
    alpha_partitions: Vec<Vec<SpinPartition>>,
    beta_partitions: Vec<Vec<SpinPartition>>,
    targets_by_rank: Vec<Vec<usize>>,
}

impl<'a> ClusterExpansionPlan<'a> {
    pub fn new(
        basis: &'a DeterminantBasis,
        space: &'a ExcitationSpace,
    ) -> Result<Self, ClusterError> {
        let orbital_mask = if basis.norb == 64 {
            u64::MAX
        } else {
            (1_u64 << basis.norb) - 1
        };
        let alpha_reference = space.reference & orbital_mask;
        let beta_reference = space.reference >> basis.norb;
        let alpha_partitions = spin_partitions(&basis.alpha_strings, alpha_reference)?;
        let beta_partitions = spin_partitions(&basis.beta_strings, beta_reference)?;

        let mut amplitude_by_determinant = vec![None; basis.len()];
        for (amplitude_index, excitation) in space.excitations.iter().enumerate() {
            amplitude_by_determinant[excitation.determinant_index] = Some(amplitude_index);
        }

        let maximum_rank = basis.nalpha + basis.nbeta;
        let mut targets_by_rank = vec![Vec::new(); maximum_rank + 1];
        for (target, determinant) in basis.determinants().enumerate() {
            let rank = (space.reference & !determinant).count_ones() as usize;
            targets_by_rank[rank].push(target);
        }

        Ok(Self {
            basis,
            space,
            amplitude_by_determinant,
            alpha_partitions,
            beta_partitions,
            targets_by_rank,
        })
    }

    pub fn exponential_on_reference(
        &self,
        amplitudes: &Amplitudes,
    ) -> Result<Vec<f64>, ClusterError> {
        if amplitudes.values.len() != self.space.excitations.len() {
            return Err(ClusterError::AmplitudeLength {
                actual: amplitudes.values.len(),
                expected: self.space.excitations.len(),
            });
        }

        let mut wavefunction = vec![0.0; self.basis.len()];
        wavefunction[self.space.reference_index] = 1.0;

        for target_rank in 1..self.targets_by_rank.len() {
            let targets = &self.targets_by_rank[target_rank];
            let coefficients: Result<Vec<_>, ClusterError> = if targets.len() >= 1_024 {
                targets
                    .par_iter()
                    .map(|&target| {
                        self.target_coefficient(&wavefunction, amplitudes, target, target_rank)
                            .map(|coefficient| (target, coefficient))
                    })
                    .collect()
            } else {
                targets
                    .iter()
                    .map(|&target| {
                        self.target_coefficient(&wavefunction, amplitudes, target, target_rank)
                            .map(|coefficient| (target, coefficient))
                    })
                    .collect()
            };
            for (target, coefficient) in coefficients? {
                wavefunction[target] = coefficient;
            }
        }
        Ok(wavefunction)
    }

    fn target_coefficient(
        &self,
        wavefunction: &[f64],
        amplitudes: &Amplitudes,
        target: usize,
        target_rank: usize,
    ) -> Result<f64, ClusterError> {
        let (alpha_target, beta_target) = self
            .basis
            .string_pair(target)
            .expect("target address belongs to determinant basis");
        let mut coefficient = 0.0;
        for alpha in &self.alpha_partitions[alpha_target] {
            for beta in &self.beta_partitions[beta_target] {
                let amplitude_rank = alpha.rank + beta.rank;
                if amplitude_rank == 0 || amplitude_rank > self.space.max_rank {
                    continue;
                }
                let Some(amplitude_determinant) = self
                    .basis
                    .pair_address(alpha.amplitude_string, beta.amplitude_string)
                else {
                    continue;
                };
                let amplitude_index = self.amplitude_by_determinant[amplitude_determinant].ok_or(
                    ClusterError::MissingAmplitude {
                        determinant: amplitude_determinant,
                    },
                )?;
                let amplitude = amplitudes.values[amplitude_index];
                if amplitude == 0.0 {
                    continue;
                }
                let Some(source) = self
                    .basis
                    .pair_address(alpha.source_string, beta.source_string)
                else {
                    continue;
                };
                let phase = alpha.phase * beta.phase;
                coefficient +=
                    amplitude_rank as f64 * amplitude * wavefunction[source] * f64::from(phase);
            }
        }
        Ok(coefficient / target_rank as f64)
    }
}

fn spin_partitions(
    strings: &[u64],
    reference: u64,
) -> Result<Vec<Vec<SpinPartition>>, ClusterError> {
    let addresses: HashMap<u64, usize> = strings
        .iter()
        .enumerate()
        .map(|(index, &bits)| (bits, index))
        .collect();
    if !addresses.contains_key(&reference) {
        return Err(ClusterError::MissingReferenceString);
    }

    strings
        .iter()
        .map(|&target| {
            let holes = reference & !target;
            let particles = target & !reference;
            let mut partitions = Vec::new();
            let mut amplitude_holes = holes;
            loop {
                let rank = amplitude_holes.count_ones() as usize;
                let mut amplitude_particles = particles;
                loop {
                    if amplitude_particles.count_ones() as usize == rank {
                        let source_holes = holes & !amplitude_holes;
                        let source_particles = particles & !amplitude_particles;
                        let amplitude = (reference & !amplitude_holes) | amplitude_particles;
                        let source = (reference & !source_holes) | source_particles;
                        let amplitude_string = addresses
                            .get(&amplitude)
                            .copied()
                            .ok_or(ClusterError::MissingPartitionString)?;
                        let source_string = addresses
                            .get(&source)
                            .copied()
                            .ok_or(ClusterError::MissingPartitionString)?;
                        let (_, reference_phase) =
                            apply_spin_excitation(reference, amplitude_holes, amplitude_particles)
                                .ok_or(ClusterError::MissingPartitionString)?;
                        let (mapped, source_phase) =
                            apply_spin_excitation(source, amplitude_holes, amplitude_particles)
                                .ok_or(ClusterError::MissingPartitionString)?;
                        if mapped != target {
                            return Err(ClusterError::MissingPartitionString);
                        }
                        partitions.push(SpinPartition {
                            amplitude_string,
                            source_string,
                            rank,
                            phase: source_phase * reference_phase,
                        });
                    }
                    if amplitude_particles == 0 {
                        break;
                    }
                    amplitude_particles = (amplitude_particles - 1) & particles;
                }
                if amplitude_holes == 0 {
                    break;
                }
                amplitude_holes = (amplitude_holes - 1) & holes;
            }
            partitions.sort_by_key(|partition| {
                (
                    partition.rank,
                    partition.amplitude_string,
                    partition.source_string,
                )
            });
            Ok(partitions)
        })
        .collect()
}

fn apply_spin_excitation(determinant: u64, holes: u64, particles: u64) -> Option<(u64, i8)> {
    let mut state = determinant;
    let mut phase = 1_i8;
    let mut remaining_holes = holes;
    while remaining_holes != 0 {
        let orbital = remaining_holes.trailing_zeros() as usize;
        let (next, sign) = apply_annihilation(state, orbital)?;
        state = next;
        phase *= sign as i8;
        remaining_holes &= remaining_holes - 1;
    }
    let mut remaining_particles = particles;
    while remaining_particles != 0 {
        let orbital = (u64::BITS - 1 - remaining_particles.leading_zeros()) as usize;
        let (next, sign) = apply_creation(state, orbital)?;
        state = next;
        phase *= sign as i8;
        remaining_particles &= !(1_u64 << orbital);
    }
    Some((state, phase))
}

pub struct ClusterOperator<'a> {
    basis: &'a DeterminantBasis,
    space: &'a ExcitationSpace,
    amplitudes: &'a Amplitudes,
}

impl<'a> ClusterOperator<'a> {
    pub fn new(
        basis: &'a DeterminantBasis,
        space: &'a ExcitationSpace,
        amplitudes: &'a Amplitudes,
    ) -> Result<Self, ClusterError> {
        if amplitudes.values.len() != space.excitations.len() {
            return Err(ClusterError::AmplitudeLength {
                actual: amplitudes.values.len(),
                expected: space.excitations.len(),
            });
        }
        Ok(Self {
            basis,
            space,
            amplitudes,
        })
    }

    pub fn apply(&self, input: &[f64], output: &mut [f64]) -> Result<(), ClusterError> {
        if input.len() != self.basis.len() || output.len() != self.basis.len() {
            return Err(ClusterError::VectorLength {
                actual: input.len().max(output.len()),
                expected: self.basis.len(),
            });
        }
        output.fill(0.0);
        for (source_index, &coefficient) in input.iter().enumerate() {
            if coefficient == 0.0 {
                continue;
            }
            let source = self
                .basis
                .determinant(source_index)
                .expect("source address belongs to determinant basis");
            for (excitation, &amplitude) in
                self.space.excitations.iter().zip(&self.amplitudes.values)
            {
                if amplitude == 0.0 {
                    continue;
                }
                if let Some((target, phase)) = excitation.apply(source)
                    && let Some(target_index) = self.basis.address(target)
                {
                    output[target_index] += coefficient * amplitude * phase;
                }
            }
        }
        Ok(())
    }

    pub fn apply_adjoint(&self, input: &[f64], output: &mut [f64]) -> Result<(), ClusterError> {
        if input.len() != self.basis.len() || output.len() != self.basis.len() {
            return Err(ClusterError::VectorLength {
                actual: input.len().max(output.len()),
                expected: self.basis.len(),
            });
        }
        output.fill(0.0);
        let mut unit = vec![0.0; self.basis.len()];
        let mut column = vec![0.0; self.basis.len()];
        for source in 0..self.basis.len() {
            unit[source] = 1.0;
            self.apply(&unit, &mut column)?;
            for target in 0..self.basis.len() {
                output[source] += column[target] * input[target];
            }
            unit[source] = 0.0;
        }
        Ok(())
    }

    pub fn exponential_on_reference(&self, threshold: f64) -> Result<Vec<f64>, ClusterError> {
        let mut result = vec![0.0; self.basis.len()];
        result[self.space.reference_index] = 1.0;
        let mut term = result.clone();
        for order in 1..=self.basis.nalpha + self.basis.nbeta {
            let mut next = vec![0.0; self.basis.len()];
            self.apply(&term, &mut next)?;
            for value in &mut next {
                *value /= order as f64;
            }
            let next_norm = next.iter().map(|value| value * value).sum::<f64>().sqrt();
            for (result_value, next_value) in result.iter_mut().zip(&next) {
                *result_value += next_value;
            }
            term = next;
            if next_norm < threshold {
                break;
            }
        }
        Ok(result)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::excitation::hartree_fock_reference;

    #[test]
    fn cluster_maps_reference_amplitudes_to_target_coefficients() {
        let basis = DeterminantBasis::new(2, 2, 0).unwrap();
        let reference = hartree_fock_reference(2, 1, 1);
        let space = ExcitationSpace::new(&basis, reference, 2).unwrap();
        let amplitudes = Amplitudes {
            values: (1..=space.excitations.len())
                .map(|index| index as f64 * 0.1)
                .collect(),
        };
        let cluster = ClusterOperator::new(&basis, &space, &amplitudes).unwrap();
        let mut input = vec![0.0; basis.len()];
        input[space.reference_index] = 1.0;
        let mut output = vec![0.0; basis.len()];
        cluster.apply(&input, &mut output).unwrap();
        for (excitation, amplitude) in space.excitations.iter().zip(&amplitudes.values) {
            assert!((output[excitation.determinant_index] - amplitude).abs() < 1e-12);
        }
    }

    #[test]
    fn ranked_expansion_matches_taylor_coefficients_at_every_supported_rank() {
        for (norb, nelec) in [(2, 2), (4, 4)] {
            let basis = DeterminantBasis::new(norb, nelec, 0).unwrap();
            let reference = hartree_fock_reference(norb, basis.nalpha, basis.nbeta);
            for max_rank in 1..=basis.nalpha + basis.nbeta {
                let space = ExcitationSpace::new(&basis, reference, max_rank).unwrap();
                let amplitudes = Amplitudes {
                    values: (0..space.excitations.len())
                        .map(|index| (index as f64 + 1.0) * 1e-4)
                        .collect(),
                };
                let expected = ClusterOperator::new(&basis, &space, &amplitudes)
                    .unwrap()
                    .exponential_on_reference(1e-15)
                    .unwrap();
                let actual = ClusterExpansionPlan::new(&basis, &space)
                    .unwrap()
                    .exponential_on_reference(&amplitudes)
                    .unwrap();
                assert!(
                    max_error(&actual, &expected) < 1e-12,
                    "norb={norb}, rank={max_rank}, error={}",
                    max_error(&actual, &expected)
                );
            }
        }
    }

    #[test]
    fn ranked_expansion_matches_taylor_in_a_compact_symmetry_sector() {
        let basis = DeterminantBasis::with_symmetry(4, 4, 0, &[1, 2, 1, 2], 1).unwrap();
        assert!(basis.len() < basis.alpha_strings.len() * basis.beta_strings.len());
        let reference = hartree_fock_reference(4, basis.nalpha, basis.nbeta);
        for max_rank in 1..=basis.nalpha + basis.nbeta {
            let space = ExcitationSpace::new(&basis, reference, max_rank).unwrap();
            let amplitudes = Amplitudes {
                values: (0..space.excitations.len())
                    .map(|index| (index as f64 + 1.0) * 1e-4)
                    .collect(),
            };
            let expected = ClusterOperator::new(&basis, &space, &amplitudes)
                .unwrap()
                .exponential_on_reference(1e-15)
                .unwrap();
            let actual = ClusterExpansionPlan::new(&basis, &space)
                .unwrap()
                .exponential_on_reference(&amplitudes)
                .unwrap();
            assert!(
                max_error(&actual, &expected) < 1e-12,
                "rank={max_rank}, error={}",
                max_error(&actual, &expected)
            );
        }
    }

    #[test]
    fn factored_spin_partition_phases_match_direct_excitation_application() {
        let basis = DeterminantBasis::new(4, 4, 0).unwrap();
        let reference = hartree_fock_reference(4, basis.nalpha, basis.nbeta);
        let space = ExcitationSpace::full_rank(&basis, reference).unwrap();
        let alpha_reference = reference & ((1_u64 << basis.norb) - 1);
        let beta_reference = reference >> basis.norb;
        let alpha_partitions = spin_partitions(&basis.alpha_strings, alpha_reference).unwrap();
        let beta_partitions = spin_partitions(&basis.beta_strings, beta_reference).unwrap();
        let mut amplitude_by_determinant = vec![None; basis.len()];
        for (amplitude_index, excitation) in space.excitations.iter().enumerate() {
            amplitude_by_determinant[excitation.determinant_index] = Some(amplitude_index);
        }
        let beta_count = basis.beta_strings.len();
        for target in 0..basis.len() {
            let alpha_target = target / beta_count;
            let beta_target = target % beta_count;
            for alpha in &alpha_partitions[alpha_target] {
                for beta in &beta_partitions[beta_target] {
                    if alpha.rank + beta.rank == 0 {
                        continue;
                    }
                    let amplitude_determinant =
                        alpha.amplitude_string * beta_count + beta.amplitude_string;
                    let source = alpha.source_string * beta_count + beta.source_string;
                    let amplitude_index = amplitude_by_determinant[amplitude_determinant].unwrap();
                    let excitation = &space.excitations[amplitude_index];
                    let (mapped, direct_phase) = excitation
                        .apply(basis.determinant(source).unwrap())
                        .unwrap();
                    assert_eq!(basis.address(mapped), Some(target));
                    assert_eq!(direct_phase, f64::from(alpha.phase) * f64::from(beta.phase));
                }
            }
        }
    }

    fn max_error(left: &[f64], right: &[f64]) -> f64 {
        left.iter()
            .zip(right)
            .map(|(a, b)| (a - b).abs())
            .fold(0.0_f64, f64::max)
    }
}
