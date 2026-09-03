//! Transparent determinant-based electronic-structure reference workbench.
//!
//! This crate implements the Rust calculation stack developed for Quantum
//! Harness challenge #129. It favors small, auditable algorithms and
//! independently checked paths over production-code specialization:
//!
//! - [`fcidump`], [`determinant`], [`strings`], [`direct_fci`], and
//!   [`davidson`] form the direct full-configuration-interaction path;
//! - [`cluster`] and [`coupled_cluster`] implement arbitrary-order
//!   determinant coupled cluster;
//! - [`truncated_ci`], [`mbpt`], and [`unitary_cc`] reuse the same operator
//!   machinery for the Level 3 methods;
//! - [`libcint_frontend`], [`rhf`], and [`ao2mo`] provide the direct-integral
//!   route that removes Python from production calculations.
//!
//! Public molecule inputs use Angstrom coordinates. The libcint interface
//! converts coordinates internally to Bohr. Total energies, orbital energies,
//! nuclear repulsion, and energy-valued integrals are in Hartree; overlaps,
//! orbital coefficients, CI coefficients, and CC amplitudes are
//! dimensionless.
//!
//! Committed PySCF data is an independent oracle and fixture source. The Rust
//! production commands do not import Python.

pub mod active_space;
pub mod amplitudes;
pub mod ao2mo;
pub mod benchmark;
pub mod cluster;
pub mod combinadic;
pub mod coupled_cluster;
pub mod davidson;
pub mod dense_fci;
pub mod determinant;
pub mod diis;
pub mod direct_fci;
pub mod excitation;
pub mod fcidump;
pub mod hamiltonian;
pub mod libcint_frontend;
pub mod mbpt;
pub mod molecule;
pub mod operator;
pub mod optimizer;
pub mod problem;
pub mod published_reference;
pub mod reference;
pub mod rhf;
pub mod strings;
pub mod truncated_ci;
pub mod unitary_cc;
