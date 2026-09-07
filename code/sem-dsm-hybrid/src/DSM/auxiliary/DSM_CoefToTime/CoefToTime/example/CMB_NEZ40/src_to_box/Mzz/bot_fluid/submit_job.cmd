#!/bin/bash

# Submit this script with: sbatch <this-filename>

#SBATCH --time=00:10:00   # walltime
#SBATCH --ntasks=18   # number of processor cores (i.e. tasks)
#####SBATCH --nodes=2   # number of nodes
#SBATCH --mem-per-cpu=2G   # memory per CPU core
#SBATCH -J "resave-M11"   # job name


## /SBATCH -p general # partition (queue)
## /SBATCH -o slurm.%N.%j.out # STDOUT
## /SBATCH -e slurm.%N.%j.err # STDERR

# LOAD MODULES, INSERT CODE, AND RUN YOUR PROGRAMS HERE
module load  intel/18.1 openmpi/3.1.2_intel-18.1
mpirun ./spectotime
