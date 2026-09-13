#!/usr/bin/env bash
# Check magnetogram-fdips-wedge: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     OPTIONAL: the nominal inputs on the alternative build (ALTBUILD below)
#   run.sh --help                       list the runtime knobs below, and the altbuild line when one is declared
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# Upstream test: the target `test_fdips_wedge` of code/swmf/GM/BATSRUS/util/DATAREAD/srcMagnetogram/Makefile.
# The build, the control file, the run and the graded files are the upstream
# recipe. The one departure is the magnetogram: upstream runs DIPOLE11.exe to
# generate it into the working directory, and this check ships that same
# magnetogram under ic/ instead, so the two initial conditions can differ.
# The wedge magnetogram is upstream's own fdips_wedge_input.gz, shipped here under ic/ the same way.

KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_MPI_RANKS "2" "MPI ranks of the graded run; the upstream test uses 2 and the graded reference is produced with 2 (the domain decomposition of FDIPS depends on it, so changing it changes the graded numbers at round-off)"
knob SAB_MPI_EXTRA "" "extra arguments passed to mpiexec (for example --oversubscribe on a host with fewer slots than ranks); empty is the graded value and does not change the result"
knob SAB_BUILD_JOBS "4" "make -j for the build; affects build time only, never the graded values"
# Alternative build, OPTIONAL. Set ALTBUILD to one line naming a legitimately different build of the
# same source (IEEE mode, -O0, a second compiler present in the image: something a correct candidate
# could plausibly be) ONLY when this check can be built that way; leave it empty otherwise. When it is
# set, `run.sh altbuild` runs ic/nominal on that build and selfcheck measures the check's floor from it.
ALTBUILD="the same Config.pl configuration built with ./Config.pl -O0 before make BATSRUS, which sets every OPTn level of Makefile.conf to -O0 where the shipped gfortran template uses -O3; same pinned source, same deck"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
# mpiexec forwards standard input to rank 0 and drains it. The produce driver
# feeds the check list to its own loop on standard input, so a check that leaves
# stdin connected swallows the checks after it; take stdin away here.
exec < /dev/null
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"
if [ "$IC" = altbuild ]; then
  [ -n "$ALTBUILD" ] || { echo "run.sh: this check declares no alternative build" >&2; exit 2; }
  INPUTS=nominal
fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
export LC_ALL=C
# The pinned source is the SWMF tree. The upstream standalone BATSRUS tests run in
# the standalone layout: GM/BATSRUS with share/ and util/ beside it, which is exactly
# what BATSRUS's own Config.pl -install clones into its root. Assemble that layout from
# the pinned tree here; SOURCE_DIR itself is never modified.
cp -R "$SOURCE_DIR/GM/BATSRUS/." "$WORK/src"
cp -R "$SOURCE_DIR/share" "$WORK/src/share"
cp -R "$SOURCE_DIR/util" "$WORK/src/util"
cd "$WORK/src"

SRC="$WORK/src"
# Mechanical build reuse is deliberately outside the scientific run boundary.
export LC_ALL=C
fail() { echo "run.sh: $1" >&2; shift; tail -n 40 "$@" >&2 || true; exit 1; }
BUILD_TASK="swmf-batsrus"
BUILD_STAGE="main"
BUILD_SPEC='Config.pl -install -compiler=gfortran; make -C util/DATAREAD/srcMagnetogram libSHARE; make -C util/DATAREAD/srcMagnetogram FDIPS'
BUILD_GROUP="magnetogram-fdips"
BUILD_TARGETS="FDIPS,libSHARE"
BUILD_TARGET="a100-sxm4-80gb"
BUILD_TARGET_SHA256="fec36b64e17d0893720e55b74a78c61d4e0f5cfc796322ff92f1a3b04a53c132"
BUILD_MODE=normal
if [ "$IC" = altbuild ]; then BUILD_MODE=altbuild; fi
CACHE_FILES=(FDIPS.exe)
CACHE_ENABLED=0
if [ -n "${SAB_BUILD_CACHE_ROOT:-}" ] && [ -n "${SAB_SOURCE_FINGERPRINT:-}" ]; then CACHE_ENABLED=1; fi

configure_source() {
  {
  ./Config.pl -install -compiler=gfortran
  if [ "$IC" = altbuild ]; then
    ./Config.pl -O0 >> "$WORK/config.log" 2>&1
    grep -q '^OPT3 = -O0' Makefile.conf || { echo "run.sh: Config.pl -O0 did not set OPT3" >&2; exit 1; }
  fi
  # libSHARE first and on its own: the srcMagnetogram targets list it as a
  # prerequisite next to their own objects, which is not parallel-safe.
  } > "$WORK/config.log" 2>&1 || fail "configuration failed" "$WORK/config.log"
}

build_source() {
  {
  make -C util/DATAREAD/srcMagnetogram libSHARE
  make -j"$SAB_BUILD_JOBS" -C util/DATAREAD/srcMagnetogram FDIPS
  } > "$WORK/build.log" 2>&1 || fail "build failed" "$WORK/build.log"
}


BUILD_BIN_DIR="$SRC/util/DATAREAD/srcMagnetogram"

binary_digest() {
  local base=$1 name
  for name in "${CACHE_FILES[@]}"; do
    [ -s "$base/$name" ] || return 1
    [ -x "$base/$name" ] || return 1
  done
  (cd "$base" && sha256sum "${CACHE_FILES[@]}" | awk '{printf "%s%s", sep, $1; sep=" ";} END {print ""}')
}

cache_entry_valid() {
  local expected actual name
  [ -f "$CACHE_READY" ] && [ -f "$CACHE_DIGEST_FILE" ] || return 1
  [ "$(cat "$CACHE_READY" 2>/dev/null || true)" = "$BUILD_FINGERPRINT" ] || return 1
  expected="$(cat "$CACHE_DIGEST_FILE" 2>/dev/null || true)"
  actual="$(binary_digest "$CACHE_DIR" 2>/dev/null || true)"
  [ -n "$expected" ] && [ "$expected" = "$actual" ] || return 1
  for name in "${CACHE_FILES[@]}"; do [ -x "$CACHE_DIR/$name" ] && [ -s "$CACHE_DIR/$name" ] || return 1; done
}

restore_cache() {
  local name
  for name in "${CACHE_FILES[@]}"; do
    cp "$CACHE_DIR/$name" "$BUILD_BIN_DIR/$name" || return 1
    [ -x "$BUILD_BIN_DIR/$name" ] && [ -s "$BUILD_BIN_DIR/$name" ] || return 1
  done
}

publish_cache() {
  local name
  mkdir -p "$CACHE_DIR" || return 1
  # Write binaries and digest before the ready marker; a partial entry cannot hit.
  printf '%s\n' building > "$CACHE_READY" || return 1
  for name in "${CACHE_FILES[@]}"; do cp "$BUILD_BIN_DIR/$name" "$CACHE_DIR/$name" || return 1; done
  binary_digest "$CACHE_DIR" > "$CACHE_DIGEST_FILE" || return 1
  printf '%s\n' "$BUILD_FINGERPRINT" > "$CACHE_READY" || return 1
}

run_cached_build() {
  local compiler_version make_version mpi_version mpi_include name
  STAGE_BUILD_SECONDS=0
  if [ "$CACHE_ENABLED" -eq 1 ]; then
    compiler_version="$(gfortran --version 2>&1 || true)"
    make_version="$(make --version 2>&1 || true)"
    mpi_version="$(mpif90 --version 2>&1 || true)"
    mpi_include="$(mpif90 -showme:compile 2>/dev/null || true)"
    BUILD_FINGERPRINT="$(printf '%s\0' \
        "cache-schema=batsrus-build-v1" \
        "task=$BUILD_TASK" \
        "stage=$BUILD_STAGE" \
        "source-fingerprint=$SAB_SOURCE_FINGERPRINT" \
        "source-root=$SOURCE_DIR" \
        "build-group=$BUILD_GROUP" \
        "build-spec=$BUILD_SPEC" \
        "build-mode=$BUILD_MODE" \
        "altbuild-spec=$ALTBUILD" \
        "target=$BUILD_TARGET" \
        "target-descriptor-sha256=$BUILD_TARGET_SHA256" \
        "runner=linux-docker" \
        "make-targets=$BUILD_TARGETS" \
        "make-jobs=$SAB_BUILD_JOBS" \
        "mpi-ranks=$SAB_MPI_RANKS" \
        "mpi-include=$mpi_include" \
        "compiler=gfortran" \
        "compiler-version=$compiler_version" \
        "make-version=$make_version" \
        "mpi-version=$mpi_version" \
        "machine=$(uname -m)" | sha256sum | cut -d' ' -f1)"
    CACHE_DIR="$SAB_BUILD_CACHE_ROOT/$BUILD_TASK/$BUILD_GROUP/$BUILD_FINGERPRINT"
    CACHE_BINARY="$CACHE_DIR/BATSRUS.exe"
    CACHE_POSTIDL="$CACHE_DIR/PostIDL.exe"
    CACHE_DIGEST_FILE="$CACHE_DIR/binaries.sha256"
    CACHE_READY="$CACHE_DIR/ready.sha256"
    CACHE_HIT=0
    if cache_entry_valid; then
      if (configure_source && restore_cache); then
        echo "SAB_BUILD_CACHE=hit group=$BUILD_GROUP stage=$BUILD_STAGE fingerprint=$BUILD_FINGERPRINT variant=$IC altbuild=$BUILD_MODE"
        STAGE_BUILD_SECONDS=0
        CACHE_HIT=1
      else
        echo "run.sh: cached binaries could not be configured/restored; using complete cold build for $BUILD_GROUP/$BUILD_STAGE" >&2
      fi
    fi
    if [ "$CACHE_HIT" -eq 0 ]; then
      echo "SAB_BUILD_CACHE=miss group=$BUILD_GROUP stage=$BUILD_STAGE fingerprint=$BUILD_FINGERPRINT variant=$IC altbuild=$BUILD_MODE"
      BUILD_START=$(date +%s)
      configure_source
      build_source
      for name in "${CACHE_FILES[@]}"; do
        [ -x "$BUILD_BIN_DIR/$name" ] && [ -s "$BUILD_BIN_DIR/$name" ] || fail "build did not produce configured $name at $BUILD_BIN_DIR" "$WORK/build.log"
      done
      STAGE_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
      if publish_cache; then
        echo "SAB_BUILD_CACHE=published group=$BUILD_GROUP stage=$BUILD_STAGE fingerprint=$BUILD_FINGERPRINT variant=$IC altbuild=$BUILD_MODE"
      else
        echo "run.sh: warning: could not publish build cache for $BUILD_GROUP/$BUILD_STAGE; retaining local build" >&2
      fi
    fi
  else
    echo "SAB_BUILD_CACHE=disabled reason=missing solve-scoped source fingerprint or cache root"
    BUILD_START=$(date +%s)
    configure_source
    build_source
    for name in "${CACHE_FILES[@]}"; do
      [ -x "$BUILD_BIN_DIR/$name" ] && [ -s "$BUILD_BIN_DIR/$name" ] || fail "build did not produce configured $name at $BUILD_BIN_DIR" "$WORK/build.log"
    done
    STAGE_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
  fi
}

run_cached_build
BUILD_SECONDS=$STAGE_BUILD_SECONDS
echo "SAB_BUILD_SECONDS=$BUILD_SECONDS"   # zero means no compile on a verified cache hit


RUN="$WORK/src/util/DATAREAD/srcMagnetogram"
cd "$RUN"
for f in "$CHECK_DIR/ic/$INPUTS"/*; do
  case "$f" in
    *.gz) gunzip -c "$f" > "$(basename "${f%.gz}")" ;;
    *)    cp "$f" "$(basename "$f")" ;;
  esac
done

mpiexec -n "$SAB_MPI_RANKS" ${SAB_MPI_EXTRA:-} ./FDIPS.exe > tool.log 2>&1 || { echo "run.sh: FDIPS.exe failed" >&2; tail -n 40 tool.log >&2; exit 1; }

cp "fdips_field.out" "$OUT_DIR/fdips_field.out"
