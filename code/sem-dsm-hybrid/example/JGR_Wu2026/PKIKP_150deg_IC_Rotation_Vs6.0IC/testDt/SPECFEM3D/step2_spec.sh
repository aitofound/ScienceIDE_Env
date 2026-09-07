#!/bin/bash

echo "!!!!!!!!!!!!!!!!!!!!!!!!!!"
echo "Note: Please ensure that the injected waves are prepared and the path is specified in DATA/Par_file."
echo "Running SEM solver for the 3-D PKIKP example: $(date)"

# Load required modules (customize for your HPC system)
modules="prun/2.2 ohpc intel/2018 hwloc/2.12.0 ucx/1.18.0 libfabric/1.18.0"
mpiruncmd="mpirun"
dir_src="../../../../../src/SPECFEM3D/bin/"

# Get current directory
currentdir=$(pwd)

# Extract number of processors from DATA/Par_file
NPROC=$(grep ^NPROC DATA/Par_file | cut -d = -f 2 | cut -d \# -f 1 | tr -d ' ')

# Prepare SBATCH job script
echo "Preparing SBATCH script: run_specfem3D.cmd"

cat > run_specfem3D.cmd << EOF
#!/bin/bash
#SBATCH --time=10:30:00              # Wall time
#SBATCH --ntasks=${NPROC}           # Number of processor cores
#SBATCH --mem-per-cpu=4G            # Memory per CPU core (adjust as needed)
#SBATCH -J "solver_PKIKP"           # Job name
##SBATCH -p general                 # Uncomment and modify partition if needed
##SBATCH -o slurm.%N.%j.out        # STDOUT
##SBATCH -e slurm.%N.%j.err        # STDERR
#SBATCH --partition=scavenger
#SBATCH --qos=scavenger

# Load modules and run the solver
module load ${modules}
${mpiruncmd} ${dir_src}/xspecfem3D
EOF

# Submit the solver job
echo "Submitting the solver job..."
sbatch run_specfem3D.cmd

