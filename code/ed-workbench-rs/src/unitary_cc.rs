use thiserror::Error;

use crate::amplitudes::Amplitudes;
use crate::cluster::{ClusterError, ClusterOperator};
use crate::determinant::{DeterminantBasis, DeterminantError};
use crate::direct_fci::DirectFciOperator;
use crate::excitation::{ExcitationError, ExcitationSpace, hartree_fock_reference};
use crate::operator::{LinearOperator, OperatorError};
use crate::optimizer::{BfgsConfig, OptimizationResult, minimize_bfgs};

#[derive(Debug, Error)]
pub enum UnitaryCcError {
    #[error(transparent)]
    Determinant(#[from] DeterminantError),
    #[error(transparent)]
    Excitation(#[from] ExcitationError),
    #[error(transparent)]
    Cluster(#[from] ClusterError),
    #[error(transparent)]
    Operator(#[from] OperatorError),
}

pub struct UnitaryCcModel<'a> {
    operator: &'a DirectFciOperator,
    basis: DeterminantBasis,
    space: ExcitationSpace,
}

impl<'a> UnitaryCcModel<'a> {
    pub fn new(operator: &'a DirectFciOperator, rank: usize) -> Result<Self, UnitaryCcError> {
        let problem = operator.problem();
        let basis = DeterminantBasis::from_problem(problem)?;
        let reference = hartree_fock_reference(problem.norb, basis.nalpha, basis.nbeta);
        let space = ExcitationSpace::new(&basis, reference, rank)?;
        Ok(Self {
            operator,
            basis,
            space,
        })
    }

    pub fn parameter_count(&self) -> usize {
        self.space.excitations.len()
    }

    pub fn energy(&self, parameters: &[f64]) -> Result<f64, UnitaryCcError> {
        let amplitudes = Amplitudes {
            values: parameters.to_vec(),
        };
        let cluster = ClusterOperator::new(&self.basis, &self.space, &amplitudes)?;
        let mut state = vec![0.0; self.basis.len()];
        state[self.space.reference_index] = 1.0;
        let mut term = state.clone();
        for order in 1..=64 {
            let mut forward = vec![0.0; self.basis.len()];
            let mut adjoint = vec![0.0; self.basis.len()];
            cluster.apply(&term, &mut forward)?;
            cluster.apply_adjoint(&term, &mut adjoint)?;
            for index in 0..term.len() {
                term[index] = (forward[index] - adjoint[index]) / order as f64;
                state[index] += term[index];
            }
            let term_norm = term.iter().map(|value| value * value).sum::<f64>().sqrt();
            if term_norm < 1e-14 {
                break;
            }
        }
        let norm = state.iter().map(|value| value * value).sum::<f64>();
        let mut h_state = vec![0.0; self.basis.len()];
        self.operator.apply(&state, &mut h_state)?;
        let numerator: f64 = state.iter().zip(&h_state).map(|(a, b)| a * b).sum();
        Ok(numerator / norm)
    }

    pub fn optimize(&self, config: &BfgsConfig) -> OptimizationResult {
        minimize_bfgs(&vec![0.0; self.parameter_count()], config, |parameters| {
            self.energy(parameters).unwrap_or(f64::INFINITY)
        })
    }
}
