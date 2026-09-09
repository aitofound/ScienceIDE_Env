#!/bin/bash
# Submit this script with: sbatch submit_coupling.cmd
#SBATCH --time=04:50:00   # Walltime
#SBATCH -J "PKiKP_3Hz_coupling"   # Job name
#SBATCH --ntasks=256   # Number of processor cores
#SBATCH --mem-per-cpu=4G   # Memory per CPU core
##SBATCH --partition=scavenger   # Partition
##SBATCH --qos=scavenger         # Quality of service
module load 
mpirun ./coupling_integral
