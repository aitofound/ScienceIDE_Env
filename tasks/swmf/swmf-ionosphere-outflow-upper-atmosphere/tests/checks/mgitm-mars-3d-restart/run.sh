#!/usr/bin/env bash
# Check mgitm-mars-3d-restart: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_END_MINUTES=2 sab.py task selfcheck ...
# Parallel build jobs default to the CPUs this container may use (cgroup v2 cpu.max), not the host count.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_START_MINUTES "2" "minutes of simulated time in the ungraded first stage, which writes the restart files (upstream: 2)"
knob SAB_END_MINUTES "3" "minutes of simulated time, counted from the first stage's #TIMESTART, at which the graded restart stage stops (upstream: 3, so the graded stage advances one further minute); run time scales with the difference"
knob SAB_RANKS "4" "MPI ranks for the graded restart stage (upstream runs the restart stage on 4; the deck's 2x2 blocks divide over 1, 2 or 4)"
knob SAB_START_RANKS "2" "MPI ranks for the ungraded first stage (upstream runs it on 1 or 2)"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the build of the pinned source (default: the CPUs allowed to this container); it changes build time only, never the graded run"
# Alternative build, OPTIONAL: the SWMF's own ./Config.pl -O0 rewrites every OPTn line of
# Makefile.conf to -O0 where the shipped gfortran template (share/build/Makefile.Linux.gfortran)
# builds at -O3 -- a legitimately different build of the same pinned source and deck.
ALTBUILD="the same Config.pl configuration built with ./Config.pl -O0 before make GITM, which sets every OPTn level of Makefile.conf to -O0 where the shipped gfortran template uses -O3; same pinned source, same decks"
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
exec < /dev/null                 # mpiexec must not read the produce driver's stdin
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
export LC_ALL=C OMP_NUM_THREADS=1

# One configured source is built directly at its final family path per solve.
# The shared root is private to test.sh produce; direct run.sh calls fall back
# to a fresh self-contained source copy.
. "$CHECK_DIR/build-cache.sh"
BUILD_FAMILY="mgitm-mars-small"
BUILD_SPEC='install=BATSRUS;compiler=gfortran;component=UA/MGITM;config=-Mars,-g=8,4,120,4;target=GITM'
BUILD_INPUT_KEY="none"
sab_prepare_build

# Upstream test this check reproduces: make -C UA/MGITM test_gitm_mars_3d, its
# restart stage (UA/MGITM/Makefile.test targets test_gitm_mars_3d_run, which
# ends with Restart.pl, and test_gitm_mars_3d_restart).
if [ "$SAB_BUILD_CACHE_HIT" -eq 1 ]; then
  sab_report_build_reuse
else
  cd "$SRC"
  BUILD_START=$(date +%s)
  GIT_TERMINAL_PROMPT=0 ./Config.pl -install=BATSRUS -compiler=gfortran > "$WORK/install.log" 2>&1
  if [ "$IC" = altbuild ]; then
    ./Config.pl -O0 >> "$WORK/build.log" 2>&1
    grep -q '^OPT3 = -O0' Makefile.conf || { echo "run.sh: Config.pl -O0 did not set OPT3 in Makefile.conf" >&2; exit 1; }
  fi
  cd "$SRC/UA/MGITM"
  ./Config.pl -Mars >> "$WORK/build.log" 2>&1
  ./Config.pl -g=8,4,120,4 >> "$WORK/build.log" 2>&1
  make -j"$SAB_MAKE_JOBS" GITM >> "$WORK/build.log" 2>&1
  BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
  test ! -e "$SRC/.sab-rundir-template"
  make rundir RUNDIR="$SRC/.sab-rundir-template" STANDALONE=YES > "$WORK/rundir.log" 2>&1
  sab_finish_build "$BUILD_SECONDS"
fi
cd "$SRC"

# Run directory exactly as the upstream test builds it.
[ -d "$SRC/.sab-rundir-template" ] || { echo "run.sh: family rundir template is missing" >&2; exit 1; }
mkdir "$WORK/run"
cp -R "$SRC/.sab-rundir-template/." "$WORK/run/"
# The knobs move the #TIMEEND of each stage; at the graded defaults both decks
# are copied through unchanged. The restart deck carries no #TIMESTART of its
# own, so its window is written from the first stage's start time.
python3 - "$CHECK_DIR/ic/$INPUTS" "$WORK/run" "$SAB_START_MINUTES" "$SAB_END_MINUTES" <<'PY'
import datetime, sys
from pathlib import Path
ic, run = Path(sys.argv[1]), Path(sys.argv[2])
start_minutes, end_minutes = float(sys.argv[3]), float(sys.argv[4])

def rewrite(name, minutes, start=None):
    lines = (ic / name).read_text(encoding="utf-8").split("\n")
    if start is None:
        i = lines.index("#TIMESTART")
        start = [int(float(lines[i + k].split()[0])) for k in range(1, 7)]
    j = lines.index("#TIMEEND")
    t = datetime.datetime(*start) + datetime.timedelta(minutes=minutes)
    for k, value in zip(range(j + 1, j + 7), (t.year, t.month, t.day, t.hour, t.minute, t.second)):
        parts = lines[k].split(None, 1)
        tail = "\t\t" + parts[1] if len(parts) > 1 else ""
        lines[k] = ("%02d" % value) + tail
    (run / name).write_text("\n".join(lines), encoding="utf-8")
    return start

start = rewrite("UAM.in.start", start_minutes)
rewrite("UAM.in.restart", end_minutes, start)
PY

cd "$WORK/run"
# Stage 1, ungraded: run the initial window and turn its restart dump into the
# restart input, exactly as the upstream run stage ends.
cp UAM.in.start UAM.in
if ! mpiexec -n "$SAB_START_RANKS" --oversubscribe ./GITM.exe > runlog_start 2>&1 < /dev/null; then
  echo "run.sh: GITM.exe failed in the ungraded first stage; last lines of its log follow" >&2
  tail -40 runlog_start >&2
  exit 1
fi
./Restart.pl > restart.log 2>&1 < /dev/null
# Put the first stage's own log and plot files out of the way so that the files
# PostProc.pl merges below, and the files graded, are the restart stage's alone.
mkdir -p UA/data/stage1
for f in UA/data/log0*.dat UA/data/3DALL_*; do if [ -e "$f" ]; then mv "$f" UA/data/stage1/; fi; done
# Stage 2, graded: restart and advance the further window.
cp UAM.in.restart UAM.in
if ! mpiexec -n "$SAB_RANKS" --oversubscribe ./GITM.exe > runlog_restart 2>&1 < /dev/null; then
  echo "run.sh: GITM.exe failed in the graded restart stage; last lines of its log follow" >&2
  tail -40 runlog_restart >&2
  exit 1
fi
./PostProc.pl RESULTS > postproc.log 2>&1 < /dev/null

# The graded files, under the fixed names rubric.json lists: the log the restart
# stage opens at its own first step, and the state at the end of its window.
grab() {
  local dest="$1" last="" f; shift
  for f in "$@"; do [ -e "$f" ] && last="$f"; done
  [ -n "$last" ] || { echo "run.sh: no output file matched: $*" >&2; exit 1; }
  cp "$last" "$OUT_DIR/$dest"
}
grab ua_log.dat RESULTS/UA/log0*.dat
grab ua_state.bin RESULTS/UA/3DALL_*.bin
