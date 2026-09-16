#!/usr/bin/env bash
# Check radiativebox-diffusion: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# Official test: Phantom SETUP=radiativebox (build/Makefile_setups; src/setup/setup_radiativebox.f90),
# the Gaussian faster-than-light radiation diffusion box (iradtype=3), evolved from the setup's own
# initial condition; the graded file is the last full dump.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_NDUMPS=1 sab.py task selfcheck ...
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_NDUMPS "200" "number of dumps of length dtmax = 0.14490389 to evolve; 200 is the official window written by setup_radiativebox.f90 (tmax = 28.9807779) and is the graded default; runtime scales linearly with it"
knob SAB_NX "32" "number of particles across the box in x in radbox.setup (official 32); the particle count and the runtime scale as SAB_NX^3, the step count as SAB_NX^2"
knob SAB_NMAX "-1" "cap on the number of time steps (nmax in the .in); -1 runs to the graded window; a small cap exercises build, setup, run and output only"
knob SAB_THREADS "1" "OMP_NUM_THREADS for phantomsetup and phantom; the graded default is 1 because the periodic radiation derivative path is OpenMP-order dependent (see rubric.json)"
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
# (build/.depends is empty), so the build is serial and one goal per invocation.
export SYSTEM=gfortran OMP_NUM_THREADS="$SAB_THREADS" OMP_STACKSIZE=64M
BUILD_START=$(date +%s)
if ! (cd "$SRC" && make ${MAKE_EXTRA[@]+"${MAKE_EXTRA[@]}"} SETUP=radiativebox phantom >"$WORK/make.log" 2>&1 && make ${MAKE_EXTRA[@]+"${MAKE_EXTRA[@]}"} SETUP=radiativebox setup >>"$WORK/make.log" 2>&1); then
  echo "run.sh: build failed" >&2; tail -n 40 "$WORK/make.log" >&2; exit 1
fi
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only

# The initial condition: this check's radbox.setup and radbox.in from ic/<ic>/.
# phantomsetup reads an existing .in, so the frozen .in fixes the physics options that the
# setup routine does not own (in particular iopacity_type = 2 with kappa_cgs = 1 cm^2/g, the
# constant opacity that setup_radiativebox.f90 gives every particle; the code default
# iopacity_type = 1 would read the MESA opacity table, which is not in the repository).
cp -R "$CHECK_DIR/ic/$INPUTS/." "$RUN/"
cd "$RUN"
python3 - radbox.setup "$SAB_NX" <<'PY'
import re, sys
path, nx = sys.argv[1:]
text = open(path, encoding="ascii").read()
pat = re.compile(r"^(\s*nx\s*=\s*)\S+", re.M)
if not pat.search(text): sys.exit("run.sh: no 'nx =' line in radbox.setup")
open(path, "w", encoding="ascii").write(pat.sub(lambda m: m.group(1) + nx, text, count=1))
PY
yes '' | head -n 40 | "$SRC/bin/phantomsetup" radbox >setup1.log 2>&1 || true
[ -f radbox_00000.tmp ] || { yes '' | head -n 40 | "$SRC/bin/phantomsetup" radbox >setup2.log 2>&1 || true; }
[ -f radbox_00000.tmp ] || { echo "run.sh: phantomsetup wrote no radbox_00000.tmp" >&2; tail -n 40 setup*.log >&2; exit 1; }

# The graded run: every written dump a full dump (nfulldump=1), the wall-clock limits off
# (dtwallmax/twallmax would tie the step sequence to the host), and the window set to
# SAB_NDUMPS times the dtmax that setup_radiativebox.f90 derives from the diffusion time.
# nout = SAB_NDUMPS makes phantom write only the t = 0 dump and the final one (io_control.f90:183);
# it gates the writing only and leaves the timestep sequence untouched, and it keeps the run
# directory from filling with 200 dumps of 7.7 MB.
python3 - radbox.in "$SAB_NDUMPS" "$SAB_NMAX" <<'PY'
import re, sys
path, ndumps, nmax = sys.argv[1:]
text = open(path, encoding="ascii", errors="replace").read()
def get(key):
    m = re.search(r"^\s*%s\s*=\s*(\S+)" % re.escape(key), text, re.M)
    if not m: sys.exit("run.sh: no '%s =' line in the .in" % key)
    return float(m.group(1))
def setkey(text, key, value, required=True):
    # dtwallmax is written by dynamic_dtmax.f90:80 only when it is positive, so an .in with the
    # wall-clock dump limit already off carries no such line; absent, the code default is 0 = off.
    pat = re.compile(r"^(\s*%s\s*=\s*)\S+" % re.escape(key), re.M)
    if not pat.search(text):
        if required: sys.exit("run.sh: no '%s =' line in the .in" % key)
        return text
    return pat.sub(lambda m: m.group(1) + value, text, count=1)
text = setkey(text, "tmax", "%.17g" % (int(ndumps) * get("dtmax")))
text = setkey(text, "nout", ndumps)
text = setkey(text, "nfulldump", "1")
text = setkey(text, "dtwallmax", "000:00", required=False)
text = setkey(text, "twallmax", "000:00")
if int(nmax) >= 0: text = setkey(text, "nmax", nmax)
open(path, "w", encoding="ascii").write(text)
PY
if ! "$SRC/bin/phantom" radbox.in >phantom.log 2>&1; then
  echo "run.sh: phantom failed" >&2; tail -n 60 phantom.log >&2; exit 1
fi

# Graded files, named as rubric.json describes them, and nothing else: the last full dump.
# phantom.log stays in the work directory and is NOT copied into OUT_DIR - it carries wall and
# CPU times and the OpenMP-reduction energy sums, and the verifier compares every file it finds
# under OUT_DIR, so a log there would make the byte-identical safeguard inert. Its tail is
# printed to stderr above when the run fails, which is when it is wanted.
last="$(ls radbox_[0-9][0-9][0-9][0-9][0-9] | tail -n 1)"
[ -n "$last" ] || { echo "run.sh: no dump written" >&2; exit 1; }
cp "$last" "$OUT_DIR/final_dump"
