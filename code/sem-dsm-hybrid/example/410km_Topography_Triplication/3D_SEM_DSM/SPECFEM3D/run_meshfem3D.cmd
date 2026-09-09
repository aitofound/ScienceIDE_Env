#!/bin/bash
#SBATCH --time=00:40:00
#SBATCH --ntasks=256
#SBATCH --mem-per-cpu=4G
#SBATCH -J "PKPPKP_mesh"
#SBATCH --partition=scavenger
#SBATCH --qos=scavenger

module load prun/2.2 ohpc intel/2018 hwloc/2.12.0 ucx/1.18.0 libfabric/1.18.0
mpirun ../../../../src/SPECFEM3D/bin//xmeshfem3D
