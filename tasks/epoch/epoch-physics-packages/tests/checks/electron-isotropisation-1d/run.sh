#!/usr/bin/env bash
# Check electron-isotropisation-1d: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh --help                       list the runtime knobs below
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_T_END=2e-14 sab.py task selfcheck ...
# The MPI rank layout (nprocx/nprocy/nprocz in the deck) is deliberately NOT a
# knob: it selects the per-rank random streams and the particle order, so it is
# part of the initial condition, and ic/variant is exactly a different layout.
# Parallel build jobs default to the CPUs this container may use (cgroup v2 cpu.max), not the host count.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_NX "10" "cells (upstream deck: 10); the box length is fixed, so this also sets the cell size and hence the timestep"
knob SAB_PPC "5000" "pseudoparticles per cell (upstream: 5000, the graded value); runtime and sampling noise both scale with it"
knob SAB_T_END "5e-14" "end time in seconds (upstream deck: 5e-14, the graded value); runtime scales linearly"
knob SAB_NSTEP_SNAPSHOT "100" "timesteps between dumps (upstream: 10); sets how many rows the graded series has"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the one build of the pinned source (default: the CPUs allowed to this container); each job needs about 0.3 GB"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
[ -d "$CHECK_DIR/ic/$IC" ] || { echo "run.sh: no initial condition ic/$IC" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

# Upstream test this check reproduces: code/epoch/epoch1d/example_decks/electron_isotropisation.deck
# Build: the Nanbu collision operator is a runtime deck option, so the stock build is used (no DEFINE)
make -C "$WORK/src/epoch1d" COMPILER=gfortran -j"$SAB_MAKE_JOBS" > "$WORK/make.log" 2>&1

# The deck of this initial condition, with the runtime knobs written into the
# lines they own (each is tagged with its knob name in the deck).
RUN="$WORK/run"; mkdir -p "$RUN"
sed -E -e "s|^[[:space:]]*nx = .*# SAB_NX\$|  nx = $SAB_NX # SAB_NX|" \
  -e "s|^[[:space:]]*npart_per_cell = .*# SAB_PPC\$|  npart_per_cell = $SAB_PPC # SAB_PPC|" \
  -e "s|^[[:space:]]*t_end = .*# SAB_T_END\$|  t_end = $SAB_T_END # SAB_T_END|" \
  -e "s|^[[:space:]]*nstep_snapshot = .*# SAB_NSTEP_SNAPSHOT\$|  nstep_snapshot = $SAB_NSTEP_SNAPSHOT # SAB_NSTEP_SNAPSHOT|" \
  "$CHECK_DIR/ic/$IC/input.deck" > "$RUN/input.deck"

# The rank count is the product of the layout the deck asks for.
ranks=1
for key in nprocx nprocy nprocz; do
  v="$(sed -n "s/^[[:space:]]*$key[[:space:]]*=[[:space:]]*\([0-9][0-9]*\).*/\1/p" "$RUN/input.deck" | head -1)"
  if [ -n "$v" ]; then ranks=$((ranks * v)); fi
done

# EPOCH resolves physics_table_location relative to the working directory, so
# the run is launched from the dimension directory of the build; the deck path
# is fed to the binary on stdin, which is how EPOCH takes its output directory.
cd "$WORK/src/epoch1d"
echo "$RUN" | mpirun -n "$ranks" --oversubscribe --bind-to none bin/epoch1d > "$WORK/run.log" 2>&1

# Graded files: the physical series this check compares, extracted from the SDF
# dumps (never the dumps themselves: their header carries the run date and the
# machine name).
python3 "$CHECK_DIR/extract.py" --run "$RUN" --out "$OUT_DIR"
