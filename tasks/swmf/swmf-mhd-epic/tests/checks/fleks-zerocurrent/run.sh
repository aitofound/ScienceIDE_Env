#!/usr/bin/env bash
# Check fleks-zerocurrent: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_STOP_SCALE "1" "multiplies the positive iteration count and simulated end time of the deck's #STOP block (upstream: the zero-current configuration to t = 6.25); run time scales with it"
knob SAB_MPI_RANKS "1" "MPI ranks for FLEKS.exe (the shipped runner PC/FLEKS/tests/validate_tests.py runs it serially unless -n is given); the graded numbers depend on the rank count through the order of the MPI reductions, so this is a knob for iteration only"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the build of the pinned source (default: the CPUs allowed to this container); it changes build time only, never the graded run"
# Alternative build, OPTIONAL: the framework's own ./Config.pl -O0 rewrites every OPTn line of
# Makefile.conf to -O0 where the shipped gfortran template (share/build/Makefile.Linux.gfortran)
# builds at -O3. PC/FLEKS/Makefile.conf includes that file and its C++ rule compiles with ${OPT3},
# so the whole FLEKS solver is rebuilt at -O0 -- a legitimately different build of the same source.
ALTBUILD="the same standalone FLEKS configuration built with ./Config.pl -O0 at the SWMF root before make EXE, which sets every OPTn level of Makefile.conf to -O0 where the shipped gfortran template uses -O3 (PC/FLEKS/Makefile.conf includes it and its C++ rule compiles with OPT3); same pinned source, same deck"
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
exec < /dev/null
WORK="$(mktemp -d)"
cleanup() { local rc=$?; if [ "$rc" -ne 0 ]; then for f in "$WORK"/*.log; do [ -f "$f" ] || continue; echo "== $f" >&2; tail -30 "$f" >&2; done; fi; rm -rf "$WORK"; }
trap cleanup EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"
export LC_ALL=C OMP_NUM_THREADS=1 GIT_TERMINAL_PROMPT=0

cd "$WORK/src"
BUILD_START=$(date +%s)
./Config.pl -install=BATSRUS -compiler=gfortran > "$WORK/install.log" 2>&1
# AMReX, 3-dimensional, exactly the configure line share/Scripts/Config.pl would run (its own
# build hard-codes make -j 4; SAB_MAKE_JOBS is used here instead). ./Config.pl -amrex3d then only
# points util/AMREX/InstallDir at it and writes the AMREX definitions into Makefile.conf.
( cd util/AMREX \
  && ./configure --prefix InstallDir3D --comp gnu --enable-fortran-api no --debug no \
       --enable-tiny-profile yes --dim 3 --allow-different-compiler yes \
  && make -j"$SAB_MAKE_JOBS" && make install ) > "$WORK/amrex.log" 2>&1
./Config.pl -amrex3d > "$WORK/build.log" 2>&1
if [ "$IC" = altbuild ]; then
  ./Config.pl -O0 >> "$WORK/build.log" 2>&1
  grep -q '^OPT3 = -O0' Makefile.conf || { echo "run.sh: Config.pl -O0 did not set OPT3 in Makefile.conf" >&2; exit 1; }
fi
# The build PC/FLEKS/tests/README.md documents for the standalone suite: two grid levels (the AMR
# decks need them) and the Exosphere user source (the ionization decks need it).
cd "$WORK/src/PC/FLEKS"
./Config.pl -amrex3d -lev=2 -u=Exo >> "$WORK/build.log" 2>&1
make -j"$SAB_MAKE_JOBS" EXE >> "$WORK/build.log" 2>&1
make PIDL >> "$WORK/build.log" 2>&1
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"

# The run directory PC/FLEKS/Makefile builds for the standalone executable, which is what
# tests/validate_tests.py assembles by hand before every test.
make rundir RUNDIR="$WORK/run" > "$WORK/rundir.log" 2>&1
# PC/FLEKS/Makefile links FLEKS.exe from ${BINDIR}, which in the SWMF layout is the framework's
# bin/; make EXE puts the standalone executable in PC/FLEKS/bin/, so link it here.
ln -sf "$WORK/src/PC/FLEKS/bin/FLEKS.exe" "$WORK/run/FLEKS.exe"
python3 - "$CHECK_DIR/ic/$INPUTS/PARAM.in" "$WORK/run/PARAM.in" "$SAB_STOP_SCALE" <<'PY'
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
            if value <= 0:
                continue
            parts = lines[k].split(None, 1)
            tail = "\t\t\t" + parts[1] if len(parts) > 1 else ""
            lines[k] = (("%d" % max(1, int(round(value * scale)))) if integer else ("%.10g" % (value * scale))) + tail
open(dst, "w", encoding="utf-8").write("\n".join(lines))
PY

( cd "$WORK/run" && mpiexec -n "$SAB_MPI_RANKS" --oversubscribe ./FLEKS.exe > runlog 2>&1 ) || {
  echo "run.sh: FLEKS.exe failed; last lines of the run log follow" >&2
  tail -40 "$WORK/run/runlog" >&2
  exit 1
}
( cd "$WORK/run" && ./PostProc.pl RESULTS > postproc.log 2>&1 )

grab() {
  local dest="$1" last="" f; shift
  for f in "$@"; do [ -e "$f" ] && last="$f"; done
  [ -n "$last" ] || { echo "run.sh: no output file matched: $*" >&2; exit 1; }
  case "$last" in
    *.gz) gunzip -c "$last" > "$OUT_DIR/$dest" ;;
    *) cp "$last" "$OUT_DIR/$dest" ;;
  esac
}
cd "$WORK/run"
grab pc_cut.out RESULTS/PC/*=0*.out
grab pc_energy.log RESULTS/PC/log_pic_n*.log
