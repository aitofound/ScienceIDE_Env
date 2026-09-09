#!/bin/bash


###################################### Start: Specify Input Parameters ###################################
# Directories for SEM and DSM inputs
dir_SEM_input=../SPECFEM3D/OUTPUT_FILES
dir_DSM_input=../DSM/box_to_recv

# Modules required (adjust based on your HPC environment)
modules=""

# Source directory for coupling executable
#src="../../../../src/Coupling/src_largeNpackGroup/"
src="../../../../src/Coupling/src/"

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

# ==========================================================
# Teleseismic station setup
# ==========================================================
#!/bin/bash

mkdir -p DATA

# --------------------------------------
# Array geometry
# --------------------------------------
dist0=130
ddist=1.0
ndist=16

# Source location
evlo=-115.4
evla=0.0

# Azimuths
azA=90.0
azB=80.0

# Total number of stations
echo $((2 * ndist)) > DATA/STATION

awk -v d0=${dist0} -v dd=${ddist} -v n=${ndist} \
    -v evlo=${evlo} -v evla=${evla} \
    -v azA=${azA} -v azB=${azB} '
BEGIN {
    pi = atan2(0, -1)
    deg2rad = pi / 180.0
    rad2deg = 180.0 / pi

    evlo_r = evlo * deg2rad
    evla_r = evla * deg2rad
    azB_r  = azB  * deg2rad

    for (i = 0; i < n; i++) {

        dist  = d0 + i * dd
        delta = dist * deg2rad

        # ======================================
        # Array A: azimuth = 90° (equator)
        # ======================================
        lonA = evlo + dist
        latA = evla
        printf "A_dist%.2f %.2f %.2f\n", dist, lonA, latA

        # ======================================
        # Array B: azimuth = 70° (great circle)
        # ======================================
        x = sin(evla_r)*cos(delta) + cos(evla_r)*sin(delta)*cos(azB_r)
        latB = atan2(x, sqrt(1.0 - x*x))

        lonB = evlo_r + atan2(sin(azB_r)*sin(delta)*cos(evla_r), \
                              cos(delta) - sin(evla_r)*sin(latB))

        printf "B_dist%.2f %.2f %.2f\n", \
               dist, lonB * rad2deg, latB * rad2deg
    }
}' >> DATA/STATION



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
echo "#SBATCH --time=06:50:00   # Walltime" >> submit_coupling.cmd
echo "#SBATCH -J \"PKP_2Hz_coupling\"   # Job name" >> submit_coupling.cmd
echo "#SBATCH --ntasks=$NPROC   # Number of processor cores" >> submit_coupling.cmd
echo "#SBATCH --mem-per-cpu=4G   # Memory per CPU core" >> submit_coupling.cmd
echo "##SBATCH --partition=scavenger   # Partition" >> submit_coupling.cmd
echo "##SBATCH --qos=scavenger         # Quality of service" >> submit_coupling.cmd
echo "module load ${modules}" >> submit_coupling.cmd
echo "$mpiruncmd ./coupling_integral" >> submit_coupling.cmd

# Submit the job
echo "Submitting coupling job..."
sbatch submit_coupling.cmd

# Final instructions
echo
echo "Waiting for the job to complete (~12 minutes)."
echo "When finished, check the generated seismograms in the seismograms/ directory."
echo `date`:

