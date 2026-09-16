#!/usr/bin/env bash
# Check shelfice-remesh: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (genmake2 -ieee; see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
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
knob SAB_STEPS 80 "time steps (nTimeSteps in the deck, 300 s each; the upstream deck runs 20); runtime scales linearly, the graded final-state files are named by the final iteration number"
knob SAB_BUILD_JOBS 4 "parallel make jobs for the per-check build of mitgcmuv; wall time only"
# Alternative build: the same source under genmake2 -ieee (gfortran -O0 -ffloat-store, strict IEEE arithmetic)
# instead of the optimised optfile. `run.sh altbuild` runs ic/nominal on it; selfcheck measures the floor from it.
ALTBUILD="genmake2 -ieee: the same source at -O0 -ffloat-store, strict IEEE arithmetic, instead of the optimised optfile"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"; GENMAKE_EXTRA=()
if [ "$IC" = altbuild ]; then INPUTS=nominal; GENMAKE_EXTRA=(-ieee); fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

# Upstream test this check reproduces: code/mitgcm/verification/shelfice_2d_remesh/input
[ -x "$WORK/src/tools/genmake2" ] || { echo "run.sh: $SOURCE_DIR has no tools/genmake2" >&2; exit 2; }
mkdir "$WORK/build"
OPTFILE="$WORK/src/tools/build_options/linux_amd64_gfortran"
case "$(uname -m)" in
  aarch64|arm64)
    # The upstream x86 optfile's medium code model is not a valid GCC option
    # on arm64; removing only that target-specific flag preserves the recipe.
    sed 's/-mcmodel=medium//g' "$OPTFILE" >"$WORK/build/linux_arm64_gfortran"
    OPTFILE="$WORK/build/linux_arm64_gfortran"
    ;;
esac
BUILD_START=$(date +%s)
[ -f "$CHECK_DIR/mods/genmake_local" ] && cp "$CHECK_DIR/mods/genmake_local" "$WORK/build/"   # experiment build flags, read by genmake2 from the build dir
( cd "$WORK/build" \
  && "$WORK/src/tools/genmake2" -rootdir "$WORK/src" -mods "$CHECK_DIR/mods" \
       -optfile "$OPTFILE" ${GENMAKE_EXTRA[@]+"${GENMAKE_EXTRA[@]}"} \
  && make depend \
  && make -j "$SAB_BUILD_JOBS" ) >"$WORK/build.log" 2>&1 || { tail -n 60 "$WORK/build.log" >&2; echo "run.sh: build failed" >&2; exit 1; }
[ -x "$WORK/build/mitgcmuv" ] || { echo "run.sh: build left no mitgcmuv" >&2; exit 1; }
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only

mkdir "$WORK/run"
cp "$CHECK_DIR/ic/nominal"/* "$WORK/run/"
[ "$INPUTS" = nominal ] || cp "$CHECK_DIR/ic/$INPUTS"/* "$WORK/run/"   # variant: the deck files that differ, laid over nominal
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
