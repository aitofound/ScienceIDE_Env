#!/usr/bin/env bash
#SBATCH --job-name=qh129-replicates
#SBATCH --partition=xhacnormalb
#SBATCH --account=giggleliu
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=56
#SBATCH --mem=196G
#SBATCH --array=0-17%18
#SBATCH --time=00:15:00
#SBATCH --output=/work/share/giggleliu/cfys01/quantum-harness-129/slurm-%A_%a.out
#SBATCH --error=/work/share/giggleliu/cfys01/quantum-harness-129/slurm-%A_%a.err

set -euo pipefail
unset LC_ALL
export LANG=C

: "${QH129_ORCHESTRATION:?submit with --export=ALL,QH129_ORCHESTRATION=/absolute/path}"
source "${QH129_ORCHESTRATION}/common.sh"

readonly TOLERANCES=(1e-6 1e-7 1e-8)
readonly SUBSPACES=(12 16 20 24 32 48)
readonly TOLERANCE="${TOLERANCES[$((SLURM_ARRAY_TASK_ID / 6))]}"
readonly SUBSPACE="${SUBSPACES[$((SLURM_ARRAY_TASK_ID % 6))]}"
readonly REPLICATES=12
printf -v CASE_ID '%03d' "${SLURM_ARRAY_TASK_ID}"
readonly RUN_DIR="${QH129_RUN_ROOT}/${SLURM_ARRAY_JOB_ID}/davidson-replicates/${CASE_ID}"
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

source /usr/share/Modules/init/bash
module purge
module load compiler/gcc/11.4.0
qh129_activate_toolchain
qh129_verify_source

test -x "${QH129_BINARY}"
rustc --version | grep -F 'rustc 1.89.0 '

export RAYON_NUM_THREADS=56
export OMP_NUM_THREADS=1
export OPENBLAS_NUM_THREADS=1
export MKL_NUM_THREADS=1

{
    printf 'schema=quantum-harness-129.scnet-davidson-replicates.v1\n'
    printf 'case_id=%s\n' "${CASE_ID}"
    printf 'residual_tolerance=%s\n' "${TOLERANCE}"
    printf 'max_subspace=%s\n' "${SUBSPACE}"
    printf 'max_iterations=100\n'
    printf 'parallel_blocks=56\n'
    printf 'rayon_threads=56\n'
    printf 'replicates=%s\n' "${REPLICATES}"
    printf 'reference_energy_eh=%s\n' "${QH129_REFERENCE_ENERGY}"
    printf 'started_at=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} >"${RUN_DIR}/case.env"

qh129_record_environment "${RUN_DIR}/environment.env"
printf 'replicate\tenergy_eh\tresidual_norm\titerations\telapsed_seconds\tcpu_percent\tmax_rss_kib\n' \
    >"${RUN_DIR}/summary.tsv"

for ((replicate = 0; replicate < REPLICATES; replicate++)); do
    printf -v REPLICATE_ID '%02d' "${replicate}"
    readonly_rep_dir="${RUN_DIR}/replicate-${REPLICATE_ID}"
    mkdir -p "${readonly_rep_dir}"

    /usr/bin/time -v -o "${readonly_rep_dir}/davidson.time" \
        "${QH129_BINARY}" davidson "${QH129_FCIDUMP}" \
        --residual-tolerance "${TOLERANCE}" \
        --max-iterations 100 \
        --max-subspace "${SUBSPACE}" \
        --parallel-blocks 56 \
        --parallel-memory-budget-gib 2 \
        --strict-parallel-memory \
        >"${readonly_rep_dir}/davidson.stdout" \
        2>"${readonly_rep_dir}/davidson.stderr"

    grep -Fx 'converged: true' "${readonly_rep_dir}/davidson.stdout" >/dev/null
    energy="$(awk '/^energy:/{print $2}' "${readonly_rep_dir}/davidson.stdout")"
    residual="$(awk '/^residual norm:/{print $3}' "${readonly_rep_dir}/davidson.stdout")"
    iterations="$(awk '/^iterations:/{print $2}' "${readonly_rep_dir}/davidson.stdout")"
    elapsed="$(awk -F: '/Elapsed \\(wall clock\\)/{gsub(/^[ \t]+/,"",$NF); print $NF}' \
        "${readonly_rep_dir}/davidson.time")"
    cpu_percent="$(awk -F: '/Percent of CPU/{gsub(/^[ \t]+|%$/,"",$2); print $2}' \
        "${readonly_rep_dir}/davidson.time")"
    max_rss_kib="$(awk -F: '/Maximum resident set/{gsub(/^[ \t]+/,"",$2); print $2}' \
        "${readonly_rep_dir}/davidson.time")"

    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "${REPLICATE_ID}" "${energy}" "${residual}" "${iterations}" \
        "${elapsed}" "${cpu_percent}" "${max_rss_kib}" \
        >>"${RUN_DIR}/summary.tsv"
done
