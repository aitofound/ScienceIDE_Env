#!/bin/bash
#SBATCH --job-name=ed-ccpvdz-smoke
#SBATCH --partition=xhacnormalb
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=2G
#SBATCH --time=00:10:00
#SBATCH --output=/work/home/qbics2622/ed-workbench-runs/smoke-%j.out
#SBATCH --error=/work/home/qbics2622/ed-workbench-runs/smoke-%j.err

set -euo pipefail
export LC_ALL=C
export LANG=C

home=/work/home/qbics2622
binary="$home/ed-workbench/target/release/ed_workbench_rs"
fcidump="$home/H2O_CCPVDZ_C2V.FCIDUMP"

echo "job_id=$SLURM_JOB_ID"
echo "node=$(hostname)"
echo "started=$(date --iso-8601=seconds)"
"$binary" inspect "$fcidump"
echo "finished=$(date --iso-8601=seconds)"
