#!/bin/bash
#SBATCH --time=01:00:00                # Walltime
#SBATCH --ntasks=10              # Number of processor cores
#SBATCH -J "DSMsyn_tosac"              # Job name
#SBATCH --output=slurm.%N.%j.out       # STDOUT file
#SBATCH --error=slurm.%N.%j.err        # STDERR file

module load intel openmpi/intel/4.0.1
mpirun ./spectotime < DATA/Par_file
