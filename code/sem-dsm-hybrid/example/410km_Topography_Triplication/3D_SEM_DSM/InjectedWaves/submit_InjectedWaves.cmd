#!/bin/bash
#SBATCH --time=05:00:00
#SBATCH --ntasks=262
#SBATCH --mem-per-cpu=8G
#SBATCH -J 410km_Mij_Inject
#SBATCH -o OUTPUT_FILES/slurm-%j.out
#SBATCH -e OUTPUT_FILES/slurm-%j.out

set -e
module load 
mpirun ./extract_Injectedwaves
