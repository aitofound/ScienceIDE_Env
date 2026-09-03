use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

use ed_workbench_rs::fcidump::Fcidump;
use ed_workbench_rs::reference::{Reference, sha256_hex};
use serde::Deserialize;

#[derive(Deserialize)]
struct DavidsonRecord {
    calculation_commit: String,
    energy_unit: String,
    determinants: usize,
    fcidump_sha256: String,
    pyscf_fci_energy: f64,
    rust_fci_energy: f64,
    rust_minus_pyscf: f64,
    residual_tolerance: f64,
    residual_norm: f64,
    converged: bool,
}

#[derive(Deserialize)]
struct SeriesRecord {
    calculation_commit: String,
    energy_unit: String,
    determinants: usize,
    fcidump_sha256: String,
    fci_energy: f64,
    pyscf_ccsd_energy: f64,
    residual_tolerance: f64,
    results: Vec<RankRecord>,
}

#[derive(Deserialize)]
struct RankRecord {
    rank: usize,
    amplitudes: usize,
    energy: f64,
    method_minus_fci: f64,
    residual_norm: f64,
    converged: bool,
}

fn fixture(slug: &str) -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("fixtures")
        .join(slug)
}

#[test]
fn committed_stretched_water_records_are_complete_and_self_consistent() {
    for slug in ["h2o-631g-fc-r1p5", "h2o-631g-fc-r2p0"] {
        let root = fixture(slug);
        let bytes = fs::read(root.join("FCIDUMP")).unwrap();
        let dump = Fcidump::parse(std::str::from_utf8(&bytes).unwrap()).unwrap();
        let reference = Reference::load(&root.join("reference.json")).unwrap();
        let raw_reference: serde_json::Value =
            serde_json::from_slice(&fs::read(root.join("reference.json")).unwrap()).unwrap();
        let davidson: DavidsonRecord =
            serde_json::from_slice(&fs::read(root.join("davidson_result.json")).unwrap()).unwrap();
        let series: SeriesRecord =
            serde_json::from_slice(&fs::read(root.join("cc_series_results.json")).unwrap())
                .unwrap();

        assert_eq!(dump.norb, 12);
        assert_eq!(dump.nelec, 8);
        assert_eq!(dump.ms2, 0);
        assert_eq!(reference.frozen_orbitals, vec![0]);
        assert_eq!(reference.number_of_active_molecular_orbitals, Some(12));
        assert_eq!(reference.number_of_active_electrons, Some(8));
        assert_eq!(raw_reference["fci_converged"], true);
        assert_eq!(raw_reference["ccsd_converged"], true);

        let checksum = sha256_hex(&bytes);
        assert_eq!(checksum, reference.fcidump_sha256);
        assert_eq!(checksum, davidson.fcidump_sha256);
        assert_eq!(checksum, series.fcidump_sha256);
        assert_eq!(davidson.energy_unit, "hartree");
        assert_eq!(series.energy_unit, "hartree");
        assert_eq!(davidson.calculation_commit.len(), 40);
        assert_eq!(series.calculation_commit.len(), 40);
        assert_eq!(davidson.determinants, 245_025);
        assert_eq!(series.determinants, 245_025);

        assert!(davidson.converged);
        assert!(davidson.residual_norm <= davidson.residual_tolerance);
        assert_eq!(davidson.pyscf_fci_energy, reference.fci_energy);
        assert!(
            (davidson.rust_fci_energy - davidson.pyscf_fci_energy - davidson.rust_minus_pyscf)
                .abs()
                < 1e-14
        );
        assert!(davidson.rust_minus_pyscf.abs() < 2e-12);

        assert_eq!(series.fci_energy, reference.fci_energy);
        assert_eq!(series.results.len(), 8);
        let mut previous_amplitudes = 0;
        for (index, result) in series.results.iter().enumerate() {
            assert_eq!(result.rank, index + 1);
            assert!(result.amplitudes > previous_amplitudes);
            assert!(result.converged);
            assert!(result.residual_norm <= series.residual_tolerance);
            assert!((result.energy - series.fci_energy - result.method_minus_fci).abs() < 2e-12);
            previous_amplitudes = result.amplitudes;
        }
        assert!(
            (series.results[1].energy - series.pyscf_ccsd_energy).abs() < 5e-8,
            "{slug}: Rust CC(2) {}, PySCF CCSD {}",
            series.results[1].energy,
            series.pyscf_ccsd_energy
        );
        assert!((series.results[7].energy - series.fci_energy).abs() < 1e-6);
    }
}

#[test]
fn twice_equilibrium_cc3_exhibits_nonvariational_overshoot() {
    let series: SeriesRecord = serde_json::from_slice(
        &fs::read(fixture("h2o-631g-fc-r2p0").join("cc_series_results.json")).unwrap(),
    )
    .unwrap();
    assert!(series.results[2].method_minus_fci < 0.0);
    assert!(series.results[3].method_minus_fci > 0.0);
}

#[test]
#[ignore = "release-mode Davidson validation of both stretched geometries takes about 75 seconds"]
fn live_stretched_davidson_matches_committed_results() {
    let project = Path::new(env!("CARGO_MANIFEST_DIR"));
    for slug in ["h2o-631g-fc-r1p5", "h2o-631g-fc-r2p0"] {
        let root = fixture(slug);
        let output = Command::new(project.join("target/release/ed_workbench_rs"))
            .arg("davidson")
            .arg(root.join("FCIDUMP"))
            .arg("--residual-tolerance")
            .arg("1e-7")
            .arg("--max-iterations")
            .arg("100")
            .arg("--max-subspace")
            .arg("24")
            .output()
            .unwrap();
        assert!(
            output.status.success(),
            "{}",
            String::from_utf8_lossy(&output.stderr)
        );
        assert!(String::from_utf8_lossy(&output.stdout).contains("converged: true"));
    }
}

#[test]
#[ignore = "release-mode CC(1)-CC(8) validation of both stretched geometries takes about six minutes"]
fn live_stretched_cc_series_converges_through_rank_eight() {
    let project = Path::new(env!("CARGO_MANIFEST_DIR"));
    for slug in ["h2o-631g-fc-r1p5", "h2o-631g-fc-r2p0"] {
        let root = fixture(slug);
        let output = Command::new(project.join("target/release/ed_workbench_rs"))
            .arg("cc-series")
            .arg(root.join("FCIDUMP"))
            .arg(root.join("reference.json"))
            .arg("--max-rank")
            .arg("8")
            .arg("--residual-tolerance")
            .arg("1e-6")
            .arg("--max-iterations")
            .arg("100")
            .output()
            .unwrap();
        assert!(
            output.status.success(),
            "{}",
            String::from_utf8_lossy(&output.stderr)
        );
        assert!(String::from_utf8_lossy(&output.stdout).contains("series converged: true"));
    }
}
