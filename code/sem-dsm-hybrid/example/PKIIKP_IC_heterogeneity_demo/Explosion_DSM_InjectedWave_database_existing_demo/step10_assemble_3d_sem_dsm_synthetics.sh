#!/usr/bin/env bash
#
# Step 10: assemble final 3-D SEM-DSM synthetics from this case's SEM
# coupling output and the existing DSM database configured in Par_file_SEM_DSM.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PARAM_FILE="${ROOT_DIR}/DATA/Par_file_SEM_DSM"
PYTHON_BIN="${PYTHON_BIN:-python3}"

param() {
  local key=$1 fallback=${2:-}
  awk -F= -v key="$key" -v fallback="$fallback" '
    $0 !~ /^[[:space:]]*#/ && $1 ~ "^[[:space:]]*" key "[[:space:]]*$" {
      v=$2
      sub(/#.*/, "", v)
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", v)
      print v
      found=1
      exit
    }
    END { if (!found) print fallback }
  ' "${PARAM_FILE}"
}

resolve_case_path() {
  local path=$1
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
    echo "Reused 1-D teleseismic DSM synthetics must use the same 1-D DSM model." >&2
    exit 1
  fi
  echo "DSM model check passed for ${label}: ${reference_model}"
}

DSM_DATABASE_DIR_RAW=$(param DSM_DATABASE_DIR "")
INJECTED_METADATA_RAW=$(param INJECTED_WAVES_METADATA "")

if [[ -z "${DSM_DATABASE_DIR_RAW}" ]]; then
  echo "Error: set DSM_DATABASE_DIR in ${PARAM_FILE}."
  exit 1
fi
if [[ -z "${INJECTED_METADATA_RAW}" ]]; then
  echo "Error: set INJECTED_WAVES_METADATA in ${PARAM_FILE}."
  exit 1
fi

DSM_DATABASE_DIR=$(resolve_case_path "${DSM_DATABASE_DIR_RAW}")
INJECTED_METADATA=$(resolve_case_path "${INJECTED_METADATA_RAW}")
SOURCE_MODE=$(param SINGLE_FORCE_ENZ -1)
case "${SOURCE_MODE}" in
  -1)
    ONE_D_DIR="${DSM_DATABASE_DIR}/1D_tele_syn/explosion/OUTPUT_FILES/disp_solid_time_sac"
    ONE_D_DSM_MODEL=$(first_existing_file \
      "${DSM_DATABASE_DIR}/1D_tele_syn/explosion/DATA/dsm_model_input" \
      "${DSM_DATABASE_DIR}/1D_tele_syn/explosion/DATA/dsm_model") || {
      echo "Error: could not find reused 1-D DSM model under ${DSM_DATABASE_DIR}/1D_tele_syn/explosion/DATA" >&2
      exit 1
    }
    ONE_D_SCALE="none"
    ;;
  0)
    ONE_D_DIR="${DSM_DATABASE_DIR}/1D_tele_syn/OUTPUT_FILES/CMTSOLUTION_time_sac"
    ONE_D_DSM_MODEL=""
    ONE_D_SCALE="none"
    ;;
  1)
    ONE_D_DIR="${DSM_DATABASE_DIR}/1D_tele_syn/Fr/OUTPUT_FILES/disp_solid_time_sac"
    ONE_D_DSM_MODEL=""
    ONE_D_SCALE="none"
    ;;
  2)
    ONE_D_DIR="${DSM_DATABASE_DIR}/1D_tele_syn/Ft/OUTPUT_FILES/disp_solid_time_sac"
    ONE_D_DSM_MODEL=""
    ONE_D_SCALE="none"
    ;;
  3)
    ONE_D_DIR="${DSM_DATABASE_DIR}/1D_tele_syn/Fz/OUTPUT_FILES/disp_solid_time_sac"
    ONE_D_DSM_MODEL=""
    ONE_D_SCALE="none"
    ;;
  *)
    echo "Error: Unsupported SINGLE_FORCE_ENZ=${SOURCE_MODE}." >&2
    echo "Supported values: -1 explosion, 0 moment-tensor Green functions, 1 North, 2 East, 3 Vertical." >&2
    exit 1
    ;;
esac

echo "----------------------------------------------------------------------"
echo "Starting Step 10: Assemble 3-D SEM-DSM synthetics"
echo "Root Directory: ${ROOT_DIR}"
echo "SINGLE_FORCE_ENZ=${SOURCE_MODE}"
echo "1-D DSM SAC directory: ${ONE_D_DIR}"
echo "InjectedWaves metadata: ${INJECTED_METADATA}"
echo "----------------------------------------------------------------------"

if [[ -n "${ONE_D_DSM_MODEL}" ]]; then
  check_dsm_model_file_matches_current "1-D teleseismic" "${ONE_D_DSM_MODEL}"
fi

cd "${ROOT_DIR}"
HELPER="auxiliary/assemble_3d_sem_dsm_synthetics.py"
args=(
  "${HELPER}"
  --one-d-dir "${ONE_D_DIR}"
  --coupling-dir "${ROOT_DIR}/WORK/Coupling/OUTPUT_FILES"
  --metadata "${INJECTED_METADATA}"
  --stations "${ROOT_DIR}/DATA/tele_station.txt"
  --cmt "${ROOT_DIR}/DATA/CMTSOLUTION"
  --output-root "${ROOT_DIR}/OUTPUT_FILES"
)
if grep -q -- '--one-d-scale' "${HELPER}"; then
  args+=(--one-d-scale "${ONE_D_SCALE}")
fi
if [[ -n "${ONE_D_DSM_MODEL}" ]] && grep -q -- '--one-d-dsm-model' "${HELPER}"; then
  args+=(--one-d-dsm-model "${ONE_D_DSM_MODEL}")
fi
"${PYTHON_BIN}" "${args[@]}"

echo "----------------------------------------------------------------------"
echo "Step 10 complete."
echo "----------------------------------------------------------------------"
