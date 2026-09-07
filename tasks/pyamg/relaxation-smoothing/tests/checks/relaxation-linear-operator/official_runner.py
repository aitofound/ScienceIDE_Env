#!/usr/bin/env python3
import argparse
from pathlib import Path

import numpy as np
import pytest


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--test", required=True)
    parser.add_argument("--node", required=True)
    parser.add_argument("--seed", required=True, type=int)
    parser.add_argument("--basetemp", required=True)
    args = parser.parse_args()
    np.random.seed(args.seed)
    return pytest.main(["-q", f"{args.test}::{args.node}", "--basetemp", args.basetemp])


if __name__ == "__main__":
    raise SystemExit(main())
