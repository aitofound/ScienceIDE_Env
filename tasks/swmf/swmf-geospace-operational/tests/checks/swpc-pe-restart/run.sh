#!/usr/bin/env bash
# Check swpc-pe-restart: the TEST half of the check.
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
knob SAB_STEADY_SCALE "1.0" "multiplies the MaxIter of every steady-state #STOP block of the deck (the upstream test's own reduction of the deck's 700 and 1500 to 70 and 200 is already applied; the graded default preserves those 70 and 200 cumulative iterations)"
knob SAB_STOP_SCALE "1.0" "multiplies the first run's 120 s time-accurate window and the restart's approved 60 s continuation; the graded default preserves t=120 s then t=180 s absolute; SAB_COUPLE_MAX=5.0 caps every positive DtCouple in both copied decks, preserving the existing 5 s GM-IE period; run time scales with it"
knob SAB_COUPLE_MAX "5.0" "caps every positive DtCouple in the copied deck at 5.0 s; this coordinated upstream-clock setting preserves the existing 5 s GM-IE period throughout the full-window run; run time scales with it"
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
BUILD_CONFIG="mhdpe-ng2-ie181-pidl-interpolate"
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

# Upstream test this check reproduces: make test_swpc_pe (Makefile.test target test_swpc_pe, restart stage)
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
  ./Config.pl -o=GM:u=Default,e=MhdPe,ng=2,g=8,8,8,IE:g=181,361 >> "$WORK/build.log" 2>&1
  if [ "$IC" = altbuild ]; then
    ./Config.pl -O0 >> "$WORK/build.log" 2>&1
    grep -q '^OPT3 = -O0' Makefile.conf || { echo "run.sh: Config.pl -O0 did not set OPT3 in Makefile.conf" >&2; exit 1; }
  fi
  make -j"$SAB_MAKE_JOBS" SWMF >> "$WORK/build.log" 2>&1
  make PIDL INTERPOLATE >> "$WORK/build.log" 2>&1
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
Scripts/TestParam.pl -F "$WORK/run/PARAM.in_pe_init" > "$WORK/testparam.log" 2>&1 || true
perl -pi -e 'if(/MaxIter|MaxBlock/){s/700/70/; s/1500/200/; s/5000/350/}; s/#BORIS/BORIS/; s/^\#(COMPONENTMAP.*production)/$1/i; s/^(COMPONENTMAP.*nightly)/\#$1/i' "$WORK/run/PARAM.in_pe_init"
if [ "$SAB_STOP_SCALE" != "1.0" ] && [ "$SAB_STOP_SCALE" != "1" ]; then perl -pi -e 's/^1 min(\s+DtOutput)/3$1/; s/^1 min(\s+DtSaveMagGrid)/3$1/; s/^20(\s+DtSaveMagGrid)/3$1/; s/^20(\s+DtOutput)/3$1/' "$WORK/run/PARAM.in_pe_init"; fi
if [ "$SAB_STOP_SCALE" != "1.0" ] && [ "$SAB_STOP_SCALE" != "1" ]; then perl -0777 -pi -e 's/(min idl\s+StringPlot\n)100(\s+DnSavePlot)/${1}5$2/' "$WORK/run/PARAM.in_pe_init"; fi
perl -pi -e 's/^360(\s+nGridLon)/72$1/; s/^171(\s+nGridLat)/25$1/;' "$WORK/run/PARAM.in_pe_init"
perl -pi -e 's/^221(\s+nGridLon)/23$1/; s/^131(\s+nGridLat)/13$1/;' "$WORK/run/PARAM.in_pe_init"
# The knobs rescale the steady-state iteration counts and the time-accurate
# window, then cap positive component coupling clocks; the graded defaults keep
# the approved t=120 s initial run and t=180 s absolute restart endpoint.
python3 "$WORK/knobs.py" "$WORK/run/PARAM.in_pe_init" "$SAB_STEADY_SCALE" "$SAB_STOP_SCALE"
cd "$WORK/run"
cp "PARAM.in_pe_init" PARAM.in
ln -s "$SRC/bin/INTERPOLATE.exe" "$WORK/run/"
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
RESTART_DIRS=(SWMF_RESTART.*)
if [ "${#RESTART_DIRS[@]}" -eq 1 ] && [ -d "${RESTART_DIRS[0]}" ]; then
  ./Restart.pl -i "${RESTART_DIRS[0]}" >> runlog 2>&1
elif [ "${#RESTART_DIRS[@]}" -gt 1 ]; then
  echo "run.sh: multiple restart trees; refusing arbitrary selection" >&2; exit 1
elif [ -f RESTART.out ]; then
  ./Restart.pl >> runlog 2>&1
else
  echo "run.sh: the first run left no unique restart state to restart from" >&2; exit 1
fi
cd "$SRC"
Scripts/TestParam.pl -F "$WORK/run/PARAM.in_pe_restart" >> "$WORK/testparam.log" 2>&1 || true
perl -pi -e 's/#BORIS/BORIS/; s/^\#(COMPONENTMAP.*production)/$1/i; s/^(COMPONENTMAP.*nightly)/\#$1/i' "$WORK/run/PARAM.in_pe_restart"
if [ "$SAB_STOP_SCALE" != "1.0" ] && [ "$SAB_STOP_SCALE" != "1" ]; then perl -pi -e 's/^1 min(\s+DtOutput)/3$1/; s/^1 min(\s+DtSaveMagGrid)/3$1/; s/^20(\s+DtSaveMagGrid)/3$1/; s/^20(\s+DtOutput)/3$1/' "$WORK/run/PARAM.in_pe_restart"; fi
if [ "$SAB_STOP_SCALE" != "1.0" ] && [ "$SAB_STOP_SCALE" != "1" ]; then perl -0777 -pi -e 's/(aur idl\s+StringPlot\n-1\s+DnSavePlot\n)1 min(\s+DtSavePlot)/${1}3$2/' "$WORK/run/PARAM.in_pe_restart"; fi
perl -pi -e 's/^360(\s+nGridLon)/72$1/; s/^171(\s+nGridLat)/25$1/;' "$WORK/run/PARAM.in_pe_restart"
perl -pi -e 's/^221(\s+nGridLon)/23$1/; s/^131(\s+nGridLat)/13$1/;' "$WORK/run/PARAM.in_pe_restart"
python3 "$WORK/knobs.py" "$WORK/run/PARAM.in_pe_restart" 1 "$SAB_STOP_SCALE" "$WORK/run/RESTART.in" 60
cd "$WORK/run"
cp "PARAM.in_pe_restart" PARAM.in
if ! mpiexec -n "$SAB_RANKS" --oversubscribe ./SWMF.exe > runlog_restart 2>&1; then
  echo "run.sh: the restart run of SWMF.exe failed; last lines of its log follow" >&2
  tail -40 runlog_restart >&2
  exit 1
fi
./PostProc.pl -noptec > postproc.log 2>&1
# Producer-aligned restart capture. The persistent files are opened at the
# restart start (the first deck's actual end), while event products use the
# second deck's absolute end and its restart-relative stage duration.
shopt -s nullglob
read -r CAP_PERSIST_START CAP_PERSIST_START_SHORT CAP_END CAP_END_SHORT CAP_STAGE_SECONDS < <(python3 - "$WORK/run/PARAM.in_pe_init" "$WORK/run/PARAM.in_pe_restart" <<'PY'
import datetime, sys
from pathlib import Path

def end_stamp(path):
    lines = Path(path).read_text(encoding="utf-8").splitlines()
    i = next(i for i, line in enumerate(lines) if line.strip() == "#ENDTIME")
    vals = [int(float(lines[i + j].split()[0])) for j in range(1, 7)]
    return datetime.datetime(*vals)
start = end_stamp(sys.argv[1])
end = end_stamp(sys.argv[2])
print(start.strftime("%Y%m%d-%H%M%S"), start.strftime("%y%m%d_%H%M%S"),
      end.strftime("%Y%m%d-%H%M%S"), end.strftime("%y%m%d_%H%M%S"),
      int((end - start).total_seconds()))
PY
)
CAP_OPEN_DATE="$CAP_PERSIST_START"
CAP_OPEN_SHORT="$CAP_PERSIST_START_SHORT"
fail_capture() { echo "run.sh: producer-aligned capture: $*" >&2; exit 1; }

check_grid_header() {
  local source="$1"
  [[ "$(basename "$source")" == *"_e${CAP_END}.out" ]] || fail_capture "grid date does not match restart deck endpoint: $source (expected $CAP_END)"
  python3 - "$source" "$CAP_STAGE_SECONDS" <<'PY'
import math, sys
from pathlib import Path
path, expected = sys.argv[1], float(sys.argv[2])
lines = [line.strip() for line in Path(path).read_text(encoding="utf-8", errors="replace").splitlines() if line.strip()]
if len(lines) < 3 or not lines[0].startswith("Magnetometer grid"):
    raise SystemExit(f"{path}: not a producer magnetometer-grid header")
try:
    actual = float(lines[1].split()[1].replace("D", "E").replace("d", "e"))
except (IndexError, ValueError):
    raise SystemExit(f"{path}: missing grid TimeIn header")
if not math.isclose(actual, expected, rel_tol=0.0, abs_tol=1e-9):
    raise SystemExit(f"{path}: TimeIn={actual:g}, expected restart-relative {expected:g}")
PY
}
filter_endpoint_grids() {
  local source
  GRID_FILES=()
  # Filter by the deck-derived absolute endpoint before enforcing uniqueness;
  # earlier cadence frames are valid retained products, not ambiguity.
  for source in "$@"; do
    [[ "$(basename "$source")" == *"_e${CAP_END}.out" ]] && GRID_FILES+=("$source")
  done
}
filter_endpoint_grids GM/IO2/mag_grid_global_e*.out
[ "${#GRID_FILES[@]}" -eq 1 ] || fail_capture "expected one global endpoint grid at ${CAP_END}, found ${#GRID_FILES[@]}"
GRIDFILE="${GRID_FILES[0]}"
check_grid_header "$GRIDFILE"
# INTERPOLATE.exe interpolates the magnetometer grid onto the stations of
# magin_GEM.dat, as the upstream restart recipe does; its input file names the
# grid file, so line 1 is rewritten with the grid this run actually wrote.
{ echo "$GRIDFILE"; tail -n +2 INTERPOLATE.in; } > INTERPOLATE.in.run
./INTERPOLATE.exe < INTERPOLATE.in.run > interpolate.log 2>&1

pick_persistent() {
  local dest="$1" family="$2"; shift 2
  local candidates=() source expected
  case "$family" in
    log) expected="log_e${CAP_OPEN_DATE}.log" ;;
    magnetometers) expected="magnetometers_e${CAP_OPEN_DATE}.mag" ;;
    geoindex) expected="geoindex_e${CAP_OPEN_DATE}.log" ;;
    superindex) expected="superindex_e${CAP_OPEN_DATE}.log" ;;
    ie) expected="IE_t${CAP_OPEN_SHORT}.log" ;;
    *) fail_capture "unknown persistent family $family" ;;
  esac
  # Filter by the producer's opening period before enforcing uniqueness. The
  # initial-stage files remain in this shared WORK and must not be deleted.
  for source in "$@"; do
    [[ "$(basename "$source")" == "$expected" ]] && candidates+=("$source")
  done
  [ "${#candidates[@]}" -eq 1 ] || fail_capture "expected one fresh $family file at opening period ${CAP_OPEN_DATE}, found ${#candidates[@]}"
  source="${candidates[0]}"
  case "$family" in
    log) [[ "$(basename "$source")" =~ ^log_e[0-9]{8}-[0-9]{6}\.log$ ]] || fail_capture "wrong log producer name: $source" ;;
    magnetometers) [[ "$(basename "$source")" =~ ^magnetometers_e[0-9]{8}-[0-9]{6}\.mag$ ]] || fail_capture "wrong magnetometer producer name: $source" ;;
    geoindex) [[ "$(basename "$source")" =~ ^geoindex_e[0-9]{8}-[0-9]{6}\.log$ ]] || fail_capture "wrong geoindex producer name: $source" ;;
    superindex) [[ "$(basename "$source")" =~ ^superindex_e[0-9]{8}-[0-9]{6}\.log$ ]] || fail_capture "wrong superindex producer name: $source" ;;
    ie) [[ "$(basename "$source")" =~ ^IE_t[0-9]{6}_[0-9]{6}\.log$ ]] || fail_capture "unsupported IE producer name: $source" ;;
  esac
  case "$family" in
    log) [[ "$(basename "$source")" == "log_e${CAP_OPEN_DATE}.log" ]] || fail_capture "log open date does not match restart: $source" ;;
    magnetometers) [[ "$(basename "$source")" == "magnetometers_e${CAP_OPEN_DATE}.mag" ]] || fail_capture "magnetometer open date does not match restart: $source" ;;
    geoindex) [[ "$(basename "$source")" == "geoindex_e${CAP_OPEN_DATE}.log" ]] || fail_capture "geoindex open date does not match restart: $source" ;;
    superindex) [[ "$(basename "$source")" == "superindex_e${CAP_OPEN_DATE}.log" ]] || fail_capture "superindex open date does not match restart: $source" ;;
    ie) [[ "$(basename "$source")" == "IE_t${CAP_OPEN_SHORT}.log" ]] || fail_capture "IE open date does not match restart: $source" ;;
  esac
  cp "$source" "$OUT_DIR/$dest"
}
check_persistent_dates() {
  local path="$1" family="$2"; shift 2
  python3 - "$path" "$family" "$CAP_END" "$CAP_STAGE_SECONDS" "$@" <<'PY'
import datetime, sys
from pathlib import Path
path, family, end_token, stage_seconds = sys.argv[1:5]
required = sys.argv[5:]
end = datetime.datetime.strptime(end_token, "%Y%m%d-%H%M%S")
required_dates = {datetime.datetime.strptime(x, "%Y%m%d-%H%M%S") for x in required}
found = []
for line in Path(path).read_text(encoding="utf-8", errors="replace").splitlines():
    t = line.split()
    if len(t) < 7:
        continue
    try:
        vals = tuple(int(t[i]) for i in range(1, 7))
        if not 1900 <= vals[0] <= 2200:
            continue
        found.append(datetime.datetime(*vals))
    except (ValueError, IndexError):
        continue
if not found:
    raise SystemExit(f"{path}: no embedded producer date rows")
missing = sorted(required_dates - set(found))
if missing:
    raise SystemExit(f"{path}: missing embedded dates {missing}")
if family == "ie":
    # Its 5-second schedule legitimately ends at 00:02:55, not 00:03:00.
    if any(value > end for value in found):
        raise SystemExit(f"{path}: IE row extends beyond restart endpoint {end}")
elif max(found) > end:
    raise SystemExit(f"{path}: persistent row extends beyond restart endpoint {end}")
PY
}
pick_persistent log.log log GM/IO2/log_e*.log
pick_persistent magnetometers.mag magnetometers GM/IO2/magnetometers_e*.mag
pick_persistent geoindex.log geoindex GM/IO2/geoindex_e*.log
pick_persistent ie.log ie IE/ionosphere/IE_t*.log
pick_persistent superindex.log superindex GM/IO2/superindex_e*.log
check_persistent_dates "$OUT_DIR/log.log" log "$CAP_PERSIST_START" "$CAP_END"
check_persistent_dates "$OUT_DIR/magnetometers.mag" magnetometers "$CAP_PERSIST_START" "$CAP_END"
check_persistent_dates "$OUT_DIR/geoindex.log" geoindex "$CAP_PERSIST_START" "$CAP_END"
check_persistent_dates "$OUT_DIR/ie.log" ie "$CAP_PERSIST_START"
check_persistent_dates "$OUT_DIR/superindex.log" superindex "$CAP_PERSIST_START" "$CAP_END"

pick_merged_idl() {
  local candidates=(IE/ionosphere/it"${CAP_END_SHORT}"_*.idl)
  local merged=() f
  for f in "${candidates[@]}"; do
    [[ "$(basename "$f")" =~ ^it${CAP_END_SHORT}_[0-9]{3}\.idl$ ]] && merged+=("$f")
  done
  [ "${#merged[@]}" -eq 1 ] || fail_capture "expected one merged IDL at ${CAP_END_SHORT}, found ${#merged[@]}"
  local source="${merged[0]}"
  python3 - "$source" "$CAP_END" "$CAP_STAGE_SECONDS" <<'PY'
import datetime, math, re, sys
from pathlib import Path
path, end_token, expected = sys.argv[1], sys.argv[2], float(sys.argv[3])
text = Path(path).read_text(encoding="utf-8", errors="replace")
expected_title = datetime.datetime.strptime(end_token, "%Y%m%d-%H%M%S").strftime("%Y-%m-%d-%H-%M-%S")
if expected_title not in "\n".join(text.splitlines()[:24]):
    raise SystemExit(f"{path}: TITLE does not carry absolute restart endpoint {expected_title}")
blocks = re.findall(r"^BEGIN\s+(\S+)\s+HEMISPHERE\s*$", text, re.M)
if sorted(blocks) != ["NORTHERN", "SOUTHERN"]:
    raise SystemExit(f"{path}: merged IDL must contain both hemispheres, got {blocks}")
matches = re.findall(r"^\s*([+-.0-9EeDd]+)\s+Time_Simulation\s*$", text, re.M)
if len(matches) != 1 or not math.isclose(float(matches[0].replace("D", "E").replace("d", "e")), expected, rel_tol=0.0, abs_tol=1e-9):
    raise SystemExit(f"{path}: IDL Time_Simulation does not match restart-relative {expected:g}")
PY
  cp "$source" "$OUT_DIR/ionosphere.idl"
}
pick_merged_idl
filter_endpoint_grids GM/IO2/mag_grid_global_e*.out
[ "${#GRID_FILES[@]}" -eq 1 ] || fail_capture "global endpoint grid became ambiguous after PostProc"
pick_grid_us=()
for source in GM/IO2/mag_grid_us_e*.out; do
  [[ "$(basename "$source")" == *"_e${CAP_END}.out" ]] && pick_grid_us+=("$source")
done
[ "${#pick_grid_us[@]}" -eq 1 ] || fail_capture "expected one US endpoint grid at ${CAP_END}, found ${#pick_grid_us[@]}"
check_grid_header "${pick_grid_us[0]}"
cp "${pick_grid_us[0]}" "$OUT_DIR/mag_grid_us.out"
cp "$GRIDFILE" "$OUT_DIR/mag_grid_global.out"
[ -f GM/IO2/ABK.txt ] || fail_capture "missing producer station ABK.txt"
cp GM/IO2/ABK.txt "$OUT_DIR/station_abk.txt"
