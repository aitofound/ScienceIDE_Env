#!/usr/bin/env bash
# Check ex-moonimpact-restart: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (ALTBUILD below)
#   run.sh --help                       list the runtime knobs below, and the altbuild line when one is declared
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# Upstream test reproduced here: code/swmf/GM/BATSRUS/Param/MOONIMPACT/PARAM.in.restartsave
#   Makefile.test has no target for this deck pair: it is an upstream example.
#   configure:      ./Config.pl -default -u=MoonImpact -e=MhdHyp -ng=2 -g=6,6,6
#                   (the configuration of test_moonimpact, whose PARAM.in is the same deck
#                    with the restart commands and the impact switched off)
#   run:            mpiexec -n 2 ./BATSRUS.exe on PARAM.in.restartsave, then Restart.pl -i,
#                   then mpiexec -n 2 ./BATSRUS.exe on PARAM.in.restartread
#   post-process:   ./PostProc.pl -M -replace RESULTS/<stage>
#   upstream check: none is shipped; solution/solve.sh produces the reference from the
#                   pinned build, as the pipeline does for every upstream example.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_MAX_ITERATION=10 sab.py task selfcheck ...
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_MAX_ITERATION "50" "steady iterations of the background run (#STOP MaxIteration of PARAM.in.restartsave). Shortened 2026-09-13 from the upstream 300 (93 s once the frame rule below made both stages write frames instead of 1-2) under the 60 s window ruling"
knob SAB_SIMULATION_TIME "0.15" "physical seconds of the time-accurate impact run (#STOP tSimulationMax of PARAM.in.restartread); run time scales with it. Shortened 2026-09-13 from the upstream 1.0 alongside SAB_MAX_ITERATION"
knob SAB_PLOT_FRAMES "5" "minimum frames of the graded y=0 VAR tcp series in each stage before that stage ends; sets the RestartSave #SAVEPLOT cadence to SAB_MAX_ITERATION / SAB_PLOT_FRAMES steps and the RestartRead one to SAB_SIMULATION_TIME / SAB_PLOT_FRAMES seconds (floor 1 step / any positive seconds)"
knob SAB_MPI_RANKS "2" "MPI ranks; 2 is the upstream test decomposition and the graded one"
knob SAB_OMP_THREADS "1" "OpenMP threads per rank (upstream OMPIRUN default)"
knob SAB_BUILD_JOBS "0" "parallel make jobs; 0 means one per available core. Build time only, never graded"
ALTBUILD="the same Config.pl configuration built with ./Config.pl -O0 before make BATSRUS, which sets every OPTn level of Makefile.conf to -O0 where the shipped gfortran template uses -O3; same pinned source, same deck"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
# Nothing here reads standard input, and mpiexec would swallow the driver's
# check list if it were left connected.
exec </dev/null
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"
if [ "$IC" = altbuild ]; then
  [ -n "$ALTBUILD" ] || { echo "run.sh: this check declares no alternative build" >&2; exit 2; }
  INPUTS=nominal
fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
export LC_ALL=C
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
SRC="$WORK/src"
# The pinned source is the SWMF tree. The upstream standalone BATSRUS tests run in
# the standalone layout: GM/BATSRUS with share/ and util/ beside it, which is exactly
# what BATSRUS's own Config.pl -install clones into its root. Assemble that layout from
# the pinned tree here; SOURCE_DIR itself is never modified.
cp -R "$SOURCE_DIR/GM/BATSRUS/." "$SRC"
cp -R "$SOURCE_DIR/share" "$SRC/share"
cp -R "$SOURCE_DIR/util" "$SRC/util"

# Rewrite one #STOP value of a copied PARAM file: the runtime knobs above.
set_stop() {
  python3 - "$1" "$2" "$3" "$4" <<'PY'
import sys
path, occurrence, which, value = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
lines = open(path, encoding="utf-8").read().split("\n")
seen = 0
for index, line in enumerate(lines):
    if line.strip() != "#STOP":
        continue
    seen += 1
    if seen != occurrence:
        continue
    target = index + (1 if which == "MaxIteration" else 2)
    parts = lines[target].split("\t", 1)
    lines[target] = value + ("\t" + parts[1] if len(parts) > 1 else "")
    open(path, "w", encoding="utf-8").write("\n".join(lines))
    break
else:
    raise SystemExit("run.sh: #STOP occurrence %d not found in %s" % (occurrence, path))
PY
}

# Rewrite one #SAVEPLOT StringPlot entry (occurrence-th line whose value equals
# marker) to the given cadence: "Dn" rewrites DnSavePlot, "Dt" rewrites
# DtSavePlot. So the graded window and SAB_PLOT_FRAMES still control how many
# frames of that series get written, in either a steady-state or time-accurate stage.
set_cadence() {
  python3 - "$1" "$2" "$3" "$4" "$5" <<'PY'
import sys
path, occurrence, marker, which, value = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4], sys.argv[5]
lines = open(path, encoding="utf-8").read().split("\n")
seen = 0
for index, line in enumerate(lines):
    if line.split("\t", 1)[0].strip() != marker:
        continue
    seen += 1
    if seen != occurrence:
        continue
    target = index + (1 if which == "Dn" else 2)
    parts = lines[target].split("\t", 1)
    lines[target] = value + ("\t" + parts[1] if len(parts) > 1 else "")
    open(path, "w", encoding="utf-8").write("\n".join(lines))
    break
else:
    raise SystemExit("run.sh: StringPlot %r occurrence %d not found in %s" % (marker, occurrence, path))
PY
}

# The last file of a numbered plot series: the final state of the run. The one
# wall-clock line of the Tecplot header (AUXDATA SAVEDATE) is dropped, so that
# two runs of the same code produce byte-identical graded files and the
# verifier's "byte-identical, so probably no port happened" warning still works;
# gzip -n keeps the compressed form deterministic for the same reason.
copy_last() {
  local dest=$1; shift
  local last; last="$(ls -1 "$@" | LC_ALL=C sort | tail -1)"
  case "$last" in
    *.gz) gunzip -c "$last" | grep -v 'AUXDATA SAVEDATE' | gzip -n -c > "$dest" ;;
    *)    grep -v 'AUXDATA SAVEDATE' "$last" > "$dest" ;;
  esac
}

# Cores available to this container (the CFS quota, not the host's core count).
cores() {
  local quota period
  if [ -r /sys/fs/cgroup/cpu.max ]; then
    read -r quota period < /sys/fs/cgroup/cpu.max || true
    if [ "${quota:-max}" != max ] && [ -n "${period:-}" ]; then
      echo $(( (quota + period - 1) / period )); return
    fi
  fi
  getconf _NPROCESSORS_ONLN
}

JOBS="$SAB_BUILD_JOBS"; [ "$JOBS" != 0 ] || JOBS="$(cores)"
BUILD_START=$(date +%s)
if ! ( cd "$SRC" \
    && ./Config.pl -install -compiler=gfortran \
    && ./Config.pl -default -u=MoonImpact -e=MhdHyp -ng=2 -g=6,6,6 \
    && { [ "$IC" != altbuild ] || { ./Config.pl -O0 >> "$WORK/config.log" 2>&1 && grep -q '^OPT3 = -O0' Makefile.conf; }; } \
    && make -j"$JOBS" BATSRUS \
    && make PIDL ) > "$WORK/build.log" 2>&1; then
  echo "run.sh: build failed" >&2; tail -60 "$WORK/build.log" >&2; exit 1
fi
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only

RUN="$SRC/run_check"
( cd "$SRC" && make rundir RUNDIR=run_check STANDALONE=YES GMDIR="$SRC" ) >> "$WORK/build.log" 2>&1

# One BATSRUS run in the run directory; $1 is the name of its stdout log.
# stdin is closed for both: mpiexec forwards standard input to rank 0 and would
# otherwise drain the check list the produce driver feeds its loop.
batsrus() { ( cd "$RUN" && OMP_NUM_THREADS="$SAB_OMP_THREADS" mpiexec --oversubscribe -n "$SAB_MPI_RANKS" ./BATSRUS.exe > "$1" 2>&1 </dev/null ); }
postproc() { ( cd "$RUN" && ./PostProc.pl "$@" ) >> "$WORK/build.log" 2>&1 </dev/null; }

# One executable and one run directory for both runs, as the upstream deck pair
# intends: PARAM.in.restartread reads the restart file PARAM.in.restartsave writes.
cp "$CHECK_DIR/ic/$INPUTS/PARAM.in.restartsave" "$RUN/PARAM.in"
set_stop "$RUN/PARAM.in" 1 MaxIteration "$SAB_MAX_ITERATION"
CADENCE_SAVE=$(( SAB_MAX_ITERATION / SAB_PLOT_FRAMES )); [ "$CADENCE_SAVE" -ge 1 ] || CADENCE_SAVE=1
set_cadence "$RUN/PARAM.in" 1 "y=0 VAR tcp" Dn "$CADENCE_SAVE"
batsrus runlog_restartsave
postproc -M -replace RESULTS/RestartSave

cp "$CHECK_DIR/ic/$INPUTS/PARAM.in.restartread" "$RUN/PARAM.in"
set_stop "$RUN/PARAM.in" 1 tSimulationMax "$SAB_SIMULATION_TIME"
CADENCE_READ=$(python3 -c "print('%.10g' % ($SAB_SIMULATION_TIME / $SAB_PLOT_FRAMES))")
set_cadence "$RUN/PARAM.in" 1 "y=0 VAR tcp" Dt "$CADENCE_READ"
( cd "$RUN" && ./Restart.pl -i RESULTS/RestartSave/RESTART ) >> "$WORK/build.log" 2>&1
batsrus runlog_restartread
postproc -M -replace RESULTS/RestartRead

cat $(ls -1 "$RUN"/RESULTS/RestartSave/GM/log_n*.log | LC_ALL=C sort) > "$OUT_DIR/log_background.log"
cat $(ls -1 "$RUN"/RESULTS/RestartRead/GM/log_n*.log | LC_ALL=C sort) > "$OUT_DIR/log_impact.log"
copy_last "$OUT_DIR/y0_background.dat" "$RUN"/RESULTS/RestartSave/GM/y=0_var_*.dat
copy_last "$OUT_DIR/y0_impact.dat" "$RUN"/RESULTS/RestartRead/GM/y=0_var_*.dat

# The frame rule: count each stage's graded series' actual saves (before
# copy_last picks the last one) and fail if either stage did not yield enough.
FRAMES_SAVE=$(ls -1 "$RUN"/RESULTS/RestartSave/GM/y=0_var_*.dat 2>/dev/null | wc -l | tr -d ' ')
FRAMES_READ=$(ls -1 "$RUN"/RESULTS/RestartRead/GM/y=0_var_*.dat 2>/dev/null | wc -l | tr -d ' ')
echo "SAB_PLOT_FRAMES=$FRAMES_SAVE,$FRAMES_READ"
[ "$FRAMES_SAVE" -ge 5 ] || { echo "run.sh: only $FRAMES_SAVE frames of the graded y=0 VAR tcp series (RestartSave stage) were written (need >= 5)" >&2; exit 1; }
[ "$FRAMES_READ" -ge 5 ] || { echo "run.sh: only $FRAMES_READ frames of the graded y=0 VAR tcp series (RestartRead stage) were written (need >= 5)" >&2; exit 1; }
