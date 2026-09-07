#!/bin/bash
#SBATCH --time=01:10:00   # walltime
#SBATCH --ntasks=512   # number of processor cores (i.e. tasks)
####You may need to specify a larger memory size than default if database is very large
#SBATCH --mem-per-cpu=4G   # memory per CPU core
#SBATCH -J "PKIKP_gen"   # job name
## /SBATCH -p general # partition (queue)
## /SBATCH -o slurm.%N.%j.out # STDOUT
## /SBATCH -e slurm.%N.%j.err # STDERR
# LOAD MODULES, INSERT CODE, AND RUN YOUR PROGRAMS HERE
##SBATCH --partition=scavenger
##SBATCH --qos=scavenger
 
module load 
mpirun ../../../../../src/SPECFEM3D/bin//xgenerate_databases_PKIKP3D
