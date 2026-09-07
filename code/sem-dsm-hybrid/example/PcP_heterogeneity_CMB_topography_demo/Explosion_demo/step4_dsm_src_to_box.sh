#!/usr/bin/env bash
#
# Step 4: Prepare and submit DSM source-to-box Green's function computation.
# This script runs from the PcP_heterogeneity_CMB_topography_demo root.

# Observed runtime estimate from completed Slurm logs for this case.
# Queue wait time is not included; actual runtime can vary with filesystem and cluster load.
#   dsm_src_to_box: ntasks=512, runs=1, runtime=00:21:25.
# End observed runtime estimate.

set -euo pipefail

# --- 1. User Configuration ---
# SLURM Job Parameters
TIME_LIMIT="23:59:00"
MEM_PER_CPU="4G"

# Modules to load (leave empty if none required)
MODULES=""

# --- 2. Path Setup ---
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORK_DIR="${ROOT_DIR}/WORK/DSM/src_to_box"
PARAM_FILE="${ROOT_DIR}/DATA/Par_file_SEM_DSM"
DSM_SOLVER_DIR="$(cd "${ROOT_DIR}/../../../src/DSM/src/DSM_Solver" && pwd)"
DATA_DIR="${WORK_DIR}/DATA"
PREP_SCRIPT="${ROOT_DIR}/auxiliary/prepare_src_to_box.py"

echo "----------------------------------------------------------------------"
echo "Starting Step 4: DSM Source-to-Box Computation"
echo "Working Directory: ${WORK_DIR}"
echo "----------------------------------------------------------------------"

# --- 3. Validation and Setup ---
mkdir -p "${WORK_DIR}"

if [[ ! -f "${PARAM_FILE}" ]]; then
    echo "Error: Parameter file '${PARAM_FILE}' not found."
    exit 1
fi

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
TIME_LENGTH_DSM=$(param TIME_LENGTH_DSM_SOURCE_TO_BOX)
PHASE_BOX=$(param PHASE_SOURCE_TO_BOX)
NPROC=$(param DSM_NPROC 200)
TIME_LIMIT=$(param DSM_TIME_LIMIT "23:59:00")
MEM_PER_CPU=$(param DSM_MEM_PER_CPU "4G")
SOURCE_MODE=$(param SINGLE_FORCE_ENZ -1)
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

echo "Detected NPROC=${NPROC}"
echo "Running time ~4hours with 512 CPU cores"

case "${SOURCE_MODE}" in
  -1)
    SOURCES=("explosion")
    ;;
  0)
    SOURCES=("Mzz" "Mrr" "Mtt" "Mzr" "Mzt" "Mrt")
    ;;
  1)
    SOURCES=("Fr")
    ;;
  2)
    SOURCES=("Ft")
    ;;
  3)
    SOURCES=("Fz")
    ;;
  *)
    echo "Error: Unsupported SINGLE_FORCE_ENZ=${SOURCE_MODE}."
    echo "Supported values: -1 explosion, 0 moment-tensor Green's functions, 1 North, 2 East, 3 Vertical."
    exit 1
    ;;
esac

echo "Source components to simulate: ${SOURCES[*]}"

# --- 5. Execution Setup ---
cd "${WORK_DIR}"

for source in "${SOURCES[@]}"; do
    echo "----------------------------------------------------------------------"
    echo "Processing source component: ${source}"

    echo "Running auxiliary/prepare_src_to_box.py for ${source}..."
    python3 "${PREP_SCRIPT}" "${DATA_DIR}" "${source}"

    if [[ -f "${DATA_DIR}/src_to_box_metadata.env" ]]; then
        source "${DATA_DIR}/src_to_box_metadata.env"
    else
        echo "Error: Metadata file ${DATA_DIR}/src_to_box_metadata.env not found."
        exit 1
    fi

    CURRENT_NPROC=${NPROC}
    MAX_NPROC=$((NFREQUENCY / 4))
    if [[ "${MAX_NPROC}" -lt 1 ]]; then MAX_NPROC=1; fi
    if [[ "${CURRENT_NPROC}" -gt "${MAX_NPROC}" ]]; then
        echo "Warning: NPROC=${CURRENT_NPROC} too large for NFREQUENCY=${NFREQUENCY}. Adjusting to ${MAX_NPROC}."
        CURRENT_NPROC=${MAX_NPROC}
    fi

    TENSOR_DIR="${source}"
    mkdir -p "${TENSOR_DIR}/DATA"
    mkdir -p "${TENSOR_DIR}/OUTPUT_FILES"

    # dsmti reads table filenames from dsm_model with Fortran list-directed
    # character input, where / terminates the read. Keep these files in the
    # run directory root and use bare filenames in dsm_model.
    echo "Cleaning stale root-level table files in ${TENSOR_DIR}..."
    rm -f "${TENSOR_DIR}/depth_solid_list" "${TENSOR_DIR}/depth_fluid_list" \
          "${TENSOR_DIR}/dist_solid_list" "${TENSOR_DIR}/dist_fluid_list"

    cp "${DSM_SOLVER_DIR}/dsmti" "${TENSOR_DIR}/"
    cp "${DATA_DIR}/dsm_model_${source}" "${TENSOR_DIR}/DATA/dsm_model"
    cp "${DATA_DIR}/dsm_model_input" "${TENSOR_DIR}/DATA/"
    cp "${DATA_DIR}/depth_solid_list" "${TENSOR_DIR}/DATA/"
    cp "${DATA_DIR}/depth_fluid_list" "${TENSOR_DIR}/DATA/"
    cp "${DATA_DIR}/dist_solid_list" "${TENSOR_DIR}/DATA/"
    cp "${DATA_DIR}/dist_fluid_list" "${TENSOR_DIR}/DATA/"
    cp "${DATA_DIR}/src_to_box_metadata.env" "${TENSOR_DIR}/DATA/"

    sub_dir_output=(coef_cAnddcdr disp_fluid disp_solid stress potential pressure velo_solid)
    for subdir in "${sub_dir_output[@]}"; do
      rm -rf "${TENSOR_DIR}/OUTPUT_FILES/${subdir}"
      mkdir -p "${TENSOR_DIR}/OUTPUT_FILES/${subdir}"
    done

    echo "Generating submit_DSM_${source}.sbatch..."
    cat > "${TENSOR_DIR}/submit_DSM_${source}.sbatch" <<EOF
#!/bin/bash
#SBATCH --time=${TIME_LIMIT}
#SBATCH --ntasks=${CURRENT_NPROC}
#SBATCH --mem-per-cpu=${MEM_PER_CPU}
#SBATCH -J DSM_src_to_box_${source}
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

    chmod +x "${TENSOR_DIR}/submit_DSM_${source}.sbatch"

    if command -v sbatch >/dev/null 2>&1; then
        echo "Submitting DSM source-to-box job for ${source}..."
        JOB_ID=$(cd "${TENSOR_DIR}" && sbatch --parsable "submit_DSM_${source}.sbatch")
        echo "Submitted DSM job: ${JOB_ID}"
    else
        echo "Error: sbatch not found. Only sbatch script was generated."
        exit 1
    fi
done

echo "Step 4 completed: source-to-box jobs submitted to SLURM."

echo "----------------------------------------------------------------------"
cd "$ROOT_DIR"
