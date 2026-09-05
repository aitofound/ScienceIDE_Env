#!/usr/bin/env bash
# Check cr-bell-2d-ct-mpi2-04: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     nominal inputs, same architecture, make CFLAGS='-c -O0'
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# Official test: code/pluto/Test_Problems/Particles/CR/Bell_Instability, configuration 04 (definitions_04.h + pluto_04.ini).

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_MAXSTEPS=20 sab.py task selfcheck ...
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_TSTOP "0.5" "[Time] tstop of the deck in code units; the number of steps and the runtime scale linearly with it; the default is the graded window"
knob SAB_GRID_SCALE "1" "multiplies the zone count of every grid axis of the deck (rounded to a multiple of 4); 1 is the graded deck; runtime scales as scale^(dimensions+1)"
knob SAB_MAXSTEPS "-1" "cap on the number of time steps (pluto -maxsteps); -1 runs to SAB_TSTOP (graded); a small cap exercises build, run and output only"
# Alternative build: the same pinned source and Linux.mpicc.defs architecture with the make-line
# CFLAGS overridden to -c -O0 instead of its nominal -O3; run.sh altbuild always uses ic/nominal.
ALTBUILD="make CFLAGS='-c -O0' on the same Linux.mpicc.defs architecture instead of its nominal -O3 flags; the pinned source and nominal deck are unchanged"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
case "$IC" in
  nominal|variant) INPUTS="$IC" ;;
  altbuild) INPUTS=nominal ;;
  *) echo "run.sh: initial condition must be nominal, variant or altbuild" >&2; exit 2 ;;
esac
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
PROBLEM="$WORK/problem"; RUN="$WORK/run"
mkdir -p "$PROBLEM" "$RUN"

# The problem directory: the official problem directory (init.c and everything else) from the source tree, then this check's definitions.h and pluto.ini from ic/<ic>/.
cp -R "$SOURCE_DIR/Test_Problems/Particles/CR/Bell_Instability/." "$PROBLEM/"
cp -R "$CHECK_DIR/ic/$INPUTS/." "$PROBLEM/"

# Build against the source tree (setup.py writes only into the problem directory).
# local_make is picked up by PLUTO's own makefile template (`-include local_make`):
# the pinned -std=c17 flags hide drand48/srand48 without _DEFAULT_SOURCE.
export PLUTO_DIR="$SOURCE_DIR"
printf 'ARCH         = Linux.mpicc.defs\n' >"$PROBLEM/makefile"
printf 'CFLAGS += -D_DEFAULT_SOURCE\n' >"$PROBLEM/local_make"
MAKE_ARGS=(-j"${SAB_BUILD_JOBS:-2}")
[ "$IC" = altbuild ] && MAKE_ARGS+=("CFLAGS=-c -O0")
BUILD_START=$(date +%s)
if ! (cd "$PROBLEM" && python3 "$PLUTO_DIR/setup.py" --auto-update  >setup.log 2>&1 && make "${MAKE_ARGS[@]}" >make.log 2>&1); then
  echo "run.sh: build failed" >&2; tail -n 40 "$PROBLEM/setup.log" "$PROBLEM/make.log" >&2; exit 1
fi
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"

# The deck runs from a scratch directory; only the graded files are copied out.
cp "$PROBLEM/pluto.ini" "$RUN/pluto.ini"
export OMPI_MCA_btl=self,vader OMPI_MCA_btl_vader_single_copy_mechanism=none OMPI_MCA_btl_base_warn_component_unused=0
if [ "$SAB_TSTOP" != "0.5" ]; then
  python3 - "$RUN/pluto.ini" "$SAB_TSTOP" <<'PY'
import re, sys
path, new = sys.argv[1:]
lines = open(path, encoding="ascii").read().splitlines(keepends=True)
sec, hits = None, []
for i, ln in enumerate(lines):
    m = re.match(r"\s*\[(.+?)\]", ln)
    if m: sec = m.group(1); continue
    if sec == "Time" and re.match(r"\s*tstop\s+\S+", ln): hits.append(i)
if len(hits) != 1: sys.exit("run.sh: expected exactly one [Time] tstop line, found %d" % len(hits))
lines[hits[0]] = re.sub(r"(\s*tstop\s+)\S+", lambda m: m.group(1) + new, lines[hits[0]], count=1)
open(path, "w", encoding="ascii").write("".join(lines))
PY
fi
ARGS=()
if [ "$SAB_MAXSTEPS" -ge 0 ] 2>/dev/null; then ARGS+=(-maxsteps "$SAB_MAXSTEPS"); fi
if ! (cd "$RUN" && mpirun --allow-run-as-root --oversubscribe -np 2 "$PROBLEM/pluto" ${ARGS[@]+"${ARGS[@]}"} >pluto.log 2>&1); then
  echo "run.sh: pluto failed" >&2; tail -n 60 "$RUN/pluto.log" >&2; exit 1
fi

# Graded files, named as rubric.json describes them.
# The contract in rubric.json names the exact frames; every frame of each graded family is copied.
grep -v "^#" "$RUN/grid.out" >"$OUT_DIR/grid.out"
cp "$RUN/dbl.out" "$OUT_DIR/"
cp "$RUN"/data.*.dbl "$OUT_DIR/"
