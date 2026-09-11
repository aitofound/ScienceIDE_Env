#!/usr/bin/env bash
# Check sr-mhd-linwave: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (configure.py -debug; see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR as task inputs; no network; never modifies SOURCE_DIR.
# Normal runs may reuse the private per-solve cache beside OUT_DIR.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_RES_SCALE=0.5 sab.py task selfcheck ...
# Parallel build jobs default to the CPUs this container may use (cgroup v2 cpu.max), not the host count.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_RES_SCALE "1" "multiplies every mesh and meshblock dimension of every deck (upstream: 16/32 (3-D)); runtime scales with the square in 1-D and the fourth power in 3-D"
knob SAB_TLIM_SCALE "1" "multiplies the end time of every deck (upstream: 1.41 to 10 (one crossing time per mode)); runtime scales linearly"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the one build of the pinned source (default: the CPUs allowed to this container); each job needs about 0.2 GB"
# Alternative build: Athena++'s own configure.py -debug build uses the same compiler at -O0 -g instead of
# the default optimization. All other configure switches are unchanged; `run.sh altbuild` runs ic/nominal on it.
ALTBUILD="configure.py -debug: Athena++'s own -O0 -g build with the same compiler instead of the default optimized build; all other configure switches unchanged"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"; CONFIGURE_EXTRA=()
if [ "$IC" = altbuild ]; then INPUTS=nominal; CONFIGURE_EXTRA=(-debug); fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

# Upstream test this check reproduces: code/athena/tst/regression/scripts/tests/sr/sr_mhd_linwave.py
# Build: the configuration of the upstream test, one binary for all decks.
# The two linear-wave MHD checks have one byte-for-byte identical normal build
# recipe. Hash the full pinned source plus that complete recipe and the tools
# that interpret it; all other checks use distinct configurations and do not
# read or populate this cache.
normal_build_fingerprint() {
  python3 - "$SOURCE_DIR" <<'PY'
import hashlib
import os
import stat
import subprocess
import sys
root = os.path.realpath(sys.argv[1])
h = hashlib.sha256()
def add(name, value):
    data = os.fsencode(value)
    h.update(os.fsencode(name) + b"\0" + str(len(data)).encode("ascii") + b"\0" + data)
add("cache-schema", "athena-sr-mhd-linear-wave-normal-v1")
add("configure-recipe", "python3 configure.py -s -b --prob=gr_linear_wave --coord=cartesian --flux=hlld")
add("make-recipe", "make -j<SAB_MAKE_JOBS>")
add("SAB_MAKE_JOBS", os.environ["SAB_MAKE_JOBS"])
for name, command in (
    ("python-version", ["python3", "--version"]),
    ("cxx-version", ["g++", "--version"]),
    ("make-version", ["make", "--version"]),
):
    p = subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    add(name, os.fsdecode(p.stdout))
add("machine", os.uname().machine)
def add_tree(directory, relative="."):
    with os.scandir(directory) as entries:
        ordered = sorted(entries, key=lambda e: os.fsencode(e.name))
    for entry in ordered:
        rel = entry.name if relative == "." else os.path.join(relative, entry.name)
        st = entry.stat(follow_symlinks=False)
        add("path", rel); add("mode", format(stat.S_IMODE(st.st_mode), "04o"))
        if stat.S_ISLNK(st.st_mode):
            add("kind", "symlink"); add("target", os.readlink(entry.path))
        elif stat.S_ISDIR(st.st_mode):
            add("kind", "directory"); add_tree(entry.path, rel)
        elif stat.S_ISREG(st.st_mode):
            add("kind", "file"); h.update(str(st.st_size).encode("ascii") + b"\0")
            with open(entry.path, "rb") as stream:
                while block := stream.read(1024 * 1024): h.update(block)
        else: raise SystemExit(f"run.sh: unsupported source entry for build cache: {rel}")
add_tree(root)
print(h.hexdigest())
PY
}
build_athena() {
  local configure_log=$1 make_log=$2
  python3 configure.py -s -b --prob=gr_linear_wave --coord=cartesian --flux=hlld > "$configure_log"
  make -j"$SAB_MAKE_JOBS" > "$make_log" 2>&1
}
cd "$WORK/src"
if [ "$IC" = altbuild ]; then
  echo "SAB_BUILD_CACHE=bypass reason=altbuild"
  BUILD_START=$(date +%s)
  python3 configure.py "${CONFIGURE_EXTRA[@]}" -s -b --prob=gr_linear_wave --coord=cartesian --flux=hlld > "$WORK/configure.log"
  make -j"$SAB_MAKE_JOBS" > "$WORK/make.log" 2>&1
  BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
else
  BUILD_FINGERPRINT="$(normal_build_fingerprint)"
  CACHE_ROOT="$(dirname "$OUT_DIR")/.athena-sr-mhd-linear-wave-normal-cache"
  CACHE_DIR="$CACHE_ROOT/$BUILD_FINGERPRINT"
  CACHE_BINARY="$CACHE_DIR/athena"
  CACHE_DIGEST_FILE="$CACHE_DIR/athena.sha256"
  CACHE_READY="$CACHE_DIR/ready.sha256"
  CACHE_HIT=0
  if [ -x "$CACHE_BINARY" ] && [ -f "$CACHE_DIGEST_FILE" ] && [ -f "$CACHE_READY" ]; then
    READY_FINGERPRINT="$(cat "$CACHE_READY" 2>/dev/null || true)"
    EXPECTED_BINARY_DIGEST="$(cat "$CACHE_DIGEST_FILE" 2>/dev/null || true)"
    ACTUAL_BINARY_DIGEST="$(sha256sum "$CACHE_BINARY" | cut -d' ' -f1)"
    if [ "$READY_FINGERPRINT" = "$BUILD_FINGERPRINT" ] && [ -n "$EXPECTED_BINARY_DIGEST" ] && [ "$EXPECTED_BINARY_DIGEST" = "$ACTUAL_BINARY_DIGEST" ]; then CACHE_HIT=1; fi
  fi
  if [ "$CACHE_HIT" -eq 1 ]; then
    echo "SAB_BUILD_CACHE=hit fingerprint=$BUILD_FINGERPRINT"
    mkdir -p "$WORK/src/bin"; cp "$CACHE_BINARY" "$WORK/src/bin/athena"; BUILD_SECONDS=0
  else
    echo "SAB_BUILD_CACHE=miss fingerprint=$BUILD_FINGERPRINT"
    BUILD_START=$(date +%s); build_athena "$WORK/configure.log" "$WORK/make.log"
    BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
    mkdir -p "$CACHE_DIR"; cp "$WORK/src/bin/athena" "$CACHE_BINARY"
    sha256sum "$CACHE_BINARY" | cut -d' ' -f1 > "$CACHE_DIGEST_FILE"
    printf '%s\n' "$BUILD_FINGERPRINT" > "$CACHE_READY"
  fi
fi
echo "SAB_BUILD_SECONDS=$BUILD_SECONDS"   # nonzero on the group miss and exactly zero on a verified reuse hit

# Run every deck of this initial condition; the knobs rescale the deck's own mesh and end time.
for deck in "$CHECK_DIR/ic/$INPUTS"/*.athinput; do
  id="$(basename "$deck" .athinput)"
  overrides="$(python3 - "$deck" "$SAB_RES_SCALE" "$SAB_TLIM_SCALE" <<'PY'
import re, sys
txt, rs, ts = open(sys.argv[1]).read(), float(sys.argv[2]), float(sys.argv[3])
def get(block, key):
    m = re.search(rf"<{block}>.*?^{key}\s*=\s*([^\s#]+)", txt, re.S | re.M); return m.group(1) if m else None
out = []
for blk in ("mesh", "meshblock"):
    for k in ("nx1", "nx2", "nx3"):
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
