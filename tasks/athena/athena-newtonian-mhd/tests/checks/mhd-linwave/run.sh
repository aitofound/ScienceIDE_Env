#!/usr/bin/env bash
# Check mhd-linwave: the TEST half of the check.
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
knob SAB_RES_SCALE "1" "multiplies every mesh and meshblock dimension of every deck (upstream: 32 x 16 x 16 cells with meshblocks of 8^3 and five static refinement regions (upstream also runs 64 x 32 x 32 and two uniform 64 x 32 x 32 runs)); runtime scales with the square in 2-D and the fourth power in 3-D"
knob SAB_TLIM_SCALE "1" "multiplies the end time of every deck (upstream: 0.5, 1.0 and 2.0); runtime scales linearly"
knob SAB_SERIES "default" "which decks of ic/ run: default, or full for the whole upstream series (see the deck headers, # sab_series)"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the builds of the pinned source (default: the CPUs allowed to this container); each job needs about 0.2 GB"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
[ -d "$CHECK_DIR/ic/$IC" ] || { echo "run.sh: no initial condition ic/$IC" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

marker() { sed -n "s/^# $1 *= *//p" "$2" | head -1; }

# Select the decks this run grades. Every deck carries "# sab_series" in its header;
# the default series is the graded one, "full" adds the rest of the upstream series.
decks=()
for deck in "$CHECK_DIR/ic/$IC"/*.athinput; do
  if [ "$SAB_SERIES" != full ] && [ "$(marker sab_series "$deck")" != default ]; then continue; fi
  decks+=("$deck")
done
[ "${#decks[@]}" -gt 0 ] || { echo "run.sh: no decks selected (SAB_SERIES=$SAB_SERIES)" >&2; exit 2; }

# Upstream test this check reproduces: code/athena/tst/regression/scripts/tests/mhd/mhd_linwave.py
# Build: the configuration of the upstream test, one build per Riemann solver. The object
# files are reused between solvers, exactly as the upstream regression script does, so only
# the newly selected solver is compiled after the first build.
fluxes="$(for d in "${decks[@]}"; do marker sab_flux "$d"; done | sort -u)"
cd "$WORK/src"
for flux in $fluxes; do
  BUILD_START=$(date +%s)
  python3 configure.py -b --prob=linear_wave --coord=cartesian --flux="$flux" > "$WORK/configure-$flux.log"
  make -j"$SAB_MAKE_JOBS" > "$WORK/make-$flux.log" 2>&1
  echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only
  for deck in "${decks[@]}"; do
    [ "$(marker sab_flux "$deck")" = "$flux" ] || continue
    id="$(basename "$deck" .athinput)"
    # The knobs rescale the deck's own mesh and end time; with the defaults this is a no-op.
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
tlim = float(get("time", "tlim")) * ts
out += [f"time/tlim={tlim!r}", f"output1/dt={tlim!r}"]
print(" ".join(out))
PY
)"
    mkdir -p "$WORK/run/$id"
    ( cd "$WORK/run/$id" && "$WORK/src/bin/athena" -i "$deck" -d . $overrides > run.log 2>&1 )
    # Graded files: the conserved state at t = tlim of every meshblock the deck ended with,
    # full double precision (data_format = %24.16e in the deck).
    for f in "$WORK/run/$id"/"$id".block*.out1.00001.tab; do
      cp "$f" "$OUT_DIR/$(basename "$f" .out1.00001.tab).tab"
    done
  done
done
