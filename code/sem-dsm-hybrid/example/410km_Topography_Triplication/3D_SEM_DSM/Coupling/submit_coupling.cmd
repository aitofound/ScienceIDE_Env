#!/bin/bash
# Submit this script with: sbatch submit_coupling.cmd
#SBATCH --time=04:50:00   # Walltime
#SBATCH -J "410km_coupling"   # Job name
#SBATCH --ntasks=512   # Number of processor cores
#SBATCH --mem-per-cpu=4G   # Memory per CPU core
#SBATCH --partition=scavenger   # Partition
#SBATCH --qos=scavenger         # Quality of service
module load 
mpirun ./coupling_integral
