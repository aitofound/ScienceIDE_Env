#!/bin/bash
#SBATCH --job-name=ed-ccpvdz-a1
#SBATCH --partition=xhacnormalb
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=64
#SBATCH --mem=384G
#SBATCH --time=11:30:00
#SBATCH --exclusive
#SBATCH --output=/work/home/qbics2622/ed-workbench-runs/production-%j.out
#SBATCH --error=/work/home/qbics2622/ed-workbench-runs/production-%j.err

set -euo pipefail
export LC_ALL=C
export LANG=C
export RAYON_NUM_THREADS="$SLURM_CPUS_PER_TASK"
export OMP_NUM_THREADS=1
export OPENBLAS_NUM_THREADS=1
ulimit -s unlimited

home=/work/home/qbics2622
binary="$home/ed-workbench/target/release/ed_workbench_rs"
fcidump="$home/H2O_CCPVDZ_C2V.FCIDUMP"

echo "job_id=$SLURM_JOB_ID"
echo "node=$(hostname)"
echo "started=$(date --iso-8601=seconds)"
echo "rayon_threads=$RAYON_NUM_THREADS"

/usr/bin/time -v srun \
  --ntasks=1 \
  --cpus-per-task="$SLURM_CPUS_PER_TASK" \
  --cpu-bind=cores \
  "$binary" davidson "$fcidump" \
  --residual-tolerance 1e-7 \
  --max-iterations 60 \
  --max-subspace 6 \
  --memory-budget-gib 96 \
  --parallel-blocks 64 \
  --parallel-memory-budget-gib 256 \
  --strict-parallel-memory

echo "finished=$(date --iso-8601=seconds)"
