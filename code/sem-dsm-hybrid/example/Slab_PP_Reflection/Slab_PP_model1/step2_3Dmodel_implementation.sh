#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODEL_FILE="${SCRIPT_DIR}/WORK/SPECFEM3D/DATA/dsm_model_input"
[[ -f "${SCRIPT_DIR}/DATA/model_tomography.f90" ]] || { echo "Missing DATA/model_tomography.f90" >&2; exit 1; }
[[ -f "${SCRIPT_DIR}/DATA/get_model.F90" ]] || { echo "Missing DATA/get_model.F90" >&2; exit 1; }
[[ -f "${MODEL_FILE}" ]] || { echo "Missing ${MODEL_FILE}" >&2; exit 1; }
echo "$(basename "${SCRIPT_DIR}") 3-D model input is ready: ${MODEL_FILE}"
