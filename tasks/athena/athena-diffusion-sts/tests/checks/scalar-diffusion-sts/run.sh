#!/usr/bin/env bash
# Check scalar-diffusion-sts: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     run nominal inputs on configure.py -debug (see ALTBUILD below)
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
knob SAB_RES_SCALE "1" "multiplies every mesh and meshblock dimension of every deck (upstream: 256 and 512 cells); runtime scales with the cube of the scale in 1-D (cells x steps)"
knob SAB_TLIM_SCALE "1" "multiplies the end time of every deck and the cadence of its outputs (upstream: t = 2.0); runtime scales linearly"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the one build of the pinned source (default: the CPUs allowed to this container); each job needs about 0.2 GB"
# Alternative build: Athena++'s configure.py -debug replaces the default compiler flags with -O0 -std=c++11 -g.
# It keeps the same compiler and all other configure switches. `run.sh altbuild` runs ic/nominal on it.
ALTBUILD="configure.py -debug: Athena++'s own -O0 -g build with the same compiler and all other configure switches unchanged"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"; CONFIGURE_EXTRA=()
if [ "$IC" = altbuild ]; then INPUTS=nominal; CONFIGURE_EXTRA=(-debug); fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

# Upstream test this check reproduces: code/athena/tst/regression/scripts/tests/diffusion/scalar_diffusion_sts.py
# Build: the configuration of the upstream test, one binary for all decks.
cd "$WORK/src"
BUILD_START=$(date +%s)
python3 configure.py "${CONFIGURE_EXTRA[@]}" -sts --prob=scalar_diff --coord=cartesian --flux=roe --eos=isothermal --nscalars=1 > "$WORK/configure.log"
make -j"$SAB_MAKE_JOBS" > "$WORK/make.log" 2>&1
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only

# Run every deck of this initial condition; the knobs rescale the deck's own mesh,
# end time and output cadence. Everything else is fixed by the deck.
for deck in "$CHECK_DIR/ic/$INPUTS"/*.athinput; do
  id="$(basename "$deck" .athinput)"
  overrides="$(python3 - "$deck" "$SAB_RES_SCALE" "$SAB_TLIM_SCALE" <<'PY'
import re, sys
txt, rs, ts = open(sys.argv[1]).read(), float(sys.argv[2]), float(sys.argv[3])
def get(block, key):
    m = re.search(rf"<{block}>.*?^{key}\s*=\s*([^\s#]+)", txt, re.S | re.M)
    return m.group(1) if m else None
out = []
for blk in ("mesh", "meshblock"):
    for k in ("nx1", "nx2", "nx3"):
        v = get(blk, k)
        if v is None or int(v) == 1: continue
        out.append(f"{blk}/{k}={max(4, int(round(int(v) * rs)))}")
out.append("time/tlim=" + repr(float(get("time", "tlim")) * ts))
for blk in re.findall(r"^<(output\d+)>", txt, re.M):
    dt = get(blk, "dt")
    if dt is None or float(dt) <= 0.0: continue
    out.append(f"{blk}/dt=" + repr(float(dt) * ts))
print(" ".join(out))
PY
)"
  mkdir -p "$WORK/run/$id"
  ( cd "$WORK/run/$id" && "$WORK/src/bin/athena" -i "$deck" -d . $overrides > run.log 2>&1 )
  # Graded files: the state at t = tlim of every meshblock, full double precision
  # (data_format in the deck).
  for f in "$WORK/run/$id"/"$id".block*.out2.00001.tab; do cp "$f" "$OUT_DIR/$(basename "$f" .out2.00001.tab).tab"; done
done
