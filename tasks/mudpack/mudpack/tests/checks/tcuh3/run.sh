#!/usr/bin/env bash
# Check tcuh3: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on a -O0 build of the same pinned source
#   run.sh --help                       list the runtime/resource knobs and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree, code/mudpack),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# frames: not applicable, one solve. MUDPACK's multigrid solvers iterate to a fixed cycle
# count (maxcy), not a time-stepping loop; there is one graded solve per initial condition.

KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_CPUS "1" "cores used; this is a fixed-size serial multigrid solve and does not scale with cores. MUDPACK ships OpenMP directives on its relaxation loops (515 lines across the library) but this build never passes -fopenmp, so they are inactive and the summation order is fixed; a threaded build is out of scope for this check (see comment/README.md)."
ALTBUILD="the same pinned source (driver.f and code/mudpack/src) built with gfortran -O0 instead of -O2, same -fdefault-real-8 -std=legacy; MUDPACK's default build is already IEEE (no fast-math to flip), so -O0 is the same-compiler fallback axis"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?SOURCE_DIR must name the read-only source tree}"
: "${OUT_DIR:?OUT_DIR must name the empty graded-output directory}"
: "${CHECK_DIR:?CHECK_DIR must name this check directory}"
INPUTS="$IC"
OPT="-O2"
if [ "$IC" = altbuild ]; then INPUTS=nominal; OPT="-O0"; fi
case "$INPUTS" in nominal|variant) ;; *) echo "run.sh: initial condition must be nominal, variant or altbuild" >&2; exit 2 ;; esac
[ -f "$CHECK_DIR/ic/$INPUTS/driver.f" ] || { echo "run.sh: missing ic/$INPUTS/driver.f" >&2; exit 2; }
[ -d "$SOURCE_DIR/src" ] || { echo "run.sh: SOURCE_DIR does not look like code/mudpack (no src/)" >&2; exit 2; }

WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
LIB="$WORK/lib"; mkdir -p "$LIB"
F90=(gfortran -fdefault-real-8 "$OPT" -std=legacy -J"$LIB" -I"$LIB")

# Build the whole MUDPACK static library from the untouched pinned source (54 Fortran 77/90
# files). Never -fopenmp: MUDPACK's OpenMP directives are opt-in and left inactive so the
# multigrid summation order stays fixed across builds and hosts.
#
# Build once per configuration per selfcheck (SPEC "please reuse to best effort"): the cache
# is keyed by build flags and source identity ONLY, never by the initial condition, so the
# SAME -O2 library built for `nominal` is reused for `variant`, and `altbuild`'s -O0 library
# is its own cache entry, built once and reused by every check that runs it. solve.sh mounts
# one cache root shared across the nominal, variant and altbuild solves of one selfcheck (see
# comment/README.md "## Build"). Pattern copied from tasks/swmf/swmf-batsrus/tests/checks/*/run.sh.
build_source() {
  ( cd "$LIB" && "${F90[@]}" -c "$SOURCE_DIR"/src/*.f ) >"$WORK/lib.log" 2>&1 \
    || { echo "run.sh: MUDPACK library build failed" >&2; tail -n 60 "$WORK/lib.log" >&2 || true; exit 1; }
  ar rcs "$LIB/libmudpack.a" "$LIB"/*.o >>"$WORK/lib.log" 2>&1
}

CACHE_ENABLED=0
if [ -n "${SAB_BUILD_CACHE_ROOT:-}" ] && [ -n "${SAB_SOURCE_FINGERPRINT:-}" ]; then CACHE_ENABLED=1; fi

if [ "$CACHE_ENABLED" -eq 1 ]; then
  COMPILER_VERSION="$(gfortran --version 2>&1 | head -1 || true)"
  AR_VERSION="$(ar --version 2>&1 | head -1 || true)"
  BUILD_FINGERPRINT="$(printf '%s\0' \
      "cache-schema=mudpack-build-v1" \
      "task=mudpack" \
      "source-fingerprint=$SAB_SOURCE_FINGERPRINT" \
      "fflags=-fdefault-real-8 $OPT -std=legacy" \
      "compiler-version=$COMPILER_VERSION" \
      "ar-version=$AR_VERSION" \
      "machine=$(uname -m)" | sha256sum | cut -d' ' -f1)"
  CACHE_DIR="$SAB_BUILD_CACHE_ROOT/mudpack/$BUILD_FINGERPRINT"
  CACHE_LIB="$CACHE_DIR/libmudpack.a"
  CACHE_DIGEST="$CACHE_DIR/lib.sha256"
  CACHE_READY="$CACHE_DIR/ready.sha256"
  CACHE_HIT=0
  if [ -f "$CACHE_LIB" ] && [ -f "$CACHE_DIGEST" ] && [ -f "$CACHE_READY" ]; then
    READY_FP="$(cat "$CACHE_READY" 2>/dev/null || true)"
    EXPECTED="$(cat "$CACHE_DIGEST" 2>/dev/null || true)"
    ACTUAL="$(sha256sum "$CACHE_LIB" 2>/dev/null | cut -d' ' -f1 || true)"
    if [ "$READY_FP" = "$BUILD_FINGERPRINT" ] && [ -n "$EXPECTED" ] && [ "$EXPECTED" = "$ACTUAL" ]; then
      CACHE_HIT=1
    fi
  fi
  if [ "$CACHE_HIT" -eq 1 ]; then
    if cp "$CACHE_LIB" "$LIB/libmudpack.a"; then
      echo "SAB_BUILD_CACHE=hit fingerprint=$BUILD_FINGERPRINT opt=$OPT variant=$IC"
      BUILD_SECONDS=0
    else
      echo "run.sh: cached libmudpack.a could not be copied; rebuilding" >&2
      CACHE_HIT=0
    fi
  fi
  if [ "$CACHE_HIT" -eq 0 ]; then
    echo "SAB_BUILD_CACHE=miss fingerprint=$BUILD_FINGERPRINT opt=$OPT variant=$IC"
    BUILD_START=$(date +%s)
    build_source
    BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
    if mkdir -p "$CACHE_DIR" \
        && printf 'building\n' > "$CACHE_READY" \
        && cp "$LIB/libmudpack.a" "$CACHE_LIB" \
        && sha256sum "$CACHE_LIB" | cut -d' ' -f1 > "$CACHE_DIGEST" \
        && printf '%s\n' "$BUILD_FINGERPRINT" > "$CACHE_READY"; then
      echo "SAB_BUILD_CACHE=published fingerprint=$BUILD_FINGERPRINT opt=$OPT variant=$IC"
    else
      echo "run.sh: warning: could not publish build cache for $BUILD_FINGERPRINT; using local build" >&2
    fi
  fi
else
  echo "SAB_BUILD_CACHE=disabled reason=missing solve-scoped cache root or source fingerprint"
  BUILD_START=$(date +%s)
  build_source
  BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
fi
echo "SAB_BUILD_SECONDS=$BUILD_SECONDS"   # zero means no compile on a verified cache hit

# Compile and link this check's driver: a copy of the official test/tcuh3.f with one
# output block added (see README.md). Never links against code/mudpack/test/ or edits SOURCE_DIR.
# The driver link is per-check (it is not the shared library) and always runs, cache or no
# cache; it takes a fraction of a second and is not counted in SAB_BUILD_SECONDS or cached.
if ! "${F90[@]}" "$CHECK_DIR/ic/$INPUTS/driver.f" -o "$WORK/driver.exe" -L"$LIB" -lmudpack >"$WORK/link.log" 2>&1; then
  echo "run.sh: driver build failed" >&2
  tail -n 60 "$WORK/link.log" >&2 || true
  exit 1
fi

cd "$WORK"
if ! ./driver.exe >"$WORK/stdout.log" 2>&1; then
  echo "run.sh: driver run failed" >&2
  tail -n 80 "$WORK/stdout.log" >&2 || true
  exit 1
fi
[ -s "$WORK/sab_solution.dat" ] || { echo "run.sh: driver did not write sab_solution.dat" >&2; exit 1; }
cp "$WORK/sab_solution.dat" "$OUT_DIR/solution.dat"
