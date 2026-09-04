#!/usr/bin/env bash
# Check gnewton-relativistic-orbit: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh --help                       list the runtime knobs below
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# Official test: the Phantom unit-test suite, built as `make SETUP=testgrav phantomtest`
# (build/Makefile_setups, which adds -DGRAVITY and CONST_ARTRES) and run as
# `bin/phantomtest gnewton`, i.e. test_gnewton in code/phantom/src/tests/test_gnewton.f90 dispatched by src/tests/testsuite.f90.
# test_gnewton integrates one test particle on an eccentric orbit around a 1e6 Msun central object in the generalised-Newtonian potential of src/main/extern_gnewton.f90 for a full radial period and checks the conserved energy and angular momentum together with the relativistic periapsis precession angle against its analytic value.
# The graded file is results.txt: the assertion lines of the suite's own stdout.

# Runtime knobs. Defaults are the graded values; override for iteration only,
# e.g. SAB_THREADS=2 sab.py task selfcheck ...
KNOB_HELP=""
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_THREADS "1" "OMP_NUM_THREADS for bin/phantomtest; the graded default is 1, the only setting under which the tree and sink sums are summed in a fixed order. The suite's own window and resolution are compiled into src/tests, so this is the only runtime knob"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
[ -d "$CHECK_DIR/ic/$IC" ] || { echo "run.sh: no initial condition ic/$IC" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
SRC="$WORK/src"; RUN="$WORK/run"
mkdir -p "$RUN"
cp -R "$SOURCE_DIR/." "$SRC"

# The initial condition. This test's inputs are literals in code/phantom/src/tests/test_gnewton.f90, so ic/<ic>/
# carries a unified diff against that file (nominal's is empty) and the selector list.
# The diff is applied here, before the build, by a strict applier: every context and
# deleted line must match the pinned source or the run fails.
python3 - "$SRC" "$CHECK_DIR/ic/$IC/source.patch" <<'PY'
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
export SYSTEM=gfortran OMP_NUM_THREADS="$SAB_THREADS"
BUILD_START=$(date +%s)
if ! (cd "$SRC" && make SETUP=testgrav phantomtest >"$WORK/make.log" 2>&1); then
  echo "run.sh: build failed" >&2; tail -n 40 "$WORK/make.log" >&2; exit 1
fi
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"   # the driver records it; the budget counts run time only

# The graded run. A selector that matches nothing silently runs the WHOLE suite, and
# under a SETUP without -DGRAVITY the self-gravity tests report a pass with zero
# assertions, so the assertion count is checked below and must be positive.
cd "$RUN"
cp "$CHECK_DIR/ic/$IC/selectors.txt" .
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
