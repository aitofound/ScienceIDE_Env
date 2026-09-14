#!/usr/bin/env python3
"""Driver for check db-gst-t-rounding-demo: Theorem 1.2 (Section 6).

Parametrizes the upstream demo's part (B) (paper2_degree_bounded_steiner.py,
__main__ block, lines 349-379): rebuilds the exact 30-node tree instance
(parent map, per-vertex cost, per-vertex degree bound, 5 leaf groups) from
ic/<ic>/config.json -- recorded literally, the same values the upstream
random.seed(3) construction produces, so the instance itself is the initial
condition a variant can perturb -- and runs DBGSTSolver.solve() on it, the
full LP-relaxation / power-of-2 rounding / rescaling / recursive-rounding
pipeline of Section 6. Vertex ids ("r", "n1", ...) are integer-coded
("r" -> 0, "nN" -> N) so every graded file is plain numeric text.

    python3 driver.py --source-dir SRC --config CONFIG.json --out OUT_DIR
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import sys
from pathlib import Path


def load_module(source_dir: str):
    module_path = Path(source_dir) / "paper2_degree_bounded_steiner.py"
    spec = importlib.util.spec_from_file_location("paper2_degree_bounded_steiner", module_path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def vid(name: str) -> int:
    return 0 if name == "r" else int(name[1:])


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--source-dir", required=True)
    ap.add_argument("--config", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    m = load_module(a.source_dir)
    cfg = json.loads(Path(a.config).read_text(encoding="utf-8"))

    parent = cfg["parent"]
    cost = {k: float(v) for k, v in cfg["cost"].items()}
    degree_bound = {k: float(v) for k, v in cfg["degree_bound"].items()}
    groups = cfg["groups"]

    inst = m.DBGSTInstance(parent=parent, root=cfg["root"], cost=cost, degree_bound=degree_bound, groups=groups)
    solver = m.DBGSTSolver(inst, eps_M_boost=int(cfg["eps_M_boost"]), seed=int(cfg["solver_seed"]))
    result = solver.solve()

    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    with (out / "metrics.txt").open("w", encoding="utf-8") as f:
        f.write(
            f"{result['cost']!r} {result['lp_value']!r} {result['max_degree_violation_factor']!r} "
            f"{result['groups_covered']} {result['groups_total']}\n"
        )
    with (out / "chosen_set.txt").open("w", encoding="utf-8") as f:
        for u in sorted(result["V"], key=vid):
            f.write(f"{vid(u)}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
