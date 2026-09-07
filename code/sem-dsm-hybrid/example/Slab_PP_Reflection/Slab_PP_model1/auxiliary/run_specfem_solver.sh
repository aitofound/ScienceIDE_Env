#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CASE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd "${CASE_DIR}/../../.." && pwd)"
WORK_DIR="${CASE_DIR}/WORK/SPECFEM3D"
cd "${WORK_DIR}"

echo "!!!!!!!!!!!!!!!!!!!!!!!!!!"
echo "Note: Please ensure that the injected waves are prepared and the path is specified in DATA/Par_file."
echo "Running SEM solver for Slab_PP_model1: $(date)"

# Path of the bin
dir_src="${REPO_ROOT}/src/SPECFEM3D/bin"

# Load required modules (customize for your HPC system)
modules=""
mpiruncmd="mpirun"
module_command=""
if [[ -n "${modules}" ]]; then
  module_command="module load ${modules}"
fi

# Get current directory
currentdir="${WORK_DIR}"

if [[ ! -x "${dir_src}/xspecfem3D" ]]; then
  echo "Error: SPECFEM3D solver is missing: ${dir_src}/xspecfem3D" >&2
  exit 1
fi
if [[ ! -d "../InjectedWaves/OUTPUT_FILES" ]]; then
  echo "Error: shared injected waves are missing; run ../InjectedWaves/run.sh first." >&2
  exit 1
fi

# Extract number of processors from DATA/Par_file
NPROC=$(grep ^NPROC DATA/Par_file | cut -d = -f 2 | cut -d \# -f 1 | tr -d ' ')

# Prepare SBATCH job script
echo "Preparing SBATCH script: run_specfem3D.cmd"

cat > run_specfem3D.cmd << EOF
#!/bin/bash
#SBATCH --time=10:30:00              # Wall time
#SBATCH --ntasks=${NPROC}           # Number of processor cores
#SBATCH --mem-per-cpu=4G            # Memory per CPU core (adjust as needed)
#SBATCH -J "solver_Slab_PP"         # Job name
##SBATCH -p general                 # Uncomment and modify partition if needed
##SBATCH -o slurm.%N.%j.out        # STDOUT
##SBATCH -e slurm.%N.%j.err        # STDERR
#SBATCH --partition=scavenger
#SBATCH --qos=scavenger

# Load modules and run the solver
${module_command}
${mpiruncmd} ${dir_src}/xspecfem3D
EOF

# Submit the solver job
echo "Submitting the solver job..."
jobid_spec_tmp=$(sbatch run_specfem3D.cmd)
jobid_spec=`echo ${jobid_spec_tmp} | cut -d ' ' -f 4 | tr -d ' '`

echo "Solver job submitted with Job ID: $jobid_spec."
