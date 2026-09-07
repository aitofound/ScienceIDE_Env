#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PARAM_FILE="${ROOT_DIR}/DATA/Par_file_SEM_DSM"

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
    printf '%s\n' "$(cd "$(dirname "${path}")" && pwd)/$(basename "${path}")"
  else
    printf '%s\n' "$(cd "${ROOT_DIR}/$(dirname "${path}")" && pwd)/$(basename "${path}")"
  fi
}

current_dsm_model_file() {
  if [[ -f "${ROOT_DIR}/WORK/SPECFEM3D/DATA/dsm_model_input" ]]; then
    printf '%s\n' "${ROOT_DIR}/WORK/SPECFEM3D/DATA/dsm_model_input"
  else
    printf '%s\n' "${ROOT_DIR}/DATA/dsm_model_base"
  fi
}

normalize_dsm_structure() {
  local file=$1
  awk '
    function clean(line) {
      sub(/#.*/ , "", line)
      gsub(/[dD]/ , "e", line)
      gsub(/^[[:space:]]+|[[:space:]]+$/ , "", line)
      gsub(/[[:space:]]+/ , " ", line)
      return line
    }
    NR == 4 { nz = int($1); last = 4 + 6 * nz }
    NR >= 4 && NR <= last {
      line = clean($0)
      if (line != "") print line
    }
  ' "${file}"
}

check_dsm_model_matches_current() {
  local label=$1
  local reference_model=$2
  local current_model
  current_model=$(current_dsm_model_file)
  if [[ ! -f "${current_model}" ]]; then
    echo "Error: current DSM model file not found: ${current_model}" >&2
    exit 1
  fi
  if [[ ! -f "${reference_model}" ]]; then
    echo "Error: ${label} DSM model file not found: ${reference_model}" >&2
    exit 1
  fi
  if ! diff -q <(normalize_dsm_structure "${current_model}") <(normalize_dsm_structure "${reference_model}") >/dev/null; then
    echo "Error: ${label} DSM model mismatch; cannot safely reuse 1-D DSM synthetics." >&2
    echo "  current:   ${current_model}" >&2
    echo "  reference: ${reference_model}" >&2
    exit 1
  fi
  echo "DSM model compatibility check passed for ${label}."
  echo "  current:   ${current_model}"
  echo "  reference: ${reference_model}"
}

DSM_DATABASE_DIR=$(resolve_case_path "$(param DSM_DATABASE_DIR "../EquatorialPath/WORK/DSM")")
DSM_1D_TELE_SYN_DIR=$(resolve_case_path "$(param DSM_1D_TELE_SYN_DIR "${DSM_DATABASE_DIR}/1D_tele_syn")")
SOURCE_MODE=$(param SINGLE_FORCE_ENZ -1)

case "${SOURCE_MODE}" in
  -1)
    REQUIRED_DIR="${DSM_1D_TELE_SYN_DIR}/explosion/OUTPUT_FILES/disp_solid_time_sac"
    REFERENCE_MODEL="${DSM_1D_TELE_SYN_DIR}/explosion/DATA/dsm_model"
    LABEL="explosion 1-D DSM"
    ;;
  0)
    REQUIRED_DIR="${DSM_1D_TELE_SYN_DIR}/OUTPUT_FILES/CMTSOLUTION_time_sac"
    REFERENCE_MODEL=""
    LABEL="moment-tensor 1-D DSM"
    ;;
  1)
    REQUIRED_DIR="${DSM_1D_TELE_SYN_DIR}/Fr/OUTPUT_FILES/disp_solid_time_sac"
    REFERENCE_MODEL=""
    LABEL="Fr 1-D DSM"
    ;;
  2)
    REQUIRED_DIR="${DSM_1D_TELE_SYN_DIR}/Ft/OUTPUT_FILES/disp_solid_time_sac"
    REFERENCE_MODEL=""
    LABEL="Ft 1-D DSM"
    ;;
  3)
    REQUIRED_DIR="${DSM_1D_TELE_SYN_DIR}/Fz/OUTPUT_FILES/disp_solid_time_sac"
    REFERENCE_MODEL=""
    LABEL="Fz 1-D DSM"
    ;;
  *)
    echo "Error: Unsupported SINGLE_FORCE_ENZ=${SOURCE_MODE}." >&2
    echo "Supported values: -1 explosion, 0 moment-tensor Green functions, 1 North, 2 East, 3 Vertical." >&2
    exit 1
    ;;
esac

echo "----------------------------------------------------------------------"
echo "Step 6: Reusing 1-D DSM teleseismic synthetics"
echo "DSM database directory: ${DSM_DATABASE_DIR}"
echo "1-D DSM synthetic directory: ${DSM_1D_TELE_SYN_DIR}"
echo "Required output directory: ${REQUIRED_DIR}"
echo "----------------------------------------------------------------------"

if [[ ! -d "${DSM_DATABASE_DIR}" ]]; then
  echo "Error: reused DSM database directory not found: ${DSM_DATABASE_DIR}" >&2
  exit 1
fi
if [[ ! -d "${REQUIRED_DIR}" ]]; then
  echo "Error: required reused 1-D DSM synthetics are missing: ${REQUIRED_DIR}" >&2
  exit 1
fi

if [[ -n "${REFERENCE_MODEL}" ]]; then
  check_dsm_model_matches_current "${LABEL}" "${REFERENCE_MODEL}"
else
  echo "Warning: no single reference DSM model check is implemented for ${LABEL}; verified output directory only." >&2
fi

echo "Step 6 complete: no DSM 1-D job submitted for this rotated case."
