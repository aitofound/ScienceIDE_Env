#!/usr/bin/env bash
# Check ex-earth-2d: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_STOP_SCALE=0.25 sab.py task selfcheck ...
# Parallel build jobs default to the CPUs this container may use (cgroup v2 cpu.max), not the host count.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_STOP_SCALE "1" "multiplies the retained first-session MaxIteration (default: 500 iterations); run time scales with it"
knob SAB_PLOT_FRAMES "5" "target frame count of the graded plot series before the run ends (>= 5); run.sh rewrites that series' cadence to window / SAB_PLOT_FRAMES"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the build of the pinned source (default: the CPUs allowed to this container); it changes build time only, never the graded run"
# Alternative build, OPTIONAL: BATSRUS's own ./Config.pl -O0 rewrites every OPTn line of
# Makefile.conf to -O0 where the shipped gfortran template (share/build/Makefile.Linux.gfortran)
# builds at -O3 -- a legitimately different build of the same pinned source and deck.
knob SAB_MPI_RANKS "2" "MPI ranks BATSRUS.exe runs on; the upstream test launch uses 2"
ALTBUILD="the same Config.pl configuration built with ./Config.pl -O0 before make BATSRUS, which sets every OPTn level of Makefile.conf to -O0 where the shipped gfortran template uses -O3; same pinned source, same deck"
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
exec < /dev/null                 # mpiexec must not read the produce driver's stdin
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/param" "$WORK/src"
# The pinned source is the SWMF tree. The upstream standalone BATSRUS tests run in
# the standalone layout: GM/BATSRUS with share/ and util/ beside it, which is exactly
# what BATSRUS's own Config.pl -install clones into its root. Assemble that layout from
# the pinned tree here; SOURCE_DIR itself is never modified.
cp -R "$SOURCE_DIR/GM/BATSRUS/." "$WORK/src"
cp -R "$SOURCE_DIR/share" "$WORK/src/share"
cp -R "$SOURCE_DIR/util" "$WORK/src/util"
chmod -R u+w "$WORK/src"
export LC_ALL=C OMP_NUM_THREADS=1

# Upstream test this check reproduces: upstream example Param/EARTH/PARAM.in.2D (no Makefile.test target)
# Build: Config.pl installs the tree (it writes Makefile.conf and Makefile.def
# with this working copy's path), selects the equation set, user module, block
# size and ghost layers of the upstream test, then builds BATSRUS.exe and the
# PostIDL converter.
# ---- build (run-scoped verified binary reuse; seconds exclude scientific run time)
cd "$WORK/src"
MPI_INC="$(mpif90 -showme:compile 2>/dev/null || true)"
BUILD_GROUP="mhyp-default-ng3-g8x8x1"
BUILD_SPEC="Config.pl -default -e=MhdHyp -u=Default -ng=3 -g=8,8,1"
BUILD_TASK="swmf-batsrus"
BUILD_TARGET="a100-sxm4-80gb"
BUILD_TARGET_SHA256="fec36b64e17d0893720e55b74a78c61d4e0f5cfc796322ff92f1a3b04a53c132"
BUILD_MODE=normal
if [ "$IC" = altbuild ]; then BUILD_MODE=altbuild; fi
CACHE_ENABLED=0
if [ -n "${SAB_BUILD_CACHE_ROOT:-}" ] && [ -n "${SAB_SOURCE_FINGERPRINT:-}" ]; then
  CACHE_ENABLED=1
fi

resolve_build_paths() {
  local path_record
  path_record="$(python3 - "$PWD" <<'PY_BUILD_PATHS'
import os, re, sys

root = os.path.abspath(sys.argv[1])
candidates = [os.path.join(root, "Makefile.def"),
              os.path.join(root, "src", "Makefile.def"),
              os.path.join(root, "srcBATL", "Makefile.def")]

def read_vars(path):
    out = {}
    try:
        stream = open(path, encoding="utf-8")
    except OSError:
        return out
    with stream:
        for raw in stream:
            line = raw.split("#", 1)[0].strip()
            match = re.match(r"^(?:export\s+)?([A-Za-z_]\w*)\s*(?:\?|\+)?=\s*(.*?)\s*$", line)
            if match:
                value = match.group(2).strip()
                if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
                    value = value[1:-1]
                out[match.group(1)] = value
    return out

defs = [(path, read_vars(path)) for path in candidates]
selected = next(((path, values) for path, values in defs if "BINDIR" in values), None)
if selected is None:
    raise SystemExit("generated/source Makefile.def has no configured BINDIR")
selected_path, selected_vars = selected
all_vars = {}
for _, values in defs:
    all_vars.update(values)
# Config.pl may retain a self-reference such as GMDIR=${GMDIR} in src/Makefile.def;
# in the copied tree the configured root is the only safe expansion for that cycle.
defaults = {"GMDIR": root, "DIR": root, "CURDIR": root, "PWD": root}
pattern = re.compile(r"\$\{([A-Za-z_]\w*)\}|\$\(([A-Za-z_]\w*)\)|\$([A-Za-z_]\w*)")
resolving = set()
def resolve_var(name):
    if name in resolving:
        return defaults.get(name, "")
    raw = all_vars.get(name, defaults.get(name, ""))
    resolving.add(name)
    try:
        for _ in range(20):
            changed = False
            def replace(match):
                nonlocal changed
                other = next(group for group in match.groups() if group is not None)
                replacement = resolve_var(other)
                if replacement != match.group(0):
                    changed = True
                return replacement
            expanded = pattern.sub(replace, raw)
            raw = expanded
            if not changed:
                break
        return raw.strip().strip("\"'")
    finally:
        resolving.remove(name)

def resolve_path(value):
    value = resolve_var(value) if re.fullmatch(r"[A-Za-z_]\w*", value) else value
    value = pattern.sub(lambda m: resolve_var(next(group for group in m.groups() if group is not None)), value)
    if pattern.search(value):
        raise SystemExit("unresolved configured path: " + value)
    return os.path.normpath(value if os.path.isabs(value) else os.path.join(root, value))

bindir = resolve_path(selected_vars["BINDIR"])
gmdir = resolve_path("GMDIR")
print(gmdir + "\t" + bindir + "\t" + os.path.basename(selected_path))
PY_BUILD_PATHS
)" || { echo "run.sh: could not resolve configured GMDIR/BINDIR" >&2; exit 3; }
  IFS=$'\t' read -r CONFIG_GMDIR CONFIG_BINDIR CONFIG_DEF <<EOF_PATHS
$path_record
EOF_PATHS
  [ -n "$CONFIG_GMDIR" ] && [ -n "$CONFIG_BINDIR" ] || { echo "run.sh: empty configured GMDIR/BINDIR" >&2; exit 3; }
  BINARY_DIR="$CONFIG_BINDIR"
  BATSRUS_BINARY="$BINARY_DIR/BATSRUS.exe"
  POSTIDL_BINARY="$BINARY_DIR/PostIDL.exe"
  printf 'GMDIR=%s\nBINDIR=%s\nsource=%s\n' \
    "$CONFIG_GMDIR" "$CONFIG_BINDIR" "$CONFIG_DEF" > "$WORK/configured-paths.txt"
}

configure_source() {
  ./Config.pl -install -compiler=gfortran > "$WORK/install.log" 2>&1 \
    || { tail -40 "$WORK/install.log" >&2; echo "run.sh: Config.pl -install failed" >&2; exit 3; }
  # The shipped gfortran template compiles with plain gfortran and links with
  # mpif90. INCL_EXTRA is the template hook for the MPI include flags.
  MPI_INC="$(mpif90 -showme:compile 2>/dev/null || true)"
  printf 'INCL_EXTRA = %s\n' "$MPI_INC" >> Makefile.conf
  ./Config.pl -default -e=MhdHyp -u=Default -ng=3 -g=8,8,1 >> "$WORK/config.log" 2>&1 \
    || { tail -40 "$WORK/config.log" >&2; echo "run.sh: Config.pl -default -e=MhdHyp -u=Default -ng=3 -g=8,8,1 failed" >&2; exit 3; }
  if [ "$IC" = altbuild ]; then
    ./Config.pl -O0 >> "$WORK/config.log" 2>&1 \
      || { tail -40 "$WORK/config.log" >&2; echo "run.sh: Config.pl -O0 failed" >&2; exit 3; }
    grep -q '^OPT3 = -O0' Makefile.conf \
      || { echo "run.sh: Config.pl -O0 did not set OPT3 in Makefile.conf" >&2; exit 3; }
  fi
  resolve_build_paths
}

build_source() {
  make -j"$SAB_MAKE_JOBS" BATSRUS > "$WORK/build.log" 2>&1 \
    || { tail -60 "$WORK/build.log" >&2; echo "run.sh: BATSRUS build failed" >&2; exit 3; }
  make PIDL >> "$WORK/build.log" 2>&1 \
    || { tail -40 "$WORK/build.log" >&2; echo "run.sh: PostIDL build failed" >&2; exit 3; }
}

if [ "$CACHE_ENABLED" -eq 1 ]; then
  COMPILER_VERSION="$(gfortran --version 2>&1 || true)"
  MAKE_VERSION="$(make --version 2>&1 || true)"
  MPI_VERSION="$(mpif90 --version 2>&1 || true)"
  BUILD_FINGERPRINT="$(printf '%s\0' \
      "cache-schema=batsrus-build-v3" \
      "task=$BUILD_TASK" \
      "source-fingerprint=$SAB_SOURCE_FINGERPRINT" \
      "source-root=$SOURCE_DIR" \
      "build-group=$BUILD_GROUP" \
      "build-spec=$BUILD_SPEC" \
      "build-mode=$BUILD_MODE" \
      "target=$BUILD_TARGET" \
      "target-descriptor-sha256=$BUILD_TARGET_SHA256" \
      "runner=linux-docker" \
      "make-targets=BATSRUS,PIDL" \
      "make-jobs=$SAB_MAKE_JOBS" \
      "mpi-ranks=$SAB_MPI_RANKS" \
      "mpi-include=$MPI_INC" \
      "compiler=gfortran" \
      "compiler-version=$COMPILER_VERSION" \
      "make-version=$MAKE_VERSION" \
      "mpi-version=$MPI_VERSION" \
      "machine=$(uname -m)" | sha256sum | cut -d' ' -f1)"
  CACHE_DIR="$SAB_BUILD_CACHE_ROOT/$BUILD_TASK/$BUILD_GROUP/$BUILD_FINGERPRINT"
  CACHE_BINARY="$CACHE_DIR/BATSRUS.exe"
  CACHE_POSTIDL="$CACHE_DIR/PostIDL.exe"
  CACHE_DIGEST_FILE="$CACHE_DIR/binaries.sha256"
  CACHE_READY="$CACHE_DIR/ready.sha256"
  CACHE_HIT=0
  if [ -x "$CACHE_BINARY" ] && [ -x "$CACHE_POSTIDL" ] \
      && [ -f "$CACHE_DIGEST_FILE" ] && [ -f "$CACHE_READY" ]; then
    READY_FINGERPRINT="$(cat "$CACHE_READY" 2>/dev/null || true)"
    EXPECTED_BINARY_DIGEST="$(cat "$CACHE_DIGEST_FILE" 2>/dev/null || true)"
    ACTUAL_BINARY_DIGEST="$(sha256sum "$CACHE_BINARY" "$CACHE_POSTIDL" 2>/dev/null | awk '{printf "%s%s", sep, $1; sep=" ";} END {print ""}' || true)"
    if [ "$READY_FINGERPRINT" = "$BUILD_FINGERPRINT" ] \
        && [ -n "$EXPECTED_BINARY_DIGEST" ] \
        && [ "$EXPECTED_BINARY_DIGEST" = "$ACTUAL_BINARY_DIGEST" ]; then
      CACHE_HIT=1
    fi
  fi
  if [ "$CACHE_HIT" -eq 1 ]; then
    configure_source
    if mkdir -p "$BINARY_DIR" \
        && cp "$CACHE_BINARY" "$BATSRUS_BINARY" \
        && cp "$CACHE_POSTIDL" "$POSTIDL_BINARY" \
        && [ -x "$BATSRUS_BINARY" ] \
        && [ -x "$POSTIDL_BINARY" ]; then
      echo "SAB_BUILD_CACHE=hit group=$BUILD_GROUP fingerprint=$BUILD_FINGERPRINT variant=$IC altbuild=$BUILD_MODE bindir=$CONFIG_BINDIR"
      BUILD_SECONDS=0
    else
      echo "run.sh: cached BATSRUS/PostIDL binaries could not be copied to configured BINDIR=$CONFIG_BINDIR; rebuilding" >&2
      CACHE_HIT=0
    fi
  fi
  if [ "$CACHE_HIT" -eq 0 ]; then
    echo "SAB_BUILD_CACHE=miss group=$BUILD_GROUP fingerprint=$BUILD_FINGERPRINT variant=$IC altbuild=$BUILD_MODE"
    BUILD_START=$(date +%s)
    configure_source
    build_source
    BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
    [ -x "$BATSRUS_BINARY" ] || { echo "run.sh: build did not produce configured BATSRUS.exe at $BATSRUS_BINARY" >&2; exit 3; }
    [ -x "$POSTIDL_BINARY" ] || { echo "run.sh: build did not produce configured PostIDL.exe at $POSTIDL_BINARY" >&2; exit 3; }
    # Publish binaries only after both are built and their combined digest is
    # written; the ready fingerprint is the final marker of a valid entry.
    if mkdir -p "$CACHE_DIR" \
        && printf '%s\n' building > "$CACHE_READY" \
        && cp "$BATSRUS_BINARY" "$CACHE_BINARY" \
        && cp "$POSTIDL_BINARY" "$CACHE_POSTIDL" \
        && sha256sum "$CACHE_BINARY" "$CACHE_POSTIDL" | awk '{printf "%s%s", sep, $1; sep=" ";} END {print ""}' > "$CACHE_DIGEST_FILE" \
        && printf '%s\n' "$BUILD_FINGERPRINT" > "$CACHE_READY"; then
      echo "SAB_BUILD_CACHE=published group=$BUILD_GROUP fingerprint=$BUILD_FINGERPRINT variant=$IC altbuild=$BUILD_MODE bindir=$CONFIG_BINDIR"
    else
      echo "run.sh: warning: could not publish build cache for group $BUILD_GROUP; using local build" >&2
    fi
  fi
else
  echo "SAB_BUILD_CACHE=disabled reason=missing solve-scoped source fingerprint or cache root"
  BUILD_START=$(date +%s)
  configure_source
  build_source
  BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
fi
echo "SAB_BUILD_SECONDS=$BUILD_SECONDS"   # zero means no compile on a verified cache hit

# Run directory exactly as the upstream test builds it.
make rundir RUNDIR="$WORK/run" STANDALONE=YES GMDIR="$CONFIG_GMDIR" > "$WORK/rundir.log" 2>&1
# The knob rescales every #STOP window and the #ENDTIME of a deck that has one;
# at the graded default of 1 the deck is copied through unchanged.
python3 - "$CHECK_DIR/ic/$INPUTS/PARAM.in" "$WORK/run/PARAM.in" "$SAB_STOP_SCALE" "$SAB_PLOT_FRAMES" <<'PY'
import datetime, re, sys
src, dst, scale, frames = sys.argv[1], sys.argv[2], float(sys.argv[3]), float(sys.argv[4])
lines = open(src, encoding="utf-8").read().split("\n")

def rewrite(index, value, integer):
    parts = lines[index].split(None, 1)
    tail = "\t\t\t" + parts[1] if len(parts) > 1 else ""
    lines[index] = (("%d" % value) if integer else ("%.10g" % value)) + tail

def clock(index):
    return [int(float(lines[index + k].split()[0])) for k in range(1, 7)]

# Frame rule (2026-09-13 window/frame revision): the graded #SAVEPLOT cadence is
# rewritten from the window this run actually covers (scaled by SAB_STOP_SCALE) and
# SAB_PLOT_FRAMES, so at least that many frames of the graded series are written
# before the run ends. Sessions are tracked by #RUN markers; the window for a given
# #SAVEPLOT occurrence is (last #STOP value in the file) - (the #STOP value that
# ended the session before this one), scaled. Collected from the ORIGINAL (pre-scale)
# lines, before the #STOP/#ENDTIME rewrite below mutates them in place.
stop_values = []
run_count_at = []
runs_seen = 0
for i, line in enumerate(lines):
    run_count_at.append(runs_seen)
    s = line.strip()
    if s.startswith("#RUN"):
        runs_seen += 1
    elif s == "#STOP":
        for k in (i + 1, i + 2):
            if k < len(lines) and lines[k].split():
                try:
                    v = float(lines[k].split()[0])
                except ValueError:
                    v = 0.0
                if v > 0:
                    stop_values.append(v)
                    break

if scale != 1.0:
    for i, line in enumerate(list(lines)):
        if line.strip() == "#STOP":
            for k, integer in ((i + 1, True), (i + 2, False)):
                if k >= len(lines) or not lines[k].split():
                    break
                try:
                    value = float(lines[k].split()[0])
                except ValueError:
                    break
                if value > 0:
                    rewrite(k, max(1, int(round(value * scale))) if integer else value * scale, integer)
        elif line.strip() == "#ENDTIME":
            start = max(j for j in range(i) if lines[j].strip() == "#STARTTIME")
            t0 = datetime.datetime(*clock(start))
            t1 = t0 + (datetime.datetime(*clock(i)) - t0) * scale
            for k, value in zip(range(i + 1, i + 7), (t1.year, t1.month, t1.day, t1.hour, t1.minute, t1.second)):
                rewrite(k, value, True)

TARGETS = ["z=0 VAR idl"]
for label in TARGETS:
    occurrences = [i for i, line in enumerate(lines) if re.match(re.escape(label) + r"(\s|$)", line.strip())]
    if not occurrences:
        continue
    # Window is measured from where this series FIRST starts being written (it may be
    # re-cadenced later at a session boundary that only touches Dn/DtSavePlot) through
    # the end of the whole run; the same cadence is then applied to every occurrence,
    # so an early coarse declaration cannot leave a later fine one stranded, and an
    # early fine one is not left firing far more often than the rule requires.
    first = occurrences[0]
    r = run_count_at[first]
    preceding = stop_values[r - 1] if r >= 1 and r - 1 <= len(stop_values) - 1 else 0.0
    final = stop_values[-1] if stop_values else 0.0
    window = max(0.0, final - preceding) * scale
    for occ in occurrences:
        for k, integer in ((occ + 1, True), (occ + 2, False)):
            if k >= len(lines) or not lines[k].split():
                break
            try:
                cur = float(lines[k].split()[0])
            except ValueError:
                continue
            if cur > 0 and frames > 0 and window > 0:
                new_val = window / frames
                rewrite(k, max(1, int(round(new_val))) if integer else new_val, integer)

open(dst, "w", encoding="utf-8").write("\n".join(lines))
PY

cd "$WORK/run"
if ! mpiexec -n 2 --oversubscribe ./BATSRUS.exe > runlog 2>&1 < /dev/null; then
  echo "run.sh: BATSRUS.exe failed; last lines of the run log follow" >&2
  tail -40 runlog >&2
  exit 1
fi
./PostProc.pl -M -f=ascii -replace RESULTS > postproc.log 2>&1 < /dev/null

# Frame rule (2026-09-13): count the graded plot series actually written before grab.
cat_maybe_gz() { case "$1" in *.gz) gunzip -c "$1" 2>/dev/null ;; *) cat "$1" ;; esac; }
count_idl_frames() { local n=0 f; for f in "$@"; do [ -e "$f" ] || continue; n=$(( n + $(cat_maybe_gz "$f" | grep -c -E "^[[:space:]]*-?[0-9]+[[:space:]]+[-+0-9.eE]+[[:space:]]+-?[0-9]+[[:space:]]+[0-9]+[[:space:]]+[0-9]+[[:space:]]*$") )); done; echo "$n"; }
SAB_FRAMES_0=$(count_idl_frames RESULTS/GM/z=0_var_*.out*)  # z0_var.out
SAB_FRAME_COUNT=${SAB_FRAMES_0}
echo "SAB_PLOT_FRAMES=$SAB_FRAME_COUNT"
if [ "$SAB_FRAME_COUNT" -lt 5 ]; then
  echo "run.sh: graded plot series wrote only $SAB_FRAME_COUNT frame(s) before the run ended, need >= 5" >&2
  exit 1
fi

# The graded files, under the fixed names rubric.json lists.
grab() {
  local dest="$1" last="" f; shift
  for f in "$@"; do [ -e "$f" ] && last="$f"; done
  [ -n "$last" ] || { echo "run.sh: no output file matched: $*" >&2; exit 1; }
  case "$last" in
    *.gz) gunzip -c "$last" > "$OUT_DIR/$dest" ;;
    *) cp "$last" "$OUT_DIR/$dest" ;;
  esac
}
grab log.log RESULTS/GM/log_n*.log*
grab z0_var.out RESULTS/GM/z=0_var_*.out*
