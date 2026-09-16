#!/usr/bin/env bash
# Check nurbs-maxwell-definite: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_RUNS "r01-square-nurbs-r1-o2 r02-cube-nurbs-r1-o2" "which of this upstream target's registered configurations to run; runtime scales with the list"
knob SAB_CPUS 4 "cores this check may use: the declared per-check cpus of task.toml, never read from the host. The miniapp and the unit binary run serially, so it bounds only the parallel jobs of the pinned MFEM build (each job needs about 1 GB)"
# The pinned build is MFEM's own OPTIM_FLAGS, -O3 -std=c++17 (config/defaults.mk:29, taken by
# CXXFLAGS ?= $(OPTIM_FLAGS) at makefile:227). -O0 is NOT used as the alternative build: on
# baseline x86_64 gcc it changes nothing in floating-point evaluation (no FMA without -march,
# and gcc never reassociates), so it would record a floor of exactly zero, which is evidence
# that the alternative build computed the same thing rather than evidence of stability.
# See references/pitfalls/altbuild-floors-are-host-specific.md.
ALTBUILD='CXXFLAGS="-O3 -std=c++17 -mfma -ffp-contract=fast": the pinned source at its own -O3 with FP contraction enabled, which a correct candidate on this hardware could plausibly be'
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"; CXX_EXTRA=""
if [ "$IC" = altbuild ]; then INPUTS=nominal; CXX_EXTRA="-mfma -ffp-contract=fast"; fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

# ---- build, reusing the build an earlier check of this run already made ------------------
# Content-keyed on the PINNED SOURCE and the exact recipe, computed BEFORE the initial
# condition is overlaid. The initial condition is a set of mesh files, which no compilation
# reads, so keying the build on it would force a full 13-minute library rebuild for the
# variant and again for the altbuild and buy nothing. A changed source or recipe still
# cannot reuse the wrong tree. test.sh produce runs checks sequentially, so no cross-check
# race protocol is needed: the first check of a run builds and publishes, the rest import it
# and report SAB_BUILD_SECONDS=0.
BUILD_START=$(date +%s)
RECIPE="make serial MFEM_USE_METIS=NO CXXFLAGS=-O3 -std=c++17 $CXX_EXTRA"
# `cd` first so the hashed names are RELATIVE. sha256sum prints "<hash>  <path>", and an
# absolute path here would carry this run's mktemp directory into the digest, giving every
# check a fresh key, missing the cache every time and rebuilding MFEM once per check.
KEY="$( cd "$WORK/src" && { find . -type f -print0 | LC_ALL=C sort -z | xargs -0 sha256sum; echo "$RECIPE"; } | sha256sum | cut -c1-32 )"
CACHE="${SAB_BUILD_CACHE:-/tmp/sab-build-mfem-nurbs-isogeometric}/$KEY"
build_here() {
  local dst="$1"
  make -C "$dst" serial -j"$SAB_CPUS" MFEM_USE_METIS=NO \
       CXXFLAGS="-O3 -std=c++17 $CXX_EXTRA" > "$WORK/build.log" 2>&1 \
    || { echo "run.sh: MFEM library build failed:" >&2; tail -30 "$WORK/build.log" >&2; exit 1; }
  make -C "$dst/miniapps/nurbs" -j"$SAB_CPUS" nurbs_ex3 >> "$WORK/build.log" 2>&1 \
    || { echo "run.sh: miniapp build failed:" >&2; tail -30 "$WORK/build.log" >&2; exit 1; }
}
if [ -f "$CACHE/BUILD_OK" ]; then
  # The library is already built in this tree by an earlier check of the same run. Checks of
  # this module drive eleven different miniapps, so the binary THIS check needs may not be
  # linked yet -- building just that one against the existing libmfem.a takes seconds, where
  # falling through to a private build would repeat the whole 13-minute library compile.
  if [ ! -x "$CACHE/tree/miniapps/nurbs/nurbs_ex3" ]; then
    make -C "$CACHE/tree/miniapps/nurbs" -j"$SAB_CPUS" nurbs_ex3 > "$WORK/build.log" 2>&1 \
      || { echo "run.sh: miniapp build failed:" >&2; tail -30 "$WORK/build.log" >&2; exit 1; }
  fi
  BIN="$CACHE/tree/miniapps/nurbs/nurbs_ex3"
else
  if mkdir -p "$CACHE" 2>/dev/null && [ ! -f "$CACHE/BUILD_OK" ]; then
    # A tree without BUILD_OK is the debris of a build that died; copying onto it would nest
    # the new source inside it and then fail with no makefile at the top. Clear it first.
    rm -rf "$CACHE/tree"
    cp -R "$WORK/src" "$CACHE/tree" 2>/dev/null || true
    if [ -d "$CACHE/tree" ]; then
      build_here "$CACHE/tree" && touch "$CACHE/BUILD_OK"
      BIN="$CACHE/tree/miniapps/nurbs/nurbs_ex3"
    fi
  fi
  if [ -z "${BIN:-}" ]; then          # shared cache unavailable: build privately
    build_here "$WORK/src"
    BIN="$WORK/src/miniapps/nurbs/nurbs_ex3"
  fi
fi
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # 0 on reuse; the budget counts run time only

# The initial condition is the mesh set this check solves on. It is overlaid AFTER the build,
# onto the tree the runs read their meshes from, so nominal and variant share one build and
# still solve on different geometry.
while IFS= read -r rel; do
  [ -n "$rel" ] || continue
  mkdir -p "$WORK/src/$(dirname "$rel")"
  cp -f "$CHECK_DIR/ic/$INPUTS/files/$rel" "$WORK/src/$rel"
done < "$CHECK_DIR/ic/$INPUTS/manifest.txt"
MESHDIR="$WORK/src"

# ---- run each registered configuration in its own staged directory -----------------------
# A miniapp resolves its default mesh two directory levels above its own location, so each
# run sits two levels below a per-run root with the mesh tree linked in at that height. The
# binary's own directory is linked as well: several runs open a compiled-in default that
# never appears in the argument string.
run_case() {
  local tag="$1"; shift
  local root="$WORK/run/$tag" rd="$WORK/run/$tag/b/miniapps/nurbs"
  mkdir -p "$rd"
  ln -sfn "$MESHDIR/data" "$root/b/data"
  [ -d "$MESHDIR/miniapps/nurbs/meshes" ] && ln -sfn "$MESHDIR/miniapps/nurbs/meshes" "$rd/meshes"
  local f
  for f in "$MESHDIR/miniapps/nurbs"/*.mesh; do
    [ -e "$f" ] && ln -sfn "$f" "$rd/$(basename "$f")"
  done
  ( cd "$rd" && "$BIN" "$@" > stdout.txt 2>&1 ) || {
      echo "run.sh: $tag exited $? ; tail of its output:" >&2; tail -20 "$rd/stdout.txt" >&2; exit 1; }
  emit "$tag" "$rd"
}

# emit <tag> <run dir>: the graded files of one configuration, prefixed by its tag.
# `refined.mesh` is deliberately NOT graded where the run refines: section 9 of the packaging
# direction forbids grading element numbering after refinement, and a genuinely different
# space is still caught because the solution stream then has a different length.
emit() {
  local tag="$1" rd="$2" f line pat
  for f in sol.gf; do
    [ -s "$rd/$f" ] || { echo "run.sh: $tag did not write $f" >&2; exit 1; }
    cp "$rd/$f" "$OUT_DIR/${tag}__$f"
  done
  # Printed errors, matched by curated patterns and never by a generic "||...=" grep:
  # ex10 prints 75 lines of Newton residual history in that notation and nurbs_solenoidal
  # prints || div u_h - div u_ex ||, a residual whose exact value is zero (round-off only,
  # measured 4.04e-13). See comment/README.md and references/pitfalls/residual-below-one-ulp.md.
  : > "$OUT_DIR/${tag}__errors.txt"
  for pat in 'E_h - E'; do
    line="$(grep -F -- "$pat" "$rd/stdout.txt" | tail -1 || true)"
    [ -n "$line" ] || { echo "run.sh: $tag printed no line matching $pat" >&2; exit 1; }
    printf '%s\n' "${line##*= }" >> "$OUT_DIR/${tag}__errors.txt"
  done
}

for tag in $SAB_RUNS; do
  case "$tag" in
    r01-square-nurbs-r1-o2) run_case r01-square-nurbs-r1-o2 -m "$MESHDIR/data/square-nurbs.mesh" -r 1 -o 2 -no-vis ;;
    r02-cube-nurbs-r1-o2) run_case r02-cube-nurbs-r1-o2 -m "$MESHDIR/data/cube-nurbs.mesh" -r 1 -o 2 -no-vis ;;
    *) echo "run.sh: unknown run tag '$tag'; known: r01-square-nurbs-r1-o2 r02-cube-nurbs-r1-o2" >&2; exit 2 ;;
  esac
done
