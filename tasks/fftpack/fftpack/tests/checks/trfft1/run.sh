#!/usr/bin/env bash
# Check trfft1: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the -O0 same-compiler fallback build (ALTBUILD below)
#   run.sh --help                       list the runtime and resource knobs below, and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# This is a one-shot round-trip transform, not a time-stepping code.
# frames: not applicable, one solve.

KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_N "1000" "the transform length N (trfft1 test parameter); LENSAV and LENWRK are recomputed from it (see the official test's commented formula); the graded default is the upstream value 1000. Iteration only: values other than the default generate an unofficial input vector."
knob SAB_CPUS "1" "cores used to compile the library (parallel make -j); the transform itself is single-threaded and its summation order does not depend on this"
# Alternative build: FFTPACK's default build is already IEEE (-O2, no fast-math), so the
# family ruling declares the same-compiler -O0 fallback as the alternative (no strict-IEEE
# flip exists for a build that is already IEEE). Same pinned source, same -fdefault-real-8
# -std=legacy flags, same deck.
ALTBUILD="the same pinned source built with gfortran -O0 instead of -O2 (same -fdefault-real-8 -std=legacy flags); the same-compiler fallback per the NCAR-classic-libraries altbuild family ruling"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; echo "frames: not applicable, one solve"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"
FFLAGS="-fdefault-real-8 -O2 -std=legacy"
if [ "$IC" = altbuild ]; then
  [ -n "$ALTBUILD" ] || { echo "run.sh: this check declares no alternative build" >&2; exit 2; }
  INPUTS=nominal
  FFLAGS="-fdefault-real-8 -O0 -std=legacy"
fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
case "$SAB_CPUS" in ''|*[!0-9]*) echo "run.sh: SAB_CPUS must be a positive integer" >&2; exit 2 ;; esac
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT

# Upstream test this check reproduces: code/fftpack/test/trfft1.f, with
# RANDOM_SEED()/RANDOM_NUMBER() replaced by a read of the fixed ic/ vector
# (see driver.f and comment/README.md and this check's README).
DRIVER_SRC="$CHECK_DIR/driver.f"
if [ "$SAB_N" != "1000" ]; then
  DRIVER_SRC="$WORK/driver-scaled.f"
  python3 - "$CHECK_DIR/driver.f" "$SAB_N" "$DRIVER_SRC" <<'PY'
import math, re, sys
src_path, n_str, out_path = sys.argv[1], sys.argv[2], sys.argv[3]
n = int(n_str)
if n < 4:
    sys.exit("run.sh: SAB_N must be at least 4")
lensav = n + int(math.log(n)/math.log(2)) + 4
lenwrk = n
text = open(src_path, encoding="ascii").read()
text = re.sub(r"PARAMETER\(N=\d+\)", "PARAMETER(N=%d)" % n, text, count=1)
text = re.sub(r"PARAMETER\(LENSAV=\s*\d+\)", "PARAMETER(LENSAV=%d)" % lensav, text, count=1)
text = re.sub(r"PARAMETER\(LENWRK=\d+\)", "PARAMETER(LENWRK=%d)" % lenwrk, text, count=1)
open(out_path, "w", encoding="ascii").write(text)
PY
fi

# Within a run, please reuse the build to the best effort: every fftpack check builds
# the SAME static library from the SAME pinned source, so a build under these exact
# flags (nominal -O2, or altbuild -O0) is cached under a fixed, flag-keyed directory
# inside this container and reused by every later check of the same run; the library
# takes about 1-2s to build, so a cache miss is inconsequential against the suite budget.
BUILD_START=$(date +%s)
CACHE_ROOT="${TMPDIR:-/tmp}/sab-fftpack-buildcache"
CACHE_KEY="$(printf '%s' "$FFLAGS" | sha256sum | cut -c1-16)"
CACHE_DIR="$CACHE_ROOT/$CACHE_KEY"
mkdir -p "$CACHE_ROOT"
if [ ! -f "$CACHE_DIR/libfftpack.a" ]; then
  BUILD_TMP="$(mktemp -d)"
  cp -R "$SOURCE_DIR/." "$BUILD_TMP/src"
  chmod -R u+w "$BUILD_TMP/src"
  mkdir -p "$BUILD_TMP/src/lib" "$BUILD_TMP/src/objs"
  ( cd "$BUILD_TMP/src" && make -C src -j"$SAB_CPUS" MAKE=make AR=ar \
      F90="gfortran $FFLAGS -J../lib -I../lib" CPP="gfortran -cpp -E" ) >"$WORK/build.log" 2>&1 \
    || { tail -60 "$WORK/build.log" >&2; echo "run.sh: fftpack library build failed" >&2; rm -rf "$BUILD_TMP"; exit 3; }
  mkdir -p "$CACHE_DIR.tmp.$$"
  cp "$BUILD_TMP/src/lib/libfftpack.a" "$CACHE_DIR.tmp.$$/libfftpack.a"
  mv -T "$CACHE_DIR.tmp.$$" "$CACHE_DIR" 2>/dev/null || rm -rf "$CACHE_DIR.tmp.$$"   # another check may have published it first (mv -T refuses a non-empty destination); either copy is identical
  rm -rf "$BUILD_TMP"
  echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"
else
  echo "SAB_BUILD_SECONDS=0"
fi
[ -f "$CACHE_DIR/libfftpack.a" ] || { echo "run.sh: no libfftpack.a in cache after build" >&2; exit 3; }

cp "$DRIVER_SRC" "$WORK/driver.f"
cp "$CHECK_DIR/ic/$INPUTS/input.txt" "$WORK/input.txt"
if [ "$DRIVER_SRC" = "$WORK/driver-scaled.f" ]; then
  python3 - "$SAB_N" "$WORK/input.txt" <<'PY'
import numpy as np, sys
n = int(sys.argv[1])
r = np.random.default_rng(20260916).random(n)
with open(sys.argv[2], "w") as f:
    for v in r:
        f.write("%.17e\n" % v)
PY
fi

( cd "$WORK" && gfortran $FFLAGS driver.f -o driver.exe -L"$CACHE_DIR" -lfftpack ) >"$WORK/compile.log" 2>&1 \
  || { cat "$WORK/compile.log" >&2; echo "run.sh: driver compile failed" >&2; exit 3; }
( cd "$WORK" && ./driver.exe ) >"$WORK/driver.log" 2>&1 \
  || { cat "$WORK/driver.log" >&2; echo "run.sh: driver run failed" >&2; exit 1; }
[ -s "$WORK/roundtrip_errors.txt" ] && [ -s "$WORK/transformed.txt" ] \
  || { echo "run.sh: driver did not write both graded files" >&2; exit 1; }
cp "$WORK/roundtrip_errors.txt" "$OUT_DIR/roundtrip_errors.txt"
cp "$WORK/transformed.txt" "$OUT_DIR/transformed.txt"
