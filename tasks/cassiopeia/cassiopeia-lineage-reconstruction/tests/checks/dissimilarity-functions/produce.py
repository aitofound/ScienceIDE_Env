#!/usr/bin/env python3
"""直接导出官方科学阶段的数值；不监听或记录 assertions。"""
import argparse
import json
from pathlib import Path
import time

import numba
import numpy as np
import pandas as pd

from cassiopeia.solver import dissimilarity_functions, solver_utilities


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    config = json.loads(args.inputs.read_text(encoding="utf-8"))
    if config["schema_version"] != 1:
        raise ValueError("unsupported IC schema")
    args.out.mkdir(parents=True, exist_ok=True)
    print(f"NUMBA_DISABLE_JIT={numba.config.DISABLE_JIT}", flush=True)
    start = time.monotonic()
    priors = {int(char): {int(state): probability for state, probability in states.items()}
              for char, states in config["priors"].items()}
    weights = {}
    for transformation in config["transformations"]:
        name = transformation["name"]
        transformed = solver_utilities.transform_priors(priors, name)
        weights[name] = transformed
        keys = [(char, state) for char, states in transformed.items() for state in states]
        values = np.array([transformed[char][state] for char, state in keys], dtype=np.float64)
        np.savez(args.out / transformation["output"], keys=np.asarray(keys, dtype=np.int64), values=values)
        print(f"transformation={name} values={len(values)}", flush=True)
    sequences = {}
    for name, states in config["sequences"].items():
        if any(isinstance(state, list) for state in states):
            sequences[name] = [tuple(state) if isinstance(state, list) else state for state in states]
        else:
            sequences[name] = np.asarray(states, dtype=np.int64)
    missing = config["missing_state_indicator"]
    keys, values = [], []
    for case in config["scalar_cases"]:
        first, second = case["operands"]
        s1, s2 = sequences[first], sequences[second]
        function = getattr(dissimilarity_functions, case["function"])
        case_weights = weights[case["weights"]] if case["weights"] is not None else None
        if case["function"] == "hamming_distance":
            value = function(s1, s2, ignore_missing_state=case["ignore_missing_state"],
                             missing_state_indicator=missing)
        elif case["function"] == "cluster_dissimilarity":
            if case["linkage"] != "mean" or case["base_function"] != "weighted_hamming_distance":
                raise ValueError("unsupported cluster scenario")
            value = function(dissimilarity_functions.weighted_hamming_distance, s1, s2,
                             missing, case_weights, np.mean, normalize=case["normalize"])
        else:
            value = function(s1, s2, missing, case_weights)
        keys.append([case["id"], first, second])
        values.append(value)
        print(f"case={case['id']} completed", flush=True)
    np.savez(args.out / "scores.npz", keys=np.asarray(keys, dtype=str),
             values=np.asarray(values, dtype=np.float64))
    phylip = config["phylip"]
    matrix = pd.DataFrame(phylip["matrix"], index=phylip["cell_ids"])
    solver_utilities.save_dissimilarity_as_phylip(matrix, str(args.out / phylip["output"]))
    print(f"phylip_cells={len(matrix)} scalar_cases={len(keys)} elapsed_seconds={time.monotonic()-start:.9f}", flush=True)


if __name__ == "__main__":
    main()
