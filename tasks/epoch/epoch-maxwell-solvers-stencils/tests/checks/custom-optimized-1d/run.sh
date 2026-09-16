#!/usr/bin/env bash
# Check custom-optimized-1d: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     OPTIONAL: the nominal inputs on the alternative build (ALTBUILD below)
#   run.sh --help                       list the runtime knobs below, and the altbuild line when one is declared
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

# Runtime knobs. Defaults are the graded values, which are the upstream test's
# own resolution, window and rank layout; override for iteration only, e.g.
#   SAB_NX=80 sab.py task selfcheck ...
# Parallel build jobs default to the CPUs this container may use (cgroup v2 cpu.max), not the host count.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_NX "240" "cells along x of the deck (upstream: 240); runtime scales as the square, because dt falls with dx"
knob SAB_TEND_FS "75" "end time in femtoseconds (upstream: 75); the dump cadence scales with it, so the graded dump count stays 8; runtime scales linearly"
knob SAB_NPROCX "2" "MPI ranks along x; the deck's nprocx, and the total rank count (upstream test: 2)"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the one build of epoch1d (default: the CPUs allowed to this container); each job needs about 0.2 GB"
# Alternative build, declared for this check: the same pinned source and deck with the
# gfortran FFLAGS line of the scratch copy's epoch1d/Makefile changed from -O3 to -O0
# (the plain -O0 build, without the MODE=debug profile's traps and bounds checks, which
# abort inside Open MPI/PMIx's own MPI_Init on this multi-rank deck -- signal 8 in
# mpi_minimal_init, not in EPOCH's arithmetic). `run.sh altbuild` runs ic/nominal on it
# and selfcheck measures the floor.
ALTBUILD="the same pinned source and deck with the makefile's gfortran FFLAGS line changed from -O3 to -O0 in the scratch build copy (epoch1d/Makefile): the -O0 build without the debug profile's traps and bounds checks, which abort inside MPI_Init under this Open MPI"
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

# A cache is scoped to this produce invocation, outside OUT_ROOT so its
# bookkeeping can never become graded output.  The complete source tree and
# all build/tool inputs are part of the key; a missing or partially published
# entry always falls back to this check's independent cold build.
DIMENSION="epoch1d"
BUILD_GROUP="${DIMENSION}-gfortran-normal-double"
if [ "$IC" = altbuild ]; then
  # Alternative builds intentionally bypass and never populate the normal
  # cache: their changed flags must not be mistaken for the nominal binary.
  echo "SAB_BUILD_CACHE=bypass reason=altbuild group=$BUILD_GROUP"
  cp -R "$SOURCE_DIR/." "$WORK/src"
  BUILD_START=$(date +%s.%N)
  n="$(grep -c '^  FFLAGS = -O3 -g -std=f2003$' "$WORK/src/epoch1d/Makefile" || true)"
  [ "$n" = 1 ] || { echo "run.sh: expected exactly one gfortran FFLAGS line in epoch1d/Makefile, found $n" >&2; exit 2; }
  sed -i 's/^  FFLAGS = -O3 -g -std=f2003$/  FFLAGS = -O0 -g -std=f2003/' "$WORK/src/epoch1d/Makefile"
  make -C "$WORK/src/epoch1d" COMPILER=gfortran -j"$SAB_MAKE_JOBS" > "$WORK/make.log" 2>&1
  BUILD_SECONDS="$(awk -v start="$BUILD_START" -v end="$(date +%s.%N)" 'BEGIN { print end - start }')"
else
BUILD_FINGERPRINT="$(python3 - "$SOURCE_DIR" "$BUILD_GROUP" "$SAB_MAKE_JOBS" <<'PYFINGERPRINT'
import hashlib
import os
import stat
import subprocess
import sys

root, group, make_jobs = sys.argv[1:]
root = os.path.realpath(root)
hash_ = hashlib.sha256()

def field(name, value):
    data = os.fsencode(str(value))
    hash_.update(os.fsencode(name) + b"\0" + str(len(data)).encode("ascii") + b"\0" + data)

field("cache-schema", "epoch-normal-build-v1")
field("source-root", root)
field("build-group", group)
field("dimension", "epoch1d")
field("precision", "double-default")
field("compiler-argument", "COMPILER=gfortran")
field("make-target", "default")
field("make-jobs", make_jobs)
field("compiler-flags", "-O3 -g -std=f2003")
for name, command in (("compiler-version", ["gfortran", "--version"]),
                     ("make-version", ["make", "--version"])):
    result = subprocess.run(command, check=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    field(name, result.stdout.decode(errors="replace"))
field("machine", os.uname().machine)

def tree(directory, relative="."):
    entries = sorted(os.scandir(directory), key=lambda entry: os.fsencode(entry.name))
    for entry in entries:
        rel = entry.name if relative == "." else os.path.join(relative, entry.name)
        info = entry.stat(follow_symlinks=False)
        field("path", rel)
        field("mode", format(stat.S_IMODE(info.st_mode), "04o"))
        if stat.S_ISLNK(info.st_mode):
            field("kind", "symlink")
            field("target", os.readlink(entry.path))
        elif stat.S_ISDIR(info.st_mode):
            field("kind", "directory")
            tree(entry.path, rel)
        elif stat.S_ISREG(info.st_mode):
            field("kind", "file")
            field("size", info.st_size)
            with open(entry.path, "rb") as stream:
                while chunk := stream.read(1024 * 1024):
                    hash_.update(chunk)
        else:
            raise SystemExit(f"run.sh: unsupported source entry for build cache: {rel}")

tree(root)
print(hash_.hexdigest())
PYFINGERPRINT
)"
CACHE_ROOT="$(dirname "$OUT_DIR").sab-build-cache"
CACHE_DIR="$CACHE_ROOT/$BUILD_GROUP/$BUILD_FINGERPRINT"
CACHE_BINARY="$CACHE_DIR/epoch1d"
CACHE_DIGEST_FILE="$CACHE_DIR/epoch1d.sha256"
CACHE_READY="$CACHE_DIR/ready.sha256"
CACHE_HIT=0
if [ -x "$CACHE_BINARY" ] && [ -f "$CACHE_DIGEST_FILE" ] && [ -f "$CACHE_READY" ]; then
  READY_FINGERPRINT="$(cat "$CACHE_READY" 2>/dev/null || true)"
  EXPECTED_BINARY_DIGEST="$(cat "$CACHE_DIGEST_FILE" 2>/dev/null || true)"
  ACTUAL_BINARY_DIGEST="$(sha256sum "$CACHE_BINARY" 2>/dev/null | cut -d' ' -f1 || true)"
  if [ "$READY_FINGERPRINT" = "$BUILD_FINGERPRINT" ] \
      && [ -n "$EXPECTED_BINARY_DIGEST" ] \
      && [ "$EXPECTED_BINARY_DIGEST" = "$ACTUAL_BINARY_DIGEST" ]; then
    CACHE_HIT=1
  fi
fi
  if [ "$CACHE_HIT" -eq 1 ]; then
    if mkdir -p "$WORK/src/epoch1d/bin" \
        && cp "$CACHE_BINARY" "$WORK/src/epoch1d/bin/epoch1d" \
        && [ -x "$WORK/src/epoch1d/bin/epoch1d" ]; then
      echo "SAB_BUILD_CACHE=hit group=$BUILD_GROUP fingerprint=$BUILD_FINGERPRINT"
      BUILD_SECONDS=0
    else
      echo "run.sh: cached binary could not be copied; rebuilding group $BUILD_GROUP" >&2
      CACHE_HIT=0
    fi
  fi
  if [ "$CACHE_HIT" -eq 0 ]; then
    echo "SAB_BUILD_CACHE=miss group=$BUILD_GROUP fingerprint=$BUILD_FINGERPRINT"
    cp -R "$SOURCE_DIR/." "$WORK/src"
    BUILD_START=$(date +%s.%N)
    make -C "$WORK/src/epoch1d" COMPILER=gfortran -j"$SAB_MAKE_JOBS" > "$WORK/make.log" 2>&1
    BUILD_SECONDS="$(awk -v start="$BUILD_START" -v end="$(date +%s.%N)" 'BEGIN { print end - start }')"
    [ -x "$WORK/src/epoch1d/bin/epoch1d" ] || { echo "run.sh: build did not produce epoch1d/bin/epoch1d" >&2; exit 1; }
    # Publish only after the binary and its digest exist.  A ready marker
    # containing `building` makes an interrupted publication unusable.
    if mkdir -p "$CACHE_DIR" \
        && printf '%s\n' building > "$CACHE_READY" \
        && cp "$WORK/src/epoch1d/bin/epoch1d" "$CACHE_BINARY" \
        && sha256sum "$CACHE_BINARY" | cut -d' ' -f1 > "$CACHE_DIGEST_FILE" \
        && printf '%s\n' "$BUILD_FINGERPRINT" > "$CACHE_READY"; then
      echo "SAB_BUILD_CACHE=published group=$BUILD_GROUP fingerprint=$BUILD_FINGERPRINT"
    else
      echo "run.sh: warning: could not publish build cache for group $BUILD_GROUP; using local build" >&2
    fi
  fi
fi
echo "SAB_BUILD_SECONDS=$BUILD_SECONDS"   # nonzero on a compile; exactly zero only on a verified normal-cache hit

RANKS=$(( $SAB_NPROCX ))
SNAP_FS="$(awk -v t="$SAB_TEND_FS" 'BEGIN{printf "%.17g", t * 12 / 75}')"

deck="$CHECK_DIR/ic/$INPUTS/optimized/input.deck"      # the one official deck this check runs
case_id="optimized"
mkdir -p "$WORK/run/$case_id"
# The knobs rewrite only the resolution, the window, the dump cadence and the
# rank layout; every other line is the upstream deck. The anchors are
# unambiguous: the control block's t_end is 75 * femto while the laser
# block's own t_end, where it has one, is 14 * femto.
sed -e "s|^\( *nx *= *\)240 *$|\1$SAB_NX|" \
    -e "s|^\( *t_end *= *\)75\( *\* *femto\)|\1$SAB_TEND_FS\2|" \
    -e "s|^\( *dt_snapshot *= *\)12\( *\* *femto\)|\1$SNAP_FS\2|" \
    -e "s|^\( *nprocx *= *\)2 *$|\1$SAB_NPROCX|" \
    "$deck" > "$WORK/run/$case_id/input.deck"
( cd "$WORK/src/epoch1d" \
  && echo "$WORK/run/$case_id" | mpirun -n "$RANKS" --oversubscribe --bind-to none \
       ./bin/epoch1d > "$WORK/run/$case_id/run.log" 2>&1 )
# Graded files: Ey and Bz of every dump, as raw little-endian float64.
# extract.py decodes only the plain-variable blocks, so the run date and the
# machine name in the SDF header can never reach a graded file.
python3 "$CHECK_DIR/extract.py" "$OUT_DIR" \
    "Electric Field/Ey=${case_id}_Ey" "Magnetic Field/Bz=${case_id}_Bz" \
    -- "$WORK/run/$case_id"/*.sdf
