#!/usr/bin/env bash
#SBATCH --job-name=qh129-build-smoke
#SBATCH --partition=xhacnormalb
#SBATCH --account=giggleliu
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=56
#SBATCH --mem=196G
#SBATCH --time=00:45:00
#SBATCH --output=/work/share/giggleliu/cfys01/quantum-harness-129/slurm-%j.out
#SBATCH --error=/work/share/giggleliu/cfys01/quantum-harness-129/slurm-%j.err

set -euo pipefail
unset LC_ALL
export LANG=C

: "${QH129_ORCHESTRATION:?submit with --export=ALL,QH129_ORCHESTRATION=/absolute/path}"
source "${QH129_ORCHESTRATION}/common.sh"

readonly RUN_DIR="${QH129_RUN_ROOT}/${SLURM_JOB_ID}/build-smoke"
mkdir -p "${RUN_DIR}"

qh129_finish() {
    local status=$?
    set +e
    printf '%s\n' "${status}" >"${RUN_DIR}/exit-status.txt"
    printf '%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >"${RUN_DIR}/ended-at.txt"
    qh129_record_environment "${RUN_DIR}/environment-final.env"
    exit "${status}"
}
trap qh129_finish EXIT

printf '%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >"${RUN_DIR}/started-at.txt"

source /usr/share/Modules/init/bash
module purge
module load compiler/gcc/11.4.0
module load compiler/cmake/3.25.0

qh129_verify_source
qh129_require_toolchain
qh129_record_environment "${RUN_DIR}/environment-build.env"

readonly LOCAL_ROOT="${SLURM_TMPDIR:-/tmp}/quantum-harness-129-${SLURM_JOB_ID}"
readonly LOCAL_SOURCE="${LOCAL_ROOT}/source"
readonly LOCAL_VENDOR="${LOCAL_ROOT}/vendor"
readonly LOCAL_LIBCINT="${LOCAL_ROOT}/libcint-v6.1.2-offline"
readonly LOCAL_CARGO_HOME="${LOCAL_ROOT}/cargo"
readonly LOCAL_TARGET="${LOCAL_ROOT}/target"
readonly LOCAL_BINARY="${LOCAL_TARGET}/release/ed_workbench_rs"

mkdir -p \
    "${LOCAL_SOURCE}" \
    "${LOCAL_CARGO_HOME}" \
    "${LOCAL_TARGET}" \
    "${QH129_ARTIFACT_ROOT}"

qh129_git archive "${QH129_EXPECTED_COMMIT}" | tar -x -C "${LOCAL_SOURCE}"
cp "${QH129_TOOLCHAIN_ROOT}/vendor.tar.gz" "${LOCAL_ROOT}/vendor.tar.gz"
tar -xzf "${LOCAL_ROOT}/vendor.tar.gz" -C "${LOCAL_ROOT}"
test -d "${LOCAL_VENDOR}"
cp "${QH129_LIBCINT_ARCHIVE}" "${LOCAL_ROOT}/libcint-v6.1.2-offline.tar.gz"
tar -xzf "${LOCAL_ROOT}/libcint-v6.1.2-offline.tar.gz" -C "${LOCAL_ROOT}"
test "$(git --git-dir="${LOCAL_LIBCINT}/.git" --work-tree="${LOCAL_LIBCINT}" \
    rev-parse 'v6.1.2^{tree}')" = "${QH129_EXPECTED_LIBCINT_TREE}"

{
    printf '[source.crates-io]\n'
    printf 'replace-with = "vendored-sources"\n\n'
    printf '[source.vendored-sources]\n'
    printf 'directory = "%s"\n\n' "${LOCAL_VENDOR}"
    printf '[net]\n'
    printf 'offline = true\n'
} >"${LOCAL_CARGO_HOME}/config.toml"

export CARGO_HOME="${LOCAL_CARGO_HOME}"
export CARGO_TARGET_DIR="${LOCAL_TARGET}"
export PATH="${QH129_RUST_HOME}/bin:${PATH}"
export CARGO_NET_OFFLINE=true
export CINT_SRC="${LOCAL_LIBCINT}"
export CC="$(command -v gcc)"
export CXX="$(command -v g++)"
export CMAKE="$(command -v cmake)"
cmake --version | head -1 | grep -F 'cmake version 3.25.0'
export CARGO_BUILD_JOBS=16
export RAYON_NUM_THREADS=56

(
    cd "${LOCAL_SOURCE}"
    /usr/bin/time -v -o "${RUN_DIR}/cargo-build.time" \
        cargo build --release --locked --offline
    /usr/bin/time -v -o "${RUN_DIR}/cargo-test.time" \
        cargo test --locked --all-targets --offline
)

test -x "${LOCAL_BINARY}"
if test -e "${QH129_BINARY}"; then
    test "$(qh129_sha256 "${LOCAL_BINARY}")" = "$(qh129_sha256 "${QH129_BINARY}")"
else
    readonly STAGED_BINARY="${QH129_BINARY}.job-${SLURM_JOB_ID}"
    test ! -e "${STAGED_BINARY}"
    cp "${LOCAL_BINARY}" "${STAGED_BINARY}"
    chmod 755 "${STAGED_BINARY}"
    mv "${STAGED_BINARY}" "${QH129_BINARY}"
fi
qh129_record_environment "${RUN_DIR}/environment-binary.env"

"${QH129_BINARY}" verify \
    "${LOCAL_SOURCE}/fixtures/h2-equilibrium-sto3g/FCIDUMP" \
    "${LOCAL_SOURCE}/fixtures/h2-equilibrium-sto3g/reference.json" \
    >"${RUN_DIR}/verify-h2-equilibrium.stdout" \
    2>"${RUN_DIR}/verify-h2-equilibrium.stderr"

"${QH129_BINARY}" verify \
    "${LOCAL_SOURCE}/fixtures/h4-sto3g/FCIDUMP" \
    "${LOCAL_SOURCE}/fixtures/h4-sto3g/reference.json" \
    >"${RUN_DIR}/verify-h4.stdout" \
    2>"${RUN_DIR}/verify-h4.stderr"

"${QH129_BINARY}" verify \
    "${LOCAL_SOURCE}/fixtures/h2o-sto3g/FCIDUMP" \
    "${LOCAL_SOURCE}/fixtures/h2o-sto3g/reference.json" \
    >"${RUN_DIR}/verify-h2o-sto3g.stdout" \
    2>"${RUN_DIR}/verify-h2o-sto3g.stderr"

/usr/bin/time -v -o "${RUN_DIR}/ccpvdz.time" \
    "${QH129_BINARY}" benchmark h2o-cc-pvdz \
    --sources 16 \
    --memory-budget-gib 2 \
    --json-output "${RUN_DIR}/h2o-ccpvdz-ae.json" \
    >"${RUN_DIR}/ccpvdz.stdout" \
    2>"${RUN_DIR}/ccpvdz.stderr"

grep -F '"full_fci_executed": false' "${RUN_DIR}/h2o-ccpvdz-ae.json"

/usr/bin/time -v -o "${RUN_DIR}/davidson.time" \
    "${QH129_BINARY}" davidson "${LOCAL_SOURCE}/fixtures/h2o-631g-fc/FCIDUMP" \
    --residual-tolerance 1e-7 \
    --max-iterations 60 \
    --max-subspace 20 \
    --parallel-blocks 56 \
    --parallel-memory-budget-gib 2 \
    --strict-parallel-memory \
    >"${RUN_DIR}/davidson.stdout" \
    2>"${RUN_DIR}/davidson.stderr"

grep -F -- '-76.12117' "${RUN_DIR}/davidson.stdout"

(
    cd "${RUN_DIR}"
    sha256sum \
        environment-build.env \
        environment-binary.env \
        h2o-ccpvdz-ae.json \
        davidson.stdout \
        verify-h2-equilibrium.stdout \
        verify-h4.stdout \
        verify-h2o-sto3g.stdout \
        >SHA256SUMS
)
