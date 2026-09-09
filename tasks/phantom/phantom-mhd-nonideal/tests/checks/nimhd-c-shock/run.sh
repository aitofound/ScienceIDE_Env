#!/usr/bin/env bash
# Check nimhd-c-shock: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# Official test: Phantom SETUP=nimhdshock (ambipolar C-shock; build/Makefile_setups, src/setup/setup_shock.f90),
# evolved from the setup's own initial condition; the graded file is the last full dump.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_NMAX=5 sab.py task selfcheck ...
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_TMAX "2.500E+04" "end time of the run in code units (official 4.000E+06); the number of steps and the runtime scale linearly with it; the default is the graded window"
knob SAB_DTMAX "1.250E+04" "time between dumps in code units; the graded default divides SAB_TMAX so the last dump lands exactly on it"
knob SAB_NX "200" "resolution (number of particles in x over the left half of the tube) written into shock.setup; npart scales as nx^3 and the runtime with it; 200 is both the official value and the graded value"
knob SAB_NMAX "-1" "cap on the number of time steps (nmax in the .in); -1 runs to SAB_TMAX (graded); a small cap exercises build, setup, run and output only"
knob SAB_THREADS "2" "OMP_NUM_THREADS for phantomsetup and phantom; the graded default. Changing it may change the order in which neighbour sums are accumulated. Cross-thread behaviour was not calibrated at the graded window, so no cross-thread determinism or equivalence result is claimed; every run must satisfy rubric.json"
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
mkdir -p "$RUN"
cp -R "$SOURCE_DIR/." "$SRC"

# Build phantom and phantomsetup for this SETUP. Parallel make is broken upstream
# (build/.depends is empty and the goals clean each other), so the build is serial
# and one goal per invocation; changing SETUP is always a full rebuild.
export SYSTEM=gfortran OMP_NUM_THREADS="$SAB_THREADS"
BUILD_START=$(date +%s)
if ! (cd "$SRC" && make ${MAKE_EXTRA[@]+"${MAKE_EXTRA[@]}"} SETUP=nimhdshock phantom >"$WORK/make.log" 2>&1 && make ${MAKE_EXTRA[@]+"${MAKE_EXTRA[@]}"} SETUP=nimhdshock setup >>"$WORK/make.log" 2>&1); then
  echo "run.sh: build failed" >&2; tail -n 40 "$WORK/make.log" >&2; exit 1
fi
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only

# The initial condition: this check's fixed inputs from ic/<ic>/, then phantomsetup.
cp -R "$CHECK_DIR/ic/$INPUTS/." "$RUN/"
cd "$RUN"

# Resolution and window knobs. setup_shock.f90 is the only setup that keeps tmax/dtmax in the
# .setup: it extends the tube by v*tmax, so the window also fixes the particle count. Both are
# rewritten here; the graded defaults are the values already in the frozen .setup.
python3 - shock.setup "nx=$SAB_NX" "tmax=$SAB_TMAX" "dtmax=$SAB_DTMAX" <<'PY'
import re, sys
path = sys.argv[1]
text = open(path, encoding="ascii", errors="replace").read()
for kv in sys.argv[2:]:
    key, value = kv.split("=", 1)
    pat = re.compile(r"^(\s*%s\s*=\s*)\S+" % re.escape(key), re.M)
    if not pat.search(text):
        sys.exit("run.sh: no '%s =' line in the .setup" % key)
    text = pat.sub(lambda m: m.group(1) + value, text, count=1)
open(path, "w", encoding="ascii").write(text)
PY

# phantomsetup is a two-pass program: with the .setup supplied the first call already writes the
# dump and the .in; without one it writes the .setup and stops, so it is called again. Remaining
# prompts read 40 blank lines, i.e. every default, exactly as the upstream buildbot drives it.
python3 -c "import sys; sys.stdout.write(chr(10) * 40)" >answers.txt
"$SRC/bin/phantomsetup" shock <answers.txt >setup1.log 2>&1 || true
[ -f shock.in ] || "$SRC/bin/phantomsetup" shock <answers.txt >setup2.log 2>&1 || true
[ -f shock.in ] || { echo "run.sh: phantomsetup wrote no shock.in" >&2; tail -n 40 setup1.log >&2; exit 1; }

# The graded run: every dump a full dump (nfulldump=1, else dumps 1..9 are small dumps that carry
# neither velocities nor psi nor the non-ideal coefficients), the wall-clock limits off
# (they would tie the step sequence to the host), the window from the knobs.
# The NICIL coefficients are restated here so the non-ideal configuration of the graded run is
# explicit and independent of the library defaults in src/lib/NICIL/src/nicil.F90:124-126.
# ic/<ic>/in_overrides.txt, if present, is appended last: it is where an initial condition that has
# no continuous scalar in its .setup puts its one perturbed value, and phantom's own writer would
# truncate it if it were shipped inside a frozen .in.
KEYS=("tmax=$SAB_TMAX" "dtmax=$SAB_DTMAX" nfulldump=1 dtwallmax=000:00 twallmax=000:00 use_ohm=F use_hall=F use_ambi=T eta_constant=T eta_const_type=2 C_AD=1.000E+05)
if [ "$SAB_NMAX" -ge 0 ] 2>/dev/null; then KEYS+=("nmax=$SAB_NMAX"); fi
if [ -f in_overrides.txt ]; then
  while IFS= read -r line || [ -n "$line" ]; do
    line="${line%%#*}"; line="$(printf '%s' "$line" | tr -d '[:space:]')"
    [ -n "$line" ] && KEYS+=("$line")
  done <in_overrides.txt
fi
python3 - shock.in "${KEYS[@]}" <<'PY'
import re, sys
path = sys.argv[1]
text = open(path, encoding="ascii", errors="replace").read()
for kv in sys.argv[2:]:
    key, value = kv.split("=", 1)
    pat = re.compile(r"^(\s*%s\s*=\s*)\S+" % re.escape(key), re.M)
    if not pat.search(text):
        sys.exit("run.sh: no '%s =' line in the .in" % key)
    text = pat.sub(lambda m: m.group(1) + value, text, count=1)
open(path, "w", encoding="ascii").write(text)
PY

if ! "$SRC/bin/phantom" shock.in >phantom.log 2>&1; then
  echo "run.sh: phantom failed" >&2; tail -n 60 phantom.log >&2; exit 1
fi

# The graded file, named as rubric.json describes it: the last full dump, and nothing else.
# The .ev file, phantom.log and the make/setup logs stay in the work directory: they carry
# OpenMP-reduction sums, wall times and timestamps, and a file in OUT_DIR that can never
# match would make the verifier's byte-identical safeguard inert. On failure the tails above
# go to stderr.
last="$(ls shock_[0-9][0-9][0-9][0-9][0-9] | tail -n 1)"
[ -n "$last" ] || { echo "run.sh: no dump written" >&2; exit 1; }
cp "$last" "$OUT_DIR/final_dump"
