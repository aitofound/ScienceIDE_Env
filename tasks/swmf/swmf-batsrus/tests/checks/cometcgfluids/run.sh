#!/usr/bin/env bash
# Check cometcgfluids: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     OPTIONAL: the nominal inputs on the alternative build (ALTBUILD below)
#   run.sh --help                       list the runtime knobs below, and the altbuild line when one is declared
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_STEP_SCALE=0.25 sab.py task selfcheck ...
# Parallel build jobs default to the CPUs this container may use (cgroup v2 cpu.max), not the host count.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_STEP_SCALE "1" "multiplies the iteration limit of every #STOP block (the upstream windows are 80, 100, 150 and 180 iterations of the four sessions); run time scales close to linearly and the graded plot frames are always the last ones the run wrote"
knob SAB_TIME_SCALE "1" "multiplies the positive tSimulationMax of every #STOP block (this deck sets -1.0 everywhere, so the default 1 is a no-op); kept so every check of this task takes the same two window knobs"
knob SAB_MPI_RANKS "2" "MPI ranks BATSRUS.exe runs on (upstream test: 2); BATSRUS is rank-count independent to about 1e-12 on this class of problem, so this only changes the run time"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the build of the pinned source (default: the CPUs allowed to this container); each job needs about 0.3 GB"
# Alternative build, OPTIONAL: BATSRUS's own optimisation switch (share/Scripts/Config.pl
# set_optimization_ rewrites every OPTn line of the copied tree's Makefile.conf to -O0; the
# shipped gfortran template builds at OPT3 = -O3). Same pinned source, same deck.
ALTBUILD="the same Config.pl configuration built with ./Config.pl -O0 before make BATSRUS, which sets every OPTn level of Makefile.conf to -O0 where the shipped gfortran template uses -O3; same pinned source, same deck"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
export LC_ALL=C
exec < /dev/null    # nothing here reads stdin, and mpiexec would otherwise drain the driver's check list
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"
if [ "$IC" = altbuild ]; then
  [ -n "$ALTBUILD" ] || { echo "run.sh: this check declares no alternative build" >&2; exit 2; }
  INPUTS=nominal
fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
# The pinned source is the SWMF tree. The upstream standalone BATSRUS tests run in
# the standalone layout: GM/BATSRUS with share/ and util/ beside it, which is exactly
# what BATSRUS's own Config.pl -install clones into its root. Assemble that layout from
# the pinned tree here; SOURCE_DIR itself is never modified.
cp -R "$SOURCE_DIR/GM/BATSRUS/." "$WORK/src"
cp -R "$SOURCE_DIR/share" "$WORK/src/share"
cp -R "$SOURCE_DIR/util" "$WORK/src/util"
cd "$WORK/src"

# The deck this run executes, taken from ic/<IC>/ and staged under Param/SAB/ of
# the copied source tree so that BATSRUS reads it the way `make rundir` expects.
# SAB_STEP_SCALE and SAB_TIME_SCALE rewrite the #STOP blocks; with the defaults
# (1) the deck is used byte for byte.
mkdir -p Param/SAB
cp "$CHECK_DIR"/ic/"$INPUTS"/PARAM.in Param/SAB/PARAM.in
if [ "$SAB_TIME_SCALE" != 1 ] || [ "$SAB_STEP_SCALE" != 1 ]; then
  python3 - Param/SAB/PARAM.in "$SAB_TIME_SCALE" "$SAB_STEP_SCALE" <<'PY'
import re, sys
path, ts, ss = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
lines = open(path).read().splitlines(True)
def scale(line, factor):
    m = re.match(r"^(\s*)(\S+)(\s.*)?$", line.rstrip("\n"))
    if not m:
        return line
    try:
        number = float(m.group(2))
    except ValueError:
        return line
    if number <= 0:                      # -1 means "not used"; 0 means "no steps"
        return line
    return "%s%.10g%s\n" % (m.group(1), number * factor, m.group(3) or "")
for i, line in enumerate(lines):
    if line.startswith("#STOP"):
        lines[i + 1] = scale(lines[i + 1], ss)
        lines[i + 2] = scale(lines[i + 2], ts)
open(path, "w").writelines(lines)
PY
fi

# Upstream test this check reproduces: code/swmf/GM/BATSRUS/Param/ROSETTA/PARAM.in.fluids.all  (Makefile.test target test_cometCGfluids)
# Build: Config.pl -install -compiler=gfortran, then ./Config.pl -u=CometCGfluids -e=CometCG3FluidsPe -ng=2 -g=4,4,4 followed by ./Config.pl -default, then make BATSRUS and make PIDL.
# Every check of this task carries its own build because every official BATSRUS
# test sets its own compile-time equation set, user module and block size.
BUILD_START=$(date +%s)
./Config.pl -install -compiler=gfortran > "$WORK/install.log" 2>&1
./Config.pl -u=CometCGfluids -e=CometCG3FluidsPe -ng=2 -g=4,4,4 > "$WORK/config.log" 2>&1
./Config.pl -default >> "$WORK/config.log" 2>&1
if [ "$IC" = altbuild ]; then
  ./Config.pl -O0 >> "$WORK/config.log" 2>&1
  grep -q '^OPT3 = -O0' Makefile.conf || { echo "run.sh: Config.pl -O0 did not set OPT3 in Makefile.conf" >&2; exit 1; }
fi
make -j"$SAB_MAKE_JOBS" BATSRUS > "$WORK/make.log" 2>&1
make PIDL >> "$WORK/make.log" 2>&1
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only

make rundir RUNDIR=run_test STANDALONE=YES GMDIR="$WORK/src" > "$WORK/rundir.log" 2>&1
cp Param/SAB/PARAM.in run_test/PARAM.in
cp Param/ROSETTA/CG_MOC.bdf.gz run_test/
( cd run_test && gunzip -f CG_MOC.bdf.gz )   # the triangulated 67P nucleus the user module reads
( cd run_test && mpiexec --oversubscribe --bind-to none -n "$SAB_MPI_RANKS" ./BATSRUS.exe < /dev/null > runlog 2>&1 ) || { echo "run.sh: BATSRUS.exe failed on PARAM.in" >&2; tail -40 run_test/runlog >&2; exit 1; }
grep -q "Finished Numerical Simulation" run_test/runlog || { echo "run.sh: BATSRUS.exe did not finish the run" >&2; tail -40 run_test/runlog >&2; exit 1; }
( cd run_test && ./PostProc.pl -m -replace RESULTS < /dev/null >> "$WORK/postproc.log" 2>&1 )

# Copy the last frame of one plot series (or the log) into OUT_DIR under a fixed
# name, so the graded file list does not depend on the knobs above.
copy_last() {
  local dir="$1" pattern="$2" name="$3" file
  file="$(ls -1 "$dir"/$pattern 2>/dev/null | LC_ALL=C sort | tail -1)"
  [ -n "$file" ] || { echo "run.sh: no output matching $dir/$pattern" >&2; exit 1; }
  cp "$file" "$OUT_DIR/$name"
}

copy_last run_test/RESULTS/GM 'log_n*.log' log.log

# The frame rule does not apply: this check grades only log.log (the same file
# upstream grades), not a plot series; log rows are written every step so the
# 180-step (SAB_STEP_SCALE-scaled) window always yields far more than 5 of them.
echo "SAB_PLOT_FRAMES=exempt"
