#!/usr/bin/env bash
#
# Step 0: Stage the old 660 km pudding heterogeneity model for xgenerate_databases.

set -euo pipefail

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

if [[ "${DSM_MODEL_MODE}" == "DSM1D" ]]; then
  echo "DSM1D selected: xgenerate_databases was rebuilt from model_tomo/1D_DSM."
  echo "Skipping the 660 km pudding heterogeneity preparation; tomography_model.xyz is not used."
  exit 0
fi

OLD_HETERO_DIR="/proj/mazu/wenbowu/SEM_DSM/SPECFEM3D_old/EXAMPLES_COUPLING/PKPPKPbc_660_PudingAlldepth_20180824CI/DATA/meshfem3D_files/heterogeneities"
HETERO_DIR="${ROOT_DIR}/DATA/heterogeneities"

echo "----------------------------------------------------------------------"
echo "Starting Step 0: 660 km pudding heterogeneity preparation"
echo "Root Directory:         ${ROOT_DIR}"
echo "Old heterogeneity dir:  ${OLD_HETERO_DIR}"
echo "Case heterogeneity dir: ${HETERO_DIR}"
echo "----------------------------------------------------------------------"

mkdir -p "${HETERO_DIR}"

for file in Puding_heterogeneities.py exp_ACF_heterogeneities.py submit_puding.cmd Puding_model.png; do
  if [[ -f "${OLD_HETERO_DIR}/${file}" ]]; then
    cp "${OLD_HETERO_DIR}/${file}" "${HETERO_DIR}/${file}"
  fi
done

if [[ "${REGENERATE_PUDING_HETEROGENEITY:-0}" == "1" ]]; then
  if [[ -f "${HETERO_DIR}/tomography_model.xyz" && ! -L "${HETERO_DIR}/tomography_model.xyz" ]]; then
    echo "Refusing to overwrite existing real file: ${HETERO_DIR}/tomography_model.xyz" >&2
    echo "Move it aside first, or use the saved old-version model." >&2
    exit 1
  fi
  rm -f "${HETERO_DIR}/tomography_model.xyz"
  (
    cd "${HETERO_DIR}"
    python3 Puding_heterogeneities.py
  )
else
  if [[ ! -f "${OLD_HETERO_DIR}/tomography_model.xyz" ]]; then
    echo "Missing saved old-version tomography model: ${OLD_HETERO_DIR}/tomography_model.xyz" >&2
    exit 1
  fi
  if [[ ! -e "${HETERO_DIR}/tomography_model.xyz" ]]; then
    ln -s "${OLD_HETERO_DIR}/tomography_model.xyz" "${HETERO_DIR}/tomography_model.xyz"
  fi
fi

if [[ ! -f "${HETERO_DIR}/tomography_model.xyz" ]]; then
  echo "Missing ${HETERO_DIR}/tomography_model.xyz" >&2
  exit 1
fi

echo "----------------------------------------------------------------------"
echo "Preparation Summary:"
echo "  TOMOGRAPHY_PATH=${HETERO_DIR}"
echo "  tomography_model.xyz=$(readlink -f "${HETERO_DIR}/tomography_model.xyz")"
echo "Step 0: Heterogeneity preparation complete. Move to Step 1."
echo "----------------------------------------------------------------------"
