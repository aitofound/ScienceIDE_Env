#!/usr/bin/env bash
#
# Step 7: Convert DSM source-to-box frequency-domain wavefield to time domain,
# cut the injection window, and save wavefields for the SPECFEM3D solver.
# This script runs from the its demo root.

# Observed runtime estimate from completed Slurm logs for this case.
# Queue wait time is not included; actual runtime can vary with filesystem and cluster load.
#   injected_waves: ntasks=113, runs=1, runtime=00:09:51.
# End observed runtime estimate.

set -euo pipefail

# --- 1. User Configuration ---
TIME_LIMIT="02:00:00"
MEM_PER_CPU="8G"
MODULES=""
MPIRUN_CMD="mpirun"

# --- 2. Path Setup ---
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PARAM_FILE="${ROOT_DIR}/DATA/Par_file_SEM_DSM"
WORK_DIR="${ROOT_DIR}/WORK/InjectedWaves"
DATA_DIR="${WORK_DIR}/DATA"
OUTPUT_DIR="${WORK_DIR}/OUTPUT_FILES"
DSM_SRC_TO_BOX_DIR_DEFAULT="${ROOT_DIR}/WORK/DSM/src_to_box"
SEM_DATABASE_DIR="${ROOT_DIR}/WORK/SPECFEM3D/OUTPUT_FILES/DATABASES_MPI"
PREP_SCRIPT="${ROOT_DIR}/auxiliary/prepare_injected_waves.py"

echo "----------------------------------------------------------------------"
echo "Starting Step 7: Extract Injected DSM Wavefield"
echo "Working Directory: ${WORK_DIR}"
echo "----------------------------------------------------------------------"

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


resolve_case_path() {
  local path=$1
  if [[ -z "${path}" ]]; then
    return 1
  fi
  if [[ "${path}" = /* ]]; then
    echo "${path}"
  else
    echo "${ROOT_DIR}/${path}"
  fi
}

depth_count_from_table() {
  local table=$1
  if [[ ! -f "${table}" ]]; then
    echo "Error: depth table not found: ${table}" >&2
    return 1
  fi
  awk '
    NR == 2 {
      n = int($1)
      if (n < 0 || n != $1 + 0) exit 1
      print n
      found = 1
      exit
    }
    END { if (!found) exit 1 }
  ' "${table}"
}

cap_nproc_by_depth_tables() {
  local requested=$1
  local elastic_count acoustic_count max_count
  elastic_count=$(depth_count_from_table "${SEM_DATABASE_DIR}/depth_table_elastic") || return 1
  acoustic_count=$(depth_count_from_table "${SEM_DATABASE_DIR}/depth_table_acoustic") || return 1
  if (( elastic_count > acoustic_count )); then
    max_count=${elastic_count}
  else
    max_count=${acoustic_count}
  fi
  if (( requested > max_count )); then
    echo "Warning: INJECTED_WAVES_NPROC=${requested} exceeds max depth-table count ${max_count}; using ${max_count}." >&2
    requested=${max_count}
  fi
  printf '%s\n' "${requested}"
}

NPROC=$(param INJECTED_WAVES_NPROC "$(param NPROC 200)")
TIME_LIMIT=$(param INJECTED_WAVES_TIME_LIMIT "${TIME_LIMIT}")
MEM_PER_CPU=$(param INJECTED_WAVES_MEM_PER_CPU "${MEM_PER_CPU}")
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
NPT_ONE_PACKAGE=$(param NPT_ONE_PACKAGE 600)
FILTERING=$(param INJECTED_WAVES_FILTERING 0)
FREQ_LOW=$(param INJECTED_WAVES_FREQ_LOW 0.005)
FREQ_HIGH=$(param INJECTED_WAVES_FREQ_HIGH 1.0)
SINGLE_FORCE_MODE=$(param SINGLE_FORCE_ENZ -1)
EXTRACT_BIN_OVERRIDE=$(param EXTRACT_INJECTEDWAVES_BIN "")
DSM_SRC_TO_BOX_DIR_RAW=$(param DSM_SRC_TO_BOX_DIR "")
DSM_DATABASE_DIR_RAW=$(param DSM_DATABASE_DIR "")
if [[ -n "${DSM_SRC_TO_BOX_DIR_RAW}" ]]; then
    DSM_SRC_TO_BOX_DIR=$(resolve_case_path "${DSM_SRC_TO_BOX_DIR_RAW}")
elif [[ -n "${DSM_DATABASE_DIR_RAW}" ]]; then
    DSM_SRC_TO_BOX_DIR="$(resolve_case_path "${DSM_DATABASE_DIR_RAW}")/src_to_box"
else
    DSM_SRC_TO_BOX_DIR="${DSM_SRC_TO_BOX_DIR_DEFAULT}"
fi

if [[ ! -f "${PREP_SCRIPT}" ]]; then
    echo "Error: Preparation script '${PREP_SCRIPT}' not found."
    exit 1
fi

if [[ ! -d "${SEM_DATABASE_DIR}" ]]; then
    echo "Error: SEM database directory not found: ${SEM_DATABASE_DIR}"
    echo "Run steps 1-3 and wait for SPECFEM3D mesh/database generation first."
    exit 1
fi

NPROC=$(cap_nproc_by_depth_tables "${NPROC}") || {
    echo "Error: Could not determine max depth-table count from ${SEM_DATABASE_DIR}." >&2
    exit 1
}

case "${SINGLE_FORCE_MODE}" in
  -1)
    SOURCE_TYPE=3
    SINGLE_FORCE_COMPONENTS="0 0 1"
    SOURCES=("explosion")
    ;;
  0)
    SOURCE_TYPE=2
    SINGLE_FORCE_COMPONENTS="0 0 0"
    SOURCES=("Mzz" "Mrr" "Mtt" "Mzr" "Mzt" "Mrt")
    ;;
  1)
    SOURCE_TYPE=1
    SINGLE_FORCE_COMPONENTS="0 1 0"
    SOURCES=("Fr")
    ;;
  2)
    SOURCE_TYPE=1
    SINGLE_FORCE_COMPONENTS="1 0 0"
    SOURCES=("Ft")
    ;;
  3)
    SOURCE_TYPE=1
    SINGLE_FORCE_COMPONENTS="0 0 1"
    SOURCES=("Fz")
    ;;
  *)
    echo "Error: Unsupported SINGLE_FORCE_ENZ=${SINGLE_FORCE_MODE}."
    echo "Supported values: -1 explosion, 0 moment-tensor Green's functions, 1 North, 2 East, 3 Vertical."
    exit 1
    ;;
esac

for this_source in "${SOURCES[@]}"; do
    if [[ ! -d "${DSM_SRC_TO_BOX_DIR}/${this_source}" ]]; then
        echo "Error: DSM source-to-box directory not found: ${DSM_SRC_TO_BOX_DIR}/${this_source}"
        echo "Run step4_dsm_src_to_box.sh for this source type, or set DSM_DATABASE_DIR/DSM_SRC_TO_BOX_DIR in ${PARAM_FILE}."
        exit 1
    fi
done

mkdir -p "${DATA_DIR}" "${OUTPUT_DIR}"

echo "Preparing cut-window metadata..."
python3 "${PREP_SCRIPT}" "${DATA_DIR}"

METADATA_FILE="${DATA_DIR}/injected_waves_metadata.env"
if [[ ! -f "${METADATA_FILE}" ]]; then
    echo "Error: Metadata file ${METADATA_FILE} not found."
    exit 1
fi
source "${METADATA_FILE}"

SRC_METADATA="${DSM_SRC_TO_BOX_DIR}/${SOURCES[0]}/DATA/src_to_box_metadata.env"
if [[ -f "${SRC_METADATA}" ]]; then
    source "${SRC_METADATA}"
fi

NFREQ_INJECT=$(param NFREQ_INJECT "${NFREQUENCY:-}")
if [[ -z "${NFREQ_INJECT}" ]]; then
    echo "Error: Could not determine NFREQ_INJECT."
    echo "Set NFREQ_INJECT in ${PARAM_FILE} or rerun step4 so ${SRC_METADATA} contains NFREQUENCY."
    exit 1
fi

find_extract_bin() {
    if [[ -n "${EXTRACT_BIN_OVERRIDE}" ]]; then
        printf '%s\n' "${EXTRACT_BIN_OVERRIDE}"
        return
    fi

    local candidates=(
        "${ROOT_DIR}/../../../src/InjectedWaves/extract_Injectedwaves"
        "${ROOT_DIR}/../../../src/InjectedWaves/src/extract_Injectedwaves"
        "${ROOT_DIR}/../../../src/InjectedWaves/bin/extract_Injectedwaves"
        "${ROOT_DIR}/WORK/InjectedWaves/extract_Injectedwaves"
        "${WORK_DIR}/extract_Injectedwaves"
    )

    local candidate
    for candidate in "${candidates[@]}"; do
        if [[ -x "${candidate}" || -f "${candidate}" ]]; then
            printf '%s\n' "${candidate}"
            return
        fi
    done
}

EXTRACT_BIN="$(find_extract_bin || true)"
if [[ -z "${EXTRACT_BIN}" ]]; then
    echo "Error: extract_Injectedwaves executable was not found."
    echo "Build or copy it, then set EXTRACT_INJECTEDWAVES_BIN in ${PARAM_FILE} if it is not in a standard location."
    exit 1
fi

cp "${EXTRACT_BIN}" "${WORK_DIR}/extract_Injectedwaves"
chmod +x "${WORK_DIR}/extract_Injectedwaves"

for this_source in "${SOURCES[@]}"; do
    mkdir -p "${OUTPUT_DIR}/${this_source}/velo_solid" \
             "${OUTPUT_DIR}/${this_source}/stress" \
             "${OUTPUT_DIR}/${this_source}/disp_fluid" \
             "${OUTPUT_DIR}/${this_source}/pressure" \
             "${OUTPUT_DIR}/${this_source}/chi_dot"
done

cat > "${DATA_DIR}/Par_file" <<EOF
${SOURCE_TYPE}
${SINGLE_FORCE_COMPONENTS}
${DSM_SRC_TO_BOX_DIR}/
${SEM_DATABASE_DIR}/
${NFREQ_INJECT}
${TIME_START_CUT} ${TIME_END_CUT}
${NPT_ONE_PACKAGE}
${FILTERING}
${FREQ_LOW} ${FREQ_HIGH}
EOF

cp "${DATA_DIR}/Par_file" "${OUTPUT_DIR}/InjectedWaves_Par_file"

echo "Cut window source: ${CUT_WINDOW_SOURCE}"
echo "Cut window: ${TIME_START_CUT} to ${TIME_END_CUT} s"
echo "NFREQ_INJECT=${NFREQ_INJECT}"
echo "INJECTED_WAVES_NPROC=${NPROC}"
echo "Source type=${SOURCE_TYPE}; sources=${SOURCES[*]}"

cat > "${WORK_DIR}/submit_InjectedWaves.sbatch" <<EOF
#!/bin/bash
#SBATCH --time=${TIME_LIMIT}
#SBATCH --ntasks=${NPROC}
#SBATCH --mem-per-cpu=${MEM_PER_CPU}
#SBATCH -J InjectedWaves
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
${MPIRUN_CMD} ./extract_Injectedwaves
EOF
chmod +x "${WORK_DIR}/submit_InjectedWaves.sbatch"

if command -v sbatch >/dev/null 2>&1; then
    echo "Submitting InjectedWaves job..."
    JOB_ID=$(cd "${WORK_DIR}" && sbatch --parsable submit_InjectedWaves.sbatch)
    echo "Submitted InjectedWaves job: ${JOB_ID}"
    echo "----------------------------------------------------------------------"
    echo "Step 7 completed: Job submitted to SLURM."
else
    echo "Error: sbatch not found. Only sbatch script was generated."
    exit 1
fi

echo "----------------------------------------------------------------------"
cd "${ROOT_DIR}"
