#!/usr/bin/env bash
# Check srblast-spherical: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# Official test: Phantom SETUP=srblast (build/Makefile_setups:677; src/setup/setup_srblast.f90), run the way
# scripts/buildbot.sh check_phantomsetup runs a setup -- phantomsetup with default answers
# (40 blank lines on stdin) and then phantom -- except that the run evolves for a short window
# instead of stopping at nmax=0, so the graded file is an evolved full dump.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_NMAX=5 sab.py task selfcheck ...
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_TMAX "0.050" "tmax of the .in in code units (the official deck integrates to 0.200); runtime scales linearly; the default stops after 10 of the official 40 dumps"
knob SAB_DTMAX "0.005" "dtmax of the .in, the time between dumps (official value; SAB_TMAX/SAB_DTMAX dumps are written)"
knob SAB_NPARTX "40" "npartx of the .setup: particles across the periodic box (the official value); the particle count scales as npartx^3"
knob SAB_NMAX "-1" "cap on the number of time steps (nmax in the .in); -1 runs to SAB_TMAX (graded); a small cap exercises build, setup, run and output only"
knob SAB_THREADS "2" "OMP_NUM_THREADS for phantom; the graded default (88 thousand particles do scale: 71.5 s at 1 thread against 34.1 s at 2)"
# Alternative build: Phantom's own gfortran DEBUG=yes build replaces -O3 with -O0 and enables its runtime checks.
# `run.sh altbuild` uses the nominal inputs; selfcheck measures the floor from the second legitimate build.
ALTBUILD="make SYSTEM=gfortran OPENMP=yes DEBUG=yes: the same pinned source with Phantom's own -O0 gfortran debug build (bounds, NaN and floating-point checks) instead of the nominal -O3 build"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"; MAKE_EXTRA=()
if [ "$IC" = altbuild ]; then INPUTS=nominal; MAKE_EXTRA=(SYSTEM=gfortran OPENMP=yes DEBUG=yes); fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
SRC="$WORK/src"; RUN="$WORK/run"
mkdir -p "$SRC" "$RUN"
cp -R "$SOURCE_DIR/." "$SRC"

# Build phantom and phantomsetup for this SETUP. Parallel make is broken upstream
# (build/.depends is empty), so the build is serial and one goal per invocation.
export SYSTEM=gfortran OMP_NUM_THREADS="$SAB_THREADS" OMP_STACKSIZE=512M
BUILD_START=$(date +%s)
if ! (cd "$SRC" && make ${MAKE_EXTRA[@]+"${MAKE_EXTRA[@]}"} SETUP=srblast phantom >"$WORK/make.log" 2>&1 && make ${MAKE_EXTRA[@]+"${MAKE_EXTRA[@]}"} SETUP=srblast setup >>"$WORK/make.log" 2>&1); then
  echo "run.sh: build failed" >&2; tail -n 40 "$WORK/make.log" >&2; exit 1
fi
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only

# The initial condition: this check's myrun.setup from ic/<ic>/, with the resolution knob
# applied, then phantomsetup. phantomsetup is a two-pass program (a call without a .setup only
# writes the .setup and stops); with the .setup supplied it reads the remaining prompts from
# stdin. It is run at ONE thread whatever SAB_THREADS is: the setup stage is the only part of
# Phantom whose output depends on the thread count (relaxation and OpenMP reductions), and the
# graded initial condition must not.
cp -R "$CHECK_DIR/ic/$INPUTS/." "$RUN/"
cd "$RUN"
# The resolution knob is written into the .setup before phantomsetup reads it.
python3 - myrun.setup npartx "$SAB_NPARTX" <<'PY'
import re, sys
path, key, value = sys.argv[1:]
text = open(path, encoding="ascii", errors="replace").read()
pat = re.compile(r"^(\s*%s\s*=\s*)\S+" % re.escape(key), re.M)
if not pat.search(text): sys.exit("run.sh: no '%s =' line in the .setup" % key)
open(path, "w", encoding="ascii").write(pat.sub(lambda m: m.group(1) + value, text, count=1))
PY
yes '' | head -n 40 | env OMP_NUM_THREADS=1 "$SRC/bin/phantomsetup" myrun >setup1.log 2>&1 || true
if [ ! -f myrun.in ]; then
  yes '' | head -n 40 | env OMP_NUM_THREADS=1 "$SRC/bin/phantomsetup" myrun >setup2.log 2>&1 || true
fi
[ -f myrun.in ] || { echo "run.sh: phantomsetup wrote no myrun.in" >&2; tail -n 40 setup1.log >&2; exit 1; }
ls myrun_00000* >/dev/null 2>&1 || { echo "run.sh: phantomsetup wrote no t=0 dump" >&2; tail -n 40 setup1.log >&2; exit 1; }

# The graded run: every dump a full dump (nfulldump=1, so the graded dump carries the binary64
# arrays and not the float32 small-dump subset), the wall-clock dump limits off (dtwallmax and
# twallmax would tie the step sequence to the host), and the window from the knobs. tmax and
# dtmax are written here as literals rather than left to the values phantomsetup rounds into the
# .in, so that the two initial conditions are graded over exactly the same window and produce the
# same number of dumps.
python3 - myrun.in "$SAB_TMAX" "$SAB_DTMAX" "$SAB_NMAX" <<'PY'
import re, sys
path, tmax, dtmax, nmax = sys.argv[1:]
text = open(path, encoding="ascii", errors="replace").read()
def setkey(text, key, value):
    pat = re.compile(r"^(\s*%s\s*=\s*)\S+" % re.escape(key), re.M)
    if not pat.search(text): sys.exit("run.sh: no '%s =' line in the .in" % key)
    return pat.sub(lambda m: m.group(1) + value, text, count=1)
text = setkey(text, "tmax", tmax)
text = setkey(text, "dtmax", dtmax)
text = setkey(text, "nfulldump", "1")
text = setkey(text, "dtwallmax", "000:00")
text = setkey(text, "twallmax", "000:00")
if int(nmax) >= 0: text = setkey(text, "nmax", nmax)
open(path, "w", encoding="ascii").write(text)
PY

if ! OMP_NUM_THREADS="$SAB_THREADS" "$SRC/bin/phantom" myrun.in >phantom.log 2>&1; then
  echo "run.sh: phantom failed" >&2; tail -n 60 phantom.log >&2; exit 1
fi

# Graded file, named as rubric.json describes it: the last full dump, and nothing else. OUT_DIR
# carries the graded file alone: tests/test.sh compares every file it finds there, so a wall-clock
# log or the OpenMP-reduction sums of the .ev file would make its byte-identical safeguard inert.
# myrun01.ev and phantom.log stay in the work directory; their tails go to stdout, which the driver
# keeps in run.log, outside the comparison.
last="$(ls myrun_[0-9][0-9][0-9][0-9][0-9] 2>/dev/null | tail -n 1)"
[ -n "$last" ] || { echo "run.sh: no dump written" >&2; tail -n 40 phantom.log >&2; exit 1; }
cp "$last" "$OUT_DIR/final_dump"
echo "run.sh: graded dump $last (ungraded, not copied: myrun01.ev, phantom.log; tails follow)"
tail -n 5 myrun01.ev 2>/dev/null || true
tail -n 20 phantom.log
