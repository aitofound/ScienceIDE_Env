#!/usr/bin/env bash
# Check turb-driven: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     nominal inputs on configure.py -debug (same FFT library and switches)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_RES_SCALE=0.5 sab.py task selfcheck ...
# Parallel build jobs default to the CPUs this container may use (cgroup v2 cpu.max), not the host count.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_RES_SCALE "1" "multiplies every mesh dimension of the deck, the meshblock staying 16^3 as upstream (default: 32x16x16 cells in 2 meshblocks; SAB_RES_SCALE=2 is the upstream 64x32x32 in 16 meshblocks); runtime scales with the cube"
knob SAB_NLIM_SCALE "1" "multiplies the graded window, the deck's time/nlim (default: 32 cycles, about t = 0.28 against the upstream t = 0.3); runtime scales linearly and the window is what keeps the chaotic spread bounded, so shortening it is safe and lengthening it is not"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for each build of the pinned source (default: the CPUs allowed to this container); each job needs about 0.2 GB"
# Alternative build: Athena++'s own configure.py -debug build (-O0 -g), with the same compiler,
# FFTW library and all other configure switches. It runs ic/nominal; selfcheck measures the floor from it.
ALTBUILD="configure.py -debug: Athena++'s -O0 -g build with the same compiler, FFTW library and all other configure switches unchanged"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"; CONFIGURE_EXTRA=()
if [ "$IC" = altbuild ]; then INPUTS=nominal; CONFIGURE_EXTRA=(-debug); fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
# Open MPI inside a container: no shared-memory single-copy transport, and the driver allows root.
export OMPI_MCA_btl_vader_single_copy_mechanism=none
export OMPI_ALLOW_RUN_AS_ROOT="${OMPI_ALLOW_RUN_AS_ROOT:-1}" OMPI_ALLOW_RUN_AS_ROOT_CONFIRM="${OMPI_ALLOW_RUN_AS_ROOT_CONFIRM:-1}"
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"
cd "$WORK/src"

# Upstream test this check reproduces: code/athena/tst/regression/scripts/tests/turb/turb_3d.py
# Build: normal binaries are cached privately under this solve's output root.  The
# key is SHA-256 over the exact ordered configure.py arguments.  A missing or
# invalid entry takes this check's complete configure-and-make fallback; altbuild
# always bypasses the normal cache.
configure_fingerprint() {
  python3 - "$@" <<'PY'
import hashlib
import os
import sys

h = hashlib.sha256()


def add(value):
    data = os.fsencode(value)
    h.update(str(len(data)).encode("ascii") + b"\0" + data)


add("athena-configure-args-v1")
for arg in sys.argv[1:]:
    add(arg)
print(h.hexdigest())
PY
}

build() {
  local tag=$1; shift
  local fingerprint="" cache_root="" cache_dir="" cache_binary=""
  local cache_digest_file="" cache_ready="" ready="" expected_digest="" actual_digest=""
  local cache_hit=0 build_seconds
  local -a configure_args=("${CONFIGURE_EXTRA[@]}" "$@")
  make clean > /dev/null 2>&1 || true

  if [ "$IC" != altbuild ]; then
    fingerprint="$(configure_fingerprint "${configure_args[@]}")"
    cache_root="$(dirname "$OUT_DIR")/.athena-normal-build-cache"
    cache_dir="$cache_root/$fingerprint"
    cache_binary="$cache_dir/athena"
    cache_digest_file="$cache_dir/athena.sha256"
    cache_ready="$cache_dir/ready.sha256"
    if [ -x "$cache_binary" ] && [ -f "$cache_digest_file" ] && [ -f "$cache_ready" ]; then
      ready="$(cat "$cache_ready" 2>/dev/null || true)"
      expected_digest="$(cat "$cache_digest_file" 2>/dev/null || true)"
      actual_digest="$(sha256sum "$cache_binary" | cut -d' ' -f1)"
      if [ "$ready" = "$fingerprint" ] && [ -n "$expected_digest" ] \
         && [ "$expected_digest" = "$actual_digest" ]; then
        cache_hit=1
      fi
    fi
  fi

  if [ "$cache_hit" -eq 1 ]; then
    echo "SAB_BUILD_CACHE=hit fingerprint=$fingerprint"
    cp "$cache_binary" "$WORK/athena-$tag"
    build_seconds=0
  else
    if [ "$IC" = altbuild ]; then
      echo "SAB_BUILD_CACHE=bypass reason=altbuild"
    else
      echo "SAB_BUILD_CACHE=miss fingerprint=$fingerprint"
    fi
    BUILD_START=$(date +%s)
    python3 configure.py "${configure_args[@]}" > "$WORK/configure-$tag.log"
    make -j"$SAB_MAKE_JOBS" > "$WORK/make-$tag.log" 2>&1
    build_seconds=$(( $(date +%s) - BUILD_START ))
    # A real cache miss must remain distinguishable from reuse even if a tiny
    # build straddles no whole-second clock tick.
    [ "$build_seconds" -gt 0 ] || build_seconds=1
    cp bin/athena "$WORK/athena-$tag"
    if [ "$IC" != altbuild ]; then
      mkdir -p "$cache_dir"
      cp "$WORK/athena-$tag" "$cache_binary"
      sha256sum "$cache_binary" | cut -d' ' -f1 > "$cache_digest_file"
      printf '%s\n' "$fingerprint" > "$cache_ready"
    fi
  fi
  SAB_BUILD_TOTAL=$(( ${SAB_BUILD_TOTAL:-0} + build_seconds ))
  echo "SAB_BUILD_SECONDS=$SAB_BUILD_TOTAL"   # actual compile total; exactly zero when every requested normal binary was reused
}
build ser -fft --prob=turb --coord=cartesian
build mpi -mpi -fft --prob=turb --coord=cartesian

# The knobs rescale the deck's own settings; every launch of an initial condition gets the same ones.
overrides() { python3 - "$1" "$SAB_RES_SCALE" "$SAB_NLIM_SCALE" <<'PY'
import re, sys
txt, rs, ns = open(sys.argv[1]).read(), float(sys.argv[2]), float(sys.argv[3])
def get(block, key):
    m = re.search(rf"<{block}>.*?^{key}\s*=\s*([^\s#]+)", txt, re.S | re.M)
    return m.group(1) if m else None
out = []
for k in ("nx1", "nx2", "nx3"):        # the mesh only; the meshblock stays 16^3, as upstream
    v = get("mesh", k)
    if v is None or int(v) == 1: continue
    out.append(f"mesh/{k}={max(16, int(round(int(v) * rs)))}")
nlim = max(1, int(round(int(get("time", "nlim")) * ns)))
out += [f"time/nlim={nlim}", f"output1/dcycle={nlim}"]
print(" ".join(out))
PY
}

launch() { # launch <deck> <build tag> <ranks: 0 runs the binary directly> <run id>
  local deck="$CHECK_DIR/ic/$INPUTS/$1.athinput" tag=$2 np=$3 rid=$4 ov
  ov="$(overrides "$deck")"
  mkdir -p "$WORK/run/$rid"
  # stdin is closed on every launch: mpirun forwards and drains the caller's stdin, which would eat
  # the check list the produce driver is reading from.
  if [ "$np" -eq 0 ]; then
    ( cd "$WORK/run/$rid" && "$WORK/athena-$tag" -i "$deck" -d . job/problem_id="$rid" $ov > run.log 2>&1 < /dev/null )
  else
    ( cd "$WORK/run/$rid" && mpirun --oversubscribe -np "$np" "$WORK/athena-$tag" -i "$deck" -d . job/problem_id="$rid" $ov > run.log 2>&1 < /dev/null )
  fi
}
launch turb ser 0 turb_ser
launch turb mpi 2 turb_np2

# Graded files: the state at the end of the window of every meshblock of every launch,
# full double precision (data_format = %24.16e in the decks).
for f in "$WORK"/run/*/*.out1.00001.tab; do cp "$f" "$OUT_DIR/$(basename "$f" .out1.00001.tab).tab"; done
