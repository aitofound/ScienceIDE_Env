#!/bin/bash

echo "computing the Green's functions of the ULVZ example : `date`"

######################################Start - Specify your input parameters###################################

#source depth
source_depth_km=600.0

#source type, 1 for moment tensor and 2 for single force. 
moment_or_force=1 

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
distance_min=111.0
distance_max=119.0
distance_spacing=0.007
ndistance=`echo $distance_max $distance_min $distance_spacing | awk '{print int(($1-$2)/$3);}'`

# Ddepth_for_stress: Defines the spacing to create three radial nodes for computing stress at each depth specified
# in the Green's function depth file. These additional nodes are necessary for accurate stress calculations 
# (spatial derivative calculations).
# : Specifies the threshold for merging closely spaced nodes. If the depth difference between two nodes 
# is less than , they are combined into a single node to avoid redundancy.
# Recommended values: In most cases <2Hz,  = 0.01 km and Ddepth_for_stress = 0.04 km provide sufficient 
# accuracy and stability.
# Ddepth_for_stress must be greater than depth_tolerence. 
depth_tolerence=0.0025
Ddepth_for_stress=0.01


# Parameters for defining depths in each layer (zone). Follow these rules:
#  (1) In this PKP example, the depths and distances of Green's functions in the fluid outer core
#      and the solid inner core are saved in different files (fluid VS. solid). Their depths are
#      different, but the distances must be the same.
#  (2) Ensure the number of depths in each zone is at least equal to the number of fitting depths 
#      (e.g., nfit_dep=4 as specified in DSM_SEM/InjectedWaves/src/interpolate_DSMtoSEM_par.f90). 
#      This is required for the next step in InjectedWaves.
# Note: Depth values do not need to be equally spaced. However, this script generates depths with 
# equal spacing for simplicity.
#layer1 - the fluid outer core, 2889.0 km to 2909.0 km
declare -i ndep_OC_layer1=41 #number of depths in this layer
id_zone_layer1=2  #zone ID
top_dep_layer1=2889.0 #top depth
bot_dep_layer1=2909.0 #bottom depth

#layer2 - the lower mantle
declare -i ndep_LM_layer2=361 #number of depths in this layer
id_zone_layer2=3 #zone ID
top_dep_layer2=2785.0 #top depth
bot_dep_layer2=2889.0 #bottom depth

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
###################################### End: Specify Input Parameters #######################################



#set up distance table
echo "Creating distance table..."
echo ${distance_min} ${ndistance} ${distance_spacing} | awk '{ndist=$2; ddist=$3; dist0=$1; print ndist; for(idist=1;idist<=ndist;idist=idist+1) {print dist0+(idist-1)*ddist}; }' >dist_solid_list

# Note: The distance values in fluids must match those in solids, even if the number of fluid depths is zero. 
# Failure to ensure this consistency will result in an error of running the code.
cp dist_solid_list dist_fluid_list

echo "Creating depth table..."
#the inner core layer
echo ${Ddepth_for_stress} ${depth_tolerence}  >depth_solid_list
echo ${ndep_LM_layer2} >>depth_solid_list
echo $ndep_LM_layer2 $top_dep_layer2 $bot_dep_layer2 $id_zone_layer2 | awk '{ddep=($3-$2)/($1-1.0); for(idep=1;idep<=$1;idep++) {if(idep==$1) printf("%10.4f %d\n",$3,$4); else printf("%10.4f %d\n",$2+(idep-1)*ddep,$4);} }' >>depth_solid_list

#the outer core layer
echo  ${Ddepth_for_stress} ${depth_tolerence}  > depth_fluid_list
echo ${ndep_OC_layer1} >>depth_fluid_list
echo $ndep_OC_layer1 $top_dep_layer1 $bot_dep_layer1 $id_zone_layer1 | awk '{ddep=($3-$2)/($1-1.0); for(idep=1;idep<=$1;idep++) {if(idep==$1) printf("%10.4f %d\n",$3,$4); else printf("%10.4f %d\n",$2+(idep-1)*ddep,$4);} }' >>depth_fluid_list
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
        if (NR == 71) {
            print source_depth_km " 0.0  90.00  " moment_or_force " #depth(km) lat lon source_type (1 for moment tensor and 2 for single force)";
        } else if (NR == 72) {
	    print force " #exp + 6 components. MT: all Mij active. Explosion: only exp and first component used (10^exp scaling × scalar magnitude). Single force: exp,Fz,Fr,Ft active."
        } else if (NR == 77) {
            print "2 #save displacement (choose 1) or velocity (choose 2) seismograms in solid media";
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
# Note that the unit is dyn_cm=10^-7 N_m, so setting exp = 7 means 1 N_m.
setup_and_run_job "explosion" "7 1.0 1.0 1.0 0.0 0.0 0.0" "DSM_ULVZ_explosion" ${NPROC} "23:59:00"

# Delete the copied files after use
echo "    Cleaning up copied input files..."
rm -f depth_solid_list
rm -f depth_fluid_list
rm -f dist_solid_list
rm -f dist_fluid_list

echo "All jobs submitted."
