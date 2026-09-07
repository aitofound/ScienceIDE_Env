#!/usr/bin/env bash
#
# Case-local model-source selection for this SEM-DSM demo.
# This file is sourced by Step 0, Step 2, and Step 3 scripts.

select_generate_databases_model() {
  if [[ $# -ne 3 ]]; then
    echo "Usage: select_generate_databases_model CASE_ROOT SPECFEM_ROOT DSM1D_OR_3D" >&2
    return 2
  fi

  local case_root=$1
  local specfem_root=$2
  local model_mode
  local source_file

  model_mode=$(printf '%s' "$3" | tr '[:lower:]' '[:upper:]')
  case "${model_mode}" in
    DSM1D)
      MODEL_SOURCE_DIR="${specfem_root}/src/generate_databases/model_tomo/1D_DSM"
      ;;
    DSM3D)
      MODEL_SOURCE_DIR="${case_root}/DATA"
      ;;
    *)
      echo "Error: DSM1D_OR_3D must be DSM1D or DSM3D, got '${model_mode}'." >&2
      return 1
      ;;
  esac

  CASE_MODEL="${MODEL_SOURCE_DIR}/model_tomography.f90"
  CASE_GET_MODEL="${MODEL_SOURCE_DIR}/get_model.F90"
  ACTIVE_MODEL="${specfem_root}/src/generate_databases/model_tomography.f90"
  ACTIVE_GET_MODEL="${specfem_root}/src/generate_databases/get_model.F90"
  GENERATOR="${specfem_root}/bin/xgenerate_databases"

  for source_file in "${CASE_MODEL}" "${CASE_GET_MODEL}"; do
    if [[ ! -f "${source_file}" ]]; then
      echo "Error: required model source is missing: ${source_file}" >&2
      return 1
    fi
  done
}
