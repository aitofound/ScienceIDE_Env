#!/usr/bin/env bash
#
# Check that the current SPECFEM3D SEM box matches the one used to build an
# existing InjectedWaves database.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PARAM_FILE="${ROOT_DIR}/DATA/Par_file_SEM_DSM"
CURRENT_DATA_DIR="${ROOT_DIR}/WORK/SPECFEM3D/DATA"
REFERENCE_DATA_DIR=""
REFERENCE_MANIFEST=""

usage() {
  cat <<'USAGE'
Usage: check_sem_box_compatibility.sh [options]

Options:
  --current-data-dir DIR    Current SPECFEM3D DATA directory.
  --reference-data-dir DIR  Reference SPECFEM3D DATA directory.
  --reference-manifest FILE Reference SEM box checksum manifest.
  --param-file FILE         SEM-DSM parameter file.
  -h, --help                Show this help.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --current-data-dir)
      CURRENT_DATA_DIR=$(cd "$2" && pwd)
      shift 2
      ;;
    --reference-data-dir)
      REFERENCE_DATA_DIR=$(cd "$2" && pwd)
      shift 2
      ;;
    --reference-manifest)
      REFERENCE_MANIFEST=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
      shift 2
      ;;
    --param-file)
      PARAM_FILE=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

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
    printf '%s\n' "${path}"
  else
    printf '%s\n' "${ROOT_DIR}/${path}"
  fi
}

infer_reference_data_dir() {
  local injected_path injected_dir work_dir
  injected_path=$(param INJECTED_WAVEFIELD_PATH "")
  [[ -n "${injected_path}" ]] || return 1
  injected_path=$(resolve_case_path "${injected_path}")
  injected_dir=$(dirname "${injected_path}")
  work_dir=$(dirname "${injected_dir}")
  printf '%s\n' "${work_dir}/SPECFEM3D/DATA"
}

infer_reference_manifest() {
  local injected_path injected_dir
  injected_path=$(param INJECTED_WAVEFIELD_PATH "")
  [[ -n "${injected_path}" ]] || return 1
  injected_path=$(resolve_case_path "${injected_path}")
  injected_dir=$(dirname "${injected_path}")
  printf '%s\n' "${injected_dir}/DATA/sem_box_manifest.sha256"
}

if [[ ! -f "${PARAM_FILE}" ]]; then
  echo "Error: missing parameter file: ${PARAM_FILE}" >&2
  exit 1
fi

if [[ -z "${REFERENCE_MANIFEST}" ]]; then
  manifest_from_param=$(param SEM_BOX_REFERENCE_MANIFEST "")
  if [[ -n "${manifest_from_param}" ]]; then
    REFERENCE_MANIFEST=$(resolve_case_path "${manifest_from_param}")
  else
    REFERENCE_MANIFEST=$(infer_reference_manifest || true)
  fi
fi

if [[ -z "${REFERENCE_DATA_DIR}" ]]; then
  reference_from_param=$(param SEM_BOX_REFERENCE_DATA_DIR "")
  if [[ -n "${reference_from_param}" ]]; then
    REFERENCE_DATA_DIR=$(resolve_case_path "${reference_from_param}")
  else
    REFERENCE_DATA_DIR=$(infer_reference_data_dir) || {
      echo "Error: set SEM_BOX_REFERENCE_DATA_DIR or INJECTED_WAVEFIELD_PATH in ${PARAM_FILE}" >&2
      exit 1
    }
  fi
fi

[[ -d "${CURRENT_DATA_DIR}" ]] || { echo "Error: missing current DATA directory: ${CURRENT_DATA_DIR}" >&2; exit 1; }
[[ -d "${REFERENCE_DATA_DIR}" ]] || { echo "Error: missing reference DATA directory: ${REFERENCE_DATA_DIR}" >&2; exit 1; }

files=(
  "meshfem3D_files/Mesh_Par_file"
  "meshfem3D_files/Coupling_Par_file"
  "meshfem3D_files/interfaces.dat"
  "meshfem3D_files/topo_top.dat"
  "meshfem3D_files/topo_CMB.dat"
)

status=0
if [[ -n "${REFERENCE_MANIFEST}" && -f "${REFERENCE_MANIFEST}" ]]; then
  while read -r expected rel; do
    [[ -n "${expected}" && -n "${rel}" ]] || continue
    [[ "${expected}" != \#* ]] || continue
    current="${CURRENT_DATA_DIR}/${rel}"
    if [[ ! -f "${current}" ]]; then
      echo "SEM box compatibility mismatch: missing ${rel}" >&2
      echo "  current:  ${current}" >&2
      echo "  manifest: ${REFERENCE_MANIFEST}" >&2
      status=1
      continue
    fi
    actual=$(sha256sum "${current}" | awk '{print $1}')
    if [[ "${actual}" != "${expected}" ]]; then
      echo "SEM box compatibility mismatch: checksum differs for ${rel}" >&2
      echo "  expected: ${expected}" >&2
      echo "  actual:   ${actual}" >&2
      echo "  manifest: ${REFERENCE_MANIFEST}" >&2
      status=1
    fi
  done < "${REFERENCE_MANIFEST}"
  if [[ "${status}" -ne 0 ]]; then
    cat >&2 <<EOF

The current SEM box does not match the manifest saved with the existing
InjectedWaves database:
  ${REFERENCE_MANIFEST}
EOF
    exit "${status}"
  fi
  echo "SEM box compatibility check passed."
  echo "Reference manifest: ${REFERENCE_MANIFEST}"
  echo "Current DATA:        ${CURRENT_DATA_DIR}"
  exit 0
fi

for rel in "${files[@]}"; do
  current="${CURRENT_DATA_DIR}/${rel}"
  reference="${REFERENCE_DATA_DIR}/${rel}"
  if [[ ! -f "${current}" && ! -f "${reference}" ]]; then
    continue
  fi
  if [[ ! -f "${current}" || ! -f "${reference}" ]]; then
    echo "SEM box compatibility mismatch: missing ${rel}" >&2
    echo "  current:   ${current}" >&2
    echo "  reference: ${reference}" >&2
    status=1
    continue
  fi
  if ! cmp -s "${current}" "${reference}"; then
    echo "SEM box compatibility mismatch: ${rel}" >&2
    diff -u "${reference}" "${current}" | sed -n '1,80p' >&2 || true
    status=1
  fi
done

if [[ "${status}" -ne 0 ]]; then
  cat >&2 <<EOF

The current SEM box does not match the SPECFEM3D DATA used by the existing
InjectedWaves database. Reuse is unsafe unless these files match exactly:
  ${REFERENCE_DATA_DIR}
  ${CURRENT_DATA_DIR}
EOF
  exit "${status}"
fi

echo "SEM box compatibility check passed."
echo "Reference DATA: ${REFERENCE_DATA_DIR}"
echo "Current DATA:   ${CURRENT_DATA_DIR}"
