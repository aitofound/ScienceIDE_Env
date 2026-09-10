#!/bin/bash
#SBATCH --time=23:59:00   # walltime
#SBATCH --ntasks=256   # number of processor cores
#SBATCH --mem-per-cpu=4G   # memory per CPU core
#SBATCH -J "DSM_PKP"   # job name
##SBATCH --partition=scavenger
##SBATCH --qos=scavenger
###module load 
mpirun ./dsmti <dsm_model
