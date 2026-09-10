#!/bin/bash
#SBATCH --job-name=parallel_sheets
#SBATCH --nodes=2              # or 1 if using 1 node
#SBATCH --ntasks-per-node=16   # or 32 for 1 node
#SBATCH --cpus-per-task=1
#SBATCH --time=02:00:00
#####SBATCH --partition=your_partition
#SBATCH --output=out_%j.txt
#SBATCH --error=err_%j.txt

# --- Load system MPI module (very important) ---
module purge
module load mpi/openmpi    # Or your cluster’s MPI module

# --- Activate your python venv ---
source ~/tensorflow_env/bin/activate

# (Optional) verify mpi4py is installed in the venv:
# python -c "import mpi4py; print('mpi4py OK')"

# --- Run your MPI Python program through SLURM ---
srun -n 32 python parallel_sheets_mpi.py

