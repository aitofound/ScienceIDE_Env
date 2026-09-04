#!/usr/bin/env bash
KNOB_HELP=""
CHECK_DIR="${CHECK_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)}"
DEF() { python3 -c "import json,sys;print(json.load(open(sys.argv[1]))[sys.argv[2]])" "$CHECK_DIR/ic/nominal/params.json" "$1"; }
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_LENGTH "$(DEF length)" "Pauli string length in qubits (default: ic params). Multiplication is bit-parallel over the length, so this is the workload knob"
knob SAB_PRODUCTS "$(DEF n_products)" "successive products taken (default: ic params). Linear in runtime"
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
      -Dpybind11_DIR="$(python3 -m pybind11 --cmakedir)" >"$WORK/cmake.log" 2>&1
cmake --build "$WORK/b" --target stim_python_bindings >>"$WORK/cmake.log" 2>&1
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"
MOD="$(find "$WORK/b" -name 'stim*.so' | head -1)"
[ -n "$MOD" ] || { cp "$WORK/cmake.log" "$OUT_DIR/cmake-failed.log" 2>/dev/null; echo "run.sh: stim python module not built; see cmake-failed.log" >&2; exit 1; }

# Record the vector word backend this build compiled. Stim's machine flags are
# guarded on CMAKE_SYSTEM_PROCESSOR (CMakeLists.txt:25) and its backends are
# x86-only, so the same source yields bitword_256_avx on an AVX2 host and the
# portable bitword_64 elsewhere. The incumbent is meaningless without it.
{ grep -m1 -oE "march=native|mavx2|msse2" "$WORK/cmake.log" || echo "no-machine-flag"; } > "$OUT_DIR/word_backend.txt"
uname -m >> "$OUT_DIR/word_backend.txt"

PARAMS="$CHECK_DIR/ic/$IC/params.json" OUT="$OUT_DIR" MOD="$MOD" python3 - <<'PYEOF'
import json, os, sys, importlib.util
import numpy as np
spec = importlib.util.spec_from_file_location("stim", os.environ["MOD"])
stim = importlib.util.module_from_spec(spec); spec.loader.exec_module(stim)
p = json.load(open(os.environ["PARAMS"])); out = os.environ["OUT"]
L = int(os.environ.get("SAB_LENGTH") or p["length"])
n = int(os.environ.get("SAB_PRODUCTS") or p["n_products"])

rng = np.random.default_rng(p["seed"])
def mk(L):
    # Build a Pauli string from a fixed RNG rather than stim.PauliString.random,
    # so the instance depends only on numpy semantics and not on stim internals
    # a port might reorder.
    return stim.PauliString("".join("_XYZ"[i] for i in rng.integers(0, 4, size=L)))

acc = mk(L)
for _ in range(n):
    acc = acc * mk(L)

# xs/zs are the bit arrays; sign carries the group law including the i phase, so
# grading it means the phase convention is checked and not just the support.
np.save(os.path.join(out, "acc_xs.npy"), np.asarray(acc.to_numpy()[0], dtype=np.uint8))
np.save(os.path.join(out, "acc_zs.npy"), np.asarray(acc.to_numpy()[1], dtype=np.uint8))
np.save(os.path.join(out, "acc_sign.npy"), np.array([complex(acc.sign).real, complex(acc.sign).imag], dtype=np.float64))
np.save(os.path.join(out, "acc_weight.npy"), np.array([acc.weight], dtype=np.int64))

print(f"length={L} products={n} sign={acc.sign} weight={acc.weight}")
PYEOF
