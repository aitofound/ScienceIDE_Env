# Generate the 310-km (top of the model, with 0 m), 330-km (20000 m below the top) and 500-km interfaces.
# Both interfaces are flat (no topography) and are represented as constant-depth grids:
echo -190000.0 | gawk '{for(i=1;i<111;i++) {for(j=1;j<111;j++) print $1;}}' >topo_500km.dat
echo -20000.0 | gawk '{for(i=1;i<111;i++) {for(j=1;j<111;j++) print $1;}}' >topo_330km.dat
echo 0.0 | gawk '{for(i=1;i<111;i++) {for(j=1;j<111;j++) print $1;}}' >topo_310km.dat
