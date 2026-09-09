"""Native investigation only: preserve the circle frontend, vary the QP algorithm.

The alternative solves the same positive-definite QP with NumPy, retaining the
original DAQP as a fallback if an inequality becomes active. No source is edited.
Every unconstrained result is certified against the binary64 problem by primal
feasibility, gradient residual, and a strong-convexity objective-gap bound.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import sys
import time

import mink
import numpy as np


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repository", type=Path, required=True)
    ap.add_argument("--algorithm", choices=("baseline", "float32", "cg"), required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--residual", type=float, default=1e-7)
    ap.add_argument("--mode", default="nominal")
    args = ap.parse_args()
    root = args.repository.resolve()
    check = root / "tasks/mink/mink-constrained-differential-ik/tests/checks/examples-arm-panda"
    os.environ["SOURCE_DIR"] = str(root / "code/mink")
    os.environ["PYTHONDONTWRITEBYTECODE"] = "1"
    args.out.mkdir(parents=True, exist_ok=True)
    original = mink.solve_ik
    certificates = []
    fallback = 0

    def alternate(configuration, tasks, dt, solver, damping=1e-12,
                  safety_break=False, limits=None, constraints=None, **kwargs):
        nonlocal fallback
        configuration.check_limits(safety_break=safety_break)
        qp = mink.build_ik(configuration, tasks, dt, damping, limits, constraints)
        hessian, linear = qp.P, qp.q
        if args.algorithm == "float32":
            dq = np.linalg.solve(hessian.astype(np.float32), -linear.astype(np.float32)).astype(np.float64)
        else:
            # Standard conjugate-gradient solve, terminated by an absolute
            # gradient residual. This changes the numerical solver, not the QP.
            dq = np.zeros_like(linear)
            r = -linear.copy()
            p = r.copy()
            for _ in range(30):
                if np.max(np.abs(r)) <= args.residual:
                    break
                hp = hessian @ p
                rr = r @ r
                alpha = rr / (p @ hp)
                dq += alpha * p
                r -= alpha * hp
                p = r + (r @ r) / rr * p
        violation = float(np.max(qp.G @ dq - qp.h, initial=0.)) if qp.G is not None else 0.
        gradient = hessian @ dq + linear
        grad_max = float(np.max(np.abs(gradient)))
        smallest = float(np.linalg.eigvalsh(hessian)[0])
        if violation > 1e-12 or qp.A is not None or grad_max > 1.01e-7:
            fallback += 1
            return original(configuration, tasks, dt, solver, damping, safety_break, limits, constraints, **kwargs)
        certificates.append((violation, grad_max, float(gradient @ gradient / (2 * smallest))))
        return dq / dt

    if args.algorithm != "baseline":
        mink.solve_ik = alternate
    spec = importlib.util.spec_from_file_location("panda_circle_runner", check / "circle/producer.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    sys.argv = [str(check / "circle/producer.py"), "--ic", str(check / "ic" / args.mode), "--out", str(args.out)]
    start = time.perf_counter()
    module.main()
    evidence = {"algorithm": args.algorithm, "mode": args.mode, "elapsed_s": time.perf_counter()-start,
                "mink_file": mink.__file__, "independent_solves": len(certificates), "daqp_fallbacks": fallback,
                "cg_absolute_gradient_tolerance": args.residual if args.algorithm == "cg" else None,
                "maximum_primal_violation": max((x[0] for x in certificates), default=0.),
                "maximum_gradient_residual": max((x[1] for x in certificates), default=0.),
                "maximum_strong_convexity_objective_gap_bound": max((x[2] for x in certificates), default=0.)}
    (args.out / "qp-certification.json").write_text(json.dumps(evidence, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(evidence))


if __name__ == "__main__":
    main()
