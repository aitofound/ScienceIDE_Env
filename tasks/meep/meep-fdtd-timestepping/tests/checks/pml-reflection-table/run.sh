#!/usr/bin/env bash
# Check pml-reflection-table: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh --help                       what this check exposes; it has no runtime knob
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# Upstream test: code/meep/tests/pml.cpp. It is the only dedicated coverage of
# the perfectly matched layer, and it measures the layer's reflection by
# running the same configuration twice, at two resolutions or two layer
# thicknesses, taking the Fourier amplitude of the field at a probe point in
# each, and forming the squared relative difference. Six sweeps run: a 1D
# resolution sweep over eight resolutions with a conductivity, a 2D TE sweep
# over four resolutions with conductivity, dispersion and an off-diagonal
# permittivity, a 1D thickness sweep over seven thicknesses, and cylindrical
# thickness sweeps at azimuthal index 0, 1 and 2. Upstream only the monotone
# decay of the reflection across each sweep survives, as a pass or a fail;
# ic/*/source.patch emits the Fourier amplitudes the reflections are computed
# from, the peak field each run reached, and the step counts, at %0.17g. The
# upstream comparisons are left in place and still abort the run if the
# reflection stops decaying.

# This check exposes no runtime knob: the graded values have to come from the
# window described in rubric.json "knobs", so there is nothing to scale without
# changing what is graded. SAB_BUILD_JOBS is a build-only setting and is printed
# as such, not as a knob, because build time is outside the suite budget.
HELP=""
build_setting() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; HELP+="build-only setting: $name=$default  $desc"$'\n'; }
build_setting SAB_BUILD_JOBS "4" "parallel compile jobs; affects build time only, never the graded values"
HELP+="runtime knobs: none. See the \"knobs\" field of rubric.json for why this check cannot be shortened."$'\n'
if [ "${1:-}" = "--help" ]; then printf '%s' "$HELP"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
[ -d "$CHECK_DIR/ic/$IC" ] || { echo "run.sh: no initial condition ic/$IC" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT

# Copy the tree under test. The images pre-build the pinned source in place, so
# the object files come with it and the build below is incremental: only the one
# patched file recompiles and links against the existing libmeep. Measured by
# running this script against a pre-built tree: 1 s of build for a copy of
# 245 MB, against 159 s for a clean rebuild of the same tree.
# -Rp, not -R: the copy must preserve modification times, or every object
# file in the pre-built tree looks newer or older than its source at random
# and make rebuilds far more than the one patched file.
cp -Rp "$SOURCE_DIR/." "$WORK/src"
cd "$WORK/src"
patch -p1 --silent < "$CHECK_DIR/ic/$IC/source.patch"

BUILD_START=$(date +%s)
if [ ! -f config.status ]; then
  sh autogen.sh --enable-shared --enable-ccache --with-libctl=/usr/share/libctl \
     --without-mpi --without-python --without-scheme >"$WORK/conf.log" 2>&1 \
     || { echo "run.sh: configure failed" >&2; tail -n 30 "$WORK/conf.log" >&2; exit 1; }
fi
make -C src -j"$SAB_BUILD_JOBS" >"$WORK/libmeep.log" 2>&1 \
  || { echo "run.sh: libmeep build failed" >&2; tail -n 30 "$WORK/libmeep.log" >&2; exit 1; }
make -C tests pml -j"$SAB_BUILD_JOBS" >"$WORK/test.log" 2>&1 \
  || { echo "run.sh: test build failed" >&2; tail -n 30 "$WORK/test.log" >&2; exit 1; }
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"

cd "$WORK/src/tests"
./pml >"$WORK/raw.txt" 2>&1 \
  || { echo "run.sh: pml exited nonzero" >&2; tail -n 20 "$WORK/raw.txt" >&2; exit 1; }

# Graded file: one value per line, each preceded by its name as a comment.
# Sorted by name so the order the configurations ran in cannot affect the
# comparison, and commented so the stock text loader reads exactly the numbers
# while the file stays readable.
grep '^SAB|' "$WORK/raw.txt" \
  | awk -F'|' '{printf "%s\t%s\n", $2, $3}' \
  | LC_ALL=C sort \
  | awk -F'\t' '{printf "# %s\n%s\n", $1, $2}' > "$OUT_DIR/pml.txt"
n=$(grep -vc '^#' "$OUT_DIR/pml.txt")
if [ "$n" -ne 224 ]; then
  echo "run.sh: expected 224 graded values, got $n" >&2; exit 1
fi

# This check has no window or resolution knob, and the resolutions and layer
# thicknesses are the sweep itself. The windows are not pinned to step counts
# here: each run ends when the field at the probe has decayed below a
# millionth of its peak, and the number of 50-time-unit blocks that takes is
# emitted as a graded integer so that a run which stops at a different point
# fails the comparison rather than being compared against a different window.
# This check is the most expensive in the task at about thirty-three seconds.
# SAB_BUILD_JOBS affects the build only, and build time is outside the suite
# budget.
