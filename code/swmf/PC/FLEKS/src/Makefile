-include ../Makefile.def
include ../Makefile.conf

SEARCH_C = -I../include -I${SHAREDIR} ${AMREXSEARCH}
VERBOSE_GMRES = 
FLAGC_EXTRA = -D_${COMPONENT}_COMPONENT_ ${VERBOSE_GMRES}

-include Makefile.DEPEND

LIBFLEKS = libFLEKS.a

# source files
SRCS := \
	Domain.cpp \
	ParticleTracker.cpp \
	Particles.cpp \
	TestParticles.cpp \
	LinearSolver.cpp \
	GridUtility.cpp \
	PlotWriter.cpp \
	TimeCtr.cpp \
	BC.cpp \
	WaveBC.cpp \
	FluidInterface.cpp \
	Pic.cpp \
	PicIO.cpp \
	Grid.cpp  \
	DataContainer.cpp \
	VisitWriter.cpp  \
	FleksDistributionMap.cpp \
	InitialCondition.cpp \
	ic/WaveIC.cpp \
	ic/BeamIC.cpp \
	ic/TopHatIC.cpp \
	ic/FadeevIC.cpp \
	ic/RegisterAll.cpp

OBJECTS := $(patsubst %, %.o,$(basename $(SRCS)))

# Keep converter independent of libFLEKS.a: the full archive includes PT/OH
# coupling objects that require SWMF-only OH symbols.
CONVERTER_OBJECTS = \
	Converter.o \
	DataContainer.o \
	Grid.o \
	VisitWriter.o \
	FleksDistributionMap.o

CONVERTEREXE = ../bin/converter.exe

default: EXE

EXE:
	$(MAKE) LIB
	$(MAKE) main.o
	mkdir -p ../bin
	${LINK.f90} -o ../bin/FLEKS.exe main.o \
		-L. -lFLEKS -L${LIBDIR} -lSHARE ${LflagMpi} ${NOMAINFLAG}

CONVERTER:	${CONVERTER_OBJECTS}
	${LINK.f90} -o ${CONVERTEREXE} ${CONVERTER_OBJECTS} \
		-L${LIBDIR} -lSHARE ${LflagMpi} ${NOMAINFLAG}

	@echo
	@echo ${CONVERTEREXE} has been brought up to date.
	@echo

DEPEND:
	@perl ${SCRIPTDIR}/depend.pl ${SEARCH_C} ${OBJECTS} main.o ${CONVERTER_OBJECTS}

LIB: DEPEND
	$(MAKE) ${LIBFLEKS}
	@echo
	@echo ${LIBFLEKS} has been brought up to date.
	@echo

${LIBFLEKS}:${OBJECTS}	
	${AR} ${LIBFLEKS} ${OBJECTS}

clean:
	-@(rm -f ${OBJECTS} main.o ${LIBFLEKS})
	-@(rm -rf ${CONVERTEREXE} ${CONVERTER_OBJECTS})
	-@(rm -rf .d)

distclean: clean


#FLAGCC_EXCEPTION =

#ifeq (${C_COMPILER_NAME},icc)
#	FLAGCC_EXCEPTION = -no-ip -no-vec -fno-inline
#endif

# OH-PT simulations may fail with -O2 but without -no-ip -no-vec -fno-inline
# on Pleiades (icc). It seems the failure is caused by the split_particles()
# method. The crash can not be re-produced with -O1 or -O0 or with gcc. It
# can be an icc bug.
#Particles.o: Particles.cpp
#	${COMPILE.mpicxx} ${FLAGCC} ${FLAGCC_EXCEPTION}  Particles.cpp
