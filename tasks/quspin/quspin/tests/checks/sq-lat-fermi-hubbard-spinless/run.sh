#!/usr/bin/env bash
if [ "${1:-}" = "--help" ]; then printf '%s\n' \
  'SAB_LX=2 square-lattice width' \
  'SAB_LY=2 square-lattice height (runtime scales combinatorially with Lx*Ly)' \
  'SAB_THREADS=1 threads the BLAS/OpenMP reductions may use, default the declared per-check cpus'
  exit 0
fi
set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant>}"
case "$IC" in nominal|variant) ;; *) echo "run.sh: IC must be nominal or variant" >&2; exit 2;; esac
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
export SAB_LX="${SAB_LX:-2}"
export SAB_LY="${SAB_LY:-2}"
export OMP_NUM_THREADS="${SAB_THREADS:-1}" OPENBLAS_NUM_THREADS="${SAB_THREADS:-1}" MKL_NUM_THREADS="${SAB_THREADS:-1}"
echo "SAB_BUILD_SECONDS=0"
python3 "$CHECK_DIR/runner.py" "$CHECK_DIR/ic/$IC/config.json" "$OUT_DIR/observable.json"
