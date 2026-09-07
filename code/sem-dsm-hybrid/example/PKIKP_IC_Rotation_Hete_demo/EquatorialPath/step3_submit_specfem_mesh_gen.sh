#!/usr/bin/env bash
#
# Step 3: Create and submit SPECFEM3D mesh/database Slurm jobs.
# This script runs from the PKP_precursor_ULVZ_demo root.

set -euo pipefail

# --- 1. User Configuration ---
# SLURM Job Parameters
TIME_LIMIT="01:00:00"
MEM_PER_CPU="4G"

# Modules to load (leave empty if none required)
# e.g., MODULES="intel/2021.4.0 openmpi/4.1.1"
MODULES=""

# --- 2. Path Setup ---
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORK_DIR="${ROOT_DIR}/WORK/SPECFEM3D"
DATA_DIR="${WORK_DIR}/DATA"
SEM_DSM_PARAM_FILE="${ROOT_DIR}/DATA/Par_file_SEM_DSM"
PAR_FILE="${DATA_DIR}/Par_file"
OUTPUT_DIR="${WORK_DIR}/OUTPUT_FILES"
BIN_DIR="$(cd "${ROOT_DIR}/../../../src/SPECFEM3D/bin" && pwd)"

echo "----------------------------------------------------------------------"
echo "Starting Step 3: SPECFEM3D Mesh and Database Generation"
echo "Working Directory: ${WORK_DIR}"
echo "----------------------------------------------------------------------"

# --- 3. Validation ---
if [[ ! -f "${PAR_FILE}" ]]; then
    echo "Error: SPECFEM3D parameter file '${PAR_FILE}' not found."
    echo "Please run Step 1 first."
    exit 1
fi

param_sem_dsm() {
  local key=$1
  local default=${2:-}
  local value
  value=$(awk -F= -v key="$key" '
    $0 !~ /^[[:space:]]*#/ && $1 ~ "^[[:space:]]*" key "[[:space:]]*$" {
      v=$2
      sub(/#.*/, "", v)
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", v)
      print v
      found=1
      exit
    }
    END { if (!found) exit 1 }
  ' "${SEM_DSM_PARAM_FILE}" 2>/dev/null || true)
  if [[ -n "${value}" ]]; then
    printf '%s\n' "${value}"
  else
    printf '%s\n' "${default}"
  fi
}

SLURM_PARTITION=$(param_sem_dsm SLURM_PARTITION "")
SLURM_QOS=$(param_sem_dsm SLURM_QOS "")
DSM1D_OR_3D=$(param_sem_dsm DSM1D_OR_3D DSM3D)
DSM1D_OR_3D=$(printf '%s' "${DSM1D_OR_3D}" | tr '[:lower:]' '[:upper:]')
case "${DSM1D_OR_3D}" in
  DSM1D|DSM3D) ;;
  *)
    echo "Error: DSM1D_OR_3D must be DSM1D or DSM3D, got '${DSM1D_OR_3D}'." >&2
    exit 1
    ;;
esac
SBATCH_PARTITION_DIRECTIVE=""
SBATCH_QOS_DIRECTIVE=""
if [[ -n "${SLURM_PARTITION}" ]]; then
  SBATCH_PARTITION_DIRECTIVE="#SBATCH --partition=${SLURM_PARTITION}"
fi
if [[ -n "${SLURM_QOS}" ]]; then
  SBATCH_QOS_DIRECTIVE="#SBATCH --qos=${SLURM_QOS}"
fi

# Read NPROC from Par_file
NPROC=$(awk -F= '
  $0 !~ /^[[:space:]]*#/ && $1 ~ /^[[:space:]]*NPROC[[:space:]]*$/ {
    v=$2
    sub(/#.*/, "", v)
    gsub(/^[[:space:]]+|[[:space:]]+$/, "", v)
    print v
    found=1
    exit
  }
  END { if (!found) exit 1 }
' "${PAR_FILE}") || { echo "Could not read NPROC from ${PAR_FILE}" >&2; exit 1; }

SPECFEM_ROOT="$(cd "${BIN_DIR}/.." && pwd)"
MODEL_SETUP_HELPER="${ROOT_DIR}/auxiliary/select_generate_databases_model.sh"

if [[ ! -f "${MODEL_SETUP_HELPER}" ]]; then
  echo "Error: model-selection helper is missing: ${MODEL_SETUP_HELPER}" >&2
  exit 1
fi

# shellcheck source=auxiliary/select_generate_databases_model.sh
source "${MODEL_SETUP_HELPER}"
select_generate_databases_model "${ROOT_DIR}" "${SPECFEM_ROOT}" "${DSM1D_OR_3D}"
DATABASE_BIN="${GENERATOR}"

for source_file in "${CASE_MODEL}" "${CASE_GET_MODEL}"; do
  if [[ ! -f "${source_file}" ]]; then
    echo "Error: required case model source is missing: ${source_file}" >&2
    echo "Check the selected case model sources for ${DSM1D_OR_3D}." >&2
    exit 1
  fi
done

if ! cmp -s "${CASE_MODEL}" "${ACTIVE_MODEL}" || \
   ! cmp -s "${CASE_GET_MODEL}" "${ACTIVE_GET_MODEL}"; then
  echo "Error: active SPECFEM3D model sources do not match this case." >&2
  echo "Run './step0_prepare_icb_topography_heterogeneity.sh --install' before Step 3." >&2
  exit 1
fi

if [[ ! -x "${DATABASE_BIN}" || "${ACTIVE_MODEL}" -nt "${DATABASE_BIN}" || \
      "${ACTIVE_GET_MODEL}" -nt "${DATABASE_BIN}" ]]; then
  echo "Error: xgenerate_databases is missing or older than the active model sources." >&2
  echo "Run './step0_prepare_icb_topography_heterogeneity.sh --install' before Step 3." >&2
  exit 1
fi

echo "Detected NPROC=${NPROC}"
echo "DSM1D_OR_3D=${DSM1D_OR_3D}"
echo "Database generator: ${DATABASE_BIN}"

# --- 4. Directory Preparation ---
echo "Setting up OUTPUT_FILES directory structure..."
mkdir -p "${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR}/DATABASES_MPI"

# Create per-processor database directories (required by SPECFEM3D)
for ((iproc=0; iproc<NPROC; iproc++)); do
  printf -v zeropad_iproc "%04d" "${iproc}"
  mkdir -p "${OUTPUT_DIR}/DATABASES_MPIiproc${zeropad_iproc}"
done

# --- 5. SLURM Script Generation ---
cd "$WORK_DIR"

# Helper to generate module load commands
MODULE_CMD=""
if [[ -n "${MODULES}" ]]; then
    MODULE_CMD="module load ${MODULES}"
fi

# Create SLURM script for Mesher
echo "Generating run_meshfem3D.sbatch..."
cat > run_meshfem3D.sbatch <<EOF
#!/bin/bash
#SBATCH --time=${TIME_LIMIT}
#SBATCH --ntasks=${NPROC}
#SBATCH --mem-per-cpu=${MEM_PER_CPU}
#SBATCH -J mesh_$(basename "${ROOT_DIR}")
#SBATCH -o OUTPUT_FILES/slurm-mesh-%j.out
#SBATCH -e OUTPUT_FILES/slurm-mesh-%j.out
${SBATCH_PARTITION_DIRECTIVE}
${SBATCH_QOS_DIRECTIVE}

job_start_epoch=\$(date +%s)
echo "Job started at: \$(date '+%Y-%m-%d %H:%M:%S %Z')"
print_job_runtime() {
    local job_status=\$?
    local job_end_epoch=\$(date +%s)
    local job_elapsed=\$((job_end_epoch-job_start_epoch))
    printf 'Job finished at: %s\n' "\$(date '+%Y-%m-%d %H:%M:%S %Z')"
    printf 'Job runtime: %02d:%02d:%02d (%d seconds)\n' \$((job_elapsed/3600)) \$(((job_elapsed%3600)/60)) \$((job_elapsed%60)) "\${job_elapsed}"
    printf 'Job exit status: %d\n' "\${job_status}"
}
trap print_job_runtime EXIT

${MODULE_CMD}
set -e
mpirun "${BIN_DIR}/xmeshfem3D"
EOF

# Create SLURM script for Database Generator
echo "Generating run_generate_databases.sbatch..."
cat > run_generate_databases.sbatch <<EOF
#!/bin/bash
#SBATCH --time=${TIME_LIMIT}
#SBATCH --ntasks=${NPROC}
#SBATCH --mem-per-cpu=${MEM_PER_CPU}
#SBATCH -J gen_$(basename "${ROOT_DIR}")
#SBATCH -o OUTPUT_FILES/slurm-gen-%j.out
#SBATCH -e OUTPUT_FILES/slurm-gen-%j.out
${SBATCH_PARTITION_DIRECTIVE}
${SBATCH_QOS_DIRECTIVE}

job_start_epoch=\$(date +%s)
echo "Job started at: \$(date '+%Y-%m-%d %H:%M:%S %Z')"
print_job_runtime() {
    local job_status=\$?
    local job_end_epoch=\$(date +%s)
    local job_elapsed=\$((job_end_epoch-job_start_epoch))
    printf 'Job finished at: %s\n' "\$(date '+%Y-%m-%d %H:%M:%S %Z')"
    printf 'Job runtime: %02d:%02d:%02d (%d seconds)\n' \$((job_elapsed/3600)) \$(((job_elapsed%3600)/60)) \$((job_elapsed%60)) "\${job_elapsed}"
    printf 'Job exit status: %d\n' "\${job_status}"
}
trap print_job_runtime EXIT

${MODULE_CMD}
set -e
mpirun "${DATABASE_BIN}"
EOF

chmod +x run_meshfem3D.sbatch run_generate_databases.sbatch

# --- 6. Job Submission ---
if command -v sbatch >/dev/null 2>&1; then
    echo "Submitting xmeshfem3D..."
    MESH_JOB=$(sbatch --parsable run_meshfem3D.sbatch)
    echo "Submitted mesh job: ${MESH_JOB}"

    echo "Submitting xgenerate_databases with dependency on ${MESH_JOB}..."
    GEN_JOB=$(sbatch --parsable --dependency=afterok:${MESH_JOB} run_generate_databases.sbatch)
    echo "Submitted database job: ${GEN_JOB}"
    
    echo "----------------------------------------------------------------------"
    echo "Step 3 completed: Mesh and database jobs submitted to SLURM."
    echo "Use 'squeue -u $USER' to monitor your jobs."

else
    echo "Error: sbatch not found. Only sbatch scripts were generated."
    exit 1
fi

echo "----------------------------------------------------------------------"
cd "$ROOT_DIR"
