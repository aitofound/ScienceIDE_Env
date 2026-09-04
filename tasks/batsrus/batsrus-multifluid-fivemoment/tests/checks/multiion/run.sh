#!/usr/bin/env bash
# Check multiion: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh --help                       list the runtime knobs below
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# Official test: code/batsrus/Param/MULTIION/PARAM.in  (Makefile.test target test_multiion)
# Configuration: ./Config.pl -default -u=Default -e=MultiIonPe -ng=2 -g=64,1,1

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_TIME_SCALE=0.05 sab.py task selfcheck ...
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_TIME_SCALE "1" "multiplies every tSimulationMax of the deck (the #STOP blocks of ic/<ic>/PARAM.in); 1 is the graded window, and the number of time steps and the run time scale with it"
knob SAB_MPI_RANKS "2" "MPI ranks for BATSRUS.exe; 2 is the graded value and the rank count of the upstream Makefile.test recipe"
knob SAB_BUILD_JOBS "4" "make -j for the BATSRUS build; build time only, never the graded run time"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; exit 0; fi

set -euo pipefail
# Read nothing from the inherited standard input. The produce driver starts the
# checks from a `while read` loop over a process substitution, and mpiexec slurps
# whatever standard input it is given: without this line the first check's mpiexec
# eats the driver's list of checks and the other thirteen never start.
exec 0</dev/null
IC="${1:?usage: run.sh <nominal|variant> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
[ -d "$CHECK_DIR/ic/$IC" ] || { echo "run.sh: no initial condition ic/$IC" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
SRC="$WORK/src"; RUN="$WORK/run"
mkdir -p "$SRC"
cp -R "$SOURCE_DIR/." "$SRC/"
chmod -R u+w "$SRC"

# Build. Config.pl -install writes Makefile.conf from share/build/Makefile.<OS>.gfortran
# and then tries to clone srcUserExtra, which is access-restricted and absent from the
# pin; that attempt fails harmlessly (the run has no network) and the build goes on.
BUILD_START=$(date +%s)
cd "$SRC"
export LC_ALL=C
fail() { echo "run.sh: $1" >&2; shift; tail -n 40 "$@" >&2 || true; exit 1; }
./Config.pl -install -compiler=gfortran >"$WORK/install.log" 2>&1 || fail "Config.pl -install failed" "$WORK/install.log"
./Config.pl -default -u=Default -e=MultiIonPe -ng=2 -g=64,1,1 >"$WORK/config.log" 2>&1 || fail "Config.pl failed" "$WORK/config.log"
make -j"$SAB_BUILD_JOBS" BATSRUS >"$WORK/make.log" 2>&1 || fail "make BATSRUS failed" "$WORK/make.log"
make PIDL >>"$WORK/make.log" 2>&1 || fail "make PIDL failed" "$WORK/make.log"
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only

# Run directory, exactly as Makefile.test's test_rundir does.
make rundir RUNDIR="$RUN" STANDALONE=YES GMDIR="$SRC" >"$WORK/rundir.log" 2>&1 || fail "make rundir failed" "$WORK/rundir.log"
cp "$CHECK_DIR/ic/$IC/PARAM.in" "$RUN/PARAM.in"
if [ "$SAB_TIME_SCALE" != "1" ]; then
  python3 - "$RUN/PARAM.in" "$SAB_TIME_SCALE" <<'PY'
import sys
path, scale = sys.argv[1], float(sys.argv[2])
lines = open(path, encoding="ascii", errors="replace").read().splitlines(keepends=True)
hit = 0
i = 0
while i < len(lines):
    if lines[i].startswith("#STOP"):
        # the #STOP block is: MaxIteration, then tSimulationMax
        j, seen = i + 1, 0
        while j < len(lines) and seen < 2:
            if lines[j].strip():
                seen += 1
                if seen == 2:
                    head, sep, tail = lines[j].partition("\t")
                    lines[j] = "%.10g%s%s" % (float(head.strip()) * scale, sep or "\t\t\t", tail or "tSimulationMax\n")
                    if not lines[j].endswith("\n"):
                        lines[j] += "\n"
                    hit += 1
            j += 1
        i = j
        continue
    i += 1
if hit == 0:
    sys.exit("run.sh: SAB_TIME_SCALE: no #STOP block found in PARAM.in")
open(path, "w", encoding="ascii").write("".join(lines))
PY
fi

# Run, exactly as Makefile.test's test_<name>_run does: mpiexec then PostProc.pl.
# --bind-to none keeps concurrent checks from all landing on the same two cores;
# it changes wall time only, never the arithmetic.
cd "$RUN"
if ! mpiexec --bind-to none --oversubscribe -n "$SAB_MPI_RANKS" ./BATSRUS.exe >runlog 2>&1; then
  echo "run.sh: BATSRUS.exe failed" >&2; tail -n 60 runlog >&2; exit 1
fi
./PostProc.pl -m -replace RESULT >postproc.log 2>&1 || { echo "run.sh: PostProc.pl failed" >&2; tail -n 30 postproc.log >&2; exit 1; }

# Graded files, named as rubric.json lists them.
#   final.out     the last snapshot of the 1d__mhd_1_* plot series, written at
#                 the deck's tSimulationMax (BATSRUS shortens the last step to land
#                 on it exactly, src/ModBatsrusMethods.f90:617), in ASCII IDL
# The run's log file is deliberately not graded: BATSRUS writes it at six
# significant digits (src/ModWriteLogSatFile.f90:395, format es14.5e3), and at
# that precision a difference far below the graded bound can still cross a
# printing boundary and appear as one whole unit in the last printed place.
FINAL="$(ls -1 "RESULT/GM/1d__mhd_1_"*.out 2>/dev/null | LC_ALL=C sort | tail -n 1)"
[ -n "$FINAL" ] || { echo "run.sh: no RESULT/GM/1d__mhd_1_*.out plot file was written" >&2; ls -la RESULT/GM >&2 || true; exit 1; }
cp "$FINAL" "$OUT_DIR/final.out"
