#!/usr/bin/env bash
KNOB_HELP=""
CHECK_DIR="${CHECK_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)}"
DEF() { python3 -c "import json,sys;print(json.load(open(sys.argv[1]))[sys.argv[2]])" "$CHECK_DIR/ic/nominal/params.json" "$1"; }
knob() { local name=$1 default=$2 desc=$3; [ -n "${!name:-}" ] || printf -v "$name" '%s' "$default"; export "$name"; KNOB_HELP+="$name=$default  $desc"$'\n'; }
knob SAB_MAX_DEPTH "$(DEF max_depth)" "hierarchy depth (default: ic params). The auxiliary-density-operator count grows combinatorially in depth and exponent count, so this is the dominant cost and the primary workload knob"
knob SAB_NK "$(DEF Nk)" "Matsubara terms in the bath expansion (default: ic params). Adds exponents, which also grows the hierarchy combinatorially"
knob SAB_N_TIMES "$(DEF n_times)" "graded time points (default: ic params). Near-linear in runtime"
# Alternative build, run.sh altbuild. The same pinned source and the same pinned wheels,
# with qutip's own Cython extensions compiled unoptimised and with FP contraction off.
# See the build block below for how the flags are applied; the inputs are ic/nominal.
ALTBUILD="qutip's Cython extensions compiled with -O0 -ffp-contract=off instead of the -O3 -funroll-loops that code/qutip/setup.py:118 hard-codes on every extension, from the same pinned source and the same pinned numpy/scipy/Cython wheels, with the nominal inputs unchanged"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; echo "SAB_QUTIP_INSTALL_ROOT=<absolute solve-scoped path>  optional shared QuTiP install root; default: current output root/.sab-qutip-install"; echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
INPUTS="$IC"
if [ "$IC" = altbuild ]; then INPUTS=nominal; fi
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
[ -d "$CHECK_DIR/ic/$INPUTS" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
cp -R "$SOURCE_DIR/." "$WORK/src"

# Upstream test this check reproduces: code/qutip/qutip/tests/solver/heom/test_bofin_solvers.py
# The alternative build lowers the optimisation level of qutip's own Cython extensions.
# setup.py appends -O3 -funroll-loops to every extension as extra_compile_args
# (code/qutip/setup.py:118) and setuptools puts extra_compile_args last on each compile
# line, so CFLAGS alone cannot lower it; CC and CXX therefore point at a wrapper that
# drops those two flags and appends -O0 -ffp-contract=off. Same source tree, same wheels,
# same pip command; only the compiler flags differ.
if [ "$IC" = altbuild ]; then
  mkdir -p "$WORK/bin"
  export SAB_ALTBUILD_LOG="$WORK/altbuild-compiles.log"
  for pair in cc:gcc cxx:g++; do
    real="$(command -v "${pair#*:}")" || true
    [ -n "$real" ] || { echo "run.sh: altbuild needs ${pair#*:} in the image" >&2; exit 2; }
    {
      echo '#!/usr/bin/env bash'
      echo '# altbuild compiler wrapper, written by run.sh: drop the hard-coded -O3 -funroll-loops'
      echo '# and compile the same sources unoptimised with FP contraction off.'
      echo 'args=(); for a in "$@"; do case "$a" in -O3|-funroll-loops) ;; *) args+=("$a") ;; esac; done'
      echo '[ -z "${SAB_ALTBUILD_LOG:-}" ] || printf "%s\n" "$*" >>"$SAB_ALTBUILD_LOG"'
      echo "exec $real \"\${args[@]}\" -O0 -ffp-contract=off"
    } >"$WORK/bin/sab-alt-${pair%%:*}"
    chmod +x "$WORK/bin/sab-alt-${pair%%:*}"
  done
  export CC="$WORK/bin/sab-alt-cc" CXX="$WORK/bin/sab-alt-cxx"
fi
# All seven checks have this exact install recipe.  By default they cooperate
# through a solve-scoped path beside their output directories; callers may pass
# an absolute SAB_QUTIP_INSTALL_ROOT to choose another solve-scoped location.
# The opt and altbuild paths are disjoint.  A verified hit imports the compiled
# package from the shared source and reports zero build seconds.  Any absent,
# incomplete, stale, unwritable, or unusable shared entry takes this check's
# original full private build path instead, so run.sh remains self-contained.
BUILD_MODE=opt
[ "$IC" != altbuild ] || BUILD_MODE=altbuild-O0-no-contract
BUILD_FINGERPRINT="$(python3 -B - "$WORK/src" "$BUILD_MODE" <<'PYBUILD'
import hashlib, os, platform, stat, subprocess, sys
root, mode = sys.argv[1:]
h = hashlib.sha256()
for value in (
    "qutip-shared-install-v1",
    mode,
    "pip install --no-build-isolation --no-deps --quiet -e .",
    platform.machine(),
):
    h.update(value.encode("utf-8") + b"\0")
for command in (("python3", "--version"), ("pip", "--version"),
                ("gcc", "--version"), ("g++", "--version")):
    result = subprocess.run(command, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, check=True).stdout
    h.update(b"command\0" + b"\0".join(x.encode() for x in command) + b"\0" + result + b"\0")
entries = []
for dirpath, dirnames, filenames in os.walk(root, followlinks=False):
    dirnames.sort()
    filenames.sort()
    entries.extend(os.path.join(dirpath, name) for name in dirnames + filenames)
for path in sorted(entries, key=lambda p: os.path.relpath(p, root)):
    rel = os.path.relpath(path, root).encode("utf-8")
    st = os.lstat(path)
    h.update(rel + b"\0" + str(stat.S_IMODE(st.st_mode)).encode() + b"\0")
    if stat.S_ISLNK(st.st_mode):
        h.update(b"link\0" + os.readlink(path).encode("utf-8") + b"\0")
    elif stat.S_ISREG(st.st_mode):
        h.update(b"file\0")
        with open(path, "rb") as stream:
            for chunk in iter(lambda: stream.read(1 << 20), b""):
                h.update(chunk)
        h.update(b"\0")
    elif stat.S_ISDIR(st.st_mode):
        h.update(b"dir\0")
    else:
        h.update(b"other\0" + str(st.st_mode).encode() + b"\0")
print(h.hexdigest())
PYBUILD
)"

DEFAULT_INSTALL_ROOT="$(dirname "$OUT_DIR")/.sab-qutip-install"
INSTALL_ROOT="${SAB_QUTIP_INSTALL_ROOT:-$DEFAULT_INSTALL_ROOT}"
SHARED=""; RUN_SOURCE=""; BUILD_SECONDS=0; CACHE_HIT=0
case "$INSTALL_ROOT" in
  /*) SHARED="$INSTALL_ROOT/$BUILD_MODE-$BUILD_FINGERPRINT" ;;
  *) echo "run.sh: SAB_QUTIP_INSTALL_ROOT must be absolute; building privately" >&2 ;;
esac

shared_import_ok() {
  PYTHONPATH="$1" python3 -B - "$1" <<'PYIMPORT'
import os, sys
root = os.path.realpath(sys.argv[1])
import qutip
module = os.path.realpath(qutip.__file__)
if os.path.commonpath((root, module)) != root:
    raise SystemExit(f"qutip imported from {module}, outside shared source {root}")
PYIMPORT
}

if [ -n "$SHARED" ] && [ -f "$SHARED/BUILD_OK.sha256" ] \
   && [ "$(cat "$SHARED/BUILD_OK.sha256" 2>/dev/null || true)" = "$BUILD_FINGERPRINT" ] \
   && [ -d "$SHARED/src/qutip" ]; then
  if [ "$IC" != altbuild ] \
     || [ "$(cat "$SHARED/ALTBUILD_COMPILE_COMMANDS" 2>/dev/null || echo 0)" -gt 0 ]; then
    if shared_import_ok "$SHARED/src" >/dev/null 2>&1; then
      CACHE_HIT=1; RUN_SOURCE="$SHARED/src"
    fi
  fi
fi

if [ "$CACHE_HIT" -eq 1 ]; then
  echo "SAB_QUTIP_INSTALL=hit mode=$BUILD_MODE fingerprint=$BUILD_FINGERPRINT"
else
  echo "SAB_QUTIP_INSTALL=miss mode=$BUILD_MODE fingerprint=$BUILD_FINGERPRINT"
  # Only the process that creates the mode+fingerprint directory publishes it.
  # A pre-existing incomplete/mismatched directory is preserved and ignored;
  # this check then performs the independent private fallback below.
  if [ -n "$SHARED" ] && mkdir -p "$INSTALL_ROOT" 2>/dev/null && mkdir "$SHARED" 2>/dev/null; then
    SHARED_START=$(date +%s)
    SHARED_OK=0
    if mkdir -p "$SHARED/src" \
       && cp -R "$WORK/src/." "$SHARED/src" \
       && ( cd "$SHARED/src" && pip install --no-build-isolation --no-deps --quiet -e . ); then
      SHARED_OK=1
      if [ "$IC" = altbuild ]; then
        compiles=$(wc -l <"$SAB_ALTBUILD_LOG" 2>/dev/null || echo 0)
        if [ "$compiles" -gt 0 ]; then
          printf '%s\n' "$compiles" >"$SHARED/ALTBUILD_COMPILE_COMMANDS"
        else
          echo "run.sh: shared altbuild compiler wrapper was never invoked; falling back to a private build" >&2
          SHARED_OK=0
        fi
      fi
      if [ "$SHARED_OK" -eq 1 ] && ! shared_import_ok "$SHARED/src" >/dev/null 2>&1; then
        echo "run.sh: shared QuTiP install is not importable from its documented path; building privately" >&2
        SHARED_OK=0
      fi
    else
      echo "run.sh: shared QuTiP install failed; building privately" >&2
    fi
    BUILD_SECONDS=$(( BUILD_SECONDS + $(date +%s) - SHARED_START ))
    if [ "$SHARED_OK" -eq 1 ]; then
      printf '%s\n' "$BUILD_FINGERPRINT" >"$SHARED/BUILD_OK.sha256"
      RUN_SOURCE="$SHARED/src"
    fi
  fi
fi

if [ -z "$RUN_SOURCE" ]; then
  PRIVATE_START=$(date +%s)
  ( cd "$WORK/src" && pip install --no-build-isolation --no-deps --quiet -e . )
  BUILD_SECONDS=$(( BUILD_SECONDS + $(date +%s) - PRIVATE_START ))
  RUN_SOURCE="$WORK/src"
  if [ "$IC" = altbuild ]; then
    compiles=$(wc -l <"$SAB_ALTBUILD_LOG" 2>/dev/null || echo 0)
    [ "$compiles" -gt 0 ] || { echo "run.sh: the altbuild compiler wrapper was never invoked, so the alternative build did not take effect" >&2; exit 1; }
  fi
fi
if [ "$CACHE_HIT" -eq 0 ] && [ "$BUILD_SECONDS" -eq 0 ]; then BUILD_SECONDS=1; fi
export PYTHONPATH="$RUN_SOURCE${PYTHONPATH:+:$PYTHONPATH}"
echo "SAB_BUILD_SECONDS=$BUILD_SECONDS"
if [ "$IC" = altbuild ]; then
  compiles=$(cat "$SHARED/ALTBUILD_COMPILE_COMMANDS" 2>/dev/null || wc -l <"$SAB_ALTBUILD_LOG" 2>/dev/null || echo 0)
  echo "SAB_ALTBUILD_COMPILE_COMMANDS=$compiles"
fi

export OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=2 MKL_NUM_THREADS=2 NUMEXPR_NUM_THREADS=2
PARAMS="$CHECK_DIR/ic/$INPUTS/params.json" OUT="$OUT_DIR" python3 - <<'PYEOF'
import json, os
import numpy as np
import qutip
from qutip.solver.heom import HEOMSolver, DrudeLorentzBath

p = json.load(open(os.environ["PARAMS"])); out = os.environ["OUT"]
depth = int(os.environ.get("SAB_MAX_DEPTH") or p["max_depth"])
Nk    = int(os.environ.get("SAB_NK") or p["Nk"])
nt    = int(os.environ.get("SAB_N_TIMES") or p["n_times"])
lam, gamma, T, tf = float(p["lam"]), float(p["gamma"]), float(p["T"]), float(p["t_final"])
integ = p["integrator"]

# Upstream's DrudeLorentzPureDephasingModel verbatim. The 1e-5 Hamiltonian is
# not decoration: upstream notes a singular system breaks the SuperLU solve
# that HEOM's spsolve relies on, so the weak-but-nonzero H is load-bearing.
H   = qutip.Qobj(1e-5 * np.ones((2, 2)))
Q   = qutip.sigmaz()
rho0 = 0.5 * qutip.Qobj(np.ones((2, 2)))
bath = DrudeLorentzBath(Q, lam=lam, gamma=gamma, T=T, Nk=Nk)

opts = {"atol": float(integ["atol"]), "rtol": float(integ["rtol"]),
        "nsteps": int(integ["nsteps"]), "progress_bar": ""}
solver = HEOMSolver(H, bath, max_depth=depth, options=opts)
tlist = np.linspace(0, tf, nt)
res = solver.run(rho0, tlist)

# Graded: the coherence, which is what upstream compares against its analytic
# pure-dephasing solution. NOT the raw auxiliary-density-operator vector: the
# ADOs are an implementation-ordered list and a port may legitimately reorder
# them, so only the physical system state is gradable.
proj = qutip.basis(2,0) * qutip.basis(2,1).dag()
coh = np.asarray(qutip.expect(proj, res.states), dtype=np.complex128)
np.save(os.path.join(out, "coherence_real.npy"), np.ascontiguousarray(coh.real, dtype=np.float64))
np.save(os.path.join(out, "coherence_imag.npy"), np.ascontiguousarray(coh.imag, dtype=np.float64))

# Populations: trace-preserving by construction, so their sum is the
# normalisation gate for the whole hierarchy.
pops = np.array([np.real(np.diag(s.full())) for s in res.states], dtype=np.float64)
np.save(os.path.join(out, "populations.npy"), pops.ravel())
np.save(os.path.join(out, "trace.npy"), pops.sum(axis=1))

print(f"depth={depth} Nk={Nk} ADOs={len(solver.ados.labels)} times={nt} "
      f"coh0={coh[0].real:.12f} cohN={coh[-1].real:.12f}")
PYEOF
