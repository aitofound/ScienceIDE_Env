# Generate the interfaces.
# All interfaces are flat (no topography) and are represented as constant-depth grids:
echo -800000.0 | gawk '{for(i=1;i<111;i++) {for(j=1;j<111;j++) print $1;}}' >topo_800km.dat
echo -760000.0 | gawk '{for(i=1;i<111;i++) {for(j=1;j<111;j++) print $1;}}' >topo_760km.dat
echo -660000.0 | gawk '{for(i=1;i<111;i++) {for(j=1;j<111;j++) print $1;}}' >topo_660km.dat
echo -410000.0 | gawk '{for(i=1;i<111;i++) {for(j=1;j<111;j++) print $1;}}' >topo_410km.dat
echo -210000.0 | gawk '{for(i=1;i<111;i++) {for(j=1;j<111;j++) print $1;}}' >topo_210km.dat
echo -120000.0 | gawk '{for(i=1;i<111;i++) {for(j=1;j<111;j++) print $1;}}' >topo_120km.dat
echo -35000.0 | gawk '{for(i=1;i<111;i++) {for(j=1;j<111;j++) print $1;}}' >topo_35km.dat
echo -20000.0 | gawk '{for(i=1;i<111;i++) {for(j=1;j<111;j++) print $1;}}' >topo_20km.dat
echo 0.0 | gawk '{for(i=1;i<1001;i++) {for(j=1;j<1001;j++) print $1;}}' >topo_0km.dat

# Create a placeholder file for bathymetry and topography in the cubed-sphere system.
# Although the model does not include an ocean (no bathymetry is needed),
# the input file is required by the code for consistency.
# We generate this file with a 20x20 grid, where all values are set to zero.
echo "20 20" > real_bathymetry_topography  # Grid dimensions NXIxNETA: 20 x 20
echo "-20.0 -20.0" >>real_bathymetry_topography  # Minimal coordinates (XI_MIN, ETA_MIN)
echo "3.0 3.0" >>real_bathymetry_topography      # Grid spacing (DXI, DETA)
echo | awk '{for(i=1;i<21;i++) {for(j=1;j<21;j++) print 0.0;}}' >>real_bathymetry_topography
