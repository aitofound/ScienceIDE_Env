#!/bin/bash
#SBATCH --time=10:30:00              # Wall time
#SBATCH --ntasks=375           # Number of processor cores
#SBATCH --mem-per-cpu=5G            # Memory per CPU core (adjust as needed)
#SBATCH -J "solver_ULVZ"           # Job name
##SBATCH -p general                 # Uncomment and modify partition if needed
##SBATCH -o slurm.%N.%j.out        # STDOUT
##SBATCH -e slurm.%N.%j.err        # STDERR
##SBATCH --partition=scavenger
##SBATCH --qos=scavenger

# Load modules and run the solver
module load prun/2.2 ohpc intel/2018 hwloc/2.12.0 ucx/1.18.0 libfabric/1.18.0
mpirun  ../../bin//xspecfem3D
