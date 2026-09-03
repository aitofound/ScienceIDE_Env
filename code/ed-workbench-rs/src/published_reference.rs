use std::fs;
use std::path::Path;

use serde::Deserialize;
use thiserror::Error;

const EXPECTED_QUANTITY: &str = "method_total_energy_minus_fci_total_energy";

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SeriesKind {
    Ci,
    Mbpt,
    Cc,
}

#[derive(Debug, Deserialize)]
pub struct PublishedSystem {
    pub name: String,
    pub basis: String,
    pub frozen_orbitals: Vec<usize>,
    pub bond_length_angstrom: f64,
    pub bond_angle_degree: f64,
    pub active_spatial_orbitals: usize,
    pub active_electrons: usize,
    pub determinants: usize,
    pub fci_energy_printed: f64,
}

#[derive(Debug, Deserialize)]
struct EquilibriumSeries {
    ci: Vec<f64>,
    mbpt: Vec<f64>,
    cc: Vec<f64>,
}

#[derive(Debug, Deserialize)]
pub struct HirataTable2 {
    pub schema_version: usize,
    pub citation: String,
    pub doi: String,
    pub table: usize,
    pub page: usize,
    pub quantity: String,
    pub energy_unit: String,
    pub printed_decimals: u32,
    pub transcription_note: String,
    pub system: PublishedSystem,
    equilibrium: EquilibriumSeries,
}

#[derive(Debug, Error)]
pub enum PublishedReferenceError {
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
    #[error("invalid Hirata Table 2 reference: {0}")]
    Invalid(String),
}

impl HirataTable2 {
    pub fn load(path: &Path) -> Result<Self, PublishedReferenceError> {
        let contents =
            fs::read_to_string(path).map_err(|source| PublishedReferenceError::Read {
                path: path.display().to_string(),
                source,
            })?;
        let table: Self =
            serde_json::from_str(&contents).map_err(|source| PublishedReferenceError::Parse {
                path: path.display().to_string(),
                source,
            })?;
        table.validate()?;
        Ok(table)
    }

    pub fn difference(&self, series: SeriesKind, order: usize) -> Option<f64> {
        let values = match series {
            SeriesKind::Ci => &self.equilibrium.ci,
            SeriesKind::Mbpt => &self.equilibrium.mbpt,
            SeriesKind::Cc => &self.equilibrium.cc,
        };
        order
            .checked_sub(1)
            .and_then(|index| values.get(index))
            .copied()
    }

    fn validate(&self) -> Result<(), PublishedReferenceError> {
        let invalid = |message: &str| PublishedReferenceError::Invalid(message.to_string());
        if self.schema_version != 1 {
            return Err(invalid("schema_version must be 1"));
        }
        if self.doi != "10.1016/S0009-2614(00)00387-0" || self.table != 2 || self.page != 222 {
            return Err(invalid(
                "source must identify Hirata 2000 Table 2 on page 222",
            ));
        }
        if self.quantity != EXPECTED_QUANTITY {
            return Err(invalid(
                "quantity must be method total energy minus FCI total energy",
            ));
        }
        if self.energy_unit != "hartree" || self.printed_decimals != 6 {
            return Err(invalid("energy unit or printed precision is inconsistent"));
        }
        if self.system.name != "H2O"
            || self.system.basis != "6-31G"
            || self.system.frozen_orbitals != [0]
            || (self.system.bond_length_angstrom - 0.967).abs() > 1e-12
            || (self.system.bond_angle_degree - 107.6).abs() > 1e-12
            || self.system.active_spatial_orbitals != 12
            || self.system.active_electrons != 8
            || self.system.determinants != 245_025
        {
            return Err(invalid(
                "molecular or active-space settings are inconsistent",
            ));
        }
        if (self.system.fci_energy_printed - -76.121174).abs() > 0.5e-6 {
            return Err(invalid("printed FCI energy is inconsistent"));
        }
        for (name, values, expected) in [
            ("CI", &self.equilibrium.ci, 8),
            ("MBPT", &self.equilibrium.mbpt, 20),
            ("CC", &self.equilibrium.cc, 8),
        ] {
            if values.len() != expected || values.iter().any(|value| !value.is_finite()) {
                return Err(invalid(&format!(
                    "{name} series must contain {expected} finite values"
                )));
            }
        }
        Ok(())
    }
}

pub fn rounded_published_match(computed: f64, published: f64, decimals: u32) -> bool {
    if !computed.is_finite() || !published.is_finite() || decimals > 15 {
        return false;
    }
    let scale = 10_f64.powi(decimals as i32);
    (computed * scale).round() == (published * scale).round()
}
