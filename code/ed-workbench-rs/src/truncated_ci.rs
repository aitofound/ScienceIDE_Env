use std::time::{Duration, Instant};

use thiserror::Error;

use crate::davidson::{DavidsonConfig, DavidsonError, DavidsonResult, lowest_eigenpair};
use crate::determinant::{DeterminantBasis, DeterminantError};
use crate::direct_fci::DirectFciOperator;
use crate::excitation::hartree_fock_reference;
use crate::operator::{LinearOperator, OperatorError};

pub struct ProjectedOperator<'a> {
    full: &'a DirectFciOperator,
    selected: Vec<usize>,
    diagonal: Vec<f64>,
}

impl<'a> ProjectedOperator<'a> {
    pub fn through_rank(
        full: &'a DirectFciOperator,
        rank: usize,
    ) -> Result<Self, DeterminantError> {
        let problem = full.problem();
        let basis = DeterminantBasis::from_problem(problem)?;
        let reference = hartree_fock_reference(problem.norb, basis.nalpha, basis.nbeta);
        let selected: Vec<_> = basis
            .determinants()
            .enumerate()
            .filter(|(_, determinant)| (reference & !*determinant).count_ones() as usize <= rank)
            .map(|(index, _)| index)
            .collect();
        let diagonal = selected
            .iter()
            .map(|&index| full.diagonal()[index])
            .collect();
        Ok(Self {
            full,
            selected,
            diagonal,
        })
    }

    pub fn selected_indices(&self) -> &[usize] {
        &self.selected
    }
}

impl LinearOperator for ProjectedOperator<'_> {
    fn dimension(&self) -> usize {
        self.selected.len()
    }

    fn diagonal(&self) -> &[f64] {
        &self.diagonal
    }

    fn apply(&self, input: &[f64], output: &mut [f64]) -> Result<(), OperatorError> {
        if input.len() != self.dimension() {
            return Err(OperatorError::InputLength {
                actual: input.len(),
                expected: self.dimension(),
            });
        }
        if output.len() != self.dimension() {
            return Err(OperatorError::OutputLength {
                actual: output.len(),
                expected: self.dimension(),
            });
        }
        let mut full_input = vec![0.0; self.full.dimension()];
        for (&index, &value) in self.selected.iter().zip(input) {
            full_input[index] = value;
        }
        let mut full_output = vec![0.0; self.full.dimension()];
        self.full.apply(&full_input, &mut full_output)?;
        for (value, &index) in output.iter_mut().zip(&self.selected) {
            *value = full_output[index];
        }
        Ok(())
    }
}

#[derive(Debug, Error)]
pub enum TruncatedCiError {
    #[error("CI rank {requested} is outside 1..={maximum}")]
    InvalidRank { requested: usize, maximum: usize },
    #[error(transparent)]
    Determinant(#[from] DeterminantError),
    #[error(transparent)]
    Davidson(#[from] DavidsonError),
}

#[derive(Debug, Clone)]
pub struct CiSeriesEntry {
    pub rank: usize,
    pub dimension: usize,
    pub result: DavidsonResult,
    pub elapsed: Duration,
}

pub fn solve_ci(
    full: &DirectFciOperator,
    rank: usize,
    config: &DavidsonConfig,
) -> Result<DavidsonResult, TruncatedCiError> {
    validate_rank(full, rank)?;
    let projected = ProjectedOperator::through_rank(full, rank)?;
    let mut initial = vec![0.0; projected.dimension()];
    let index = projected
        .diagonal()
        .iter()
        .enumerate()
        .min_by(|(_, left), (_, right)| left.total_cmp(right))
        .expect("non-empty CI space")
        .0;
    initial[index] = 1.0;
    Ok(lowest_eigenpair(&projected, &initial, config)?)
}

pub fn solve_ci_series(
    full: &DirectFciOperator,
    max_rank: usize,
    config: &DavidsonConfig,
) -> Result<Vec<CiSeriesEntry>, TruncatedCiError> {
    validate_rank(full, max_rank)?;
    let problem = full.problem();
    let basis = DeterminantBasis::from_problem(problem)?;
    let reference = hartree_fock_reference(problem.norb, basis.nalpha, basis.nbeta);
    let reference_index = basis
        .address(reference)
        .expect("Hartree-Fock reference is in the determinant basis");
    let mut previous_full = vec![0.0; basis.len()];
    previous_full[reference_index] = 1.0;
    let mut series = Vec::with_capacity(max_rank);

    for rank in 1..=max_rank {
        let projected = ProjectedOperator::through_rank(full, rank)?;
        let initial: Vec<f64> = projected
            .selected_indices()
            .iter()
            .map(|&index| previous_full[index])
            .collect();
        let started = Instant::now();
        let result = lowest_eigenpair(&projected, &initial, config)?;
        let elapsed = started.elapsed();
        previous_full.fill(0.0);
        for (&index, &coefficient) in projected.selected_indices().iter().zip(&result.eigenvector) {
            previous_full[index] = coefficient;
        }
        let converged = result.converged;
        series.push(CiSeriesEntry {
            rank,
            dimension: projected.dimension(),
            result,
            elapsed,
        });
        if !converged {
            break;
        }
    }
    Ok(series)
}

fn validate_rank(full: &DirectFciOperator, rank: usize) -> Result<(), TruncatedCiError> {
    let maximum = full.problem().nelec;
    if rank == 0 || rank > maximum {
        return Err(TruncatedCiError::InvalidRank {
            requested: rank,
            maximum,
        });
    }
    Ok(())
}
