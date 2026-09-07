#!/bin/bash
#SBATCH --time=01:20:00
#SBATCH --ntasks=512
#SBATCH --mem-per-cpu=7G
#SBATCH -J "410_gen"
#SBATCH --partition=scavenger
#SBATCH --qos=scavenger

module load prun/2.2 ohpc intel/2018 hwloc/2.12.0 ucx/1.18.0 libfabric/1.18.0
mpirun ../../../../src/SPECFEM3D/bin//xgenerate_databases_410km_Topo
