#!/usr/bin/env bash
# Check gm-mgitm: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_STOP_SCALE=0.25 sab.py task selfcheck ...
# Parallel build jobs default to the CPUs this container may use (cgroup v2 cpu.max), not the host count.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_STOP_SCALE "0.25" "multiplies both #STOP blocks of the deck (upstream: 10 steady GM iterations, then a time-accurate window of 0.4 s coupled to UA every 0.2 s); run time scales with it (graded default shortened from the upstream 1.0, correction 2026-09-06; the coupling interval is scaled by an additional 2/3, so three couplings remain)"
knob SAB_RANKS "4" "MPI ranks (upstream runs this test on 2 ranks, or on 4 when the suite is started with more; the COMPONENTMAP gives every rank to both GM and UA)"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the build of the pinned source (default: the CPUs allowed to this container); it changes build time only, never the graded run"
# Alternative build, OPTIONAL: the SWMF's own ./Config.pl -O0 rewrites every OPTn line of
# Makefile.conf to -O0 where the shipped gfortran template (share/build/Makefile.Linux.gfortran)
# builds at -O3 -- a legitimately different build of the same pinned source and deck.
ALTBUILD="the same Config.pl configuration built with ./Config.pl -O0 before make SWMF, which sets every OPTn level of Makefile.conf to -O0 where the shipped gfortran template uses -O3; same pinned source, same deck"
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
exec < /dev/null                 # mpiexec must not read the produce driver's stdin
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
export LC_ALL=C OMP_NUM_THREADS=1

# One configured source is built directly at its final family path per solve.
# The shared root is private to test.sh produce; direct run.sh calls fall back
# to a fresh self-contained source copy.
. "$CHECK_DIR/build-cache.sh"
BUILD_FAMILY="gm-mgitm"
BUILD_SPEC='install=BATSRUS;compiler=gfortran;framework=-default,-v=Empty,GM/BATSRUS,UA/MGITM,-o=GM:u=Mars,e=MhdMars,g=8,8,8,UA:Mars,g=8,4,120,4;pre=UA/MGITM/src:DEPEND;targets=SWMF,PIDL,PGITM'
BUILD_INPUT_KEY="none"
sab_prepare_build

# Upstream test this check reproduces: make test12 (Makefile.test target test12).
if [ "$SAB_BUILD_CACHE_HIT" -eq 1 ]; then
  sab_report_build_reuse
else
  cd "$SRC"
  BUILD_START=$(date +%s)
  GIT_TERMINAL_PROMPT=0 ./Config.pl -install=BATSRUS -compiler=gfortran > "$WORK/install.log" 2>&1
  ./Config.pl -default -v=Empty,GM/BATSRUS,UA/MGITM >> "$WORK/build.log" 2>&1
  ./Config.pl -o=GM:u=Mars,e=MhdMars,g=8,8,8,UA:Mars,g=8,4,120,4 >> "$WORK/build.log" 2>&1
  if [ "$IC" = altbuild ]; then
    ./Config.pl -O0 >> "$WORK/build.log" 2>&1
    grep -q '^OPT3 = -O0' Makefile.conf || { echo "run.sh: Config.pl -O0 did not set OPT3 in Makefile.conf" >&2; exit 1; }
  fi
  # UA/MGITM's Makefile reaches its own src/libGITM.a target directly from its LIB
  # rule and so never runs the DEPEND target that writes src/Makefile.DEPEND; without
  # that file a parallel make has no module ordering inside the component and races on
  # modsizegitm.mod. Run the component's own DEPEND target first, then build in parallel.
  make -C UA/MGITM/src DEPEND >> "$WORK/build.log" 2>&1
  [ -s UA/MGITM/src/Makefile.DEPEND ] || { echo "run.sh: UA/MGITM/src/Makefile.DEPEND was not written" >&2; exit 1; }
  make -j"$SAB_MAKE_JOBS" SWMF >> "$WORK/build.log" 2>&1
  make PIDL >> "$WORK/build.log" 2>&1
  make PGITM >> "$WORK/build.log" 2>&1
  BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
  test ! -e "$SRC/.sab-rundir-template"
  make rundir RUNDIR="$SRC/.sab-rundir-template" > "$WORK/rundir.log" 2>&1
  sab_finish_build "$BUILD_SECONDS"
fi
cd "$SRC"

# Run directory exactly as the upstream test builds it.
[ -d "$SRC/.sab-rundir-template" ] || { echo "run.sh: family rundir template is missing" >&2; exit 1; }
mkdir "$WORK/run"
cp -R "$SRC/.sab-rundir-template/." "$WORK/run/"
# The knob rescales every #STOP window of the deck; at the graded default of 1
# the deck is copied through unchanged. Correction 2026-09-06: it now also
# rescales the positive UA-GM #COUPLE1 DtCouple (0.2 s) by the same factor, so
# the coupled window crosses three coupling times --
# without this the second #STOP's TimeMax (0.4 s) would shrink below DtCouple
# and the window would stop coupling only once, or not at all.
python3 - "$CHECK_DIR/ic/$INPUTS/PARAM.in" "$WORK/run/PARAM.in" "$SAB_STOP_SCALE" <<'PY'
import sys
src, dst, scale = sys.argv[1], sys.argv[2], float(sys.argv[3])
lines = open(src, encoding="utf-8").read().split("\n")
if scale != 1.0:
    for i, line in enumerate(list(lines)):
        if line.strip() != "#STOP":
            continue
        for k, integer in ((i + 1, True), (i + 2, False)):
            if k >= len(lines) or not lines[k].split():
                break
            try:
                value = float(lines[k].split()[0])
            except ValueError:
                break
            if value > 0:
                parts = lines[k].split(None, 1)
                tail = "\t\t\t" + parts[1] if len(parts) > 1 else ""
                new = max(1, int(round(value * scale))) if integer else value * scale
                lines[k] = (("%d" % new) if integer else ("%.10g" % new)) + tail
    for k, line in enumerate(list(lines)):
        tokens = line.split(None, 1)
        if len(tokens) != 2 or not tokens[1].startswith("DtCouple"):
            continue
        try:
            value = float(tokens[0])
        except ValueError:
            continue
        if value > 0:
            lines[k] = ("%.10g" % (value * scale * (2.0 / 3.0))) + "\t\t\t" + tokens[1]
open(dst, "w", encoding="utf-8").write("\n".join(lines))
PY
# Upstream (Makefile.test test12_rundir) runs this cp from inside the just-built
# RUNDIR, where make rundir's own GM/BATSRUS rundir target has already linked
# RUNDIR/GM/Param -> GM/BATSRUS/Param; from there GM/Param/MARS/marsmgsp.txt
# resolves to the same file. Naming it directly from the source tree (still $SRC
# at this point in the script) copies the identical file without depending on that
# symlink still being in scope.
cp GM/BATSRUS/Param/MARS/marsmgsp.txt "$WORK/run/"

cd "$WORK/run"
if ! mpiexec -n "$SAB_RANKS" --oversubscribe ./SWMF.exe > runlog 2>&1 < /dev/null; then
  echo "run.sh: SWMF.exe failed; last lines of the run log follow" >&2
  tail -40 runlog >&2
  exit 1
fi
./PostProc.pl -m -f=ascii -replace RESULTS > postproc.log 2>&1 < /dev/null

# The graded files, under the fixed names rubric.json lists.
grab() {
  local dest="$1" last="" f; shift
  for f in "$@"; do [ -e "$f" ] && last="$f"; done
  [ -n "$last" ] || { echo "run.sh: no output file matched: $*" >&2; exit 1; }
  cp "$last" "$OUT_DIR/$dest"
}
grab gm_log.log RESULTS/GM/log_n*.log
grab ua_log.dat RESULTS/UA/log0*.dat
grab ua_state.bin RESULTS/UA/3DALL_*.bin
grab gm_x0.outs RESULTS/GM/x=0_mhd_*.outs
grab gm_y0.outs RESULTS/GM/y=0_mhd_*.outs
grab gm_z0.outs RESULTS/GM/z=0_mhd_*.outs
