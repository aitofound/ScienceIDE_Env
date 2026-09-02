#!/usr/bin/env bash
# Check moving-window-1d: the TEST half of the check.
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
knob SAB_NX "256" "cells in the window (deck: 256); runtime scales with the cell count and, through the CFL time step, with the number of window shifts"
knob SAB_T_END "10e-9" "graded end time in seconds (deck: 10e-9); the window advances 2e8 m/s, so this sets how many cells it shifts"
knob SAB_DT_SNAPSHOT "1e-9" "time between SDF dumps (deck: 1e-10, 101 dumps); fewer dumps means less I/O but the graded dump indices move"
knob SAB_PPC "5" "macroparticles loaded per cell (deck: 5); runtime scales linearly, and the random stream changes with it"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the one build of the pinned source (default: the CPUs allowed to this container)"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
[ -d "$CHECK_DIR/ic/$IC" ] || { echo "run.sh: no initial condition ic/$IC" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

# Upstream test this check reproduces: code/epoch/epoch1d/example_decks/window.deck
# Build: only the epoch1d binary of the pinned tree (the SDF library is built with it).
make -C "$WORK/src/epoch1d" COMPILER=gfortran -j"$SAB_MAKE_JOBS" > "$WORK/make.log" 2>&1

# Apply the knobs to the marked deck lines, then run and extract the graded arrays.
export OMPI_ALLOW_RUN_AS_ROOT=1 OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1

mkdir -p "$WORK/main"
cp "$CHECK_DIR/ic/$IC/input.deck" "$WORK/main/input.deck"
python3 - "$WORK/main/input.deck" SAB_NX SAB_T_END SAB_DT_SNAPSHOT SAB_PPC <<'PY'
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
( cd "$WORK/src/epoch1d" && echo "$WORK/main" \
    | mpirun -n 2 --oversubscribe --bind-to none ./bin/epoch1d > "$WORK/main/epoch.log" 2>&1 )
python3 "$CHECK_DIR/extract.py" "$WORK/main" "$OUT_DIR" main
