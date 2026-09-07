#!/bin/bash
#SBATCH --time=05:00:00
#SBATCH --ntasks=262
#SBATCH --mem-per-cpu=8G
#SBATCH --partition=scavenger
#SBATCH --qos=scavenger
#SBATCH -J 410km_Mtt_Inject
#SBATCH -o OUTPUT_FILES/Mtt/slurm-%j.out
#SBATCH -e OUTPUT_FILES/Mtt/slurm-%j.out

set -e
if [[ -n "" ]]; then
  module load 
fi
mpirun ./extract_Injectedwaves_Mtt
