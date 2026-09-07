#!/usr/bin/env bash
#
# Step 5: Prepare and submit DSM box-to-receiver (reciprocity) Green's function computation.
# This script runs from the SEM-DSM case root.

# Reference runtime from a completed 256-task box-to-receiver DSM run.
# Queue wait time is not included; actual runtime can vary with filesystem and cluster load.
#   dsm_box_to_recv: ntasks=256, runs=1, runtime=00:23:58.
# End reference runtime.

set -euo pipefail

# --- 1. User Configuration ---
# SLURM Job Parameters
TIME_LIMIT="23:59:00"
MEM_PER_CPU="4G"

# Modules to load (leave empty if none required)
MODULES=""

# --- 2. Path Setup ---
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_DIR="${ROOT_DIR}/WORK/DSM/box_to_recv"
PARAM_FILE="${ROOT_DIR}/DATA/Par_file_SEM_DSM"
DSM_SOLVER_DIR="$(cd "${ROOT_DIR}/../../../src/DSM/src/DSM_Solver" && pwd)"
DATA_DIR="${WORK_DIR}/DATA"
PREP_SCRIPT="${ROOT_DIR}/auxiliary/prepare_box_to_recv.py"

echo "----------------------------------------------------------------------"
echo "Starting Step 5: DSM Box-to-Receiver Computation"
echo "Working Directory: ${WORK_DIR}"
echo "----------------------------------------------------------------------"

# --- 3. Validation and Setup ---
mkdir -p "${WORK_DIR}"

if [[ ! -f "${PARAM_FILE}" ]]; then
    echo "Error: Parameter file '${PARAM_FILE}' not found."
    exit 1
fi

if grep -Eq '^[[:space:]]*REUSE_DSM_DATABASE[[:space:]]*=[[:space:]]*\.([Tt][Rr][Uu][Ee])' "${PARAM_FILE}"; then
    echo "Step 5 skipped: reusing the configured shared DSM box-to-receiver database."
    exit 0
fi

# Preserve a WORK-tree entry point while keeping all generated products in the
# modern box_to_recv layout used by the parameter-driven coupling workflow.
cat > "${WORK_DIR}/run.sh" <<'RUNNER_EOF'
#!/usr/bin/env bash
set -euo pipefail
CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
exec "${CASE_DIR}/step5_dsm_box_to_recv.sh" "$@"
RUNNER_EOF
chmod +x "${WORK_DIR}/run.sh"

param() {
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
  ' "${PARAM_FILE}" 2>/dev/null || true)
  if [[ -n "${value}" ]]; then
    printf '%s\n' "${value}"
  else
    printf '%s\n' "${default}"
  fi
}

# Read Configuration from Par_file_SEM_DSM
TIME_LENGTH_DSM=$(param TIME_LENGTH_DSM_BOX_TO_RECEIVER)
PHASE_BOX=$(param PHASE_BOX_TO_RECEIVER)
COMPONENT_SET=$(param TELESEISMIC_COMPONENT "Z")
SOURCE_MODE=$(param SINGLE_FORCE_ENZ -1)
NPROC=$(param DSM_NPROC 200)
TIME_LIMIT=$(param DSM_TIME_LIMIT "23:59:00")
MEM_PER_CPU=$(param DSM_MEM_PER_CPU "4G")
SLURM_PARTITION=$(param SLURM_PARTITION "")
SLURM_QOS=$(param SLURM_QOS "")
SBATCH_PARTITION_DIRECTIVE=""
SBATCH_QOS_DIRECTIVE=""
if [[ -n "${SLURM_PARTITION}" ]]; then
    SBATCH_PARTITION_DIRECTIVE="#SBATCH --partition=${SLURM_PARTITION}"
fi
if [[ -n "${SLURM_QOS}" ]]; then
    SBATCH_QOS_DIRECTIVE="#SBATCH --qos=${SLURM_QOS}"
fi

if [[ ! -f "${PREP_SCRIPT}" ]]; then
    echo "Error: Preparation script '${PREP_SCRIPT}' not found."
    exit 1
fi

if [[ ! -f "${DSM_SOLVER_DIR}/dsmti" ]]; then
    echo "Error: DSM solver 'dsmti' not found in ${DSM_SOLVER_DIR}"
    exit 1
fi

# Determine which force components to run
FORCES=()
case "${SOURCE_MODE}" in
  1|2)
    FORCES=("Fr" "Ft")
    ;;
  3)
    FORCES=("Fz")
    ;;
  *)
    if [[ "${COMPONENT_SET}" == *"Z"* ]]; then
        FORCES+=("Fz")
    fi
    if [[ "${COMPONENT_SET}" == *"R"* ]] || [[ "${COMPONENT_SET}" == *"T"* ]]; then
        FORCES+=("Fr" "Ft")
    fi
    ;;
esac

if [[ ${#FORCES[@]} -eq 0 ]]; then
    echo "Error: No valid station-side DSM components found."
    echo "Check SINGLE_FORCE_ENZ=${SOURCE_MODE} and TELESEISMIC_COMPONENT=${COMPONENT_SET}."
    exit 1
fi

echo "Configured NPROC=${NPROC}; each per-force job will be capped by NFREQUENCY/4."
echo "Components to simulate: ${FORCES[*]}"

# --- 4. Preparation and Execution for each force ---
for force in "${FORCES[@]}"; do
    echo "----------------------------------------------------------------------"
    echo "Processing component: ${force}"
    
    # Run Python Script for this component
    python3 "${PREP_SCRIPT}" "${DATA_DIR}" "${force}"

    # Source metadata
    METADATA_FILE="${DATA_DIR}/box_to_recv_metadata_${force}.env"
    if [[ -f "${METADATA_FILE}" ]]; then
        source "${METADATA_FILE}"
    else
        echo "Error: Metadata file ${METADATA_FILE} not found."
        exit 1
    fi

    # Adjust NPROC based on NFREQUENCY
    CURRENT_NPROC=${NPROC}
    MAX_NPROC=$((NFREQUENCY / 4))
    if [[ "${MAX_NPROC}" -lt 1 ]]; then MAX_NPROC=1; fi
    if [[ "${CURRENT_NPROC}" -gt "${MAX_NPROC}" ]]; then
        echo "Warning: NPROC=${CURRENT_NPROC} too large for NFREQUENCY=${NFREQUENCY}. Adjusting to ${MAX_NPROC}."
        CURRENT_NPROC=${MAX_NPROC}
    fi

    # Setup execution directory
    TENSOR_DIR="${WORK_DIR}/${force}"
    mkdir -p "${TENSOR_DIR}/DATA"
    mkdir -p "${TENSOR_DIR}/OUTPUT_FILES"

    echo "Cleaning stale root-level table files in ${force}..."
    rm -f "${TENSOR_DIR}/depth_solid_list" "${TENSOR_DIR}/depth_fluid_list" \
          "${TENSOR_DIR}/dist_solid_list" "${TENSOR_DIR}/dist_fluid_list"

    # Copy solver and data
    cp "${DSM_SOLVER_DIR}/dsmti" "${TENSOR_DIR}/"
    cp "${DATA_DIR}/dsm_model_${force}" "${TENSOR_DIR}/DATA/dsm_model"
    cp "${DATA_DIR}/depth_solid_list" "${TENSOR_DIR}/DATA/"
    cp "${DATA_DIR}/depth_fluid_list" "${TENSOR_DIR}/DATA/"
    cp "${DATA_DIR}/dist_solid_list" "${TENSOR_DIR}/DATA/"
    cp "${DATA_DIR}/dist_fluid_list" "${TENSOR_DIR}/DATA/"
    cp "${METADATA_FILE}" "${TENSOR_DIR}/DATA/"

    # Create subdirectories for DSM outputs
    sub_dir_output=(coef_cAnddcdr disp_fluid disp_solid stress potential pressure velo_solid)
    for subdir in "${sub_dir_output[@]}"; do
      rm -rf "${TENSOR_DIR}/OUTPUT_FILES/${subdir}"
      mkdir -p "${TENSOR_DIR}/OUTPUT_FILES/${subdir}"
    done

    # SLURM Script Generation
    echo "Generating submit_DSM_${force}.sbatch..."
    cat > "${TENSOR_DIR}/submit_DSM_${force}.sbatch" <<EOF
#!/bin/bash
#SBATCH --time=${TIME_LIMIT}
#SBATCH --ntasks=${CURRENT_NPROC}
#SBATCH --mem-per-cpu=${MEM_PER_CPU}
#SBATCH -J DSM_box_to_recv_${force}
#SBATCH -o OUTPUT_FILES/slurm-%j.out
#SBATCH -e OUTPUT_FILES/slurm-%j.out
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

$( [[ -n "${MODULES}" ]] && echo "module load ${MODULES}" || echo "# No modules to load" )
set -e
mpirun ./dsmti < DATA/dsm_model
EOF
    chmod +x "${TENSOR_DIR}/submit_DSM_${force}.sbatch"

    # Job Submission
    if command -v sbatch >/dev/null 2>&1; then
        echo "Submitting DSM box-to-receiver job for ${force}..."
        JOB_ID=$(cd "${TENSOR_DIR}" && sbatch --parsable "submit_DSM_${force}.sbatch")
        echo "Submitted DSM job: ${JOB_ID}"
    else
        echo "Error: sbatch not found. Only sbatch script was generated."
    fi
done

echo "----------------------------------------------------------------------"
echo "Step 5 completed."
echo "----------------------------------------------------------------------"
cd "$ROOT_DIR"
