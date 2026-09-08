#!/usr/bin/env bash
# Check isomip-steep-icecavity: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (genmake2 -ieee; see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# What it does: copies SOURCE_DIR, builds one MITgcm executable for this
# configuration with the tree's own genmake2 (build configuration in mods/:
# SIZE.h, packages.conf and the *_OPTIONS.h headers of the upstream
# experiment; gfortran optfile linux_amd64_gfortran, single process, tiles
# only), runs it in a scratch directory holding the deck ic/<ic>/ with
# nTimeSteps set from SAB_STEPS, and copies the graded files, every
# <field>.<iteration>.data/.meta pair of the final iteration written by
# MITgcm's end-of-run state dump (dumpInitAndLast), into OUT_DIR. The log,
# the initial-state dump, the final pickup and everything else stay behind.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_STEPS=12 sab.py task selfcheck ...
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_STEPS 60 "time steps (nTimeSteps in the deck, 1800 s each; the upstream deck runs 20); runtime scales linearly, the graded final-state files are named by the final iteration number"
knob SAB_BUILD_JOBS 4 "parallel make jobs for the per-check build of mitgcmuv; wall time only"
# Alternative build: the same source under genmake2 -ieee (gfortran -O0 -ffloat-store, strict IEEE arithmetic)
# instead of the optimised optfile. `run.sh altbuild` runs ic/nominal on it; selfcheck measures the floor from it.
ALTBUILD="genmake2 -ieee: the same source at -O0 -ffloat-store, strict IEEE arithmetic, instead of the optimised optfile"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"; GENMAKE_EXTRA=()
if [ "$IC" = altbuild ]; then INPUTS=nominal; GENMAKE_EXTRA=(-ieee); fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

# Upstream test this check reproduces: code/mitgcm/verification/isomip/input.stic
[ -x "$WORK/src/tools/genmake2" ] || { echo "run.sh: $SOURCE_DIR has no tools/genmake2" >&2; exit 2; }
mkdir "$WORK/build"
OPTFILE="$WORK/src/tools/build_options/linux_amd64_gfortran"
case "$(uname -m)" in
  aarch64|arm64)
    # The upstream x86 optfile's medium code model is not a valid GCC option
    # on arm64; removing only that target-specific flag preserves the recipe.
    sed 's/-mcmodel=medium//g' "$OPTFILE" >"$WORK/build/linux_arm64_gfortran"
    OPTFILE="$WORK/build/linux_arm64_gfortran"
    ;;
esac

# Hash every source and mods path, kind, permission mode and byte (or symlink
# target), plus the complete normal recipe, tool identities and architecture.
# Thus only checks with byte-identical compile-time package/options inputs can
# reuse this executable; the distinct remeshing and streamice recipes cannot.
normal_build_fingerprint() {
  python3 - "$SOURCE_DIR" "$CHECK_DIR/mods" <<'PY_FINGERPRINT'
import hashlib
import os
import stat
import subprocess
import sys

source_root, mods_root = sys.argv[1:]
h = hashlib.sha256()


def add_field(name, value):
    data = os.fsencode(value)
    h.update(os.fsencode(name) + b"\0" + str(len(data)).encode("ascii") + b"\0" + data)


add_field("cache-schema", "mitgcm-normal-build-v1")
add_field("genmake", "tools/genmake2 -rootdir SOURCE -mods MODS -optfile effective-linux-gfortran; arm64 strips only -mcmodel=medium")
add_field("make", "make depend && make -j SAB_BUILD_JOBS")
for name, command in (("gfortran-version", ["gfortran", "--version"]),
                      ("make-version", ["make", "--version"])):
    proc = subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    add_field(name, os.fsdecode(proc.stdout))
add_field("machine", os.uname().machine)


def add_tree(root, label, relative="."):
    with os.scandir(root) as entries:
        ordered = sorted(entries, key=lambda entry: os.fsencode(entry.name))
    for entry in ordered:
        rel = entry.name if relative == "." else os.path.join(relative, entry.name)
        st = entry.stat(follow_symlinks=False)
        add_field(f"{label}-path", rel)
        add_field("mode", format(stat.S_IMODE(st.st_mode), "04o"))
        if stat.S_ISLNK(st.st_mode):
            add_field("kind", "symlink")
            add_field("target", os.readlink(entry.path))
        elif stat.S_ISDIR(st.st_mode):
            add_field("kind", "directory")
            add_tree(entry.path, label, rel)
        elif stat.S_ISREG(st.st_mode):
            add_field("kind", "file")
            h.update(str(st.st_size).encode("ascii") + b"\0")
            with open(entry.path, "rb") as stream:
                while block := stream.read(1024 * 1024):
                    h.update(block)
        else:
            raise SystemExit(f"run.sh: unsupported {label} entry for build cache: {rel}")


add_tree(os.path.realpath(source_root), "source")
add_tree(os.path.realpath(mods_root), "mods")
print(h.hexdigest())
PY_FINGERPRINT
}

build_mitgcm() {
  local build_log=$1
  [ -f "$CHECK_DIR/mods/genmake_local" ] && cp "$CHECK_DIR/mods/genmake_local" "$WORK/build/"   # experiment build flags, read by genmake2 from the build dir
  ( cd "$WORK/build" \
    && "$WORK/src/tools/genmake2" -rootdir "$WORK/src" -mods "$CHECK_DIR/mods" \
         -optfile "$OPTFILE" ${GENMAKE_EXTRA[@]+"${GENMAKE_EXTRA[@]}"} \
    && make depend \
    && make -j "$SAB_BUILD_JOBS" ) >"$build_log" 2>&1 \
    || { tail -n 60 "$build_log" >&2; echo "run.sh: build failed" >&2; exit 1; }
  [ -x "$WORK/build/mitgcmuv" ] || { echo "run.sh: build left no mitgcmuv" >&2; exit 1; }
}

if [ "$IC" = altbuild ]; then
  # Alternative IEEE builds never reuse the normal cache and remain
  # deliberately independent per check.
  echo "SAB_BUILD_CACHE=bypass reason=altbuild"
  BUILD_START=$(date +%s)
  build_mitgcm "$WORK/build.log"
  BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
else
  BUILD_FINGERPRINT="$(normal_build_fingerprint)"
  CACHE_ROOT="$(dirname "$OUT_DIR")/.mitgcm-normal-build-cache"
  CACHE_DIR="$CACHE_ROOT/$BUILD_FINGERPRINT"
  CACHE_BINARY="$CACHE_DIR/mitgcmuv"
  CACHE_DIGEST_FILE="$CACHE_DIR/mitgcmuv.sha256"
  CACHE_READY="$CACHE_DIR/ready.sha256"
  CACHE_HIT=0

  # Publish ready last. Any missing/malformed marker, missing executable, or
  # digest mismatch is a miss and follows the independent full-build path.
  if [ -x "$CACHE_BINARY" ] && [ -f "$CACHE_DIGEST_FILE" ] && [ -f "$CACHE_READY" ]; then
    READY_FINGERPRINT="$(cat "$CACHE_READY" 2>/dev/null || true)"
    EXPECTED_BINARY_DIGEST="$(cat "$CACHE_DIGEST_FILE" 2>/dev/null || true)"
    ACTUAL_BINARY_DIGEST="$(sha256sum "$CACHE_BINARY" | cut -d' ' -f1)"
    if [ "$READY_FINGERPRINT" = "$BUILD_FINGERPRINT" ] \
       && [ -n "$EXPECTED_BINARY_DIGEST" ] \
       && [ "$EXPECTED_BINARY_DIGEST" = "$ACTUAL_BINARY_DIGEST" ]; then
      CACHE_HIT=1
    fi
  fi

  if [ "$CACHE_HIT" -eq 1 ]; then
    echo "SAB_BUILD_CACHE=hit fingerprint=$BUILD_FINGERPRINT"
    cp "$CACHE_BINARY" "$WORK/build/mitgcmuv"
    BUILD_SECONDS=0
  else
    echo "SAB_BUILD_CACHE=miss fingerprint=$BUILD_FINGERPRINT"
    BUILD_START=$(date +%s)
    build_mitgcm "$WORK/build.log"
    BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
    mkdir -p "$CACHE_DIR"
    cp "$WORK/build/mitgcmuv" "$CACHE_BINARY"
    sha256sum "$CACHE_BINARY" | cut -d' ' -f1 >"$CACHE_DIGEST_FILE"
    printf '%s\n' "$BUILD_FINGERPRINT" >"$CACHE_READY"
  fi
fi
echo "SAB_BUILD_SECONDS=$BUILD_SECONDS"   # actual compile time; exactly zero on a verified normal-build reuse hit

mkdir "$WORK/run"
cp "$CHECK_DIR/ic/nominal"/* "$WORK/run/"
[ "$INPUTS" = nominal ] || cp "$CHECK_DIR/ic/$INPUTS"/* "$WORK/run/"   # variant: the deck files that differ, laid over nominal
python3 - "$WORK/run/data" "$SAB_STEPS" <<'PY'
import re, sys
path, steps = sys.argv[1], int(sys.argv[2])
text = open(path, encoding="utf-8").read()
text, n = re.subn(r"^(\s*nTimeSteps\s*=\s*)\S+", lambda m: m.group(1) + str(steps) + ",", text, count=1, flags=re.M | re.I)
if n != 1:
    sys.exit("run.sh: deck has no nTimeSteps line")
open(path, "w", encoding="utf-8").write(text)
PY
cd "$WORK/run"
# A single-process MITgcm writes its log to standard output (STDOUT.0000 only exists for MPI runs).
"$WORK/build/mitgcmuv" >mitgcmuv.stdout 2>mitgcmuv.stderr || { tail -n 40 mitgcmuv.stdout mitgcmuv.stderr >&2 || true; echo "run.sh: mitgcmuv failed" >&2; exit 1; }
grep -q "PROGRAM MAIN: Execution ended Normally" mitgcmuv.stdout || { tail -n 40 mitgcmuv.stdout mitgcmuv.stderr >&2 || true; echo "run.sh: mitgcmuv did not end normally" >&2; exit 1; }
# The final iteration is the deck's nIter0 plus the steps run; collect its dump.
final="$(ls *.data | sed -nE 's/^[A-Za-z_0-9]+\.([0-9]{10})\.data$/\1/p' | sort | tail -1)"
[ -n "$final" ] || { echo "run.sh: no state dump written" >&2; exit 1; }
ls *."$final".data | grep -q "^T\." || { echo "run.sh: final dump $final has no T field" >&2; exit 1; }
cp ./*."$final".data ./*."$final".meta "$OUT_DIR/"
