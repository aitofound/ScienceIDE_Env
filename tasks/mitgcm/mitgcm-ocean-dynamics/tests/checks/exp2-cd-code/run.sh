#!/usr/bin/env bash
# Check exp2-cd-code: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh --help                       list the runtime knobs below
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# What it does: copies SOURCE_DIR, builds one MITgcm executable for this
# configuration with the tree's own genmake2 (build configuration in mods/:
# SIZE.h, packages.conf and the *_OPTIONS.h headers of the upstream
# experiment; gfortran optfile linux_amd64_gfortran, single process, tiles
# only), runs it in a scratch directory holding the deck ic/<ic>/ with
# nTimeSteps set from SAB_STEPS, and copies the graded files, every
# <field>.<iteration>.data/.meta pair of the final iteration written by
# MITgcm's end-of-run state dump (dumpInitAndLast), into OUT_DIR. The log,
# the initial-state dump, the final pickup and everything else stay behind.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_STEPS=12 sab.py task selfcheck ...
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_STEPS 40 "time steps (nTimeSteps in the deck, 108000 s each; the upstream deck runs 26); runtime scales linearly, the graded final-state files are named by the final iteration number"
knob SAB_BUILD_JOBS 4 "parallel make jobs for the per-check build of mitgcmuv; wall time only"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
[ -d "$CHECK_DIR/ic/$IC" ] || { echo "run.sh: no initial condition ic/$IC" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

# Upstream test this check reproduces: code/mitgcm/verification/exp2/input
[ -x "$WORK/src/tools/genmake2" ] || { echo "run.sh: $SOURCE_DIR has no tools/genmake2" >&2; exit 2; }
mkdir "$WORK/build"
BUILD_START=$(date +%s)
[ -f "$CHECK_DIR/mods/genmake_local" ] && cp "$CHECK_DIR/mods/genmake_local" "$WORK/build/"   # experiment build flags, read by genmake2 from the build dir
( cd "$WORK/build" \
  && "$WORK/src/tools/genmake2" -rootdir "$WORK/src" -mods "$CHECK_DIR/mods" \
       -optfile "$WORK/src/tools/build_options/linux_amd64_gfortran" \
  && make depend \
  && make -j "$SAB_BUILD_JOBS" ) >"$WORK/build.log" 2>&1 || { tail -n 60 "$WORK/build.log" >&2; echo "run.sh: build failed" >&2; exit 1; }
[ -x "$WORK/build/mitgcmuv" ] || { echo "run.sh: build left no mitgcmuv" >&2; exit 1; }
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only

mkdir "$WORK/run"
cp "$CHECK_DIR/ic/nominal"/* "$WORK/run/"
[ "$IC" = nominal ] || cp "$CHECK_DIR/ic/$IC"/* "$WORK/run/"   # variant: the deck files that differ, laid over nominal
python3 - "$WORK/run/data" "$SAB_STEPS" <<'PY'
import re, sys
path, steps = sys.argv[1], int(sys.argv[2])
text = open(path, encoding="utf-8").read()
text, n = re.subn(r"^(\s*nTimeSteps\s*=\s*)\S+", lambda m: m.group(1) + str(steps) + ",", text, count=1, flags=re.M | re.I)
if n != 1:
    sys.exit("run.sh: deck has no nTimeSteps line")
open(path, "w", encoding="utf-8").write(text)
PY
cd "$WORK/run"
# A single-process MITgcm writes its log to standard output (STDOUT.0000 only exists for MPI runs).
"$WORK/build/mitgcmuv" >mitgcmuv.stdout 2>mitgcmuv.stderr || { tail -n 40 mitgcmuv.stdout mitgcmuv.stderr >&2 || true; echo "run.sh: mitgcmuv failed" >&2; exit 1; }
grep -q "PROGRAM MAIN: Execution ended Normally" mitgcmuv.stdout || { tail -n 40 mitgcmuv.stdout mitgcmuv.stderr >&2 || true; echo "run.sh: mitgcmuv did not end normally" >&2; exit 1; }
# The final iteration is the deck's nIter0 plus the steps run; collect its dump.
final="$(ls *.data | sed -nE 's/^[A-Za-z_0-9]+\.([0-9]{10})\.data$/\1/p' | sort | tail -1)"
[ -n "$final" ] || { echo "run.sh: no state dump written" >&2; exit 1; }
ls *."$final".data | grep -q "^T\." || { echo "run.sh: final dump $final has no T field" >&2; exit 1; }
cp ./*."$final".data ./*."$final".meta "$OUT_DIR/"
