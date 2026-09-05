#!/usr/bin/env bash
# Check rcwa-evanescent-field-profile: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (ALTBUILD below)
#   run.sh --help                       list the runtime knobs below, and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_NUMG "1" "NumBasis, the Fourier basis size. The layer eigenproblem is 2*NumBasis square and the eigensolve is O(NumBasis^3), so this is the check's cost dial. The graded value 1 is upstream's own setting."
# Alternative build (skill 5.8.0+): one legitimately different build of the same pinned source,
# used by `run.sh altbuild` to run the NOMINAL inputs so that self-validation can measure this
# check's floor. Every check already rebuilds S4 from source in its own scratch copy, so the
# alternative build is the same make invocation with CC/CXX swapped: no second tree is needed.
ALTBUILD="clang and clang++ (Debian bookworm's clang package) instead of gcc and g++, on the same pinned source with the same explicitly pinned flags and the same undefined HAVE_LAPACK, so the in-tree eigensolver and S-matrix linear algebra this module owns are recompiled by a second toolchain rather than replaced by a library"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"; SAB_CC=gcc; SAB_CXX=g++
if [ "$IC" = altbuild ]; then
  INPUTS=nominal; SAB_CC=clang; SAB_CXX=clang++
  command -v "$SAB_CC" >/dev/null 2>&1 && command -v "$SAB_CXX" >/dev/null 2>&1 \
    || { echo "run.sh: the alternative build needs clang and clang++; both Dockerfiles install them" >&2; exit 2; }
fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

# Upstream test this check reproduces: code/s4/examples/0d/fabry_perot/tir_field.lua
BUILD_START=$(date +%s)
# Flags are pinned explicitly rather than inherited from Makefile.Linux, which
# appends -march=native (not reproducible across machines) and
# -fcx-limited-range (drops the NaN-rescue branch of complex multiply and
# divide, in a solver built entirely on std::complex<double>). HAVE_LAPACK is
# deliberately NOT defined, which is upstream's default on Linux and keeps the
# O(N^3) eigendecomposition inside S4/RNP/Eigensystems.cpp - a path this module
# owns - rather than delegating it to a system LAPACK the port could merely swap.
# -Wno-error=int-conversion is required: main_lua.c:2207 passes an S4_LayerID
# (an int) where Simulation_GetAmplitudes wants an S4_Layer*, which clang 16+
# and gcc 14+ reject outright. No check calls GetAmplitudes.
# CC/CXX are given on the make command line so they override Makefile.Linux's `CC = gcc` /
# `CXX = g++`; for the nominal and variant runs they name exactly those two, and for altbuild
# they name clang/clang++ (the only difference between the two builds).
make -C "$WORK/src" build/S4 \
  CC="$SAB_CC" CXX="$SAB_CXX" \
  CFLAGS="-O2 -fPIC -Wall -Wno-error=int-conversion" \
  CXXFLAGS="-O2 -fPIC -Wall -std=c++11" \
  CPPFLAGS="-IS4 -IS4/RNP -IS4/kiss_fft" \
  LUA_INC="-I/usr/include/lua5.2" LUA_LIB="-llua5.2" \
  LA_LIBS="-llapack -lblas" OBJDIR=build >"$WORK/build.log" 2>&1 \
  || { echo "run.sh: build failed ($SAB_CC/$SAB_CXX)" >&2; tail -40 "$WORK/build.log" >&2; exit 1; }
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only

# Stage the deck and apply the knob. The deck already carries the graded
# NumBasis; this rewrite is a no-op unless SAB_NUMG was overridden.
sed "s/S:SetNumG([0-9]*)/S:SetNumG($SAB_NUMG)/" "$CHECK_DIR/ic/$INPUTS/input.lua" > "$WORK/input.lua"
(cd "$WORK" && "$WORK/src/build/S4" input.lua) > "$OUT_DIR/output.txt"
