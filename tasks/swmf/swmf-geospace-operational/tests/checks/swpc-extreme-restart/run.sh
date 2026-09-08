#!/usr/bin/env bash
# Check swpc-extreme-restart: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_STEADY_SCALE=0.25 sab.py task selfcheck ...
# Parallel build jobs default to the CPUs this container may use (cgroup v2 cpu.max), not the host count.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_STEADY_SCALE "0.1" "multiplies the MaxIter of every steady-state #STOP block of the deck (the upstream test's own reduction of the deck's 700 and 1500 to 70 and 200 is already applied; this knob further multiplies those to the graded default of 7 and 20 cumulative iterations, cut to fit the suite inside its budget while keeping a genuine two-stage relaxation); run time scales with it"
knob SAB_STOP_SCALE "0.15" "multiplies the time-accurate window of both runs (120 s at the upstream value in the first run, and this check's own restart window, matched to the first run); the graded default gives an 18 s window in each run; SAB_COUPLE_MAX=5.0 caps every positive DtCouple in both copied decks, preserving the existing 5 s GM-IE period and giving every active GM-IM/IE-IM/GM-RB/RB path at least three coupling opportunities in each window; every output cadence this check grades is shortened to match so each window still carries several saved frames; run time scales with it"
knob SAB_COUPLE_MAX "5.0" "caps every positive DtCouple in the copied deck at 5.0 s; this coordinated upstream-clock setting preserves the existing 5 s GM-IE period and gives every active GM-IM, IE-IM and GM-RB/RB path at least three coupling opportunities in the 18 s graded window; run time scales with it"
knob SAB_RANKS "8" "MPI ranks for mpiexec; the deck's #COMPONENTMAP divides them between GM, IE and IM, so this changes the domain decomposition as well as the run time"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs when this check builds its audited configuration (default: the CPUs allowed to this container; unused when a prior check in this solve supplied the shared build); it changes build time only, never the graded run"
ALTBUILD="the same Config.pl configuration built with ./Config.pl -O0 before make SWMF, which sets every OPTn level of Makefile.conf to -O0 where the shipped gfortran template uses -O3; same pinned source, same deck"
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
BUILD_CONFIG="mhd-ng2-ie181-pidl"
BUILD_MODE=default
[ "$IC" = altbuild ] && BUILD_MODE=o0
CACHE_KEY="$BUILD_CONFIG-$IC-$BUILD_MODE"  # IC keeps nominal, variant and altbuild solves mechanically separate.
BUILD_REUSED=0
BUILD_CACHE_STATUS=local
if [ -n "${SAB_SHARED_BUILD_ROOT:-}" ]; then
  mkdir -p "$SAB_SHARED_BUILD_ROOT"
  CACHE_DIR="$SAB_SHARED_BUILD_ROOT/$CACHE_KEY"
  if [ -f "$CACHE_DIR/.sab-build-complete" ]; then
    grep -Fxq "config=$BUILD_CONFIG" "$CACHE_DIR/.sab-build-complete" || { echo "run.sh: shared-build marker has the wrong configuration: $CACHE_DIR" >&2; exit 1; }
    grep -Fxq "ic=$IC" "$CACHE_DIR/.sab-build-complete" || { echo "run.sh: shared-build marker has the wrong initial condition: $CACHE_DIR" >&2; exit 1; }
    grep -Fxq "mode=$BUILD_MODE" "$CACHE_DIR/.sab-build-complete" || { echo "run.sh: shared-build marker has the wrong build mode: $CACHE_DIR" >&2; exit 1; }
    SRC="$CACHE_DIR"
    BUILD_REUSED=1
    BUILD_CACHE_STATUS=reused
  elif [ -e "$CACHE_DIR" ]; then
    echo "run.sh: refusing incomplete shared build: $CACHE_DIR" >&2
    exit 1
  else
    mkdir "$CACHE_DIR"
    cp -R "$SOURCE_DIR/." "$CACHE_DIR"
    SRC="$CACHE_DIR"
    BUILD_CACHE_STATUS=built
  fi
else
  SRC="$WORK/src"
  cp -R "$SOURCE_DIR/." "$SRC"
fi
export LC_ALL=C OMP_NUM_THREADS=1 GIT_TERMINAL_PROMPT=0
export OMPI_ALLOW_RUN_AS_ROOT=1 OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1

# Upstream test this check reproduces: Param/SWPC/PARAM.in_extreme_restart (upstream example deck, restart stage; no Makefile.test target)
# Build: Config.pl -install writes Makefile.conf and Makefile.def for this
# working copy (GIT_TERMINAL_PROMPT=0 so that its optional srcUserExtra clone
# fails at once instead of waiting for credentials), then the upstream test's
# own version and option lines select the components, the equation set, the
# block size and the ionosphere grid, and make builds SWMF.exe and the
# post-processing executables.
cd "$SRC"
if [ "$BUILD_REUSED" -eq 1 ]; then
  BUILD_SECONDS=0
else
  BUILD_START=$(date +%s)
  ./Config.pl -install=BATSRUS -compiler=gfortran > "$WORK/install.log" 2>&1
  ./Config.pl -default -v=Empty,GM/BATSRUS,IE/Ridley_serial,IM/RCM2 >> "$WORK/build.log" 2>&1
  ./Config.pl -o=GM:u=Default,e=Mhd,ng=2,g=8,8,8,IE:g=181,361 >> "$WORK/build.log" 2>&1
  if [ "$IC" = altbuild ]; then
    ./Config.pl -O0 >> "$WORK/build.log" 2>&1
    grep -q '^OPT3 = -O0' Makefile.conf || { echo "run.sh: Config.pl -O0 did not set OPT3 in Makefile.conf" >&2; exit 1; }
  fi
  make -j"$SAB_MAKE_JOBS" SWMF >> "$WORK/build.log" 2>&1
  make PIDL >> "$WORK/build.log" 2>&1
  BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
  if [ -n "${SAB_SHARED_BUILD_ROOT:-}" ]; then
    printf 'config=%s\nic=%s\nmode=%s\n' "$BUILD_CONFIG" "$IC" "$BUILD_MODE" > "$SRC/.sab-build-complete"
  fi
fi
echo "SAB_BUILD_CACHE_STATUS=$BUILD_CACHE_STATUS config=$BUILD_CONFIG ic=$IC mode=$BUILD_MODE"
echo "SAB_BUILD_SECONDS=$BUILD_SECONDS"   # actual compile time for this check; zero only when this solve reused its completed matching build

# Run directory exactly as the upstream test builds it: make rundir, then the
# deck and the inputs of ic/, then the recipe's own edits of the deck.
make rundir RUNDIR="$WORK/run" > "$WORK/rundir.log" 2>&1
cp -R "$CHECK_DIR/ic/$INPUTS/." "$WORK/run/"
cat > "$WORK/knobs.py" <<'KNOBS_PY'
# Rescale the steady-state iteration counts and the time-accurate window of one
# SWMF deck. Written by run.sh into $WORK so that the check directory stays the
# four contract files plus ic/.
import datetime, os, re, sys

deck, steady, stop = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
restart_in = sys.argv[4] if len(sys.argv) > 4 else ""
window = float(sys.argv[5]) if len(sys.argv) > 5 and sys.argv[5] else 0.0
lines = open(deck, encoding="utf-8").read().split("\n")
end = next((i for i, s in enumerate(lines) if s.strip() == "#END"), len(lines))


def put(i, text):
    m = re.match(r"^(\s*)(\S+)(.*)$", lines[i])
    if not m:
        raise SystemExit("knobs.py: %s line %d carries no value" % (deck, i + 1))
    lines[i] = m.group(1) + text + m.group(3)


def num(i):
    return float(lines[i].split()[0])


couple_max = float(os.environ.get("SAB_COUPLE_MAX", "5.0"))
if couple_max <= 0:
    raise SystemExit("knobs.py: SAB_COUPLE_MAX must be positive")


def cap_coupling_clocks():
    rows = []
    for i in range(end):
        if lines[i].strip().endswith("DtCouple"):
            value = num(i)
            if value > 0:
                rows.append((i, value))
    if not rows:
        raise SystemExit("knobs.py: copied deck has no active DtCouple rows")
    before = ",".join("%.1f" % value for _, value in rows)
    for i, value in rows:
        if value > couple_max:
            put(i, "%.1f" % couple_max)
    after = ",".join("%.1f" % num(i) for i, _ in rows)
    print("SAB_ACTIVE_DTCOUPLE_BEFORE=" + before)
    print("SAB_ACTIVE_DTCOUPLE_AFTER=" + after)


cap_coupling_clocks()

def clock(i):
    return datetime.datetime(*[int(num(i + k)) for k in range(1, 7)])


if steady != 1.0:
    for i in range(end):
        if lines[i].strip() == "#STOP":
            value = num(i + 1)
            if value > 0:
                put(i + 1, "%d" % max(1, int(round(value * steady))))

if stop != 1.0:
    i_end = next((i for i in range(end) if lines[i].strip() == "#ENDTIME"), None)
    if i_end is None:
        raise SystemExit("knobs.py: %s has no #ENDTIME to rescale" % deck)
    if restart_in:
        text = open(restart_in, encoding="utf-8").read().split("\n")
        t0, t_sim = None, None
        for j, s in enumerate(text):
            if s.strip() == "#STARTTIME":
                t0 = datetime.datetime(*[int(float(text[j + k].split()[0])) for k in range(1, 7)])
            elif s.strip() == "#TIMESIMULATION":
                t_sim = float(text[j + 1].split()[0])
        if t0 is None or t_sim is None:
            raise SystemExit("knobs.py: %s carries no #STARTTIME and #TIMESIMULATION" % restart_in)
        t1 = t0 + datetime.timedelta(seconds=t_sim + window * stop)
    else:
        i_start = next(i for i in range(end) if lines[i].strip() == "#STARTTIME")
        t0 = clock(i_start)
        t1 = t0 + (clock(i_end) - t0) * stop
    for k, value in zip(range(i_end + 1, i_end + 7),
                        (t1.year, t1.month, t1.day, t1.hour, t1.minute, t1.second)):
        put(k, "%d" % value)

open(deck, "w", encoding="utf-8").write("\n".join(lines))
KNOBS_PY
Scripts/TestParam.pl -F "$WORK/run/PARAM.in_extreme_init" > "$WORK/testparam.log" 2>&1 || true
perl -pi -e 'if(/MaxIter|MaxBlock/){s/700/70/; s/1500/200/; s/5000/350/}; s/#BORIS/BORIS/' "$WORK/run/PARAM.in_extreme_init"
perl -pi -e 's/^1 min(\s+DtOutput)/3$1/; s/^1 min(\s+DtSaveMagGrid)/3$1/; s/^20(\s+DtSaveMagGrid)/3$1/; s/^20(\s+DtOutput)/3$1/' "$WORK/run/PARAM.in_extreme_init"
perl -0777 -pi -e 's/(min idl\s+StringPlot\n)100(\s+DnSavePlot)/${1}5$2/' "$WORK/run/PARAM.in_extreme_init"
perl -pi -e 's/^360(\s+nGridLon)/72$1/; s/^171(\s+nGridLat)/25$1/;' "$WORK/run/PARAM.in_extreme_init"
perl -pi -e 's/^221(\s+nGridLon)/23$1/; s/^131(\s+nGridLat)/13$1/;' "$WORK/run/PARAM.in_extreme_init"
# The knobs rescale the steady-state iteration counts and the time-accurate
# window, then cap positive component coupling clocks; the graded defaults keep
# the 18 s window while exercising every active path at least three times.
python3 "$WORK/knobs.py" "$WORK/run/PARAM.in_extreme_init" "$SAB_STEADY_SCALE" "$SAB_STOP_SCALE"
cd "$WORK/run"
cp "PARAM.in_extreme_init" PARAM.in
# This check runs no INTERPOLATE.exe stage.
if ! mpiexec -n "$SAB_RANKS" --oversubscribe ./SWMF.exe > runlog 2>&1; then
  echo "run.sh: SWMF.exe failed; last lines of the run log follow" >&2
  tail -40 runlog >&2
  exit 1
fi
# Restart stage: the same run directory advanced from the restart tree the
# first run wrote, with the deck the upstream test restarts from.
# A deck with #RESTARTOUTDIR writes a dated SWMF_RESTART.<date> tree and the
# upstream recipe restarts from it by name; a deck without one leaves RESTART.out
# in the run directory and the upstream recipe calls Restart.pl with no argument.
RESTART_DIR="$(ls -d SWMF_RESTART.* 2>/dev/null | LC_ALL=C sort | tail -1 || true)"
if [ -n "$RESTART_DIR" ]; then
  ./Restart.pl -i "$RESTART_DIR" >> runlog 2>&1
elif [ -f RESTART.out ]; then
  ./Restart.pl >> runlog 2>&1
else
  echo "run.sh: the first run left no restart state to restart from" >&2; exit 1
fi
cd "$SRC"
Scripts/TestParam.pl -F "$WORK/run/PARAM.in_extreme_restart" >> "$WORK/testparam.log" 2>&1 || true
perl -pi -e 's/#BORIS/BORIS/' "$WORK/run/PARAM.in_extreme_restart"
perl -pi -e 's/^1 min(\s+DtOutput)/3$1/; s/^1 min(\s+DtSaveMagGrid)/3$1/; s/^20(\s+DtSaveMagGrid)/3$1/; s/^20(\s+DtOutput)/3$1/' "$WORK/run/PARAM.in_extreme_restart"
perl -0777 -pi -e 's/(aur idl\s+StringPlot\n-1\s+DnSavePlot\n)1 min(\s+DtSavePlot)/${1}3$2/' "$WORK/run/PARAM.in_extreme_restart"
perl -pi -e 's/^360(\s+nGridLon)/72$1/; s/^171(\s+nGridLat)/25$1/;' "$WORK/run/PARAM.in_extreme_restart"
perl -pi -e 's/^221(\s+nGridLon)/23$1/; s/^131(\s+nGridLat)/13$1/;' "$WORK/run/PARAM.in_extreme_restart"
python3 "$WORK/knobs.py" "$WORK/run/PARAM.in_extreme_restart" 1 "$SAB_STOP_SCALE" "$WORK/run/RESTART.in" 120
cd "$WORK/run"
cp "PARAM.in_extreme_restart" PARAM.in
if ! mpiexec -n "$SAB_RANKS" --oversubscribe ./SWMF.exe > runlog_restart 2>&1; then
  echo "run.sh: the restart run of SWMF.exe failed; last lines of its log follow" >&2
  tail -40 runlog_restart >&2
  exit 1
fi
./PostProc.pl -noptec > postproc.log 2>&1

# The graded files, under the fixed names rubric.json lists. Each name takes the
# last output of its series, which is the last write of the graded run.
grab() {
  local dest="$1" last="" f; shift
  for f in "$@"; do [ -e "$f" ] && last="$f"; done
  [ -n "$last" ] || { echo "run.sh: no output file matched: $*" >&2; exit 1; }
  cp "$last" "$OUT_DIR/$dest"
}
grab log.log                GM/IO2/log_e*.log
grab magnetometers.mag      GM/IO2/magnetometers_e*.mag
grab geoindex.log           GM/IO2/geoindex_e*.log
grab ie.log                 IE/ionosphere/IE_t*.log
grab ionosphere.idl         IE/ionosphere/it*.idl
grab superindex.log         GM/IO2/superindex_e*.log
grab mag_grid_global.out    GM/IO2/mag_grid_global_e*.out
