"""Produce circle and constrained bank after the source build in run.sh."""
import argparse
import os
from pathlib import Path
import subprocess
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ic", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()
    check = Path(__file__).resolve().parent
    mode = args.ic.resolve().name
    if mode not in ("nominal", "variant"):
        raise ValueError("initial condition must be nominal or variant")
    subprocess.run([sys.executable, str(check/"circle/producer.py"), "--ic", str(args.ic),
                    "--out", str(args.out/"circle")], check=True)
    subprocess.run([sys.executable, str(check/"run_bank.py"), "--mode", mode,
                    "--out", str(args.out/"bank"), "--steps", os.environ.get("SAB_PANDA_BANK_STEPS", "96")], check=True)


if __name__ == "__main__":
    main()
