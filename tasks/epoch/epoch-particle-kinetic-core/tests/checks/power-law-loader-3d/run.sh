#!/usr/bin/env bash
# Check power-law-loader-3d: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     OPTIONAL: the nominal inputs on the alternative build (ALTBUILD below)
#   run.sh --help                       list the runtime knobs below, and the altbuild line when one is declared
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_PPC=2 sab.py task selfcheck ...
# Parallel build jobs default to the CPUs this container may use (cgroup v2 cpu.max), not the host count.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_PPC "15" "macroparticles per cell per species (upstream: 15); the loader is the whole runtime of this check and both runtime and memory scale linearly"
knob SAB_NCELLS "64" "cells along each axis (upstream: 100 x 100 x 100); the particle count, and so the runtime and the memory, scale with the cube"
knob SAB_NPROCX "2" "MPI ranks along x. Changing the rank layout changes the seed (7842432 + rank) and the per-rank particle counts, so the loaded distribution is a different, equally valid realisation and its output is not comparable with the graded default"
knob SAB_NPROCY "2" "MPI ranks along y; same caveat as SAB_NPROCX"
knob SAB_NPROCZ "1" "MPI ranks along z; same caveat. Total ranks = SAB_NPROCX * SAB_NPROCY * SAB_NPROCZ"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the one build of the pinned source (default: the CPUs allowed to this container); each job needs about 0.3 GB"
# Alternative build, OPTIONAL: the same pinned source and deck on a legitimately different,
# stricter build than the stock release build (see ALTBUILD below). `run.sh altbuild` runs
# ic/nominal on that build; selfcheck measures this check's floor from it.
ALTBUILD="epoch3d/Makefile's stock gfortran FFLAGS line changed from -O3 -g -std=f2003 to -O0 -g -std=f2003 in the scratch copy only (sed on the copied epoch3d/Makefile, never SOURCE_DIR): the same pinned source and deck at zero optimisation instead of the stock -O3 release build. (EPOCH's own MODE=debug profile was tried first and SIGFPEs on this leaf's decks -- -ffpe-trap=invalid,zero,overflow catching a legitimate operation in the pinned source -- so this leaf uses the traps-free build-flag fallback instead.)"
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
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"
# Alternative build (see ALTBUILD): the scratch copy only, never SOURCE_DIR.
if [ "$IC" = altbuild ]; then
  sed -i 's/^  FFLAGS = -O3 -g -std=f2003$/  FFLAGS = -O0 -g -std=f2003/' "$WORK/src/epoch3d/Makefile"
fi

# Upstream test this check reproduces: code/epoch/epoch3d/example_decks/power_law.deck
# Build: the stock gfortran build of epoch3d, no DEFINE (triangle shape function, per-particle weight); altbuild changes the copied Makefile's FFLAGS from -O3 to -O0 instead (see ALTBUILD)
cd "$WORK/src"
BUILD_START=$(date +%s)
make -C epoch3d COMPILER=gfortran -j"$SAB_MAKE_JOBS" > "$WORK/make.log" 2>&1
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # reported to the driver; the budget counts run time only

# The deck of this initial condition with the knobs written into it.
mkdir -p "$WORK/run"
sed -e "s|^  nx = .*|  nx = $SAB_NCELLS|" \
    -e "s|^  ny = .*|  ny = $SAB_NCELLS|" \
    -e "s|^  nz = .*|  nz = $SAB_NCELLS|" \
    -e "s|^  nparticles = nx \* ny \* nz \* .*|  nparticles = nx * ny * nz * $SAB_PPC|" \
    -e "s|^  nprocx = .*|  nprocx = $SAB_NPROCX|" \
    -e "s|^  nprocy = .*|  nprocy = $SAB_NPROCY|" \
    -e "s|^  nprocz = .*|  nprocz = $SAB_NPROCZ|" \
    "$CHECK_DIR/ic/$INPUTS/input.deck" > "$WORK/run/input.deck"

ranks=$(( SAB_NPROCX * SAB_NPROCY * SAB_NPROCZ ))
cd "$WORK/src/epoch3d"
echo "$WORK/run" | mpirun --oversubscribe --bind-to none -n "$ranks" \
    "$WORK/src/epoch3d/bin/epoch3d" > "$WORK/run/epoch.log" 2>&1

# Graded files: the arrays rubric.json names, as raw little-endian float64.
python3 "$CHECK_DIR/extract.py" "$WORK/run" "$OUT_DIR"
