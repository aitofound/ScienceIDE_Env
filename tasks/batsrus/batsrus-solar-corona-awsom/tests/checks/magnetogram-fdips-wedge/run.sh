#!/usr/bin/env bash
# Check magnetogram-fdips-wedge: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     OPTIONAL: the nominal inputs on the alternative build (ALTBUILD below)
#   run.sh --help                       list the runtime knobs below, and the altbuild line when one is declared
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# Upstream test: the target `test_fdips_wedge` of code/batsrus/util/DATAREAD/srcMagnetogram/Makefile.
# The build, the control file, the run and the graded files are the upstream
# recipe. The one departure is the magnetogram: upstream runs DIPOLE11.exe to
# generate it into the working directory, and this check ships that same
# magnetogram under ic/ instead, so the two initial conditions can differ.
# The wedge magnetogram is upstream's own fdips_wedge_input.gz, shipped here under ic/ the same way.

KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_MPI_RANKS "2" "MPI ranks of the graded run; the upstream test uses 2 and the graded reference is produced with 2 (the domain decomposition of FDIPS depends on it, so changing it changes the graded numbers at round-off)"
knob SAB_MPI_EXTRA "" "extra arguments passed to mpiexec (for example --oversubscribe on a host with fewer slots than ranks); empty is the graded value and does not change the result"
knob SAB_BUILD_JOBS "4" "make -j for the build; affects build time only, never the graded values"
# Alternative build, OPTIONAL. Set ALTBUILD to one line naming a legitimately different build of the
# same source (IEEE mode, -O0, a second compiler present in the image: something a correct candidate
# could plausibly be) ONLY when this check can be built that way; leave it empty otherwise. When it is
# set, `run.sh altbuild` runs ic/nominal on that build and selfcheck measures the check's floor from it.
ALTBUILD="the same Config.pl configuration built with ./Config.pl -O0 before make BATSRUS, which sets every OPTn level of Makefile.conf to -O0 where the shipped gfortran template uses -O3; same pinned source, same deck"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
# mpiexec forwards standard input to rank 0 and drains it. The produce driver
# feeds the check list to its own loop on standard input, so a check that leaves
# stdin connected swallows the checks after it; take stdin away here.
exec < /dev/null
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"
if [ "$IC" = altbuild ]; then
  [ -n "$ALTBUILD" ] || { echo "run.sh: this check declares no alternative build" >&2; exit 2; }
  INPUTS=nominal
fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
export LC_ALL=C
cp -R "$SOURCE_DIR/." "$WORK/src"
cd "$WORK/src"

BUILD_START=$(date +%s)
{
  ./Config.pl -install -compiler=gfortran
  if [ "$IC" = altbuild ]; then
    ./Config.pl -O0 >> "$WORK/config.log" 2>&1
    grep -q '^OPT3 = -O0' Makefile.conf || { echo "run.sh: Config.pl -O0 did not set OPT3" >&2; exit 1; }
  fi
  # libSHARE first and on its own: the srcMagnetogram targets list it as a
  # prerequisite next to their own objects, which is not parallel-safe.
  make -C util/DATAREAD/srcMagnetogram libSHARE
  make -j"$SAB_BUILD_JOBS" -C util/DATAREAD/srcMagnetogram FDIPS
} > "$WORK/build.log" 2>&1 || { echo "run.sh: build failed" >&2; tail -n 60 "$WORK/build.log" >&2; exit 1; }
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"

RUN="$WORK/src/util/DATAREAD/srcMagnetogram"
cd "$RUN"
for f in "$CHECK_DIR/ic/$INPUTS"/*; do
  case "$f" in
    *.gz) gunzip -c "$f" > "$(basename "${f%.gz}")" ;;
    *)    cp "$f" "$(basename "$f")" ;;
  esac
done

mpiexec -n "$SAB_MPI_RANKS" ${SAB_MPI_EXTRA:-} ./FDIPS.exe > tool.log 2>&1 || { echo "run.sh: FDIPS.exe failed" >&2; tail -n 40 tool.log >&2; exit 1; }

cp "fdips_field.out" "$OUT_DIR/fdips_field.out"
