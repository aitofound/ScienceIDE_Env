#!/usr/bin/env bash
#
# Step 8: Prepare and submit the coupled SPECFEM3D solver.
# This script uses an existing InjectedWaves output directory.

# Observed runtime estimate from completed Slurm logs for this case.
# Queue wait time is not included; actual runtime can vary with filesystem and cluster load.
#   specfem_solver: ntasks=256, runs=1, runtime=03:28:46.
# End observed runtime estimate.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PARAM_FILE="${ROOT_DIR}/DATA/Par_file_SEM_DSM"
WORK_DIR="${ROOT_DIR}/WORK/SPECFEM3D"
PAR_FILE="${WORK_DIR}/DATA/Par_file"
OUTPUT_DIR="${WORK_DIR}/OUTPUT_FILES"
BIN_DIR="$(cd "${ROOT_DIR}/../../../src/SPECFEM3D/bin" && pwd)"

MODULES=""
MPIRUN_CMD="mpirun"

echo "----------------------------------------------------------------------"
echo "Starting Step 8: SPECFEM3D Coupled Solver"
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

replace_key() {
  local file=$1
  local key=$2
  local value=$3
  awk -v key="$key" -v value="$value" '
    BEGIN { done=0 }
    $0 !~ /^[[:space:]]*#/ && $1 == key {
      printf("%-32s= %s\n", key, value)
      done=1
      next
    }
    { print }
    END {
      if (!done) {
        printf("%-32s= %s\n", key, value)
      }
    }
  ' "$file" > "${file}.tmp"
  mv "${file}.tmp" "$file"
}

to_float_or_blank() {
  local value=$1
  if [[ -z "${value}" ]]; then
    return 1
  fi
  awk -v v="${value}" 'BEGIN { gsub(/[dD]/, "e", v); if (v+0 == v) print v+0; else exit 1 }'
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

if [[ ! -f "${PARAM_FILE}" ]]; then
  echo "Error: missing ${PARAM_FILE}"
  exit 1
fi
if [[ ! -f "${PAR_FILE}" ]]; then
  echo "Error: missing ${PAR_FILE}. Run step1 first."
  exit 1
fi
if [[ ! -x "${BIN_DIR}/xspecfem3D" ]]; then
  echo "Error: missing SPECFEM3D solver executable: ${BIN_DIR}/xspecfem3D"
  exit 1
fi

INJECTED_WAVEFIELD_PATH_RAW=$(param INJECTED_WAVEFIELD_PATH "")
DSM_SRC_TO_BOX_DIR_RAW=$(param DSM_SRC_TO_BOX_DIR "")
DSM_SRC_TO_BOX_DIR=""
if [[ -n "${DSM_SRC_TO_BOX_DIR_RAW}" ]]; then
  DSM_SRC_TO_BOX_DIR=$(resolve_case_path "${DSM_SRC_TO_BOX_DIR_RAW}")
  check_src_to_box_dsm_model "${DSM_SRC_TO_BOX_DIR}"
fi
if [[ -z "${INJECTED_WAVEFIELD_PATH_RAW}" ]]; then
  INJECTED_WAVEFIELD_PATH="${ROOT_DIR}/WORK/InjectedWaves/OUTPUT_FILES"
else
  INJECTED_WAVEFIELD_PATH=$(resolve_case_path "${INJECTED_WAVEFIELD_PATH_RAW}")
fi
check_injected_wavefield_consistency "${INJECTED_WAVEFIELD_PATH}" "${DSM_SRC_TO_BOX_DIR}"
check_sem_tables_against_injected_wavefield "${WORK_DIR}/OUTPUT_FILES/DATABASES_MPI" "${INJECTED_WAVEFIELD_PATH}"
if [[ -x "${ROOT_DIR}/check_sem_box_compatibility.sh" ]]; then
  "${ROOT_DIR}/check_sem_box_compatibility.sh" \
    --current-data-dir "${WORK_DIR}/DATA" \
    --param-file "${PARAM_FILE}"
fi
INJECTED_METADATA_RAW=$(param INJECTED_WAVES_METADATA "")
if [[ -n "${INJECTED_METADATA_RAW}" ]]; then
  INJECTED_METADATA=$(resolve_case_path "${INJECTED_METADATA_RAW}")
else
  INJECTED_METADATA="$(cd "$(dirname "${INJECTED_WAVEFIELD_PATH}")" && pwd)/DATA/injected_waves_metadata.env"
fi

requested_dt=$(param SPECFEM3D_SOLVER_DT "")
dt_safety=$(param SPECFEM3D_SOLVER_DT_SAFETY_FACTOR 0.5)
dt_cap=$(param SPECFEM3D_SOLVER_MAX_DT "")

extract_suggested_dt() {
  local file
  for file in "${OUTPUT_DIR}/output_mesher.txt" "${OUTPUT_DIR}/output_meshfem3D.txt"; do
    [[ -f "${file}" ]] || continue
    awk '
      BEGIN { IGNORECASE=1 }
      /Maximum suggested time step/ {
        for (i=1; i<=NF; i++) {
          if ($i ~ /^[0-9.+-]+([eEdD][+-]?[0-9]+)?$/) {
            v=$i
          }
        }
      }
      END { if (v != "") { gsub(/[dD]/, "e", v); print v } }
    ' "${file}"
    return
  done
}

if [[ -n "${requested_dt}" ]]; then
  DT=$(to_float_or_blank "${requested_dt}") || { echo "Error: bad SPECFEM3D_SOLVER_DT=${requested_dt}"; exit 1; }
  DT_SOURCE="user-specified SPECFEM3D_SOLVER_DT"
else
  suggested_dt=$(extract_suggested_dt || true)
  if [[ -z "${suggested_dt}" ]]; then
    if [[ -n "${dt_cap}" ]]; then
      DT=$(to_float_or_blank "${dt_cap}") || { echo "Error: bad SPECFEM3D_SOLVER_MAX_DT=${dt_cap}"; exit 1; }
      DT_SOURCE="SPECFEM3D_SOLVER_MAX_DT fallback; no suggested DT found"
    else
      echo "Error: could not find suggested DT in output_meshfem3D.txt/output_mesher.txt."
      echo "Set SPECFEM3D_SOLVER_DT or SPECFEM3D_SOLVER_MAX_DT in ${PARAM_FILE}."
      exit 1
    fi
  else
    DT=$(awk -v s="${suggested_dt}" -v f="${dt_safety}" -v cap="${dt_cap}" 'BEGIN {
      gsub(/[dD]/, "e", s)
      dt = s * f
      if (cap != "") {
        gsub(/[dD]/, "e", cap)
        if (dt > cap + 0.0) dt = cap + 0.0
      }
      printf("%.10g\n", dt)
    }')
    DT_SOURCE="suggested DT ${suggested_dt} * safety factor ${dt_safety}, capped by ${dt_cap:-none}"
  fi
fi

requested_nstep=$(param SPECFEM3D_SOLVER_NSTEP "")
additional_buffer=$(param SPECFEM3D_SOLVER_ADDITIONAL_TIME_BUFFER 20.0)
metadata_loaded=0
if [[ -f "${INJECTED_METADATA}" ]]; then
  source "${INJECTED_METADATA}"
  metadata_loaded=1
fi

if [[ -n "${requested_nstep}" ]]; then
  NSTEP=$(awk -v v="${requested_nstep}" 'BEGIN { if (v !~ /^[0-9]+$/ || v < 1) exit 1; print v }') || {
    echo "Error: bad SPECFEM3D_SOLVER_NSTEP=${requested_nstep}"
    exit 1
  }
  NSTEP_SOURCE="user-specified SPECFEM3D_SOLVER_NSTEP"
else
  if [[ "${metadata_loaded}" -ne 1 ]]; then
    echo "Error: missing ${INJECTED_METADATA}; cannot auto-compute NSTEP."
    echo "Set SPECFEM3D_SOLVER_NSTEP or INJECTED_WAVES_METADATA in ${PARAM_FILE}."
    exit 1
  fi
  before=${TIME_BUFFER_CUT_BEFORE:-}
  if [[ -z "${before}" && -n "${MIN_CORNER_ARRIVAL_SEC:-}" && -n "${TIME_START_CUT:-}" ]]; then
    before=$(awk -v a="${MIN_CORNER_ARRIVAL_SEC}" -v s="${TIME_START_CUT}" 'BEGIN { print a - s }')
  fi
  if [[ -z "${before}" ]]; then
    before=${TIME_BUFFER_CUT:-0.0}
  fi
  if [[ -z "${MIN_CORNER_ARRIVAL_SEC:-}" || -z "${MAX_CORNER_ARRIVAL_SEC:-}" ]]; then
    if [[ -n "${TIME_START_CUT:-}" && -n "${TIME_END_CUT:-}" ]]; then
      duration=$(awk -v s="${TIME_START_CUT}" -v e="${TIME_END_CUT}" -v dt="${DT}" 'BEGIN { print e - s - dt }')
      awk -v duration="${duration}" 'BEGIN { if (duration <= 0.0) exit 1 }' || {
        echo "Error: explicit cut window must be longer than DT=${DT}."
        exit 1
      }
      NSTEP_SOURCE="step7 explicit cut window duration - DT"
    else
      echo "Error: step7 metadata lacks corner arrivals and cut-window times."
      echo "Set SPECFEM3D_SOLVER_NSTEP in ${PARAM_FILE}."
      exit 1
    fi
  else
    after=${TIME_BUFFER_CUT_AFTER:-${TIME_BUFFER_CUT:-0.0}}
    awk -v after="${after}" -v b="${additional_buffer}" -v dt="${DT}" 'BEGIN { if (after <= b + dt) exit 1 }' || {
      echo "Error: TIME_BUFFER_CUT_AFTER=${after} must be larger than"
      echo "       SPECFEM3D_SOLVER_ADDITIONAL_TIME_BUFFER + DT = ${additional_buffer} + ${DT}."
      echo "       Increase TIME_BUFFER_CUT_AFTER or reduce SPECFEM3D_SOLVER_ADDITIONAL_TIME_BUFFER."
      exit 1
    }
    duration=$(awk -v before="${before}" -v mn="${MIN_CORNER_ARRIVAL_SEC}" -v mx="${MAX_CORNER_ARRIVAL_SEC}" -v b="${additional_buffer}" 'BEGIN {
      print before + (mx - mn) + b
    }')
    NSTEP_SOURCE="buffer_before + corner-arrival range + additional buffer"
  fi
  NSTEP=$(awk -v duration="${duration}" -v dt="${DT}" 'BEGIN {
    n = int(duration / dt)
    if (n * dt < duration) n += 1
    if (n < 1) n = 1
    print n
  }')
fi

if [[ "${metadata_loaded}" -eq 1 && -n "${TIME_START_CUT:-}" && -n "${TIME_END_CUT:-}" ]]; then
  run_length=$(awk -v n="${NSTEP}" -v dt="${DT}" 'BEGIN { print n * dt }')
  cut_length=$(awk -v s="${TIME_START_CUT}" -v e="${TIME_END_CUT}" 'BEGIN { print e - s }')
  awk -v run="${run_length}" -v cut="${cut_length}" 'BEGIN { if (run >= cut) exit 1 }' || {
    echo "Error: SPECFEM3D run length NSTEP*DT must be smaller than the injected wavefield cut window."
    echo "       NSTEP=${NSTEP}, DT=${DT}, NSTEP*DT=${run_length}"
    echo "       TIME_END_CUT - TIME_START_CUT=${cut_length}"
    exit 1
  }
fi
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
' "${PAR_FILE}") || { echo "Error: could not read NPROC from ${PAR_FILE}"; exit 1; }
TIME_LIMIT=$(param SPECFEM3D_SOLVER_TIME_LIMIT "02:00:00")
MEM_PER_CPU=$(param SPECFEM3D_SOLVER_MEM_PER_CPU "4G")

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

replace_key "${PAR_FILE}" INJECTED_WAVEFIELD_PATH "${INJECTED_WAVEFIELD_PATH}"
replace_key "${PAR_FILE}" DT "${DT}"
replace_key "${PAR_FILE}" NSTEP "${NSTEP}"

echo "Injected wavefield path: ${INJECTED_WAVEFIELD_PATH}"
echo "DT=${DT} (${DT_SOURCE})"
echo "NSTEP=${NSTEP} (${NSTEP_SOURCE})"

SUBMIT_SOLVER=1

cd "${WORK_DIR}"
cat > run_specfem3D.sbatch <<EOF
#!/bin/bash
#SBATCH --time=${TIME_LIMIT}
#SBATCH --ntasks=${NPROC}
#SBATCH --mem-per-cpu=${MEM_PER_CPU}
#SBATCH -J specfem_$(basename "${ROOT_DIR}")
#SBATCH -o OUTPUT_FILES/slurm-specfem-%j.out
#SBATCH -e OUTPUT_FILES/slurm-specfem-%j.out
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
${MPIRUN_CMD} "${BIN_DIR}/xspecfem3D"
EOF
chmod +x run_specfem3D.sbatch

if [[ "${SUBMIT_SOLVER}" -eq 1 ]]; then
  if command -v sbatch >/dev/null 2>&1; then
    echo "Submitting SPECFEM3D solver..."
    JOB_ID=$(sbatch --parsable run_specfem3D.sbatch)
    echo "Submitted SPECFEM3D solver job: ${JOB_ID}"
  else
    echo "Error: sbatch not found. Only run_specfem3D.sbatch was generated."
    exit 1
  fi
else
  echo "Solver submission skipped. Review the warning above, rerun step3 if needed, then rerun step8."
fi

echo "----------------------------------------------------------------------"
cd "${ROOT_DIR}"
