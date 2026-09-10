#^CFG COPYRIGHT UM
SHELL = /bin/sh

include ../Makefile.def

SEARCH_EXTRA =  

include ../Makefile.conf
-include Makefile.DEPEND

#
#	Makefile for parallel code ALTOR
#


# BLAS and LAPACK libraries

OBJECTS= \
	m01module.o \
	p100variables.o \
	p001grid.o \
	p002form.o \
	p002profiles.o \
	p003functions.o \
	p004ionization_check.o \
	p005rotate.o \
	p006particles_move.o \
	p007mask.o \
	p008kind.o \
	p102particles_initiate_foil_move.o \
	p103density.o \
	p105output_phase.o \
	p106mover.o \
	p107fields_advance.o \
	p108fields_initiate.o \
	p109output_field.o \
	p003qed.o  


#2dperiodicMPI.o

ANALYZE: Analyzer2D.o
	${FTN} -o Analyze.exe Analyzer2D.o	

############################################################################

DEPEND:
	@perl ${SCRIPTDIR}/depend.pl ${SEARCH} \
		${OBJECTS}


MY_LIB  = libALTOR.a

LIB:	DEPEND
	make ${MY_LIB}
	@echo
	@echo ${MY_LIB} has been brought up to date.
	@echo

${MY_LIB}: ${OBJECTS}
	rm -f ${MY_LIB}
	${AR} ${MY_LIB} ${OBJECTS}

# Object files not included into the library
OBJECTS_EXE = p999main.o

# Other requireed libraries
LIBSHARE  = ${LIBDIR}/libSHARE.a
LIBTIMING = ${LIBDIR}/libTIMING.a

# Libraries should be compiled first, because modules are used in main.
${OBJECTS_EXE}: ${LIBSHARE} ${LIBTIMING} ${MY_LIB}

EXE = ${PCDIR}/srcQED/ALTOR.exe

ALTOR:
	make ${EXE}
	@echo ' '
	@echo Program ALTOR has been brought up to date.
	@echo ' '	

${EXE}: ${OBJECTS_EXE}
	rm -rf Tmp_; mkdir Tmp_
	cd Tmp_; \
		ar -x ../${MY_LIB}; \
		ar -x ${LIBTIMING}; \
		ar -x ${LIBSHARE}
	${LINK.f90} -o ${EXE} ${OBJECTS_EXE} Tmp_/*.o \
		${LflagMpi}
	rm -rf Tmp_ 

distclean: clean
	rm -f PIC_ModSize.f90




