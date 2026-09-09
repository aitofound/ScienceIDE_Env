#!/bin/bash

###################################### Start: Specify Input Parameters ###################################
# Directories for SEM and DSM inputs
dir_SEM_input=../SPECFEM3D/OUTPUT_FILES
dir_DSM_input=../DSM/box_to_recv/

# Modules required (adjust based on your HPC environment)
modules=""

# Source directory for coupling executable
#src="../../../../src/Coupling/src/"
src="/proj/mazu/wenbowu/SEM_DSM/Coupling/src"

# Parameters from DATA/Par_file
npoints_taper_SEM=120   # Number of points to taper SEM displacements and tractions

# Components to compute
vertical_component=.true.   # Compute vertical component
radial_component=.false.    # Compute radial component
transverse_component=.false. # Compute transverse component

# Number of interpolation points for extracting Green's functions at target distances and depths.
# Must be an even number (e.g., 2, 4, 6, ...).
# Higher values improve accuracy but slow computation. Typically 4 is good ennough.
nfit_dist=4
nfit_dep=4

# Number of processors (ensure NPROC <= number of SEM output packages, usually >1000)
NPROC=512

# Teleseismic station setup
# ------------------------------------------------------------
mkdir -p DATA
station_file="../DATA/tele_station.txt"

if [ ! -f "${station_file}" ]; then
  echo "ERROR: station file not found: ${station_file}" >&2
  exit 1
fi

awk '
/^[[:space:]]*#/ || NF < 3 { next }
NF >= 4 && $3 ~ /^[-+]?[0-9.]+$/ && $4 ~ /^[-+]?[0-9.]+$/ {
    print $2, $3, $4
    next
}
$2 ~ /^[-+]?[0-9.]+$/ && $3 ~ /^[-+]?[0-9.]+$/ {
    print $1, $2, $3
}
' "${station_file}" > DATA/STATION.tmp

ndist="$(wc -l < DATA/STATION.tmp | awk '{print $1}')"
if [ "${ndist}" -lt 1 ]; then
  echo "ERROR: no stations found in ${station_file}" >&2
  rm -f DATA/STATION.tmp
  exit 1
fi

{
  echo "${ndist}"
  cat DATA/STATION.tmp
} > DATA/STATION
rm -f DATA/STATION.tmp

###################################### End: Specify Input Parameters #######################################

# Current working directory and MPI run command
currentdir=`pwd`
mpiruncmd="mpirun"

# Set up the directory structure
echo
echo "Setting up example..."
echo

# Clean output directories unless "noclean" is passed as an argument
if [ "$1" != "noclean" ]; then
  echo "Cleaning seismograms/..."
  rm -rf seismograms/
  mkdir -p OUTPUT_FILES
  mkdir -p seismograms
fi

# Prepare the DATA/Par_file input file
echo
echo "Creating DATA/Par_file..."
echo "directory_SEM_input= $dir_SEM_input" > DATA/Par_file
echo "directory_DSM_input= $dir_DSM_input" >> DATA/Par_file
echo "npoints_taper_SEM= $npoints_taper_SEM" >> DATA/Par_file
echo "vertical_component= $vertical_component" >> DATA/Par_file
echo "radial_component= $radial_component" >> DATA/Par_file
echo "transverse_component= $transverse_component" >> DATA/Par_file
echo "nfit_dep= $nfit_dep" >> DATA/Par_file
echo "nfit_dist= $nfit_dist" >> DATA/Par_file

# Copy the coupling executable to the current directory
echo "Copying the executable file..."
cp ${src}/coupling_integral .
echo

# Prepare the SBATCH job script
echo "Generating SBATCH script: submit_coupling.cmd..."
echo "#!/bin/bash" > submit_coupling.cmd
echo "# Submit this script with: sbatch submit_coupling.cmd" >> submit_coupling.cmd
echo "#SBATCH --time=04:50:00   # Walltime" >> submit_coupling.cmd
echo "#SBATCH -J \"410km_coupling\"   # Job name" >> submit_coupling.cmd
echo "#SBATCH --ntasks=$NPROC   # Number of processor cores" >> submit_coupling.cmd
echo "#SBATCH --mem-per-cpu=4G   # Memory per CPU core" >> submit_coupling.cmd
echo "#SBATCH --partition=scavenger   # Partition" >> submit_coupling.cmd
echo "#SBATCH --qos=scavenger         # Quality of service" >> submit_coupling.cmd
echo "module load ${modules}" >> submit_coupling.cmd
echo "$mpiruncmd ./coupling_integral" >> submit_coupling.cmd

# Submit the job
echo "Submitting coupling job..."
sbatch submit_coupling.cmd

# Final instructions
echo
echo "Waiting for the job to complete (~4 hours)."
echo "When finished, check the generated seismograms in the seismograms/ directory."
echo `date`:
