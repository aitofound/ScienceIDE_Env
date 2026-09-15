#!/usr/bin/env bash
# Check exc-geometry-models: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     not declared by this check; see rubric.json
#   run.sh --help                       list the runtime knobs below
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

# Runtime knobs. Defaults are the graded values.
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_BUILD_JOBS "4" "compile parallelism for the one-off Pinocchio library build; scales build time only, which is reported separately and excluded from the suite budget"
knob SAB_TIMEOUT_SECONDS "900" "wall-clock limit for the adapter process; the graded run takes well under a second, so this only bounds a hang"
knob SAB_EIGEN_INCLUDE "" "explicit path to the Eigen include directory; empty means derive it from the compiler prefix, which is what the images provide"
knob SAB_PINOCCHIO_PREBUILT "/opt/sab/pinocchio-prebuilt" "where the once-per-run Pinocchio library build lives; the oracle image prepares it here, and this script builds it here when it is absent. The name must start with SAB_ because the produce driver runs each check under env -i and passes only SAB_ variables through"
# This check declares no alternative build: -O0 with -ffp-contract=off reproduces
# the -O2 result bit-identically here, so it would measure a floor of zero. See
# rubric.json.
ALTBUILD=""
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"
if [ "$IC" = altbuild ]; then
  echo "run.sh: this check declares no alternative build" >&2
  exit 2
fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }

# Upstream test this check reproduces: code/pinocchio/examples/geometry-models.cpp
#
# Build reuse. The Pinocchio library is a heavy C++ template build, so the leaf
# builds it once per run and every check of the run links against that one tree.
# The image prepares it at SAB_PINOCCHIO_PREBUILT; when that is absent this script
# builds it itself into the same shared location, so whichever check runs first
# pays the cost and the others reuse it. SAB_BUILD_SECONDS therefore reports a
# large number for the first check of a run and a small one (this check's own
# adapter compile) for the rest.
#
# This module's build differs from the other Pinocchio leaves in one way that it
# genuinely needs: collision support and the URDF parser are ON, because the
# module under test is the geometry, distance and collision machinery, which
# does not exist in the library at all without -DBUILD_WITH_COLLISION_SUPPORT=ON
# (every GeometryData collision field is behind #ifdef PINOCCHIO_WITH_COLLISION),
# and because the collision decks are URDF robot descriptions vendored with the
# pinned source.
PREBUILT="$SAB_PINOCCHIO_PREBUILT"
BUILD_START=$(date +%s)
if [ ! -f "$PREBUILT/.ready" ]; then
  mkdir -p "$PREBUILT"
  # A lock so two checks of the same run cannot build into the same tree at once.
  exec 9>"$PREBUILT/.lock"
  flock 9
  if [ ! -f "$PREBUILT/.ready" ]; then
    echo "run.sh: building the Pinocchio library once for this run into $PREBUILT" >&2
    rm -rf "$PREBUILT/src"
    cp -R "$SOURCE_DIR/." "$PREBUILT/src"
    cmake -S "$PREBUILT/src" -B "$PREBUILT/build" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF -DBUILD_BENCHMARK=OFF \
      -DBUILD_PYTHON_INTERFACE=OFF -DBUILD_WITH_COLLISION_SUPPORT=ON \
      -DBUILD_WITH_URDF_SUPPORT=ON -DBUILD_WITH_OPENMP_SUPPORT=ON \
      -DINSTALL_DOCUMENTATION=OFF >"$PREBUILT/configure.log" 2>&1
    cmake --build "$PREBUILT/build" --parallel "$SAB_BUILD_JOBS" \
      --target pinocchio_default pinocchio_collision pinocchio_parsers \
      >"$PREBUILT/build.log" 2>&1
    # pinocchio_collision_parallel is a header-only INTERFACE target (src/
    # CMakeLists.txt), so there is nothing to build or link for it; the pooled
    # entry points come in through its headers and libpinocchio_collision.
    touch "$PREBUILT/.ready"
  fi
  exec 9>&-
fi

# Compile this check's adapter against the shared library tree. The defines are
# the ones Pinocchio's own unit tests are built with; without the Boost.MPL
# limits the joint variant does not instantiate.
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
# Locate the toolchain prefix. The produce driver runs each check under env -i
# and passes only SAB_ variables through, so the image's CONDA_PREFIX,
# EIGEN3_INCLUDE_DIR and PKG_CONFIG_PATH are all stripped before this script sees
# them; only PATH survives. The prefix is therefore derived from where the
# compiler actually is, and pkg-config is tried but never trusted to succeed. An
# empty result from pkg-config is legitimate and must not kill the script under
# set -e; a wrong guess must fail loudly here rather than as a confusing compile
# error.
CXX_PATH="$(command -v g++)"
CXX_PREFIX="${CXX_PATH%/bin/g++}"
EIGEN_INC="$(pkg-config --variable=includedir eigen3 2>/dev/null || true)"
if [ -z "$EIGEN_INC" ] || [ ! -f "$EIGEN_INC/Eigen/Core" ]; then
  EIGEN_INC=""
  for cand in "${SAB_EIGEN_INCLUDE:-}" "$CXX_PREFIX/include/eigen3" \
              /opt/sab/env/include/eigen3 /usr/include/eigen3 /usr/local/include/eigen3; do
    if [ -n "$cand" ] && [ -f "$cand/Eigen/Core" ]; then EIGEN_INC="$cand"; break; fi
  done
fi
[ -n "$EIGEN_INC" ] || { echo "run.sh: cannot find Eigen (no Eigen/Core under pkg-config, the compiler prefix $CXX_PREFIX, or the usual locations)" >&2; exit 1; }
[ -f "$CXX_PREFIX/include/coal/collision.h" ] || { echo "run.sh: cannot find the coal headers under $CXX_PREFIX/include/coal" >&2; exit 1; }

# coal's own compile definitions decide the layout of the coal types this
# adapter passes across the library boundary (octomap support adds members), so
# they must be exactly the ones the prebuilt library was compiled with. CMake
# reads them from coal's exported target file for the library build; this script
# reads them from the same file, and falls back to the values of the pinned
# conda packages (coal 3.0.4, octomap 1.10.0) when that file is not where it is
# expected. A grep that matches nothing is legitimate here and must not kill the
# script under set -e.
COAL_TARGETS="$CXX_PREFIX/lib/cmake/coal/coalTargets.cmake"
COAL_DEFS=""
if [ -f "$COAL_TARGETS" ]; then
  COAL_DEFS="$(sed -n 's/.*INTERFACE_COMPILE_DEFINITIONS "\([^"]*\)".*/\1/p' "$COAL_TARGETS" | head -1 || true)"
fi
[ -n "$COAL_DEFS" ] || COAL_DEFS="COAL_HAS_OCTOMAP;COAL_HAVE_OCTOMAP;OCTOMAP_MAJOR_VERSION=1;OCTOMAP_MINOR_VERSION=10;OCTOMAP_PATCH_VERSION=0"
COAL_FLAGS=()
IFS=';' read -r -a coal_def_list <<<"$COAL_DEFS"
for d in "${coal_def_list[@]}"; do [ -n "$d" ] && COAL_FLAGS+=("-D$d"); done

# The robot descriptions the collision decks use are vendored with the pinned
# source under models/; the two directory macros below are the ones Pinocchio's
# own unit tests are compiled with, pointed at this run's copy of that tree.
# -fopenmp is passed for every check, not only the one that uses the pooled entry
# points: the shared library is built with OpenMP support, so the flag keeps the
# adapter's view of the headers identical to the library's, and it is inert for
# the checks that never touch a pool.
g++ -std=c++17 -O2 -DNDEBUG -fopenmp \
  -DBOOST_MPL_CFG_NO_PREPROCESSED_HEADERS -DBOOST_MPL_LIMIT_LIST_SIZE=30 \
  -DBOOST_MPL_LIMIT_VECTOR_SIZE=30 -DBOOST_FUSION_INVOKE_MAX_ARITY=12 \
  -DPINOCCHIO_ENABLE_TEMPLATE_INSTANTIATION -DPINOCCHIO_DISABLE_UNSUPPORTED_WARNINGS \
  -DPINOCCHIO_WITH_COLLISION -DPINOCCHIO_WITH_HPP_FCL -DPINOCCHIO_WITH_URDFDOM \
  "${COAL_FLAGS[@]}" \
  -DEXAMPLE_ROBOT_DATA_MODEL_DIR="\"$PREBUILT/src/models/example-robot-data/robots\"" \
  -DPINOCCHIO_MODEL_DIR="\"$PREBUILT/src/models\"" \
  -DBOOST_TEST_DYN_LINK -DBOOST_TEST_MODULE=excgeometrymodelsCheck \
  -I"$CHECK_DIR" -I"$PREBUILT/src/include" -I"$PREBUILT/build/include" \
  -isystem "$EIGEN_INC" -isystem "$CXX_PREFIX/include" \
  "$CHECK_DIR/official.cpp" -o "$WORK/official" \
  -L"$PREBUILT/build/src" -lpinocchio_parsers -lpinocchio_collision -lpinocchio_default \
  -L"$CXX_PREFIX/lib" -lcoal -loctomap -loctomath \
  -lboost_filesystem -lboost_serialization -lboost_unit_test_framework \
  -Wl,-rpath,"$PREBUILT/build/src" -Wl,-rpath,"$CXX_PREFIX/lib"
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"

# Run the instrumented upstream test. Every original assertion is active, so a
# nonzero exit means an upstream identity broke and the check fails here.
#
# Only numerical.jsonl is written to OUT_DIR. Boost.Test can also emit an XML
# report, but it records per-case wall times, so shipping it would leave an
# ungraded sidecar that differs on every run and would permanently mask the
# selfcheck's byte-identical reading of the graded output (the failure mode in
# references/pitfalls/ungraded-sidecars-mask-identical-graded-output.md). The
# assertions are enforced by this script's exit status instead, which is the
# same gate without the noise.
SAB_IC_DIR="$CHECK_DIR/ic/$INPUTS" SAB_OUT="$WORK/numerical.jsonl" \
  timeout "$SAB_TIMEOUT_SECONDS" "$WORK/official" --log_level=error \
  >"$WORK/official.log" 2>&1 || {
    echo "run.sh: the upstream assertions did not all pass" >&2
    cat "$WORK/official.log" >&2
    exit 1
  }

cp "$WORK/numerical.jsonl" "$OUT_DIR/numerical.jsonl"
