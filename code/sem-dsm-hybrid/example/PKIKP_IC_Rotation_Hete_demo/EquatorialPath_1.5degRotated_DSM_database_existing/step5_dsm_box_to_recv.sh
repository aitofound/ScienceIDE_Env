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

reference_dsm_model_file() {
  local dsm_database_dir
  dsm_database_dir=$(param DSM_DATABASE_DIR "../EquatorialPath/WORK/DSM")
  dsm_database_dir=$(resolve_case_path "${dsm_database_dir}")
  if [[ -f "${dsm_database_dir}/../SPECFEM3D/DATA/dsm_model_input" ]]; then
    printf '%s\n' "${dsm_database_dir}/../SPECFEM3D/DATA/dsm_model_input"
  elif [[ -f "${dsm_database_dir}/src_to_box/DATA/dsm_model_input" ]]; then
    printf '%s\n' "${dsm_database_dir}/src_to_box/DATA/dsm_model_input"
  elif [[ -f "${dsm_database_dir}/src_to_box/explosion/DATA/dsm_model_input" ]]; then
    printf '%s\n' "${dsm_database_dir}/src_to_box/explosion/DATA/dsm_model_input"
  elif [[ -f "${dsm_database_dir}/../../DATA/dsm_model_base" ]]; then
    printf '%s\n' "${dsm_database_dir}/../../DATA/dsm_model_base"
  else
    return 1
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

check_dsm_model_compatibility() {
  local current_model reference_model
  current_model=$(current_dsm_model_file)
  reference_model=$(reference_dsm_model_file) || {
    echo "Error: could not find reference DSM model for reused DSM database." >&2
    echo "Set DSM_DATABASE_DIR correctly in ${PARAM_FILE}." >&2
    exit 1
  }
  if [[ ! -f "${current_model}" ]]; then
    echo "Error: current DSM model file not found: ${current_model}" >&2
    exit 1
  fi
  if ! diff -q <(normalize_dsm_structure "${current_model}") <(normalize_dsm_structure "${reference_model}") >/dev/null; then
    echo "Error: DSM model mismatch; cannot safely reuse the DSM database." >&2
    echo "  current:   ${current_model}" >&2
    echo "  reference: ${reference_model}" >&2
    echo "The comparison uses the generated dsm_model_input when present, so attenuation-related model changes are included." >&2
    exit 1
  fi
  echo "DSM model compatibility check passed."
  echo "  current:   ${current_model}"
  echo "  reference: ${reference_model}"
}

DSM_BOX_TO_RECV_DIR=$(resolve_case_path "$(param DSM_BOX_TO_RECV_DIR "../EquatorialPath/WORK/DSM/box_to_recv")")

echo "----------------------------------------------------------------------"
echo "Step 5: Reusing DSM box-to-receiver database"
echo "DSM box-to-receiver directory: ${DSM_BOX_TO_RECV_DIR}"
echo "----------------------------------------------------------------------"

check_dsm_model_compatibility

if [[ ! -d "${DSM_BOX_TO_RECV_DIR}" ]]; then
  echo "Error: reused DSM box-to-receiver directory not found: ${DSM_BOX_TO_RECV_DIR}" >&2
  exit 1
fi
for comp in Fz; do
  if [[ ! -d "${DSM_BOX_TO_RECV_DIR}/${comp}" ]]; then
    echo "Error: expected ${comp} receiver database is missing: ${DSM_BOX_TO_RECV_DIR}/${comp}" >&2
    exit 1
  fi
done

echo "Step 5 complete: no DSM job submitted for this rotated case."
