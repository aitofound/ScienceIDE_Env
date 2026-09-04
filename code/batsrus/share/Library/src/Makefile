#  Copyright (C) 2002 Regents of the University of Michigan,
#  portions used with permission
#  For more information, see http://csem.engin.umich.edu/tools/swmf

include ../../../Makefile.def
include ../../../Makefile.conf
-include Makefile.DEPEND
-include Makefile.RULES

# These modules provide the "infrastructure" for SWMF and components.
# A component does not have to use the whole library.
# The minimum reqguirement is ModIoUnit.o

# This rule is used to regenerate ModMpiInterface.f90 from a modified
# SWMF_MPI_routines.dat
#MPIINTERFACE:

ModMpiInterfaces.f90: MpiTemplate.f90 SWMF_MPI_routines.dat
	${SCRIPTDIR}/CreateModMpi.pl

OBJECTS = \
	ModBlasLapack.o\
	ModConst.o\
	ModCoordTransform.o\
	ModCubicEquation.o\
	ModExactRS.o\
	ModFreq.o\
	ModHyperGeometric.o\
	ModInitialState.o\
	ModInterpolate.o\
	ModInterpolateAMR.o\
	ModIoUnit.o\
	ModHdf5Utils.o\
	ModKind.o\
	ModLinearAdvection.o\
	ModLinearSolver.o\
	linear_solver_wrapper.o\
	linear_solver_wrapper_c.o\
	FluidPicInterface.o\
	Writer.o\
	PlotFileIO.o\
	ModLookupTable.o\
	ModMpiInterfaces.o\
	ModMpi.o\
	ModMpiModified.o\
	ModMpiOrig.o\
	ModNumConst.o\
	ModPlanetConst.o\
	ModPlotFile.o\
	ModProcessVarName.o\
	ModRandomNumber.o\
	ModReadParam.o\
	ModSort.o\
	ModTimeConvert.o\
	ModTriangulate.o\
	ModTriangulateSpherical.o\
	ModUtility.o\
	ModUtility_c.o\
	ModGridInfo.o\
	GridInfo.o\
	CON_axes.o\
	CON_geopack.o\
	CON_line_extract.o\
	CON_planet.o\
	CON_planet_field.o\
	CON_ray_trace.o\
	CON_star.o

DEPEND:
	@perl ${SCRIPTDIR}/depend.pl ${OBJECTS} PostIDL.o

MY_LIB = ${LIBDIR}/libSHARE.a
MY_DYN_LIB = ${LIBDIR}/libSHARE.so


LIB:	DEPEND
	${MAKE} ${MY_LIB}
	@echo
	@echo ${MY_LIB} has been brought up to date.
	@echo

${MY_LIB}: ${OBJECTS}
	rm -f ${MY_LIB}
	${AR} ${MY_LIB} ${OBJECTS}

PLOTFILE_LIB = ${LIBDIR}/libPLOTFILE.a

PLOTFILELIB:
	${MAKE} ${PLOTFILE_LIB}
	@echo
	@echo ${PLOTFILE_LIB} has been brought up to date.
	@echo

${PLOTFILE_LIB}: ModPlotFileSimple.o
	rm -f ${PLOTFILE_LIB}
	${AR} ${PLOTFILE_LIB} ModPlotFileSimple.o

${BINDIR}/PostIDL.exe: PostIDL.o ${MY_LIB}
	${LINK.f90} -o ${BINDIR}/PostIDL.exe PostIDL.o -L${LIBDIR} -lSHARE ${LflagMpi} 

PIDL: DEPEND
	make ${BINDIR}/PostIDL.exe

${BINDIR}/Convert2Tec.exe: FormatConverter.o ${MY_LIB}
	${LINK.cpp} -o ${BINDIR}/Convert2Tec.exe FormatConverter.o PlotFileIO.o

2TEC: DEPEND
	make ${BINDIR}/Convert2Tec.exe

${BINDIR}/Convert2Vtk.exe: FormatConverter.o ${MY_LIB}
	${LINK.cpp} -o ${BINDIR}/Convert2Vtk.exe FormatConverter.o PlotFileIO.o

2VTK: DEPEND
	make ${BINDIR}/Convert2Vtk.exe

LIBSO: DEPEND
	${MAKE} ${MY_DYN_LIB}
	@echo
	@echo ${MY_DYN_LIB} has been brought up to date.
	@echo

${MY_DYN_LIB}: ${OBJECTS}
	rm -f ${MY_DYN_LIB}
	${LINK.f90} -shared -fPIC -o ${MY_DYN_LIB} ${OBJECTS} -lstdc++

clean: cleanfiles

distclean: clean
	rm -f ModHdf5Utils.f90 ModSpice.f90
