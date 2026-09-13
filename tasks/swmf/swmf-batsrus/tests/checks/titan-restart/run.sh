#!/usr/bin/env bash
# Check titan-restart: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (ALTBUILD below)
#   run.sh --help                       list the runtime knobs below, and the altbuild line when one is declared
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# Upstream test reproduced here: code/swmf/GM/BATSRUS/Param/TITAN/PARAM.in
#   Makefile.test:  test_titan_start then test_titan_restart_save / _restart_read / _restart_check
#   configure:      ./Config.pl -default -openmp -u=Titan -e=MhdTitan -ng=2 -g=6,6,6
#   rundir:         copy PARAM.in and untar Param/TITAN/TitanInput.tgz into the run directory
#   run:            OMP_NUM_THREADS=2 mpiexec -n 2 ./BATSRUS.exe (three times, one run directory)
#   post-process:   ./PostProc.pl -M -replace RESULTS/<stage>
#   upstream check: DiffNum.pl -t -r=1e-5 -a=1e-15 on RestartSave/GM/log_n000001.log with the
#                   last 25 lines of RestartRead/GM/log_n000026.log appended

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_MAX_ITERATION=10 sab.py task selfcheck ...
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_MAX_ITERATION "50" "iterations of the whole window; the restart splits it in half (25 + 25)"
knob SAB_MPI_RANKS "2" "MPI ranks; 2 is the upstream test decomposition and the graded one"
knob SAB_OMP_THREADS "2" "OpenMP threads per rank (upstream OMPIRUN default)"
knob SAB_BUILD_JOBS "0" "parallel make jobs; 0 means one per available core. Build time only, never graded"
ALTBUILD="the same Config.pl configuration built with ./Config.pl -O0 before make BATSRUS, which sets every OPTn level of Makefile.conf to -O0 where the shipped gfortran template uses -O3; same pinned source, same deck"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
# Nothing here reads standard input, and mpiexec would swallow the driver's
# check list if it were left connected.
exec </dev/null
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"
if [ "$IC" = altbuild ]; then
  [ -n "$ALTBUILD" ] || { echo "run.sh: this check declares no alternative build" >&2; exit 2; }
  INPUTS=nominal
fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
export LC_ALL=C
WORK="$(mktemp -d)"
OUT_ROOT="$(cd "$(dirname "$OUT_DIR")" && pwd -P)"
DIAGNOSTICS_ROOT="${SAB_DIAGNOSTICS_ROOT:-$OUT_ROOT/.diagnostics}"
DIAGNOSTICS_DIR="$DIAGNOSTICS_ROOT/$(basename "$CHECK_DIR")/$IC"
mkdir -p "$DIAGNOSTICS_DIR"

# Keep configuration, build and solver failure evidence outside the ephemeral
# source copy. The graded output remains exactly the files emitted below.
preserve_diagnostics() {
  local status="${1:-0}" artifact
  set +e
  mkdir -p "$DIAGNOSTICS_DIR"
  printf 'check=%s\nic=%s\nexit_status=%s\nwork=%s\nsource_dir=%s\nout_dir=%s\n' \
    "$(basename "$CHECK_DIR")" "$IC" "$status" "$WORK" "$SOURCE_DIR" "$OUT_DIR" \
    > "$DIAGNOSTICS_DIR/controller.txt"
  for artifact in install.log config.log make-batsrus.log make-pidl.log build.log \
      config-altbuild.log rundir.log cache-validation.log configured-paths.txt; do
    [ -f "$WORK/$artifact" ] && cp "$WORK/$artifact" "$DIAGNOSTICS_DIR/$artifact"
  done
  for artifact in "$WORK"/*.exit; do
    [ -f "$artifact" ] && cp "$artifact" "$DIAGNOSTICS_DIR/$(basename "$artifact")"
  done
  if [ -n "${RUN:-}" ] && [ -d "$RUN" ]; then
    for artifact in "$RUN"/runlog "$RUN"/runlog_* "$RUN"/*/runlog*; do
      [ -f "$artifact" ] && cp "$artifact" "$DIAGNOSTICS_DIR/$(basename "$artifact")"
    done
  fi
  if [ -n "${CACHE_DIR:-}" ] && [ -d "$CACHE_DIR" ]; then
    for artifact in "$CACHE_DIR"/ready.sha256 "$CACHE_DIR"/binaries.sha256; do
      [ -f "$artifact" ] && cp "$artifact" "$DIAGNOSTICS_DIR/cache-$(basename "$artifact")"
    done
  fi
  set -e
}
trap 'status=$?; trap - EXIT; preserve_diagnostics "$status"; rm -rf "$WORK"; exit "$status"' EXIT
SRC="$WORK/src"
# The pinned source is the SWMF tree. The upstream standalone BATSRUS tests run in
# the standalone layout: GM/BATSRUS with share/ and util/ beside it, which is exactly
# what BATSRUS's own Config.pl -install clones into its root. Assemble that layout from
# the pinned tree here; SOURCE_DIR itself is never modified.
cp -R "$SOURCE_DIR/GM/BATSRUS/." "$SRC"
cp -R "$SOURCE_DIR/share" "$SRC/share"
cp -R "$SOURCE_DIR/util" "$SRC/util"

# Rewrite one #STOP value of a copied PARAM file: the runtime knobs above.
set_stop() {
  python3 - "$1" "$2" "$3" "$4" <<'PY'
import sys
path, occurrence, which, value = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
lines = open(path, encoding="utf-8").read().split("\n")
seen = 0
for index, line in enumerate(lines):
    if line.strip() != "#STOP":
        continue
    seen += 1
    if seen != occurrence:
        continue
    target = index + (1 if which == "MaxIteration" else 2)
    parts = lines[target].split("\t", 1)
    lines[target] = value + ("\t" + parts[1] if len(parts) > 1 else "")
    open(path, "w", encoding="utf-8").write("\n".join(lines))
    break
else:
    raise SystemExit("run.sh: #STOP occurrence %d not found in %s" % (occurrence, path))
PY
}

# The last file of a numbered plot series: the final state of the run. The one
# wall-clock line of the Tecplot header (AUXDATA SAVEDATE) is dropped, so that
# two runs of the same code produce byte-identical graded files and the
# verifier's "byte-identical, so probably no port happened" warning still works;
# gzip -n keeps the compressed form deterministic for the same reason.
copy_last() {
  local dest=$1; shift
  local last; last="$(ls -1 "$@" | LC_ALL=C sort | tail -1)"
  case "$last" in
    *.gz) gunzip -c "$last" | grep -v 'AUXDATA SAVEDATE' | gzip -n -c > "$dest" ;;
    *)    grep -v 'AUXDATA SAVEDATE' "$last" > "$dest" ;;
  esac
}

# Cores available to this container (the CFS quota, not the host's core count).
cores() {
  local quota period
  if [ -r /sys/fs/cgroup/cpu.max ]; then
    read -r quota period < /sys/fs/cgroup/cpu.max || true
    if [ "${quota:-max}" != max ] && [ -n "${period:-}" ]; then
      echo $(( (quota + period - 1) / period )); return
    fi
  fi
  getconf _NPROCESSORS_ONLN
}

JOBS="$SAB_BUILD_JOBS"; [ "$JOBS" != 0 ] || JOBS="$(cores)"
BUILD_TASK="swmf-batsrus"
BUILD_GROUP="planetary-ionospheres"
BUILD_SPEC="./Config.pl -default -openmp -u=Titan -e=MhdTitan -ng=2 -g=6,6,6; make BATSRUS; make PIDL"
BUILD_CONFIG="./Config.pl -default -openmp -u=Titan -e=MhdTitan -ng=2 -g=6,6,6"
BUILD_MODE=normal
if [ "$IC" = altbuild ]; then BUILD_MODE=altbuild; fi

# A cache entry is valid only for the same pinned source, complete Config.pl
# command, optimisation mode and toolchain. Runtime input and rank/thread knobs
# are deliberately absent: they do not change the compiled image, so nominal
# and variant runs of the same configuration share one normal cache entry.
CACHE_ROOT="${SAB_BUILD_CACHE_ROOT:-$OUT_ROOT/.sab-build-cache}"
mkdir -p "$CACHE_ROOT"
SOURCE_PIN="9dfe746d48aa650b5209c3039f1a8676bc624899"
# The declared pin is part of the task contract; the tree digest additionally
# prevents a changed candidate source from borrowing the untouched-source cache.
SOURCE_TREE_DIGEST="$(find "$SRC" -type f -print0 | LC_ALL=C sort -z | xargs -0 sha256sum | sha256sum | cut -d' ' -f1)"
COMPILER_VERSION="$(gfortran --version 2>&1 || true)"
MAKE_VERSION="$(make --version 2>&1 || true)"
MPI_VERSION="$(mpif90 --version 2>&1 || true)"
MPI_COMPILE_FLAGS="$(mpif90 --showme:compile 2>&1 || true)"
MPI_LINK_FLAGS="$(mpif90 --showme:link 2>&1 || true)"
BUILD_FINGERPRINT="$(printf '%s\0' \
    "cache-schema=batsrus-build-v3" \
    "task=$BUILD_TASK" \
    "source-pin=$SOURCE_PIN" \
    "source-tree-digest=$SOURCE_TREE_DIGEST" \
    "build-group=$BUILD_GROUP" \
    "build-config=$BUILD_CONFIG" \
    "build-mode=$BUILD_MODE" \
    "target=a100-sxm4-80gb" \
    "runner=linux-docker" \
    "make-targets=BATSRUS,PIDL" \
    "compiler=gfortran" \
    "compiler-version=$COMPILER_VERSION" \
    "make-version=$MAKE_VERSION" \
    "mpi-version=$MPI_VERSION" \
    "mpi-compile-flags=$MPI_COMPILE_FLAGS" \
    "mpi-link-flags=$MPI_LINK_FLAGS" \
    "machine=$(uname -m)" | sha256sum | cut -d' ' -f1)"
CACHE_DIR="$CACHE_ROOT/$BUILD_TASK/$BUILD_MODE/$BUILD_GROUP/$BUILD_FINGERPRINT"
CACHE_BINARY="$CACHE_DIR/BATSRUS.exe"
CACHE_POSTIDL="$CACHE_DIR/PostIDL.exe"
CACHE_DIGEST_FILE="$CACHE_DIR/binaries.sha256"
CACHE_READY="$CACHE_DIR/ready.sha256"

configure_source() {
  cd "$SRC"
  ./Config.pl -install -compiler=gfortran > "$WORK/install.log" 2>&1 \
    || { tail -40 "$WORK/install.log" >&2; echo "run.sh: Config.pl -install failed" >&2; return 1; }
  ./Config.pl -default -openmp -u=Titan -e=MhdTitan -ng=2 -g=6,6,6 > "$WORK/config.log" 2>&1 \
    || { tail -40 "$WORK/config.log" >&2; echo "run.sh configuration failed" >&2; return 1; }
  if [ "$IC" = altbuild ]; then
    ./Config.pl -O0 >> "$WORK/config.log" 2>&1 \
      || { tail -40 "$WORK/config.log" >&2; echo "run.sh: Config.pl -O0 failed" >&2; return 1; }
    grep -q '^OPT3 = -O0' Makefile.conf \
      || { echo "run.sh: Config.pl -O0 did not set OPT3 in Makefile.conf" >&2; return 1; }
  fi
}

resolve_configured_paths() {
  local resolved
  if resolved="$(python3 - "$SRC/Makefile.def" "$SRC" <<'PY_RESOLVE'
import os, re, sys
makefile, source = sys.argv[1:]
values = {}
for raw in open(makefile, encoding="utf-8"):
    line = raw.strip()
    if not line or line.startswith("#") or "=" not in line:
        continue
    key, value = line.split("=", 1)
    key = key.strip()
    if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", key):
        values[key] = value.strip()
ref = re.compile(r"\$\{([^}]+)\}|\$\(([^)]+)\)|\$([A-Za-z_][A-Za-z0-9_]*)")
def resolve(key, stack=()):
    if key not in values:
        raise SystemExit("missing make variable " + key)
    if key in stack:
        raise SystemExit("recursive make variable " + key)
    value = values[key]
    for _ in range(32):
        changed = False
        def replace(match):
            nonlocal changed
            changed = True
            return resolve(next(group for group in match.groups() if group), stack + (key,))
        new = ref.sub(replace, value)
        value = new
        if not changed or not ref.search(value):
            break
    return value
source = os.path.realpath(source)
gmdir = os.path.realpath(resolve("GMDIR"))
bindir = resolve("BINDIR")
if not os.path.isabs(bindir):
    bindir = os.path.join(gmdir, bindir)
bindir = os.path.realpath(bindir)
print(gmdir)
print(bindir)
PY_RESOLVE
  )"; then
    :
  else
    echo "run.sh: could not resolve GMDIR/BINDIR from generated Makefile.def" >&2
    return 1
  fi
  CONFIG_GMDIR="$(printf '%s\n' "$resolved" | sed -n '1p')"
  CONFIG_BINDIR="$(printf '%s\n' "$resolved" | sed -n '2p')"
  if [ "$CONFIG_GMDIR" != "$SRC" ]; then
    echo "run.sh: generated GMDIR does not match copied source: $CONFIG_GMDIR" >&2
    return 1
  fi
  case "$CONFIG_BINDIR" in
    "$SRC"/*) ;;
    *) echo "run.sh: generated BINDIR escapes copied source: $CONFIG_BINDIR" >&2; return 1 ;;
  esac
  CONFIG_BATSRUS="$CONFIG_BINDIR/BATSRUS.exe"
  CONFIG_POSTIDL="$CONFIG_BINDIR/PostIDL.exe"
  printf 'GMDIR=%s\nBINDIR=%s\nBATSRUS=%s\nPostIDL=%s\n' \
    "$CONFIG_GMDIR" "$CONFIG_BINDIR" "$CONFIG_BATSRUS" "$CONFIG_POSTIDL" \
    > "$WORK/configured-paths.txt"
}

check_binary() {
  local path="$1" label="$2"
  if [ ! -s "$path" ] || [ ! -x "$path" ]; then
    echo "run.sh: configured $label is missing or not executable: $path" >&2
    return 1
  fi
}

cache_valid=0
printf 'cache_dir=%s\ncache_fingerprint=%s\ncache_binary=%s\ncache_postidl=%s\n' \
  "$CACHE_DIR" "$BUILD_FINGERPRINT" "$CACHE_BINARY" "$CACHE_POSTIDL" \
  > "$WORK/cache-validation.log"
if [ -x "$CACHE_BINARY" ] && [ -s "$CACHE_BINARY" ] \
    && [ -x "$CACHE_POSTIDL" ] && [ -s "$CACHE_POSTIDL" ] \
    && [ -f "$CACHE_DIGEST_FILE" ] && [ -f "$CACHE_READY" ]; then
  READY_FINGERPRINT="$(cat "$CACHE_READY" 2>/dev/null || true)"
  EXPECTED_BINARY_DIGEST="$(cat "$CACHE_DIGEST_FILE" 2>/dev/null || true)"
  ACTUAL_BINARY_DIGEST="$(sha256sum "$CACHE_BINARY" "$CACHE_POSTIDL" 2>/dev/null | awk '{printf "%s%s", sep, $1; sep=" ";} END {print ""}' || true)"
  if [ "$READY_FINGERPRINT" = "$BUILD_FINGERPRINT" ] \
      && [ -n "$EXPECTED_BINARY_DIGEST" ] \
      && [ "$EXPECTED_BINARY_DIGEST" = "$ACTUAL_BINARY_DIGEST" ]; then
    cache_valid=1
    printf 'cache_valid=1\nready_fingerprint=%s\nexpected_digest=%s\nactual_digest=%s\n' \
      "$READY_FINGERPRINT" "$EXPECTED_BINARY_DIGEST" "$ACTUAL_BINARY_DIGEST" \
      >> "$WORK/cache-validation.log"
  fi
fi
printf 'cache_valid=%s\n' "$cache_valid" >> "$WORK/cache-validation.log"

CACHE_HIT=0
if [ "$cache_valid" -eq 1 ]; then
  # Config.pl still runs on a hit so this check's generated Makefile.def and
  # make rundir use the configured BINDIR, not a guessed bin/ directory.
  if configure_source && resolve_configured_paths \
      && mkdir -p "$CONFIG_BINDIR" \
      && cp "$CACHE_BINARY" "$CONFIG_BATSRUS" \
      && cp "$CACHE_POSTIDL" "$CONFIG_POSTIDL" \
      && check_binary "$CONFIG_BATSRUS" BATSRUS.exe \
      && check_binary "$CONFIG_POSTIDL" PostIDL.exe; then
    CACHE_HIT=1
    echo "SAB_BUILD_CACHE=hit group=$BUILD_GROUP fingerprint=$BUILD_FINGERPRINT mode=$BUILD_MODE"
    BUILD_SECONDS=0
  else
    echo "run.sh: cached binaries could not be restored at configured BINDIR; rebuilding" >&2
  fi
fi

if [ "$CACHE_HIT" -eq 0 ]; then
  echo "SAB_BUILD_CACHE=miss group=$BUILD_GROUP fingerprint=$BUILD_FINGERPRINT mode=$BUILD_MODE"
  BUILD_START=$(date +%s)
  configure_source
  resolve_configured_paths
  make -j"$JOBS" BATSRUS > "$WORK/make-batsrus.log" 2>&1 \
    || { tail -60 "$WORK/make-batsrus.log" >&2; echo "run.sh: BATSRUS build failed" >&2; exit 1; }
  make PIDL > "$WORK/make-pidl.log" 2>&1 \
    || { tail -60 "$WORK/make-pidl.log" >&2; echo "run.sh: PostIDL build failed" >&2; exit 1; }
  check_binary "$CONFIG_BATSRUS" BATSRUS.exe
  check_binary "$CONFIG_POSTIDL" PostIDL.exe
  BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
  # Publish the binaries first and the matching ready marker last. A failed or
  # interrupted build therefore cannot be accepted as a cache hit later.
  if mkdir -p "$CACHE_DIR" \
      && printf '%s\n' building > "$CACHE_READY" \
      && cp "$CONFIG_BATSRUS" "$CACHE_BINARY" \
      && cp "$CONFIG_POSTIDL" "$CACHE_POSTIDL" \
      && sha256sum "$CACHE_BINARY" "$CACHE_POSTIDL" | awk '{printf "%s%s", sep, $1; sep=" ";} END {print ""}' > "$CACHE_DIGEST_FILE" \
      && printf '%s\n' "$BUILD_FINGERPRINT" > "$CACHE_READY"; then
    echo "SAB_BUILD_CACHE=published group=$BUILD_GROUP fingerprint=$BUILD_FINGERPRINT mode=$BUILD_MODE"
  else
    echo "run.sh: warning: could not publish build cache; using completed local build" >&2
  fi
fi
echo "SAB_BUILD_SECONDS=$BUILD_SECONDS"   # zero means no compile on a verified cache hit

RUN="$SRC/run_check"
( cd "$SRC" && make rundir RUNDIR=run_check STANDALONE=YES GMDIR="$SRC" ) >> "$WORK/build.log" 2>&1

# One BATSRUS run in the run directory; $1 is the name of its stdout log.
# stdin is closed for both: mpiexec forwards standard input to rank 0 and would
# otherwise drain the check list the produce driver feeds its loop.
batsrus() { ( cd "$RUN" && OMP_NUM_THREADS="$SAB_OMP_THREADS" mpiexec --oversubscribe -n "$SAB_MPI_RANKS" ./BATSRUS.exe > "$1" 2>&1 </dev/null ); }
postproc() { ( cd "$RUN" && ./PostProc.pl "$@" ) >> "$WORK/build.log" 2>&1 </dev/null; }

( cd "$RUN" && tar xzf Param/TITAN/TitanInput.tgz ) >> "$WORK/build.log" 2>&1
# One executable and one run directory for all three runs, as
# test_titan_start and test_titan_restart share them upstream.
NREAD=$(( SAB_MAX_ITERATION / 2 ))
NSAVE=$(( SAB_MAX_ITERATION - NREAD ))

# 1. the direct run over the whole window
cp "$CHECK_DIR/ic/$INPUTS/PARAM.in" "$RUN/PARAM.in"
set_stop "$RUN/PARAM.in" 1 MaxIteration "$SAB_MAX_ITERATION"
batsrus runlog_start
postproc -M -replace RESULTS/Start

# 2. the first half, writing a restart file
cp "$CHECK_DIR/ic/$INPUTS/PARAM.in.restartsave" "$RUN/PARAM.in"
set_stop "$RUN/PARAM.in" 1 MaxIteration "$NSAVE"
batsrus runlog_restartsave
postproc -M -replace RESULTS/RestartSave

# 3. the second half, read back from that restart file
cp "$CHECK_DIR/ic/$INPUTS/PARAM.in.restartread" "$RUN/PARAM.in"
set_stop "$RUN/PARAM.in" 1 MaxIteration "$NREAD"
( cd "$RUN" && ./Restart.pl -i RESULTS/RestartSave/RESTART ) >> "$WORK/build.log" 2>&1
batsrus runlog_restartread
postproc -M -replace RESULTS/RestartRead

# 4. the concatenated log, built exactly as test_titan_restart_check builds it
cat "$RUN"/RESULTS/RestartSave/GM/log_n*.log > "$OUT_DIR/log_all.log"
tail -n "$NREAD" "$RUN"/RESULTS/RestartRead/GM/log_n*.log >> "$OUT_DIR/log_all.log"
cp "$RUN"/RESULTS/Start/GM/log_n*.log "$OUT_DIR/log_direct.log"
copy_last "$OUT_DIR/y0_final.dat" "$RUN"/RESULTS/RestartRead/GM/y=0_mhd_*.dat
