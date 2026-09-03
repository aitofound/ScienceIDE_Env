#!/usr/bin/env bash

set -euo pipefail
unset LC_ALL
export LANG=C

: "${QH129_ORCHESTRATION:?missing orchestration path}"
: "${SLURM_PROCID:?worker must run under srun}"
: "${SLURM_JOB_ID:?worker must run under Slurm}"

source "${QH129_ORCHESTRATION}/common.sh"

readonly TOLERANCES=(1e-6 1e-7 1e-8)
readonly SUBSPACES=(12 16 20 24 32 48)
readonly GROUPS_PER_CASE=4
readonly REPLICATES_PER_GROUP=3
readonly CASE_INDEX=$((SLURM_PROCID / GROUPS_PER_CASE))
readonly GROUP_INDEX=$((SLURM_PROCID % GROUPS_PER_CASE))
readonly TOLERANCE="${TOLERANCES[$((CASE_INDEX / 6))]}"
readonly SUBSPACE="${SUBSPACES[$((CASE_INDEX % 6))]}"
printf -v CASE_ID '%03d' "${CASE_INDEX}"
printf -v GROUP_ID '%02d' "${GROUP_INDEX}"
readonly RUN_DIR="${QH129_RUN_ROOT}/${SLURM_JOB_ID}/davidson-gang/${CASE_ID}/group-${GROUP_ID}"
mkdir -p "${RUN_DIR}"

qh129_finish() {
    local status=$?
    set +e
    printf '%s\n' "${status}" >"${RUN_DIR}/exit-status.txt"
    printf '%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >"${RUN_DIR}/ended-at.txt"
    qh129_record_environment "${RUN_DIR}/environment-final.env"
    (
        cd "${RUN_DIR}"
        find . -type f ! -name SHA256SUMS -print0 \
            | sort -z \
            | xargs -0 sha256sum \
            >SHA256SUMS
    )
    exit "${status}"
}
trap qh129_finish EXIT

qh129_activate_toolchain
qh129_verify_source
test -x "${QH129_BINARY}"

export RAYON_NUM_THREADS=14
export OMP_NUM_THREADS=1
export OPENBLAS_NUM_THREADS=1
export MKL_NUM_THREADS=1

{
    printf 'schema=quantum-harness-129.scnet-davidson-gang.v1\n'
    printf 'case_id=%s\n' "${CASE_ID}"
    printf 'group_id=%s\n' "${GROUP_ID}"
    printf 'slurm_procid=%s\n' "${SLURM_PROCID}"
    printf 'residual_tolerance=%s\n' "${TOLERANCE}"
    printf 'max_subspace=%s\n' "${SUBSPACE}"
    printf 'max_iterations=100\n'
    printf 'parallel_blocks=14\n'
    printf 'rayon_threads=14\n'
    printf 'replicates_per_group=%s\n' "${REPLICATES_PER_GROUP}"
    printf 'reference_energy_eh=%s\n' "${QH129_REFERENCE_ENERGY}"
    printf 'started_at=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} >"${RUN_DIR}/group.env"

qh129_record_environment "${RUN_DIR}/environment.env"
printf 'replicate\tenergy_eh\tresidual_norm\titerations\telapsed_seconds\tcpu_percent\tmax_rss_kib\n' \
    >"${RUN_DIR}/summary.tsv"

for ((offset = 0; offset < REPLICATES_PER_GROUP; offset++)); do
    replicate=$((GROUP_INDEX * REPLICATES_PER_GROUP + offset))
    printf -v REPLICATE_ID '%02d' "${replicate}"
    replicate_dir="${RUN_DIR}/replicate-${REPLICATE_ID}"
    mkdir -p "${replicate_dir}"

    /usr/bin/time -v -o "${replicate_dir}/davidson.time" \
        "${QH129_BINARY}" davidson "${QH129_FCIDUMP}" \
        --residual-tolerance "${TOLERANCE}" \
        --max-iterations 100 \
        --max-subspace "${SUBSPACE}" \
        --parallel-blocks 14 \
        --parallel-memory-budget-gib 2 \
        --strict-parallel-memory \
        >"${replicate_dir}/davidson.stdout" \
        2>"${replicate_dir}/davidson.stderr"

    grep -Fx 'converged: true' "${replicate_dir}/davidson.stdout" >/dev/null
    energy="$(awk '/^energy:/{print $2}' "${replicate_dir}/davidson.stdout")"
    residual="$(awk '/^residual norm:/{print $3}' "${replicate_dir}/davidson.stdout")"
    iterations="$(awk '/^iterations:/{print $2}' "${replicate_dir}/davidson.stdout")"
    elapsed="$(awk -F: '/Elapsed \\(wall clock\\)/{gsub(/^[ \t]+/,"",$NF); print $NF}' \
        "${replicate_dir}/davidson.time")"
    cpu_percent="$(awk -F: '/Percent of CPU/{gsub(/^[ \t]+|%$/,"",$2); print $2}' \
        "${replicate_dir}/davidson.time")"
    max_rss_kib="$(awk -F: '/Maximum resident set/{gsub(/^[ \t]+/,"",$2); print $2}' \
        "${replicate_dir}/davidson.time")"

    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "${REPLICATE_ID}" "${energy}" "${residual}" "${iterations}" \
        "${elapsed}" "${cpu_percent}" "${max_rss_kib}" \
        >>"${RUN_DIR}/summary.tsv"
done
