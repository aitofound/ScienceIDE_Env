#!/usr/bin/env bash
# Check dustysedov-two-fluid: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# Official test: Phantom SETUP=dustysedov (build/Makefile_setups; src/setup/setup_dustysedov.f90), the Sedov blast wave with a second dust fluid, evolved for a short window; the graded file is the last full dump. The setup has no .setup file: its only inputs are the two prompt answers (the particle count across the box for gas and for dust), which ic/<ic>/answers.txt supplies on stdin, and every physical parameter is a literal in the setup source

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_NMAX=5 sab.py task selfcheck ...
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_NPARTX "ic" "particles across the box, answered once for the gas and once for the dust (the setup prompts twice; the code default offered at the prompt is 64). 'ic' is the graded default and uses the two answers this check vendors in ic/<ic>/answers.txt; a number rewrites both of them. Runtime scales as npartx^4"
knob SAB_TMAX "0.1" "tmax of the .in in code units (the default the setup writes is 10); the number of steps and the runtime scale linearly with it"
knob SAB_DTMAX "0.05" "time between dumps (dtmax in the .in; the default the setup writes is 1); the graded default writes two full dumps"
knob SAB_NMAX "-1" "cap on the number of time steps (nmax in the .in); -1 runs to the window above (graded); a small cap exercises build, setup, run and output only"
knob SAB_THREADS "2" "OMP_NUM_THREADS for phantomsetup and phantom; the graded default; changing it changes the order in which the OpenMP loops sum, so the graded arrays may move at round-off, within the check's bound"
knob SAB_MAXP "60000" "particle-array bound passed to phantomsetup as --maxp; must exceed the gas plus dust particle count"
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
mkdir -p "$RUN" "$SRC"
cp -R "$SOURCE_DIR/." "$SRC"

# The initial condition: this check's fixed inputs from ic/<ic>/.
cp -R "$CHECK_DIR/ic/$INPUTS/." "$RUN/"

# This check's initial condition lives in source literals, so ic/<ic>/source.patch is applied
# to the scratch copy of the source before the build (nominal's patch is empty).
if [ -s "$RUN/source.patch" ]; then
  (cd "$SRC" && patch -p1 --batch --forward <"$RUN/source.patch") || { echo "run.sh: source.patch did not apply" >&2; exit 1; }
fi

# Build phantom and phantomsetup for SETUP=dustysedov. Parallel make is broken upstream
# (build/.depends is empty, so the objects carry no inter-dependencies) and two goals in one
# invocation race and clean each other's objects: build serially, one goal per call.
export SYSTEM=gfortran OMP_NUM_THREADS="$SAB_THREADS"
BUILD_START=$(date +%s)
if ! (cd "$SRC" && make ${MAKE_EXTRA[@]+"${MAKE_EXTRA[@]}"} SETUP=dustysedov phantom >"$WORK/make.log" 2>&1 && make ${MAKE_EXTRA[@]+"${MAKE_EXTRA[@]}"} SETUP=dustysedov setup >>"$WORK/make.log" 2>&1); then
  echo "run.sh: build failed" >&2; tail -n 40 "$WORK/make.log" >&2; exit 1
fi
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only
cd "$RUN"

# Rewrite one "key = value" line of a Phantom options file in place.
setkey() {
  python3 - "$1" "$2" "$3" <<'PY'
import re, sys
path, key, value = sys.argv[1:]
text = open(path, encoding="ascii", errors="replace").read()
pat = re.compile(r"^(\s*%s\s*=\s*)\S+" % re.escape(key), re.M)
if not pat.search(text):
    sys.exit("run.sh: no '%s =' line in %s" % (key, path))
open(path, "w", encoding="ascii").write(pat.sub(lambda m: m.group(1) + value, text, count=1))
PY
}

# The prompt answers are an input of this check: ic/<ic>/answers.txt is copied in above and
# used as it stands unless the resolution knob names a number, which rewrites both answers.
if [ "$SAB_NPARTX" != "ic" ]; then printf '%s\n%s\n' "$SAB_NPARTX" "$SAB_NPARTX" >answers.txt; fi
[ -s answers.txt ] || { echo "run.sh: ic/$INPUTS/answers.txt is empty" >&2; exit 1; }

# phantomsetup is a two-pass program: with an incomplete .setup it rewrites the file and
# stops, so it is called until the t=0 dump appears (the upstream growth workflow calls it
# three times for the same reason). It is fed 40 blank lines because reaching EOF on the
# prompt reader aborts with a backtrace, and --maxp caps the up-front particle allocation
# (the default reserves gigabytes for any problem size; the cap is bit-neutral, only the
# array bound changes).
for i in 1 2 3; do
  { cat answers.txt; yes '' | head -n 40; } | "$SRC/bin/phantomsetup" dustysedov --maxp="$SAB_MAXP" >"setup$i.log" 2>&1 || true
  if [ -f dustysedov_00000.tmp ] && [ -f dustysedov.in ]; then break; fi
done
[ -f dustysedov_00000.tmp ] || { echo "run.sh: phantomsetup wrote no dustysedov_00000.tmp" >&2; tail -n 40 setup*.log >&2; exit 1; }
[ -f dustysedov.in ] || { echo "run.sh: phantomsetup wrote no dustysedov.in" >&2; tail -n 40 setup*.log >&2; exit 1; }

# The graded run: every dump a full dump (nfulldump=1; the default 10 makes the
# intermediate dumps small dumps that carry only float32 x,y,z,h), the wall-clock controls
# off (dtwallmax/twallmax would tie the step sequence to the host), the window from the knobs.
setkey dustysedov.in tmax "$SAB_TMAX"
setkey dustysedov.in dtmax "$SAB_DTMAX"
setkey dustysedov.in nfulldump "1"
setkey dustysedov.in dtwallmax "000:00"
setkey dustysedov.in twallmax "000:00"
if [ "$SAB_NMAX" != "-1" ]; then setkey dustysedov.in nmax "$SAB_NMAX"; fi
if ! "$SRC/bin/phantom" dustysedov.in >phantom.log 2>&1; then
  echo "run.sh: phantom failed" >&2; tail -n 60 phantom.log >&2; exit 1
fi

# The graded file: the last full dump. Nothing else is copied out - the .ev file and the
# log carry OpenMP-reduction sums and wall-clock times that are not part of the state.
last="$(ls -1 dustysedov_[0-9][0-9][0-9][0-9][0-9] 2>/dev/null | tail -n 1 || true)"
[ -n "$last" ] || { echo "run.sh: no dump written" >&2; tail -n 40 phantom.log >&2; exit 1; }
cp "$last" "$OUT_DIR/final_dump"
echo "run.sh: graded dump $last from ic/$INPUTS"
grep -E '^ *(npart|nptmass) *=' phantom.log | head -n 4 || true
