#!/usr/bin/env bash
#
# Step 0: reuse EquatorialPath ICB topography and heterogeneity inputs for DSM3D.

set -euo pipefail
shopt -s nullglob

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"

# --- Case-specific SPECFEM3D model installation ---
REPO_ROOT="$(cd "${ROOT_DIR}/../../.." && pwd)"
SPECFEM_ROOT="${REPO_ROOT}/src/SPECFEM3D"
MODEL_SETUP_HELPER="${ROOT_DIR}/auxiliary/select_generate_databases_model.sh"

if [[ ! -f "${MODEL_SETUP_HELPER}" ]]; then
  echo "Error: model-selection helper is missing: ${MODEL_SETUP_HELPER}" >&2
  exit 1
fi

DSM_MODEL_MODE=$(awk -F= '
  $0 !~ /^[[:space:]]*#/ && $1 ~ /^[[:space:]]*DSM1D_OR_3D[[:space:]]*$/ {
    value=$2
    sub(/#.*/, "", value)
    gsub(/^[[:space:]]+|[[:space:]]+$/, "", value)
    print toupper(value)
    found=1
    exit
  }
  END { if (!found) print "DSM3D" }
' "${ROOT_DIR}/DATA/Par_file_SEM_DSM")

# shellcheck source=auxiliary/select_generate_databases_model.sh
source "${MODEL_SETUP_HELPER}"
select_generate_databases_model "${ROOT_DIR}" "${SPECFEM_ROOT}" "${DSM_MODEL_MODE}"

model_usage() {
  echo "Usage: $0 [--check|--install|--install-only]"
}

require_case_model_sources() {
  local source_file
  for source_file in "${CASE_MODEL}" "${CASE_GET_MODEL}"; do
    if [[ ! -f "${source_file}" ]]; then
      echo "Error: required case model source is missing: ${source_file}" >&2
      return 1
    fi
  done
}

install_case_model_sources() {
  cp "${CASE_MODEL}" "${ACTIVE_MODEL}"
  cp "${CASE_GET_MODEL}" "${ACTIVE_GET_MODEL}"
  echo "Installed the case model_tomography.f90 and get_model.F90 sources."
}

build_case_generator() {
  make -C "${SPECFEM_ROOT}" CLEAN=generate_databases clean
  make -C "${SPECFEM_ROOT}" xgenerate_databases
}

check_case_model() {
  require_case_model_sources
  if ! cmp -s "${CASE_MODEL}" "${ACTIVE_MODEL}" || \
     ! cmp -s "${CASE_GET_MODEL}" "${ACTIVE_GET_MODEL}"; then
    echo "Error: active SPECFEM3D model sources do not match this case." >&2
    echo "Run '$0 --install' to install them and rebuild xgenerate_databases." >&2
    return 1
  fi
  if [[ ! -x "${GENERATOR}" || "${ACTIVE_MODEL}" -nt "${GENERATOR}" || \
        "${ACTIVE_GET_MODEL}" -nt "${GENERATOR}" ]]; then
    echo "Error: xgenerate_databases is missing or older than the active model sources." >&2
    echo "Run '$0 --install' to rebuild it." >&2
    return 1
  fi
}

ensure_case_model() {
  require_case_model_sources
  install_case_model_sources
  build_case_generator
  check_case_model
  echo "Model source pair and xgenerate_databases are ready for this case."
}

if [[ $# -gt 1 ]]; then
  model_usage >&2
  exit 2
fi

MODEL_SETUP_MODE="${1:-}"
case "${MODEL_SETUP_MODE}" in
  "")
    ensure_case_model
    ;;
  --check)
    check_case_model
    echo "Model source pair and xgenerate_databases are ready for this case."
    exit 0
    ;;
  --install)
    require_case_model_sources
    install_case_model_sources
    build_case_generator
    check_case_model
    echo "Model source pair and xgenerate_databases are ready for this case."
    ;;
  --install-only)
    require_case_model_sources
    install_case_model_sources
    echo "Install-only mode: clean and rebuild with:"
    echo "  make -C ${SPECFEM_ROOT} CLEAN=generate_databases clean"
    echo "  make -C ${SPECFEM_ROOT} xgenerate_databases"
    exit 0
    ;;
  *)
    model_usage >&2
    exit 2
    ;;
esac
# --- End case-specific SPECFEM3D model installation ---

SEM_DSM_PARAM_FILE="${ROOT_DIR}/DATA/Par_file_SEM_DSM"
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
  ' "${SEM_DSM_PARAM_FILE}" 2>/dev/null || true)
  if [[ -n "${value}" ]]; then
    printf '%s\n' "${value}"
  else
    printf '%s\n' "${default}"
  fi
}

resolve_case_path() {
  local path=$1
  if [[ "${path}" = /* ]]; then
    printf "%s\n" "${path}"
  else
    printf "%s\n" "${ROOT_DIR}/${path}"
  fi
}

echo "----------------------------------------------------------------------"
echo "Step 0: Reuse EquatorialPath DSM3D Inputs"
echo "----------------------------------------------------------------------"

DSM1D_OR_3D=$(param DSM1D_OR_3D DSM3D)
DSM1D_OR_3D=$(printf '%s' "${DSM1D_OR_3D}" | tr '[:lower:]' '[:upper:]')
case "${DSM1D_OR_3D}" in
  DSM1D|DSM3D) ;;
  *)
    echo "Error: DSM1D_OR_3D must be DSM1D or DSM3D, got '${DSM1D_OR_3D}'." >&2
    exit 1
    ;;
esac
echo "DSM1D_OR_3D=${DSM1D_OR_3D}"

if [[ "${DSM1D_OR_3D}" == "DSM1D" ]]; then
  echo "DSM1D selected: shared ICB topography and heterogeneity tiles are not used."
  echo "----------------------------------------------------------------------"
  exit 0
fi

TOPO_FILE=$(resolve_case_path "$(param ICB_TOPOGRAPHY_FILE "../EquatorialPath/DATA/topo_files/latlon_ICB_topo.txt")")
TOMOGRAPHY_PATH=$(resolve_case_path "$(param TOMOGRAPHY_PATH "../EquatorialPath/DATA/heterogeneities/tiles")")

if [[ ! -s "${TOPO_FILE}" ]]; then
  echo "Error: shared EquatorialPath ICB topography file is missing or empty:" >&2
  echo "  ${TOPO_FILE}" >&2
  exit 1
fi

if [[ ! -d "${TOMOGRAPHY_PATH}" ]]; then
  echo "Error: shared EquatorialPath heterogeneity tile directory is missing:" >&2
  echo "  ${TOMOGRAPHY_PATH}" >&2
  exit 1
fi

tiles=("${TOMOGRAPHY_PATH}"/tomography_model_box*.xyz)
if [[ ${#tiles[@]} -eq 0 ]]; then
  echo "Error: no shared tomography_model_box*.xyz files were found in:" >&2
  echo "  ${TOMOGRAPHY_PATH}" >&2
  exit 1
fi
for tile in "${tiles[@]}"; do
  if [[ ! -s "${tile}" ]]; then
    echo "Error: shared heterogeneity tile is empty: ${tile}" >&2
    exit 1
  fi
done

echo "Reusing ICB topography file:"
echo "  ${TOPO_FILE}"
echo "Reusing ${#tiles[@]} heterogeneity XYZ tiles from:"
echo "  ${TOMOGRAPHY_PATH}"
echo "----------------------------------------------------------------------"
