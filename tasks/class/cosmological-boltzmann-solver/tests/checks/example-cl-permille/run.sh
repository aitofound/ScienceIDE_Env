#!/usr/bin/env bash
# Check example-cl-permille: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     OPTIONAL: the nominal inputs on the alternative build (ALTBUILD below)
#   run.sh --help                       list the runtime knobs below, and the altbuild line when one is declared
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

KNOB_HELP="fixed official precision file cl_permille.pre; no runtime knob"
ALTBUILD="same pinned source with OPTFLAG=-O2"
if [ "${1:-}" = "--help" ]; then printf '%s\n' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"
if [ "$IC" = altbuild ]; then
  [ -n "$ALTBUILD" ] || { echo "run.sh: this check declares no alternative build" >&2; exit 2; }
  INPUTS=nominal
fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT

# Upstream test this check reproduces: ./class explanatory.ini cl_permille.pre
# (documented in the upstream README; explanatory.ini and cl_permille.pre are not vendored under code/class/ at this pin, so
# both ship under ic/, byte-identical to the pinned upstream files.)
BUILD_START=$(date +%s)
MAKE_ARGS=("CLASSDIR=$SOURCE_DIR")
CONFIG=default
if [ "$IC" = altbuild ]; then MAKE_ARGS+=("OPTFLAG=-O2"); CONFIG=O2; fi
# Cross-check build cache (best-effort, per skill "reuse to the best effort"): keyed by
# build configuration only (default vs. the -O2 altbuild), shared with every other plain
# "make <target>" check in this leaf. Populated once via the consolidating "libclass.a"
# target (TOOLS+SOURCE+EXTERNAL); each check still runs its own "make <target>" on the
# copy to compile and link whatever that target additionally needs. Installed atomically
# (build in a uniquely-named scratch dir, then rename into place). Self-contained: builds
# straight from SOURCE_DIR when SAB_BUILD_CACHE is unset (a solo/lint run).
if [ -n "${SAB_BUILD_CACHE:-}" ]; then
  CACHE_DIR="$SAB_BUILD_CACHE/$CONFIG"
  if [ ! -f "$CACHE_DIR/.sab-ready" ]; then
    TMP_BUILD="$SAB_BUILD_CACHE/.building-$CONFIG-$$"
    rm -rf "$TMP_BUILD"
    cp -R "$SOURCE_DIR/." "$TMP_BUILD"
    make -C "$TMP_BUILD" -j2 libclass.a "${MAKE_ARGS[@]}" >/dev/null
    touch "$TMP_BUILD/.sab-ready"
    rm -rf "$CACHE_DIR"
    mv "$TMP_BUILD" "$CACHE_DIR"
  fi
  cp -R "$CACHE_DIR/." "$WORK/src"
  touch "$WORK/src/build/"* "$WORK/src/libclass.a" 2>/dev/null || true
else
  cp -R "$SOURCE_DIR/." "$WORK/src"
fi
make -C "$WORK/src" -j2 class "${MAKE_ARGS[@]}" >/dev/null
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"
cp "$CHECK_DIR/ic/$INPUTS/explanatory.ini" "$WORK/src/explanatory.ini"
cp "$CHECK_DIR/ic/$INPUTS/cl_permille.pre" "$WORK/src/cl_permille.pre"
mkdir -p "$WORK/src/output"
set +e
(cd "$WORK/src" && ./class explanatory.ini cl_permille.pre) >"$WORK/run.log" 2>&1
DRIVER_RC=$?
set -e
if [ "$DRIVER_RC" -ne 0 ]; then
  echo "run.sh: driver failed (exit $DRIVER_RC); last 20 lines of its log:" >&2
  tail -n 20 "$WORK/run.log" >&2
  exit "$DRIVER_RC"
fi
F="$(find "$WORK/src/output" -maxdepth 1 -type f -name 'explanatory*_cl.dat' | LC_ALL=C sort | tail -1)"
[ -n "$F" ] && [ -f "$F" ] || { echo "run.sh: no cl output file matching explanatory*_cl.dat" >&2; exit 1; }
awk 'NF>=8 && $1 !~ /^#/ {for(i=1;i<=8;i++) if($i!="") printf "%.17g%s",$i,(i==8?"\n":" "); ok=1} END{if (!ok) exit 1}' "$F" >"$OUT_DIR/cl.txt"
F="$(find "$WORK/src/output" -maxdepth 1 -type f -name 'explanatory*_cl_lensed.dat' | LC_ALL=C sort | tail -1)"
[ -n "$F" ] && [ -f "$F" ] || { echo "run.sh: no cl_lensed output file matching explanatory*_cl_lensed.dat" >&2; exit 1; }
awk 'NF>=8 && $1 !~ /^#/ {for(i=1;i<=8;i++) if($i!="") printf "%.17g%s",$i,(i==8?"\n":" "); ok=1} END{if (!ok) exit 1}' "$F" >"$OUT_DIR/cl_lensed.txt"
F="$(find "$WORK/src/output" -maxdepth 1 -type f -name 'explanatory*_pk.dat' | LC_ALL=C sort | tail -1)"
[ -n "$F" ] && [ -f "$F" ] || { echo "run.sh: no pk output file matching explanatory*_pk.dat" >&2; exit 1; }
awk 'NF>=2 && $1 !~ /^#/ {printf "%.17g %.17g\n",$1,$2; ok=1} END{if (!ok) exit 1}' "$F" >"$OUT_DIR/pk.txt"
