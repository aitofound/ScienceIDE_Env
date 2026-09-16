#!/usr/bin/env bash
# Check testvshifte: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (see ALTBUILD below)
#   run.sh --help                       list the runtime/resource knobs and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree, code/spherepack),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# This check is one of 20 that build the same 27,964-line SPHEREPACK 3.2 static
# library. frames: not applicable, one solve -- this is a one-shot Fortran 77
# test driver (not a time-stepping code), so the five-frame rule does not apply.

KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_CPUS "1" "cores for the build and the run; testvshifte is a single-threaded Fortran 77 program whose grid size and loop count are compile-time constants of the official driver, so there is no runtime-scale knob to expose beyond this"
ALTBUILD="the same pinned SPHEREPACK source (code/spherepack/src) built with gfortran -fdefault-real-8 -O0 -std=legacy in place of the default -O2 -- a same-compiler fallback, since -O2 here already runs in IEEE mode (no -ffast-math), so there is no strict-IEEE flip to declare for this family"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
export LC_ALL=C
exec < /dev/null
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"
[ "$IC" != altbuild ] || INPUTS=nominal
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }

# Upstream test this check reproduces: code/spherepack/test/testvshifte.f  (run by \`make\` in test/)
# Within a run, please reuse the build to the best effort: libspherepack.a is built once per build
# configuration (flags only, never the initial condition) into a small on-disk cache and every
# check of this leaf's run reuses it; a check that finds nothing to reuse builds for itself.
FLAGS_TAG=nominal-O2
FFLAGS="-fdefault-real-8 -O2 -std=legacy"
if [ "$IC" = altbuild ]; then FLAGS_TAG=altbuild-O0; FFLAGS="-fdefault-real-8 -O0 -std=legacy"; fi
CACHE_ROOT="${SAB_BUILD_CACHE_ROOT:-/tmp/sab-spherepack-buildcache}"
LIBDIR="$CACHE_ROOT/$FLAGS_TAG"
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
BUILD_START=$(date +%s)
if [ -f "$LIBDIR/.ready" ] && [ -f "$LIBDIR/libspherepack.a" ]; then
  echo "SAB_BUILD_CACHE=hit dir=$LIBDIR"
  BUILD_SECONDS=0
else
  echo "SAB_BUILD_CACHE=miss dir=$LIBDIR"
  mkdir -p "$WORK/lib"
  ( cd "$WORK/lib"
    for f in "$SOURCE_DIR"/src/*.f; do
      base="$(basename "$f" .f)"
      gfortran $FFLAGS -J. -I. -c "$f" -o "$base.o"
    done
    ar rcs libspherepack.a *.o
  )
  mkdir -p "$LIBDIR"
  cp "$WORK/lib/libspherepack.a" "$LIBDIR/libspherepack.a.tmp.$$"
  mv "$LIBDIR/libspherepack.a.tmp.$$" "$LIBDIR/libspherepack.a"
  : > "$LIBDIR/.ready"
  BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
fi
echo "SAB_BUILD_SECONDS=$BUILD_SECONDS"

# Compile and run this check's driver from ic/$INPUTS (byte-identical to the official test for
# nominal; the variant differs by a fixed 2-ulp factor on the active input identified in the
# rubric's "variant" field; altbuild reruns the nominal driver source on the -O0 library above).
cp "$CHECK_DIR/ic/$INPUTS/testvshifte.f" "$WORK/testvshifte.f"
gfortran $FFLAGS -I"$LIBDIR" "$WORK/testvshifte.f" "$LIBDIR/libspherepack.a" -o "$WORK/testvshifte.exe"
( cd "$WORK" && ./testvshifte.exe > "$OUT_DIR/stdout.log" 2>&1 )
