#!/usr/bin/env bash
#
# Step 0: generate inner-core heterogeneity inputs and a flat ICB interface grid.

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
  echo "Installed DATA/model_tomography.f90 and DATA/get_model.F90."
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

if [[ -z "${PYTHON_BIN:-}" ]]; then
  if [[ -x "/user/wenbo.wu/.conda/envs/sqhao_seismo/bin/python" ]]; then
    PYTHON_BIN="/user/wenbo.wu/.conda/envs/sqhao_seismo/bin/python"
  else
    PYTHON_BIN="python3"
  fi
fi

TOPO_DIR="${ROOT_DIR}/DATA/topo_files"
TOPO_GENERATOR="${TOPO_DIR}/generate_bump.py"
TOPO_FILE="${TOPO_DIR}/latlon_ICB_topo.txt"

HETERO_DIR="${ROOT_DIR}/DATA/heterogeneities"
HETERO_GENERATOR="${HETERO_DIR}/generate_stochastic_heterogeneity_tiled_overlap.py"
TILE_DIR="${HETERO_DIR}/tiles"

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
    printf '%s
' "${value}"
  else
    printf '%s
' "${default}"
  fi
}

write_zero_icb_topography() {
  cat > "${TOPO_FILE}" <<EOF_ZERO_TOPO
2 2
0.0 0.0 1.0 1.0
1.0 1.0
0.00 0.00
0.00 0.00
EOF_ZERO_TOPO
}

warn_short_dsm_length() {
  local key=$1
  local value
  value=$(param "${key}" "")
  if [[ -z "${value}" ]]; then
    return
  fi

  if awk -v value="${value}" "BEGIN {
      if (value !~ /^[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eEdD][+-]?[0-9]+)?$/) exit 1
      exit !(value + 0 < 4000.0)
    }"; then
    echo "WARNING: ${key}=${value} s is shorter than 4000 s."
    echo "         Low-frequency PKIIKP DSM errors could be large due to wrap-around artifacts."
  fi
}

echo "----------------------------------------------------------------------"
echo "Step 0: Generate Inner-Core Heterogeneity Inputs"
echo "----------------------------------------------------------------------"
echo "Using Python: ${PYTHON_BIN}"
echo ""

warn_short_dsm_length TIME_LENGTH_DSM_SOURCE_TO_BOX
warn_short_dsm_length TIME_LENGTH_DSM_BOX_TO_RECEIVER
warn_short_dsm_length TIME_LENGTH_DSM_1D
echo ""

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
  echo "DSM1D selected: writing zero ICB topography and skipping 3-D heterogeneity generation."
  mkdir -p "${TOPO_DIR}" "${TILE_DIR}"
  write_zero_icb_topography
  rm -f "${TILE_DIR}"/tomography_model_box*.xyz "${TILE_DIR}"/tomography_model_box*.vtk

  echo ""
  echo "Generated zero topography file:"
  echo "  ${TOPO_FILE}"
  echo "Skipped heterogeneity tiles:"
  echo "  ${TILE_DIR}"
  echo "----------------------------------------------------------------------"
  exit 0
fi

for required_file in "${TOPO_GENERATOR}" "${HETERO_GENERATOR}"; do
  if [[ ! -f "${required_file}" ]]; then
    echo "Error: required generator is missing: ${required_file}" >&2
    exit 1
  fi
done

echo "Generating ICB topography..."
(cd "${TOPO_DIR}" && "${PYTHON_BIN}" "$(basename "${TOPO_GENERATOR}")")

if [[ ! -s "${TOPO_FILE}" ]]; then
  echo "Error: topography file was not generated: ${TOPO_FILE}" >&2
  exit 1
fi

echo ""
echo "Generating inner-core heterogeneity tiles..."
mkdir -p "${TILE_DIR}"
rm -f "${TILE_DIR}"/tomography_model_box*.xyz "${TILE_DIR}"/tomography_model_box*.vtk
(cd "${HETERO_DIR}" && "${PYTHON_BIN}" "$(basename "${HETERO_GENERATOR}")")

tiles=("${TILE_DIR}"/tomography_model_box*.xyz)
if [[ ${#tiles[@]} -eq 0 ]]; then
  echo "Error: no heterogeneity XYZ tiles were generated in ${TILE_DIR}" >&2
  exit 1
fi

echo ""
echo "Generated topography file:"
echo "  ${TOPO_FILE}"
echo "Generated heterogeneity tiles:"
echo "  ${TILE_DIR}"
echo "----------------------------------------------------------------------"
