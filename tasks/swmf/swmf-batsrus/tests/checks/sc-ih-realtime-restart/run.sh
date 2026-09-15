#!/usr/bin/env bash
# Check sc-ih-realtime-restart: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# Upstream test: make test10 (both SWMF.exe invocations, test10_run then test10_restart)
#   deck: code/swmf/Param/PARAM.in.realtime.restart.SCIH_threadbc
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
knob SAB_STOP_SCALE "0.06" "multiplies every positive MaxIter and every positive tSimulationMax of every #STOP block of every stage deck; 0.06 is the graded value; run time scales with it (1 would reproduce the upstream window unchanged)"
knob SAB_RESTART_TMAX "60" "simulated seconds of the physical restart window (the second SWMF invocation): the installed deck's #ENDTIME (upstream: the official second-magnetogram date, 150 s after the carried start) is moved to the carried start plus this many seconds, while the magnetogram files keep their upstream dates, so SC and IH run this many one-second couplings from the restart; 60 is the graded value since 2026-09-14 (the full 150 s window measured about 400 s of run time, over the 300 s cap; one 50 s B0/thread update cycle completes inside 60 s); 0 disables the insert and the window is the upstream 150 s"
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
BUILD_PROFILE=sc-ih-awesom-realtime-tools
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

# The deck installer: copy one stage deck of ic/<inputs> into the run directory as
# PARAM.in, applying SAB_STOP_SCALE, and run the upstream parameter check on it.
# At SAB_STOP_SCALE=1 the deck reaches the run directory unchanged; the graded
# default may be smaller (see rubric.json default_vs_upstream) to keep the
# suite's run time short while the graded window stays physically meaningful.
cat > "$WORK/stopscale.py" <<'PY'
import re, sys
src, dst, scale = sys.argv[1], sys.argv[2], float(sys.argv[3])
lines = open(src, encoding="ascii", errors="replace").read().split("\n")
if scale != 1.0:
    # #STOP MaxIter is a cumulative iteration count across the whole deck (each
    # session's #STOP raises it), not a per-session delta, so independently
    # floor(1)-ing every scaled value can round two consecutive sessions to the
    # same cumulative target at an aggressive scale: the later session then
    # takes zero net iterations and whatever it was meant to do (turn a
    # component on, write a restart) never happens. prev_max_iter keeps the
    # scaled sequence strictly increasing so every session that had a positive
    # raw delta still gets at least one real iteration.
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
    # past its end. DnXxx are iteration counts, DtXxx simulation-time
    # intervals; either may carry a trailing unit comment (e.g. "DtOutput [sec]").
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
# Synthetic line-of-sight images. Every `los` #SAVEPLOT entry of the installed deck
# is rewritten by losonce.py according to LOS_MODE (set per stage below):
#   drop      the entry is removed and nPlotFile decremented: the stage is an ungraded
#             prerequisite and its images are never read;
#   once      DnSavePlot = -1 and DtSavePlot = -1: BATSRUS's final save writes a plot
#             file whose two cadences are both negative unless it was already written
#             at the last step, so each instrument yields exactly one image, at the
#             final state of the run (the component must still be on at the end);
#   dn=N|dt=T the image is written at that cumulative step or simulation time (used
#             where the component is switched off before the end of the run);
#   keep      the deck's own cadence, as scaled above.
# LOS_INSTRUMENTS, when set, replaces the entry's StringsInstrument list (the knob
# SAB_LOS_INSTRUMENTS above; the graded instruments by default). LOS_GENERIC=drop
# also removes the generic (observer-position) los entries, which are never graded.
# Measured 2026-09-14 on the worker (2 ranks, refined SC grid): one EUV image
# (aia, euvi; 512 px) costs about 80 s, one white-light coronagraph image 5 s
# (c2, c3) to 30 s (cor1, cor2); on the scaled cadence the images were written at
# every iteration and cost an order of magnitude more than the MHD steps.
cat > "$WORK/losonce.py" <<'PY'
import os, sys
p, mode = sys.argv[1], sys.argv[2]
ins = os.environ.get("LOS_INSTRUMENTS", "")
generic = os.environ.get("LOS_GENERIC", "keep")   # "drop": remove generic (non-instrument) los entries too
lines = open(p, encoding="ascii", errors="replace").read().split("\n")
out, i, dropped = [], 0, 0
saveplot = None   # index in out of the nPlotFile line of the current #SAVEPLOT block
while i < len(lines):
    line = lines[i]; f = line.split()
    if f[:1] == ["#SAVEPLOT"]:
        out.append(line); saveplot = len(out); out.append(lines[i + 1]); i += 2; continue
    if len(f) >= 2 and f[0] == "los" and "StringPlot" in line:
        # an instrument entry (los ins ...) is StringPlot, DnSavePlot, DtSavePlot,
        # StringsInstrument; any other los entry (los LGQ ..., observer position and
        # image geometry lines) only has its two cadence lines rewritten
        is_ins = f[1].lower() == "ins"
        if is_ins:
            n = 4
        else:                            # up to the next entry or the end of the block
            n = 1
            while i + n < len(lines) and lines[i + n].split() and not lines[i + n].rstrip().endswith("StringPlot"):
                n += 1
        block = lines[i:i + n]
        i += n
        if not is_ins and generic != "drop":
            out.extend(block); continue
        if mode == "drop" or not is_ins:
            n = int(out[saveplot].split()[0]) - 1
            out[saveplot] = "%d\t\t\tnPlotFile" % n
            dropped += 1
            continue
        if mode == "once":
            dn, dt = "-1", "-1.0"
        elif mode.startswith("dn="):
            dn, dt = mode[3:], "-1.0"
        elif mode.startswith("dt="):
            dn, dt = "-1", mode[3:]
        elif mode == "keep":
            dn, dt = block[1].split()[0], block[2].split()[0]
        else:
            sys.exit("losonce.py: unknown mode " + mode)
        block[1] = dn + "\t\t\tDnSavePlot"
        block[2] = dt + "\t\t\tDtSavePlot"
        if ins and is_ins:
            block[3] = ins + "\t\t\tStringsInstrument"
        out.extend(block); continue
    out.append(line); i += 1
open(p, "w", encoding="ascii").write("\n".join(out))
PY
# Every #SAVEPLOT entry of both decks (only the logs are graded) is written exactly once
# (plotonce.py, the same rewriter the graded stages use with no primary series;
# PLOT_ONCE names the cumulative target for a component switched off before the
# end of the run): measured 2026-09-14, the scaled cadences wrote the IH
# spherical-shell plot (a 170 MB ASCII file, about 10 s each) and every cut at
# every iteration, which cost more than the MHD steps themselves.
cat > "$WORK/plotonce.py" <<'PY'
import os, re, sys
src, dst, frames = sys.argv[1], sys.argv[2], float(sys.argv[3])
# targets: the primary graded series, retimed to window / frames. A target may be
# qualified by component ("SC:x=0 VAR idl_ascii") or plain (every component).
targets = set(sys.argv[4:])
# PLOT_ONCE: how every OTHER #SAVEPLOT entry is written exactly once, per
# component, as "COMP=spec [COMP=spec ...]"; spec is "end" (DnSavePlot = -1 and
# DtSavePlot = -1: BATSRUS's final save writes a file whose two cadences are
# both negative unless it was already written at the last step, so the entry is
# written once, at the end of the run; only for a component still on at the
# end), "session=K" (once at session K's cumulative #STOP target of the entry's
# kind, sessions numbered from 1 as in the decks, for a component switched off
# after session K), "dn=N" or "dt=T"
# (explicit). Components not listed use "end".
once = {}
for item in os.environ.get("PLOT_ONCE", "").split():
    comp, spec = item.split("=", 1)
    once[comp] = spec
lines = open(src, encoding="ascii", errors="replace").read().split("\n")
# Assign every line to a session index. #RUN is the command that actually ends
# a session (a following "Begin session <n>" is only a descriptive comment
# some decks include and is not present in every deck), so the session
# increments right after each #RUN line. The component of a line is the one of
# the enclosing #BEGIN_COMP block (none outside).
session_of, comp_of = [], []
session_id, comp = 0, ""
for line in lines:
    f = line.split()
    if f[:1] == ["#BEGIN_COMP"] and len(f) > 1:
        comp = f[1]
    session_of.append(session_id); comp_of.append(comp)
    if f[:1] == ["#END_COMP"]:
        comp = ""
    if f[:1] == ["#RUN"]:
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
def setval(idx, value, integer):
    parts = lines[idx].split(None, 1)
    tail = "\t\t\t" + parts[1] if len(parts) > 1 else ""
    lines[idx] = (("%d" % value) if integer else ("%.10g" % value)) + tail
def rewrite(idx, integer, window):
    setval(idx, max(1, int(round(window / frames))) if integer else (window / frames), integer)
for i, line in enumerate(lines):
    # the StringPlot name (e.g. "y=0 MHD idl") itself contains spaces, so the
    # tag is found from the right, not the left.
    parts = line.rsplit(None, 1)
    if len(parts) != 2 or parts[1] != "StringPlot":
        continue
    name, comp = parts[0].strip(), comp_of[i]
    dn_i, dt_i = i + 1, i + 2
    try:
        dn = float(lines[dn_i].split()[0]); dt = float(lines[dt_i].split()[0])
    except (IndexError, ValueError):
        continue
    if name in targets or (comp + ":" + name) in targets:
        win = local_window.get(session_of[i], {})
        # whichever field is this entry's own active positive trigger gets the
        # new cadence, from its own session's LOCAL window of the same kind; if
        # that session's #STOP does not set that kind (a step-cadence plot inside
        # a purely time-limited session, or vice versa) the window is unknowable
        # analytically, so this entry is left untouched.
        if dt > 0 and win.get("tm"):
            rewrite(dt_i, False, win["tm"])
        elif dn > 0 and win.get("it"):
            rewrite(dn_i, True, win["it"])
        continue
    # every other entry: once
    spec = once.get(comp, "end")
    if spec.startswith("session="):
        it, tm = session_stop.get(int(spec[8:]) - 1, (None, None))   # sessions are numbered from 1 as in the decks
        if it is not None: spec = "dn=%d" % it
        elif tm is not None: spec = "dt=%.10g" % tm
        else: sys.exit("plotframes.py: session %s has no #STOP target" % spec[8:])
    if spec == "end":
        setval(dn_i, -1, True); setval(dt_i, -1.0, False)
    elif spec.startswith("dn="):
        setval(dn_i, int(float(spec[3:])), True); setval(dt_i, -1.0, False)
    elif spec.startswith("dt="):
        setval(dn_i, -1, True); setval(dt_i, float(spec[3:]), False)
    else:
        sys.exit("plotframes.py: unknown PLOT_ONCE spec " + spec)
open(dst, "w", encoding="ascii").write("\n".join(lines))
PY
install_deck() {   # install_deck <deck file name under ic/<inputs>/>
  [ -f "$CHECK_DIR/ic/$INPUTS/$1" ] || { echo "run.sh: ic/$INPUTS/$1 is missing" >&2; exit 2; }
  python3 "$WORK/stopscale.py" "$CHECK_DIR/ic/$INPUTS/$1" "$WORK/run/PARAM.in" "$SAB_STOP_SCALE"
  # Upstream runs TestParam.pl -F on every deck it installs and ignores its exit
  # status (the leading '-' of the Makefile rule); it rewrites nothing when the
  # deck is valid for this configuration.
  LOS_INSTRUMENTS="${SAB_LOS_INSTRUMENTS:-}" python3 "$WORK/losonce.py" "$WORK/run/PARAM.in" drop
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
  ./Config.pl -default -v=Empty,SC/BATSRUS,IH/BATSRUS
  ./Config.pl -o=SC:u=Awsom,e=Awsom,ng=2,g=4,4,4
  ./Config.pl -o=IH:u=Awsom,e=Awsom,ng=2,g=4,4,4
} > "$WORK/build.log" 2>&1 || { echo "run.sh: Config.pl failed" >&2; tail -n 60 "$WORK/build.log" >&2; exit 1; }
if [ "$IC" = altbuild ]; then
  ./Config.pl -O0 >> "$WORK/build.log" 2>&1
  grep -q '^OPT3 = -O0' Makefile.conf \
    || { echo "run.sh: Config.pl -O0 did not set OPT3 in Makefile.conf" >&2; exit 1; }
fi
{
  make -j"$SAB_MAKE_JOBS" SWMF
  make PIDL
  make -j"$SAB_MAKE_JOBS" -C util/EMPIRICAL/srcEE FRM
  make -j"$SAB_MAKE_JOBS" -C util/DATAREAD/srcMagnetogram HARMONICS
  make -j"$SAB_MAKE_JOBS" -C util/DATAREAD/srcMagnetogram CONVERTHARMONICS
  make -j"$SAB_MAKE_JOBS" -C util/DATAREAD/srcMagnetogram FDIPS
} >> "$WORK/build.log" 2>&1 || { echo "run.sh: build failed" >&2; tail -n 60 "$WORK/build.log" >&2; exit 1; }
  BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
  publish_build_cache
fi
echo "SAB_BUILD_SECONDS=$BUILD_SECONDS"   # zero on full reuse; the driver excludes actual compile time from the run budget

# ---- run directory ----------------------------------------------------------
make rundir RUNDIR="$WORK/run" > "$WORK/rundir.log" 2>&1 \
  || { echo "run.sh: make rundir failed" >&2; tail -n 40 "$WORK/rundir.log" >&2; exit 1; }

# ---- run ---------------------------------------------------------------------
# The real-time setup of test10_rundir, reproduced line for line: the shipped
# HARMONICS.in and HARMONICSGRID.in are edited to the test order and grid, the
# GONG magnetogram of the vendored SWMF_data subset is remapped to the SWMF map
# format, HARMONICS.exe fits the spherical harmonics and CONVERTHARMONICS.exe
# reconstructs the potential field on the run grid.
cd "$WORK/run/SC"
perl -i -pe 's/dipole11uniform/fitsfile_01/; s/harmonics11uniform/endmagnetogram/; s/\d+(\s+MaxOrder)/30$1/' HARMONICS.in
perl -i -pe 's/harmonics/endmagnetogram/; s/\d+(\s+MaxOrder)/30$1/; s/\d+(\s+nR)/30$1/; s/\d+(\s+nLon)/90$1/; s/\d+(\s+nLat)/90$1/' HARMONICSGRID.in
cp "$CHECK_DIR/ic/$INPUTS/2261_222.fits.gz" .
gzip -d 2261_222.fits.gz; mv 2261_222.fits endmagnetogram
{ python3 remap_magnetogram.py endmagnetogram fitsfile
  ./HARMONICS.exe
  mv MAGNETOGRAMTIME.in ENDMAGNETOGRAMTIME.in
  mpiexec -n "$SAB_MPI_RANKS" ${SAB_MPI_EXTRA:-} ./CONVERTHARMONICS.exe
} > "$WORK/magnetogram.log" 2>&1 \
  || { echo "run.sh: the real-time magnetogram setup failed" >&2; tail -n 40 "$WORK/magnetogram.log" >&2; exit 1; }
mv harmonics_bxyz.out "$WORK/run/harmonics_new_bxyz.out"
cd "$WORK/src"
# ParamConvert.pl expands the #INCLUDE of the magnetogram time into the deck, as
# test10_rundir does; the stop-scale knob is applied to the expanded deck.
cp "$CHECK_DIR/ic/$INPUTS/PARAM.in.realtime" "$WORK/run/SC/PARAM.tmp"
( cd "$WORK/run/SC" && "$WORK/src/share/Scripts/ParamConvert.pl" PARAM.tmp "$WORK/run/PARAM.in.expanded" ) \
  >> "$WORK/paramconvert.log" 2>&1 \
  || { echo "run.sh: ParamConvert.pl failed" >&2; tail -n 40 "$WORK/paramconvert.log" >&2; exit 1; }
# The first invocation writes the threaded-field-line restart state and the
# end-magnetogram date consumed by the physical restart. Scale 0.06 keeps every
# cumulative local prerequisite session distinct (6/7/9 iterations). These are
# local-time-stepping iterations at t=0, not a fabricated physical cycle.
python3 "$WORK/stopscale.py" "$WORK/run/PARAM.in.expanded" "$WORK/run/PARAM.in" "$SAB_STOP_SCALE"
python3 "$WORK/plotonce.py" "$WORK/run/PARAM.in" "$WORK/run/PARAM.in" 1
LOS_INSTRUMENTS="${SAB_LOS_INSTRUMENTS:-}" python3 "$WORK/losonce.py" "$WORK/run/PARAM.in" drop
( cd "$WORK/src" && ./Scripts/TestParam.pl -F "$WORK/run/PARAM.in" ) >> "$WORK/testparam.log" 2>&1 || true
rm -f "$WORK/run/PARAM.in_orig_"
run_swmf runlog
# test10_restart: the end magnetogram of the first window becomes the start
# magnetogram of the second, a new GONG map is remapped and fitted, and the
# artificial 150 s time offset of the upstream test is applied to it.
( cd "$WORK/run" && ./Restart.pl ) >> "$WORK/restart.log" 2>&1 \
  || { echo "run.sh: Restart.pl failed" >&2; tail -n 40 "$WORK/restart.log" >&2; exit 1; }
rm -f "$WORK/run/harmonics_bxyz.out"
mv "$WORK/run/harmonics_new_bxyz.out" "$WORK/run/harmonics_bxyz.out"
cd "$WORK/run/SC"
rm -f STARTMAGNETOGRAMTIME.in startmagnetogram.dat PARAM.tmp
rm -f fitsfile_01.out endmagnetogram endmagnetogram.dat
cp ENDMAGNETOGRAMTIME.in STARTMAGNETOGRAMTIME.in
cp "$CHECK_DIR/ic/$INPUTS/2261_218.fits.gz" .
gzip -d 2261_218.fits.gz; mv 2261_218.fits endmagnetogram
{ python3 remap_magnetogram.py endmagnetogram fitsfile
  perl -i -pe 's/0.2270982/0.2164177/' fitsfile_01.out
  ./HARMONICS.exe
  mv MAGNETOGRAMTIME.in ENDMAGNETOGRAMTIME.in
} > "$WORK/magnetogram_restart.log" 2>&1 \
  || { echo "run.sh: the restart magnetogram setup failed" >&2; tail -n 40 "$WORK/magnetogram_restart.log" >&2; exit 1; }
cd "$WORK/src"
# ParamConvert.pl expands the #INCLUDE of the magnetogram time into the deck, as
# test10_rundir does; the stop-scale knob is applied to the expanded deck.
cp "$CHECK_DIR/ic/$INPUTS/PARAM.in.realtime.restart" "$WORK/run/SC/PARAM.tmp"
( cd "$WORK/run/SC" && "$WORK/src/share/Scripts/ParamConvert.pl" PARAM.tmp "$WORK/run/PARAM.in.expanded" ) \
  >> "$WORK/paramconvert.log" 2>&1 \
  || { echo "run.sh: ParamConvert.pl failed" >&2; tail -n 40 "$WORK/paramconvert.log" >&2; exit 1; }
# Scale only the restart deck's positive #STOP/cadence numbers. Its earlier
# #ENDTIME is the official second magnetogram date, exactly 150 s after the
# carried start, and is deliberately not scaled: SC/IH therefore complete 150
# one-second couplings and B0/thread updates at 50, 100 and 150 s. The later
# SC-off absolute #STOP=60 target is already in the past and adds no window.
python3 "$WORK/stopscale.py" "$WORK/run/PARAM.in.expanded" "$WORK/run/PARAM.in" "$SAB_STOP_SCALE"
python3 "$WORK/plotonce.py" "$WORK/run/PARAM.in" "$WORK/run/PARAM.in" 1
# The restart window knob: the installed deck's #ENDTIME block (the official
# second-magnetogram date, 150 s after the carried start, which ParamConvert.pl has
# inlined) is moved to STARTTIME + SAB_RESTART_TMAX; a later #STOP does not override
# an #ENDTIME (measured 2026-09-14: the run went to 150 s regardless), while the
# magnetogram files in SC/ keep their upstream dates, so the boundary still
# interpolates toward the second map at the upstream rate.
if [ "${SAB_RESTART_TMAX%.*}" != "0" ]; then
  python3 - "$WORK/run/PARAM.in" "$SAB_RESTART_TMAX" <<'PY'
import sys, datetime
p, tmax = sys.argv[1], float(sys.argv[2])
lines = open(p, encoding="ascii", errors="replace").read().split("\n")
def block(i):   # the seven value lines after #STARTTIME / #ENDTIME
    return [float(lines[i + k].split()[0]) for k in range(1, 8)]
start = end = None
for i, line in enumerate(lines):
    tag = line.split()[:1]
    if tag == ["#STARTTIME"] and start is None:
        start = i
    if tag == ["#ENDTIME"]:
        end = i
if start is None or end is None:
    sys.exit("run.sh: the restart deck has no #STARTTIME / #ENDTIME block")
y, mo, d, h, mi, sec, frac = block(start)
t = datetime.datetime(int(y), int(mo), int(d), int(h), int(mi), int(sec)) + datetime.timedelta(seconds=tmax + frac)
labels = ["iYear", "iMonth", "iDay", "iHour", "iMinute", "iSecond", "FracSecond"]
values = [t.year, t.month, t.day, t.hour, t.minute, t.second, 0.0]
for k, (lab, val) in enumerate(zip(labels, values), start=1):
    lines[end + k] = (("%d" % val) if k < 7 else "%.1f" % val) + "\t\t\t" + lab
open(p, "w", encoding="ascii").write("\n".join(lines))
PY
fi
LOS_INSTRUMENTS="${SAB_LOS_INSTRUMENTS:-}" python3 "$WORK/losonce.py" "$WORK/run/PARAM.in" drop
( cd "$WORK/src" && ./Scripts/TestParam.pl -F "$WORK/run/PARAM.in" ) >> "$WORK/testparam.log" 2>&1 || true
rm -f "$WORK/run/PARAM.in_orig_"
run_swmf runlog_restart
( cd "$WORK/run" && ./PostProc.pl -M -cat -f=ascii RESULTS ) >> "$WORK/postproc.log" 2>&1 \
  || { echo "run.sh: PostProc.pl failed" >&2; tail -n 40 "$WORK/postproc.log" >&2; exit 1; }

# ---- graded files ------------------------------------------------------------
cd "$WORK/run"
grab sc_log.log RESULTS/SC/log_n*.log
grab ih_log.log RESULTS/IH/log_n*.log
# The SC cuts at the end of the run (eleven significant digits): the two-ulp
# variant never shows in the six-digit logs, it does in these (measured 2026-09-15).
grab sc_x0_var.outs RESULTS/SC/x=0_var_*.out RESULTS/SC/x=0_var_*.outs
grab sc_y0_var.outs RESULTS/SC/y=0_var_*.out RESULTS/SC/y=0_var_*.outs
grab sc_z0_var.outs RESULTS/SC/z=0_var_*.out RESULTS/SC/z=0_var_*.outs

# ---- graded-series frame count ------------------------------------------------
# The graded cuts are written once per invocation, at its end (plotonce.py
# above; the restart deck has no #STOP window, it ends at #ENDTIME), so the
# frame rule is carried by the logs: each log's data rows across both
# invocations (two header lines, then one row per saved iteration).
count_log_rows() { local n; n=$(( $(wc -l < "$1") - 2 )); [ "$n" -ge 0 ] || n=0; echo "$n"; }
FSC=$(count_log_rows "$OUT_DIR/sc_log.log")
FIH=$(count_log_rows "$OUT_DIR/ih_log.log")
FRAMES=$FSC; [ "$FIH" -lt "$FRAMES" ] && FRAMES=$FIH
if [ "$FRAMES" -lt 5 ]; then
  echo "run.sh: a graded log wrote only $FRAMES data rows (< 5) [sc_log=$FSC ih_log=$FIH]" >&2; exit 1
fi
echo "SAB_PLOT_FRAMES=$FRAMES"

echo "SAB_BUILD_SECONDS=$(( BUILD_SECONDS + BUILD_EXTRA ))"   # the total build time of this check; the budget counts run time only
