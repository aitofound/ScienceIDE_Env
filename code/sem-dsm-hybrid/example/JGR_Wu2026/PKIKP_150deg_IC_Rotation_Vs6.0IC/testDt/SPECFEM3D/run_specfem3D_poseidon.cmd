#!/bin/bash
#SBATCH --time=10:10:00   # walltime
#SBATCH --ntasks=1024   # number of processor cores (i.e. tasks)
####You may need to specify a larger memory size than default if database is very large
#SBATCH --mem-per-cpu=4G   # memory per CPU core
#SBATCH -J "CMB_mesh"   # job name
## /SBATCH -p general # partition (queue)
## /SBATCH -o slurm.%N.%j.out # STDOUT
## /SBATCH -e slurm.%N.%j.err # STDERR
#SBATCH --partition=scavenger
#SBATCH --qos=scavenger
# LOAD MODULES, INSERT CODE, AND RUN YOUR PROGRAMS HERE
 
module load intel openmpi/intel/4.0.1
mpirun ../../../../../src/SPECFEM3D/bin//xspecfem3D
