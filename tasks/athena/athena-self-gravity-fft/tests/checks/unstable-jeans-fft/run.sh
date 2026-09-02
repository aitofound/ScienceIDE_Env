#!/usr/bin/env bash
# Check unstable-jeans-fft: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh --help                       list the runtime knobs below
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_RES_SCALE=0.5 sab.py task selfcheck ...
# Parallel build jobs default to the CPUs this container may use (cgroup v2 cpu.max), not the host count.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_RES_SCALE "1" "multiplies every mesh dimension of both decks, the meshblock staying 16^3 as upstream (default: 32x16x16 and 64x32x32; SAB_RES_SCALE=2 is the upstream 64x32x32 and 128x64x64); runtime scales with the fourth power"
knob SAB_TLIM_SCALE "1" "multiplies the end time of both decks (default and upstream: 0.046, one e-folding time 1/omega of the unstable mode); runtime scales linearly"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for each build of the pinned source (default: the CPUs allowed to this container); each job needs about 0.2 GB"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
[ -d "$CHECK_DIR/ic/$IC" ] || { echo "run.sh: no initial condition ic/$IC" >&2; exit 2; }
# Open MPI inside a container: no shared-memory single-copy transport, and the driver allows root.
export OMPI_MCA_btl_vader_single_copy_mechanism=none
export OMPI_ALLOW_RUN_AS_ROOT="${OMPI_ALLOW_RUN_AS_ROOT:-1}" OMPI_ALLOW_RUN_AS_ROOT_CONFIRM="${OMPI_ALLOW_RUN_AS_ROOT_CONFIRM:-1}"
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"
cd "$WORK/src"

# Upstream test this check reproduces: code/athena/tst/regression/scripts/tests/grav/unstable_jeans_3d_fft.py
# Build: the one configuration(s) of the upstream test, one binary each, kept as $WORK/athena-<tag>.
build() { local tag=$1; shift; make clean > /dev/null 2>&1 || true
  BUILD_START=$(date +%s)
  python3 configure.py "$@" > "$WORK/configure-$tag.log"
  make -j"$SAB_MAKE_JOBS" > "$WORK/make-$tag.log" 2>&1
  SAB_BUILD_TOTAL=$(( ${SAB_BUILD_TOTAL:-0} + $(date +%s) - BUILD_START ))
  echo "SAB_BUILD_SECONDS=$SAB_BUILD_TOTAL"   # running total over this check's builds; the driver records the last line, the budget counts run time only
  cp bin/athena "$WORK/athena-$tag"; }
build fft -fft --prob=jeans --grav=fft --coord=cartesian

# The knobs rescale the deck's own settings; every launch of an initial condition gets the same ones.
overrides() { python3 - "$1" "$SAB_RES_SCALE" "$SAB_TLIM_SCALE" <<'PY'
import re, sys
txt, rs, ts = open(sys.argv[1]).read(), float(sys.argv[2]), float(sys.argv[3])
def get(block, key):
    m = re.search(rf"<{block}>.*?^{key}\s*=\s*([^\s#]+)", txt, re.S | re.M)
    return m.group(1) if m else None
out = []
for k in ("nx1", "nx2", "nx3"):        # the mesh only; the meshblock stays 16^3, as upstream
    v = get("mesh", k)
    if v is None or int(v) == 1: continue
    out.append(f"mesh/{k}={max(16, int(round(int(v) * rs)))}")
tlim = float(get("time", "tlim")) * ts
out += [f"time/tlim={tlim!r}", f"output1/dt={tlim!r}"]
print(" ".join(out))
PY
}

launch() { # launch <deck> <build tag> <ranks: 0 runs the binary directly> <run id>
  local deck="$CHECK_DIR/ic/$IC/$1.athinput" tag=$2 np=$3 rid=$4 ov
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
launch jeans_unstable_r16 fft 0 jeans_unstable_r16
launch jeans_unstable_r32 fft 0 jeans_unstable_r32

# Graded files: the state at the end of the window of every meshblock of every launch,
# full double precision (data_format = %24.16e in the decks).
for f in "$WORK"/run/*/*.out1.00001.tab; do cp "$f" "$OUT_DIR/$(basename "$f" .out1.00001.tab).tab"; done
