# Generate the top boundary of SEM model and the ICB interfaces.
# Both interfaces are flat (no topography) and are represented as constant-depth grids:
# - The ICB is set to a constant depth of -15,000.0 meters.
# - The model top interface is set to 0.0 meters.
echo -50000.0 | gawk '{for(i=1;i<301;i++) {for(j=1;j<301;j++) print $1;}}' >topo_ICB.dat
echo 0.0 | gawk '{for(i=1;i<301;i++) {for(j=1;j<301;j++) print $1;}}' >topo_top.dat

# Create a placeholder file for topography in the cubed-sphere system.
# Although the model includes the fluid outer core, this simulation is 1-D 
# the input file is required by the code for consistency.
# We generate this file with a 20x20 grid, where all values are set to zero.
echo "90 90" > real_bathymetry_topography  # Grid dimensions NXIxNETA: 20 x 20
echo "-80.0 -80.0" >>real_bathymetry_topography  # Minimal coordinates (XI_MIN, ETA_MIN)
echo "3.0 3.0" >>real_bathymetry_topography      # Grid spacing (DXI, DETA)
echo | awk '{for(i=1;i<21;i++) {for(j=1;j<21;j++) print 0.0;}}' >>real_bathymetry_topography
