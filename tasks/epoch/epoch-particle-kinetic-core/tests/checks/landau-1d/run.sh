#!/usr/bin/env bash
# Check landau-1d: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     OPTIONAL: the nominal inputs on the alternative build (ALTBUILD below)
#   run.sh --help                       list the runtime knobs below, and the altbuild line when one is declared
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_NSTEPS=100 sab.py task selfcheck ...
# Parallel build jobs default to the CPUs this container may use (cgroup v2 cpu.max), not the host count.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_NSTEPS "20000" "graded window in time steps (upstream deck: t_end = 3.0e-1, about 75800 steps); runtime scales linearly"
knob SAB_FRAMES "4" "graded dumps after the one at step 0; output volume only"
knob SAB_NX "400" "cells along x (upstream: 400); runtime scales linearly"
knob SAB_NPART "1600" "macroparticles per species (upstream: 1600); runtime scales linearly"
knob SAB_NPROCX "2" "MPI ranks along x. Changing it changes the seed (7842432 + rank) and the per-rank particle counts, so the run is a different, equally valid realisation and its output is not comparable with the graded default"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the one build of the pinned source (default: the CPUs allowed to this container); each job needs about 0.3 GB"
# Alternative build, OPTIONAL: the same pinned source and deck on a legitimately different,
# stricter build than the stock release build (see ALTBUILD below). `run.sh altbuild` runs
# ic/nominal on that build; selfcheck measures this check's floor from it.
ALTBUILD="epoch1d/Makefile's stock gfortran FFLAGS line changed from -O3 -g -std=f2003 to -O0 -g -std=f2003 in the scratch copy only (sed on the copied epoch1d/Makefile, never SOURCE_DIR): the same pinned source and deck at zero optimisation instead of the stock -O3 release build. (EPOCH's own MODE=debug profile was tried first and SIGFPEs on this leaf's decks -- -ffpe-trap=invalid,zero,overflow catching a legitimate operation in the pinned source -- so this leaf uses the traps-free build-flag fallback instead.)"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

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

# A produce invocation supplies SAB_BUILD_CACHE and SAB_SOURCE_ID. The cache
# contains only the compiled source tree, never a deck, run output, extracted
# result or verifier record. Without those variables this driver is deliberately
# cold and independently builds on every invocation.
BUILD_COMPILER="${SAB_COMPILER:-gfortran}"
BUILD_PRECISION="${SAB_PRECISION:-double}"
BUILD_DIM="epoch1d"
BUILD_FLAGS="-O3 -g -std=f2003"
BUILD_DEFINE=""
BUILD_ALTBUILD="none"
if [ "$IC" = altbuild ]; then
  BUILD_FLAGS="-O0 -g -std=f2003"
  BUILD_ALTBUILD="$ALTBUILD"
fi
BUILD_COMPILER_VERSION="$("$BUILD_COMPILER" --version 2>/dev/null | head -n 1 || true)"
BUILD_SOURCE_ID="${SAB_SOURCE_ID:-}"
if [ -z "$BUILD_SOURCE_ID" ] && [ -n "${SAB_BUILD_CACHE:-}" ]; then
  echo "run.sh: SAB_SOURCE_ID is missing; disabling the cache for this independent invocation" >&2
fi
BUILD_ID="source=${BUILD_SOURCE_ID:-absent}|config=$BUILD_DIM|compiler=$BUILD_COMPILER|compiler-version=$BUILD_COMPILER_VERSION|precision=$BUILD_PRECISION|define=$BUILD_DEFINE|altbuild=$BUILD_ALTBUILD|flags=$BUILD_FLAGS"
hash_file() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}'; else shasum -a 256 "$1" | awk '{print $1}'; fi
}
hash_stdin() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum; else shasum -a 256; fi
}
BUILD_KEY="$(printf '%s' "$BUILD_ID" | hash_stdin | awk '{print $1}')"
BUILD_CACHE_ROOT=""
if [ -n "$BUILD_SOURCE_ID" ]; then
  BUILD_CACHE_ROOT="${SAB_BUILD_CACHE:-}"
fi
BUILD_CACHE_DIR="${BUILD_CACHE_ROOT:+$BUILD_CACHE_ROOT/$BUILD_KEY}"
BUILD_CACHE_SRC="$BUILD_CACHE_DIR/src"
BUILD_CACHE_BINARY="$BUILD_CACHE_SRC/$BUILD_DIM/bin/$BUILD_DIM"
BUILD_CACHE_IDENTITY="$BUILD_CACHE_DIR/identity"
BUILD_CACHE_DIGEST="$BUILD_CACHE_DIR/binary.sha256"
BUILD_CACHE_COMPLETE="$BUILD_CACHE_DIR/complete"
BUILD_REUSED=0
if [ -n "$BUILD_CACHE_DIR" ] \
    && [ -f "$BUILD_CACHE_COMPLETE" ] \
    && [ -f "$BUILD_CACHE_IDENTITY" ] \
    && [ "$(cat "$BUILD_CACHE_IDENTITY" 2>/dev/null || true)" = "$BUILD_ID" ] \
    && [ -x "$BUILD_CACHE_BINARY" ] \
    && [ -f "$BUILD_CACHE_DIGEST" ]; then
  EXPECTED_DIGEST="$(cat "$BUILD_CACHE_DIGEST" 2>/dev/null || true)"
  ACTUAL_DIGEST="$(hash_file "$BUILD_CACHE_BINARY" 2>/dev/null || true)"
  if [ -n "$EXPECTED_DIGEST" ] && [ "$EXPECTED_DIGEST" = "$ACTUAL_DIGEST" ]; then
    if cp -R "$BUILD_CACHE_SRC/." "$WORK/src" \
        && [ -x "$WORK/src/$BUILD_DIM/bin/$BUILD_DIM" ]; then
      BUILD_REUSED=1
      echo "SAB_BUILD_CACHE=hit key=$BUILD_KEY dimension=$BUILD_DIM"
    else
      echo "run.sh: cached build entry is incomplete; falling back to an independent build" >&2
    fi
  else
    echo "run.sh: cached build digest mismatch; falling back to an independent build" >&2
  fi
fi
if [ "$BUILD_REUSED" -eq 0 ]; then
  echo "SAB_BUILD_CACHE=miss key=$BUILD_KEY dimension=$BUILD_DIM"
  cp -R "$SOURCE_DIR/." "$WORK/src"
  # Alternative build: edit only the scratch copy, never SOURCE_DIR.
  if [ "$IC" = altbuild ]; then
    MAKEFILE="$WORK/src/$BUILD_DIM/Makefile"
    BEFORE=$(grep -c '^  FFLAGS = -O3 -g -std=f2003$' "$MAKEFILE" || true)
    [ "$BEFORE" -eq 1 ] || { echo "run.sh: altbuild FFLAGS pattern matched $BEFORE lines in $BUILD_DIM/Makefile, expected 1" >&2; exit 2; }
    sed -i 's/^  FFLAGS = -O3 -g -std=f2003$/  FFLAGS = -O0 -g -std=f2003/' "$MAKEFILE"
    AFTER=$(grep -c '^  FFLAGS = -O0 -g -std=f2003$' "$MAKEFILE" || true)
    [ "$AFTER" -eq 1 ] || { echo "run.sh: altbuild FFLAGS edit did not take in $BUILD_DIM/Makefile" >&2; exit 2; }
  fi

# Upstream test this check reproduces: code/epoch/epoch1d/tests/test_landau.py
# Build: the stock gfortran build of epoch1d, no DEFINE (triangle shape function, Boris pusher, per-particle weight); altbuild changes the copied Makefile's FFLAGS from -O3 to -O0 instead (see ALTBUILD)
  BUILD_START=$(date +%s.%N)
  if [ -n "$BUILD_DEFINE" ]; then
    make -C "$WORK/src/$BUILD_DIM" COMPILER="$BUILD_COMPILER" DEFINE="$BUILD_DEFINE" -j"$SAB_MAKE_JOBS" > "$WORK/make.log" 2>&1
  else
    make -C "$WORK/src/$BUILD_DIM" COMPILER="$BUILD_COMPILER" -j"$SAB_MAKE_JOBS" > "$WORK/make.log" 2>&1
  fi
  BUILD_SECONDS="$(awk -v start="$BUILD_START" -v end="$(date +%s.%N)" 'BEGIN { print end - start }')"
  [ -x "$WORK/src/$BUILD_DIM/bin/$BUILD_DIM" ] || { echo "run.sh: build did not produce $BUILD_DIM/bin/$BUILD_DIM" >&2; exit 1; }
  if [ -n "$BUILD_CACHE_DIR" ]; then
    mkdir -p "$BUILD_CACHE_DIR"
    cp -R "$WORK/src/." "$BUILD_CACHE_SRC"
    [ -x "$BUILD_CACHE_BINARY" ] || { echo "run.sh: refusing to publish incomplete $BUILD_DIM cache artifact" >&2; exit 1; }
    hash_file "$BUILD_CACHE_BINARY" > "$BUILD_CACHE_DIGEST"
    printf '%s\n' "$BUILD_ID" > "$BUILD_CACHE_IDENTITY"
    printf '%s\n' complete > "$BUILD_CACHE_COMPLETE"
    echo "SAB_BUILD_CACHE=published key=$BUILD_KEY dimension=$BUILD_DIM"
  fi
else
  BUILD_SECONDS=0
fi
echo "SAB_BUILD_SECONDS=$BUILD_SECONDS"   # zero only after a validated complete-cache reuse

if [ "$IC" = variant ]; then SAB_NPROCX=4; fi  # identical physics, independent rank-seeded realisation

# The deck of this initial condition with the knobs written into it.
mkdir -p "$WORK/run"
# The graded window is a fixed number of steps (nsteps) dumped at a fixed step
# cadence (nstep_snapshot), so the graded frames do not depend on the time step.
cadence="$(python3 -c "import sys; print(max(1, int(sys.argv[1]) // int(sys.argv[2])))" "$SAB_NSTEPS" "$SAB_FRAMES")"
sed -e "s|^  nsteps = .*|  nsteps = $SAB_NSTEPS|" \
    -e "s|^  nstep_snapshot = .*|  nstep_snapshot = $cadence|" \
    -e "s|^  nx = .*|  nx = $SAB_NX|" \
    -e "s|^  nparticles = .*|  nparticles = $SAB_NPART|" \
    -e "s|^  nprocx = .*|  nprocx = $SAB_NPROCX|" \
    "$CHECK_DIR/ic/$INPUTS/input.deck" > "$WORK/run/input.deck"

ranks=$(( SAB_NPROCX ))
cd "$WORK/src/epoch1d"
echo "$WORK/run" | mpirun --oversubscribe --bind-to none -n "$ranks" \
    "$WORK/src/epoch1d/bin/epoch1d" > "$WORK/run/epoch.log" 2>&1

# Graded files: the arrays rubric.json names, as raw little-endian float64.
python3 "$CHECK_DIR/extract.py" "$WORK/run" "$OUT_DIR"
