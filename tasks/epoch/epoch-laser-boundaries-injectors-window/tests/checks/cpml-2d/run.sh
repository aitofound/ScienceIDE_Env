#!/usr/bin/env bash
# Check cpml-2d: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh --help                       list the runtime knobs below
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_NX=64 sab.py task selfcheck ...  A knob whose name matches a
# "# SAB_..." marker in the deck replaces that deck line's whole right-hand
# side, so its value is a deck expression, not necessarily a bare number.
# Parallel build jobs default to the CPUs this container may use (cgroup v2 cpu.max), not the host count.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_NX "240" "cells along x (upstream: 240); runtime scales linearly, and with the step count through the CFL time step"
knob SAB_NY "nx/3" "cells along y as a deck expression (upstream: nx/3); the CPML layer runs the full transverse extent"
knob SAB_T_END "75 * femto" "graded end time as a deck expression (upstream: 75 * femto); runtime scales linearly"
knob SAB_DT_SNAPSHOT "25 * femto" "time between SDF dumps (upstream: 25 * femto, 4 dumps); fewer dumps means less I/O but the graded dump indices move"
knob SAB_MAXWELL_SOLVER "lehe_x" "interior Maxwell stencil (upstream default targets: lehe_x and pukhov); yee is the third deck of epoch2d/tests/maxwell_solvers, and the CPML boundary this check grades is the same in all of them"
knob SAB_CPML_THICKNESS "6" "cells in the CPML absorbing layer at each x face (EPOCH default, and the value the upstream deck runs at: 6); the layer is inside the graded domain"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the one build of the pinned source (default: the CPUs allowed to this container)"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
[ -d "$CHECK_DIR/ic/$IC" ] || { echo "run.sh: no initial condition ic/$IC" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

# Upstream test this check reproduces: code/epoch/epoch2d/tests/maxwell_solvers/lehe_x/input.deck (one of the two default targets of epoch2d/tests/test_maxwell_solvers.py)
# Build: only the epoch2d binary of the pinned tree (the SDF library is built with it).
BUILD_START=$(date +%s)
make -C "$WORK/src/epoch2d" COMPILER=gfortran -j"$SAB_MAKE_JOBS" > "$WORK/make.log" 2>&1
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # reported to the driver; the budget counts run time only

# Apply the knobs to the marked deck lines, then run and extract the graded arrays.
export OMPI_ALLOW_RUN_AS_ROOT=1 OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1

mkdir -p "$WORK/main"
cp "$CHECK_DIR/ic/$IC/input.deck" "$WORK/main/input.deck"
python3 - "$WORK/main/input.deck" SAB_NX SAB_NY SAB_T_END SAB_DT_SNAPSHOT SAB_MAXWELL_SOLVER SAB_CPML_THICKNESS <<'PY'
import os, re, sys
path, markers = sys.argv[1], sys.argv[2:]
text = open(path, encoding="utf-8").read()
for marker in markers:
    pattern = re.compile(r"(?m)^(\s*[A-Za-z_0-9]+\s*=\s*).*?(\s*#\s*" + re.escape(marker) + r")\s*$")
    text, n = pattern.subn(lambda m: m.group(1) + os.environ[marker] + m.group(2), text)
    if n != 1:
        raise SystemExit("run.sh: deck marker %s matched %d lines, expected 1" % (marker, n))
open(path, "w", encoding="utf-8").write(text)
PY
( cd "$WORK/src/epoch2d" && echo "$WORK/main" \
    | mpirun -n 4 --oversubscribe --bind-to none ./bin/epoch2d > "$WORK/main/epoch.log" 2>&1 )
python3 "$CHECK_DIR/extract.py" "$WORK/main" "$OUT_DIR" main
