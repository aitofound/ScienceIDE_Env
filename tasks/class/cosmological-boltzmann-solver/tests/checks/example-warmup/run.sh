#!/usr/bin/env bash
# Check example-warmup: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     OPTIONAL: the nominal inputs on the alternative build (ALTBUILD below)
#   run.sh --help                       list the runtime knobs below, and the altbuild line when one is declared
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

KNOB_HELP="fixed official example script warmup.py; no runtime knob"
ALTBUILD="same pinned source with OPTFLAG=-O2 (make libclass.a OPTFLAG=-O2, then build classy against it)"
if [ "${1:-}" = "--help" ]; then printf '%s\n' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"
MAKE_ARGS=("CLASSDIR=$SOURCE_DIR")
if [ "$IC" = altbuild ]; then
  [ -n "$ALTBUILD" ] || { echo "run.sh: this check declares no alternative build" >&2; exit 2; }
  INPUTS=nominal
  MAKE_ARGS+=("OPTFLAG=-O2")
fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT

# Upstream test this check reproduces: code/class/scripts/warmup.py (also notebooks/warmup.ipynb)
CONFIG=classy-default
[ "$IC" = altbuild ] && CONFIG=classy-O2
BUILD_START=$(date +%s)
# Cross-check build cache (best-effort, per skill "reuse to the best effort"): keyed by
# build configuration only (the classy extension layered on default vs. the -O2 altbuild),
# shared with every other classy-based check in this leaf (14 example checks, hmcode,
# python-wrapper). Populated once via libclass.a + "python3 setup.py build_ext --inplace"
# (the ~48s classy compile this addendum measured as the dominant repeated cost). Installed
# atomically (build in a uniquely-named scratch dir, then rename into place) so a corrupted
# partial copy can never be read as a hit. The two symlinks are absolute (point at this
# check's own $WORK/src), so they are recreated fresh after every cache copy rather than
# copied verbatim; touch after copy defeats cp's fresh mtimes so setup.py's own incremental
# build (harmless if it reruns) does not think the copied .o/.so files are stale. Self-
# contained: builds straight from SOURCE_DIR when SAB_BUILD_CACHE is unset (a solo/lint run).
if [ -n "${SAB_BUILD_CACHE:-}" ]; then
  CACHE_DIR="$SAB_BUILD_CACHE/$CONFIG"
  if [ ! -f "$CACHE_DIR/.sab-ready" ]; then
    TMP_BUILD="$SAB_BUILD_CACHE/.building-$CONFIG-$$"
    rm -rf "$TMP_BUILD"
    cp -R "$SOURCE_DIR/." "$TMP_BUILD"
    rm -rf "$TMP_BUILD/build" "$TMP_BUILD/libclass.a"
    ln -s "$TMP_BUILD/external" "$TMP_BUILD/python/external"
    ln -s "$TMP_BUILD/include" "$TMP_BUILD/python/include"
    make -C "$TMP_BUILD" -j2 libclass.a "${MAKE_ARGS[@]}" >/dev/null
    if ! (cd "$TMP_BUILD/python" && python3 setup.py build_ext --inplace) >"$TMP_BUILD/build.log" 2>&1; then
      echo "run.sh: classy build failed populating the cache; last 20 lines of its log:" >&2
      tail -n 20 "$TMP_BUILD/build.log" >&2
      rm -rf "$TMP_BUILD"
      exit 1
    fi
    touch "$TMP_BUILD/.sab-ready"
    rm -rf "$CACHE_DIR"
    mv "$TMP_BUILD" "$CACHE_DIR"
  fi
  cp -R "$CACHE_DIR/." "$WORK/src"
  rm -f "$WORK/src/python/external" "$WORK/src/python/include"
  ln -s "$WORK/src/external" "$WORK/src/python/external"
  ln -s "$WORK/src/include" "$WORK/src/python/include"
  touch "$WORK/src/build/"* "$WORK/src/libclass.a" "$WORK/src/python/"*.so "$WORK/src/python/classy.c" 2>/dev/null || true
else
  cp -R "$SOURCE_DIR/." "$WORK/src"
  rm -rf "$WORK/src/build" "$WORK/src/libclass.a"
  ln -s "$WORK/src/external" "$WORK/src/python/external"
  ln -s "$WORK/src/include" "$WORK/src/python/include"
  make -C "$WORK/src" -j2 libclass.a "${MAKE_ARGS[@]}" >/dev/null
  if ! (cd "$WORK/src/python" && python3 setup.py build_ext --inplace) >"$WORK/build.log" 2>&1; then
    echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"
    echo "run.sh: classy build failed; last 20 lines of its log:" >&2
    tail -n 20 "$WORK/build.log" >&2
    exit 1
  fi
fi
mkdir -p "$WORK/src/output"
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"

set +e
(cd "$WORK/src/python" && PYTHONPATH="$WORK/src/python" MPLBACKEND=Agg python3 -B "$CHECK_DIR/harness.py" \
  "$CHECK_DIR/ic/$INPUTS/params.json" "$OUT_DIR/observable.json") >"$WORK/harness.log" 2>&1
HARNESS_RC=$?
set -e
if [ "$HARNESS_RC" -ne 0 ]; then
  echo "run.sh: harness failed (exit $HARNESS_RC); last 20 lines of its log:" >&2
  tail -n 20 "$WORK/harness.log" >&2
  exit "$HARNESS_RC"
fi
[ -s "$OUT_DIR/observable.json" ]
