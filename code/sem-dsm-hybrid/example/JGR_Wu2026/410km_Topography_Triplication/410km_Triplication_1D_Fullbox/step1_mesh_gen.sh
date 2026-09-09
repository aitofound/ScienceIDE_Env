#!/bin/bash
echo "Running 410-km simulation example: $(date)"


# Set paths and parameters
DIR_SRC="../../../bin/"
CURRENT_DIR=$(pwd)
MODULES="prun/2.2 ohpc intel/2018 hwloc/2.12.0 ucx/1.18.0 libfabric/1.18.0"
MPIRUN_CMD="mpirun"

# Setup directory structure
echo -e "\nSetting up example directory structure...\n"
mkdir -p OUTPUT_FILES OUTPUT_FILES/DATABASES_MPI

# Clean output files unless "noclean" is passed
if [ "$1" != "noclean" ]; then
    echo "Cleaning OUTPUT_FILES/"
    echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
    echo "!!!!!!!!!!!!IMPORTANT!!!!!!!!!!!!!!!!!!!!"
    echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
    echo "(1) Ensure OUTPUT_FILES/ is cleaned before submitting a new job."
    echo "(2) Verify your previous job has finished or has been manually terminated."
    echo "    Running multiple jobs simultaneously can corrupt the output files."
    echo

    rm -rf OUTPUT_FILES/*
fi

# Get number of processors (NPROC) from Par_file
NPROC=$(grep ^NPROC DATA/Par_file | cut -d '=' -f 2 | cut -d '#' -f 1 | tr -d ' ')

# Create MPI subdirectories
mkdir -p OUTPUT_FILES/DATABASES_MPI
for (( iproc=0; iproc<$NPROC; iproc++ )); do
    PADDED=$(printf "%04d" $iproc)
    mkdir -p "OUTPUT_FILES/DATABASES_MPIiproc${PADDED}"
done

# Prepare SBATCH scripts
echo -e "\nGenerating SBATCH job scripts..."

# Mesh generation script
cat <<EOF > run_meshfem3D.cmd
#!/bin/bash
#SBATCH --time=00:40:00
#SBATCH --ntasks=${NPROC}
#SBATCH --mem-per-cpu=4G
#SBATCH -J "PKPPKP_mesh"
#SBATCH --partition=scavenger
#SBATCH --qos=scavenger

module load ${MODULES}
${MPIRUN_CMD} ${DIR_SRC}/xmeshfem3D
EOF

# Database generation script
cat <<EOF > run_generate_databases.cmd
#!/bin/bash
#SBATCH --time=00:40:00
#SBATCH --ntasks=${NPROC}
#SBATCH --mem-per-cpu=6G
#SBATCH -J "PKPPKP_gen"
#SBATCH --partition=scavenger
#SBATCH --qos=scavenger

module load ${MODULES}
${MPIRUN_CMD} ${DIR_SRC}/xgenerate_databases_DSM1D
EOF

# Submit meshing job
echo -e "\nSubmitting meshing job..."
JOBID_MESH=$(sbatch run_meshfem3D.cmd | awk '{print $4}')
echo "Meshing job submitted with Job ID: $JOBID_MESH."

# Submit database job dependent on meshing
echo -e "\nSubmitting database generation job..."
JOBID_GEN=$(sbatch --dependency=afterany:$JOBID_MESH run_generate_databases.cmd | awk '{print $4}')
echo "Database generation job submitted with Job ID: $JOBID_GEN."

# Final instructions
echo -e "\nNext Steps:"
echo "1. Use OUTPUT_FILES/DATABASES_MPI/{depth_table_elastic, dist_table_elastic} to guide DSM configuration."
echo "2. Proceed with DSM and injected wave computations."
echo "3. Run the SEM solver once the injected sources are ready."
