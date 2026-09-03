use nalgebra::linalg::SymmetricEigen;
use thiserror::Error;

use crate::determinant::{DeterminantBasis, DeterminantError};
use crate::fcidump::Fcidump;
use crate::hamiltonian::build_dense_hamiltonian;

#[derive(Debug, Error)]
pub enum DenseFciError {
    #[error(transparent)]
    Determinant(#[from] DeterminantError),
    #[error("Hamiltonian is not symmetric; maximum asymmetry is {0:e}")]
    NonSymmetric(f64),
    #[error("Hamiltonian has no eigenvalues")]
    EmptySpectrum,
}

pub fn ground_state_energy(dump: &Fcidump) -> Result<f64, DenseFciError> {
    let basis =
        DeterminantBasis::with_symmetry(dump.norb, dump.nelec, dump.ms2, &dump.orbsym, dump.isym)?;
    let matrix = build_dense_hamiltonian(dump, &basis);
    let asymmetry = (&matrix - matrix.transpose()).amax();
    if asymmetry > 1e-10 {
        return Err(DenseFciError::NonSymmetric(asymmetry));
    }
    let eigensystem = SymmetricEigen::new(matrix);
    eigensystem
        .eigenvalues
        .iter()
        .copied()
        .reduce(f64::min)
        .ok_or(DenseFciError::EmptySpectrum)
}
