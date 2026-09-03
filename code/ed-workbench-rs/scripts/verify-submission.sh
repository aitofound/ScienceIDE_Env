#!/usr/bin/env bash

set -euo pipefail

repository_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repository_root"

cargo fmt --check
cargo clippy --all-targets -- -D warnings
cargo test --locked

while IFS= read -r -d '' json_file; do
  jq empty "$json_file"
done < <(git ls-files -z '*.json')

sha256_file() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
  else
    shasum -a 256 "$1" | awk '{print $1}'
  fi
}

while IFS= read -r -d '' reference_file; do
  expected=$(jq -r '.fcidump_sha256 // empty' "$reference_file")
  if [[ -n "$expected" ]]; then
    fcidump="$(dirname "$reference_file")/FCIDUMP"
    [[ -f "$fcidump" ]]
    actual=$(sha256_file "$fcidump")
    [[ "$actual" == "$expected" ]]
  fi
done < <(find fixtures -name reference.json -type f -print0)

benchmark_json=fixtures/h2o-ccpvdz-ae/benchmark-m4.json
jq -e '
  .schema_version == 1 and
  .norb == 24 and
  .nelec == 10 and
  .nalpha == 5 and
  .nbeta == 5 and
  .space.alpha_strings == 42504 and
  .space.beta_strings == 42504 and
  .space.determinants == 1806590016 and
  .point_group_symmetry == false and
  .full_fci_executed == false and
  .memory_budget_bytes == 2147483648 and
  .bounded_memory.conservative_peak_bytes < .memory_budget_bytes and
  .rhf_absolute_error < 1e-8
' "$benchmark_json" >/dev/null

benchmark_summary_json=fixtures/h2o-ccpvdz-ae/benchmark-m4-summary.json
jq -e '
  .schema_version == 1 and
  .artifact_kind == "h2o-ccpvdz-five-process-summary" and
  .measured_commit == "025a6dd27836f2e9011ef63ee35630a667bdd786" and
  .measured_release == "v0.1.1" and
  .determinants == 1806590016 and
  .sources_per_run == 16 and
  .raw_contributions_per_run == 640016 and
  (.runs | length) == 5 and
  .aggregate.maximum_peak_rss_bytes == 468975616 and
  .aggregate.maximum_peak_rss_bytes < 2147483648
' "$benchmark_summary_json" >/dev/null

parallel_sigma_json=fixtures/h2o-631g-fc/parallel-sigma-m4.json
jq -e '
  .schema_version == 1 and
  .artifact_kind == "h2o-631g-fc-parallel-sigma" and
  .problem.determinants == 245025 and
  .parallel_policy.source_blocks == 4 and
  .parallel_policy.preflight_workspace_bytes == 7840800 and
  (.runs | length) == 5 and
  .aggregate.median_serial_seconds == 14.181091542 and
  .aggregate.median_parallel_seconds == 4.381184834 and
  .aggregate.ratio_of_medians > 3.2 and
  .aggregate.maximum_serial_parallel_error < 1e-10
' "$parallel_sigma_json" >/dev/null

python_bin=${PYTHON:-.venv/bin/python}
if [[ ! -x "$python_bin" ]] && ! command -v "$python_bin" >/dev/null 2>&1; then
  printf 'Python oracle environment not found: %s\n' "$python_bin" >&2
  exit 1
fi
"$python_bin" -m unittest scripts.oracle.test_units -v

verification_tmp=$(mktemp -d)
trap 'rm -rf -- "$verification_tmp"' EXIT
cargo run --quiet --locked -- cc-series \
  fixtures/h4-sto3g/FCIDUMP \
  fixtures/h4-sto3g/reference.json \
  --max-rank 2 \
  --residual-tolerance 1e-8 \
  --json-output "$verification_tmp/h4-cc-series.json" >/dev/null
jq -e '
  .schema_version == 1 and
  .artifact_kind == "cc-series" and
  .energy_unit == "hartree" and
  (.results | length) == 2 and
  ([.results[].rank] == [1, 2]) and
  ([.results[].termination] == ["converged", "converged"]) and
  ([.results[].converged] | all)
' "$verification_tmp/h4-cc-series.json" >/dev/null

if cargo run --quiet --locked -- davidson \
  fixtures/h4-sto3g/FCIDUMP \
  --max-iterations 1 \
  --workspace "$verification_tmp/davidson" >/dev/null 2>&1; then
  printf 'one-iteration H4 Davidson unexpectedly converged\n' >&2
  exit 1
fi
jq -e '
  .schema_version == 1 and
  .dimension == 36 and
  .completed_iterations == 1 and
  .basis_count == .sigma_count and
  .basis_count > 0 and
  .scalar_type == "f64" and
  .byte_order == "little" and
  .last_converged == false
' "$verification_tmp/davidson/checkpoint.json" >/dev/null
cargo run --quiet --locked -- davidson \
  fixtures/h4-sto3g/FCIDUMP \
  --max-iterations 100 \
  --workspace "$verification_tmp/davidson" \
  --resume >/dev/null

git diff --check
