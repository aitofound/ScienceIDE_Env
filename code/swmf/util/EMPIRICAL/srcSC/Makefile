#  Copyright (C) 2002 Regents of the University of Michigan,
#  portions used with permission 
#  For more information, see http://csem.engin.umich.edu/tools/swmf

include ../../../Makefile.def

SEARCHDIR = -I${SHAREDIR} -I${MAGNETOGRAMDIR}

include ../../../Makefile.conf

MY_LIB = ${LIBDIR}/libEMPIRICALSC.a

-include Makefile.DEPEND
-include Makefile.RULES

#  Redefine .cpp to .o rule with included linear alhgebra
INC_DIRS := . include/
INC_FLAGS := $(addprefix -I,$(INC_DIRS))


chromosphere.o: chromosphere.cpp
	${COMPILE.mpicxx} ${FLAGCC} $(INC_FLAGS) chromosphere.cpp -o chromosphere.o


CHROMO = chromo_interface.o chromosphere.o

OBJECTS = ${CHROMO} \
	ModTransitionRegion.o

DEPEND:
	@perl ${SCRIPTDIR}/depend.pl ${SEARCHDIR} ${OBJECTS} ${OBJECTS_EXE}

LIBSHARE:
	cd ${SHAREDIR}; make LIB

LIBMAGNETOGRAM: LIBSHARE
	cd ${MAGNETOGRAMDIR}; make LIB

LIB:	DEPEND LIBMAGNETOGRAM
	make ${MY_LIB}
	@echo
	@echo ${MY_LIB} has been brought up to date.
	@echo

${MY_LIB}: ${OBJECTS}
	rm -f ${MY_LIB}
	${AR} ${MY_LIB} ${OBJECTS}


#######################################################################
# Setting BLESS=YES or Y will copy the solution into the reference solution
BLESS=NO

DIFFNUM = ${SCRIPTDIR}/DiffNum.pl -BLESS=${BLESS}



clean: cleanfiles

distclean: clean

