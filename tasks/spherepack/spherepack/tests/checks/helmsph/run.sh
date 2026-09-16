#!/usr/bin/env bash
# Check helmsph: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (see ALTBUILD below)
#   run.sh --help                       list the runtime/resource knobs and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree, code/spherepack),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# helmsph is one of 20 checks that build the same 27,964-line SPHEREPACK 3.2
# static library. It solves one Helmholtz equation on a 19x36 (ten-degree)
# grid with a single direct spectral solve -- not a time-stepping code and
# not iterative, so, unlike advec and shallow, there is no natural window to
# scale. frames: not applicable, one solve.

KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_CPUS "1" "cores for the build and the run; this driver is a single direct spectral solve on a compile-time 19x36 grid, so there is no runtime-scale knob to expose beyond this"
ALTBUILD="the same pinned SPHEREPACK source (code/spherepack/src) built with gfortran -fdefault-real-8 -O0 -std=legacy in place of the default -O2 (same-compiler fallback, since -O2 here already runs in IEEE mode with no fast-math)"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
export LC_ALL=C
exec < /dev/null
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"
[ "$IC" != altbuild ] || INPUTS=nominal
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }

# Upstream test this check reproduces: code/spherepack/src/helmsph.f (the Helmholtz
# example the SPHEREPACK abstract advertises; no Makefile target runs it, and upstream
# ships no reference output file -- only the prose in the file's own header comment,
# "maximum error = 0.114E-12 *** (64 BIT)" -- the pinned build generates this check's
# reference). Within a run, please reuse the build to the best effort: libspherepack.a is
# built once per build configuration (flags only) into a small on-disk cache shared by
# every check of this leaf's run; a check that finds nothing to reuse builds for itself.
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

# ic/nominal/helmsph.f is the official example with one addition (a dump of the solution
# field u(nlat,nlon) to sab_field.out); ic/variant/helmsph.f also perturbs the driver's
# own pi by 2 ulps (see rubric.json "variant").
cp "$CHECK_DIR/ic/$INPUTS/helmsph.f" "$WORK/helmsph.f"
gfortran $FFLAGS -I"$LIBDIR" "$WORK/helmsph.f" "$LIBDIR/libspherepack.a" -o "$WORK/helmsph.exe"
( cd "$WORK" && ./helmsph.exe > stdout.log 2>&1 )
cp "$WORK/stdout.log" "$OUT_DIR/stdout.log"
[ -f "$WORK/sab_field.out" ] || { echo "run.sh: helmsph.exe did not write sab_field.out" >&2; tail -40 "$WORK/stdout.log" >&2; exit 1; }
cp "$WORK/sab_field.out" "$OUT_DIR/sab_field.out"
