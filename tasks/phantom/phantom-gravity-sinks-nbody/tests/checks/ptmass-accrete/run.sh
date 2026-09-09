#!/usr/bin/env bash
# Check ptmass-accrete: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh altbuild                     the nominal inputs on the alternative build (see ALTBUILD below)
#   run.sh --help                       list the runtime knobs below and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads CHECK_DIR and SOURCE_DIR; writes OUT_DIR and one solve-local prebuild cache beside it; no network; never modifies SOURCE_DIR.
#
# Official test: the Phantom unit-test suite, built as `make ${MAKE_EXTRA[@]+"${MAKE_EXTRA[@]}"} SETUP=testgrav phantomtest`
# (build/Makefile_setups, which adds -DGRAVITY and CONST_ARTRES) and run as
# `bin/phantomtest ptmassaccrete`, i.e. test_accretion in code/phantom/src/tests/test_ptmass.f90 dispatched by src/tests/testsuite.f90.
# test_accretion drives accrete_particle in src/main/ptmass.F90 three times: two gas particles onto one sink, a whole 1000-particle disc onto the same sink, and the fast-accretion path that uses a neighbour list. Each sub-test checks the accretion and dead-particle flags and then the mass, the centre-of-mass position and velocity, the linear momentum and the angular momentum of the sink against the exact budget of what it swallowed.
# The graded file is results.txt: the assertion lines of the suite's own stdout.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_THREADS=2 sab.py task selfcheck ...
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_THREADS "1" "OMP_NUM_THREADS for bin/phantomtest; the graded default is 1, the only setting under which the tree and sink sums are summed in a fixed order. The suite's own window and resolution are compiled into src/tests, so this is the only runtime knob"
# Alternative build: Phantom's own gfortran DEBUG=yes build replaces -O3 with -O0 and enables its runtime checks.
# `run.sh altbuild` uses the nominal inputs; selfcheck measures the floor from the second legitimate build.
ALTBUILD="make SYSTEM=gfortran OPENMP=yes DEBUG=yes: the same pinned source with Phantom's own -O0 gfortran debug build (bounds, NaN and floating-point checks) instead of the nominal -O3 build"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; [ -z "$ALTBUILD" ] || echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"; MAKE_EXTRA=()
if [ "$IC" = altbuild ]; then INPUTS=nominal; MAKE_EXTRA=(SYSTEM=gfortran OPENMP=yes DEBUG=yes); fi
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
SRC="$WORK/src"; RUN="$WORK/run"
mkdir -p "$RUN"
export SYSTEM=gfortran OMP_NUM_THREADS="$SAB_THREADS"

# The twelve SETUP=testgrav checks share only an unpatched, optimized prebuild.
# The cache lives beside the check output directories, so it is scoped to this
# one solve.  Its key covers the exact source bytes, toolchain, machine and make
# recipe.  Every check copies the prebuilt tree before applying its own patch;
# a missing or invalid cache therefore falls back to the original full build.
normal_prebuild_fingerprint() {
  python3 - "$SOURCE_DIR" <<'PY_CACHE'
import hashlib, os, pathlib, subprocess, sys
root = pathlib.Path(sys.argv[1]).resolve()
h = hashlib.sha256()

def field(name, value):
    h.update(name.encode('utf-8') + b'\0' + value.encode('utf-8', 'surrogateescape') + b'\0')

field('recipe', 'SYSTEM=gfortran;SETUP=testgrav;goal=phantomtest;mode=optimized')
field('machine', os.uname().machine)
field('gfortran', subprocess.check_output(['gfortran', '--version'], text=True).splitlines()[0])
for dirpath, dirnames, filenames in os.walk(root):
    dirnames.sort()
    filenames.sort()
    base = pathlib.Path(dirpath)
    for name in filenames:
        path = base / name
        rel = path.relative_to(root).as_posix()
        field('path', rel)
        if path.is_symlink():
            field('symlink', os.readlink(path))
            continue
        if not path.is_file():
            raise SystemExit(f'run.sh: unsupported source entry for prebuild cache: {rel}')
        with path.open('rb') as stream:
            while True:
                block = stream.read(1024 * 1024)
                if not block:
                    break
                h.update(block)
        h.update(b'\0')
print(h.hexdigest())
PY_CACHE
}

BUILD_SECONDS=0
USE_PREBUILD=0
if [ "$IC" = altbuild ]; then
  echo "SAB_BUILD_CACHE=bypass reason=altbuild"
  cp -R "$SOURCE_DIR/." "$SRC"
else
  BUILD_FINGERPRINT="$(normal_prebuild_fingerprint)"
  CACHE_ROOT="$(dirname "$OUT_DIR")/.phantom-testgrav-prebuild-cache"
  CACHE_DIR="$CACHE_ROOT/$BUILD_FINGERPRINT"
  CACHE_SRC="$CACHE_DIR/src"
  CACHE_BINARY="$CACHE_SRC/bin/phantomtest"
  CACHE_DIGEST_FILE="$CACHE_DIR/phantomtest.sha256"
  CACHE_READY="$CACHE_DIR/ready.sha256"
  CACHE_HIT=0

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
    echo "SAB_BUILD_CACHE=hit group=testgrav fingerprint=$BUILD_FINGERPRINT"
    USE_PREBUILD=1
  else
    echo "SAB_BUILD_CACHE=miss group=testgrav fingerprint=$BUILD_FINGERPRINT"
    if mkdir -p "$CACHE_ROOT" 2>/dev/null && mkdir "$CACHE_DIR" 2>/dev/null; then
      mkdir -p "$CACHE_SRC"
      cp -R "$SOURCE_DIR/." "$CACHE_SRC"
      PREBUILD_START=$(date +%s)
      if ! (cd "$CACHE_SRC" && make SETUP=testgrav phantomtest >"$CACHE_DIR/prebuild.log" 2>&1); then
        echo "run.sh: shared SETUP=testgrav prebuild failed" >&2
        tail -n 40 "$CACHE_DIR/prebuild.log" >&2
        exit 1
      fi
      BUILD_SECONDS=$(( $(date +%s) - PREBUILD_START ))
      [ "$BUILD_SECONDS" -gt 0 ] || BUILD_SECONDS=1
      sha256sum "$CACHE_BINARY" | cut -d' ' -f1 >"$CACHE_DIGEST_FILE"
      printf '%s\n' "$BUILD_FINGERPRINT" >"$CACHE_READY"
      USE_PREBUILD=1
    else
      echo "SAB_BUILD_CACHE=fallback group=testgrav reason=cache-unavailable"
    fi
  fi

  if [ "$USE_PREBUILD" -eq 1 ]; then
    # Preserve source/object mtimes so a non-empty variant patch rebuilds only
    # the affected unit-test object and link, while an empty nominal patch is
    # an exact reuse of the verified binary.
    cp -a "$CACHE_SRC/." "$SRC"
  else
    cp -R "$SOURCE_DIR/." "$SRC"
  fi
fi

# The initial condition. This test's inputs are literals in code/phantom/src/tests/test_ptmass.f90, so ic/<ic>/
# carries a unified diff against that file (nominal's is empty) and the selector list.
# The diff is applied here, before the build, by a strict applier: every context and
# deleted line must match the pinned source or the run fails.
python3 - "$SRC" "$CHECK_DIR/ic/$INPUTS/source.patch" <<'PY'
import pathlib, re, sys
root, patch = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
text = patch.read_text(encoding="utf-8")
if text.strip():
    lines, i = text.split("\n"), 0
    while i < len(lines):
        if not lines[i].startswith("--- "):
            i += 1
            continue
        if not lines[i + 1].startswith("+++ "):
            sys.exit("run.sh: malformed patch header")
        target = root.joinpath(*lines[i + 1][4:].split("\t")[0].split("/")[1:])
        src = target.read_text(encoding="utf-8").split("\n")
        out, pos, i = [], 0, i + 2
        while i < len(lines) and lines[i].startswith("@@"):
            m = re.match(r"@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@", lines[i])
            if not m:
                sys.exit("run.sh: malformed hunk header")
            start, nold, nnew, i = int(m.group(1)) - 1, int(m.group(2) or 1), int(m.group(4) or 1), i + 1
            out.extend(src[pos:start]); pos = start
            while nold > 0 or nnew > 0:
                ln = lines[i]; i += 1
                tag, body = (ln[:1], ln[1:]) if ln else (" ", "")
                if tag == "\\":
                    continue
                if tag in (" ", "-"):
                    if src[pos] != body:
                        sys.exit("run.sh: patch context does not match the pinned source at line %d" % (pos + 1))
                    pos, nold = pos + 1, nold - 1
                    if tag == " ":
                        out.append(body); nnew -= 1
                elif tag == "+":
                    out.append(body); nnew -= 1
                else:
                    sys.exit("run.sh: unexpected patch line %r" % ln)
        out.extend(src[pos:])
        target.write_text("\n".join(out), encoding="utf-8")
PY

# Build the unit-test binary for this SETUP. Parallel make is broken upstream
# (build/.depends is absent, so no Fortran module dependencies are expressed) and two
# goals in one invocation clean each other, so the build is serial with a single goal.
# An empty normal patch can use the shared binary exactly.  A unique variant
# patch recompiles from the shared base and reports that incremental work; an
# altbuild or cache fallback performs the original independent full build.
if [ "$IC" != altbuild ] && [ "$USE_PREBUILD" -eq 1 ] && [ ! -s "$CHECK_DIR/ic/$INPUTS/source.patch" ]; then
  if [ "$CACHE_HIT" -eq 1 ]; then
    BUILD_SECONDS=0
    echo "SAB_BUILD_CACHE=reuse group=testgrav binary=verified"
  fi
else
  BUILD_START=$(date +%s)
  if ! (cd "$SRC" && make ${MAKE_EXTRA[@]+"${MAKE_EXTRA[@]}"} SETUP=testgrav phantomtest >"$WORK/make.log" 2>&1); then
    echo "run.sh: build failed" >&2; tail -n 40 "$WORK/make.log" >&2; exit 1
  fi
  BUILD_DELTA=$(( $(date +%s) - BUILD_START ))
  [ "$BUILD_DELTA" -gt 0 ] || BUILD_DELTA=1
  BUILD_SECONDS=$(( BUILD_SECONDS + BUILD_DELTA ))
fi
echo "SAB_BUILD_SECONDS=$BUILD_SECONDS"   # nonzero for a build, exactly zero only for a verified final-binary reuse

# The graded run. A selector that matches nothing silently runs the WHOLE suite, and
# under a SETUP without -DGRAVITY the self-gravity tests report a pass with zero
# assertions, so the assertion count is checked below and must be positive.
cd "$RUN"
cp "$CHECK_DIR/ic/$INPUTS/selectors.txt" .
"$SRC/bin/phantomtest" $(tr '\n' ' ' <selectors.txt) >phantomtest.log 2>&1 || true

# Graded file: the assertion lines only. The banner, the allocation report, the wall and
# CPU times and the thread count are host facts, not results, and are dropped.
grep -E '^[[:space:]]*(checking |PASSED:|FAILED:)' phantomtest.log >results.txt || true
np="$(sed -n 's/^[[:space:]]*PASSED:[[:space:]]*\([0-9][0-9]*\)[[:space:]]*of.*/\1/p' results.txt | tail -n 1)"
nt="$(sed -n 's/^[[:space:]]*PASSED:[[:space:]]*[0-9][0-9]*[[:space:]]*of[[:space:]]*\([0-9][0-9]*\).*/\1/p' results.txt | tail -n 1)"
[ -n "${np:-}" ] && [ "$np" -gt 0 ] || { echo "run.sh: the suite asserted nothing (PASSED='${np:-none}' of '${nt:-none}')" >&2; tail -n 40 phantomtest.log >&2; exit 1; }
# OUT_DIR carries the graded file and nothing else: the run log stays in the work
# directory, because the verifier byte-compares every file it finds in OUT_DIR and a log
# with a wall-clock time in it would make that byte-identical safeguard inert.
cp results.txt "$OUT_DIR/results.txt"
