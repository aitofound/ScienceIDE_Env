use serde::{Deserialize, Serialize};
use std::time::Instant;
use thiserror::Error;

use crate::ao2mo::{Ao2MoError, transform_to_mo};
use crate::direct_fci::{DirectFciError, DirectFciKernel};
use crate::libcint_frontend::{ENERGY_UNIT, IntegralError, compute_ao_integrals};
use crate::molecule::Molecule;
use crate::rhf::{RhfConfig, RhfError, solve_rhf};
use crate::strings::OneBodyLink;

pub const BENCHMARK_SCHEMA_VERSION: u32 = 1;
pub const H2O_CC_PVDZ_PYSCF_RHF_ENERGY: f64 = -76.025_792_594_904_77;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct FciSpaceEstimate {
    pub norb: usize,
    pub nelec: usize,
    pub ms2: isize,
    pub nalpha: usize,
    pub nbeta: usize,
    pub alpha_strings: u64,
    pub beta_strings: u64,
    pub determinants: u64,
    pub vector_bytes: u64,
    pub minimum_current_davidson_bytes: u64,
    pub subspace_24_bytes: u64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct KernelMemoryEstimate {
    pub alpha_links: u64,
    pub beta_links: u64,
    pub link_struct_bytes: u64,
    pub conservative_peak_bytes: u64,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct BenchmarkTimings {
    pub ao_integrals_seconds: f64,
    pub rhf_seconds: f64,
    pub ao_to_mo_seconds: f64,
    pub link_tables_seconds: f64,
    pub sparse_columns_seconds: f64,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct SparseKernelMetrics {
    pub sources: usize,
    pub source_indices: Vec<usize>,
    pub total_nonzeros: u64,
    pub raw_contributions: u64,
    pub columns_per_second: f64,
    pub contributions_per_second: f64,
    pub checksum: f64,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct BoundedBenchmarkResult {
    pub schema_version: u32,
    pub system: String,
    pub geometry: String,
    pub basis: String,
    pub basis_provenance: String,
    pub coordinate_unit: String,
    pub energy_unit: String,
    pub nuclear_repulsion_energy: f64,
    pub all_electron: bool,
    pub point_group_symmetry: bool,
    pub full_fci_executed: bool,
    pub norb: usize,
    pub nelec: usize,
    pub nalpha: usize,
    pub nbeta: usize,
    pub rhf_reference_source: String,
    pub rhf_reference_energy: f64,
    pub rhf_total_energy: f64,
    pub rhf_absolute_error: f64,
    pub rhf_iterations: usize,
    pub rhf_density_rms: f64,
    pub rhf_converged: bool,
    pub rayon_threads: usize,
    pub memory_budget_bytes: u64,
    pub space: FciSpaceEstimate,
    pub bounded_memory: KernelMemoryEstimate,
    pub timings: BenchmarkTimings,
    pub sparse_kernel: SparseKernelMetrics,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct BoundedBenchmarkConfig {
    pub sources: usize,
    pub memory_budget_gib: f64,
}

impl Default for BoundedBenchmarkConfig {
    fn default() -> Self {
        Self {
            sources: 16,
            memory_budget_gib: 2.0,
        }
    }
}

#[derive(Debug, Error, Clone, Copy, PartialEq, Eq)]
pub enum BenchmarkError {
    #[error("invalid electron/spin counts")]
    InvalidSpin,
    #[error("determinant count or memory estimate does not fit in u64")]
    CountOverflow,
    #[error("memory budget must be finite, positive, and representable in bytes")]
    InvalidMemoryBudget,
    #[error(
        "bounded benchmark estimate {estimated_bytes} bytes exceeds budget {budget_bytes} bytes"
    )]
    MemoryBudgetExceeded {
        estimated_bytes: u64,
        budget_bytes: u64,
    },
}

#[derive(Debug, Error)]
pub enum BoundedBenchmarkError {
    #[error("sparse source count must be positive")]
    InvalidSourceCount,
    #[error("sparse source count {sources} exceeds determinant dimension {dimension}")]
    TooManySources { sources: usize, dimension: usize },
    #[error("libcint returned {norb} orbitals and {nelec} electrons; expected 24 and 10")]
    UnexpectedSystem { norb: usize, nelec: usize },
    #[error("RHF did not converge")]
    RhfNotConverged,
    #[error("Rust RHF differs from the PySCF reference by {absolute_error:e} Eh")]
    RhfReferenceMismatch { absolute_error: f64 },
    #[error("constructed link counts differ from the analytic estimate")]
    LinkCountMismatch,
    #[error(transparent)]
    Estimate(#[from] BenchmarkError),
    #[error(transparent)]
    Integrals(#[from] IntegralError),
    #[error(transparent)]
    Rhf(#[from] RhfError),
    #[error(transparent)]
    Ao2Mo(#[from] Ao2MoError),
    #[error(transparent)]
    DirectFci(#[from] DirectFciError),
}

impl FciSpaceEstimate {
    pub fn new(norb: usize, nelec: usize, ms2: isize) -> Result<Self, BenchmarkError> {
        let nalpha_twice = nelec as isize + ms2;
        let nbeta_twice = nelec as isize - ms2;
        if nelec > 2 * norb
            || nalpha_twice < 0
            || nbeta_twice < 0
            || nalpha_twice % 2 != 0
            || nbeta_twice % 2 != 0
        {
            return Err(BenchmarkError::InvalidSpin);
        }
        let nalpha = (nalpha_twice / 2) as usize;
        let nbeta = (nbeta_twice / 2) as usize;
        if nalpha > norb || nbeta > norb {
            return Err(BenchmarkError::InvalidSpin);
        }
        let alpha_strings = u64::try_from(binomial_checked(norb, nalpha)?)
            .map_err(|_| BenchmarkError::CountOverflow)?;
        let beta_strings = u64::try_from(binomial_checked(norb, nbeta)?)
            .map_err(|_| BenchmarkError::CountOverflow)?;
        let determinants = alpha_strings
            .checked_mul(beta_strings)
            .ok_or(BenchmarkError::CountOverflow)?;
        let vector_bytes = determinants
            .checked_mul(size_of::<f64>() as u64)
            .ok_or(BenchmarkError::CountOverflow)?;
        let minimum_current_davidson_bytes = vector_bytes
            .checked_mul(5)
            .ok_or(BenchmarkError::CountOverflow)?;
        let subspace_24_bytes = vector_bytes
            .checked_mul(48)
            .ok_or(BenchmarkError::CountOverflow)?;
        Ok(Self {
            norb,
            nelec,
            ms2,
            nalpha,
            nbeta,
            alpha_strings,
            beta_strings,
            determinants,
            vector_bytes,
            minimum_current_davidson_bytes,
            subspace_24_bytes,
        })
    }
}

impl KernelMemoryEstimate {
    pub fn new(space: &FciSpaceEstimate) -> Result<Self, BenchmarkError> {
        let alpha_links_per_string = space
            .nalpha
            .checked_mul(space.norb - space.nalpha + 1)
            .ok_or(BenchmarkError::CountOverflow)?;
        let beta_links_per_string = space
            .nbeta
            .checked_mul(space.norb - space.nbeta + 1)
            .ok_or(BenchmarkError::CountOverflow)?;
        let alpha_links = space
            .alpha_strings
            .checked_mul(
                u64::try_from(alpha_links_per_string).map_err(|_| BenchmarkError::CountOverflow)?,
            )
            .ok_or(BenchmarkError::CountOverflow)?;
        let beta_links = space
            .beta_strings
            .checked_mul(
                u64::try_from(beta_links_per_string).map_err(|_| BenchmarkError::CountOverflow)?,
            )
            .ok_or(BenchmarkError::CountOverflow)?;
        let total_strings = space
            .alpha_strings
            .checked_add(space.beta_strings)
            .ok_or(BenchmarkError::CountOverflow)?;
        let total_links = alpha_links
            .checked_add(beta_links)
            .ok_or(BenchmarkError::CountOverflow)?;
        let link_struct_bytes = total_links
            .checked_mul(size_of::<OneBodyLink>() as u64)
            .ok_or(BenchmarkError::CountOverflow)?;

        // The link vectors grow geometrically, so reserve twice their logical
        // payload. The remaining terms conservatively cover vector headers,
        // string/address storage, simultaneous AO/MO tensors, a sparse
        // accumulator, and allocator/runtime headroom.
        let link_capacity_bytes = link_struct_bytes
            .checked_mul(2)
            .ok_or(BenchmarkError::CountOverflow)?;
        let vector_headers = total_strings
            .checked_mul(size_of::<Vec<OneBodyLink>>() as u64)
            .ok_or(BenchmarkError::CountOverflow)?;
        let string_and_address_storage = total_strings
            .checked_mul(72)
            .ok_or(BenchmarkError::CountOverflow)?;
        let norb = u64::try_from(space.norb).map_err(|_| BenchmarkError::CountOverflow)?;
        let orbital_tensor_bytes = norb
            .checked_pow(4)
            .and_then(|count| count.checked_mul(6))
            .and_then(|count| count.checked_mul(size_of::<f64>() as u64))
            .ok_or(BenchmarkError::CountOverflow)?;
        let sparse_accumulator_headroom = 128_u64 * 1024 * 1024;
        let runtime_headroom = 256_u64 * 1024 * 1024;
        let conservative_peak_bytes = [
            link_capacity_bytes,
            vector_headers,
            string_and_address_storage,
            orbital_tensor_bytes,
            sparse_accumulator_headroom,
            runtime_headroom,
        ]
        .into_iter()
        .try_fold(0_u64, |total, value| total.checked_add(value))
        .ok_or(BenchmarkError::CountOverflow)?;

        Ok(Self {
            alpha_links,
            beta_links,
            link_struct_bytes,
            conservative_peak_bytes,
        })
    }

    pub fn enforce_budget(&self, budget_bytes: u64) -> Result<(), BenchmarkError> {
        if self.conservative_peak_bytes > budget_bytes {
            return Err(BenchmarkError::MemoryBudgetExceeded {
                estimated_bytes: self.conservative_peak_bytes,
                budget_bytes,
            });
        }
        Ok(())
    }
}

pub fn gibibytes_to_bytes(gibibytes: f64) -> Result<u64, BenchmarkError> {
    let bytes = gibibytes * 1024_f64.powi(3);
    if !bytes.is_finite() || bytes < 1.0 || bytes > u64::MAX as f64 {
        return Err(BenchmarkError::InvalidMemoryBudget);
    }
    Ok(bytes.floor() as u64)
}

pub fn run_h2o_cc_pvdz_benchmark(
    config: BoundedBenchmarkConfig,
) -> Result<BoundedBenchmarkResult, BoundedBenchmarkError> {
    if config.sources == 0 {
        return Err(BoundedBenchmarkError::InvalidSourceCount);
    }
    let space = FciSpaceEstimate::new(24, 10, 0)?;
    let bounded_memory = KernelMemoryEstimate::new(&space)?;
    let memory_budget_bytes = gibibytes_to_bytes(config.memory_budget_gib)?;
    bounded_memory.enforce_budget(memory_budget_bytes)?;
    let dimension =
        usize::try_from(space.determinants).map_err(|_| BenchmarkError::CountOverflow)?;
    if config.sources > dimension {
        return Err(BoundedBenchmarkError::TooManySources {
            sources: config.sources,
            dimension,
        });
    }

    let molecule = Molecule::h2o_cc_pvdz();
    let integral_started = Instant::now();
    let integrals = compute_ao_integrals(&molecule)?;
    let ao_integrals_seconds = integral_started.elapsed().as_secs_f64();
    if integrals.nao != 24 || integrals.nelec != 10 {
        return Err(BoundedBenchmarkError::UnexpectedSystem {
            norb: integrals.nao,
            nelec: integrals.nelec,
        });
    }

    let rhf_started = Instant::now();
    let rhf = solve_rhf(&integrals, &RhfConfig::default())?;
    let rhf_seconds = rhf_started.elapsed().as_secs_f64();
    if !rhf.converged {
        return Err(BoundedBenchmarkError::RhfNotConverged);
    }
    let rhf_absolute_error = (rhf.total_energy - H2O_CC_PVDZ_PYSCF_RHF_ENERGY).abs();
    if rhf_absolute_error > 1e-8 {
        return Err(BoundedBenchmarkError::RhfReferenceMismatch {
            absolute_error: rhf_absolute_error,
        });
    }

    let transform_started = Instant::now();
    let problem = transform_to_mo(&integrals, &rhf)?;
    let ao_to_mo_seconds = transform_started.elapsed().as_secs_f64();
    let basis_provenance = integrals.basis_provenance.clone();
    let nuclear_repulsion_energy = integrals.nuclear_repulsion;
    let rhf_total_energy = rhf.total_energy;
    let rhf_iterations = rhf.iterations;
    let rhf_density_rms = rhf.density_rms;
    drop(rhf);
    drop(integrals);

    let links_started = Instant::now();
    let kernel = DirectFciKernel::new(problem)?;
    let link_tables_seconds = links_started.elapsed().as_secs_f64();
    if kernel.dimension() != dimension
        || u64::try_from(kernel.alpha_link_count()).ok() != Some(bounded_memory.alpha_links)
        || u64::try_from(kernel.beta_link_count()).ok() != Some(bounded_memory.beta_links)
    {
        return Err(BoundedBenchmarkError::LinkCountMismatch);
    }

    let sparse_started = Instant::now();
    let mut total_nonzeros = 0_u64;
    let mut raw_contributions = 0_u64;
    let mut checksum = 0.0;
    let source_indices: Vec<_> = sample_sources(dimension, config.sources).collect();
    for &source in &source_indices {
        let column = kernel.apply_source_sparse(source)?;
        total_nonzeros = total_nonzeros
            .checked_add(
                u64::try_from(column.entries.len()).map_err(|_| BenchmarkError::CountOverflow)?,
            )
            .ok_or(BenchmarkError::CountOverflow)?;
        raw_contributions = raw_contributions
            .checked_add(
                u64::try_from(column.raw_contributions)
                    .map_err(|_| BenchmarkError::CountOverflow)?,
            )
            .ok_or(BenchmarkError::CountOverflow)?;
        for &(destination, value) in &column.entries {
            let mixed = (destination as u64)
                .wrapping_mul(1_000_003)
                .wrapping_add(source as u64)
                % 104_729;
            checksum += value * (1.0 + mixed as f64 / 104_729.0);
        }
    }
    let sparse_columns_seconds = sparse_started.elapsed().as_secs_f64();

    Ok(BoundedBenchmarkResult {
        schema_version: BENCHMARK_SCHEMA_VERSION,
        system: "H2O/cc-pVDZ all-electron".to_string(),
        geometry: molecule.atom,
        basis: molecule.basis,
        basis_provenance,
        coordinate_unit: molecule.coordinate_unit.to_string(),
        energy_unit: ENERGY_UNIT.to_string(),
        nuclear_repulsion_energy,
        all_electron: true,
        point_group_symmetry: false,
        full_fci_executed: false,
        norb: space.norb,
        nelec: space.nelec,
        nalpha: space.nalpha,
        nbeta: space.nbeta,
        rhf_reference_source: "PySCF 2.14.0, symmetry=False".to_string(),
        rhf_reference_energy: H2O_CC_PVDZ_PYSCF_RHF_ENERGY,
        rhf_total_energy,
        rhf_absolute_error,
        rhf_iterations,
        rhf_density_rms,
        rhf_converged: true,
        rayon_threads: rayon::current_num_threads(),
        memory_budget_bytes,
        space,
        bounded_memory,
        timings: BenchmarkTimings {
            ao_integrals_seconds,
            rhf_seconds,
            ao_to_mo_seconds,
            link_tables_seconds,
            sparse_columns_seconds,
        },
        sparse_kernel: SparseKernelMetrics {
            sources: config.sources,
            source_indices,
            total_nonzeros,
            raw_contributions,
            columns_per_second: config.sources as f64 / sparse_columns_seconds,
            contributions_per_second: raw_contributions as f64 / sparse_columns_seconds,
            checksum,
        },
    })
}

fn sample_sources(dimension: usize, count: usize) -> impl Iterator<Item = usize> {
    (0..count).map(move |index| {
        if count == 1 {
            0
        } else {
            ((index as u128 * (dimension - 1) as u128) / (count - 1) as u128) as usize
        }
    })
}

fn binomial_checked(n: usize, k: usize) -> Result<u128, BenchmarkError> {
    if k > n {
        return Ok(0);
    }
    let k = k.min(n - k);
    let mut result = 1_u128;
    for index in 0..k {
        let mut numerator = (n - index) as u128;
        let mut denominator = (index + 1) as u128;
        let common = gcd(numerator, denominator);
        numerator /= common;
        denominator /= common;
        let common = gcd(result, denominator);
        result /= common;
        denominator /= common;
        debug_assert_eq!(denominator, 1);
        result = result
            .checked_mul(numerator)
            .ok_or(BenchmarkError::CountOverflow)?;
    }
    Ok(result)
}

fn gcd(mut left: u128, mut right: u128) -> u128 {
    while right != 0 {
        let remainder = left % right;
        left = right;
        right = remainder;
    }
    left
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn water_ccpvdz_space_is_exact() {
        let estimate = FciSpaceEstimate::new(24, 10, 0).unwrap();
        assert_eq!(estimate.nalpha, 5);
        assert_eq!(estimate.nbeta, 5);
        assert_eq!(estimate.alpha_strings, 42_504);
        assert_eq!(estimate.beta_strings, 42_504);
        assert_eq!(estimate.determinants, 1_806_590_016);
        assert_eq!(estimate.vector_bytes, 14_452_720_128);
        assert_eq!(estimate.minimum_current_davidson_bytes, 72_263_600_640);
        assert_eq!(estimate.subspace_24_bytes, 693_730_566_144);
    }

    #[test]
    fn rejects_invalid_spin_and_detects_overflow() {
        assert!(matches!(
            FciSpaceEstimate::new(24, 10, 1),
            Err(BenchmarkError::InvalidSpin)
        ));
        assert!(matches!(
            FciSpaceEstimate::new(128, 128, 0),
            Err(BenchmarkError::CountOverflow)
        ));
    }

    #[test]
    fn bounded_kernel_estimate_fits_default_but_rejects_small_budget() {
        let space = FciSpaceEstimate::new(24, 10, 0).unwrap();
        let memory = KernelMemoryEstimate::new(&space).unwrap();
        assert_eq!(memory.alpha_links, 4_250_400);
        assert_eq!(memory.beta_links, 4_250_400);
        assert!(memory.conservative_peak_bytes < gibibytes_to_bytes(2.0).unwrap());
        assert!(memory.conservative_peak_bytes > gibibytes_to_bytes(0.5).unwrap());
        assert!(
            memory
                .enforce_budget(gibibytes_to_bytes(2.0).unwrap())
                .is_ok()
        );
        assert!(matches!(
            memory.enforce_budget(gibibytes_to_bytes(0.5).unwrap()),
            Err(BenchmarkError::MemoryBudgetExceeded { .. })
        ));
    }

    #[test]
    fn source_sampling_spans_the_space_without_duplicates() {
        let sources: Vec<_> = sample_sources(1_806_590_016, 16).collect();
        assert_eq!(sources.len(), 16);
        assert_eq!(sources[0], 0);
        assert_eq!(sources[15], 1_806_590_015);
        assert!(sources.windows(2).all(|pair| pair[0] < pair[1]));
    }
}
