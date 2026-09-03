#!/usr/bin/env bash
# Check bloch-redfield-eigenbasis-tools: the TEST half of the check.
#   run.sh nominal | run.sh variant     run one initial condition (see ic/)
#   run.sh --help                       list the runtime knobs below
# Environment supplied by the produce driver: SOURCE_DIR (read-only source tree),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.

KNOB_HELP=""
# CHECK_DIR is set by the produce driver; --help may be invoked with only the
# script's own location known, so fall back to it. ic/nominal/params.json stays
# the single source of truth for every graded default, and the knob defaults
# below are read from it rather than duplicated here.
CHECK_DIR="${CHECK_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)}"
DEF() { python3 -c "import json,sys;print(json.load(open(sys.argv[1]))[sys.argv[2]])" "$CHECK_DIR/ic/nominal/params.json" "$1"; }
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_N_QUBITS "$(DEF n_qubits)" "qubits in the Ising chain; Hilbert dimension is 2^n and the tensor is dimension^2 x dimension^2 (default: ic params). Tensor assembly runs over all eigenstate pairs, so cost grows as 16^n in the worst case and memory as 16^n; this is the workload knob and it bites hard"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant> | run.sh --help}"
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
[ -d "$CHECK_DIR/ic/$IC" ] || { echo "run.sh: no initial condition ic/$IC" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

# Upstream test this check reproduces: code/qutip/qutip/tests/core/test_brtools.py
BUILD_START=$(date +%s)
( cd "$WORK/src" && pip install --no-build-isolation --no-deps --quiet -e . )
echo "SAB_BUILD_SECONDS=$(( $(date +%s) - BUILD_START ))"

PARAMS="$CHECK_DIR/ic/$IC/params.json" OUT="$OUT_DIR" python3 - <<'PYEOF'
import json, os
import numpy as np
import qutip
from qutip.core.blochredfield import bloch_redfield_tensor

p = json.load(open(os.environ["PARAMS"])); out = os.environ["OUT"]
nq    = int(os.environ.get("SAB_N_QUBITS") or p["n_qubits"])
h     = float(p["field"]); J = float(p["coupling_j"]); kappa = float(p["kappa"])

def at(single, k):
    factors = [qutip.qeye(2)] * nq
    factors[k] = single
    return qutip.tensor(factors)

# Transverse-field Ising chain: H = -J sum sz_k sz_{k+1} - h sum sx_k
H = 0
for k in range(nq - 1):
    H = H - J * at(qutip.sigmaz(), k) * at(qutip.sigmaz(), k + 1)
for k in range(nq):
    H = H - h * at(qutip.sigmax(), k)

# Bath couples to the total transverse magnetisation.
Mx = sum(at(qutip.sigmax(), k) for k in range(nq))
a_ops = [(Mx, lambda w: kappa * (w >= 0))]

# fock_basis=True is REQUIRED for a gradable result and is not the default
# (blochredfield.py:32 defaults it to False). With fock_basis=False the tensor
# comes back in the Hamiltonian's eigenbasis, whose eigenvectors carry an
# arbitrary phase and an eigensolver-dependent ordering, so its matrix elements
# would differ between correct implementations - the same gauge freedom that
# makes raw Floquet modes ungradable. In the Fock basis the tensor is a fixed
# operator and its elements are well defined.
R = bloch_redfield_tensor(
    H, a_ops,
    sec_cutoff=float(p["sec_cutoff"]),
    fock_basis=bool(p["fock_basis"]),
    br_computation_method=str(p["br_computation_method"]),
    sparse_eigensolver=bool(p["sparse_eigensolver"]),
)

M = np.asarray(R.full(), dtype=np.complex128)
np.save(os.path.join(out, "br_tensor_real.npy"), np.ascontiguousarray(M.real, dtype=np.float64))
np.save(os.path.join(out, "br_tensor_imag.npy"), np.ascontiguousarray(M.imag, dtype=np.float64))

# The Hamiltonian spectrum the tensor was built from. Eigenvalues are
# gauge-invariant where eigenvectors are not, so this separates a port that
# broke the eigendecomposition from one that broke the tensor assembly.
np.save(os.path.join(out, "hamiltonian_eigenvalues.npy"),
        np.sort(np.real(H.eigenenergies())).astype(np.float64))

# Trace preservation: a Lindblad-form generator annihilates the identity on the
# left, so column sums of the superoperator vanish. A physical invariant of the
# tensor, independent of basis choice within the Fock basis.
dim = int(np.sqrt(M.shape[0]))
tr_gen = np.abs(M.reshape(dim, dim, dim, dim).trace(axis1=0, axis2=1)).max()
np.save(os.path.join(out, "trace_generator_norm.npy"), np.array([tr_gen], dtype=np.float64))

print(f"n_qubits={nq} dim={2**nq} tensor={M.shape} trace_gen_norm={tr_gen:.3e}")
PYEOF
