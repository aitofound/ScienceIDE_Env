#^CFG COPYRIGHT UM
SHELL = /bin/sh

include ../Makefile.def

INCL_EXTRA = -I../srcBATL  

include ../Makefile.conf
-include Makefile.DEPEND
-include Makefile.RULES

PIC_ModRandom.o: PIC_ModRandom.f90
	${COMPILE.f90} ${Cflag0} PIC_ModRandom.f90

#
#	Makefile for parallel code ALTOR
#


# BLAS and LAPACK libraries

OBJECTS= \
	PC_ModSize.o \
	PIC_ModProc.o \
	PIC_ModBatlInterface.o \
	PIC_ModMain.o \
	PIC_ModFormFactor_${FF}.o \
	PC_ModHybrid.o \
	PIC_ModLaserBeam.o \
	PIC_ModField.o \
	PC_ModParticleInField.o \
	PC_ModPassField.o \
	PIC_ModParticles.o \
	PC_ModPhysics.o \
	PIC_ModRandom.o \
	PIC_ModLogFile.o \
        PIC_ModOutput.o \
	PIC_set_param.o \
	PIC_methods.o


#2dperiodicMPI.o

ANALYZE: Analyzer2D.o
	${FTN} -o Analyze.exe Analyzer2D.o	

############################################################################

DEPEND:
	@perl ${SCRIPTDIR}/depend.pl ${INCL_EXTRA} \
		${OBJECTS}


MY_LIB  = libALTOR.a
LIB_BATL = ../srcBATL/libBATL.a

LIB:	DEPEND
	make ${MY_LIB}
	@echo
	@echo ${MY_LIB} has been brought up to date.
	@echo


${MY_LIB}: ${OBJECTS}
	rm -f ${MY_LIB}
	cp -f ${LIB_BATL} ${MY_LIB}
	${AR} ${MY_LIB} ${OBJECTS}

# Object files not included into the library
OBJECTS_EXE = PIC_main.o

# Other requireed libraries
LIBSHARE  = ${LIBDIR}/libSHARE.a
LIBTIMING = ${LIBDIR}/libTIMING.a

# Libraries should be compiled first, because modules are used in main.
${OBJECTS_EXE}: ${LIBSHARE} ${LIBTIMING} ${MY_LIB}

EXE = ${BINDIR}/ALTOR.exe

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

clean: cleanfiles

distclean: clean
	rm -f PC_ModSize.f90




