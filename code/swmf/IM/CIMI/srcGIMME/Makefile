
include ../Makefile.def

SEARCHDIR = -I${SHAREDIR} -I${DATAREADINDICESDIR} -I${EMPIRICALUADIR} -I../srcGLOW 

include ../Makefile.conf
-include Makefile.DEPEND
-include Makefile.RULES

OBJECTS = \
	GIMME_iono_grid.o\
	GIMME_conductance.o\
	GIMME_mag_input.o\
	GIMME_plots.o\
	GIMME_electric_potential.o\
	GIMME_cimi_interface.o


DEPEND:
	@${SCRIPTDIR}/depend.pl ${SEARCHDIR} ${OBJECTS}


MY_LIB = ${LIBDIR}/libGIMME.a


LIB: DEPEND
	make ${MY_LIB}
	@echo
	@echo ${MY_LIB} has been brought up to date.
	@echo

${MY_LIB}: ${OBJECTS}
	rm -f ${MY_LIB}
	${AR} ${MY_LIB} ${OBJECTS}

#${MY_LIB}: ${LIBSE} ${OBJECTS}
#	rm -f ${MY_LIB}
#	${AR} ${MY_LIB} ${OBJECTS} 
#
# Object files not included into the library
OBJECTS_EXE = main_iono.o

# Other requireed libraries
LIBSHARE  = ${LIBDIR}/libSHARE.a
LIBTIMING = ${LIBDIR}/libTIMING.a
#LIBINDICES  = ${LIBDIR}/libINDICES.a
#LIBEMPIRICALIE   = ${LIBDIR}/libEMPIRICALIE.a


# Libraries should be compiled first, because modules are used in main
${OBJECTS_EXE}: ${LIBSHARE} ${LIBTIMING} ${MY_LIB}   

EXE = ${BINDIR}/GIMME.exe

GIMME:
	@echo ' '
	@make ${EXE}
	@echo "Program ${EXE} has been brought up to date."
	@echo ' '

${EXE}: ${OBJECTS_EXE}
	rm -rf Tmp_; mkdir Tmp_
	cd Tmp_; \
		ar -x ${MY_LIB}; \
		ar -x ${LIBTIMING}; \
		ar -x ${LIBDIR}/libINDICES.a; \
		ar -x ${LIBSHARE};\
		ar -x ${LIBDIR}/libEMPIRICALUA.a; \
		ar -x ${LIBDIR}/libSE.a
	${LINK.f90} -o ${EXE} ${OBJECTS_EXE} Tmp_/*.o ${LBLAS} ${LflagMpi}
	#rm -rf Tmp_

GIMME_orig:
	${COMPILE.f90} ${LflagMpi} -o ${EXE} main_iono.f90 ${OBJECTS} \
	-L${LIBDIR} -lEMPIRICALUA -lTIMING -lINDICES -lSE -lSHARE -lNOMPI

#######################################################################   
unit_test_conductivity_compile:
	@(make ModChem_MR.o)
	@(make ModConduct.o)
	@(make ModMsis00.o) 
	@(make calc_conductivity.o)
	${LINK.f90} -o ${BINDIR}/unit_test_conductivity.exe \
		ModChem_MR.o ModConduct.o ModMsis00.o \
		calc_conductivity.o -L${LIBDIR}  ${LflagMpi}

#######################################################################    

clean: cleanfiles

distclean: clean

