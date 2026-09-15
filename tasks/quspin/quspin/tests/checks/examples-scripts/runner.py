#!/usr/bin/env python3
"""Run the upstream QuSpin official example scripts owned by this check.

Upstream treats its example decks as official tests: `run_all_tests.sh` runs
this suite through `examples/scripts/run_examples.sh`, which executes every
`example*.py` and counts a non-zero exit status as a failure.  The examples
have no reference output, so the check keeps upstream's own pass condition
(the example must run to completion) and adds one small deterministic physical
observable, measured by the production path, as the graded calibration.
"""

from __future__ import annotations

import json
import os
import signal
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

# The official glob in examples/scripts/run_examples.sh is exactly `example*.py`.
# examples/scripts/outdated/ is a parked directory upstream never runs.
FILES = [
    "example0.py", "example00.py", "example1.py", "example1_original.py",
    "example2.py", "example3.py", "example4.py", "example5.py", "example6.py",
    "example7.py", "example8.py", "example9.py", "example10.py", "example11.py",
    "example12.py", "example13.py", "example14.py", "example15.py", "example16.py",
    "example17.py", "example18.py", "example19.py", "example20.py", "example21.py",
    "example22.py", "example23.py", "example24.py", "example25.py", "example26.py",
    "example27.py", "example28.py",
    # Three more official examples live in this directory without matching the
    # `example*.py` glob.  They are documented as examples in
    # sphinx/source/examples/user-basis_example{0,1,2}.rst, which embeds each
    # file with `literalinclude` and offers it as a download, so they are part
    # of the published example set even though run_examples.sh skips them.
    "user_basis_trivial-spin.py",
    "user_basis_trivial-spinless_fermion.py",
    "user_basis_trivial-boson.py",
]

# example12.py indexes sys.argv[1] and [2]; upstream run_examples.sh passes the
# OMP thread count twice for exactly that reason.
ARGV_NEEDS_TWO = {"example12.py": ["2", "2"]}

# example27.py drives its solver through the optional sparse_dot_mkl
# accelerator, which needs a system MKL runtime the image does not carry;
# example11.py performs a 2D
# exact-diagonalisation sweep that does not finish inside the check window on
# the declared cores.  Both are recorded in comment/coverage-matrix.md rather
# than silently skipped.
# example1.py, example1_original.py and example2.py each run a long adaptive
# ramp sweep (example1.py defaults to n_real=100 disorder realisations) and are
# the three that dominate this check's wall time; they still run, with the cap
# below applied.
EXCLUDED = {
    "example11.py": "2D sweep exceeds the check window on the declared cores",
    "example27.py": "requires a system MKL runtime for sparse_dot_mkl",
}

# A hung or pathologically slow example must fail this check, not hang the whole
# suite.  subprocess.run(timeout=) kills only the direct child: several examples
# spawn joblib workers, and an orphaned worker keeps the CPU and outlives the
# run.  Start each example in its own process group and kill the whole group.
PER_EXAMPLE_TIMEOUT_S = int(os.environ.get("SAB_EXAMPLE_TIMEOUT_S", "600"))


def run_example(filename: str, workdir: Path, env: dict, timeout: int):
    """Run one example in its own process group, killing the group on timeout."""
    command = [sys.executable, filename] + ARGV_NEEDS_TWO.get(filename, [])
    proc = subprocess.Popen(command, cwd=workdir, env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            start_new_session=True)
    try:
        out, err = proc.communicate(timeout=timeout)
        return proc.returncode, out, err
    except subprocess.TimeoutExpired:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except ProcessLookupError:
            pass
        out, err = proc.communicate()
        return (f"TIMEOUT after {timeout}s", out, err)


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit("usage: runner.py <group> <source> <out> <config>")
    group, source, out, config_path = sys.argv[1:]
    config = json.loads(Path(config_path).read_text())
    workdir = Path(source) / "examples" / "scripts"
    env = os.environ.copy()
    env.update({"PYTHONDONTWRITEBYTECODE": "1", "OMP_NUM_THREADS": "1",
                "OPENBLAS_NUM_THREADS": "1", "KMP_DUPLICATE_LIB_OK": "TRUE"})

    def run_one(filename):
        rc, out, err = run_example(filename, workdir, env, PER_EXAMPLE_TIMEOUT_S)
        return filename, rc, out, err

    ran = [f for f in FILES if f not in EXCLUDED]
    with ThreadPoolExecutor(max_workers=min(2, len(ran))) as pool:
        results = list(pool.map(run_one, ran))
    failures = [(f, rc, out, err) for f, rc, out, err in results if rc != 0]
    if failures:
        for filename, rc, out, err in failures:
            sys.stderr.write(f"[{filename}] rc={rc}\n{out[-2000:]}{err[-2000:]}\n")
        return 1

    # A compact physical calibration observable on the same production path the
    # examples drive: the finite spin-chain ground energy at the configured
    # coupling.  See the sibling grouped checks for why the variant perturbs J
    # (which enters the off-diagonal elements) and not the longitudinal field
    # (which is exactly a constant times the identity in a fixed-magnetization
    # sector, so it cannot calibrate a spectrum observable).
    import numpy as np
    from quspin.basis import spin_basis_1d
    from quspin.operators import hamiltonian

    L = int(config.get("L", 8))
    h = float(config.get("h", 0.5))
    J = float(config.get("J", 1.0))
    basis = spin_basis_1d(L, Nup=L // 2)
    bonds = [[J, i, i + 1] for i in range(L - 1)]
    field = [[h, i] for i in range(L)]
    H = hamiltonian([["xx", bonds], ["yy", bonds], ["zz", bonds], ["z", field]], [],
                    basis=basis, dtype=np.float64)
    ground = float(H.eigsh(k=1, which="SA", return_eigenvectors=False)[0])
    Path(out).write_text(json.dumps({
        "group": group,
        "upstream_examples": ran,
        "upstream_passed": len(ran),
        "upstream_excluded": sorted(EXCLUDED),
        "ground_energy": ground,
        "L": L,
        "h": h,
        "J": J,
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
