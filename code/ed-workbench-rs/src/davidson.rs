use std::fs;
use std::path::{Path, PathBuf};

use nalgebra::{DMatrix, linalg::SymmetricEigen};
use serde::{Deserialize, Serialize};
use thiserror::Error;

use crate::operator::{LinearOperator, OperatorError};

mod checkpoint;
mod storage;

use checkpoint::{
    CHECKPOINT_SCHEMA_VERSION, CheckpointWrite, DavidsonCheckpoint, load_checkpoint,
    load_result_vector, save_checkpoint,
};
use storage::{DiskVectorStore, MemoryVectorStore, VectorStore, io_error};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DavidsonConfig {
    pub residual_tolerance: f64,
    pub energy_tolerance: f64,
    pub max_iterations: usize,
    pub max_subspace: usize,
}

impl Default for DavidsonConfig {
    fn default() -> Self {
        Self {
            residual_tolerance: 1e-10,
            energy_tolerance: 1e-12,
            max_iterations: 100,
            max_subspace: 24,
        }
    }
}

#[derive(Debug, Clone)]
pub struct DavidsonWorkspaceConfig {
    pub path: PathBuf,
    pub resume: bool,
    pub checkpoint_every: usize,
    pub operator_fingerprint: String,
}

#[derive(Debug, Clone)]
pub struct DavidsonRunConfig {
    pub algorithm: DavidsonConfig,
    pub workspace: Option<DavidsonWorkspaceConfig>,
}

impl DavidsonRunConfig {
    pub fn in_memory(algorithm: DavidsonConfig) -> Self {
        Self {
            algorithm,
            workspace: None,
        }
    }
}

#[derive(Debug, Clone)]
pub struct DavidsonResult {
    pub energy: f64,
    pub eigenvector: Vec<f64>,
    pub residual_norm: f64,
    pub iterations: usize,
    pub converged: bool,
}

#[derive(Debug, Error)]
pub enum DavidsonError {
    #[error("operator dimension is zero")]
    EmptyOperator,
    #[error("initial vector length is {actual}, expected {expected}")]
    InitialLength { actual: usize, expected: usize },
    #[error("initial vector has zero or non-finite norm")]
    InvalidInitialVector,
    #[error("max_subspace must be at least 2")]
    InvalidSubspace,
    #[error("requested {requested} roots from an operator of dimension {dimension}")]
    InvalidRootCount { requested: usize, dimension: usize },
    #[error(
        "multi-root Davidson needs max_subspace >= 2 * roots, got {max_subspace} for {roots} roots"
    )]
    RootSubspace { roots: usize, max_subspace: usize },
    #[error("max_iterations must be at least 1")]
    InvalidIterations,
    #[error("{field} must be finite and positive")]
    InvalidTolerance { field: &'static str },
    #[error("checkpoint_every must be at least 1")]
    InvalidCheckpointCadence,
    #[error("a disk workspace requires a nonempty operator fingerprint")]
    EmptyOperatorFingerprint,
    #[error("workspace {path} is not empty; use resume or a new directory")]
    WorkspaceNotEmpty { path: PathBuf },
    #[error("workspace I/O failed at {path}: {source}")]
    WorkspaceIo {
        path: PathBuf,
        #[source]
        source: std::io::Error,
    },
    #[error("stored vector length is {actual}, expected {expected}")]
    StoredVectorLength { actual: usize, expected: usize },
    #[error("stored vector contains a non-finite value")]
    InvalidStoredVector,
    #[error("vector index {index} is outside a store with {count} vectors")]
    VectorIndex { index: usize, count: usize },
    #[error("invalid vector file {path}: {reason}")]
    InvalidVectorFile { path: PathBuf, reason: String },
    #[error("invalid checkpoint {path}: {reason}")]
    InvalidCheckpoint { path: PathBuf, reason: String },
    #[error("checkpoint field {field} is incompatible: expected {expected}, got {actual}")]
    CheckpointMismatch {
        field: &'static str,
        expected: String,
        actual: String,
    },
    #[error("Davidson state at iteration {iteration} contains non-finite {quantity}")]
    NonFiniteState {
        iteration: usize,
        quantity: &'static str,
    },
    #[error(transparent)]
    Operator(#[from] OperatorError),
}

struct WorkspaceRuntime {
    path: PathBuf,
    checkpoint_every: usize,
    operator_fingerprint: String,
}

struct InitialState {
    basis: Box<dyn VectorStore>,
    sigma_basis: Box<dyn VectorStore>,
    previous_energy: Option<f64>,
    last_result: DavidsonResult,
    completed_iterations: usize,
    runtime: Option<WorkspaceRuntime>,
}

pub fn lowest_eigenpair(
    operator: &impl LinearOperator,
    initial: &[f64],
    config: &DavidsonConfig,
) -> Result<DavidsonResult, DavidsonError> {
    lowest_eigenpair_with_run_config(
        operator,
        initial,
        &DavidsonRunConfig::in_memory(config.clone()),
    )
}

pub fn lowest_eigenpair_with_run_config(
    operator: &impl LinearOperator,
    initial: &[f64],
    run: &DavidsonRunConfig,
) -> Result<DavidsonResult, DavidsonError> {
    let dimension = validate_inputs(operator, initial, &run.algorithm)?;
    let mut state = initialize_state(operator, initial, run, dimension)?;
    if state.last_result.converged || state.completed_iterations >= run.algorithm.max_iterations {
        return Ok(state.last_result);
    }

    for iteration in (state.completed_iterations + 1)..=run.algorithm.max_iterations {
        let subspace = state.basis.len();
        let projected =
            projected_matrix(state.basis.as_ref(), state.sigma_basis.as_ref(), dimension)?;
        let eigensystem = SymmetricEigen::new(projected);
        let (root, &energy) = eigensystem
            .eigenvalues
            .iter()
            .enumerate()
            .min_by(|(_, a), (_, b)| a.total_cmp(b))
            .expect("non-empty Davidson projected spectrum");
        let coefficients = eigensystem.eigenvectors.column(root);
        let mut eigenvector =
            linear_combination(state.basis.as_ref(), coefficients.as_slice(), dimension)?;
        let sigma = linear_combination(
            state.sigma_basis.as_ref(),
            coefficients.as_slice(),
            dimension,
        )?;
        let mut residual = sigma;
        axpy(-energy, &eigenvector, &mut residual);
        let residual_norm = norm(&residual);
        let energy_change = state
            .previous_energy
            .map_or(f64::INFINITY, |previous| (energy - previous).abs());
        ensure_finite(iteration, "energy", std::iter::once(energy))?;
        ensure_finite(iteration, "residual", residual.iter().copied())?;

        state.last_result = DavidsonResult {
            energy,
            eigenvector: eigenvector.clone(),
            residual_norm,
            iterations: iteration,
            converged: residual_norm <= run.algorithm.residual_tolerance
                && energy_change <= run.algorithm.energy_tolerance,
        };
        if state.last_result.converged
            || (residual_norm <= run.algorithm.residual_tolerance && subspace == dimension)
        {
            state.last_result.converged = true;
            save_runtime_checkpoint(&state, &run.algorithm, iteration, Some(energy), true)?;
            return Ok(state.last_result);
        }
        state.previous_energy = Some(energy);

        let mut correction = residual;
        for (index, value) in correction.iter_mut().enumerate() {
            let denominator = energy - operator.diagonal()[index];
            if denominator.abs() > 1e-12 {
                *value /= denominator;
            }
        }
        orthogonalize_store(&mut correction, state.basis.as_ref())?;
        if norm(&correction) < 1e-12 {
            correction = coordinate_fallback(dimension, state.basis.as_ref())?;
        }
        if norm(&correction) < 1e-12 && state.basis.len() == dimension {
            state.last_result.converged =
                state.last_result.residual_norm <= run.algorithm.residual_tolerance;
            save_runtime_checkpoint(&state, &run.algorithm, iteration, Some(energy), true)?;
            return Ok(state.last_result);
        }
        normalize(&mut correction)?;
        let mut correction_sigma = vec![0.0; dimension];
        operator.apply(&correction, &mut correction_sigma)?;
        ensure_finite(iteration, "sigma vector", correction_sigma.iter().copied())?;

        if state.basis.len() >= run.algorithm.max_subspace {
            normalize(&mut eigenvector)?;
            let mut restarted_sigma = vec![0.0; dimension];
            operator.apply(&eigenvector, &mut restarted_sigma)?;
            let mut basis_vectors = vec![eigenvector, correction];
            let mut sigma_vectors = vec![restarted_sigma, correction_sigma];
            orthonormalize_last(&mut basis_vectors, &mut sigma_vectors);
            state.basis.replace_all(&basis_vectors)?;
            state.sigma_basis.replace_all(&sigma_vectors)?;
        } else {
            state.basis.push(&correction)?;
            state.sigma_basis.push(&correction_sigma)?;
        }

        let final_iteration = iteration == run.algorithm.max_iterations;
        save_runtime_checkpoint(
            &state,
            &run.algorithm,
            iteration,
            Some(energy),
            final_iteration,
        )?;
    }
    Ok(state.last_result)
}

fn validate_inputs(
    operator: &impl LinearOperator,
    initial: &[f64],
    config: &DavidsonConfig,
) -> Result<usize, DavidsonError> {
    let dimension = operator.dimension();
    if dimension == 0 {
        return Err(DavidsonError::EmptyOperator);
    }
    if initial.len() != dimension {
        return Err(DavidsonError::InitialLength {
            actual: initial.len(),
            expected: dimension,
        });
    }
    if config.max_subspace < 2 {
        return Err(DavidsonError::InvalidSubspace);
    }
    if config.max_iterations == 0 {
        return Err(DavidsonError::InvalidIterations);
    }
    for (field, tolerance) in [
        ("residual_tolerance", config.residual_tolerance),
        ("energy_tolerance", config.energy_tolerance),
    ] {
        if !tolerance.is_finite() || tolerance <= 0.0 {
            return Err(DavidsonError::InvalidTolerance { field });
        }
    }
    Ok(dimension)
}

fn initialize_state(
    operator: &impl LinearOperator,
    initial: &[f64],
    run: &DavidsonRunConfig,
    dimension: usize,
) -> Result<InitialState, DavidsonError> {
    let Some(workspace) = &run.workspace else {
        return fresh_state(
            operator,
            initial,
            Box::new(MemoryVectorStore::new(dimension)),
            Box::new(MemoryVectorStore::new(dimension)),
            None,
        );
    };
    if workspace.checkpoint_every == 0 {
        return Err(DavidsonError::InvalidCheckpointCadence);
    }
    if workspace.operator_fingerprint.trim().is_empty() {
        return Err(DavidsonError::EmptyOperatorFingerprint);
    }
    if workspace.resume {
        resume_state(workspace, &run.algorithm, dimension)
    } else {
        prepare_fresh_workspace(&workspace.path)?;
        let basis = DiskVectorStore::create(workspace.path.join("basis"), dimension)?;
        let sigma = DiskVectorStore::create(workspace.path.join("sigma"), dimension)?;
        fresh_state(
            operator,
            initial,
            Box::new(basis),
            Box::new(sigma),
            Some(WorkspaceRuntime {
                path: workspace.path.clone(),
                checkpoint_every: workspace.checkpoint_every,
                operator_fingerprint: workspace.operator_fingerprint.clone(),
            }),
        )
    }
}

fn fresh_state(
    operator: &impl LinearOperator,
    initial: &[f64],
    mut basis: Box<dyn VectorStore>,
    mut sigma_basis: Box<dyn VectorStore>,
    runtime: Option<WorkspaceRuntime>,
) -> Result<InitialState, DavidsonError> {
    let dimension = operator.dimension();
    let mut first = initial.to_vec();
    normalize(&mut first)?;
    let mut first_sigma = vec![0.0; dimension];
    operator.apply(&first, &mut first_sigma)?;
    ensure_finite(0, "initial sigma vector", first_sigma.iter().copied())?;
    basis.push(&first)?;
    sigma_basis.push(&first_sigma)?;
    Ok(InitialState {
        basis,
        sigma_basis,
        previous_energy: None,
        last_result: DavidsonResult {
            energy: f64::NAN,
            eigenvector: vec![0.0; dimension],
            residual_norm: f64::INFINITY,
            iterations: 0,
            converged: false,
        },
        completed_iterations: 0,
        runtime,
    })
}

fn resume_state(
    workspace: &DavidsonWorkspaceConfig,
    config: &DavidsonConfig,
    dimension: usize,
) -> Result<InitialState, DavidsonError> {
    let checkpoint = load_checkpoint(&workspace.path)?;
    validate_checkpoint(
        &checkpoint,
        config,
        dimension,
        &workspace.operator_fingerprint,
    )?;
    let basis = DiskVectorStore::open(
        workspace.path.join("basis"),
        dimension,
        checkpoint.basis_generation,
        checkpoint.basis_count,
    )?;
    let sigma = DiskVectorStore::open(
        workspace.path.join("sigma"),
        dimension,
        checkpoint.sigma_generation,
        checkpoint.sigma_count,
    )?;
    if basis.len() != sigma.len() {
        return Err(DavidsonError::CheckpointMismatch {
            field: "basis_sigma_count",
            expected: basis.len().to_string(),
            actual: sigma.len().to_string(),
        });
    }
    let eigenvector = load_result_vector(&workspace.path, &checkpoint)?;
    Ok(InitialState {
        basis: Box::new(basis),
        sigma_basis: Box::new(sigma),
        previous_energy: checkpoint.previous_energy,
        last_result: DavidsonResult {
            energy: checkpoint.last_energy,
            eigenvector,
            residual_norm: checkpoint.last_residual_norm,
            iterations: checkpoint.completed_iterations,
            converged: checkpoint.last_converged,
        },
        completed_iterations: checkpoint.completed_iterations,
        runtime: Some(WorkspaceRuntime {
            path: workspace.path.clone(),
            checkpoint_every: workspace.checkpoint_every,
            operator_fingerprint: workspace.operator_fingerprint.clone(),
        }),
    })
}

fn validate_checkpoint(
    checkpoint: &DavidsonCheckpoint,
    config: &DavidsonConfig,
    dimension: usize,
    fingerprint: &str,
) -> Result<(), DavidsonError> {
    check_field(
        "schema_version",
        CHECKPOINT_SCHEMA_VERSION,
        checkpoint.schema_version,
    )?;
    check_field(
        "operator_fingerprint",
        fingerprint,
        checkpoint.operator_fingerprint.as_str(),
    )?;
    check_field("dimension", dimension, checkpoint.dimension)?;
    check_field(
        "residual_tolerance",
        config.residual_tolerance.to_bits(),
        checkpoint.residual_tolerance.to_bits(),
    )?;
    check_field(
        "energy_tolerance",
        config.energy_tolerance.to_bits(),
        checkpoint.energy_tolerance.to_bits(),
    )?;
    check_field("max_subspace", config.max_subspace, checkpoint.max_subspace)?;
    check_field("scalar_type", "f64", checkpoint.scalar_type.as_str())?;
    check_field("byte_order", "little", checkpoint.byte_order.as_str())?;
    if checkpoint.basis_count == 0 || checkpoint.basis_count != checkpoint.sigma_count {
        return Err(DavidsonError::InvalidCheckpoint {
            path: PathBuf::from("checkpoint.json"),
            reason: "basis and sigma counts must be equal and nonzero".to_string(),
        });
    }
    ensure_finite(
        checkpoint.completed_iterations,
        "checkpoint scalars",
        [
            checkpoint.last_energy,
            checkpoint.last_residual_norm,
            checkpoint.previous_energy.unwrap_or(0.0),
        ]
        .into_iter(),
    )?;
    Ok(())
}

fn check_field<T: ToString + PartialEq>(
    field: &'static str,
    expected: T,
    actual: T,
) -> Result<(), DavidsonError> {
    if expected != actual {
        return Err(DavidsonError::CheckpointMismatch {
            field,
            expected: expected.to_string(),
            actual: actual.to_string(),
        });
    }
    Ok(())
}

fn prepare_fresh_workspace(path: &Path) -> Result<(), DavidsonError> {
    if path.exists() {
        let mut entries = fs::read_dir(path).map_err(|source| io_error(path, source))?;
        if entries
            .next()
            .transpose()
            .map_err(|source| io_error(path, source))?
            .is_some()
        {
            return Err(DavidsonError::WorkspaceNotEmpty {
                path: path.to_path_buf(),
            });
        }
    } else {
        fs::create_dir_all(path).map_err(|source| io_error(path, source))?;
    }
    Ok(())
}

fn save_runtime_checkpoint(
    state: &InitialState,
    config: &DavidsonConfig,
    iteration: usize,
    previous_energy: Option<f64>,
    force: bool,
) -> Result<(), DavidsonError> {
    let Some(runtime) = &state.runtime else {
        return Ok(());
    };
    if !force && !iteration.is_multiple_of(runtime.checkpoint_every) {
        return Ok(());
    }
    save_checkpoint(
        &runtime.path,
        &CheckpointWrite {
            operator_fingerprint: &runtime.operator_fingerprint,
            dimension: state.basis.dimension(),
            config,
            completed_iterations: iteration,
            previous_energy,
            basis_generation: state.basis.generation(),
            basis_count: state.basis.len(),
            sigma_generation: state.sigma_basis.generation(),
            sigma_count: state.sigma_basis.len(),
            result: &state.last_result,
        },
    )
}

fn projected_matrix(
    basis: &dyn VectorStore,
    sigma_basis: &dyn VectorStore,
    dimension: usize,
) -> Result<DMatrix<f64>, DavidsonError> {
    let subspace = basis.len();
    let mut projected = DMatrix::zeros(subspace, subspace);
    let mut basis_vector = vec![0.0; dimension];
    let mut sigma_vector = vec![0.0; dimension];
    for i in 0..subspace {
        basis.load(i, &mut basis_vector)?;
        for j in 0..=i {
            sigma_basis.load(j, &mut sigma_vector)?;
            let value = dot(&basis_vector, &sigma_vector);
            projected[(i, j)] = value;
            projected[(j, i)] = value;
        }
    }
    Ok(projected)
}

fn linear_combination(
    store: &dyn VectorStore,
    coefficients: &[f64],
    dimension: usize,
) -> Result<Vec<f64>, DavidsonError> {
    let mut result = vec![0.0; dimension];
    let mut vector = vec![0.0; dimension];
    for (index, &coefficient) in coefficients.iter().enumerate() {
        store.load(index, &mut vector)?;
        axpy(coefficient, &vector, &mut result);
    }
    Ok(result)
}

fn orthogonalize_store(vector: &mut [f64], basis: &dyn VectorStore) -> Result<(), DavidsonError> {
    let mut basis_vector = vec![0.0; vector.len()];
    for _ in 0..2 {
        for index in 0..basis.len() {
            basis.load(index, &mut basis_vector)?;
            let overlap = dot(&basis_vector, vector);
            axpy(-overlap, &basis_vector, vector);
        }
    }
    Ok(())
}

fn coordinate_fallback(
    dimension: usize,
    basis: &dyn VectorStore,
) -> Result<Vec<f64>, DavidsonError> {
    let mut best = vec![0.0; dimension];
    let mut best_norm = 0.0;
    for index in 0..dimension {
        let mut candidate = vec![0.0; dimension];
        candidate[index] = 1.0;
        orthogonalize_store(&mut candidate, basis)?;
        let candidate_norm = norm(&candidate);
        if candidate_norm > best_norm {
            best = candidate;
            best_norm = candidate_norm;
        }
    }
    Ok(best)
}

fn ensure_finite(
    iteration: usize,
    quantity: &'static str,
    values: impl Iterator<Item = f64>,
) -> Result<(), DavidsonError> {
    if values.into_iter().any(|value| !value.is_finite()) {
        return Err(DavidsonError::NonFiniteState {
            iteration,
            quantity,
        });
    }
    Ok(())
}

pub fn lowest_eigenpairs(
    operator: &impl LinearOperator,
    roots: usize,
    config: &DavidsonConfig,
) -> Result<Vec<DavidsonResult>, DavidsonError> {
    let dimension = operator.dimension();
    if roots == 0 || roots > dimension {
        return Err(DavidsonError::InvalidRootCount {
            requested: roots,
            dimension,
        });
    }
    if config.max_subspace < 2 * roots {
        return Err(DavidsonError::RootSubspace {
            roots,
            max_subspace: config.max_subspace,
        });
    }

    let mut coordinate_order: Vec<_> = (0..dimension).collect();
    coordinate_order
        .sort_by(|&left, &right| operator.diagonal()[left].total_cmp(&operator.diagonal()[right]));
    let mut basis = Vec::with_capacity(config.max_subspace);
    let mut sigma_basis = Vec::with_capacity(config.max_subspace);
    let initial_count = (2 * roots).min(dimension).min(config.max_subspace);
    for &coordinate in coordinate_order.iter().take(initial_count) {
        let mut vector = vec![0.0; dimension];
        vector[coordinate] = 1.0;
        let mut sigma = vec![0.0; dimension];
        operator.apply(&vector, &mut sigma)?;
        basis.push(vector);
        sigma_basis.push(sigma);
    }

    let mut previous_energies = vec![f64::INFINITY; roots];
    let mut last_results = Vec::new();
    for iteration in 1..=config.max_iterations {
        let subspace = basis.len();
        let mut projected = DMatrix::zeros(subspace, subspace);
        for i in 0..subspace {
            for j in 0..=i {
                let value = dot(&basis[i], &sigma_basis[j]);
                projected[(i, j)] = value;
                projected[(j, i)] = value;
            }
        }
        let eigensystem = SymmetricEigen::new(projected);
        let mut root_order: Vec<_> = (0..subspace).collect();
        root_order.sort_by(|&left, &right| {
            eigensystem.eigenvalues[left].total_cmp(&eigensystem.eigenvalues[right])
        });

        let mut ritz_vectors = Vec::with_capacity(roots);
        let mut ritz_sigmas = Vec::with_capacity(roots);
        let mut residuals = Vec::with_capacity(roots);
        last_results.clear();
        for root in 0..roots {
            let projected_root = root_order[root];
            let energy = eigensystem.eigenvalues[projected_root];
            let coefficients = eigensystem.eigenvectors.column(projected_root);
            let mut eigenvector = vec![0.0; dimension];
            let mut sigma = vec![0.0; dimension];
            for k in 0..subspace {
                axpy(coefficients[k], &basis[k], &mut eigenvector);
                axpy(coefficients[k], &sigma_basis[k], &mut sigma);
            }
            let mut residual = sigma.clone();
            axpy(-energy, &eigenvector, &mut residual);
            let residual_norm = norm(&residual);
            let energy_change = (energy - previous_energies[root]).abs();
            last_results.push(DavidsonResult {
                energy,
                eigenvector: eigenvector.clone(),
                residual_norm,
                iterations: iteration,
                converged: residual_norm <= config.residual_tolerance
                    && energy_change <= config.energy_tolerance,
            });
            ritz_vectors.push(eigenvector);
            ritz_sigmas.push(sigma);
            residuals.push(residual);
        }
        if last_results.iter().all(|result| result.converged) || iteration == config.max_iterations
        {
            return Ok(last_results);
        }
        for (previous, result) in previous_energies.iter_mut().zip(&last_results) {
            *previous = result.energy;
        }

        let mut corrections = Vec::with_capacity(roots);
        for root in 0..roots {
            if last_results[root].residual_norm <= config.residual_tolerance {
                continue;
            }
            let mut correction = std::mem::take(&mut residuals[root]);
            for (index, value) in correction.iter_mut().enumerate() {
                let denominator = last_results[root].energy - operator.diagonal()[index];
                if denominator.abs() > 1e-12 {
                    *value /= denominator;
                }
            }
            orthogonalize(&mut correction, &basis);
            orthogonalize(&mut correction, &corrections);
            if norm(&correction) > 1e-12 {
                normalize(&mut correction)?;
                corrections.push(correction);
            }
        }
        if corrections.is_empty() {
            continue;
        }

        if basis.len() + corrections.len() > config.max_subspace {
            basis = ritz_vectors;
            sigma_basis = ritz_sigmas;
        }
        for correction in corrections {
            let mut correction_sigma = vec![0.0; dimension];
            operator.apply(&correction, &mut correction_sigma)?;
            basis.push(correction);
            sigma_basis.push(correction_sigma);
        }
    }
    Ok(last_results)
}

fn dot(left: &[f64], right: &[f64]) -> f64 {
    left.iter().zip(right).map(|(a, b)| a * b).sum()
}

fn norm(vector: &[f64]) -> f64 {
    dot(vector, vector).sqrt()
}

fn axpy(alpha: f64, x: &[f64], y: &mut [f64]) {
    for (y_value, x_value) in y.iter_mut().zip(x) {
        *y_value += alpha * x_value;
    }
}

fn normalize(vector: &mut [f64]) -> Result<(), DavidsonError> {
    let magnitude = norm(vector);
    if !magnitude.is_finite() || magnitude <= 1e-15 {
        return Err(DavidsonError::InvalidInitialVector);
    }
    for value in vector {
        *value /= magnitude;
    }
    Ok(())
}

fn orthogonalize(vector: &mut [f64], basis: &[Vec<f64>]) {
    for _ in 0..2 {
        for basis_vector in basis {
            let overlap = dot(basis_vector, vector);
            axpy(-overlap, basis_vector, vector);
        }
    }
}

fn orthonormalize_last(basis: &mut [Vec<f64>], sigma_basis: &mut [Vec<f64>]) {
    if basis.len() < 2 {
        return;
    }
    let overlap = dot(&basis[0], &basis[1]);
    let first = basis[0].clone();
    let first_sigma = sigma_basis[0].clone();
    axpy(-overlap, &first, &mut basis[1]);
    axpy(-overlap, &first_sigma, &mut sigma_basis[1]);
    let magnitude = norm(&basis[1]);
    if magnitude > 1e-15 {
        for value in &mut basis[1] {
            *value /= magnitude;
        }
        for value in &mut sigma_basis[1] {
            *value /= magnitude;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use nalgebra::DVector;

    struct MatrixOperator {
        matrix: DMatrix<f64>,
        diagonal: Vec<f64>,
    }

    impl LinearOperator for MatrixOperator {
        fn dimension(&self) -> usize {
            self.matrix.nrows()
        }

        fn diagonal(&self) -> &[f64] {
            &self.diagonal
        }

        fn apply(&self, input: &[f64], output: &mut [f64]) -> Result<(), OperatorError> {
            let result = &self.matrix * DVector::from_column_slice(input);
            output.copy_from_slice(result.as_slice());
            Ok(())
        }
    }

    #[test]
    fn finds_lowest_eigenvalue_of_symmetric_matrix() {
        let matrix = DMatrix::from_row_slice(3, 3, &[1.0, 0.2, 0.0, 0.2, 2.0, 0.3, 0.0, 0.3, 4.0]);
        let expected = SymmetricEigen::new(matrix.clone()).eigenvalues.min();
        let operator = MatrixOperator {
            diagonal: matrix.diagonal().iter().copied().collect(),
            matrix,
        };
        let result = lowest_eigenpair(
            &operator,
            &[1.0, 0.1, 0.1],
            &DavidsonConfig {
                max_subspace: 3,
                ..Default::default()
            },
        )
        .unwrap();
        assert!(result.converged);
        assert!((result.energy - expected).abs() < 1e-10);
        assert!(result.residual_norm < 1e-10);
    }

    #[test]
    fn block_davidson_finds_multiple_orthogonal_roots() {
        let matrix = DMatrix::from_row_slice(
            4,
            4,
            &[
                1.0, 0.2, 0.0, 0.1, 0.2, 2.0, 0.3, 0.0, 0.0, 0.3, 3.0, 0.4, 0.1, 0.0, 0.4, 4.0,
            ],
        );
        let exact = SymmetricEigen::new(matrix.clone());
        let mut expected = exact.eigenvalues.as_slice().to_vec();
        expected.sort_by(f64::total_cmp);
        let operator = MatrixOperator {
            diagonal: matrix.diagonal().iter().copied().collect(),
            matrix,
        };
        let results = lowest_eigenpairs(
            &operator,
            2,
            &DavidsonConfig {
                max_subspace: 4,
                max_iterations: 10,
                ..Default::default()
            },
        )
        .unwrap();
        assert_eq!(results.len(), 2);
        for (result, expected) in results.iter().zip(expected) {
            assert!(result.converged);
            assert!((result.energy - expected).abs() < 1e-10);
            assert!(result.residual_norm < 1e-10);
        }
        assert!(dot(&results[0].eigenvector, &results[1].eigenvector).abs() < 1e-12);
    }

    #[test]
    fn block_davidson_rejects_too_small_a_subspace() {
        let matrix = DMatrix::identity(3, 3);
        let operator = MatrixOperator {
            diagonal: vec![1.0; 3],
            matrix,
        };
        assert!(matches!(
            lowest_eigenpairs(
                &operator,
                2,
                &DavidsonConfig {
                    max_subspace: 3,
                    ..Default::default()
                }
            ),
            Err(DavidsonError::RootSubspace {
                roots: 2,
                max_subspace: 3
            })
        ));
    }
}
