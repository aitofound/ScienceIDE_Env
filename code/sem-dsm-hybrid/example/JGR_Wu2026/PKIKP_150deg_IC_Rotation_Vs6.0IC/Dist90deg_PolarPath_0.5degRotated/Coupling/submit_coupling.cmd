#!/bin/bash
# Submit this script with: sbatch submit_coupling.cmd
#SBATCH --time=06:50:00   # Walltime
#SBATCH -J "PKIKP_coupling"   # Job name
#SBATCH --ntasks=110   # Number of processor cores
#SBATCH --mem-per-cpu=4G   # Memory per CPU core
#SBATCH --partition=mazu   # Partition
#SBATCH --qos=mazu         # Quality of service

mpirun -np "${SLURM_NTASKS}" --bind-to none ./coupling_integral
