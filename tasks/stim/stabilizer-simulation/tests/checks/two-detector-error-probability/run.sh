#!/usr/bin/env bash
# Check two-detector-error-probability: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh --help                       list the runtime knobs below
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

KNOB_HELP=""
CHECK_DIR="${CHECK_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)}"
DEF() { python3 -c "import json,sys;print(json.load(open(sys.argv[1]))[sys.argv[2]])" "$CHECK_DIR/ic/nominal/params.json" "$1"; }
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_SHOTS "$(DEF shots)" "shots sampled per error probability (default: ic params). The workload axis: the circuit is two qubits, so cost is entirely in the shot batch the frame simulator packs into words"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
[ -d "$CHECK_DIR/ic/$IC" ] || { echo "run.sh: no initial condition ic/$IC" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

# Upstream example this check reproduces: code/stim/doc/getting_started.ipynb,
# the X_ERROR two-detector circuit and its 1e6-shot detector rate.
BUILD_START=$(date +%s)
cmake -S "$WORK/src" -B "$WORK/b" -G Ninja -DCMAKE_BUILD_TYPE=Release >"$WORK/cmake.log" 2>&1
cmake --build "$WORK/b" --target stim >>"$WORK/cmake.log" 2>&1
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"
STIM="$(find "$WORK/b" -type f -perm -111 -name stim | head -1)"
[ -x "$STIM" ] || { cp "$WORK/cmake.log" "$OUT_DIR/cmake-failed.log" 2>/dev/null; echo "run.sh: stim binary not built; see cmake-failed.log" >&2; exit 1; }

# Which vector word backend this build compiled. Stim's machine flags are
# guarded on CMAKE_SYSTEM_PROCESSOR (CMakeLists.txt:25) and its word backends
# are x86-only, so the same source yields bitword_256_avx on an AVX2 host and
# the portable bitword_64 elsewhere.
{ grep -m1 -oE "march=native|mavx2|msse2" "$WORK/cmake.log" || echo "no-machine-flag"; } > "$OUT_DIR/word_backend.txt"
uname -m >> "$OUT_DIR/word_backend.txt"

PARAMS="$CHECK_DIR/ic/$IC/params.json" OUT="$OUT_DIR" SCRATCH="$WORK" STIM="$STIM" python3 - <<'PYEOF'
import json, os, subprocess
import numpy as np

p = json.load(open(os.environ["PARAMS"])); out = os.environ["OUT"]
scratch = os.environ["SCRATCH"]; stim = os.environ["STIM"]
shots = int(os.environ.get("SAB_SHOTS") or p["shots"])
probs = [float(x) for x in p["error_probabilities"]]

# The circuit is upstream's, written out verbatim per probability. H then CX
# prepares a Bell pair so the two Z measurements agree in the absence of noise;
# X_ERROR flips each independently; the DETECTOR compares the two.
def circuit_for(prob, path):
    with open(path, "w") as fh:
        fh.write("H 0\nTICK\nCX 0 1\nX_ERROR(%r) 0 1\nTICK\nM 0 1\nDETECTOR rec[-1] rec[-2]\n" % prob)

def readn(fh, n):
    """Read exactly n bytes unless EOF. A pipe may return short reads, and a
    partial shot would misalign every subsequent row of the bit table."""
    buf = bytearray()
    while len(buf) < n:
        b = fh.read(n - len(buf))
        if not b:
            break
        buf += b
    return bytes(buf)

# One detector, so b8 packs each shot into a single byte and the graded width is
# probed exactly rather than assumed: ceil(1/8)*8 would grade seven padding bits
# that are structurally zero in every run.
rates = np.zeros(len(probs), dtype=np.float64)
for i, prob in enumerate(probs):
    circ = os.path.join(scratch, "c%d.stim" % i)
    circuit_for(prob, circ)
    probe = os.path.join(scratch, "probe%d.01" % i)
    subprocess.run([stim, "detect", "--shots", "1", "--in", circ,
                    "--out", probe, "--out_format", "01", "--seed", "1"], check=True)
    with open(probe) as fh:
        n_det = len(fh.readline().strip())
    if n_det != 1:
        raise SystemExit("run.sh: expected 1 detector, probed %d" % n_det)
    width = (n_det + 7) // 8

    # Stream the sample from a pipe; nothing is written to disk.
    # Per-probability seeds are spaced by 1000, not by 1. With seed+i, two
    # initial conditions whose seeds differ by less than the number of
    # probabilities would share sampling streams - nominal at seed s would use
    # s, s+1, s+2 and a variant at s+1 would reuse s+1 and s+2 - so most of the
    # graded array would be identical between them and the variant would supply
    # almost no calibration evidence.
    proc = subprocess.Popen(
        [stim, "detect", "--shots", str(shots), "--in", circ,
         "--out_format", "b8", "--seed", str(int(p["seed"]) + 1000 * i)], stdout=subprocess.PIPE)
    fired = 0; nsh = 0
    while True:
        raw = readn(proc.stdout, 10000 * width)
        rows = len(raw) // width
        if rows == 0:
            break
        bits = np.unpackbits(np.frombuffer(raw[: rows * width], dtype=np.uint8).reshape(rows, width),
                             axis=1, bitorder="little")[:, :n_det]
        fired += int(bits.sum()); nsh += rows
    proc.stdout.close()
    if proc.wait() != 0:
        raise SystemExit("run.sh: stim detect exited nonzero at p=%r" % prob)
    if nsh != shots:
        raise SystemExit("run.sh: read %d shots, expected %d" % (nsh, shots))
    rates[i] = fired / float(nsh)

# Graded: the detector firing rate at each error probability. The mean, max and
# min of this array are the three invariants; with one detector per circuit
# there is no spread across detectors to grade, and the per-shot dispersion of a
# single Bernoulli bit is a function of the rate itself, so grading it would
# duplicate an already-graded quantity.
np.save(os.path.join(out, "detector_rates.npy"), rates)
print("shots=%d probabilities=%s" % (shots, probs))
PYEOF
