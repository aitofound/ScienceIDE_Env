use nalgebra::{DMatrix, linalg::SymmetricEigen};
use thiserror::Error;

use crate::diis::{Diis, DiisError};
use crate::libcint_frontend::AoIntegrals;

#[derive(Debug, Clone)]
pub struct RhfConfig {
    pub energy_tolerance: f64,
    pub density_tolerance: f64,
    pub max_iterations: usize,
    pub diis_history: usize,
}

impl Default for RhfConfig {
    fn default() -> Self {
        Self {
            energy_tolerance: 1e-10,
            density_tolerance: 1e-8,
            max_iterations: 100,
            diis_history: 8,
        }
    }
}

#[derive(Debug, Clone)]
pub struct RhfResult {
    pub total_energy: f64,
    pub electronic_energy: f64,
    pub orbital_energies: Vec<f64>,
    pub coefficients: DMatrix<f64>,
    pub density: DMatrix<f64>,
    pub iterations: usize,
    pub density_rms: f64,
    pub converged: bool,
}

#[derive(Debug, Error)]
pub enum RhfError {
    #[error("RHF requires a positive even electron count not exceeding 2*NAO")]
    InvalidElectronCount,
    #[error("overlap matrix is not positive definite")]
    SingularOverlap,
    #[error(transparent)]
    Diis(#[from] DiisError),
}

pub fn solve_rhf(integrals: &AoIntegrals, config: &RhfConfig) -> Result<RhfResult, RhfError> {
    if integrals.nelec == 0
        || !integrals.nelec.is_multiple_of(2)
        || integrals.nelec > 2 * integrals.nao
    {
        return Err(RhfError::InvalidElectronCount);
    }
    let n = integrals.nao;
    let nocc = integrals.nelec / 2;
    let overlap = DMatrix::from_row_slice(n, n, &integrals.overlap);
    let hcore = DMatrix::from_row_slice(n, n, &integrals.hcore);
    let orthogonalizer = symmetric_orthogonalizer(&overlap)?;
    let (_, mut coefficients) = generalized_symmetric_eigen(&hcore, &orthogonalizer);
    let mut density = density_matrix(&coefficients, nocc);
    let mut previous_energy = f64::INFINITY;
    let mut diis = Diis::new(config.diis_history);
    let mut last = None;

    for iteration in 1..=config.max_iterations {
        let raw_fock = build_fock(integrals, &density);
        let error = &raw_fock * &density * &overlap - &overlap * &density * &raw_fock;
        diis.push(raw_fock.as_slice(), error.as_slice())?;
        let fock = diis
            .extrapolate()
            .map(|data| DMatrix::from_column_slice(n, n, &data))
            .unwrap_or(raw_fock);
        let (orbital_energies, new_coefficients) =
            generalized_symmetric_eigen(&fock, &orthogonalizer);
        let new_density = density_matrix(&new_coefficients, nocc);
        let energy_fock = build_fock(integrals, &new_density);
        let electronic_energy = 0.5 * new_density.component_mul(&(&hcore + energy_fock)).sum();
        let total_energy = electronic_energy + integrals.nuclear_repulsion;
        let density_rms = (&new_density - &density).norm() / n as f64;
        let energy_change = (total_energy - previous_energy).abs();
        let converged =
            energy_change <= config.energy_tolerance && density_rms <= config.density_tolerance;
        last = Some(RhfResult {
            total_energy,
            electronic_energy,
            orbital_energies,
            coefficients: new_coefficients.clone(),
            density: new_density.clone(),
            iterations: iteration,
            density_rms,
            converged,
        });
        coefficients = new_coefficients;
        density = new_density;
        previous_energy = total_energy;
        if converged {
            return Ok(last.expect("RHF iteration result exists"));
        }
    }
    let _ = coefficients;
    Ok(last.expect("positive max_iterations produces an RHF result"))
}

fn symmetric_orthogonalizer(overlap: &DMatrix<f64>) -> Result<DMatrix<f64>, RhfError> {
    let eigensystem = SymmetricEigen::new(overlap.clone());
    if eigensystem.eigenvalues.iter().any(|&value| value <= 1e-10) {
        return Err(RhfError::SingularOverlap);
    }
    let inverse_sqrt =
        DMatrix::from_diagonal(&eigensystem.eigenvalues.map(|value| 1.0 / value.sqrt()));
    Ok(&eigensystem.eigenvectors * inverse_sqrt * eigensystem.eigenvectors.transpose())
}

fn generalized_symmetric_eigen(
    matrix: &DMatrix<f64>,
    orthogonalizer: &DMatrix<f64>,
) -> (Vec<f64>, DMatrix<f64>) {
    let transformed = orthogonalizer.transpose() * matrix * orthogonalizer;
    let eigensystem = SymmetricEigen::new(transformed);
    let coefficients = orthogonalizer * eigensystem.eigenvectors;
    let mut order: Vec<usize> = (0..eigensystem.eigenvalues.len()).collect();
    order.sort_by(|&left, &right| {
        eigensystem.eigenvalues[left].total_cmp(&eigensystem.eigenvalues[right])
    });
    let orbital_energies = order
        .iter()
        .map(|&column| eigensystem.eigenvalues[column])
        .collect();
    let mut sorted_coefficients = DMatrix::zeros(coefficients.nrows(), coefficients.ncols());
    for (target, &source) in order.iter().enumerate() {
        sorted_coefficients
            .column_mut(target)
            .copy_from(&coefficients.column(source));
    }
    (orbital_energies, sorted_coefficients)
}

fn density_matrix(coefficients: &DMatrix<f64>, nocc: usize) -> DMatrix<f64> {
    let occupied = coefficients.columns(0, nocc);
    2.0 * occupied * occupied.transpose()
}

fn build_fock(integrals: &AoIntegrals, density: &DMatrix<f64>) -> DMatrix<f64> {
    let n = integrals.nao;
    let mut fock = DMatrix::from_row_slice(n, n, &integrals.hcore);
    for mu in 0..n {
        for nu in 0..n {
            let mut contribution = 0.0;
            for kappa in 0..n {
                for lambda in 0..n {
                    contribution += density[(kappa, lambda)]
                        * (integrals.eri(mu, nu, kappa, lambda)
                            - 0.5 * integrals.eri(mu, kappa, nu, lambda));
                }
            }
            fock[(mu, nu)] += contribution;
        }
    }
    fock
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn symmetric_orthogonalization_produces_identity_metric() {
        let overlap = DMatrix::from_row_slice(2, 2, &[1.0, 0.2, 0.2, 1.0]);
        let x = symmetric_orthogonalizer(&overlap).unwrap();
        let identity = x.transpose() * overlap * x;
        assert!((identity - DMatrix::identity(2, 2)).amax() < 1e-12);
    }

    #[test]
    fn generalized_eigenpairs_are_sorted_and_satisfy_the_metric_residual() {
        let overlap = DMatrix::from_row_slice(2, 2, &[1.0, 0.2, 0.2, 1.0]);
        let matrix = DMatrix::from_row_slice(2, 2, &[-0.7, -0.3, -0.3, -0.1]);
        let x = symmetric_orthogonalizer(&overlap).unwrap();
        let (energies, coefficients) = generalized_symmetric_eigen(&matrix, &x);
        assert!(energies[0] <= energies[1]);
        for (orbital, &energy) in energies.iter().enumerate() {
            let vector = coefficients.column(orbital);
            let residual = &matrix * vector - energy * &overlap * vector;
            assert!(residual.norm() < 1e-12);
        }
    }
}
