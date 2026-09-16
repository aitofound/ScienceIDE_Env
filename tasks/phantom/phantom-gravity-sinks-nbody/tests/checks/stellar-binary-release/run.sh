#!/usr/bin/env bash
# Official Phantom release binary example: import and relax two MESA stars, assemble their
# orbit, evolve the gas plus sink core, and grade the last full dump.

KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_TMAX "10." "end time in code units; the upstream release value and graded window"
knob SAB_DTMAX "1.000" "full-dump interval in code units; the upstream release value"
knob SAB_NP1 "1000" "requested gas-particle count for the donor star in binary.setup"
knob SAB_NMAX "-1" "optional timestep cap; -1 runs to SAB_TMAX"
knob SAB_THREADS "1" "OMP_NUM_THREADS for setup and evolution"
ALTBUILD="make SYSTEM=gfortran OPENMP=yes DEBUG=yes: Phantom's own -O0 debug build of the same pinned source and nominal inputs"
if [ "${1:-}" = --help ]; then printf '%s' "$KNOB_HELP"; echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"; MAKE_EXTRA=()
if [ "$IC" = altbuild ]; then INPUTS=nominal; MAKE_EXTRA=(SYSTEM=gfortran OPENMP=yes DEBUG=yes); fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
SRC="$WORK/src"; RUN="$WORK/run"
mkdir -p "$RUN"
cp -R "$SOURCE_DIR/." "$SRC"

export SYSTEM=gfortran OMP_NUM_THREADS="$SAB_THREADS"
BUILD_START=$(date +%s)
if ! (cd "$SRC" && make ${MAKE_EXTRA[@]+"${MAKE_EXTRA[@]}"} SETUP=binary phantom >"$WORK/make.log" 2>&1 && make ${MAKE_EXTRA[@]+"${MAKE_EXTRA[@]}"} SETUP=binary setup >>"$WORK/make.log" 2>&1); then
  echo "run.sh: build failed" >&2; tail -n 40 "$WORK/make.log" >&2; exit 1
fi
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"

cp -R "$CHECK_DIR/ic/common/." "$RUN/"
cp -R "$CHECK_DIR/ic/$INPUTS/." "$RUN/"
cd "$RUN"
python3 - binary.setup "$SAB_NP1" <<'PY'
import re, sys
path, np1 = sys.argv[1:]
text = open(path, encoding="ascii", errors="replace").read()
pat = re.compile(r"^(\s*np1\s*=\s*)\S+", re.M)
if not pat.search(text):
    sys.exit("run.sh: no 'np1 =' in binary.setup")
open(path, "w", encoding="ascii").write(pat.sub(lambda m: m.group(1) + np1, text, count=1))
PY

# The upstream binary workflow deliberately calls phantomsetup three times. Pass one upgrades
# the release deck, pass two performs the stellar relaxations, and pass three checks restart/
# rewrite behaviour and recreates the binary initial dump.
for pass_number in 1 2 3; do
  if ! "$SRC/bin/phantomsetup" binary.setup >"setup${pass_number}.log" 2>&1; then
    echo "run.sh: phantomsetup pass $pass_number failed" >&2
    tail -n 60 "setup${pass_number}.log" >&2
    exit 1
  fi
done
# Preserve the upstream release regression exactly for the nominal input.  A
# numerical-noise or compiler variant can cross the convergence threshold one
# relaxation checkpoint earlier/later, so for those runs require a completed
# dump for each star without hard-coding its iteration suffix.  The evolved
# binary dump below remains the graded physics artifact in every mode.
if [ "$IC" = nominal ]; then
  for required in relax1_00005 relax2_00005; do
    [ -f "$required" ] || { echo "run.sh: required upstream setup artifact $required missing" >&2; exit 1; }
  done
else
  relax1_dump="$(find . -maxdepth 1 -type f -name 'relax1_[0-9][0-9][0-9][0-9][0-9]' ! -name 'relax1_00000' -printf '%f\n' | LC_ALL=C sort | tail -n 1)"
  relax2_dump="$(find . -maxdepth 1 -type f -name 'relax2_[0-9][0-9][0-9][0-9][0-9]' ! -name 'relax2_00000' -printf '%f\n' | LC_ALL=C sort | tail -n 1)"
  [ -n "$relax1_dump" ] || { echo "run.sh: no completed relax1 dump" >&2; exit 1; }
  [ -n "$relax2_dump" ] || { echo "run.sh: no completed relax2 dump" >&2; exit 1; }
  echo "SAB_RELAX_DUMPS=$relax1_dump,$relax2_dump"
fi
for required in binary_00000.tmp binary.in; do
  [ -f "$required" ] || { echo "run.sh: required setup artifact $required missing" >&2; exit 1; }
done

python3 - binary.in "$SAB_TMAX" "$SAB_DTMAX" "$SAB_NMAX" <<'PY'
import re, sys
path, tmax, dtmax, nmax = sys.argv[1:]
text = open(path, encoding="ascii", errors="replace").read()
def setkey(s, key, value):
    pat = re.compile(r"^(\s*%s\s*=\s*)\S+" % re.escape(key), re.M)
    if not pat.search(s):
        sys.exit("run.sh: no '%s =' in binary.in" % key)
    return pat.sub(lambda m: m.group(1) + value, s, count=1)
for key, value in (("tmax", tmax), ("dtmax", dtmax), ("nfulldump", "1"), ("dtwallmax", "000:00"), ("twallmax", "000:00")):
    text = setkey(text, key, value)
if int(nmax) >= 0:
    text = setkey(text, "nmax", nmax)
open(path, "w", encoding="ascii").write(text)
PY

if ! "$SRC/bin/phantom" binary.in >phantom.log 2>&1; then
  echo "run.sh: phantom failed" >&2; tail -n 60 phantom.log >&2; exit 1
fi
last="$(ls binary_[0-9][0-9][0-9][0-9][0-9] 2>/dev/null | tail -n 1)"
[ -n "$last" ] && [ "$last" != binary_00000 ] || { echo "run.sh: no evolved full dump" >&2; exit 1; }
cp "$last" "$OUT_DIR/final_dump"
