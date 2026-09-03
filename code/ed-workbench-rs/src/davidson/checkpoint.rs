use std::fs::{self, File};
use std::io::{BufWriter, Write};
use std::path::{Path, PathBuf};

use serde::{Deserialize, Serialize};

use super::storage::{io_error, read_vector, write_vector_atomic};
use super::{DavidsonConfig, DavidsonError, DavidsonResult};

pub(crate) const CHECKPOINT_SCHEMA_VERSION: u32 = 1;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub(crate) struct DavidsonCheckpoint {
    pub schema_version: u32,
    pub operator_fingerprint: String,
    pub dimension: usize,
    pub residual_tolerance: f64,
    pub energy_tolerance: f64,
    pub max_subspace: usize,
    pub completed_iterations: usize,
    pub previous_energy: Option<f64>,
    pub basis_generation: u64,
    pub basis_count: usize,
    pub sigma_generation: u64,
    pub sigma_count: usize,
    pub last_energy: f64,
    pub last_residual_norm: f64,
    pub last_converged: bool,
    pub result_vector_file: String,
    pub scalar_type: String,
    pub byte_order: String,
}

pub(crate) struct CheckpointWrite<'a> {
    pub operator_fingerprint: &'a str,
    pub dimension: usize,
    pub config: &'a DavidsonConfig,
    pub completed_iterations: usize,
    pub previous_energy: Option<f64>,
    pub basis_generation: u64,
    pub basis_count: usize,
    pub sigma_generation: u64,
    pub sigma_count: usize,
    pub result: &'a DavidsonResult,
}

pub(crate) fn save_checkpoint(
    workspace: &Path,
    write: &CheckpointWrite<'_>,
) -> Result<(), DavidsonError> {
    let results = workspace.join("results");
    fs::create_dir_all(&results).map_err(|source| io_error(&results, source))?;
    let result_name = format!("results/result-{:06}.bin", write.completed_iterations);
    write_vector_atomic(&workspace.join(&result_name), &write.result.eigenvector)?;

    let manifest = DavidsonCheckpoint {
        schema_version: CHECKPOINT_SCHEMA_VERSION,
        operator_fingerprint: write.operator_fingerprint.to_string(),
        dimension: write.dimension,
        residual_tolerance: write.config.residual_tolerance,
        energy_tolerance: write.config.energy_tolerance,
        max_subspace: write.config.max_subspace,
        completed_iterations: write.completed_iterations,
        previous_energy: write.previous_energy,
        basis_generation: write.basis_generation,
        basis_count: write.basis_count,
        sigma_generation: write.sigma_generation,
        sigma_count: write.sigma_count,
        last_energy: write.result.energy,
        last_residual_norm: write.result.residual_norm,
        last_converged: write.result.converged,
        result_vector_file: result_name,
        scalar_type: "f64".to_string(),
        byte_order: "little".to_string(),
    };
    let path = workspace.join("checkpoint.json");
    let temporary = workspace.join("checkpoint.json.tmp");
    let bytes =
        serde_json::to_vec_pretty(&manifest).map_err(|error| DavidsonError::InvalidCheckpoint {
            path: path.clone(),
            reason: error.to_string(),
        })?;
    let file = File::create(&temporary).map_err(|source| io_error(&temporary, source))?;
    let mut writer = BufWriter::new(file);
    writer
        .write_all(&bytes)
        .map_err(|source| io_error(&temporary, source))?;
    writer
        .flush()
        .map_err(|source| io_error(&temporary, source))?;
    writer
        .get_ref()
        .sync_all()
        .map_err(|source| io_error(&temporary, source))?;
    fs::rename(&temporary, &path).map_err(|source| io_error(&path, source))?;
    Ok(())
}

pub(crate) fn load_checkpoint(workspace: &Path) -> Result<DavidsonCheckpoint, DavidsonError> {
    let path = workspace.join("checkpoint.json");
    let bytes = fs::read(&path).map_err(|source| io_error(&path, source))?;
    serde_json::from_slice(&bytes).map_err(|error| DavidsonError::InvalidCheckpoint {
        path,
        reason: error.to_string(),
    })
}

pub(crate) fn load_result_vector(
    workspace: &Path,
    checkpoint: &DavidsonCheckpoint,
) -> Result<Vec<f64>, DavidsonError> {
    let path = checked_relative_path(workspace, &checkpoint.result_vector_file)?;
    let mut result = vec![0.0; checkpoint.dimension];
    read_vector(&path, checkpoint.dimension, &mut result)?;
    Ok(result)
}

fn checked_relative_path(workspace: &Path, relative: &str) -> Result<PathBuf, DavidsonError> {
    let path = Path::new(relative);
    if path.is_absolute()
        || path
            .components()
            .any(|component| matches!(component, std::path::Component::ParentDir))
    {
        return Err(DavidsonError::InvalidCheckpoint {
            path: workspace.join("checkpoint.json"),
            reason: format!("unsafe result vector path {relative:?}"),
        });
    }
    Ok(workspace.join(path))
}
