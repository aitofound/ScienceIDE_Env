#!/bin/bash

echo
echo "Extracting the injected DSM wavefield of the PKP_precursor example: `date`"

###################################### Start: Specify Input Parameters ###################################
# Input and output directories:
dir_SEM="../SPECFEM3D/OUTPUT_FILES/DATABASES_MPI/"
dir_DSM="../DSM/src_to_box/"
dir_Injection_src="../../../../src/InjectedWaves/src/"
dir_OUTPUT='OUTPUT_FILES'
dir_DATA='DATA'

# Modules required for WHOI HPC. Modify these as needed for your HPC system.
modules=""
mpiruncmd="mpirun"

# Number of processors used.
NPROC=141

# Assume each node has 16 CPU cores. Adjust this value based on your system configuration.
# Uncomment and modify if necessary.
# NNODE=`echo "$NPROC" | awk '{print int($1/16);}'`

# Specify source type:
# 1 = single-force (e.g., used in reciprocity application or other non-earthquake sources)
# 2 = moment tensor (e.g., used in receiver-side or deep Earth coupling scenarios).
# 3 = explosion (e.g., used in deep Earth coupling scenarios).
source_type=3

# SINGLE_FORCE_ENZ is applicable only for source_type=1 (single force). 
# It specifies the components of the single force to inject:
#    - "1" for injection of the respective component (East, North, Vertical).
#    - "0" to skip injection of the respective component.
# Examples:
#    Only vertical component:
declare -a SINGLE_FORCE_ENZ=("0" "0" "1")
#    For North-South and East-West components:
# declare -a SINGLE_FORCE_ENZ=("1" "1" "0")
#    For all three components:
# declare -a SINGLE_FORCE_ENZ=("1" "1" "1")

# Number of frequencies. Ensure the DSM folder contains all these frequencies (DSM nfreq must be >= nfreq_inject).
nfreq_inject=4096

# Start and end times for cutting waveforms. This window must include the full range of interest.
# Beware of truncation effects, such as spectral leakage and artifacts, when cutting after the first arrival.
time_start=760.0
time_end=960.0

# Number of Green's functions saved in one package:
# - A large number may cause memory issues during SEM processing.
# - A small number slows SEM as it reads the injected waves more frequently.
npt_one_packakge=600

# Apply filtering to Green's functions? 1 = Yes, 0 = No.
# Filtering reduces ringing artifacts caused by abrupt DSM frequency cutoffs.
filtering=0
freq_low=0.005  # Low cutoff frequency.
freq_high=1.0   # High cutoff frequency.

###################################### End: Specify Input Parameters #######################################


###################################### Step 1: Copy Executable ###########################################
echo "    (1) Copying the executable file extract_Injectedwaves..."
cp ${dir_Injection_src}/extract_Injectedwaves .

###################################### Step 2: DSM Setup Validation ######################################
if [[ ${source_type} -eq 1 ]]; then
    # Single-force source
    declare -a sources=("Fr" "Ft" "Fz")

elif [[ ${source_type} -eq 2 ]]; then
    # Moment-tensor source
    declare -a sources=("Mzz" "Mrr" "Mtt" "Mzr" "Mzt" "Mrt")

elif [[ ${source_type} -eq 3 ]]; then
    # Explosion source
    declare -a sources=("explosion")
fi

# Note: Ensure consistent DSM setup for source components. To do: Validation logic.

###################################### Step 3: Create Folders ###########################################
mkdir -p ${dir_OUTPUT} ${dir_DATA}
for this_source in "${sources[@]}"; do
    mkdir -p ${dir_OUTPUT}/${this_source}/{velo_solid,stress,disp_fluid,pressure,chi_dot}
done

###################################### Step 4: Prepare Parameter File ###################################
echo "Preparing parameter file Par_file..."
echo ${source_type} > ${dir_DATA}/Par_file  # Source type.
echo ${SINGLE_FORCE_ENZ[@]} >> ${dir_DATA}/Par_file  # Components to inject (if single force).
echo ${dir_DSM} >> ${dir_DATA}/Par_file  # DSM input path.
echo ${dir_SEM} >> ${dir_DATA}/Par_file  # SEM input path.
echo ${nfreq_inject} >> ${dir_DATA}/Par_file  # Number of frequencies.
echo ${time_start} ${time_end} >> ${dir_DATA}/Par_file  # Time window for waveform cutting.
echo ${npt_one_packakge} >> ${dir_DATA}/Par_file  # Number of Green's functions per package.
echo ${filtering} >> ${dir_DATA}/Par_file  # Apply filtering or not.
echo ${freq_low} ${freq_high} >> ${dir_DATA}/Par_file  # Filter frequencies.

###################################### Step 5: Prepare SBATCH Script ###################################
echo "    (2) Preparing the SBATCH script for job submission..."
cat << EOF > submit_InjectedWaves.cmd
#!/bin/bash
#SBATCH --time=02:00:00   # Walltime
#SBATCH --ntasks=${NPROC}   # Number of processors
#SBATCH --mem-per-cpu=8G   # Memory per CPU core
#SBATCH -J "PKIKP_Inject"   # Job name
#SBATCH --partition=scavenger   # Partition
#SBATCH --qos=scavenger         # QoS
#SBATCH --output=slurm.%N.%j.out   # STDOUT
#SBATCH --error=slurm.%N.%j.err    # STDERR

module load ${modules}
${mpiruncmd} ./extract_Injectedwaves
EOF

###################################### Step 6: Submit Job ##############################################
echo "    (3) Submitting the SBATCH job..."
sbatch submit_InjectedWaves.cmd
echo "        Wait for the SBATCH job to complete. It typically takes about 1 hour with 121 processors."
