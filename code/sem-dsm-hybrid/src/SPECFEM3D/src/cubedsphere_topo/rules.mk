# Build rules for the cubed-sphere topography converter.

S := ${S_TOP}/src/cubedsphere_topo

cubedsphere_topo_TARGETS = \
	$E/xcubedsphere_topo \
	$(EMPTY_MACRO)

cubedsphere_topo_OBJECTS = \
	$O/program_cubedsphere_topo.cubed.o \
	$O/read_parameter_file.cubed.o \
	$O/euler_angles.cubed.o \
	$O/coordi_convert.cubed.o \
	$O/cubedsphere_topo.cubed.o \
	$O/save_topo.cubed.o \
	$O/read_parameter_coupling.cubed.o \
	$O/search_topo.cubed.o \
	$O/read_value_parameters.cubed.o \
	$O/get_value_parameters.cubed.o \
	$(EMPTY_MACRO)

$(cubedsphere_topo_OBJECTS): S := ${S_TOP}/src/cubedsphere_topo

$E/xcubedsphere_topo: $(cubedsphere_topo_OBJECTS)
	@echo ""
	@echo "building xcubedsphere_topo"
	@echo ""
	${FCLINK} -o $@ $(cubedsphere_topo_OBJECTS)
	@echo ""

$O/%.cubed.o: $S/%.f90 ${SETUP}/constants.h
	${FCCOMPILE_CHECK} ${FCFLAGS_f90} -c -o $@ $<
