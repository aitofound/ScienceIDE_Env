#!/usr/bin/env bash
KNOB_HELP=""
CHECK_DIR="${CHECK_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)}"
DEF() { python3 -c "import json,sys;print(json.load(open(sys.argv[1]))[sys.argv[2]])" "$CHECK_DIR/ic/nominal/params.json" "$1"; }
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_DIM "$(DEF dim)" "tableau dimension (default: ic params). The packed bit matrix is dim x dim per block and its transpose is the layout operation under test"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
[ -d "$CHECK_DIR/ic/$IC" ] || { echo "run.sh: no initial condition ic/$IC" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

BUILD_START=$(date +%s)
# The Python module is the observable: stim's CLI exposes no tableau or
# Pauli-string surface, so these checks grade through the bindings that
# CMakeLists.txt:95-97 builds only when pybind11 is found. pybind11 must satisfy
# code/stim/pyproject.toml's `pybind11~=2.11.1`; the image pins it via pip.
cmake -S "$WORK/src" -B "$WORK/b" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -Dpybind11_DIR="$(python3 -m pybind11 --cmakedir)" >"$OUT_DIR/cmake.log" 2>&1
cmake --build "$WORK/b" --target stim_python_bindings >>"$OUT_DIR/cmake.log" 2>&1
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"
MOD="$(find "$WORK/b" -name 'stim*.so' | head -1)"
[ -n "$MOD" ] || { echo "run.sh: stim python module not built" >&2; exit 1; }

# Record the vector word backend this build compiled. Stim's machine flags are
# guarded on CMAKE_SYSTEM_PROCESSOR (CMakeLists.txt:25) and its backends are
# x86-only, so the same source yields bitword_256_avx on an AVX2 host and the
# portable bitword_64 elsewhere. The incumbent is meaningless without it.
{ grep -m1 -oE "march=native|mavx2|msse2" "$OUT_DIR/cmake.log" || echo "no-machine-flag"; } > "$OUT_DIR/word_backend.txt"
uname -m >> "$OUT_DIR/word_backend.txt"

PARAMS="$CHECK_DIR/ic/$IC/params.json" OUT="$OUT_DIR" MOD="$MOD" python3 - <<'PYEOF'
import json, os, sys, importlib.util
import numpy as np
spec = importlib.util.spec_from_file_location("stim", os.environ["MOD"])
stim = importlib.util.module_from_spec(spec); spec.loader.exec_module(stim)
p = json.load(open(os.environ["PARAMS"])); out = os.environ["OUT"]

# stim.Tableau.random(num_qubits) takes NO seed and is genuinely
# non-deterministic - verified: two unseeded random(8) calls compare unequal.
# Using it would make every run a different instance, so with identical initial
# conditions the two solves would disagree at order one and the check could
# never pass. Build the Clifford deterministically instead: a fixed-length
# sequence of named gates chosen by a seeded numpy RNG. Verified reproducible
# for a given seed and distinct across seeds.
def det_tableau(dim, seed):
    rng = np.random.default_rng(seed)
    t = stim.Tableau(dim)
    names = ["H", "S", "CNOT", "CZ", "SWAP"]
    for _ in range(4 * dim):
        g = names[int(rng.integers(0, len(names)))]
        if g in ("H", "S"):
            q = int(rng.integers(0, dim))
            t.append(stim.Tableau.from_named_gate(g), [q])
        else:
            a, b = rng.choice(dim, size=2, replace=False)
            t.append(stim.Tableau.from_named_gate(g), [int(a), int(b)])
    return t

dim = int(os.environ.get("SAB_DIM") or p["dim"])
t = det_tableau(dim, int(p["seed"]))

# The inverse of a Clifford tableau is its conjugate transpose in the symplectic
# sense, so inverting exercises the packed-bit-table transpose directly.
inv = t.inverse()
for name, arr in zip(("x2x","x2z","z2x","z2z","x_signs","z_signs"), inv.to_numpy()):
    np.save(os.path.join(out, f"inv_{name}.npy"), np.ascontiguousarray(arr).astype(np.uint8))

# Double inverse must return the original exactly: a transpose that loses or
# permutes bits fails here even if a single inverse looked structurally valid.
np.save(os.path.join(out, "double_inverse_identity.npy"),
        np.array([1 if t.inverse().inverse() == t else 0], dtype=np.uint8))
# Column popcounts of x2x after transpose equal row popcounts before, so a
# mismatch localises the bug to the transpose rather than the algebra.
xb = np.asarray(t.to_numpy()[0], dtype=np.uint8)
np.save(os.path.join(out, "col_popcounts.npy"), xb.sum(axis=0).astype(np.int64))
print(f"dim={dim} double_inverse_ok={t.inverse().inverse()==t} "
      f"col_popcount_sum={int(xb.sum())}")
PYEOF
