#!/usr/bin/env python3
"""直接运行原序列选择/过滤API，导出真正保留的seq与所选行support。"""
import argparse
import json
from pathlib import Path
import tempfile
import time

import numpy as np
import pandas as pd

from cassiopeia.preprocess import pipeline


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    config = json.loads(args.inputs.read_text(encoding="utf-8"))
    if config["schema_version"] != 1:
        raise ValueError("unsupported IC schema")
    args.out.mkdir(parents=True, exist_ok=True)
    for scenario in config["scenarios"]:
        frame = pd.DataFrame(config["fixtures"][scenario["fixture"]])
        start = time.monotonic()
        with tempfile.TemporaryDirectory(prefix="resolve-diagnostics-", dir=Path.cwd()) as temporary:
            result = pipeline.resolve_umi_sequence(
                frame, temporary, min_umi_per_cell=scenario["min_umi_per_cell"],
                min_avg_reads_per_umi=scenario["min_avg_reads_per_umi"], plot=scenario["plot"],
            )
            plot_count = len(list(Path(temporary).glob("*.png")))
            molecules = result[["cellBC", "UMI"]].to_numpy()
            sequences = result["seq"].tolist()
            if not all(isinstance(value, str) for value in list(molecules.flat) + sequences):
                raise TypeError("production identities and DNA sequences must be strings")
            counts = result["readCount"].tolist()
            if not all(isinstance(value, (int, np.integer)) and not isinstance(value, (bool, np.bool_))
                       and 0 <= value <= np.iinfo(np.int64).max for value in counts):
                raise TypeError("selected-row readCount must be exact nonnegative integers in the int64 domain")
            np.savez(args.out / (scenario["id"] + ".npz"), molecules=np.asarray(molecules, dtype=str),
                     sequences=np.asarray(sequences, dtype=str), read_count=np.asarray(counts, dtype=np.int64))
        print(f"scenario={scenario['id']} retained_molecules={len(result)} diagnostic_pngs={plot_count} "
              f"elapsed_seconds={time.monotonic()-start:.9f}", flush=True)


if __name__ == "__main__":
    main()
