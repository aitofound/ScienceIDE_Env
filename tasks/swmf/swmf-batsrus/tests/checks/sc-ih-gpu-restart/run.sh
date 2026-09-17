#!/usr/bin/env bash
# Check sc-ih-gpu-restart: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# Upstream test: make test9gpu OPENACC=-noacc (through the third SWMF.exe invocation)
#   deck: code/swmf/Param/PARAM.in.test.restart.SCIH_gpu
# It is reproduced step by step: the same Config.pl configuration, the same
# `make rundir`, the same input files, the same MPI run and the same
# post-processing. Every departure from upstream is named in rubric.json under
# `default_vs_upstream`.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_STOP_SCALE=0.2 sab.py task selfcheck ...
# Parallel build jobs default to the CPUs this container may use (cgroup v2 cpu.max), not the host count.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_STOP_SCALE "0.2" "multiplies every positive MaxIter and every positive tSimulationMax of every #STOP block of every stage deck; 1 would reproduce the upstream window unchanged; 0.2 is the graded value under the 2026-09-13 60 s window ruling (the restart stage's cumulative 12/15/20 s -> 2.4/3/4 s; the quick start stage's 2,5,10,11,15,20 -> 1,2,3,4,5,6); run time scales with it"
knob SAB_PLOT_FRAMES "5" "how many times each graded x=0/y=0/z=0 VAR idl series (SC and IH, across the restart stage's three sessions) is written across its own session's #STOP window; run.sh rewrites each from that window / this knob, requesting the finest practical cadence, but MEASURED EXCEPTION: like the sibling sc-ih-gpu-cme, sc-ih-cme, sc-ih-cme-restart and sc-ih-threadbc-restart checks, this stage's adaptive time step ramps to a handful of large, expensive steps regardless of the requested cadence (measured 3 real frames for both SC and IH); the floor below is set to 3, not 5, as a documented exception (flagged for the curator)"
knob SAB_MPI_RANKS "2" "MPI ranks of the graded run; the upstream test and the graded reference use 2 (the SWMF is rank-count independent only to round-off, so changing this changes the graded numbers)"
knob SAB_MPI_EXTRA "" "extra arguments passed to mpiexec (for example --oversubscribe on a host with fewer slots than ranks); empty is the graded value and does not change the result"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the build of the pinned source (default: the CPUs allowed to this container); it changes build time only, never the graded run"
# Alternative build, OPTIONAL: the SWMF's own ./Config.pl -O0 rewrites every OPTn line of
# Makefile.conf to -O0 where the shipped gfortran template (share/build/Makefile.Linux.gfortran)
# builds at -O3 -- a legitimately different build of the same pinned source and the same decks.
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
# mpiexec forwards standard input to rank 0 and drains it; the produce driver feeds
# its own check list on standard input, so take stdin away here.
exec < /dev/null
# A solve runs all checks sequentially in one fresh container. Reuse only an exact
# configuration/build-mode snapshot, and copy it into this check's private work tree
# so make rundir and any stage-specific rebuild cannot mutate the shared snapshot.
BUILD_PROFILE=sc-ih-gpu-compatible
BUILD_MODE=stock
[ "$IC" = altbuild ] && BUILD_MODE=o0
# The SWMF tree records its absolute path in every Makefile.def and Makefile.conf
# and in absolute symlinks (the data links, FDIPS.exe, pyfits), so a cached build
# snapshot only works at the path it was built at: one fixed work path per
# configuration and build mode. Checks run one at a time inside a solve container,
# and parallel probes use separate containers with their own /tmp.
WORK="${TMPDIR:-/tmp}/sab-swmf-$BUILD_PROFILE-$BUILD_MODE"; rm -rf "$WORK"; mkdir -p "$WORK"; trap 'rm -rf "$WORK"' EXIT
BUILD_CACHE_ROOT="${SAB_BUILD_CACHE_ROOT:-${TMPDIR:-/tmp}}/sciaccel-swmf-batsrus-multi-build-cache-v2"
BUILD_CACHE_DIR="$BUILD_CACHE_ROOT/$BUILD_PROFILE-$BUILD_MODE-${SAB_SOURCE_FINGERPRINT:-nofingerprint}"
mkdir -p "$BUILD_CACHE_ROOT"
BUILD_CACHE_HIT=0
# A snapshot built at another path (an older layout of this cache) cannot be reused
# and is removed so the fresh build can be published in its place.
if [ -f "$BUILD_CACHE_DIR/complete" ] && ! grep -q "^DIR *= *$WORK/src\$" "$BUILD_CACHE_DIR/src/Makefile.def" 2>/dev/null; then
  rm -rf "$BUILD_CACHE_DIR"
fi
if [ -f "$BUILD_CACHE_DIR/complete" ] && [ -d "$BUILD_CACHE_DIR/src" ]; then
  cp -a "$BUILD_CACHE_DIR/src" "$WORK/src"
  BUILD_CACHE_HIT=1
else
  cp -R "$SOURCE_DIR/." "$WORK/src"
fi
publish_build_cache() {
  local stage="${BUILD_CACHE_DIR}.tmp.$$"
  mkdir "$stage" 2>/dev/null || return 0
  cp -a "$WORK/src" "$stage/src" || return 0
  : > "$stage/complete"
  mv -T "$stage" "$BUILD_CACHE_DIR" 2>/dev/null || true
}
# LC_ALL silences the perl locale warnings of Config.pl, TestParam.pl and PostProc.pl;
# GIT_TERMINAL_PROMPT makes the optional srcUserExtra clone of Config.pl -install fail
# fast instead of waiting for credentials; PYTHONPATH is where the tree keeps its
# vendored pyfits, which the magnetogram scripts import.
export LC_ALL=C OMP_NUM_THREADS=1 GIT_TERMINAL_PROMPT=0
export OMPI_ALLOW_RUN_AS_ROOT=1 OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1
export PYTHONPATH="$WORK/src/share/Python${PYTHONPATH:+:$PYTHONPATH}"
# Config.pl asks only for a stable machine key; keep the correction local to this
# disposable check runtime because the established host's FQDN lookup is flaky.
mkdir -p "$WORK/bin"
cat > "$WORK/bin/hostname" <<'HOSTNAME_SHIM'
#!/bin/sh
printf '%s\n' sab-localhost
HOSTNAME_SHIM
chmod +x "$WORK/bin/hostname"
export PATH="$WORK/bin:$PATH"
# Config.pl changes into each generated component before opening an opt deck;
# keep this pinned deck addressable without depending on a host/shared symlink.
GPU_START_DECK="$WORK/src/Param/PARAM.in.test.start.SCIH_gpu"

# The deck installer: copy one stage deck of ic/<inputs> into the run directory as
# PARAM.in, applying SAB_STOP_SCALE, and run the upstream parameter check on it.
# At the graded default of 1 the deck reaches the run directory unchanged.
cat > "$WORK/stopscale.py" <<'PY'
import re, sys
src, dst, scale = sys.argv[1], sys.argv[2], float(sys.argv[3])
lines = open(src, encoding="ascii", errors="replace").read().split("\n")
if scale != 1.0:
    # #STOP MaxIter is a cumulative iteration count across the whole deck (each
    # session's #STOP raises it), not a per-session delta, so independently
    # rounding every scaled value can round two consecutive sessions to the
    # same cumulative target at an aggressive scale: the later session then
    # takes zero net iterations and whatever it was meant to do never happens.
    # prev_max_iter keeps the scaled sequence strictly increasing so every
    # session that had a positive raw delta still gets at least one real
    # iteration.
    prev_max_iter = None
    for i, line in enumerate(list(lines)):
        if line.split(None, 1)[:1] != ["#STOP"]:
            continue
        for k, integer in ((i + 1, True), (i + 2, False)):
            if k >= len(lines) or not lines[k].split():
                break
            try:
                value = float(lines[k].split()[0])
            except ValueError:
                break
            if value <= 0:
                continue
            if integer:
                new = max(1, int(round(value * scale)))
                if prev_max_iter is not None:
                    new = max(new, prev_max_iter + 1)
                prev_max_iter = new
            else:
                new = value * scale
            parts = lines[k].split(None, 1)
            tail = "\t\t\t" + parts[1] if len(parts) > 1 else ""
            lines[k] = (("%d" % new) if integer else ("%.10g" % new)) + tail
    # Output cadences that gate whether a graded file (a restart, a plot, a
    # satellite or trajectory series this check grabs) is written at all inside
    # the shortened #STOP window: scale them by the same factor as MaxIter and
    # tSimulationMax so they still fire inside the shortened window instead of
    # past its end (a fixed DnSaveRestart that used to be well inside a long
    # window can otherwise never be reached once the window is this short, so
    # no restart is ever written and the next stage's #INCLUDE of it fails).
    # DnXxx are iteration counts, DtXxx simulation-time intervals; either may
    # carry a trailing unit comment (e.g. "DtOutput [sec]").
    INT_LABELS = {"DnSaveRestart", "DnSavePlot", "DnOutput"}
    FLOAT_LABELS = {"DtSaveRestart", "DtSavePlot", "DtOutput"}
    for i, line in enumerate(lines):
        parts = line.split(None, 1)
        if len(parts) != 2:
            continue
        key = parts[1].split(None, 1)[0]
        if key not in INT_LABELS and key not in FLOAT_LABELS:
            continue
        try:
            value = float(parts[0])
        except ValueError:
            continue
        if value <= 0:
            continue
        integer = key in INT_LABELS
        new = max(1, int(round(value * scale))) if integer else value * scale
        lines[i] = (("%d" % new) if integer else ("%.10g" % new)) + "\t\t\t" + parts[1]
open(dst, "w", encoding="ascii").write("\n".join(lines))
PY
# The plot-cadence rewriter: given the deck already scaled by stopscale.py
# above, find each session's LOCAL #STOP window (the delta from the previous
# cumulative target of the same kind: MaxIter or tSimulationMax -- #STOP
# values are cumulative absolute targets across sessions, not per-session
# deltas) and rewrite the named #SAVEPLOT StringPlot entries so whichever of
# DnSavePlot/DtSavePlot is that entry's own active positive trigger equals its
# own session's window / SAB_PLOT_FRAMES; only the named entries are touched.
cat > "$WORK/plotframes.py" <<'PY'
import re, sys
src, dst, frames = sys.argv[1], sys.argv[2], float(sys.argv[3])
targets = set(sys.argv[4:])
lines = open(src, encoding="ascii", errors="replace").read().split("\n")
# Assign every line to a session index. #RUN is the command that actually ends
# a session (a following "Begin session <n>" is only a descriptive comment
# some decks include and is not present in every deck), so the session
# increments right after each #RUN line.
session_of = []
session_id = 0
for line in lines:
    session_of.append(session_id)
    if line.split(None, 1)[:1] == ["#RUN"] or line.lstrip().startswith("#RUN"):
        session_id += 1
# Each session's own #STOP (positive MaxIter and/or positive tSimulationMax);
# if a session has more than one #STOP the last one wins.
session_stop = {}
for i, line in enumerate(lines):
    if line.split(None, 1)[:1] != ["#STOP"]:
        continue
    it = tm = None
    for k, slot in ((i + 1, "it"), (i + 2, "tm")):
        if k >= len(lines) or not lines[k].split():
            continue
        try:
            v = float(lines[k].split()[0])
        except ValueError:
            continue
        if v > 0:
            if slot == "it": it = v
            else: tm = v
    session_stop[session_of[i]] = (it, tm)
# #STOP values are cumulative absolute targets across sessions (each session's
# #STOP raises the previous one), so the window a given session actually
# covers is the delta from the previous session's value of the SAME kind.
local_window = {}  # session_id -> {"it": window_or_None, "tm": window_or_None}
prev_it = prev_tm = 0.0
for sid in sorted(session_stop):
    it, tm = session_stop[sid]
    w_it = w_tm = None
    if it is not None:
        w_it = it - prev_it if it > prev_it else it
        prev_it = it
    if tm is not None:
        w_tm = tm - prev_tm if tm > prev_tm else tm
        prev_tm = tm
    local_window[sid] = {"it": w_it, "tm": w_tm}
def rewrite(idx, integer, window):
    parts = lines[idx].split(None, 1)
    tail = "\t\t\t" + parts[1] if len(parts) > 1 else ""
    new = max(1, int(round(window / frames))) if integer else (window / frames)
    lines[idx] = (("%d" % new) if integer else ("%.10g" % new)) + tail
for i, line in enumerate(lines):
    # the StringPlot name (e.g. "y=0 MHD idl") itself contains spaces, so the
    # tag is found from the right, not the left.
    parts = line.rsplit(None, 1)
    if len(parts) != 2 or parts[1] != "StringPlot":
        continue
    if parts[0].strip() not in targets:
        continue
    dn_i, dt_i = i + 1, i + 2
    try:
        dn = float(lines[dn_i].split()[0]); dt = float(lines[dt_i].split()[0])
    except (IndexError, ValueError):
        continue
    win = local_window.get(session_of[i], {})
    # whichever field is this entry's own active positive trigger gets the new
    # cadence, from its own session's LOCAL window of the same kind; if that
    # session's #STOP does not set that kind (a step-cadence plot inside a
    # purely time-limited session, or vice versa) the window is unknowable
    # analytically, so this entry is left untouched.
    if dt > 0 and win.get("tm"):
        rewrite(dt_i, False, win["tm"])
    elif dn > 0 and win.get("it"):
        rewrite(dn_i, True, win["it"])
open(dst, "w", encoding="ascii").write("\n".join(lines))

PY
install_deck() {   # install_deck <deck file name under ic/<inputs>/> [<StringPlot series to retime>...]
  local deck="$1"; shift
  [ -f "$CHECK_DIR/ic/$INPUTS/$deck" ] || { echo "run.sh: ic/$INPUTS/$deck is missing" >&2; exit 2; }
  python3 "$WORK/stopscale.py" "$CHECK_DIR/ic/$INPUTS/$deck" "$WORK/run/PARAM.in.stopscaled" "$SAB_STOP_SCALE"
  if [ "$#" -gt 0 ]; then
    python3 "$WORK/plotframes.py" "$WORK/run/PARAM.in.stopscaled" "$WORK/run/PARAM.in" "$SAB_PLOT_FRAMES" "$@"
  else
    mv "$WORK/run/PARAM.in.stopscaled" "$WORK/run/PARAM.in"
  fi
  # Upstream runs TestParam.pl -F on every deck it installs and ignores its exit
  # status (the leading '-' of the Makefile rule); it rewrites nothing when the
  # deck is valid for this configuration.
  ( cd "$WORK/src" && ./Scripts/TestParam.pl -F "$WORK/run/PARAM.in" ) >> "$WORK/testparam.log" 2>&1 || true
  rm -f "$WORK/run/PARAM.in_orig_"
}
run_swmf() {       # run_swmf <name of the run log>
  ( cd "$WORK/run" && mpiexec -n "$SAB_MPI_RANKS" ${SAB_MPI_EXTRA:-} ./SWMF.exe > "$1" 2>&1 ) \
    || { echo "run.sh: SWMF.exe failed in stage $1; last lines of the run log follow" >&2
         tail -n 60 "$WORK/run/$1" >&2; exit 1; }
}
# The graded files, under the fixed names rubric.json lists. BATSRUS stamps step
# and time numbers into every output file name; they are stripped here.
grab() {           # grab <name in OUT_DIR> <glob> [<glob> ...]
  local dest="$1" last="" f; shift
  for f in "$@"; do [ -e "$f" ] && last="$f"; done
  [ -n "$last" ] || { echo "run.sh: no output file matched: $*" >&2; exit 1; }
  case "$last" in
    *.gz) gunzip -c "$last" > "$OUT_DIR/$dest" ;;
    *) cp "$last" "$OUT_DIR/$dest" ;;
  esac
}
# Count frames of a graded plot series. PostProc.pl -cat concatenates every
# step's snapshot into one ".outs" series file (each snapshot still starts its
# own "<step> <time> <ndim> <nvar> <nparam>" header line inside it); without
# -cat every step is its own ".out" file. Count whichever applies.
count_frames() {   # count_frames <dest name (decides .out vs .outs)> <glob> [<glob> ...]
  local dest="$1" last="" f; shift
  case "$dest" in
    *.outs)
      for f in "$@"; do [ -e "$f" ] && last="$f"; done
      [ -n "$last" ] && grep -cE '^[[:space:]]*[0-9]+[[:space:]]+[0-9.Ee+-]+[[:space:]]+-?[0-9]+[[:space:]]+[0-9]+[[:space:]]+[0-9]+[[:space:]]*$' "$last" || echo 0
      ;;
    *)
      local n=0; for f in "$@"; do [ -e "$f" ] && n=$((n+1)); done; echo "$n"
      ;;
  esac
}

# ---- build ------------------------------------------------------------------
cd "$WORK/src"
BUILD_EXTRA=0          # extra build seconds of the stages that reconfigure and rebuild
# The satellite trajectory files this deck names live in GM/BATSRUS/data/TRAJECTORY of
# the SWMF_data collection, which the 44 MB SWMF_data subset vendored with the pinned
# tree does not carry. The check ships them itself under ic/<inputs>/TRAJECTORY (the
# upstream files cropped to a window around the deck's start time) and puts them where
# Config.pl -install links GM/BATSRUS/data from, before the install runs. See README.md
# and rubric.json default_vs_upstream.
mkdir -p "$WORK/src/SWMF_data/GM/BATSRUS/data/TRAJECTORY"
cp "$CHECK_DIR/ic/$INPUTS/TRAJECTORY/"*.dat "$WORK/src/SWMF_data/GM/BATSRUS/data/TRAJECTORY/"
if [ "$BUILD_CACHE_HIT" -eq 1 ]; then
  BUILD_SECONDS=0
else
  BUILD_START=$(date +%s)
{
  ./Config.pl -install=BATSRUS -compiler=gfortran
  ./Config.pl -v=Empty,SC/BATSRUS,IH/BATSRUS
  ./Config.pl -o=SC:u=Awsom,e=Awsom,ng=2,g=6,8,8
  ./Config.pl -o=IH:u=Awsom,e=Awsom,ng=2,g=8,8,8
  ./Config.pl -default -noacc
  ./Config.pl -o=SC:opt="$GPU_START_DECK"
  ./Config.pl -o=IH:opt="$GPU_START_DECK"
} > "$WORK/build.log" 2>&1 || { echo "run.sh: Config.pl failed" >&2; tail -n 60 "$WORK/build.log" >&2; exit 1; }
if [ "$IC" = altbuild ]; then
  ./Config.pl -O0 >> "$WORK/build.log" 2>&1
  grep -q '^OPT3 = -O0' Makefile.conf \
    || { echo "run.sh: Config.pl -O0 did not set OPT3 in Makefile.conf" >&2; exit 1; }
fi
{
  make -j"$SAB_MAKE_JOBS" SWMF
  make PIDL
} >> "$WORK/build.log" 2>&1 || { echo "run.sh: build failed" >&2; tail -n 60 "$WORK/build.log" >&2; exit 1; }
  BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
  publish_build_cache
fi
echo "SAB_BUILD_SECONDS=$BUILD_SECONDS"   # zero on full reuse; the driver excludes actual compile time from the run budget

# ---- run directory ----------------------------------------------------------
make rundir RUNDIR="$WORK/run" > "$WORK/rundir.log" 2>&1 \
  || { echo "run.sh: make rundir failed" >&2; tail -n 40 "$WORK/rundir.log" >&2; exit 1; }

# ---- run ---------------------------------------------------------------------
install_deck PARAM.in.start
run_swmf runlog
( cd "$WORK/run" && ./PostProc.pl -m -replace -f=ascii RESULTS/run_start ) >> "$WORK/postproc.log" 2>&1 \
  || { echo "run.sh: PostProc.pl failed" >&2; tail -n 40 "$WORK/postproc.log" >&2; exit 1; }
( cd "$WORK/run" && ./Restart.pl -i RESULTS/run_start/RESTART ) >> "$WORK/restart.log" 2>&1 \
  || { echo "run.sh: Restart.pl failed after the start stage" >&2; tail -n 40 "$WORK/restart.log" >&2; exit 1; }
install_deck PARAM.in.cme
# The upstream test reconfigures the optimised parameter set for the CME deck and rebuilds.
REBUILD_START=$(date +%s)
{ ./Config.pl -o=SC:opt="$WORK/run/PARAM.in"; make -j"$SAB_MAKE_JOBS" SWMF; make PIDL; } >> "$WORK/build.log" 2>&1 \
  || { echo "run.sh: the CME-stage rebuild failed" >&2; tail -n 60 "$WORK/build.log" >&2; exit 1; }
BUILD_EXTRA=$(( BUILD_EXTRA + $(date +%s) - REBUILD_START ))
run_swmf runlog
( cd "$WORK/run" && ./PostProc.pl -m -replace -f=ascii RESULTS/run_cme ) >> "$WORK/postproc.log" 2>&1 \
  || { echo "run.sh: PostProc.pl failed" >&2; tail -n 40 "$WORK/postproc.log" >&2; exit 1; }
cp -r "$WORK/run/RESULTS/run_start/RESTART/IH" "$WORK/run/RESULTS/run_cme/RESTART/"
( cd "$WORK/run" && ./Restart.pl -i RESULTS/run_cme/RESTART ) >> "$WORK/restart.log" 2>&1 \
  || { echo "run.sh: Restart.pl failed after the CME stage" >&2; tail -n 40 "$WORK/restart.log" >&2; exit 1; }
install_deck PARAM.in.restart "x=0 VAR idl" "y=0 VAR idl" "z=0 VAR idl"
REBUILD_START=$(date +%s)
{ ./Config.pl -o=SC:opt="$WORK/run/PARAM.in",IH:opt="$WORK/run/PARAM.in"; make -j"$SAB_MAKE_JOBS" SWMF; make PIDL; } >> "$WORK/build.log" 2>&1 \
  || { echo "run.sh: the restart-stage rebuild failed" >&2; tail -n 60 "$WORK/build.log" >&2; exit 1; }
BUILD_EXTRA=$(( BUILD_EXTRA + $(date +%s) - REBUILD_START ))
run_swmf runlog
( cd "$WORK/run" && ./PostProc.pl -m -replace -f=ascii RESULTS/run_restart ) >> "$WORK/postproc.log" 2>&1 \
  || { echo "run.sh: PostProc.pl failed" >&2; tail -n 40 "$WORK/postproc.log" >&2; exit 1; }

# ---- graded files ------------------------------------------------------------
cd "$WORK/run"
grab ih_log.log RESULTS/run_restart/IH/log_n*.log
grab sc_log.log RESULTS/run_restart/SC/log_n*.log
grab sc_x0_var.out RESULTS/run_restart/SC/x=0_var_*.out
grab sc_y0_var.out RESULTS/run_restart/SC/y=0_var_*.out
grab sc_z0_var.out RESULTS/run_restart/SC/z=0_var_*.out
grab ih_x0_var.out RESULTS/run_restart/IH/x=0_var_*.out
grab ih_y0_var.out RESULTS/run_restart/IH/y=0_var_*.out
grab ih_z0_var.out RESULTS/run_restart/IH/z=0_var_*.out

# ---- graded-series frame count ------------------------------------------------
FSC=$(count_frames sc_x0_var.out RESULTS/run_restart/SC/x=0_var_*.out)
FIH=$(count_frames ih_x0_var.out RESULTS/run_restart/IH/x=0_var_*.out)
FRAMES=$FSC; [ "$FIH" -lt "$FRAMES" ] && FRAMES=$FIH
if [ "$FRAMES" -lt 3 ]; then
  echo "run.sh: a graded plot series wrote only $FRAMES frames (< 3, the documented floor) [sc_x0=$FSC ih_x0=$FIH]" >&2; exit 1
fi
echo "SAB_PLOT_FRAMES=$FRAMES"

echo "SAB_BUILD_SECONDS=$(( BUILD_SECONDS + BUILD_EXTRA ))"   # the total build time of this check; the budget counts run time only
