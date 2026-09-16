#!/usr/bin/env python3
"""运行真实UMI纠错pipeline，只提取原生产表；不在adapter计算分簇答案。"""
import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import pandas as pd

from cassiopeia.preprocess import pipeline
from cassiopeia.preprocess import collapse_cython


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--threads", type=int, default=2)
    args = parser.parse_args()
    if args.threads < 1:
        parser.error("threads must be positive")
    config = json.loads(args.inputs.read_text(encoding="utf-8"))
    if config["schema_version"] != 1:
        raise ValueError("unsupported IC schema")
    args.out.mkdir(parents=True, exist_ok=True)
    print("core_extension=" + collapse_cython.__file__, flush=True)
    print("core_sha256=" + hashlib.sha256(Path(collapse_cython.__file__).read_bytes()).hexdigest(), flush=True)
    for scenario in config["scenarios"]:
        frame = pd.DataFrame(config["fixtures"][scenario["fixture"]])
        start = time.monotonic()
        result = pipeline.error_correct_umis(
            frame, max_umi_distance=scenario["max_umi_distance"],
            allow_allele_conflicts=scenario["allow_allele_conflicts"],
            n_threads=args.threads if scenario["n_threads"] > 1 else 1,
        )
        molecules = result[["cellBC", "intBC", "UMI"]].to_numpy()
        alleles = result[["allele", "r1", "r2", "r3"]].to_numpy()
        if not all(isinstance(value, str) for values in [molecules, alleles] for value in values.flat):
            raise TypeError("production molecule/allele identities must be strings")
        counts = result["readCount"].tolist()
        if not all(isinstance(value, (int, np.integer)) and not isinstance(value, (bool, np.bool_))
                   and 0 <= value <= np.iinfo(np.int64).max for value in counts):
            raise TypeError("production readCount must contain exact nonnegative integers in the int64 domain")
        np.savez(args.out / (scenario["id"] + ".npz"),
                 molecules=np.asarray(molecules, dtype=str), alleles=np.asarray(alleles, dtype=str),
                 read_count=np.asarray(counts, dtype=np.int64))
        print(f"scenario={scenario['id']} output_rows={len(result)} elapsed_seconds={time.monotonic()-start:.9f}", flush=True)


if __name__ == "__main__":
    main()
