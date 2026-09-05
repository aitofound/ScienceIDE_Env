#!/usr/bin/env bash
# Check growingdisc-official-grow: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# Official test: the growth regression of .github/workflows/growth.yml: SETUP=growingdisc evolved from the grow.setup/grow.in pair pinned by that workflow to the v2025.0.0 release, which stops itself after one full dump (nmaxdumps=1); the graded file is that dump

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_NMAX=5 sab.py task selfcheck ...
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_NP "2000" "number of gas particles (np in grow.setup); the release value; runtime scales with it"
knob SAB_NP_DUST "2000" "number of large dust particles (np_dust in grow.setup); the release value"
knob SAB_NMAXDUMPS "1" "stop after this many full dumps (nmaxdumps in grow.in); 1 is the release value and the graded window, one dtmax = 266.572976 code units"
knob SAB_TMAX "1.777E+04" "tmax of grow.in in code units; the release value; nmaxdumps stops the run long before it"
knob SAB_NMAX "-1" "cap on the number of time steps (nmax in the .in); -1 runs to the window above (graded); a small cap exercises build, setup, run and output only"
knob SAB_THREADS "2" "OMP_NUM_THREADS for phantomsetup and phantom; the graded default; changing it changes the order in which the OpenMP loops sum, so the graded arrays may move at round-off, within the check's bound"
knob SAB_MAXP "20000" "particle-array bound passed to phantomsetup as --maxp; must exceed SAB_NP+SAB_NP_DUST"
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

# Build phantom and phantomsetup for SETUP=growingdisc. Parallel make is broken upstream
# (build/.depends is empty, so the objects carry no inter-dependencies) and two goals in one
# invocation race and clean each other's objects: build serially, one goal per call.
export SYSTEM=gfortran OMP_NUM_THREADS="$SAB_THREADS"
BUILD_START=$(date +%s)
if ! (cd "$SRC" && make ${MAKE_EXTRA[@]+"${MAKE_EXTRA[@]}"} SETUP=growingdisc phantom >"$WORK/make.log" 2>&1 && make ${MAKE_EXTRA[@]+"${MAKE_EXTRA[@]}"} SETUP=growingdisc setup >>"$WORK/make.log" 2>&1); then
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

# The resolution knob rewrites the .setup before phantomsetup reads it.
setkey grow.setup np "$SAB_NP"
setkey grow.setup np_dust "$SAB_NP_DUST"

# phantomsetup is a two-pass program: with an incomplete .setup it rewrites the file and
# stops, so it is called until the t=0 dump appears (the upstream growth workflow calls it
# three times for the same reason). It is fed 40 blank lines because reaching EOF on the
# prompt reader aborts with a backtrace, and --maxp caps the up-front particle allocation
# (the default reserves gigabytes for any problem size; the cap is bit-neutral, only the
# array bound changes).
for i in 1 2 3; do
  yes '' | head -n 40 | "$SRC/bin/phantomsetup" grow.setup --maxp="$SAB_MAXP" >"setup$i.log" 2>&1 || true
  if [ -f grow_00000.tmp ] && [ -f grow.in ]; then break; fi
done
[ -f grow_00000.tmp ] || { echo "run.sh: phantomsetup wrote no grow_00000.tmp" >&2; tail -n 40 setup*.log >&2; exit 1; }
[ -f grow.in ] || { echo "run.sh: phantomsetup wrote no grow.in" >&2; tail -n 40 setup*.log >&2; exit 1; }

# The graded run: every dump a full dump (nfulldump=1; the default 10 makes the
# intermediate dumps small dumps that carry only float32 x,y,z,h), the wall-clock controls
# off (dtwallmax/twallmax would tie the step sequence to the host), the window from the knobs.
setkey grow.in tmax "$SAB_TMAX"
setkey grow.in nmaxdumps "$SAB_NMAXDUMPS"
setkey grow.in nfulldump "1"
setkey grow.in dtwallmax "000:00"
setkey grow.in twallmax "000:00"
if [ "$SAB_NMAX" != "-1" ]; then setkey grow.in nmax "$SAB_NMAX"; fi
if ! "$SRC/bin/phantom" grow.in >phantom.log 2>&1; then
  echo "run.sh: phantom failed" >&2; tail -n 60 phantom.log >&2; exit 1
fi

# The graded file: the last full dump. Nothing else is copied out - the .ev file and the
# log carry OpenMP-reduction sums and wall-clock times that are not part of the state.
last="$(ls -1 grow_[0-9][0-9][0-9][0-9][0-9] 2>/dev/null | tail -n 1 || true)"
[ -n "$last" ] || { echo "run.sh: no dump written" >&2; tail -n 40 phantom.log >&2; exit 1; }
cp "$last" "$OUT_DIR/final_dump"
echo "run.sh: graded dump $last from ic/$INPUTS"
grep -E '^ *(npart|nptmass) *=' phantom.log | head -n 4 || true
