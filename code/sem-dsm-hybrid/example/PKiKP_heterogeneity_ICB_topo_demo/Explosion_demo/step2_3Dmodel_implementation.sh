#!/usr/bin/env bash
#
# Step 2: Explain which SPECFEM3D model implementation will be used.
# This script runs from its demo root.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PARAM_FILE="${ROOT_DIR}/DATA/Par_file_SEM_DSM"
REPO_ROOT="$(cd "${ROOT_DIR}/../../.." && pwd)"
SPECFEM_ROOT="${REPO_ROOT}/src/SPECFEM3D"
MODEL_SETUP_HELPER="${ROOT_DIR}/auxiliary/select_generate_databases_model.sh"

if [[ ! -f "${MODEL_SETUP_HELPER}" ]]; then
  echo "Error: model-selection helper is missing: ${MODEL_SETUP_HELPER}" >&2
  exit 1
fi

# shellcheck source=auxiliary/select_generate_databases_model.sh
source "${MODEL_SETUP_HELPER}"

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

DSM1D_OR_3D=$(param DSM1D_OR_3D DSM3D)
DSM1D_OR_3D=$(printf '%s' "${DSM1D_OR_3D}" | tr '[:lower:]' '[:upper:]')
select_generate_databases_model "${ROOT_DIR}" "${SPECFEM_ROOT}" "${DSM1D_OR_3D}"

cat <<EOF_HEADER
----------------------------------------------------------------------
Step 2: Model implementation
----------------------------------------------------------------------

Current model mode from DATA/Par_file_SEM_DSM:
  DSM1D_OR_3D = ${DSM1D_OR_3D}

EOF_HEADER

if [[ "${DSM1D_OR_3D}" == "DSM1D" ]]; then
cat <<EOF_DSM1D
DSM1D mode uses the 1-D DSM background model in SPECFEM3D.
ICB topography and heterogeneity tiles are not applied:
  - Step 1 writes MODEL=DSM1D
  - Step 1 generates a flat ICB interface
  - xgenerate_databases calls model_DSM1D, not model_DSM3D

This mode is useful for checking SEM-DSM consistency; the scattered
wavefield should be near zero for the 1-D background model.
----------------------------------------------------------------------
EOF_DSM1D
elif [[ "${DSM1D_OR_3D}" == "DSM3D" ]]; then
cat <<EOF_DSM3D
This case keeps its SPECFEM3D model source pair in:
  ${CASE_MODEL}
  ${CASE_GET_MODEL}

Step 0 installs both files into SPECFEM3D and rebuilds the canonical
xgenerate_databases executable. After editing either source, rerun:
  ./step0_prepare_icb_topography.sh --install

Step 2 does not modify the active SPECFEM3D source tree.
----------------------------------------------------------------------
EOF_DSM3D
else
cat <<EOF_ERROR
Error: DSM1D_OR_3D must be DSM1D or DSM3D, got '${DSM1D_OR_3D}'.
----------------------------------------------------------------------
EOF_ERROR
exit 1
fi
