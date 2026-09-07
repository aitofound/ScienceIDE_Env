#!/bin/bash
#SBATCH --time=00:10:00   # walltime
#SBATCH --ntasks=200   # number of processor cores (i.e. tasks)
####You may need to specify a larger memory size than default if database is very large
#SBATCH --mem-per-cpu=4G   # memory per CPU core
#SBATCH -J "PKiKP_gen"   # job name
## /SBATCH -p general # partition (queue)
## /SBATCH -o slurm.%N.%j.out # STDOUT
## /SBATCH -e slurm.%N.%j.err # STDERR
# LOAD MODULES, INSERT CODE, AND RUN YOUR PROGRAMS HERE
##SBATCH --partition=scavenger
##SBATCH --qos=scavenger
 
module load  intel openmpi/intel/4.0.1
mpirun /proj/mazu/wenbowu/SEM_DSM/SPECFEM3D_Poseidon/bin/xgenerate_databases_ULVZ
