#!/usr/bin/env bash
# Check qed-rese-2d: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     OPTIONAL: the nominal inputs on the alternative build (ALTBUILD below)
#   run.sh --help                       list the runtime knobs below, and the altbuild line when one is declared
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_T_END=20e-15 sab.py task selfcheck ...
# The MPI rank layout (nprocx/nprocy/nprocz in the deck) is deliberately NOT a
# knob: it selects the per-rank random streams and the particle order, so it is
# part of the initial condition, and ic/variant is exactly a different layout.
# Parallel build jobs default to the CPUs this container may use (cgroup v2 cpu.max), not the host count.
cpus_allowed() { local q p; if [ -r /sys/fs/cgroup/cpu.max ] && read -r q p < /sys/fs/cgroup/cpu.max && [ "$q" != max ]; then echo $(( (q + p - 1) / p )); else nproc 2>/dev/null || getconf _NPROCESSORS_ONLN; fi; }
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_NX "128" "cells along x (upstream deck: 128); runtime scales linearly"
knob SAB_NY "128" "cells along y (upstream deck: 128); runtime scales linearly"
knob SAB_PPC "64" "pseudoparticles per cell summed over the two loaded species (upstream: 64); runtime scales linearly"
knob SAB_T_END "40e-15" "end time in seconds (upstream deck: 100e-15); runtime grows faster than linearly because emitted photons accumulate"
knob SAB_DT_SNAPSHOT "2.0e-15" "seconds between dumps (upstream: 1.0e-15); sets how many rows the graded series has"
knob SAB_MAKE_JOBS "$(cpus_allowed)" "parallel jobs for the one build of the pinned source (default: the CPUs allowed to this container); each job needs about 0.3 GB"
# Alternative build, OPTIONAL. Set ALTBUILD to one line naming a legitimately different build of the
# same source (IEEE mode, -O0, a second compiler present in the image: something a correct candidate
# could plausibly be) ONLY when this check can be built that way; leave it empty otherwise. When it is
# set, `run.sh altbuild` runs ic/nominal on that build and selfcheck measures the check's floor from it.
ALTBUILD="the same pinned source built with epoch2d/Makefile's FFLAGS at -O0 instead of -O3 in the scratch build copy only, not SOURCE_DIR (fallback from EPOCH's own MODE=debug profile, whose -ffpe-trap fires in EPOCH's mpi_minimal_init at MPI startup, before any deck-specific code runs, on every check in this leaf, verified on the worker 2026-09-05): the same source at a different optimisation level, which a correct candidate could plausibly be built at"
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
if [ "$IC" = altbuild ]; then
  # ALTBUILD fallback (a): MODE=debug (the Makefile debug profile) trips gfortran's
  # -ffpe-trap in EPOCH's own mpi_minimal_init at MPI startup (mpi_routines.F90 line 109),
  # before any deck-specific code runs, on every check in this leaf; verified on the
  # worker 2026-09-05. Falling back to the same -O3-vs-O2-style profile without traps:
  # the one gfortran FFLAGS line of the scratch copy only, never SOURCE_DIR.
  sed -i 's/^  FFLAGS = -O3 -g -std=f2003$/  FFLAGS = -O0 -g -std=f2003/' "$WORK/src/epoch2d/Makefile"
fi

# Upstream test this check reproduces: code/epoch/epoch2d/example_decks/qed_rese.deck
# Build: the QED package is behind -DPHOTONS; without it the deck aborts at parse time
DIM="2"
# Reuse only a matching executable made earlier in this produce invocation.
# The cache is private to this output root, never SOURCE_DIR and never a graded
# output. Normal and altbuild recipes have separate namespaces.
BUILD_GROUP="epoch${DIM}d"
BUILD_BINARY="$BUILD_GROUP/bin/$BUILD_GROUP"
BUILD_COMPILER="gfortran"
BUILD_PRECISION="default REAL kind with IEEE compiler flags from $BUILD_GROUP/Makefile"
BUILD_DEFINE="-DPHOTONS"
BUILD_KIND=normal
[ "$IC" = altbuild ] && BUILD_KIND=altbuild

# Fingerprint the complete effective build input: source bytes and metadata,
# dimension, exact preprocessor configuration, compiler and numerical precision,
# tool versions, architecture, and build parallelism. The source copy is used
# so the altbuild Makefile edit is part of its key.
build_fingerprint() {
  python3 - "$1" "$BUILD_GROUP" "$BUILD_KIND" "$BUILD_DEFINE" "$BUILD_COMPILER" "$BUILD_PRECISION" "$SAB_MAKE_JOBS" <<'PYFINGERPRINT'
import hashlib
import os
import stat
import subprocess
import sys

root, group, kind, define, compiler, precision, make_jobs = sys.argv[1:]
h = hashlib.sha256()

def add_field(name, value):
    data = os.fsencode(value)
    h.update(os.fsencode(name) + b"\0" + str(len(data)).encode("ascii") + b"\0" + data)

add_field("cache-schema", "epoch-build-v1")
for name, value in (("group", group), ("kind", kind), ("define", define),
                    ("compiler", compiler), ("precision", precision),
                    ("make-jobs", make_jobs), ("architecture", os.uname().machine)):
    add_field(name, value)
for name, command in (("compiler-version", [compiler, "--version"]),
                     ("mpi-compiler-version", ["mpif90", "--version"]),
                     ("make-version", ["make", "--version"])):
    proc = subprocess.run(command, check=True, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT)
    add_field(name, os.fsdecode(proc.stdout))

root = os.path.realpath(root)
def add_tree(directory, relative="."):
    with os.scandir(directory) as entries:
        ordered = sorted(entries, key=lambda entry: os.fsencode(entry.name))
    for entry in ordered:
        rel = entry.name if relative == "." else os.path.join(relative, entry.name)
        st = entry.stat(follow_symlinks=False)
        add_field("path", rel)
        add_field("mode", format(stat.S_IMODE(st.st_mode), "04o"))
        if stat.S_ISLNK(st.st_mode):
            add_field("kind", "symlink")
            add_field("target", os.readlink(entry.path))
        elif stat.S_ISDIR(st.st_mode):
            add_field("kind", "directory")
            add_tree(entry.path, rel)
        elif stat.S_ISREG(st.st_mode):
            add_field("kind", "file")
            h.update(str(st.st_size).encode("ascii") + b"\0")
            with open(entry.path, "rb") as stream:
                while block := stream.read(1024 * 1024):
                    h.update(block)
        else:
            raise SystemExit(f"run.sh: unsupported source entry for build cache: {rel}")
add_tree(root)
print(h.hexdigest())
PYFINGERPRINT
}

SOURCE_TREE="$WORK/src"
BUILD_FINGERPRINT="$(build_fingerprint "$SOURCE_TREE")"
CACHE_ROOT="$(dirname "$OUT_DIR")/.epoch-build-cache"
CACHE_DIR="$CACHE_ROOT/$BUILD_GROUP/$BUILD_KIND/$BUILD_FINGERPRINT"
CACHE_BINARY="$CACHE_DIR/$BUILD_GROUP"
CACHE_DIGEST_FILE="$CACHE_DIR/binary.sha256"
CACHE_READY="$CACHE_DIR/ready.sha256"
CACHE_HIT=0
BUILD_SECONDS=0
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
  if mkdir -p "$(dirname "$WORK/src/$BUILD_BINARY")" \
     && cp "$CACHE_BINARY" "$WORK/src/$BUILD_BINARY"; then
    echo "SAB_BUILD_CACHE=hit group=$BUILD_GROUP kind=$BUILD_KIND fingerprint=$BUILD_FINGERPRINT"
    BUILD_SECONDS=0
  else
    echo "run.sh: cached executable could not be copied; rebuilding group $BUILD_GROUP" >&2
    CACHE_HIT=0
  fi
fi
if [ "$CACHE_HIT" -eq 0 ]; then
  echo "SAB_BUILD_CACHE=miss group=$BUILD_GROUP kind=$BUILD_KIND fingerprint=$BUILD_FINGERPRINT"
  BUILD_START=$(python3 -c 'import time; print(time.monotonic())')
  if [ -n "$BUILD_DEFINE" ]; then
    make -C "$WORK/src/$BUILD_GROUP" COMPILER="$BUILD_COMPILER" DEFINE="$BUILD_DEFINE" -j"$SAB_MAKE_JOBS" > "$WORK/make.log" 2>&1
  else
    make -C "$WORK/src/$BUILD_GROUP" COMPILER="$BUILD_COMPILER" -j"$SAB_MAKE_JOBS" > "$WORK/make.log" 2>&1
  fi
  BUILD_SECONDS=$(python3 - "$BUILD_START" <<'PYELAPSED'
import sys
import time
elapsed = time.monotonic() - float(sys.argv[1])
print(f"{elapsed:.3f}")
PYELAPSED
  )
  [ -x "$WORK/src/$BUILD_BINARY" ] || { echo "run.sh: build left no $BUILD_BINARY" >&2; exit 1; }
  # Publish only after a complete executable and digest are present. A stale or
  # interrupted entry therefore remains a miss and falls back to a cold build.
  if mkdir -p "$CACHE_DIR" \
     && printf '%s\n' building > "$CACHE_READY" \
     && cp "$WORK/src/$BUILD_BINARY" "$CACHE_BINARY" \
     && sha256sum "$CACHE_BINARY" | cut -d' ' -f1 > "$CACHE_DIGEST_FILE" \
     && printf '%s\n' "$BUILD_FINGERPRINT" > "$CACHE_READY"; then
    echo "SAB_BUILD_CACHE=published group=$BUILD_GROUP kind=$BUILD_KIND fingerprint=$BUILD_FINGERPRINT"
  else
    echo "run.sh: warning: could not publish build cache; using local executable" >&2
  fi
fi
echo "SAB_BUILD_SECONDS=$BUILD_SECONDS"   # nonzero on a compile; exactly zero only on a verified cache hit


# The deck of this initial condition, with the runtime knobs written into the
# lines they own (each is tagged with its knob name in the deck).
RUN="$WORK/run"; mkdir -p "$RUN"
sed -E -e "s|^[[:space:]]*nx = .*# SAB_NX\$|  nx = $SAB_NX # SAB_NX|" \
  -e "s|^[[:space:]]*ny = .*# SAB_NY\$|  ny = $SAB_NY # SAB_NY|" \
  -e "s|^[[:space:]]*nparticles = .*# SAB_PPC\$|  nparticles = nx * ny * $SAB_PPC # SAB_PPC|" \
  -e "s|^[[:space:]]*t_end = .*# SAB_T_END\$|  t_end = $SAB_T_END # SAB_T_END|" \
  -e "s|^[[:space:]]*dt_snapshot = .*# SAB_DT_SNAPSHOT\$|  dt_snapshot = $SAB_DT_SNAPSHOT # SAB_DT_SNAPSHOT|" \
  "$CHECK_DIR/ic/$INPUTS/input.deck" > "$RUN/input.deck"

# The rank count is the product of the layout the deck asks for.
ranks=1
for key in nprocx nprocy nprocz; do
  v="$(sed -n "s/^[[:space:]]*$key[[:space:]]*=[[:space:]]*\([0-9][0-9]*\).*/\1/p" "$RUN/input.deck" | head -1)"
  if [ -n "$v" ]; then ranks=$((ranks * v)); fi
done

# EPOCH resolves physics_table_location relative to the working directory, so
# the run is launched from the dimension directory of the build; the deck path
# is fed to the binary on stdin, which is how EPOCH takes its output directory.
cd "$WORK/src/epoch2d"
echo "$RUN" | mpirun -n "$ranks" --oversubscribe --bind-to none --allow-run-as-root bin/epoch2d > "$WORK/run.log" 2>&1

# Graded files: the physical series this check compares, extracted from the SDF
# dumps (never the dumps themselves: their header carries the run date and the
# machine name).
python3 "$CHECK_DIR/extract.py" --run "$RUN" --out "$OUT_DIR"
