#!/bin/bash
echo "Running 410-km topography simulation example: $(date)"


# Set paths and parameters
DIR_SRC="../../../../../src/SPECFEM3D/bin/"
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

# Copy and convert topography
echo -e "\nPreparing 410-km topography model for SEM..."
cp DATA/meshfem3D_files/topo_files/latlon_410km_topo.txt DATA/meshfem3D_files/latlon_interface_topo.txt
#
##R_TOP_BOUND is defined in DATA/meshfem3D_files/Coupling_Par_file.
R_TOP_BOUND=6371000.0 # Radius at top of SEM model in meters
R_EARTH=6371000.0              # Earth's radius in meters
R_410km=$(awk -v r=$R_EARTH 'BEGIN{print r - 410000.0}')
#
## Depth of 410 km relative to SEM top boundary (negative)
Z_410km_SEMBOX=$(awk -v rtop=$R_TOP_BOUND -v r410=$R_410km 'BEGIN{print -(rtop - r410)}')
echo "${Z_410km_SEMBOX}"

Z_MINUS_40km=$(awk -v z=$Z_410km_SEMBOX 'BEGIN{print z - 40000.0}')
Z_PLUS_40km=$(awk -v z=$Z_410km_SEMBOX 'BEGIN{print z + 40000.0}')

echo -e "\nConverting topography (lon-lat -> cubed sphere coordinates)..."
${DIR_SRC}/xcubedsphere_topo 2 \
    ./DATA/meshfem3D_files/latlon_interface_topo.txt \
    ${Z_410km_SEMBOX} ${Z_MINUS_40km} ${Z_PLUS_40km}
# - The first argument "1" specifies which interface we're converting. 
#   This corresponds to the first entry in ./DATA/meshfem3D_files/interfaces.dat,
#   which in this case is the 410-km discontinuity.
# - The second argument is the file containing the topography data 
#   in longitude-latitude format (./DATA/meshfem3D_files/latlon_interface_topo.txt).
# - The third argument (${Z_410km_SEMBOX}) is the reference depth of the interface.
# - The last two arguments define the allowable topography range in depth:
#   any values outside [${Z_410km_SEMBOX} - 40000, ${Z_410km_SEMBOX} + 40000] will be truncated.

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
#SBATCH --mem-per-cpu=7G
#SBATCH -J "PKPPKP_gen"
#SBATCH --partition=scavenger
#SBATCH --qos=scavenger

module load ${MODULES}
${MPIRUN_CMD} ${DIR_SRC}/xgenerate_databases_410km_Topo
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
