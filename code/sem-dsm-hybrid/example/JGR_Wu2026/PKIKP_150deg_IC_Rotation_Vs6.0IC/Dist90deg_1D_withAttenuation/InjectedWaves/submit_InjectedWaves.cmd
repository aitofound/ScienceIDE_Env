#!/bin/bash
#SBATCH --time=05:00:00   # Walltime
#SBATCH --ntasks=260   # Number of processors
#SBATCH --mem-per-cpu=12G   # Memory per CPU core
#SBATCH -J "PKIKP_Inject"   # Job name
#SBATCH --partition=scavenger   # Partition
#SBATCH --qos=scavenger         # QoS
#SBATCH --output=slurm.%N.%j.out   # STDOUT
#SBATCH --error=slurm.%N.%j.err    # STDERR

module load intel openmpi/intel/4.0.1
mpirun ./extract_Injectedwaves
