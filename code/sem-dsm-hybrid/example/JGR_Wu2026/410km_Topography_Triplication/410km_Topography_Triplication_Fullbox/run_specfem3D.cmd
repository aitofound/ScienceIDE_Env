#!/bin/bash
#SBATCH --time=40:30:00              # Wall time
#SBATCH --ntasks=512           # Number of processor cores
#SBATCH --mem-per-cpu=5G            # Memory per CPU core (adjust as needed)
#SBATCH -J "solver_410-km_topo"           # Job name
##SBATCH -p general                 # Uncomment and modify partition if needed
##SBATCH -o slurm.%N.%j.out        # STDOUT
##SBATCH -e slurm.%N.%j.err        # STDERR
#SBATCH --partition=scavenger
#SBATCH --qos=scavenger

# Load modules and run the solver
module load 
mpirun ../../../../src/SPECFEM3D/bin//xspecfem3D
