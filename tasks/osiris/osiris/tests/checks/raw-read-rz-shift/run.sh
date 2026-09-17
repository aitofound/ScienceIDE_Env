#!/usr/bin/env bash
# OSIRIS check raw-read-rz-shift: build the pinned source in a reusable dimension cache and run one reduced official deck.
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_STEPS "4" "time steps in the reduced scientific window; runtime scales approximately linearly"
knob SAB_CPUS "4" "MPI ranks; supported values are 1 or 4 and the graded decomposition uses 4"
ALTBUILD=""
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant> | run.sh --help}"
case "$IC" in nominal|variant) ;; altbuild) echo "run.sh: this check declares no alternative build" >&2; exit 2 ;; *) echo "run.sh: unknown initial condition: $IC" >&2; exit 2 ;; esac
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
case "$SAB_CPUS" in 1|4) ;; *) echo "run.sh: SAB_CPUS must be 1 or 4" >&2; exit 2 ;; esac
[ -f "$CHECK_DIR/ic/$IC/deck" ] || { echo "run.sh: missing ic/$IC/deck" >&2; exit 2; }

DIM=2
CACHE="/tmp/sab-osiris-${DIM}d-production"
BUILD_START=$(date +%s)
if [ ! -x "$CACHE/bin/osiris-${DIM}D.e" ]; then
  rm -rf "$CACHE"
  mkdir -p "$CACHE"
  cp -R "$SOURCE_DIR/." "$CACHE/"
  CFG="$CACHE/config/osiris_sys.sciaccel.gnu"
  cp "$CACHE/config/osiris_sys.docker.gnu" "$CFG"
  sed -i \
    -e 's/^F90 =.*/F90 = h5pfc -cpp/' \
    -e 's/^F03 =.*/F03 = $(F90)/' \
    -e 's/^cc  =.*/cc  = mpicc/' \
    -e 's/^CC  =.*/CC  = mpicc/' \
    -e 's/^FPP =.*/FPP = gcc -C -E -x assembler-with-cpp/' \
    -e 's/--openmp/-fopenmp/g' \
    -e 's/-march=native//g' \
    -e 's/-std=c99/-std=c99 -D_DEFAULT_SOURCE/g' \
    -e 's|^MPI_FCOMPILEFLAGS =.*|MPI_FCOMPILEFLAGS = $(shell mpifort --showme:compile)|' \
    -e 's|^MPI_FLINKFLAGS    =.*|MPI_FLINKFLAGS = $(shell mpifort --showme:link)|' \
    -e 's|^H5_FCOMPILEFLAGS =.*|H5_FCOMPILEFLAGS = -I/usr/include/hdf5/openmpi|' \
    -e 's|^H5_FLINKFLAGS    =.*|H5_FLINKFLAGS =|' \
    -e 's|^# H5_HAVE_PARALLEL = 1|H5_HAVE_PARALLEL = 1\nFFTW_ROOT = /usr\nFFTW_FCOMPILEFLAGS = -I/usr/include\nFFTW_FLINKFLAGS = -lfftw3|' \
    "$CFG"
  (cd "$CACHE" && ./configure -s sciaccel.gnu -d "$DIM" -t production && make)
  BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
else
  BUILD_SECONDS=0
fi
echo "SAB_BUILD_SECONDS=$BUILD_SECONDS"

WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
DECK="$WORK/deck"
cp "$CHECK_DIR/ic/$IC/deck" "$DECK"
python3 - "$DECK" "$SAB_STEPS" "$SAB_CPUS" "$DIM" <<'PY'
import re, sys
from pathlib import Path
p, steps, cpus, dim = Path(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
text = p.read_text(encoding="utf-8")
m = re.search(r"(?mi)^\s*dt\s*=\s*([0-9.+\-dDeE]+)", text)
if not m: raise SystemExit("deck has no dt")
dt = float(m.group(1).replace("d", "e").replace("D", "E"))
text = re.sub(r"(?i)\btmax\s*=\s*[^,\n}]+", f"tmax = {dt * steps:.17g}", text, count=1)
nodes = {1: ((1,), (4,)), 2: ((1, 1), (2, 2)), 3: ((1, 1, 1), (1, 2, 2))}[dim][0 if cpus == 1 else 1]
text = re.sub(r"(?mi)^(\s*)node_number\s*\([^\n=]+\)\s*=.*$", r"\1node_number(1:%d) = %s," % (dim, ", ".join(map(str, nodes))), text, count=1)
p.write_text(text, encoding="utf-8")
PY

run_deck() {
  local deck=$1 rundir=$2
  mkdir -p "$rundir"
  (cd "$rundir" && mpirun --oversubscribe -n "$SAB_CPUS" "$CACHE/bin/osiris-${DIM}D.e" "$deck")
}

RAW_MODE="tenth"
if [ "$RAW_MODE" != none ]; then
  PRODUCER="$WORK/producer"
  cp "$CHECK_DIR/ic/$IC/producer" "$PRODUCER"
  PRODUCER_STEPS=1; RAW_FILE="RAW-driver-000000.h5"
  if [ "$RAW_MODE" = tenth ]; then PRODUCER_STEPS=10; RAW_FILE="RAW-driver-000010.h5"; fi
  python3 - "$PRODUCER" "$PRODUCER_STEPS" "$SAB_CPUS" <<'PY'
import re, sys
from pathlib import Path
p, steps, cpus = Path(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
text = p.read_text(encoding="utf-8")
m = re.search(r"(?mi)^\s*dt\s*=\s*([0-9.+\-dDeE]+)", text)
dt = float(m.group(1).replace("d", "e").replace("D", "E"))
text = re.sub(r"(?i)\btmax\s*=\s*[^,\n}]+", f"tmax = {dt * steps:.17g}", text, count=1)
nodes = (1, 1) if cpus == 1 else (2, 2)
text = re.sub(r"(?mi)^(\s*)node_number\s*\([^\n=]+\)\s*=.*$", r"\1node_number(1:2) = %s," % ", ".join(map(str, nodes)), text, count=1)
p.write_text(text, encoding="utf-8")
PY
  run_deck "$PRODUCER" "$WORK/producer-run"
  RAW_PATH="$(find "$WORK/producer-run" -type f -name "$RAW_FILE" -print -quit || true)"
  [ -n "$RAW_PATH" ] || { echo "run.sh: producer did not create $RAW_FILE" >&2; exit 1; }
  mkdir -p "$WORK/run"
  cp "$RAW_PATH" "$WORK/run/$RAW_FILE"
fi

run_deck "$DECK" "$WORK/run"
python3 "$CHECK_DIR/summarize.py" "$WORK/run" "$OUT_DIR" "invariants"
