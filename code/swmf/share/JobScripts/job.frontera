#!/bin/bash

# Use default modules. To use IDL,
# module load idl
#
# Compilation:
# to avoid using too many threads on the login node, compile code with 
# make -j 4
#
# Move run directory to the $SCRATCH disk:
# make rundir
# mv run $SCRATCH/run1
# ln -s $SCRATCH/run1 .
#
# submit the jobscript:
# sbatch job.frontera
#
# monitor jobs:
# squeue -u USERNAME
#
# Continuous post-processing has to be done on the compute nodes with
# sbatch job.postproc.frontera

#SBATCH -J sub1               # Job name
#SBATCH -o SWMF.o%j           # Name of stdout output file
#SBATCH -e SWMF.e%j           # Name of stderr error file
#SBATCH -p normal             # Queue (partition) name: normal or development
#SBATCH -N 64                 # Total # of nodes 
#SBATCH --tasks-per-node 56   # Number of MPI tasks per node. 
#SBATCH -t 24:00:00           # Run time (hh:mm:ss)
#SBATCH --mail-type=all       # Send email at begin and end of job
###SBATCH --mail-user=your_email@umich.edu
#SBATCH -A BCS21001           # Project/Allocation name (may need change)

# Any other commands must follow all #SBATCH directives...

# By 05/2020, the default MPI_Reduce algorithm in IMPI may
# produce wrong results. For example, reducing a large
# array of ~30,000 integers on 2000 MPIs does not work correctly. This bug
# can be avoided by using another MPI_Reduce algorithm. The environment
# variable I_MPI_ADJUST_REDUCE is used to select the algorithm, and 
# either I_MPI_ADJUST_REDUCE=1 or I_MPI_ADJUST_REDUCE=3 works. The
# following line can also be copied to .bashrc to select algorithm for
# an interactive session:
export I_MPI_ADJUST_REDUCE=1 

# IMPI always produces warning messages for the jobs with >128 MPIs.
# The following line suppresses such warnings. 
export UCX_LOG_LEVEL=ERROR 

# If there is error messages like
# ib_mlx5_log.c:174 Transport retry count exceeded on
#    mlx5_1:1/RoCE (synd 0x15 vend 0x81 hw_synd 0/0)
# add the following setting:
# export UCX_TLS="knem,rc"

# Launch MPI code... 
# Use ibrun instead of mpirun or mpiexec
ibrun ./SWMF.exe  > runlog_`date +%y%m%d%H%M`

# The first node running ./PostProc.pl
# ibrun -n 1 ./PostProc.pl -r=300 -n=10 >& PostProc.log &

# the remianing nodes running SWMF
# Remember to change the number of processors if -N is changed
# Based on the system admin, task_affinity might be needed
# if PostProc.pl and SWMF.exe share the same node, but with
# -o (offset) 56, they should not share the same node.
# But if the performance is greatly reduced, then task_affinity
# is suggested to add ahead of ./SWMF.exe.

# ibrun -n 3528 -o 56 ./SWMF.exe > runlog_`date +%y%m%d%H%M`

#sleep 60
#touch PostProc.STOP
#wait
