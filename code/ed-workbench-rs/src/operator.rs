use thiserror::Error;

#[derive(Debug, Error)]
pub enum OperatorError {
    #[error("input length is {actual}, expected {expected}")]
    InputLength { actual: usize, expected: usize },
    #[error("output length is {actual}, expected {expected}")]
    OutputLength { actual: usize, expected: usize },
}

pub trait LinearOperator {
    fn dimension(&self) -> usize;
    fn diagonal(&self) -> &[f64];
    fn apply(&self, input: &[f64], output: &mut [f64]) -> Result<(), OperatorError>;
}
