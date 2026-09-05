#!/usr/bin/env bash
# Check injector-2d: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     OPTIONAL: the nominal inputs on the alternative build (ALTBUILD below)
#   run.sh --help                       list the runtime knobs below, and the altbuild line when one is declared
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
knob SAB_NX "64" "cells along x (deck: 128); runtime scales with the cell count and with the step count through the CFL time step"
knob SAB_NY "64" "cells along y (deck: 128); also the number of transverse injector accumulator cells"
knob SAB_T_END "2.0e-2" "graded end time in seconds (deck: 3.0e-1); runtime scales linearly"
knob SAB_DT_SNAPSHOT "1.0e-2" "time between SDF dumps (deck: 0.5e-2, 61 dumps); fewer dumps means less I/O but the graded dump indices move"
knob SAB_PPC "8" "macroparticles per cell, for the background load and for the injector (deck: 8); runtime scales linearly and the random stream changes with it"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the one build of the pinned source (default: the CPUs allowed to this container)"
# Alternative build, OPTIONAL. Set ALTBUILD to one line naming a legitimately different build of the
# same source (IEEE mode, -O0, a second compiler present in the image: something a correct candidate
# could plausibly be) ONLY when this check can be built that way; leave it empty otherwise. When it is
# set, `run.sh altbuild` runs ic/nominal on that build and selfcheck measures the check's floor from it.
ALTBUILD="the same pinned source and deck on the same shipped gfortran build with epoch2d/Makefile's one FFLAGS line (line 72, -O3 -g -std=f2003) changed to -O0 -g -std=f2003 in the scratch build copy only: EPOCH's own MODE=debug profile aborts with SIGFPE inside Open MPI's own mpi_minimal_init (src/housekeeping/mpi_routines.F90:109) under this Open MPI before any EPOCH arithmetic runs, so this check uses the shipped build's exact flags at one lower optimisation level instead, dropping none of the compiler's own semantics and none of EPOCH's own code path."
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"
if [ "$IC" = altbuild ]; then
  [ -n "$ALTBUILD" ] || { echo "run.sh: this check declares no alternative build" >&2; exit 2; }
  INPUTS=nominal
fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

# Upstream test this check reproduces: code/epoch/epoch2d/example_decks/injectors.deck
# Build: only the epoch2d binary of the pinned tree (the SDF library is built with it).
BUILD_START=$(date +%s)
if [ "$IC" = altbuild ]; then
  MAKEFILE="$WORK/src/epoch2d/Makefile"
  BEFORE=$(grep -c '^  FFLAGS = -O3 -g -std=f2003$' "$MAKEFILE")
  [ "$BEFORE" -eq 1 ] || { echo "run.sh: altbuild Makefile FFLAGS pattern matched $BEFORE lines in epoch2d/Makefile, expected 1" >&2; exit 2; }
  sed -i 's/^  FFLAGS = -O3 -g -std=f2003$/  FFLAGS = -O0 -g -std=f2003/' "$MAKEFILE"
  AFTER=$(grep -c '^  FFLAGS = -O0 -g -std=f2003$' "$MAKEFILE")
  [ "$AFTER" -eq 1 ] || { echo "run.sh: altbuild Makefile FFLAGS edit did not take in epoch2d/Makefile" >&2; exit 2; }
  make -C "$WORK/src/epoch2d" COMPILER=gfortran -j"$SAB_MAKE_JOBS" > "$WORK/make.log" 2>&1
else
  make -C "$WORK/src/epoch2d" COMPILER=gfortran -j"$SAB_MAKE_JOBS" > "$WORK/make.log" 2>&1
fi
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # reported to the driver; the budget counts run time only

# Apply the knobs to the marked deck lines, then run and extract the graded arrays.
export OMPI_ALLOW_RUN_AS_ROOT=1 OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1

mkdir -p "$WORK/main"
cp "$CHECK_DIR/ic/$INPUTS/input.deck" "$WORK/main/input.deck"
python3 - "$WORK/main/input.deck" SAB_NX SAB_NY SAB_T_END SAB_DT_SNAPSHOT SAB_PPC <<'PY'
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
