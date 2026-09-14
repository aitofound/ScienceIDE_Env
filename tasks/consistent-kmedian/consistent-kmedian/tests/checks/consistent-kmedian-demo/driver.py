#!/usr/bin/env python3
"""Driver for check consistent-kmedian-demo.

Parametrizes the upstream demo (paper1_consistent_kmedian.py, __main__ block):
a synthetic 1-D metric (facilities/clients = point ids 0..N-1, positions
given explicitly by config["coords"]) fed point-by-point into
OnlineConsistentKMedian. The nominal coords are generated once with the
same construction and seed as the upstream demo (random.Random(0).uniform);
they are recorded literally in ic/nominal/config.json so that "coords" is
itself the initial condition a variant can perturb. Writes the graded trace
and final median set as text.

    python3 driver.py --source-dir SRC --config CONFIG.json --out OUT_DIR
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import sys
from pathlib import Path


def load_algorithm(source_dir: str):
    module_path = Path(source_dir) / "paper1_consistent_kmedian.py"
    spec = importlib.util.spec_from_file_location("paper1_consistent_kmedian", module_path)
    module = importlib.util.module_from_spec(spec)
    # dataclasses looks the module up via sys.modules[cls.__module__]; it must
    # be registered there before exec_module runs the @dataclass decorators.
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module.OnlineConsistentKMedian


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--source-dir", required=True)
    ap.add_argument("--config", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    OnlineConsistentKMedian = load_algorithm(a.source_dir)

    cfg = json.loads(Path(a.config).read_text(encoding="utf-8"))
    coords_list = [float(c) for c in cfg["coords"]]
    N = len(coords_list)
    k, z = int(cfg["k"]), int(cfg["z"])
    gamma, eps = float(cfg["gamma"]), float(cfg["eps"])

    coords = {i: coords_list[i] for i in range(N)}
    facilities = list(range(N))

    def dist(u, v):
        return abs(coords[u] - coords[v])

    algo = OnlineConsistentKMedian(facilities=facilities, dist=dist, k=k, z=z, gamma=gamma, eps=eps)

    checkpoints = {t for t in range(N) if t % 8 == 0 or t == N - 1}
    trace_rows = []
    for t in range(N):
        algo.add_point(t)
        if t in checkpoints:
            trace_rows.append((algo.total_recourse, algo.approx_cost(), algo.p))

    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    with (out / "trace.txt").open("w", encoding="utf-8") as f:
        for recourse, cost, p in trace_rows:
            f.write(f"{recourse:.17g} {cost:.17g} {p:.17g}\n")
    with (out / "medians.txt").open("w", encoding="utf-8") as f:
        for m in sorted(algo.current_solution()):
            f.write(f"{float(m):.17g}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
