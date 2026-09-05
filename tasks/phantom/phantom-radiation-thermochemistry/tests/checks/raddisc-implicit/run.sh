#!/usr/bin/env bash
# Check raddisc-implicit: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# Official test: Phantom SETUP=raddisc (build/Makefile_setups; src/setup/setup_disc.f90), the
# adiabatic accretion disc with flux-limited-diffusion radiation, evolved with the backward-Euler
# implicit radiation solver; the graded file is the last full dump.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_TMAX=0.5 sab.py task selfcheck ...
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_TMAX "1.0" "tmax of the .in in code units (the official deck runs 100 outer orbits, tmax = 1.1544e6); runtime scales linearly; the default is the graded window"
knob SAB_DTMAX "0.5" "time between dumps in code units (official 1154.4); the graded dump is the last one, so SAB_TMAX/SAB_DTMAX is the number of dumps"
knob SAB_NP "20000" "number of gas particles requested in rdisc.setup (official 1000000); the runtime scales roughly linearly with it"
knob SAB_NMAX "-1" "cap on the number of time steps (nmax in the .in); -1 runs to SAB_TMAX (graded); a small cap exercises build, setup, run and output only"
knob SAB_THREADS "2" "OMP_NUM_THREADS for phantomsetup and phantom; the graded default; this non-periodic implicit-radiation run is bit-reproducible at this thread count (see rubric.json)"
# Alternative build: unlike the rest of this leaf, this check does NOT use Phantom's DEBUG=yes
# build (see comment/README.md): under -finit-real=nan + -ffpe-trap=invalid, the module-level
# rad_errorE/rad_errorU of src/main/radiation_implicit.f90:41 are still NaN the first time
# get_energies_and_init_ev_files (initial.F90:771) writes the startup .ev record, and
# ev_data_update's max()/min() over those NaNs (src/main/energies.f90:912) traps before the first
# step - a debug-build artifact of the diagnostic running before its first legitimate value
# exists, not a bug this check's physics can expose. The fallback is the optimisation change
# alone: -O3 -> -O0 in the scratch copy of build/Makefile_defaults_gfortran, no runtime checks.
# `run.sh altbuild` uses the nominal inputs; selfcheck measures the floor from the second legitimate build.
ALTBUILD="build/Makefile_defaults_gfortran: FFLAGS -O3 -> -O0 in the scratch copy only, no other flag change: the same pinned source and nominal inputs at Phantom's own -O0 instead of the nominal -O3, without the DEBUG=yes runtime checks (this check's DEBUG=yes altbuild traps SIGFPE in energies.f90:912 before the first step; see comment/README.md)"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"; MAKE_EXTRA=()
if [ "$IC" = altbuild ]; then INPUTS=nominal; fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
SRC="$WORK/src"; RUN="$WORK/run"
mkdir -p "$RUN"
cp -R "$SOURCE_DIR/." "$SRC"
if [ "$IC" = altbuild ]; then
  # Optimisation-only alternative build (see ALTBUILD above): flip the one FFLAGS line of the
  # scratch copy, never SOURCE_DIR, and leave MAKE_EXTRA empty so DEBUG=yes's runtime checks are
  # not compiled in.
  sed -i 's/^FFLAGS+= -O3 /FFLAGS+= -O0 /' "$SRC/build/Makefile_defaults_gfortran"
  grep -q '^FFLAGS+= -O0 ' "$SRC/build/Makefile_defaults_gfortran" || { echo "run.sh: altbuild -O3->-O0 sed did not match" >&2; exit 2; }
fi

# Build phantom and phantomsetup for this SETUP. Parallel make is broken upstream
# (build/.depends is empty), so the build is serial and one goal per invocation.
export SYSTEM=gfortran OMP_NUM_THREADS="$SAB_THREADS" OMP_STACKSIZE=64M
BUILD_START=$(date +%s)
if ! (cd "$SRC" && make ${MAKE_EXTRA[@]+"${MAKE_EXTRA[@]}"} SETUP=raddisc phantom >"$WORK/make.log" 2>&1 && make ${MAKE_EXTRA[@]+"${MAKE_EXTRA[@]}"} SETUP=raddisc setup >>"$WORK/make.log" 2>&1); then
  echo "run.sh: build failed" >&2; tail -n 40 "$WORK/make.log" >&2; exit 1
fi
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only

# The initial condition: this check's rdisc.setup and rdisc.in from ic/<ic>/. The frozen .in fixes
# the physics options that setup_disc.f90 does not own: iopacity_type = 2 with kappa_cgs = 1 cm^2/g
# (the code default iopacity_type = 1 would read the MESA opacity table, which is not in the
# repository) and the implicit radiation solver with tol_rad = 1e-6, itsmax_rad = 250.
cp -R "$CHECK_DIR/ic/$INPUTS/." "$RUN/"
cd "$RUN"
python3 - rdisc.setup "$SAB_NP" <<'PY'
import re, sys
path, np_ = sys.argv[1:]
text = open(path, encoding="ascii").read()
pat = re.compile(r"^(\s*np\s*=\s*)\S+", re.M)
if not pat.search(text): sys.exit("run.sh: no 'np =' line in rdisc.setup")
open(path, "w", encoding="ascii").write(pat.sub(lambda m: m.group(1) + np_, text, count=1))
PY
yes '' | head -n 40 | "$SRC/bin/phantomsetup" rdisc >setup1.log 2>&1 || true
[ -f rdisc_00000.tmp ] || { yes '' | head -n 40 | "$SRC/bin/phantomsetup" rdisc >setup2.log 2>&1 || true; }
[ -f rdisc_00000.tmp ] || { echo "run.sh: phantomsetup wrote no rdisc_00000.tmp" >&2; tail -n 40 setup*.log >&2; exit 1; }

# The graded run: every dump a full dump (nfulldump=1), the wall-clock limits off
# (dtwallmax/twallmax would tie the step sequence to the host), the window from the knobs, and the
# implicit radiation solver on (setup_disc.f90 writes tmax and dtmax from norbits and deltat and
# does not touch implicit_radiation, so both are set here after phantomsetup has run).
python3 - rdisc.in "$SAB_TMAX" "$SAB_DTMAX" "$SAB_NMAX" <<'PY'
import re, sys
path, tmax, dtmax, nmax = sys.argv[1:]
text = open(path, encoding="ascii", errors="replace").read()
def setkey(text, key, value, required=True):
    # dtwallmax is written by dynamic_dtmax.f90:80 only when it is positive, so an .in with the
    # wall-clock dump limit already off carries no such line; absent, the code default is 0 = off.
    pat = re.compile(r"^(\s*%s\s*=\s*)\S+" % re.escape(key), re.M)
    if not pat.search(text):
        if required: sys.exit("run.sh: no '%s =' line in the .in" % key)
        return text
    return pat.sub(lambda m: m.group(1) + value, text, count=1)
text = setkey(text, "tmax", tmax)
text = setkey(text, "dtmax", dtmax)
text = setkey(text, "nfulldump", "1")
text = setkey(text, "dtwallmax", "000:00", required=False)
text = setkey(text, "twallmax", "000:00")
text = setkey(text, "implicit_radiation", "T")
if int(nmax) >= 0: text = setkey(text, "nmax", nmax)
open(path, "w", encoding="ascii").write(text)
PY
if ! "$SRC/bin/phantom" rdisc.in >phantom.log 2>&1; then
  echo "run.sh: phantom failed" >&2; tail -n 60 phantom.log >&2; exit 1
fi

# Graded files, named as rubric.json describes them, and nothing else: the last full dump.
# phantom.log stays in the work directory and is NOT copied into OUT_DIR - it carries wall and
# CPU times and the OpenMP-reduction energy sums, and the verifier compares every file it finds
# under OUT_DIR, so a log there would make the byte-identical safeguard inert. Its tail is
# printed to stderr above when the run fails, which is when it is wanted.
last="$(ls rdisc_[0-9][0-9][0-9][0-9][0-9] | tail -n 1)"
[ -n "$last" ] || { echo "run.sh: no dump written" >&2; exit 1; }
cp "$last" "$OUT_DIR/final_dump"
