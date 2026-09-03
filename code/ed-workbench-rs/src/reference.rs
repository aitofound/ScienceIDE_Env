use std::fs;
use std::path::Path;

use serde::Deserialize;
use sha2::{Digest, Sha256};
use thiserror::Error;

#[derive(Debug, Deserialize)]
pub struct Reference {
    pub system: String,
    pub fci_energy: f64,
    pub fcidump_sha256: String,
    #[serde(default)]
    pub basis: Option<String>,
    #[serde(default)]
    pub coordinate_unit: Option<String>,
    #[serde(default)]
    pub frozen_orbitals: Vec<usize>,
    #[serde(default)]
    pub number_of_active_electrons: Option<usize>,
    #[serde(default)]
    pub number_of_active_molecular_orbitals: Option<usize>,
    #[serde(default)]
    pub ccsd_total_energy: Option<f64>,
    #[serde(default)]
    pub mp2_total_energy: Option<f64>,
    #[serde(default)]
    pub active_orbital_energies: Vec<f64>,
}

#[derive(Debug, Error)]
pub enum ReferenceError {
    #[error("failed to read {path}: {source}")]
    Read {
        path: String,
        #[source]
        source: std::io::Error,
    },
    #[error("failed to parse {path}: {source}")]
    Parse {
        path: String,
        #[source]
        source: serde_json::Error,
    },
}

impl Reference {
    pub fn load(path: &Path) -> Result<Self, ReferenceError> {
        let contents = fs::read_to_string(path).map_err(|source| ReferenceError::Read {
            path: path.display().to_string(),
            source,
        })?;
        serde_json::from_str(&contents).map_err(|source| ReferenceError::Parse {
            path: path.display().to_string(),
            source,
        })
    }
}

pub fn sha256_hex(bytes: &[u8]) -> String {
    format!("{:x}", Sha256::digest(bytes))
}
