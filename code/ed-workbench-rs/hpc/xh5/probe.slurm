#!/bin/bash
#SBATCH --job-name=ed-ccpvdz-probe
#SBATCH --partition=xhacnormalb
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=64
#SBATCH --mem=128G
#SBATCH --time=1-00:00:00
#SBATCH --output=/work/home/qbics2622/ed-workbench-runs/probe-%j.out
#SBATCH --error=/work/home/qbics2622/ed-workbench-runs/probe-%j.err

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

set +e
/usr/bin/time -v srun \
  --ntasks=1 \
  --cpus-per-task="$SLURM_CPUS_PER_TASK" \
  --cpu-bind=cores \
  "$binary" davidson "$fcidump" \
  --residual-tolerance 1e-7 \
  --max-iterations 1 \
  --max-subspace 6 \
  --memory-budget-gib 96 \
  --parallel-blocks 4 \
  --parallel-memory-budget-gib 16 \
  --strict-parallel-memory
status=$?
set -e

echo "probe_exit_status=$status"
echo "finished=$(date --iso-8601=seconds)"

# A one-iteration Davidson probe is expected not to converge.
if [[ "$status" -ne 0 && "$status" -ne 1 ]]; then
  exit "$status"
fi
