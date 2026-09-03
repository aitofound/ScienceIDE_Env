use std::path::PathBuf;

use ed_workbench_rs::published_reference::{HirataTable2, SeriesKind, rounded_published_match};

fn fixture_path() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("fixtures/h2o-631g-fc/hirata2000-table2.json")
}

#[test]
fn loads_hirata_table2_equilibrium_series() {
    let table = HirataTable2::load(&fixture_path()).unwrap();
    assert_eq!(table.energy_unit, "hartree");
    assert_eq!(table.printed_decimals, 6);
    assert_eq!(table.system.basis, "6-31G");
    assert_eq!(table.system.frozen_orbitals, vec![0]);
    assert_eq!(table.system.active_spatial_orbitals, 12);
    assert_eq!(table.system.active_electrons, 8);
    assert_eq!(table.system.determinants, 245_025);
    assert_eq!(table.difference(SeriesKind::Cc, 2), Some(0.001545));
    assert_eq!(table.difference(SeriesKind::Cc, 8), Some(0.0));
    assert_eq!(table.difference(SeriesKind::Ci, 4), Some(0.000175));
    assert_eq!(table.difference(SeriesKind::Mbpt, 10), Some(0.000003));
    assert_eq!(table.difference(SeriesKind::Cc, 0), None);
    assert_eq!(table.difference(SeriesKind::Cc, 9), None);
}

#[test]
fn comparison_respects_the_papers_printed_precision() {
    assert!(rounded_published_match(0.0015446852, 0.001545, 6));
    assert!(rounded_published_match(0.00000049, 0.0, 6));
    assert!(!rounded_published_match(0.0015439, 0.001545, 6));
    assert!(!rounded_published_match(0.00000051, 0.0, 6));
}
