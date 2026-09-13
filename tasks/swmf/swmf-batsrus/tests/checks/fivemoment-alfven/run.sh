#!/usr/bin/env bash
# Check fivemoment-alfven: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     OPTIONAL: the nominal inputs on the alternative build (ALTBUILD below)
#   run.sh --help                       list the runtime knobs below, and the altbuild line when one is declared
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# Official test: code/swmf/GM/BATSRUS/Param/FIVEMOMENT/PARAM.in.alfven  (Makefile.test target test_fivemoment_alfven)
# Configuration: ./Config.pl -default -u=Default -e=FiveMoment -ng=2 -g=100,1,1

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_TIME_SCALE=0.05 sab.py task selfcheck ...
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_TIME_SCALE "1" "multiplies every tSimulationMax of the deck (the #STOP blocks of ic/<ic>/PARAM.in); 1 is the graded window, and the number of time steps and the run time scale with it"
knob SAB_PLOT_FRAMES "20" "target frame count of the graded #SAVEPLOT series; run.sh rewrites that entry cadence to window / SAB_PLOT_FRAMES before running (2026-09-13 window revision)"
knob SAB_MPI_RANKS "2" "MPI ranks for BATSRUS.exe; 2 is the graded value and the rank count of the upstream Makefile.test recipe"
knob SAB_BUILD_JOBS "4" "make -j for the BATSRUS build; build time only, never the graded run time"
# Alternative build, OPTIONAL. Set ALTBUILD to one line naming a legitimately different build of the
# same source (IEEE mode, -O0, a second compiler present in the image: something a correct candidate
# could plausibly be) ONLY when this check can be built that way; leave it empty otherwise. When it is
# set, `run.sh altbuild` runs ic/nominal on that build and selfcheck measures the check's floor from it.
ALTBUILD="the same Config.pl configuration built with ./Config.pl -O0 before make BATSRUS, which sets every OPTn level of Makefile.conf to -O0 where the shipped gfortran template uses -O3; same pinned source, same deck"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
# Read nothing from the inherited standard input. The produce driver starts the
# checks from a `while read` loop over a process substitution, and mpiexec slurps
# whatever standard input it is given: without this line the first check's mpiexec
# eats the driver's list of checks and the other thirteen never start.
exec 0</dev/null
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"
if [ "$IC" = altbuild ]; then
  [ -n "$ALTBUILD" ] || { echo "run.sh: this check declares no alternative build" >&2; exit 2; }
  INPUTS=nominal
fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
SRC="$WORK/src"; RUN="$WORK/run"
mkdir -p "$SRC"
# The pinned source is the SWMF tree. The upstream standalone BATSRUS tests run in
# the standalone layout: GM/BATSRUS with share/ and util/ beside it, which is exactly
# what BATSRUS's own Config.pl -install clones into its root. Assemble that layout from
# the pinned tree here; SOURCE_DIR itself is never modified.
cp -R "$SOURCE_DIR/GM/BATSRUS/." "$SRC/"
cp -R "$SOURCE_DIR/share" "$SRC/share"
cp -R "$SOURCE_DIR/util" "$SRC/util"
chmod -R u+w "$SRC"

# Build. Config.pl -install writes Makefile.conf from share/build/Makefile.<OS>.gfortran
# and then tries to clone srcUserExtra, which is access-restricted and absent from the
# pin; that attempt fails harmlessly (the run has no network) and the build goes on.
# The cache is run-scoped and stores only validated executables, never scientific outputs.
cd "$SRC"
export LC_ALL=C
fail() { echo "run.sh: $1" >&2; shift; tail -n 40 "$@" >&2 || true; exit 1; }
BUILD_TASK="swmf-batsrus"
BUILD_SPEC="Config.pl -install -compiler=gfortran; Config.pl -default -u=Default -e=FiveMoment -ng=2 -g=100,1,1; altbuild=Config.pl -O0"
BUILD_GROUP="swmf-batsrus-config-$(printf %s "$BUILD_SPEC" | sha256sum | cut -d' ' -f1)"
BUILD_TARGET="a100-sxm4-80gb"
BUILD_TARGET_SHA256="fec36b64e17d0893720e55b74a78c61d4e0f5cfc796322ff92f1a3b04a53c132"
BUILD_MODE=normal
if [ "$IC" = altbuild ]; then BUILD_MODE=altbuild; fi
CACHE_ENABLED=0
if [ -n "${SAB_BUILD_CACHE_ROOT:-}" ] && [ -n "${SAB_SOURCE_FINGERPRINT:-}" ]; then
  CACHE_ENABLED=1
fi

configure_source() {
./Config.pl -install -compiler=gfortran >"$WORK/install.log" 2>&1 || fail "Config.pl -install failed" "$WORK/install.log"
./Config.pl -default -u=Default -e=FiveMoment -ng=2 -g=100,1,1 >"$WORK/config.log" 2>&1 || fail "Config.pl failed" "$WORK/config.log"
if [ "$IC" = altbuild ]; then
  ./Config.pl -O0 >>"$WORK/config.log" 2>&1 || fail "Config.pl -O0 failed" "$WORK/config.log"
  grep -q '^OPT3 = -O0' Makefile.conf || fail "Config.pl -O0 did not set OPT3 in Makefile.conf" "$WORK/config.log"
fi
}

build_source() {
make -j"$SAB_BUILD_JOBS" BATSRUS >"$WORK/make.log" 2>&1 || fail "make BATSRUS failed" "$WORK/make.log"
make PIDL >>"$WORK/make.log" 2>&1 || fail "make PIDL failed" "$WORK/make.log"
}

if [ "$CACHE_ENABLED" -eq 1 ]; then
  COMPILER_VERSION="$(gfortran --version 2>&1 || true)"
  MAKE_VERSION="$(make --version 2>&1 || true)"
  MPI_VERSION="$(mpif90 --version 2>&1 || true)"
  MPI_INC="$(mpif90 -showme:compile 2>/dev/null || true)"
  BUILD_FINGERPRINT="$(printf '%s\0' \
      "cache-schema=swmf-batsrus-build-v1" \
      "task=$BUILD_TASK" \
      "source-fingerprint=$SAB_SOURCE_FINGERPRINT" \
      "source-root=$SOURCE_DIR" \
      "build-group=$BUILD_GROUP" \
      "build-spec=$BUILD_SPEC" \
      "build-mode=$BUILD_MODE" \
      "altbuild-spec=$ALTBUILD" \
      "target=$BUILD_TARGET" \
      "target-descriptor-sha256=$BUILD_TARGET_SHA256" \
      "runner=linux-docker" \
      "make-targets=BATSRUS,PIDL" \
      "make-jobs=$SAB_BUILD_JOBS" \
      "mpi-ranks=$SAB_MPI_RANKS" \
      "mpi-include=$MPI_INC" \
      "compiler=gfortran" \
      "compiler-version=$COMPILER_VERSION" \
      "make-version=$MAKE_VERSION" \
      "mpi-version=$MPI_VERSION" \
      "machine=$(uname -m)" | sha256sum | cut -d' ' -f1)"
  CACHE_DIR="$SAB_BUILD_CACHE_ROOT/$BUILD_TASK/$BUILD_GROUP/$BUILD_FINGERPRINT"
  CACHE_BINARY="$CACHE_DIR/BATSRUS.exe"
  CACHE_POSTIDL="$CACHE_DIR/PostIDL.exe"
  CACHE_DIGEST_FILE="$CACHE_DIR/binaries.sha256"
  CACHE_READY="$CACHE_DIR/ready.sha256"
  CACHE_HIT=0
  if [ -x "$CACHE_BINARY" ] && [ -s "$CACHE_BINARY" ] && [ -x "$CACHE_POSTIDL" ] && [ -s "$CACHE_POSTIDL" ] \
      && [ -f "$CACHE_DIGEST_FILE" ] && [ -f "$CACHE_READY" ]; then
    READY_FINGERPRINT="$(cat "$CACHE_READY" 2>/dev/null || true)"
    EXPECTED_BINARY_DIGEST="$(cat "$CACHE_DIGEST_FILE" 2>/dev/null || true)"
    ACTUAL_BINARY_DIGEST="$(sha256sum "$CACHE_BINARY" "$CACHE_POSTIDL" 2>/dev/null | awk '{printf "%s%s", sep, $1; sep=" ";} END {print ""}' || true)"
    if [ "$READY_FINGERPRINT" = "$BUILD_FINGERPRINT" ] && [ -n "$EXPECTED_BINARY_DIGEST" ] \
        && [ "$EXPECTED_BINARY_DIGEST" = "$ACTUAL_BINARY_DIGEST" ]; then
      CACHE_HIT=1
    fi
  fi
  if [ "$CACHE_HIT" -eq 1 ]; then
    # A cache hit still configures this check's actual source tree, so make rundir
    # and its scripts resolve exactly as in the cold path. If that setup or copy
    # fails, fall through to a complete configure/build rather than skipping work.
    if (configure_source) && mkdir -p "$SRC/src" \
        && cp "$CACHE_BINARY" "$SRC/src/BATSRUS.exe" \
        && cp "$CACHE_POSTIDL" "$SRC/src/PostIDL.exe" \
        && [ -x "$SRC/src/BATSRUS.exe" ] && [ -x "$SRC/src/PostIDL.exe" ]; then
      echo "SAB_BUILD_CACHE=hit group=$BUILD_GROUP fingerprint=$BUILD_FINGERPRINT variant=$IC altbuild=$BUILD_MODE"
      BUILD_SECONDS=0
    else
      echo "run.sh: cached BATSRUS/PostIDL binaries could not be configured or copied; rebuilding group $BUILD_GROUP" >&2
      CACHE_HIT=0
    fi
  fi
  if [ "$CACHE_HIT" -eq 0 ]; then
    echo "SAB_BUILD_CACHE=miss group=$BUILD_GROUP fingerprint=$BUILD_FINGERPRINT variant=$IC altbuild=$BUILD_MODE"
    BUILD_START=$(date +%s)
    configure_source
    build_source
    BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
    [ -x "$SRC/src/BATSRUS.exe" ] && [ -s "$SRC/src/BATSRUS.exe" ] || { echo "run.sh: build did not produce src/BATSRUS.exe" >&2; exit 3; }
    [ -x "$SRC/src/PostIDL.exe" ] && [ -s "$SRC/src/PostIDL.exe" ] || { echo "run.sh: build did not produce src/PostIDL.exe" >&2; exit 3; }
    # Publish binaries first and the matching digest/ready marker last; incomplete
    # cache entries cannot be accepted as hits and a cold build remains complete.
    if mkdir -p "$CACHE_DIR" \
        && printf '%s\n' building > "$CACHE_READY" \
        && cp "$SRC/src/BATSRUS.exe" "$CACHE_BINARY" \
        && cp "$SRC/src/PostIDL.exe" "$CACHE_POSTIDL" \
        && sha256sum "$CACHE_BINARY" "$CACHE_POSTIDL" | awk '{printf "%s%s", sep, $1; sep=" ";} END {print ""}' > "$CACHE_DIGEST_FILE" \
        && printf '%s\n' "$BUILD_FINGERPRINT" > "$CACHE_READY"; then
      echo "SAB_BUILD_CACHE=published group=$BUILD_GROUP fingerprint=$BUILD_FINGERPRINT variant=$IC altbuild=$BUILD_MODE"
    else
      echo "run.sh: warning: could not publish BATSRUS build cache for group $BUILD_GROUP; using local build" >&2
    fi
  fi
else
  echo "SAB_BUILD_CACHE=disabled reason=missing solve-scoped source fingerprint or cache root"
  BUILD_START=$(date +%s)
  configure_source
  build_source
  BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
fi
echo "SAB_BUILD_SECONDS=$BUILD_SECONDS"   # zero means no compile on a verified cache hit

# Run directory, exactly as Makefile.test's test_rundir does.
make rundir RUNDIR="$RUN" STANDALONE=YES GMDIR="$SRC" >"$WORK/rundir.log" 2>&1 || fail "make rundir failed" "$WORK/rundir.log"
cp "$CHECK_DIR/ic/$INPUTS/PARAM.in" "$RUN/PARAM.in"
if [ "$SAB_TIME_SCALE" != "1" ]; then
  python3 - "$RUN/PARAM.in" "$SAB_TIME_SCALE" <<'PY'
import sys
path, scale = sys.argv[1], float(sys.argv[2])
lines = open(path, encoding="ascii", errors="replace").read().splitlines(keepends=True)
hit = 0
i = 0
while i < len(lines):
    if lines[i].startswith("#STOP"):
        # the #STOP block is: MaxIteration, then tSimulationMax
        j, seen = i + 1, 0
        while j < len(lines) and seen < 2:
            if lines[j].strip():
                seen += 1
                if seen == 2:
                    head, sep, tail = lines[j].partition("\t")
                    lines[j] = "%.10g%s%s" % (float(head.strip()) * scale, sep or "\t\t\t", tail or "tSimulationMax\n")
                    if not lines[j].endswith("\n"):
                        lines[j] += "\n"
                    hit += 1
            j += 1
        i = j
        continue
    i += 1
if hit == 0:
    sys.exit("run.sh: SAB_TIME_SCALE: no #STOP block found in PARAM.in")
open(path, "w", encoding="ascii").write("".join(lines))
PY
fi

# ---- SAB_PLOT_FRAMES rewrites the graded #SAVEPLOT cadence (2026-09-13 window
# revision) from the window above so it keeps yielding >= 5 frames and stays
# tunable; cadence = window / SAB_PLOT_FRAMES.
WINDOW="$(awk '$2=="tSimulationMax" && $1+0>0 {v=$1} END{print v+0}' "$RUN/PARAM.in")"
python3 - "$RUN/PARAM.in" "$WINDOW" "$SAB_PLOT_FRAMES" <<'PY'
import sys, re
path, window, frames = sys.argv[1], float(sys.argv[2]), int(sys.argv[3])
series = '1d var idl_ascii'
label = 'DtSavePlot'
cadence = window / frames
valstr = str(max(1, round(cadence))) if label.startswith("Dn") else ("%.10g" % cadence)
lines = open(path, encoding="ascii", errors="replace").read().splitlines(keepends=True)
current = None
hit = False
for idx, line in enumerate(lines):
    toks = line.split()
    if not toks:
        continue
    tag = toks[-1]
    if tag in ("StringPlot", "PlotString"):
        current = " ".join(toks[:-1]).lower()
    elif tag == label and current == series:
        m = re.match(r'^(\S+)(\s*)(.*)$', line)
        sep = m.group(2) if m.group(2) else "\t\t"
        lines[idx] = valstr + sep + m.group(3)
        if not lines[idx].endswith("\n"):
            lines[idx] += "\n"
        hit = True
if not hit:
    sys.exit("run.sh: SAB_PLOT_FRAMES: could not find %s for series %r" % (label, series))
open(path, "w", encoding="ascii").write("".join(lines))
PY

# Run, exactly as Makefile.test's test_<name>_run does: mpiexec then PostProc.pl.
# --bind-to none keeps concurrent checks from all landing on the same two cores;
# it changes wall time only, never the arithmetic.
cd "$RUN"
if ! mpiexec --bind-to none --oversubscribe -n "$SAB_MPI_RANKS" ./BATSRUS.exe >runlog 2>&1; then
  echo "run.sh: BATSRUS.exe failed" >&2; tail -n 60 runlog >&2; exit 1
fi
./PostProc.pl -m -replace RESULT >postproc.log 2>&1 || { echo "run.sh: PostProc.pl failed" >&2; tail -n 30 postproc.log >&2; exit 1; }

# Graded files, named as rubric.json lists them.
#   final.out     the last snapshot of the 1d__var_1_* plot series, written at
#                 the deck's tSimulationMax (BATSRUS shortens the last step to land
#                 on it exactly, src/ModBatsrusMethods.f90:617), in ASCII IDL
# The run's log file is deliberately not graded: BATSRUS writes it at six
# significant digits (src/ModWriteLogSatFile.f90:395, format es14.5e3), and at
# that precision a difference far below the graded bound can still cross a
# printing boundary and appear as one whole unit in the last printed place.
FINAL="$(ls -1 "RESULT/GM/1d__var_1_"*.out 2>/dev/null | LC_ALL=C sort | tail -n 1)"
[ -n "$FINAL" ] || { echo "run.sh: no RESULT/GM/1d__var_1_*.out plot file was written" >&2; ls -la RESULT/GM >&2 || true; exit 1; }
FRAMES="$(ls -1 "RESULT/GM/1d__var_1_"*.out 2>/dev/null | wc -l | tr -d ' ')"
if [ "$FRAMES" -lt 5 ]; then
  echo "run.sh: only $FRAMES frames of the graded series were written (need >= 5)" >&2
  exit 1
fi
echo "SAB_PLOT_FRAMES=$FRAMES"
cp "$FINAL" "$OUT_DIR/final.out"
