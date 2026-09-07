#!/bin/bash
# Submit this script with: sbatch submit_coupling.cmd
#SBATCH --time=06:50:00   # Walltime
#SBATCH -J "PKP_2Hz_coupling"   # Job name
#SBATCH --ntasks=760   # Number of processor cores
#SBATCH --mem-per-cpu=4G   # Memory per CPU core
#SBATCH --partition=scavenger   # Partition
#SBATCH --qos=scavenger         # Quality of service
module load 
mpirun ./coupling_integral
