#!/usr/bin/env bash
# Check gm-ie-pw: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_STOP_SCALE=0.25 sab.py task selfcheck ...
# Parallel build jobs default to the CPUs this container may use (cgroup v2 cpu.max), not the host count.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_STOP_SCALE "1" "multiplies the #STOP blocks of the deck (upstream: one steady GM session then a time-accurate coupled window); run time scales with it"
knob SAB_RANKS "2" "MPI ranks (upstream runs this test with mpiexec -n 2)"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the build of the pinned source (default: the CPUs allowed to this container); it changes build time only, never the graded run"
# Alternative build, OPTIONAL: the SWMF's own ./Config.pl -O0 rewrites every OPTn line of
# Makefile.conf to -O0 where the shipped gfortran template (share/build/Makefile.Linux.gfortran)
# builds at -O3 -- a legitimately different build of the same pinned source and deck.
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
cp -R "$SOURCE_DIR/." "$WORK/src"
export LC_ALL=C OMP_NUM_THREADS=1

# The initial condition is ic/nominal with ic/<IC> laid over it, so that a variant
# carries only the files it changes and the two conditions cannot drift apart in the
# files they share.
mkdir -p "$WORK/ic"
cp -R "$CHECK_DIR/ic/nominal/." "$WORK/ic/"
[ "$INPUTS" = nominal ] || cp -R "$CHECK_DIR/ic/$INPUTS/." "$WORK/ic/"
# PW/PWOM reads its input tables and its initial field-line states through the
# data/ link that Config.pl makes to SWMF_data/PW/PWOM/data. The vendored tree
# carries no SWMF_data for PW, so the check ships that data itself, under ic/,
# and puts it where the component's own rundir target expects it.
[ -d "$WORK/ic/pwdata" ] || { echo "run.sh: ic/pwdata is missing" >&2; exit 2; }
rm -rf "$WORK/src/PW/PWOM/data"
cp -R "$WORK/ic/pwdata" "$WORK/src/PW/PWOM/data"

# Upstream test this check reproduces: make test1 (Makefile.test target test1).
cd "$WORK/src"
BUILD_START=$(date +%s)
GIT_TERMINAL_PROMPT=0 ./Config.pl -install=BATSRUS -compiler=gfortran > "$WORK/install.log" 2>&1
./Config.pl -default -v=Empty,GM/BATSRUS,IE/Ridley_serial,PW/PWOM >> "$WORK/build.log" 2>&1
./Config.pl -o=GM:u=Default,e=Mhd,ng=2,g=8,8,8,PW:Earth,IE:g=91,181 >> "$WORK/build.log" 2>&1
if [ "$IC" = altbuild ]; then
  ./Config.pl -O0 >> "$WORK/build.log" 2>&1
  grep -q '^OPT3 = -O0' Makefile.conf || { echo "run.sh: Config.pl -O0 did not set OPT3 in Makefile.conf" >&2; exit 1; }
fi
make -j"$SAB_MAKE_JOBS" SWMF >> "$WORK/build.log" 2>&1
make PIDL >> "$WORK/build.log" 2>&1
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only

# Run directory exactly as the upstream test builds it.
make rundir RUNDIR="$WORK/run" > "$WORK/rundir.log" 2>&1
# The knob rescales the #STOP window; at the graded default of 1 the deck is copied through unchanged.
python3 - "$WORK/ic/PARAM.in" "$WORK/run/PARAM.in" "$SAB_STOP_SCALE" <<'PY'
import sys
src, dst, scale = sys.argv[1], sys.argv[2], float(sys.argv[3])
lines = open(src, encoding="utf-8").read().split("\n")
if scale != 1.0:
    for i, line in enumerate(list(lines)):
        if line.strip() != "#STOP":
            continue
        for k, integer in ((i + 1, True), (i + 2, False)):
            if k >= len(lines) or not lines[k].split():
                break
            try:
                value = float(lines[k].split()[0])
            except ValueError:
                break
            if value > 0:
                parts = lines[k].split(None, 1)
                tail = "\t\t\t" + parts[1] if len(parts) > 1 else ""
                new = max(1, int(round(value * scale))) if integer else value * scale
                lines[k] = (("%d" % new) if integer else ("%.10g" % new)) + tail
open(dst, "w", encoding="utf-8").write("\n".join(lines))
PY
./Scripts/TestParam.pl -F "$WORK/run/PARAM.in" > "$WORK/testparam.log" 2>&1 || true

cd "$WORK/run/PW"; rm -f log.out; rm -rf restartOUT plots; mkdir restartOUT plots
cd "$WORK/run"
if ! mpiexec -n "$SAB_RANKS" --oversubscribe ./SWMF.exe > runlog 2>&1 < /dev/null; then
  echo "run.sh: SWMF.exe failed; last lines of the run log follow" >&2
  tail -40 runlog >&2
  exit 1
fi
./PostProc.pl -M RESULTS > postproc.log 2>&1 < /dev/null

# The graded files, under the fixed names rubric.json lists.
grab() {
  local dest="$1" src="$2"
  [ -e "$src" ] || { echo "run.sh: no output file at $src" >&2; exit 1; }
  cp "$src" "$OUT_DIR/$dest"
}
last() {
  local dest="$1" found="" f; shift
  for f in "$@"; do [ -e "$f" ] && found="$f"; done
  [ -n "$found" ] || { echo "run.sh: no output file matched: $*" >&2; exit 1; }
  cp "$found" "$OUT_DIR/$dest"
}
for i in 1 2 3 4; do
  grab "plots_iline000$i.out" "RESULTS/PW/north_plots_iline000$i.out"
done
last gm_log.log RESULTS/GM/log_n*.log
last ie.log RESULTS/IE/IE*.log
last ie.idl RESULTS/IE/it*.idl
