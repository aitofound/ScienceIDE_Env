#!/usr/bin/env bash
#
# Step 9: Apply the SEM-DSM coupling integral to produce teleseismic seismograms.
# This script consumes existing DSM box-to-receiver data.

# Observed runtime estimate from completed Slurm logs for this case.
# Queue wait time is not included; actual runtime can vary with filesystem and cluster load.
#   coupling: ntasks=256, runs=2, range=00:04:49-00:04:53, avg=00:04:51.
# End observed runtime estimate.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PARAM_FILE="${ROOT_DIR}/DATA/Par_file_SEM_DSM"
STATION_FILE="${ROOT_DIR}/DATA/tele_station.txt"
WORK_DIR="${ROOT_DIR}/WORK/Coupling"
DATA_DIR="${WORK_DIR}/DATA"
OUTPUT_DIR="${WORK_DIR}/OUTPUT_FILES"

MODULES=""
MPIRUN_CMD="mpirun"

echo "----------------------------------------------------------------------"
echo "Starting Step 9: SEM-DSM Coupling Integral"
echo "Working Directory: ${WORK_DIR}"
echo "----------------------------------------------------------------------"

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

normalize_bool() {
  local value
  value=$(printf '%s' "$1" | tr '[:upper:]' '[:lower:]')
  case "${value}" in
    .true.|true|t|1|yes|y) printf '.true.\n' ;;
    .false.|false|f|0|no|n) printf '.false.\n' ;;
    *) echo "Error: invalid logical value '$1'" >&2; return 1 ;;
  esac
}

resolve_case_path() {
  local path=$1
  if [[ -z "${path}" ]]; then
    return 1
  fi
  if [[ "${path}" = /* ]]; then
    printf '%s\n' "${path}"
  else
    printf '%s\n' "${ROOT_DIR}/${path}"
  fi
}

normalize_dsm_structure() {
  local file=$1
  awk '
    function clean(line) {
      sub(/#.*/, "", line)
      gsub(/[dD]/, "e", line)
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", line)
      gsub(/[[:space:]]+/, " ", line)
      return line
    }
    NR == 4 { nz = int($1); last = 4 + 6 * nz }
    NR >= 4 && NR <= last {
      line = clean($0)
      if (line != "") print line
    }
  ' "${file}"
}

current_dsm_model_for_check() {
  if [[ -f "${ROOT_DIR}/WORK/SPECFEM3D/DATA/dsm_model_input" ]]; then
    printf '%s\n' "${ROOT_DIR}/WORK/SPECFEM3D/DATA/dsm_model_input"
  else
    printf '%s\n' "${ROOT_DIR}/DATA/dsm_model_base"
  fi
}

check_dsm_model_file_matches_current() {
  local label=$1
  local reference_model=$2
  local current_model
  current_model=$(current_dsm_model_for_check)
  if [[ ! -f "${current_model}" ]]; then
    echo "Error: current DSM model file not found: ${current_model}" >&2
    exit 1
  fi
  if [[ ! -f "${reference_model}" ]]; then
    echo "Error: ${label} DSM model file not found: ${reference_model}" >&2
    exit 1
  fi
  if ! diff -q <(normalize_dsm_structure "${current_model}") <(normalize_dsm_structure "${reference_model}") >/dev/null; then
    echo "Error: ${label} DSM model is inconsistent with this case." >&2
    echo "  current:   ${current_model}" >&2
    echo "  reference: ${reference_model}" >&2
    echo "The comparison uses the generated dsm_model_input when present, so attenuation-related model changes are included." >&2
    exit 1
  fi
  echo "DSM model check passed for ${label}: ${reference_model}"
}

first_existing_file() {
  local file
  for file in "$@"; do
    if [[ -f "${file}" ]]; then
      printf '%s\n' "${file}"
      return 0
    fi
  done
  return 1
}

check_src_to_box_dsm_model() {
  local src_dir=$1
  local model
  model=$(first_existing_file \
    "${src_dir}/DATA/dsm_model_input" \
    "${src_dir}/explosion/DATA/dsm_model_input" \
    "${src_dir}/DATA/dsm_model" \
    "${src_dir}/explosion/DATA/dsm_model") || {
    echo "Error: could not find a DSM source-to-box model under ${src_dir}" >&2
    exit 1
  }
  check_dsm_model_file_matches_current "source-to-box" "${model}"
}

check_box_to_recv_dsm_model() {
  local box_dir=$1
  local comp=$2
  local model
  model=$(first_existing_file \
    "${box_dir}/${comp}/DATA/dsm_model_input" \
    "${box_dir}/${comp}/DATA/dsm_model" \
    "${box_dir}/DATA/dsm_model_${comp}" \
    "${box_dir}/DATA/dsm_model_input") || {
    echo "Error: could not find a DSM box-to-receiver model for ${comp} under ${box_dir}" >&2
    exit 1
  }
  check_dsm_model_file_matches_current "box-to-receiver ${comp}" "${model}"
}

canonical_dir() {
  local path=$1
  if [[ -d "${path}" ]]; then
    cd "${path}" && pwd
  else
    return 1
  fi
}

infer_case_root_from_injected_output() {
  local injected_output=$1
  local injected_work work_dir
  injected_work=$(cd "$(dirname "${injected_output}")" && pwd)
  work_dir=$(cd "${injected_work}/.." && pwd)
  cd "${work_dir}/.." && pwd
}

injected_src_to_box_from_par_file() {
  local injected_output=$1
  local par_file="${injected_output}/InjectedWaves_Par_file"
  local candidate

  for par_file in "${par_file}" "$(cd "$(dirname "${injected_output}")" && pwd)/DATA/Par_file"; do
    if [[ ! -f "${par_file}" ]]; then
      continue
    fi
    candidate=$(awk 'NR == 3 && $0 !~ /^[[:space:]]*[[:alnum:]_]+[[:space:]]*=/ { gsub(/^[[:space:]]+|[[:space:]]+$/, "", $0); sub(/[[:space:]]*\/$/, "", $0); print; exit }' "${par_file}")
    if [[ -n "${candidate}" && -d "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done
  return 1
}


check_injected_wavefield_consistency() {
  local injected_output=$1
  local src_to_box_dir=${2:-}
  local associated_src=""
  local injected_case ref_model

  if [[ ! -d "${injected_output}" ]]; then
    echo "Error: injected wavefield directory not found: ${injected_output}" >&2
    exit 1
  fi

  associated_src=$(injected_src_to_box_from_par_file "${injected_output}" || true)
  if [[ -n "${associated_src}" ]]; then
    associated_src=$(canonical_dir "${associated_src}") || {
      echo "Error: InjectedWaves_Par_file points to missing DSM source-to-box directory:" >&2
      echo "  ${associated_src}" >&2
      exit 1
    }
  else
    injected_case=$(infer_case_root_from_injected_output "${injected_output}")
    associated_src=$(canonical_dir "${injected_case}/WORK/DSM/src_to_box" || true)
  fi

  if [[ -z "${associated_src}" ]]; then
    echo "Error: could not determine DSM source-to-box directory associated with injected waves." >&2
    echo "  injected waves: ${injected_output}" >&2
    exit 1
  fi

  check_src_to_box_dsm_model "${associated_src}"

  if [[ -n "${src_to_box_dir}" ]]; then
    local src_canon
    src_canon=$(canonical_dir "${src_to_box_dir}") || {
      echo "Error: provided DSM source-to-box directory not found: ${src_to_box_dir}" >&2
      exit 1
    }
    if [[ "${src_canon}" != "${associated_src}" ]]; then
      echo "Error: injected waves were not generated from the provided DSM source-to-box database." >&2
      echo "  provided DSM_SRC_TO_BOX_DIR: ${src_canon}" >&2
      echo "  injected-wave source:       ${associated_src}" >&2
      exit 1
    fi
  fi

  echo "InjectedWaves source-to-box check passed: ${associated_src}"
}

check_sem_tables_against_injected_wavefield() {
  local current_db=$1
  local injected_output=$2
  local injected_case ref_db rel

  injected_case=$(infer_case_root_from_injected_output "${injected_output}")
  ref_db="${injected_case}/WORK/SPECFEM3D/OUTPUT_FILES/DATABASES_MPI"
  if [[ ! -d "${ref_db}" ]]; then
    echo "Error: could not find SPECFEM database associated with injected waves:" >&2
    echo "  ${ref_db}" >&2
    exit 1
  fi
  for rel in \
    depth_table_elastic depth_table_acoustic \
    dist_table_elastic dist_table_acoustic \
    depth_table_elastic_inner depth_table_acoustic_inner \
    dist_table_elastic_inner dist_table_acoustic_inner; do
    if [[ -f "${current_db}/${rel}" || -f "${ref_db}/${rel}" ]]; then
      if [[ ! -f "${current_db}/${rel}" || ! -f "${ref_db}/${rel}" ]]; then
        echo "Error: SEM table mismatch for ${rel}; file exists in only one database." >&2
        echo "  current:   ${current_db}/${rel}" >&2
        echo "  reference: ${ref_db}/${rel}" >&2
        exit 1
      fi
      if ! cmp -s "${current_db}/${rel}" "${ref_db}/${rel}"; then
        echo "Error: SEM table ${rel} differs from the database associated with injected waves." >&2
        echo "  current:   ${current_db}/${rel}" >&2
        echo "  reference: ${ref_db}/${rel}" >&2
        echo "Injected wavefields cannot be reused unless these depth/distance tables match." >&2
        exit 1
      fi
    fi
  done
  echo "SEM depth/distance table check passed against injected-wave database: ${ref_db}"
}

find_coupling_bin() {
  local override=$1
  if [[ -n "${override}" ]]; then
    resolve_case_path "${override}"
    return
  fi

  local candidates=(
    "${ROOT_DIR}/../../../src/Coupling/src/coupling_integral"
    "${ROOT_DIR}/../../../src/Coupling/src_CouplingDSMDSM/coupling_integral"
    "${WORK_DIR}/coupling_integral"
  )
  local candidate
  for candidate in "${candidates[@]}"; do
    if [[ -x "${candidate}" || -f "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return
    fi
  done
}

if [[ ! -f "${PARAM_FILE}" ]]; then
  echo "Error: missing ${PARAM_FILE}"
  exit 1
fi
if [[ ! -f "${STATION_FILE}" ]]; then
  echo "Error: missing ${STATION_FILE}"
  exit 1
fi

SEM_OUTPUT_DIR=$(param COUPLING_SEM_OUTPUT_DIR "")
DSM_BOX_TO_RECV_DIR=$(param COUPLING_DSM_BOX_TO_RECV_DIR "")
if [[ -z "${SEM_OUTPUT_DIR}" ]]; then
  SEM_OUTPUT_DIR="${ROOT_DIR}/WORK/SPECFEM3D/OUTPUT_FILES"
else
  SEM_OUTPUT_DIR=$(resolve_case_path "${SEM_OUTPUT_DIR}")
fi
if [[ -z "${DSM_BOX_TO_RECV_DIR}" ]]; then
  DSM_BOX_TO_RECV_DIR=$(param DSM_BOX_TO_RECV_DIR "")
fi
if [[ -z "${DSM_BOX_TO_RECV_DIR}" ]]; then
  DSM_BOX_TO_RECV_DIR="${ROOT_DIR}/WORK/DSM/box_to_recv"
else
  DSM_BOX_TO_RECV_DIR=$(resolve_case_path "${DSM_BOX_TO_RECV_DIR}")
fi

NPOINTS_TAPER=$(param COUPLING_NPOINTS_TAPER_SEM 120)
NFIT_DEP=$(param COUPLING_NFIT_DEP 4)
NFIT_DIST=$(param COUPLING_NFIT_DIST 4)
NPROC=$(param COUPLING_NPROC 512)
TIME_LIMIT=$(param COUPLING_TIME_LIMIT "06:50:00")
MEM_PER_CPU=$(param COUPLING_MEM_PER_CPU "4G")

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
TELE_COMPONENTS=$(param TELESEISMIC_COMPONENT "Z")

default_vertical=.false.
default_radial=.false.
default_transverse=.false.
if [[ "${TELE_COMPONENTS}" == *"Z"* || "${TELE_COMPONENTS}" == *"z"* ]]; then
  default_vertical=.true.
fi
if [[ "${TELE_COMPONENTS}" == *"R"* || "${TELE_COMPONENTS}" == *"r"* ]]; then
  default_radial=.true.
fi
if [[ "${TELE_COMPONENTS}" == *"T"* || "${TELE_COMPONENTS}" == *"t"* ]]; then
  default_transverse=.true.
fi

vertical_raw=$(param COUPLING_VERTICAL_COMPONENT "")
radial_raw=$(param COUPLING_RADIAL_COMPONENT "")
transverse_raw=$(param COUPLING_TRANSVERSE_COMPONENT "")
VERTICAL_COMPONENT=$(normalize_bool "${vertical_raw:-${default_vertical}}")
RADIAL_COMPONENT=$(normalize_bool "${radial_raw:-${default_radial}}")
TRANSVERSE_COMPONENT=$(normalize_bool "${transverse_raw:-${default_transverse}}")

COUPLING_BIN=$(find_coupling_bin "$(param COUPLING_INTEGRAL_BIN "")" || true)
if [[ -z "${COUPLING_BIN}" ]]; then
  echo "Error: coupling_integral executable was not found."
  echo "Build/copy it, or set COUPLING_INTEGRAL_BIN in ${PARAM_FILE}."
  exit 1
fi

if [[ ! -d "${SEM_OUTPUT_DIR}/DATABASES_MPI" ]]; then
  echo "Error: missing SEM coupling database directory: ${SEM_OUTPUT_DIR}/DATABASES_MPI"
  echo "Run step3 and step8 before coupling."
  exit 1
fi
if [[ -x "${ROOT_DIR}/check_sem_box_compatibility.sh" ]]; then
  "${ROOT_DIR}/check_sem_box_compatibility.sh" \
    --current-data-dir "${ROOT_DIR}/WORK/SPECFEM3D/DATA" \
    --param-file "${PARAM_FILE}"
fi
if [[ ! -d "${DSM_BOX_TO_RECV_DIR}" ]]; then
  echo "Error: missing DSM box-to-receiver directory: ${DSM_BOX_TO_RECV_DIR}"
  echo "Set DSM_BOX_TO_RECV_DIR or COUPLING_DSM_BOX_TO_RECV_DIR in ${PARAM_FILE}."
  exit 1
fi

INJECTED_WAVEFIELD_PATH_RAW=$(param INJECTED_WAVEFIELD_PATH "")
if [[ -n "${INJECTED_WAVEFIELD_PATH_RAW}" ]]; then
  INJECTED_WAVEFIELD_PATH=$(resolve_case_path "${INJECTED_WAVEFIELD_PATH_RAW}")
  check_injected_wavefield_consistency "${INJECTED_WAVEFIELD_PATH}" ""
  check_sem_tables_against_injected_wavefield "${SEM_OUTPUT_DIR}/DATABASES_MPI" "${INJECTED_WAVEFIELD_PATH}"
fi

for pair in "Fz:${VERTICAL_COMPONENT}" "Fr:${RADIAL_COMPONENT}" "Ft:${TRANSVERSE_COMPONENT}"; do
  comp=${pair%%:*}
  enabled=${pair##*:}
  if [[ "${enabled}" == ".true." ]]; then
    check_box_to_recv_dsm_model "${DSM_BOX_TO_RECV_DIR}" "${comp}"
    if [[ ! -f "${DSM_BOX_TO_RECV_DIR}/${comp}/OUTPUT_FILES/DSM_Par_file" ]]; then
      echo "Error: coupling component ${comp} is enabled, but DSM output is missing:"
      echo "  ${DSM_BOX_TO_RECV_DIR}/${comp}/OUTPUT_FILES/DSM_Par_file"
      echo "Use a DSM box-to-receiver database containing this component."
      exit 1
    fi
  fi
done

mkdir -p "${DATA_DIR}" "${OUTPUT_DIR}"
cp "${COUPLING_BIN}" "${WORK_DIR}/coupling_integral"
chmod +x "${WORK_DIR}/coupling_integral"

awk '
  $0 ~ /^[[:space:]]*#/ || NF < 4 { next }
  {
    name = $1 "_" $2
    gsub(/[^A-Za-z0-9_.-]/, "_", name)
    n += 1
    line[n] = sprintf("%s %.8f %.8f", name, $3, $4)
  }
  END {
    if (n < 1) {
      exit 1
    }
    print n
    for (i = 1; i <= n; i++) print line[i]
  }
' "${STATION_FILE}" > "${DATA_DIR}/STATION" || {
  echo "Error: failed to convert ${STATION_FILE} to coupling DATA/STATION"
  exit 1
}

cat > "${DATA_DIR}/Par_file" <<EOF
directory_SEM_input= ${SEM_OUTPUT_DIR}
directory_DSM_input= ${DSM_BOX_TO_RECV_DIR}
npoints_taper_SEM= ${NPOINTS_TAPER}
vertical_component= ${VERTICAL_COMPONENT}
radial_component= ${RADIAL_COMPONENT}
transverse_component= ${TRANSVERSE_COMPONENT}
nfit_dep= ${NFIT_DEP}
nfit_dist= ${NFIT_DIST}
EOF

COUPLING_PAR="${SEM_OUTPUT_DIR}/DATABASES_MPI/SEM_Coupling_Par_file"
if [[ -f "${COUPLING_PAR}" ]]; then
  NPACKAGE_SEM=$(awk -F= '$1 ~ /^[[:space:]]*npackage_SEM[[:space:]]*$/ { gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2); print int($2); exit }' "${COUPLING_PAR}")
  if [[ -n "${NPACKAGE_SEM}" && "${NPACKAGE_SEM}" -gt 0 && "${NPROC}" -gt "${NPACKAGE_SEM}" ]]; then
    echo "Warning: COUPLING_NPROC=${NPROC} is larger than npackage_SEM=${NPACKAGE_SEM}; using ${NPACKAGE_SEM}."
    NPROC=${NPACKAGE_SEM}
  fi
fi

SUBMIT_COUPLING=1
if ! find "${SEM_OUTPUT_DIR}" -path '*/DATABASES_MPIiproc*/disp_pack*' -type f -print -quit | grep -q .; then
  echo "Warning: no SEM displacement pack files found under ${SEM_OUTPUT_DIR}/DATABASES_MPIiproc*/disp_pack*."
  echo "Run the SPECFEM3D solver from step8 before submitting coupling."
  SUBMIT_COUPLING=0
fi
if ! find "${SEM_OUTPUT_DIR}" -path '*/DATABASES_MPIiproc*/traction_pack*' -type f -print -quit | grep -q .; then
  echo "Warning: no SEM traction pack files found under ${SEM_OUTPUT_DIR}/DATABASES_MPIiproc*/traction_pack*."
  echo "Run the SPECFEM3D solver from step8 before submitting coupling."
  SUBMIT_COUPLING=0
fi

cd "${WORK_DIR}"
cat > submit_coupling.sbatch <<EOF
#!/bin/bash
#SBATCH --time=${TIME_LIMIT}
#SBATCH --ntasks=${NPROC}
#SBATCH --mem-per-cpu=${MEM_PER_CPU}
#SBATCH -J coupling_$(basename "${ROOT_DIR}")
#SBATCH -o OUTPUT_FILES/slurm-coupling-%j.out
#SBATCH -e OUTPUT_FILES/slurm-coupling-%j.out
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
${MPIRUN_CMD} ./coupling_integral
EOF
chmod +x submit_coupling.sbatch

echo "Coupling input written to ${DATA_DIR}/Par_file"
echo "Coupling stations written to ${DATA_DIR}/STATION"
echo "Components: Z=${VERTICAL_COMPONENT}, R=${RADIAL_COMPONENT}, T=${TRANSVERSE_COMPONENT}"
echo "NPROC=${NPROC}"

if [[ "${SUBMIT_COUPLING}" -eq 1 ]]; then
  if command -v sbatch >/dev/null 2>&1; then
    echo "Submitting coupling job..."
    JOB_ID=$(sbatch --parsable submit_coupling.sbatch)
    echo "Submitted coupling job: ${JOB_ID}"
  else
    echo "Error: sbatch not found. Only submit_coupling.sbatch was generated."
    exit 1
  fi
else
  echo "Coupling submission skipped. The input files and sbatch script were generated."
fi

echo "----------------------------------------------------------------------"
cd "${ROOT_DIR}"
