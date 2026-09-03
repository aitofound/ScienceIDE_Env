use nalgebra::{DMatrix, DVector};
use thiserror::Error;

#[derive(Debug, Error)]
pub enum DiisError {
    #[error("vector and residual lengths differ")]
    LengthMismatch,
    #[error("DIIS vectors must have a positive dimension")]
    EmptyVector,
}

#[derive(Debug, Clone)]
pub struct Diis {
    max_history: usize,
    vectors: Vec<Vec<f64>>,
    residuals: Vec<Vec<f64>>,
}

impl Diis {
    pub fn new(max_history: usize) -> Self {
        Self {
            max_history: max_history.max(2),
            vectors: Vec::new(),
            residuals: Vec::new(),
        }
    }

    pub fn push(&mut self, vector: &[f64], residual: &[f64]) -> Result<(), DiisError> {
        if vector.len() != residual.len() {
            return Err(DiisError::LengthMismatch);
        }
        if vector.is_empty() {
            return Err(DiisError::EmptyVector);
        }
        self.vectors.push(vector.to_vec());
        self.residuals.push(residual.to_vec());
        if self.vectors.len() > self.max_history {
            self.vectors.remove(0);
            self.residuals.remove(0);
        }
        Ok(())
    }

    pub fn extrapolate(&self) -> Option<Vec<f64>> {
        let history = self.vectors.len();
        if history < 2 {
            return None;
        }
        let mut system = DMatrix::zeros(history + 1, history + 1);
        let mut rhs = DVector::zeros(history + 1);
        rhs[history] = -1.0;
        for i in 0..history {
            for j in 0..history {
                system[(i, j)] = dot(&self.residuals[i], &self.residuals[j]);
            }
            system[(i, history)] = -1.0;
            system[(history, i)] = -1.0;
        }
        let coefficients = system.lu().solve(&rhs)?;
        let mut output = vec![0.0; self.vectors[0].len()];
        for i in 0..history {
            for (value, source) in output.iter_mut().zip(&self.vectors[i]) {
                *value += coefficients[i] * source;
            }
        }
        output
            .iter()
            .all(|value| value.is_finite())
            .then_some(output)
    }
}

fn dot(left: &[f64], right: &[f64]) -> f64 {
    left.iter().zip(right).map(|(a, b)| a * b).sum()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn extrapolated_coefficients_cancel_linear_residuals() {
        let mut diis = Diis::new(4);
        diis.push(&[1.0, 0.0], &[1.0, 0.0]).unwrap();
        diis.push(&[0.0, 1.0], &[0.0, 1.0]).unwrap();
        let result = diis.extrapolate().unwrap();
        assert!((result[0] - 0.5).abs() < 1e-12);
        assert!((result[1] - 0.5).abs() < 1e-12);
    }
}
