#!/usr/bin/env bash
# Check rad-amr-linwave: the TEST half of the check.
#   run.sh nominal | run.sh variant | run.sh altbuild
#                                           run one initial condition (see ic/)
#   run.sh --help                       list the runtime knobs and alternative build below
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_NX1_SCALE=0.5 sab.py task selfcheck ...
# Parallel build jobs default to the CPUs this container may use (cgroup v2 cpu.max), not the host count.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_NX1_SCALE "1" "multiplies mesh/nx1 and meshblock/nx1 of the deck (upstream: 128 root cells in 8-cell blocks); runtime scales with the square"
knob SAB_TLIM_SCALE "1" "multiplies the end time of every deck (upstream: 0.7745966144169111); runtime scales linearly"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the one build of the pinned source (default: the CPUs allowed to this container); each job needs about 0.2 GB"
ALTBUILD="configure.py -debug: Athena++'s same-compiler -O0 -g debug build, with all other configure switches and the nominal inputs unchanged"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"; CONFIGURE_EXTRA=()
if [ "$IC" = altbuild ]; then INPUTS=nominal; CONFIGURE_EXTRA=(-debug); fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

# This group name and configure argv are identical only in this check's exact
# recipe peer. Other Athena++ configurations use distinct cache namespaces.
BUILD_GROUP="explicit-rad-linearwave"
CONFIGURE_ARGS=("-nr_radiation" "--prob=rad_linearwave" "--coord=cartesian" "--flux=hllc")

# Fingerprint the complete normal-build input: exact configure argv, pinned
# source bytes and metadata, tool identities, make parallelism, and architecture.
normal_build_fingerprint() {
  python3 - "$SOURCE_DIR" "$BUILD_GROUP" "$SAB_MAKE_JOBS" "${CONFIGURE_ARGS[@]}" <<'PYFINGERPRINT'
import hashlib
import os
import stat
import subprocess
import sys

root, group, make_jobs, *configure_args = sys.argv[1:]
h = hashlib.sha256()


def add_field(name, value):
    data = os.fsencode(value)
    h.update(os.fsencode(name) + b"\0" + str(len(data)).encode("ascii") + b"\0" + data)


add_field("cache-schema", "athena-normal-build-v1")
add_field("group", group)
add_field("configure-program", "python3 configure.py")
for index, arg in enumerate(configure_args):
    add_field(f"configure-arg-{index}", arg)
add_field("make-target", "default")
add_field("make-jobs", make_jobs)
for name, command in (
    ("python-version", ["python3", "--version"]),
    ("cxx-version", ["g++", "--version"]),
    ("make-version", ["make", "--version"]),
):
    proc = subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    add_field(name, os.fsdecode(proc.stdout))
add_field("machine", os.uname().machine)

root = os.path.realpath(root)


def add_tree(directory, relative="."):
    with os.scandir(directory) as entries:
        ordered = sorted(entries, key=lambda entry: os.fsencode(entry.name))
    for entry in ordered:
        rel = entry.name if relative == "." else os.path.join(relative, entry.name)
        st = entry.stat(follow_symlinks=False)
        add_field("path", rel)
        add_field("mode", format(stat.S_IMODE(st.st_mode), "04o"))
        if stat.S_ISLNK(st.st_mode):
            add_field("kind", "symlink")
            add_field("target", os.readlink(entry.path))
        elif stat.S_ISDIR(st.st_mode):
            add_field("kind", "directory")
            add_tree(entry.path, rel)
        elif stat.S_ISREG(st.st_mode):
            add_field("kind", "file")
            h.update(str(st.st_size).encode("ascii") + b"\0")
            with open(entry.path, "rb") as stream:
                while block := stream.read(1024 * 1024):
                    h.update(block)
        else:
            raise SystemExit(f"run.sh: unsupported source entry for build cache: {rel}")


add_tree(root)
print(h.hexdigest())
PYFINGERPRINT
}

build_athena() {
  cd "$WORK/src"
  if ! python3 configure.py ${CONFIGURE_EXTRA[@]+"${CONFIGURE_EXTRA[@]}"} "${CONFIGURE_ARGS[@]}" > "$WORK/configure.log" 2>&1; then
    echo "run.sh: configure failed for group $BUILD_GROUP:" >&2
    tail -20 "$WORK/configure.log" >&2
    exit 1
  fi
  if ! make -j"$SAB_MAKE_JOBS" > "$WORK/make.log" 2>&1; then
    echo "run.sh: build failed for group $BUILD_GROUP:" >&2
    tail -30 "$WORK/make.log" >&2
    exit 1
  fi
}

# Upstream test this check reproduces: code/athena/tst/regression/scripts/tests/nr_radiation/amr_linwave.py
# Build: the configuration of the upstream test, one binary for all decks.
if [ "$IC" = altbuild ]; then
  # Every alternative build remains independent and never reads or populates
  # the normal cache, even when another check has the same normal recipe.
  echo "SAB_BUILD_CACHE=bypass reason=altbuild group=$BUILD_GROUP"
  BUILD_START=$(date +%s)
  build_athena
  BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
else
  BUILD_FINGERPRINT="$(normal_build_fingerprint)"
  CACHE_ROOT="$(dirname "$OUT_DIR")/.athena-normal-build-cache"
  CACHE_DIR="$CACHE_ROOT/$BUILD_GROUP/$BUILD_FINGERPRINT"
  CACHE_BINARY="$CACHE_DIR/athena"
  CACHE_DIGEST_FILE="$CACHE_DIR/athena.sha256"
  CACHE_READY="$CACHE_DIR/ready.sha256"
  CACHE_HIT=0

  # The ready marker is published last. Missing or malformed metadata, a
  # missing executable, or a digest mismatch is a miss and takes this script's
  # complete self-contained configure+make fallback.
  if [ -x "$CACHE_BINARY" ] && [ -f "$CACHE_DIGEST_FILE" ] && [ -f "$CACHE_READY" ]; then
    READY_FINGERPRINT="$(cat "$CACHE_READY" 2>/dev/null || true)"
    EXPECTED_BINARY_DIGEST="$(cat "$CACHE_DIGEST_FILE" 2>/dev/null || true)"
    ACTUAL_BINARY_DIGEST="$(sha256sum "$CACHE_BINARY" 2>/dev/null | cut -d' ' -f1 || true)"
    if [ "$READY_FINGERPRINT" = "$BUILD_FINGERPRINT" ]        && [ -n "$EXPECTED_BINARY_DIGEST" ]        && [ "$EXPECTED_BINARY_DIGEST" = "$ACTUAL_BINARY_DIGEST" ]; then
      CACHE_HIT=1
    fi
  fi

  if [ "$CACHE_HIT" -eq 1 ]; then
    mkdir -p "$WORK/src/bin"
    if cp "$CACHE_BINARY" "$WORK/src/bin/athena"; then
      echo "SAB_BUILD_CACHE=hit group=$BUILD_GROUP fingerprint=$BUILD_FINGERPRINT"
      BUILD_SECONDS=0
    else
      echo "run.sh: cached binary could not be copied; rebuilding group $BUILD_GROUP" >&2
      CACHE_HIT=0
    fi
  fi

  if [ "$CACHE_HIT" -eq 0 ]; then
    echo "SAB_BUILD_CACHE=miss group=$BUILD_GROUP fingerprint=$BUILD_FINGERPRINT"
    BUILD_START=$(date +%s)
    build_athena
    BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))

    # Cache publication is best effort: invalidate any old ready marker, copy
    # the binary, write its digest, then publish the fingerprint last. Failure
    # to publish never invalidates the fully built local fallback used below.
    if mkdir -p "$CACHE_DIR"        && printf '%s\n' building >"$CACHE_READY" 2>/dev/null        && cp "$WORK/src/bin/athena" "$CACHE_BINARY"        && sha256sum "$CACHE_BINARY" | cut -d' ' -f1 >"$CACHE_DIGEST_FILE"        && printf '%s\n' "$BUILD_FINGERPRINT" >"$CACHE_READY"; then
      echo "SAB_BUILD_CACHE=published group=$BUILD_GROUP fingerprint=$BUILD_FINGERPRINT"
    else
      echo "run.sh: warning: could not publish build cache for group $BUILD_GROUP; using local build" >&2
    fi
  fi
fi
echo "SAB_BUILD_SECONDS=$BUILD_SECONDS"   # nonzero on a compile; exactly zero only on a verified normal-cache hit

# Run every deck of this initial condition; the knobs rescale the deck's own mesh and end time.
for deck in "$CHECK_DIR/ic/$INPUTS"/*.athinput; do
  id="$(basename "$deck" .athinput)"
  overrides="$(python3 - "$deck" "$SAB_NX1_SCALE" "$SAB_TLIM_SCALE" <<'PY'
import re, sys
txt, rs, ts = open(sys.argv[1]).read(), float(sys.argv[2]), float(sys.argv[3])
def get(block, key):
    m = re.search(rf"<{block}>.*?^{key}\s*=\s*([^\s#]+)", txt, re.S | re.M); return m.group(1) if m else None
out = []
for blk in ("mesh", "meshblock"):
    for k in ("nx1",):
        v = get(blk, k)
        if v is None or int(v) == 1: continue
        out.append(f"{blk}/{k}={max(4, int(round(int(v) * rs)))}")
tlim = float(get("time", "tlim")) * ts
out += [f"time/tlim={tlim!r}", f"output1/dt={tlim!r}"]
print(" ".join(out))
PY
)"
  mkdir -p "$WORK/run/$id"
  ( cd "$WORK/run/$id" && "$WORK/src/bin/athena" -i "$deck" -d . $overrides > run.log 2>&1 )
  # Graded files: the state at t = tlim of every meshblock, full double precision (data_format in the deck).
  for f in "$WORK/run/$id"/"$id".block*.out1.00001.tab; do cp "$f" "$OUT_DIR/$(basename "$f" .out1.00001.tab).tab"; done
done
