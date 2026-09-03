#!/usr/bin/env bash
#SBATCH --job-name=qh129-gang1008
#SBATCH --partition=xhacnormalb
#SBATCH --account=giggleliu
#SBATCH --nodes=18
#SBATCH --ntasks=72
#SBATCH --ntasks-per-node=4
#SBATCH --cpus-per-task=14
#SBATCH --mem=196G
#SBATCH --time=00:15:00
#SBATCH --output=/work/share/giggleliu/cfys01/quantum-harness-129/slurm-%j.out
#SBATCH --error=/work/share/giggleliu/cfys01/quantum-harness-129/slurm-%j.err

set -euo pipefail
unset LC_ALL
export LANG=C

: "${QH129_ORCHESTRATION:?submit with --export=ALL,QH129_ORCHESTRATION=/absolute/path}"

source /usr/share/Modules/init/bash
module purge
module load compiler/gcc/11.4.0

# One gang-scheduled allocation: 18 nodes × 4 processes/node × 14 CPUs/process.
# The scheduler cannot start a partial wave, so a running job proves that all
# 1,008 CPUs were allocated concurrently.
srun --kill-on-bad-exit=1 --cpu-bind=cores \
    bash "${QH129_ORCHESTRATION}/davidson-gang-worker.sh"
