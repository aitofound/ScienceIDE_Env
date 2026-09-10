#!/bin/bash
#SBATCH --time=05:00:00   # Walltime
#SBATCH --ntasks=262   # Number of processors
#SBATCH --mem-per-cpu=8G   # Memory per CPU core
#SBATCH -J "PKP66PKP_Inject"   # Job name
#SBATCH --partition=scavenger   # Partition
#SBATCH --qos=scavenger         # QoS
#SBATCH --output=slurm.%N.%j.out   # STDOUT
#SBATCH --error=slurm.%N.%j.err    # STDERR

module load 
mpirun ./extract_Injectedwaves
