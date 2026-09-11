#!/usr/bin/env bash
# Check cpp-rnea: the TEST half of the check.
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
knob SAB_TIMEOUT_SECONDS "600" "wall-clock limit for the adapter process; the graded run takes milliseconds, so this only bounds a hang"
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

# Upstream test this check reproduces: code/pinocchio/unittest/rnea.cpp
#
# Build reuse. The Pinocchio library is a heavy C++ template build, so the leaf
# builds it once per run and every check of the run links against that one tree.
# The image prepares it at SAB_PINOCCHIO_PREBUILT; when that is absent this script
# builds it itself into the same shared location, so whichever check runs first
# pays the cost and the other thirty-five reuse it. SAB_BUILD_SECONDS therefore
# reports a large number for the first check of a run and a small one (this
# check's own adapter compile) for the rest.
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
      -DBUILD_PYTHON_INTERFACE=OFF -DBUILD_WITH_COLLISION_SUPPORT=OFF \
      -DBUILD_WITH_URDF_SUPPORT=OFF -DBUILD_WITH_OPENMP_SUPPORT=ON \
      -DINSTALL_DOCUMENTATION=OFF >"$PREBUILT/configure.log" 2>&1
    cmake --build "$PREBUILT/build" --parallel "$SAB_BUILD_JOBS" \
      --target pinocchio_default >"$PREBUILT/build.log" 2>&1
    touch "$PREBUILT/.ready"
  fi
  exec 9>&-
fi

# Compile this check's adapter against the shared library tree. The defines are
# the ones Pinocchio's own unit tests are built with; without the Boost.MPL
# limits the joint variant does not instantiate.
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
# Locate Eigen. The produce driver runs each check under env -i and passes only
# SAB_ variables through, so the image's CONDA_PREFIX, EIGEN3_INCLUDE_DIR and
# PKG_CONFIG_PATH are all stripped before this script sees them; only PATH
# survives. The toolchain prefix is therefore derived from where the compiler
# actually is, and pkg-config is tried but never trusted to succeed. An empty
# result from pkg-config is legitimate and must not kill the script under set -e;
# a wrong guess must fail loudly here rather than as a confusing compile error.
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
# -fopenmp is passed for every check, not only the two that use the pooled entry
# points: the shared library is built with OpenMP support, so the flag keeps the
# adapter's view of the headers identical to the library's, and it is inert for
# the checks that never touch a pool.
g++ -std=c++17 -O2 -DNDEBUG -fopenmp \
  -DBOOST_MPL_CFG_NO_PREPROCESSED_HEADERS -DBOOST_MPL_LIMIT_LIST_SIZE=30 \
  -DBOOST_MPL_LIMIT_VECTOR_SIZE=30 -DBOOST_FUSION_INVOKE_MAX_ARITY=12 \
  -DPINOCCHIO_ENABLE_TEMPLATE_INSTANTIATION -DPINOCCHIO_DISABLE_UNSUPPORTED_WARNINGS \
  -DBOOST_TEST_DYN_LINK -DBOOST_TEST_MODULE=RneaCheck \
  -I"$CHECK_DIR" -I"$PREBUILT/src/include" -I"$PREBUILT/build/include" \
  -isystem "$EIGEN_INC" \
  "$CHECK_DIR/official.cpp" -o "$WORK/official" \
  -L"$PREBUILT/build/src" -lpinocchio_default -lboost_unit_test_framework \
  -Wl,-rpath,"$PREBUILT/build/src"
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
