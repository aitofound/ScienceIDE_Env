#!/usr/bin/env bash
# Check ex-example27: replays code/quspin/examples/scripts/example27.py, the
# MKL-accelerated Liouville-von Neumann evolution of the 2x2 Fermi-Hubbard
# model, as the QuSpin production path.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh --help                       list the runtime and resource knobs
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_LX 2 "lattice width"
knob SAB_LY 2 "lattice height; Hilbert space size grows fast with Lx*Ly"
knob SAB_NSTEPS 400 "number of fixed-step RK4 time steps (dt=0.1); runtime scales linearly"
knob SAB_NSAMPLE 8 "number of graded time samples taken evenly across the evolution"
knob SAB_THREADS 1 "threads the BLAS/OpenMP reductions may use"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; exit 0; fi
set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant> | run.sh --help}"
case "$IC" in nominal|variant) ;; *) echo "run.sh: unknown ic '$IC'" >&2; exit 2;; esac
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
[ -f "$CHECK_DIR/ic/$IC/config.json" ] || { echo "run.sh: no ic/$IC/config.json" >&2; exit 2; }
export OMP_NUM_THREADS="$SAB_THREADS" OPENBLAS_NUM_THREADS="$SAB_THREADS" MKL_NUM_THREADS="$SAB_THREADS"
# sparse_dot_mkl loads libmkl_rt through ctypes and does not search the venv's
# lib directory by itself; point MKL_RT at the installed shared object.
if [ -z "${MKL_RT:-}" ]; then
  for cand in "$(python3 -c 'import sys; print(sys.prefix)')"/lib/libmkl_rt.so*; do
    [ -e "$cand" ] && export MKL_RT="$cand" && break
  done
fi
echo "SAB_BUILD_SECONDS=0"
python3 "$CHECK_DIR/runner.py" "$CHECK_DIR/ic/$IC/config.json" "$OUT_DIR/observable.json"
