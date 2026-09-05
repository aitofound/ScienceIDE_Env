#!/usr/bin/env bash
# Check ex-gemreconnection-mhdhyppe: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     OPTIONAL: the nominal inputs on the alternative build (ALTBUILD below)
#   run.sh --help                       list the runtime knobs below, and the altbuild line when one is declared
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# This reproduces the upstream recipe the upstream example Param/GEMRECONNECTION/PARAM.in.MhdHypPe (no Makefile.test target) of code/batsrus/Makefile.test:
# configure with Config.pl, build BATSRUS.exe and PostIDL.exe, create a run
# directory with `make rundir`, copy the initial condition in as PARAM.in, run
# BATSRUS.exe on 2 MPI ranks, and merge the per-processor .idl pieces with
# PostProc.pl -m.

KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_TMAX_SCALE "1.0" "multiplies every positive tSimulationMax in PARAM.in (the simulated window); run time scales with it"
knob SAB_MPI_RANKS "2" "MPI ranks BATSRUS.exe runs on; the graded value is the 2 ranks upstream uses"
knob SAB_BUILD_JOBS "4" "parallel make jobs for the BATSRUS build; affects build time only, never the graded run"
# Alternative build, OPTIONAL. Set ALTBUILD to one line naming a legitimately different build of the
# same source (IEEE mode, -O0, a second compiler present in the image: something a correct candidate
# could plausibly be) ONLY when this check can be built that way; leave it empty otherwise. When it is
# set, `run.sh altbuild` runs ic/nominal on that build and selfcheck measures the check's floor from it.
ALTBUILD="the same Config.pl configuration built with ./Config.pl -O0 before make BATSRUS, which sets every OPTn level of Makefile.conf to -O0 where the shipped gfortran template uses -O3; same pinned source, same deck"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
# Nothing below may read standard input: the produce driver feeds its list of
# checks to a `while read` loop, and a child that drains that pipe (mpiexec
# forwards stdin to rank 0) would swallow the rest of the check set.
exec < /dev/null
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"
if [ "$IC" = altbuild ]; then
  [ -n "$ALTBUILD" ] || { echo "run.sh: this check declares no alternative build" >&2; exit 2; }
  INPUTS=nominal
fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
export LC_ALL=C LANG=C

WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/param" "$WORK/src"
cp -R "$SOURCE_DIR/." "$WORK/src"
chmod -R u+w "$WORK/src"
cp "$CHECK_DIR/ic/$INPUTS"/* "$WORK/param/"

# ---- runtime knobs are applied to the parameter file before anything is built
awk -v s="$SAB_TMAX_SCALE" '{ if ($2 == "tSimulationMax" && $1 + 0 > 0) sub(/^[^ \t]+/, sprintf("%.12g", $1 * s)); print }' \
  "$WORK/param/PARAM.in" > "$WORK/param/PARAM.tmp" && mv "$WORK/param/PARAM.tmp" "$WORK/param/PARAM.in"

# ---- build (the upstream example Param/GEMRECONNECTION/PARAM.in.MhdHypPe (no Makefile.test target)); the seconds are reported and excluded from the graded run time
BUILD_START=$(date +%s)
cd "$WORK/src"
./Config.pl -install -compiler=gfortran > "$WORK/install.log" 2>&1 \
  || { tail -40 "$WORK/install.log" >&2; echo "run.sh: Config.pl -install failed" >&2; exit 3; }
# The gfortran build template compiles with plain gfortran and only links with
# mpif90, so mpif.h is not on the compile path of a distribution MPI. INCL_EXTRA
# is the template's own hook for extra search directories; fill it with the
# include flags of the MPI wrapper. Identical for the reference and the candidate.
MPI_INC="$(mpif90 -showme:compile 2>/dev/null || true)"
printf 'INCL_EXTRA = %s\n' "$MPI_INC" >> Makefile.conf
./Config.pl -default -noopenmp -noacc -u=GemReconnect -e=MhdHypPe -ng=2 -g=8,8,1 > "$WORK/config.log" 2>&1 \
  || { tail -40 "$WORK/config.log" >&2; echo "run.sh: Config.pl failed" >&2; exit 3; }
if [ "$IC" = altbuild ]; then
  ./Config.pl -O0 >> "$WORK/config.log" 2>&1 \
    || { tail -40 "$WORK/config.log" >&2; echo "run.sh: Config.pl -O0 failed" >&2; exit 3; }
  grep -q '^OPT3 = -O0' Makefile.conf \
    || { echo "run.sh: Config.pl -O0 did not set OPT3 in Makefile.conf" >&2; exit 3; }
fi
make -j"$SAB_BUILD_JOBS" BATSRUS > "$WORK/build.log" 2>&1 \
  || { tail -60 "$WORK/build.log" >&2; echo "run.sh: BATSRUS build failed" >&2; exit 3; }
make PIDL >> "$WORK/build.log" 2>&1 \
  || { tail -40 "$WORK/build.log" >&2; echo "run.sh: PostIDL build failed" >&2; exit 3; }
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only

# ---- run directory and run
make rundir RUNDIR="$WORK/run" STANDALONE=YES GMDIR="$WORK/src" > "$WORK/rundir.log" 2>&1 \
  || { tail -40 "$WORK/rundir.log" >&2; echo "run.sh: make rundir failed" >&2; exit 4; }
cp "$WORK/param"/* "$WORK/run/"
cd "$WORK/run"
mpiexec -n "$SAB_MPI_RANKS" --oversubscribe ./BATSRUS.exe > runlog 2>&1 \
  || { tail -40 runlog >&2; echo "run.sh: BATSRUS.exe failed" >&2; exit 4; }
./PostProc.pl -m -replace RESULT > postproc.log 2>&1 \
  || { tail -40 postproc.log >&2; echo "run.sh: PostProc.pl failed" >&2; exit 4; }

# ---- the graded files, named as rubric.json lists them
cd "$WORK/run/RESULT/GM"
final="$(ls -1 z=0_var_1_t*.out 2>/dev/null | LC_ALL=C sort | tail -1 || true)"
[ -n "$final" ] || { echo "run.sh: no output file matching z=0_var_1_t*.out" >&2; exit 5; }
cp "$final" "$OUT_DIR/final.out"
