#!/bin/bash

echo "computing the Green's functions of the 410km_Triplication example : `date`"

######################################Start - Specify your input parameters###################################

#source depth
source_depth_km=0.0

#source type, 1 for moment tensor and 2 for single force. 
moment_or_force=2 

dir_SEM="../../SPECFEM3D/"
dir_src="../../../../../src/DSM/src/DSM_Solver/"
currentdir=`pwd`

# HPC environment settings
modules=""
mpiruncmd="mpirun"

#Number of processors used
NPROC=512
#Assume each node composed of 16 CPU cores. If not, change it accordingly.
#NNODE=`echo "$NPROC" | awk '{print int($1/16);}'`

# Parameters for distance and depth tables
# Make sure that the distance range is sufficiently large to cover the distances for the injected wavefield.
# Distances do not need to be equally spaced, but we make equal spacing for simplicity.
distance_min=4.5
distance_max=11.0
distance_spacing=0.01
ndistance=`echo $distance_max $distance_min $distance_spacing | awk '{print int(($1-$2)/$3);}'`

# Ddepth_for_stress: Defines the spacing to create three radial nodes for computing stress at each depth specified
# in the Green's function depth file. These additional nodes are necessary for accurate stress calculations 
# (spatial derivative calculations).
# : Specifies the threshold for merging closely spaced nodes. If the depth difference between two nodes 
# is less than , they are combined into a single node to avoid redundancy.
# Recommended values: In most cases <2Hz,  = 0.01 km and Ddepth_for_stress = 0.04 km provide sufficient 
# accuracy and stability.
# Ddepth_for_stress must be greater than . 
depth_tolerence=0.0025
Ddepth_for_stress=0.01


# Parameters for defining depths in each layer (zone). Follow these rules:
#  (1) In this 410-km discontinuity example, Green's functions are computed
#      only for the solid mantle (no fluid layers are involved).
#      However, for consistency and code verification, the distance tables
#      are kept identical for both solid and fluid media.
#  (2) Ensure the number of depths in each zone is at least equal to the number of fitting depths 
#      (e.g., nfit_dep=4 as specified in DSM_SEM/InjectedWaves/src/interpolate_DSMtoSEM_par.f90). 
#      This is required for the next step in InjectedWaves.
# Note: Depth values do not need to be equally spaced. However, this script generates depths with 
# equal spacing for simplicity.
#Layer1 - Above the 410-km lithosphere
declare -i ndep_UM_layer1=281 #number of depths in this layer
id_zone_layer1=7  #zone id, fixed number. Check it from your dsm model
top_dep_layer1=300.0 #depth at the top. This number must be within 410-660 km.
bot_dep_layer1=410.0 #depth at the bottom, fixed number. It is a discontinuity in the DSM model, 
                     #so you can not change it uncless the DSM model is changed.

#Layer2
declare -i ndep_UM_layer2=352 #number of depths in this layer
id_zone_layer2=6  #zone id, fixed number. Check it from your dsm model
top_dep_layer2=410.0 #depth at the top. This number must be within 410-660 km.
bot_dep_layer2=550.0 #depth at the bottom, fixed number. It is a discontinuity in the DSM model, 
                     #so you can not change it uncless the DSM model is changed.


# Output directories and subdirectories
dir_output='OUTPUT_FILES'
# Note:
#   (1) Pressure traces are used in the coupling step. However, in DSM, the potential is used to 
#       generate the injected wavefields in fluid. This is because, in DSM, the potential is equivalent 
#       to the pressure in SEM.
#   (2) Coefficients for each harmonic are saved in the directory `coef_cAnddcdr/`. These were primarily 
#       used in older versions but are retained here for convenience. Using these coefficients, 
#       Green's functions can be efficiently computed at additional distances if needed.
sub_dir_output=("coef_cAnddcdr" "disp_fluid" "disp_solid" "stress" "potential" "pressure" "velo_solid")
declare -a sources=("Mzz" "Mrr" "Mtt" "Mzr" "Mzt" "Mrt")
###################################### End: Specify Input Parameters #######################################


#set up distance table
echo "Creating distance table..."
echo ${distance_min} ${ndistance} ${distance_spacing} | awk '{ndist=$2; ddist=$3; dist0=$1; print ndist; for(idist=1;idist<=ndist;idist=idist+1) {print dist0+(idist-1)*ddist}; }' >dist_solid_list

# Note: The distance values in fluids must match those in solids, even if the number of fluid depths is zero. 
# Failure to ensure this consistency will result in an error of running the code.
cp dist_solid_list dist_fluid_list

echo "Creating depth table..."

ndep_total=`expr $ndep_UM_layer1 + $ndep_UM_layer2`

echo ${Ddepth_for_stress} ${depth_tolerence}  >depth_solid_list
echo ${ndep_total} >>depth_solid_list
#Layer 1
echo $ndep_UM_layer1 $top_dep_layer1 $bot_dep_layer1 $id_zone_layer1 | awk '{ddep=($3-$2)/($1-1.0); for(idep=1;idep<=$1;idep++) {if(idep==$1) printf("%10.4f %d\n",$3,$4); else printf("%10.4f %d\n",$2+(idep-1)*ddep,$4);} }' >>depth_solid_list
#Layer 2
echo $ndep_UM_layer2 $top_dep_layer2 $bot_dep_layer2 $id_zone_layer2 | awk '{ddep=($3-$2)/($1-1.0); for(idep=1;idep<=$1;idep++) {if(idep==$1) printf("%10.4f %d\n",$3,$4); else printf("%10.4f %d\n",$2+(idep-1)*ddep,$4);} }' >>depth_solid_list

#No depths in fluid media, but still need to prepare this file
echo  ${Ddepth_for_stress} ${depth_tolerence}  > depth_fluid_list
echo 0 >>depth_fluid_list

###################################END - Specify your input parameters#######################################



###################### Function to Set Up and Run Jobs ######################
setup_and_run_job() {
    local tensor_dir=$1
    local force_vector=$2
    local job_name=$3
    local processors=$4
    local time_estimate=$5

    echo "Setting up $tensor_dir ..."
    mkdir -p $tensor_dir
    cd $tensor_dir

    # Copy executable
    echo "    Copying executable file..."
    cp ${dir_src}/dsmti .

    # Clean previous outputs
    if [ "$1" != "noclean" ]; then
        echo "    Cleaning OUTPUT_FILES/..."
        rm -rf OUTPUT_FILES/
    fi

    # Create necessary directories
    mkdir -p ${dir_output}
    for this_subdirectory in "${sub_dir_output[@]}"; do
        mkdir -p ${dir_output}/${this_subdirectory}
    done

    # Prepare input files
    echo "    Preparing input files..."
    cp ../depth_solid_list .
    cp ../depth_fluid_list .
    cp ../dist_solid_list .
    cp ../dist_fluid_list .
    cp ${dir_SEM}/DATA/dsm_model_input .

    # Modify the `dsm_model` file for this specific run
    echo "    Modifying dsm_model file for $tensor_dir..."
    awk -v source_depth_km="$source_depth_km" -v moment_or_force="$moment_or_force" -v force="$force_vector" '{
        if (NR == 65) {
            print source_depth_km " 0.0  90.00  " moment_or_force " #depth(km) lat lon source_type (1 for moment tensor and 2 for single force)";
        } else if (NR == 66) {
            print force " #exp_dyn-cm Mij,i=1-6 (moment tensor) OR exp_dyn Force_i,i=1,6 (only first three forces functional)";
        } else if (NR == 71) {
            print "1 #save displacement (choose 1) or velocity (choose 2) seismograms in solid media";
        } else {
            print $0;
        }
    }' dsm_model_input >dsm_model

    # Prepare SBATCH script
    echo "    Preparing SBATCH script..."
    echo "#!/bin/bash" >submit_DSM_${tensor_dir}.cmd
    echo "#SBATCH --time=${time_estimate}   # walltime" >> submit_DSM_${tensor_dir}.cmd
    echo "#SBATCH --ntasks=${processors}   # number of processor cores" >> submit_DSM_${tensor_dir}.cmd
    echo "#SBATCH --mem-per-cpu=4G   # memory per CPU core" >> submit_DSM_${tensor_dir}.cmd
    echo "#SBATCH -J \"${job_name}\"   # job name" >> submit_DSM_${tensor_dir}.cmd
    # The following two lines specify that the job will run in the 'scavenger' partition and QoS at WHOI HPC.
    # Note: This configuration is specific to HPCs like ours where high processor counts (e.g., ${processors}) 
    # must use scavenger resources. On other HPCs, users may need to modify these lines to fit their system's
    # queueing policies or remove them if no such limitation exists.
    echo "##SBATCH --partition=scavenger" >>submit_DSM_${tensor_dir}.cmd
    echo "##SBATCH --qos=scavenger" >> submit_DSM_${tensor_dir}.cmd
    echo "module load ${modules}" >> submit_DSM_${tensor_dir}.cmd
    echo "${mpiruncmd} ./dsmti <dsm_model" >> submit_DSM_${tensor_dir}.cmd

    # Submit the job
    echo "Submitting SBATCH job..."
    sbatch submit_DSM_${tensor_dir}.cmd
    echo "    Wait for $tensor_dir job to complete (~${time_estimate})."
    cd ..
}

# Set up and run jobs for each element of moment tensor. 
# Note that the unit is dyn=10^-5 N, so set exp = 5.
setup_and_run_job "Fz" "5 1.0 0.0 0.0 0.0 0.0 0.0" "DSM_410km_Fz" ${NPROC} "23:59:00"
#setup_and_run_job "Fr" "5 0.0 1.0 0.0 0.0 0.0 0.0" "DSM_410km_Fr" ${NPROC} "23:59:00"
#setup_and_run_job "Ft" "5 0.0 0.0 1.0 0.0 0.0 0.0" "DSM_410km_Ft" ${NPROC} "23:59:00"

# Delete the copied files after use
echo "    Cleaning up copied input files..."
rm -f depth_solid_list
rm -f depth_fluid_list
rm -f dist_solid_list
rm -f dist_fluid_list

echo "All jobs submitted."
