use nalgebra::{DMatrix, DVector};

#[derive(Debug, Clone)]
pub struct BfgsConfig {
    pub gradient_tolerance: f64,
    pub max_iterations: usize,
    pub finite_difference_step: f64,
}

impl Default for BfgsConfig {
    fn default() -> Self {
        Self {
            gradient_tolerance: 1e-7,
            max_iterations: 100,
            finite_difference_step: 1e-5,
        }
    }
}

#[derive(Debug, Clone)]
pub struct OptimizationResult {
    pub parameters: Vec<f64>,
    pub value: f64,
    pub gradient_norm: f64,
    pub iterations: usize,
    pub converged: bool,
}

pub fn minimize_bfgs(
    initial: &[f64],
    config: &BfgsConfig,
    objective: impl Fn(&[f64]) -> f64,
) -> OptimizationResult {
    let dimension = initial.len();
    let mut x = DVector::from_column_slice(initial);
    let mut value = objective(x.as_slice());
    let mut gradient = numerical_gradient(x.as_slice(), config.finite_difference_step, &objective);
    let mut inverse_hessian = DMatrix::identity(dimension, dimension);

    for iteration in 0..=config.max_iterations {
        let gradient_norm = gradient.norm();
        if gradient_norm <= config.gradient_tolerance {
            return OptimizationResult {
                parameters: x.as_slice().to_vec(),
                value,
                gradient_norm,
                iterations: iteration,
                converged: true,
            };
        }
        if iteration == config.max_iterations {
            return OptimizationResult {
                parameters: x.as_slice().to_vec(),
                value,
                gradient_norm,
                iterations: iteration,
                converged: false,
            };
        }
        let direction = -&inverse_hessian * &gradient;
        let directional_derivative = gradient.dot(&direction);
        let mut step = 1.0;
        let candidate = loop {
            let trial = &x + step * &direction;
            let trial_value = objective(trial.as_slice());
            if trial_value <= value + 1e-4 * step * directional_derivative || step < 1e-8 {
                break (trial, trial_value);
            }
            step *= 0.5;
        };
        let new_gradient = numerical_gradient(
            candidate.0.as_slice(),
            config.finite_difference_step,
            &objective,
        );
        let s = &candidate.0 - &x;
        let y = &new_gradient - &gradient;
        let ys = y.dot(&s);
        if ys > 1e-12 {
            let rho = 1.0 / ys;
            let identity = DMatrix::identity(dimension, dimension);
            inverse_hessian = (&identity - rho * &s * y.transpose())
                * inverse_hessian
                * (&identity - rho * &y * s.transpose())
                + rho * &s * s.transpose();
        } else {
            inverse_hessian = DMatrix::identity(dimension, dimension);
        }
        x = candidate.0;
        value = candidate.1;
        gradient = new_gradient;
    }
    unreachable!()
}

fn numerical_gradient(
    point: &[f64],
    step: f64,
    objective: &impl Fn(&[f64]) -> f64,
) -> DVector<f64> {
    let mut gradient = DVector::zeros(point.len());
    let mut trial = point.to_vec();
    for index in 0..point.len() {
        trial[index] += step;
        let plus = objective(&trial);
        trial[index] -= 2.0 * step;
        let minus = objective(&trial);
        trial[index] = point[index];
        gradient[index] = (plus - minus) / (2.0 * step);
    }
    gradient
}
