#!/usr/bin/env bash
# Check fmm-crossed-grating-orders: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (ALTBUILD below)
#   run.sh --help                       list the runtime knobs below, and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_NUMG "101" "NumBasis. Upstream's SetNumG(100) is overridden by this single-iteration loop, so the loop start is the effective basis size. The layer eigenproblem is 2*NumBasis square, so cost grows as its cube."
# Alternative build: the same pinned source at -O0 rather than -O2 and with -DHAVE_BLAS -DHAVE_LAPACK,
# which sends the layer eigenproblem to LAPACK zgeev instead of the in-tree reference eigensolver in
# S4/RNP/Eigensystems.cpp. Both are legitimate builds of this source - upstream's own Makefile.Msys2
# defines HAVE_LAPACK - so a correct candidate could plausibly be either.
ALTBUILD="-O0 instead of -O2, plus -DHAVE_BLAS -DHAVE_LAPACK (LAPACK zgeev in place of the in-tree eigensolver)"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"; OPT="-O2"; LA_DEFS=""
if [ "$IC" = altbuild ]; then INPUTS=nominal; OPT="-O0"; LA_DEFS="-DHAVE_BLAS -DHAVE_LAPACK"; fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

# Upstream test this check reproduces: code/s4/examples/2d/Li_JOSA_14_2758_1997/ex3.lua
# Flags are pinned rather than inherited from Makefile.Linux, which appends -march=native (not
# reproducible across machines) and -fcx-limited-range (drops the NaN-rescue branch of complex
# multiply and divide, in a solver written entirely in std::complex<double>).
# -Wno-error=int-conversion is required: main_lua.c:2207 passes an S4_LayerID (an int) where
# Simulation_GetAmplitudes wants an S4_Layer *, which clang 16+ and gcc 14+ reject. No check calls it.
NOMINAL_CFLAGS="-O2 -fPIC -Wall -Wno-error=int-conversion"
NOMINAL_CXXFLAGS="-O2 -fPIC -Wall -std=c++11"
NOMINAL_CPPFLAGS="-IS4 -IS4/RNP -IS4/kiss_fft"
NOMINAL_LUA_INC="-I/usr/include/lua5.2"
NOMINAL_LUA_LIB="-llua5.2"
NOMINAL_LA_LIBS="-llapack -lblas"
NOMINAL_OBJDIR="build"

# Hash every source path, kind, permission mode and byte (or symlink target),
# plus the complete normal build recipe, compiler identities, make identity and
# machine architecture. A changed input gets a different cache directory.
normal_build_fingerprint() {
  python3 - "$SOURCE_DIR" \
    "$NOMINAL_CFLAGS" "$NOMINAL_CXXFLAGS" "$NOMINAL_CPPFLAGS" \
    "$NOMINAL_LUA_INC" "$NOMINAL_LUA_LIB" "$NOMINAL_LA_LIBS" "$NOMINAL_OBJDIR" <<'PY'
import hashlib
import os
import stat
import subprocess
import sys

(root, cflags, cxxflags, cppflags,
 lua_inc, lua_lib, la_libs, objdir) = sys.argv[1:]
h = hashlib.sha256()


def add_field(name, value):
    data = os.fsencode(value)
    h.update(os.fsencode(name) + b"\0" + str(len(data)).encode("ascii") + b"\0" + data)


add_field("cache-schema", "s4-normal-build-v1")
add_field("make-target", "build/S4")
for name, value in (
    ("CFLAGS", cflags), ("CXXFLAGS", cxxflags), ("CPPFLAGS", cppflags),
    ("LUA_INC", lua_inc), ("LUA_LIB", lua_lib),
    ("LA_LIBS", la_libs), ("OBJDIR", objdir),
):
    add_field(name, value)
for name, command in (("cc-version", ["gcc", "--version"]),
                      ("cxx-version", ["g++", "--version"]),
                      ("make-version", ["make", "--version"])):
    proc = subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    add_field(name, os.fsdecode(proc.stdout))
add_field("machine", os.uname().machine)

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
PY
}

build_s4() {
  local cflags=$1 cxxflags=$2 cppflags=$3 build_log=$4
  make -C "$WORK/src" build/S4 \
    CFLAGS="$cflags" \
    CXXFLAGS="$cxxflags" \
    CPPFLAGS="$cppflags" \
    LUA_INC="-I/usr/include/lua5.2" LUA_LIB="-llua5.2" \
    LA_LIBS="-llapack -lblas" OBJDIR=build >"$build_log" 2>&1 \
    || { echo "run.sh: build failed (CFLAGS=$cflags CPPFLAGS=$cppflags); last 40 lines:" >&2; tail -40 "$build_log" >&2; exit 1; }
}

if [ "$IC" = altbuild ]; then
  # The alternative -O0/LAPACK build remains deliberately independent per check.
  echo "SAB_BUILD_CACHE=bypass reason=altbuild"
  BUILD_START=$(date +%s)
  build_s4 "$OPT -fPIC -Wall -Wno-error=int-conversion" "$OPT -fPIC -Wall -std=c++11" "-IS4 -IS4/RNP -IS4/kiss_fft $LA_DEFS" "$WORK/build.log"
  BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
else
  BUILD_FINGERPRINT="$(normal_build_fingerprint)"
  CACHE_ROOT="$(dirname "$OUT_DIR")/.s4-normal-build-cache"
  CACHE_DIR="$CACHE_ROOT/$BUILD_FINGERPRINT"
  CACHE_BINARY="$CACHE_DIR/build/S4"
  CACHE_DIGEST_FILE="$CACHE_DIR/build/S4.sha256"
  CACHE_READY="$CACHE_DIR/ready.sha256"
  CACHE_HIT=0

  # The ready marker is published last. A missing/malformed marker, missing
  # executable, or binary-digest mismatch is a cache miss and takes the full
  # self-contained build path below.
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
    mkdir -p "$WORK/src/build"
    cp "$CACHE_BINARY" "$WORK/src/build/S4"
    BUILD_SECONDS=0
  else
    echo "SAB_BUILD_CACHE=miss fingerprint=$BUILD_FINGERPRINT"
    BUILD_START=$(date +%s)
    build_s4 "$NOMINAL_CFLAGS" "$NOMINAL_CXXFLAGS" "$NOMINAL_CPPFLAGS" "$WORK/build.log"
    BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))
    mkdir -p "$CACHE_DIR/build"
    cp "$WORK/src/build/S4" "$CACHE_BINARY"
    sha256sum "$CACHE_BINARY" | cut -d' ' -f1 >"$CACHE_DIGEST_FILE"
    printf '%s\n' "$BUILD_FINGERPRINT" >"$CACHE_READY"
  fi
fi
echo "SAB_BUILD_SECONDS=$BUILD_SECONDS"   # the driver records it; the budget counts run time only

# Stage the deck and apply the knob. The deck already carries the graded value, so this
# rewrite is a no-op unless the knob was overridden.
sed -E 's/(for ng = )([0-9]+)(,401,4000 do)/\1'"${SAB_NUMG}"'\3/' "$CHECK_DIR/ic/$INPUTS/input.lua" > "$WORK/input.lua"
(cd "$WORK" && "$WORK/src/build/S4" input.lua) > "$OUT_DIR/output.txt"
