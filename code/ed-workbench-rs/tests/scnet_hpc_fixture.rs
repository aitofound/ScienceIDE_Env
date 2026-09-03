use std::fs;
use std::path::PathBuf;

use serde_json::Value;

fn fixture() -> Value {
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("fixtures/hpc/scnet-2026-07-30.json");
    serde_json::from_str(&fs::read_to_string(path).expect("read SCNet fixture"))
        .expect("parse SCNet fixture")
}

#[test]
fn scnet_fixture_is_complete_and_fail_closed() {
    let fixture = fixture();

    assert_eq!(fixture["schema_version"], 1);
    assert_eq!(
        fixture["provenance"]["source_commit"],
        "48f1964a1b3b88090497e1ffce285fde09c98541"
    );
    assert_eq!(fixture["provenance"]["verified_sha256_manifests"], 37);
    assert_eq!(fixture["preflight"]["exit_status"], 0);

    let verifications = fixture["preflight"]["tiny_system_verifications"]
        .as_array()
        .expect("verification array");
    assert_eq!(verifications.len(), 3);
    assert!(verifications.iter().all(|item| item["passed"] == true));

    let bounded = &fixture["preflight"]["h2o_ccpvdz_all_electron_bounded"];
    assert_eq!(bounded["all_electron"], true);
    assert_eq!(bounded["full_fci_executed"], false);
    assert_eq!(bounded["space"]["determinants"], 1_806_590_016_u64);

    let robustness = &fixture["robustness_array"];
    assert_eq!(robustness["all_completed"], true);
    assert_eq!(robustness["cases"].as_array().expect("cases").len(), 18);
    assert_eq!(robustness["requested_max_cpus"], 1_008);
    assert_eq!(robustness["observed_peak"]["cpus"], 280);

    let replicates = &fixture["replicate_array"];
    assert_eq!(replicates["all_completed"], true);
    assert_eq!(replicates["all_converged"], true);
    assert_eq!(replicates["all_case_energies_deterministic"], true);
    assert_eq!(replicates["sample_count"], 216);
    assert_eq!(replicates["requested_max_cpus"], 1_008);
    assert_eq!(replicates["observed_peak"]["tasks"], 10);
    assert_eq!(replicates["observed_peak"]["cpus"], 560);
    assert_eq!(
        replicates["live_pending_reason_at_8_running_tasks"],
        "AssocGrpCpuLimit"
    );
    assert_eq!(fixture["scope"]["thousand_cpu_request_submitted"], true);
    assert_eq!(fixture["scope"]["thousand_cpu_observed"], false);
}

#[test]
fn scnet_davidson_results_obey_their_requested_tolerances() {
    let fixture = fixture();
    for array_name in ["robustness_array", "replicate_array"] {
        for case in fixture[array_name]["cases"].as_array().expect("cases") {
            let tolerance = case["residual_tolerance"]
                .as_f64()
                .expect("residual tolerance");
            if let Some(samples) = case["samples"].as_array() {
                for sample in samples {
                    assert!(sample["residual_norm"].as_f64().unwrap() <= tolerance);
                }
            } else {
                assert!(case["residual_norm"].as_f64().unwrap() <= tolerance);
            }
        }
    }

    let energy_range = fixture["replicate_array"]["energy_eh"]["range"]
        .as_f64()
        .expect("energy range");
    assert!(energy_range < 1.0e-9);
}
