#!/bin/bash
#SBATCH --time=06:00:00   # Walltime
#SBATCH --ntasks=232   # Number of processors
#SBATCH --mem-per-cpu=10G   # Memory per CPU core
#SBATCH -J "PKIKP_Inject"   # Job name
#SBATCH --partition=scavenger   # Partition
#SBATCH --qos=scavenger         # QoS
#SBATCH --output=slurm.%N.%j.out   # STDOUT
#SBATCH --error=slurm.%N.%j.err    # STDERR

module load 
mpirun ./extract_Injectedwaves
