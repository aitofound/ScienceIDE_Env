#  Copyright (C) 2002 Regents of the University of Michigan, 
#  portions used with permission 
#  For more information, see http://csem.engin.umich.edu/tools/swmf

include ../../../Makefile.def

include ../../../Makefile.conf

help:
	@echo "You can test the following modules in ../src:"
	@echo
	@echo "   test_axes             - test CON_axes"
	@echo "   test_coord_transform  - test ModCoordTransform"
	@echo "   test_freq             - test ModFreq"
	@echo "   test_geopack          - test CON_geopack"
	@echo "   test_interpolate      - test ModInterpolate"
	@echo "   test_interpolate_amr  - test ModInterpolateAMR"
	@echo "   test_io_unit          - test ModIoUnit"
	@echo "   test_initial_state    - test ModInitialState"
	@echo "   test_line_extract     - test CON_line_extract"
	@echo "   test_linear_advection - test ModLinearAdvection"
	@echo "   test_linear_solver    - test ModLinearSolver"
	@echo "   test_linear_wrapper   - test linear_solver_wrapper"
	@echo "   test_lookup_table     - test ModLookupTable"
	@echo "   test_planet_field     - test CON_planet_field"
	@echo "   test_plot_file        - test ModPlotFile"
	@echo "   test_plot_file_simple - test ModPlotFileSimple"
	@echo "   test_process_var_name - test ModProcessVarName"
	@echo "   test_random_number    - test ModRandomNumber"
	@echo "   test_ray_trace        - test CON_ray_trace"
	@echo "   test_sort             - test ModSort"
	@echo "   test_time_convert     - test ModTimeConvert"
	@echo "   test_triangulate      - test ModTriangulate"
	@echo "   test_utilities        - test ModUtilities"
	@echo "   test                  - test all the above"
	@echo
	@echo "To test everything with easy to read output try"
	@echo
	@echo "   make -s test"
	@echo
	@echo "The parallel tests can be executed like this"
	@echo
	@echo "   make NP=4 test_line_extract"
	@echo
	@echo "The default value for NP is ${NP}."

test:
	-@make test_axes
	-@make test_coord_transform
	-@make test_freq
	-@make test_geopack
	-@make test_io_unit
	-@make test_interpolate
	-@make test_interpolate_amr
	-@make test_initial_state
	-@make test_line_extract
	-@make test_linear_advection
	-@make test_linear_solver
	-@make test_linear_wrapper
	-@make test_lookup_table
	-@make test_planet_field
	-@make test_plot_file
	-@make test_plot_file_simple
	-@make test_random_number
	-@make test_ray_trace
	-@make test_sort
	-@make test_time_convert
	-@make test_triangulate
	-@make test_utilities
	@echo ---------------------------------------------------------
	@ls -l *.diff

clean: cleanfiles

distclean: clean
	rm -rf *.tmp *.diff test*.out

# Setting BLESS=YES or Y will copy the solution into the reference solution
BLESS=NO

DIFFNUM = ${SCRIPTDIR}/DiffNum.pl -BLESS=${BLESS}

#######################################################################
test_time_convert_exe:
	@(cd ../src; make LIB)
	@make test_time_convert.o
	${LINK.f90} -o test_time_convert.exe test_time_convert.o \
		-L${LIBDIR} -lSHARE ${Lflag}

test_time_convert:
	@echo test_time_convert starting        > test_time_convert.diff
	@make test_time_convert_exe            >> test_time_convert.diff
	@echo ./test_time_convert.exe          >> test_time_convert.diff
	-@(./test_time_convert.exe > test_time_convert.tmp)
	-@(diff test_time_convert.tmp test_time_convert.log \
						> test_time_convert.diff)
	@ls -l test_time_convert.diff

#######################################################################
test_coord_transform_exe:
	@(cd ../src; make LIB)
	@make test_coord_transform.o
	${LINK.f90} -o test_coord_transform.exe test_coord_transform.o \
		-L${LIBDIR} -lSHARE ${Lflag}

test_coord_transform: 
	@echo test_coord_transform starting     > test_coord_transform.diff
	@make test_coord_transform_exe         >> test_coord_transform.diff
	@echo ./test_coord_transform.exe       >> test_coord_transform.diff
	-@(./test_coord_transform.exe > test_coord_transform.tmp)
	-@(${DIFFNUM} -b -a=1e-12 \
		test_coord_transform.tmp test_coord_transform.log \
						> test_coord_transform.diff)
	@ls -l test_coord_transform.diff

#######################################################################
test_freq_exe:
	@(cd ../src; make LIB)
	@make test_freq.o
	${LINK.f90} -o test_freq.exe test_freq.o \
		-L${LIBDIR} -lSHARE ${Lflag}

test_freq:
	@echo test_freq starting		>  test_freq.diff
	@make test_freq_exe			>> test_freq.diff
	@echo './test_freq.exe'			>> test_freq.diff
	-@(./test_freq.exe > test_freq.tmp)
	-@(diff test_freq.tmp test_freq.log)	>  test_freq.diff
	@ls -l test_freq.diff

#######################################################################
test_linear_solver_exe:
	@(cd ../src; make LIB)
	@make test_linear_solver.o 
	${LINK.f90} -o test_linear_solver.exe test_linear_solver.o \
		-L${LIBDIR} -lSHARE ${LflagMpi}

test_linear_solver:
	@echo test_linear_solver starting 	>  test_linear_solver.diff
	@make test_linear_solver_exe		>> test_linear_solver.diff
	@echo ./test_linear_solver.exe		>> test_linear_solver.diff
	-@(./test_linear_solver.exe > test_linear_solver.tmp)
	-@(${DIFFNUM} -b -a=1e-9 \
		test_linear_solver.tmp test_linear_solver.log \
						>  test_linear_solver.diff)
	@ls -l test_linear_solver.diff

#######################################################################
SEARCH_C = -I${SHAREDIR}

test_linear_wrapper_exe:
	(cd ../src; make LIB)
	make test_linear_wrapper.o
	${LINK.f90} ${NOMAINFLAG} -o test_linear_wrapper.exe test_linear_wrapper.o \
		-L${LIBDIR} -lSHARE ${LflagMpi}

test_linear_wrapper:
	@echo test_linear_wrapper starting 	  >  test_linear_wrapper.diff
	make test_linear_wrapper_exe		  >> test_linear_wrapper.diff
	@echo ${MPIRUN} ./test_linear_wrapper.exe >> test_linear_wrapper.diff
	-@(${MPIRUN} ./test_linear_wrapper.exe > test_linear_wrapper.tmp)
	-@grep "ERROR" test_linear_wrapper.tmp > test_linear_wrapper.diff
	@ls -l test_linear_wrapper.diff

#######################################################################
test_linear_advection_exe:
	@(cd ../src; make LIB)
	@make test_linear_advection.o
	${LINK.f90} -o test_linear_advection.exe test_linear_advection.o \
		-L${LIBDIR} -lSHARE ${Lflag}

test_linear_advection:
	@echo test_linear_advection starting	>  test_linear_advection.diff
	@make test_linear_advection_exe		>> test_linear_advection.diff
	@echo ./test_linear_advection.exe	>> test_linear_advection.diff
	-@(./test_linear_advection.exe)
	-@(${DIFFNUM} \
		test_linear_advection.tmp \
		test_linear_advection.log	>  test_linear_advection.diff)
	@ls -l test_linear_advection.diff

#######################################################################
test_sort_exe:
	@(cd ../src; make LIB)
	@make test_sort.o
	${LINK.f90} -o test_sort.exe test_sort.o \
		-L${LIBDIR} -lSHARE ${Lflag}

test_sort:
	@echo test_sort starting		>  test_sort.diff
	@make test_sort_exe			>> test_sort.diff
	@echo ./test_sort.exe			>> test_sort.diff
	-@(./test_sort.exe > test_sort.tmp)
	-@(diff test_sort.tmp test_sort.log 	>  test_sort.diff)
	@ls -l test_sort.diff
#######################################################################
test_hypergeometric.o: test_hypergeometric.f90
	${COMPILE.f90} ${Cflag0} test_hypergeometric.f90

test_hypergeometric_exe:
	@(cd ../src; make LIB)
	@make test_hypergeometric.o
	${LINK.f90} -o test_hypergeometric.exe test_hypergeometric.o \
		-L${LIBDIR} -lSHARE ${Lflag}

test_hypergeometric_check:
	-@(${DIFFNUM} -b -r=1e-7 \
		test_inductance.out test_inductance.ref \
			> test_hypergeometric.diff)

test_hypergeometric:
	@echo test_hypergeometric starting     >  test_hypergeometric.diff
	@make test_hypergeometric_exe	       >> test_hypergeometric.diff
	@echo ./test_hypergeometric.exe        >> test_hypergeometric.diff
	-@(rm -f test_inductunce.out test_fields.out)
	-@(./test_hypergeometric.exe > test_hypergeometric.diff)
	-@(make test_hypergeometric_check)
	@ls -l test_hypergeometric.diff
#######################################################################
test_cubic_equation.o: test_cubic_equation.f90
	${COMPILE.f90} ${Cflag0} test_cubic_equation.f90

test_cubic_equation_exe:
	@(cd ../src; make LIB)
	@make test_cubic_equation.o
	${LINK.f90} -o test_cubic_equation.exe test_cubic_equation.o \
		-L${LIBDIR} -lSHARE ${Lflag}

test_cubic_equation:
	@echo test_cubic_equation starting     >  test_cubic_equation.diff
	@make test_cubic_equation_exe	       >> test_cubic_equation.diff
	@echo ./test_cubic_equation.exe        >> test_cubic_equation.diff
	-@(./test_cubic_equation.exe > test_cubic_equation.diff)
	@ls -l test_cubic_equation.diff
#######################################################################
test_io_unit_exe:
	@(cd ../src; make LIB)
	@make test_io_unit.o
	${LINK.f90} -o test_io_unit.exe test_io_unit.o \
		-L${LIBDIR} -lSHARE ${Lflag}

test_io_unit:
	@echo test_io_unit starting			>  test_io_unit.diff
	@make test_io_unit_exe				>> test_io_unit.diff
	@echo ./test_io_unit.exe			>> test_io_unit.diff
	-@(./test_io_unit.exe > test_io_unit.tmp)
	-@(diff test_io_unit.tmp test_io_unit.log	>  test_io_unit.diff)
	@ls -l test_io_unit.diff

#######################################################################
test_interpolate_exe:
	@(cd ../src; make LIB)
	@make test_interpolate.o
	${LINK.f90} -o test_interpolate.exe test_interpolate.o \
		-L${LIBDIR} -lSHARE ${Lflag}

test_interpolate:
	@echo test_interpolate starting		>  test_interpolate.diff
	@make test_interpolate_exe		>> test_interpolate.diff
	@echo ./test_interpolate_exe		>> test_interpolate.diff
	-@(./test_interpolate.exe > test_interpolate.tmp)
	-@(diff test_interpolate.tmp test_interpolate.log \
						>  test_interpolate.diff)
	@ls -l test_interpolate.diff
#######################################################################
test_interpolate_amr.o: test_interpolate_amr.f90
	${COMPILE.f90} ${Cflag0} test_interpolate_amr.f90

test_interpolate_amr_exe:
	@(cd ../src; make LIB)
	@make test_interpolate_amr.o
	${LINK.f90} -o test_interpolate_amr.exe test_interpolate_amr.o \
		-L${LIBDIR} -lSHARE ${Lflag}

test_interpolate_amr:
	@echo test_interpolate_amr starting     >  test_interpolate_amr.diff
	@make test_interpolate_amr_exe		>> test_interpolate_amr.diff
	@echo ./test_interpolate_amr.exe	>> test_interpolate_amr.diff
	-@(./test_interpolate_amr.exe > test_interpolate_amr.diff)
	@if [ -s test_interpolate_amr.diff ]; then echo "test_interpolate_amr run..." >> test_interpolate_amr.diff; fi
	@ls -l test_interpolate_amr.diff

#######################################################################
test_interpolate_cell_amr.o: test_interpolate_cell_amr.f90
	${COMPILE.f90} ${Cflag0} test_interpolate_cell_amr.f90

test_interpolate_cell_amr_exe:
	@(cd ../src; make LIB)
	@make test_interpolate_cell_amr.o
	${LINK.f90} -o test_interpolate_cell_amr.exe test_interpolate_cell_amr.o \
		-L${LIBDIR} -lSHARE ${Lflag}

test_interpolate_cell_amr:
	@echo test_interpolate_cell_amr starting > test_interpolate_cell_amr.diff
	@make test_interpolate_cell_amr_exe >> test_interpolate_cell_amr.diff
	@echo ./test_interpolate_cell_amr.exe  >> test_interpolate_cell_amr.diff
	-@(./test_interpolate_cell_amr.exe > test_interpolate_cell_amr.diff)
	@ls -l test_interpolate_cell_amr.diff
#######################################################################
test_initial_state_exe:
	@(cd ../src; make LIB)
	make test_initial_state.o
	${LINK.f90} -o test_initial_state.exe test_initial_state.o \
		-L${LIBDIR} -lSHARE ${LflagMpi}

test_initial_state:
	@echo test_initial_state starting	 >  test_initial_state.diff
	@make test_initial_state_exe		 >> test_initial_state.diff
	@echo ${MPIRUN} ./test_initial_state_exe >> test_initial_state.diff
	-@(${MPIRUN} ./test_initial_state.exe > test_initial_state.tmp)
	-@(${DIFFNUM} -b \
	    test_initial_state.out test_initial_state.ref.gz \
						 >  test_initial_state.diff)
	@ls -l test_initial_state.diff

#######################################################################
test_random_number_exe:
	@(cd ../src; make LIB)
	@make test_random_number.o
	${LINK.f90} -o test_random_number.exe test_random_number.o \
		-L${LIBDIR} -lSHARE  ${Lflag}

test_random_number:
	@echo test_random_number starting	>  test_random_number.diff
	@make test_random_number_exe		>> test_random_number.diff
	@echo ./test_random_number.exe		>> test_random_number.diff
	-@(./test_random_number.exe > test_random_number.tmp)
	-@(diff test_random_number.tmp test_random_number.log \
						>  test_random_number.diff)
	@ls -l test_random_number.diff

#######################################################################
test_ray_trace_exe:
	@(cd ../src; make LIB)
	@make test_ray_trace.o
	${LINK.f90} -o test_ray_trace.exe test_ray_trace.o \
		-L${LIBDIR} -lSHARE ${LflagMpi}

test_ray_trace:
	@echo test_ray_trace starting		>  test_ray_trace.diff
	@make test_ray_trace_exe		>> test_ray_trace.diff
	@echo ./test_ray_trace.exe		>> test_ray_trace.diff
	-@(${SERIAL} ./test_ray_trace.exe > test_ray_trace.tmp)
	-@(${DIFFNUM} -t test_ray_trace.tmp test_ray_trace.log \
						>  test_ray_trace.diff)
	@ls -l test_ray_trace.diff

#######################################################################
test_line_extract_exe: 
	@(cd ../src; make LIB)
	@make test_line_extract.o
	${LINK.f90} -o test_line_extract.exe test_line_extract.o \
		-L${LIBDIR} -lSHARE  ${LflagMpi}

test_line_extract:
	@echo test_line_extract starting	>  test_line_extract.diff
	@make test_line_extract_exe		>> test_line_extract.diff
	@echo ${MPIRUN} ./test_line_extract.exe	>> test_line_extract.diff
	-@(${MPIRUN} ./test_line_extract.exe > test_line_extract.tmp)
	-@(diff test_line_extract.tmp test_line_extract.log \
						>  test_line_extract.diff)
	@ls -l test_line_extract.diff

#######################################################################
test_geopack_exe:
	@(cd ../src; make LIB)
	@make test_geopack.o
	${LINK.f90} -o test_geopack.exe test_geopack.o \
		-L${LIBDIR} -lSHARE ${Lflag}

test_geopack:
	@echo test_geopack starting			>  test_geopack.diff
	@make test_geopack_exe				>> test_geopack.diff
	@echo ./test_geopack.exe			>> test_geopack.diff
	-@(./test_geopack.exe > test_geopack.tmp)
	-@(${DIFFNUM} -b -r=1e-8 \
		test_geopack.tmp test_geopack.log	>  test_geopack.diff)
	@ls -l test_geopack.diff

#######################################################################
test_axes_exe:
	@(cd ../src; make LIB)
	@make test_axes.o
	${LINK.f90} -o test_axes.exe test_axes.o \
		-L${LIBDIR} -lSHARE ${LflagMpi}

test_axes: 
	@echo test_axes starting			>  test_axes.diff
	@make test_axes_exe				>> test_axes.diff
	@echo ./test_axes.exe				>> test_axes.diff
	-@(./test_axes.exe > test_axes.tmp)
	-@(${DIFFNUM} -b -a=1e-12 \
		test_axes.tmp test_axes.log 		>  test_axes.diff)
	@ls -l test_axes.diff

#######################################################################
test_planet_field_exe:
	@(cd ../src; make LIB)
	@make test_planet_field.o
	${LINK.f90} -o test_planet_field.exe test_planet_field.o \
		-L${LIBDIR} -lSHARE ${LflagMpi}

test_planet_field: 
	@echo test_planet_field starting	>  test_planet_field.diff
	@make test_planet_field_exe		>> test_planet_field.diff
	@echo ./test_planet_field.exe 		>> test_planet_field.diff
	-@(./test_planet_field.exe > test_planet_field.tmp)
	-@(${DIFFNUM} -t -r=1e-6 \
		test_planet_field.tmp test_planet_field.log \
						>  test_planet_field.diff)
	@ls -l test_planet_field.diff

#######################################################################
test_plot_file_exe:
	@(cd ../src; make LIB)
	@make test_plot_file.o
	${LINK.f90} -o test_plot_file.exe test_plot_file.o \
		-L${LIBDIR} -lSHARE ${LflagMpi}

test_plot_file:
	@echo test_plot_file starting	>  test_plot_file.diff
	@make test_plot_file_exe	>> test_plot_file.diff
	@echo ./test_plot_file.exe	>> test_plot_file.diff
	-@(./test_plot_file.exe > test_plot_file.tmp)
	-@(diff test_plot_file.tmp test_plot_file.log \
					>  test_plot_file.diff)
	@ls -l test_plot_file.diff

#######################################################################
test_plot_file_simple_exe:
	@(cd ../src; make PLOTFILELIB)
	@make test_plot_file_simple.o
	${LINK.f90} -o test_plot_file_simple.exe test_plot_file_simple.o \
		-L${LIBDIR} -lPLOTFILE ${Lflag}

test_plot_file_simple:
	@echo test_plot_file_simple starting >  test_plot_file_simple.diff
	@make test_plot_file_simple_exe	     >> test_plot_file_simple.diff
	@echo ./test_plot_file_simple.exe    >> test_plot_file_simple.diff
	-@(./test_plot_file_simple.exe > test_plot_file_simple.tmp)
	-@(diff test_plot_file_simple.tmp test_plot_file_simple.log \
					>  test_plot_file_simple.diff)
	@ls -l test_plot_file_simple.diff
#######################################################################
test_lookup_table_exe:
	@(cd ../src; make LIB)
	@make test_lookup_table.o
	${LINK.f90} -o test_lookup_table.exe test_lookup_table.o \
		 -L${LIBDIR} -lSHARE ${LflagMpi}

test_lookup_table:
	@echo test_lookup_table starting	>  test_lookup_table.diff
	@make test_lookup_table_exe		>> test_lookup_table.diff
	@echo ${MPIRUN} ./test_lookup_table.exe >> test_lookup_table.diff
	-@(${MPIRUN} ./test_lookup_table.exe > test_lookup_table.tmp)
	-@(diff test_lookup_table.tmp test_lookup_table.log \
						>  test_lookup_table.diff)
	@ls -l test_lookup_table.diff

#######################################################################
test_utilities_exe:
	@(cd ../src; make LIB)
	@make test_utilities.o
	${LINK.f90} -o test_utilities.exe test_utilities.o \
		-L${LIBDIR} -lSHARE ${LflagMpi}

test_utilities:
	@echo test_utilities starting		>  test_utilities.diff
	@make test_utilities_exe		>> test_utilities.diff
	@echo ./test_utilities.exe		>> test_utilities.diff
	-@(./test_utilities.exe > test_utilities.tmp)
	-@(diff -b test_utilities.tmp test_utilities.log \
						>  test_utilities.diff)
	@ls -l test_utilities.diff
	@rmdir xxx/yyy; rmdir xxx

#######################################################################
test_triangulate_exe:
	@(cd ../src; make LIB)
	@make test_triangulate.o
	${LINK.f90} -o test_triangulate.exe test_triangulate.o \
		-L${LIBDIR} -lSHARE ${Lflag}

test_triangulate:
	@echo test_triangulate starting		>  test_triangulate.diff
	@make test_triangulate_exe		>> test_triangulate.diff
	@echo ./test_triangulate_exe		>> test_triangulate.diff
	-@(./test_triangulate.exe > test_triangulate.tmp)
	-@(diff test_triangulate.tmp test_triangulate.log \
						>  test_triangulate.diff)
	@ls -l test_triangulate.diff

#######################################################################
test_process_var_name:
	@(cd ../src; make LIB)
	@if [ -d ${DIR}/GM/BATSRUS ]; then	\
		make test_process_var_name1 BATSDIR=${DIR}/GM/BATSRUS;	\
	else                                    \
		make test_process_var_name1 BATSDIR=${GMDIR};	\
	fi; 

test_process_var_name1:
	@rm -f test_process_var_name.tmp test_process_var_name_compile.tmp
	@echo test_process_var_name starting > test_process_var_name.tmp
	@(cd ../src; make LIB                > test_process_var_name_compile.tmp 2>&1)
	@cp ${BATSDIR}/src/ModExtraVariables.f90 .
	@cp ${BATSDIR}/src/ModSingleFluid.f90 .
	@make ModSingleFluid.o ModExtraVariables.o \
		>> test_process_var_name_compile.tmp 2>&1
	@for i in ${BATSDIR}/srcEquation/*.f90 ; do\
		cp -f $${i} ModEquation.f90;\
		echo working on $${i} >> test_process_var_name.tmp;\
		echo working on $${i} >> test_process_var_name_compile.tmp;\
		sleep 1;\
		make test_process_var_name_exe;\
		make test_process_var_name_run;\
	done
	@make test_process_var_name_check
	@rm -f ModEquation* ModExtraVariables* ModSingleFluid* 

test_process_var_name_exe:
	@rm -f test_process_var_name.o ModEquation.o
	@make ModEquation.o test_process_var_name.o \
		>> test_process_var_name_compile.tmp 2>&1
	@${LINK.f90} -o test_process_var_name.exe test_process_var_name.o ModEquation.o \
		-L${LIBDIR} -lSHARE ${LflagMpi}  >> test_process_var_name_compile.tmp 2>&1

test_process_var_name_run:
	-@(./test_process_var_name.exe >> test_process_var_name.tmp)
	@echo test complete >> test_process_var_name.tmp

test_process_var_name_check:
	@rm -f test_process_var_name.diff
	@(perl -ne 'print if /error/i' \
		test_process_var_name_compile.tmp test_process_var_name.tmp \
		> test_process_var_name.diff)
	@ls -l test_process_var_name.diff

#######################################################################
