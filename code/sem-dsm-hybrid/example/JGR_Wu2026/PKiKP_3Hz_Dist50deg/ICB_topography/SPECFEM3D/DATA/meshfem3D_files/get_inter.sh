#Because the meshes are distorted by topography, flat interfaces are introduced below the top 
#boundary and above the bottom boundary to confine the topographic effects within the model. 
#This preprocessing simplifies the coupling table arrays by reducing them to a single depth level rather than multiple depths.
echo -25000.0 | gawk '{for(i=1;i<301;i++) {for(j=1;j<301;j++) print $1;}}' >topo_10kmBelowICB.dat
echo -7500.0 | gawk '{for(i=1;i<301;i++) {for(j=1;j<301;j++) print $1;}}' >topo_7.5kmAboveICB.dat
# - The model top interface is set to 0.0 meters.
echo 0.0 | gawk '{for(i=1;i<301;i++) {for(j=1;j<301;j++) print $1;}}' >topo_top.dat
