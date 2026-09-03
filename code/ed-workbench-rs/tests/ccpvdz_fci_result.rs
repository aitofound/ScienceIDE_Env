use std::fs;
use std::path::{Path, PathBuf};

use ed_workbench_rs::determinant::DeterminantBasis;
use ed_workbench_rs::fcidump::Fcidump;
use ed_workbench_rs::reference::sha256_hex;

fn root() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/h2o-ccpvdz-ae")
}

#[test]
fn committed_ccpvdz_fci_evidence_is_self_consistent() {
    let root = root();
    let bytes = fs::read(root.join("FCIDUMP.c2v")).unwrap();
    let dump = Fcidump::parse(std::str::from_utf8(&bytes).unwrap()).unwrap();
    let result: serde_json::Value =
        serde_json::from_slice(&fs::read(root.join("fci-c2v-xh5-result.json")).unwrap()).unwrap();
    let crosscheck: serde_json::Value =
        serde_json::from_slice(&fs::read(root.join("pyscf-crosscheck.json")).unwrap()).unwrap();
    let stdout = fs::read_to_string(root.join("xh5/production-23008083.out")).unwrap();

    assert_eq!(dump.norb, 24);
    assert_eq!(dump.nelec, 10);
    assert_eq!(dump.ms2, 0);
    assert_eq!(dump.isym, 1);
    assert_eq!(dump.ecore, 9.094848418932882);
    assert_eq!(
        sha256_hex(&bytes),
        "b55d1bcb04f6889e5b5dff1336412c5f7118b5bdb8461d504764f2a704cd6255"
    );

    let basis =
        DeterminantBasis::with_symmetry(dump.norb, dump.nelec, dump.ms2, &dump.orbsym, dump.isym)
            .unwrap();
    assert_eq!(basis.len(), 451_681_246);
    assert_eq!(
        result["scientific_scope"]["determinants"],
        serde_json::json!(basis.len())
    );
    assert_eq!(
        result["result"]["total_energy_hartree_text"],
        "-76.243218589558566"
    );
    assert_eq!(result["result"]["converged"], true);
    assert!(
        result["result"]["residual_norm"].as_f64().unwrap()
            <= result["solver"]["residual_tolerance"].as_f64().unwrap()
    );
    assert_eq!(result["hpc"]["state"], "COMPLETED");
    assert_eq!(result["hpc"]["exit_code"], "0:0");
    assert_eq!(result["acceptance"]["accepted"], true);

    let fci = result["result"]["total_energy_hartree"].as_f64().unwrap();
    let rhf = crosscheck["rhf"]["total_energy_hartree"].as_f64().unwrap();
    let cisd = crosscheck["cisd"]["total_energy_hartree"].as_f64().unwrap();
    let ccsd_t = crosscheck["ccsd_t"]["total_energy_hartree"]
        .as_f64()
        .unwrap();
    assert!(fci < cisd);
    assert!(fci < ccsd_t);
    assert!((fci - rhf + 0.217425994653681).abs() < 1e-12);
    assert!(((ccsd_t - fci) * 1000.0 - 0.647143741219).abs() < 1e-9);

    assert!(stdout.contains("energy: -76.243218589558566"));
    assert!(stdout.contains("residual norm: 6.602e-8"));
    assert!(stdout.contains("converged: true"));
}
