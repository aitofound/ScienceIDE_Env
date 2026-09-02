#!/usr/bin/env bash
# Check test-wind-unit: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh --help                       list the runtime knobs below
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# Official test: the `wind` entry of Phantom's own unit suite, src/tests/test_wind.f90,
# reached by `bin/phantomtest wind` (the selector is dispatched in src/tests/testsuite.F90).
# Upstream CI (.github/workflows/test.yml) compiles the suite under SETUP=test, testkd,
# test2 and testcyl and runs all four; each is a different compile-time physics
# configuration of the same procedure, so each is its own check. This one is SETUP=test:
# PERIODIC, MHD, DUST, RADIATION, SINK_RADIATION, DUST_NUCLEATION, CONST_ARTRES and the
# cubic kernel, the only official mode besides testkd that compiles sink radiation, so it scores
# both cases of the test: the trans-sonic wind and the super-sonic wind with Bowen dust and
# radiative acceleration on the sink (src/main/ptmass_radiation.f90, src/main/dust_formation.f90).
# The test reads nothing from disk; every initial condition is a literal in test_wind.f90,
# so ic/<ic>/ carries a unified diff that run.sh applies to the copy of that source.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_TMAX=2 sab.py task selfcheck ...
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_TMAX "12." "the tmax literal of src/tests/test_wind.f90:205, in code units; 12. is the official value and the graded one, and the run cost is roughly linear in it; the suite's own assertions on injected and accreted mass are calibrated for the official value, so a shorter window is for smoke runs only"
knob SAB_THREADS "2" "OMP_NUM_THREADS for bin/phantomtest, the graded value; the printed assertion values are the same at 1 and at 2 threads, the wall time is not"
knob SAB_SELECTORS "" "extra phantomtest selectors appended to the one in ic/<ic>/selectors.txt; empty is the graded value, and anything else changes which procedures are scored and with them the graded transcript"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
[ -d "$CHECK_DIR/ic/$IC" ] || { echo "run.sh: no initial condition ic/$IC" >&2; exit 2; }
[ -f "$CHECK_DIR/ic/$IC/source.patch" ] || { echo "run.sh: ic/$IC/source.patch missing" >&2; exit 2; }
[ -f "$CHECK_DIR/ic/$IC/selectors.txt" ] || { echo "run.sh: ic/$IC/selectors.txt missing" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
SRC="$WORK/src"; RUN="$WORK/run"
mkdir -p "$RUN"
cp -R "$SOURCE_DIR/." "$SRC"

# The initial condition: ic/<ic>/source.patch is applied to the copy of the test source
# before it is compiled. ic/nominal/source.patch is empty, i.e. the pinned source unchanged.
if [ -s "$CHECK_DIR/ic/$IC/source.patch" ]; then
  (cd "$SRC" && patch -p1 --batch --forward <"$CHECK_DIR/ic/$IC/source.patch") || {
    echo "run.sh: ic/$IC/source.patch does not apply to the pinned source" >&2; exit 1; }
fi
# The window knob edits the same literal.
python3 - "$SRC/src/tests/test_wind.f90" "$SAB_TMAX" <<'PY'
import re, sys
path, tmax = sys.argv[1:]
text = open(path, encoding="ascii", errors="replace").read()
pat = re.compile(r"^(\s*tmax\s*=\s*)\S+", re.M)
if not pat.search(text): sys.exit("run.sh: no 'tmax =' line in test_wind.f90")
open(path, "w", encoding="ascii").write(pat.sub(lambda m: m.group(1) + tmax, text, count=1))
PY

# Build bin/phantomtest for SETUP=test. Parallel make is broken upstream (build/.depends is
# empty and build/Makefile relies on the order of SRCTESTS), so the build is serial with one
# goal per invocation.
export SYSTEM=gfortran OMP_NUM_THREADS="$SAB_THREADS" OMP_STACKSIZE=512M
BUILD_START=$(date +%s)
if ! (cd "$SRC" && make SETUP=test phantomtest >"$WORK/make.log" 2>&1); then
  echo "run.sh: build failed" >&2; tail -n 40 "$WORK/make.log" >&2; exit 1
fi
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only

# Run the suite in a fresh directory: the test writes the 1D wind solution 01_profile.dat
# into the working directory.
cd "$RUN"
read -r -a SELECTORS <<<"$(tr '\n' ' ' <"$CHECK_DIR/ic/$IC/selectors.txt") $SAB_SELECTORS"
[ "${#SELECTORS[@]}" -gt 0 ] || { echo "run.sh: no phantomtest selector" >&2; exit 2; }
if ! "$SRC/bin/phantomtest" "${SELECTORS[@]}" >phantomtest.log 2>&1; then
  echo "run.sh: phantomtest exited nonzero, i.e. a scored test failed" >&2; tail -n 40 phantomtest.log >&2; exit 1
fi

# The graded file: the assertion lines of the transcript. Everything else the suite prints is
# an epigraph, a compile-settings banner, or a wall-clock, CPU-time, memory or thread-count
# report, none of which is a result, and all of which is dropped here.
grep -E '^ *checking .*(OK|FAILED)|^(PASSED|FAILED): *[0-9]' phantomtest.log >"$OUT_DIR/results.txt" || {
  echo "run.sh: phantomtest printed no assertion line" >&2; tail -n 40 phantomtest.log >&2; exit 1; }
# A selector that matches nothing runs the whole suite instead of failing (src/tests/testsuite.F90),
# and the wind test skips itself under MPI or without the wind injection module
# (src/tests/test_wind.f90:52), so the expected scored count is asserted here as well as graded.
grep -q '^PASSED: *5 of *5' "$OUT_DIR/results.txt" || {
  echo "run.sh: expected 'PASSED: 5 of 5' from the wind test under SETUP=test" >&2; cat "$OUT_DIR/results.txt" >&2; exit 1; }
echo "run.sh: graded $(wc -l <"$OUT_DIR/results.txt") assertion lines"
