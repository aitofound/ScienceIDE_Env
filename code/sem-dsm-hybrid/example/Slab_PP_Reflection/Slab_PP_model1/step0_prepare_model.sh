#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
SPECFEM_ROOT="${REPO_ROOT}/src/SPECFEM3D"
CASE_SPECFEM="${SCRIPT_DIR}/WORK/SPECFEM3D"
CASE_MODEL="${SCRIPT_DIR}/DATA/model_tomography.f90"
ACTIVE_MODEL="${SPECFEM_ROOT}/src/generate_databases/model_tomography.f90"
CASE_GET_MODEL="${SCRIPT_DIR}/DATA/get_model.F90"
ACTIVE_GET_MODEL="${SPECFEM_ROOT}/src/generate_databases/get_model.F90"
GENERATOR="${SPECFEM_ROOT}/bin/xgenerate_databases"


usage() {
  echo "Usage: $0 [--check|--install|--install-only]"
}

mode="${1:---check}"
case "${mode}" in
  --check)
    if ! cmp -s "${CASE_MODEL}" "${ACTIVE_MODEL}" || \
       ! cmp -s "${CASE_GET_MODEL}" "${ACTIVE_GET_MODEL}"; then
      echo "The active SPECFEM3D model sources are not the compatible Slab_PP_Reflection8_H_dvp0.05 pair." >&2
      echo "Run '$0 --install' to install both files and rebuild xgenerate_databases." >&2
      exit 1
    fi
    if [[ ! -x "${GENERATOR}" || "${ACTIVE_MODEL}" -nt "${GENERATOR}" || \
          "${ACTIVE_GET_MODEL}" -nt "${GENERATOR}" ]]; then
      echo "The model pair is installed, but xgenerate_databases must be rebuilt." >&2
      exit 1
    fi
    echo "Model source pair and xgenerate_databases are ready for this slab."
    ;;
  --install|--install-only)
    cp "${CASE_MODEL}" "${ACTIVE_MODEL}"
    cp "${CASE_GET_MODEL}" "${ACTIVE_GET_MODEL}"
    echo "Installed the case model and matching get_model.F90 adapter."
    if [[ "${mode}" == "--install" ]]; then
      make -C "${SPECFEM_ROOT}" xgenerate_databases
      if [[ ! -x "${GENERATOR}" || "${ACTIVE_MODEL}" -nt "${GENERATOR}" || \
            "${ACTIVE_GET_MODEL}" -nt "${GENERATOR}" ]]; then
        echo "Error: xgenerate_databases was not rebuilt successfully." >&2
        exit 1
      fi
      echo "Rebuilt ${GENERATOR}."
    else
      echo "Install-only mode: rebuild with 'make -C ${SPECFEM_ROOT} xgenerate_databases'."
    fi
    ;;
  *)
    usage >&2
    exit 2
    ;;
esac
