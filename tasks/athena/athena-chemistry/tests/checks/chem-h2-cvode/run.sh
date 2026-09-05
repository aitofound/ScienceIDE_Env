#!/usr/bin/env bash
# Check chem-h2-cvode: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (configure.py -debug; see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_TLIM_SCALE=0.5 sab.py task selfcheck ...
# Parallel build jobs default to the CPUs this container may use (cgroup v2 cpu.max), not the host count.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_RES_SCALE "1" "multiplies every mesh and meshblock dimension of every deck (default: 32/64/128/256); runtime scales with the cell count"
knob SAB_TLIM_SCALE "1" "multiplies the end time of every deck (default: 5); runtime scales linearly"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the one build of the pinned source (default: the CPUs allowed to this container); each job needs about 0.2 GB"
# Alternative build: Athena++'s own debug mode changes only the build flags to -O0 -g;
# the compiler, pinned source, all other configure switches and nominal inputs stay the same.
ALTBUILD="configure.py -debug: Athena++'s own -O0 -g build of the same pinned source, with the same compiler and all other configure switches"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"; CONFIGURE_EXTRA=()
if [ "$IC" = altbuild ]; then INPUTS=nominal; CONFIGURE_EXTRA=(-debug); fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

# Upstream test this check reproduces: code/athena/tst/regression/scripts/tests/chemistry/chem_H2_gaussian.py
# Build: the configuration of the upstream test, one binary for all decks.
cd "$WORK/src"
BUILD_START=$(date +%s)
python3 configure.py "${CONFIGURE_EXTRA[@]}" --prob=chem_H2 --chemistry=H2 --eos=isothermal --nghost=3 --chem_ode_solver=cvode --cvode_path=/usr --cflag=-std=c++14 > "$WORK/configure.log"
make -j"$SAB_MAKE_JOBS" > "$WORK/make.log" 2>&1
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only

# Run every deck of this initial condition; the knobs rescale the deck's own mesh and end time.
for deck in "$CHECK_DIR/ic/$INPUTS"/*.athinput; do
  id="$(basename "$deck" .athinput)"
  overrides="$(python3 - "$deck" "${SAB_RES_SCALE:-1}" "$SAB_TLIM_SCALE" <<'PY'
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
  last="$(ls "$WORK/run/$id"/*.out1.*.tab | sed 's/.*\.out1\.\([0-9]*\)\.tab/\1/' | sort -n | tail -1)"
  for f in "$WORK/run/$id"/*.out1."$last".tab; do
    b="$(basename "$f")"; cp "$f" "$OUT_DIR/${b%.out1.$last.tab}.tab"
  done
done
