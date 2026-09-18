SHELL=/bin/sh

DEFAULT_TARGET : amps

# These definitions may be overwritten by Makefile.def
SOURCES=src
WSD=build
SPICE=nospice
OPENMP=off

#extra compiler options can be defined in Makefile.local
EXTRACOMPILEROPTIONS= 

#extra linker options specific for fortran and c++ linker
EXTRALINKEROPTIONS_F= 
EXTRALINKEROPTIONS_CPP= 

#Compiling with the CCMC's Kameleon
KAMELEON=nokameleon
BOOST=noboost
BATL=nobatl
TESTMODE=off
INTERFACE=off
CHECKMACRO=on

#Link the SWMF' shared library
LINK_SWMF_SHARED_LIB=off

#use AVX instructions in the calculations 
AVXMODE=off

#individual compiled modules 
COMPILE_ELECTRON_IMPACT=on
COMPILE_SPUTTERING=on
COMPILE_DUST=on
COMPILE_CHARGE_EXCHANGE=on
COMPILE_PHOTOLYTIC_REACTIONS=on
COMPILE_EXOSPHERE=on
COMPILE_SURFACE=on

#the list of directories where object files are located
LINK_DIRECTORY_LIST=  

include Makefile.conf
include Makefile.def


#include the local $(MAKE)file (defined the AMPS' compiling variables)  
include Makefile.local

#determine the list of the directories to be compiled
ifeq ($(COMPILE_EXOSPHERE),on)
        LINK_DIRECTORY_LIST+=models/exosphere/*.o
endif

ifeq ($(COMPILE_SURFACE),on)
        LINK_DIRECTORY_LIST+=models/surface/*.o
endif

ifeq ($(COMPILE_ELECTRON_IMPACT),on)
        LINK_DIRECTORY_LIST+=models/electron_impact/*.o
endif

ifeq ($(COMPILE_SPUTTERING),on)
        LINK_DIRECTORY_LIST+=models/sputtering/*.o
endif

ifeq ($(COMPILE_DUST),on)
        LINK_DIRECTORY_LIST+=models/dust/*.o
endif

ifeq ($(COMPILE_CHARGE_EXCHANGE),on)
        LINK_DIRECTORY_LIST+=models/charge_exchange/*.o
endif

ifeq ($(COMPILE_PHOTOLYTIC_REACTIONS),on)
        LINK_DIRECTORY_LIST+=models/photolytic_reactions/*.o
endif

#the default value of the c++ compiler flags
SEARCH_C=-DMPI_ON  -I${CWD}/${WSD}/pic -I${CWD}/${WSD}/main -I${CWD}/srcInterface -I${CWD}/${WSD}/meshAMR -I${CWD}/${WSD}/interface -I${CWD}/${WSD}/general -I${CWD}/${WSD}/models/electron_impact -I${CWD}/${WSD}/models/sputtering -I${CWD}/${WSD}/models/dust -I${CWD}/${WSD}/models/charge_exchange -I${CWD}/${WSD}/models/photolytic_reactions -I${CWD}/${WSD}/species -I${CWD}/${WSD}/models/exosphere -I${CWD}/${WSD}/models/surface -I${SPICE}/include -I${BOOST}/include -I${KAMELEON}/src -I${CWD}/utility/PostProcess -I${SHAREDIR}  -I${CWD}

SEARCH_C+=${EXTRACOMPILEROPTIONS}

SEARCH_C_GENERAL= ${EXTRACOMPILEROPTIONS} 

#define the "compile kameleon' flag only when KAMELEON is used (to exclude including of the KAMELEON headers on machimes where KAMELEON is not installed) 
ifneq ($(KAMELEON),nokameleon)   
	SEARCH_C+=-D _PIC_COMPILE__KAMELEON_ 
else
	SEARCH_C+=-D _NO_KAMELEON_CALLS_
endif

#define _NO_SPICE_CALLS_ when SPICE library is not intended to be linked
ifeq ($(SPICE),nospice)
	SEARCH_C+=-D _NO_SPICE_CALLS_
endif

ifeq ($(SPICE),)
	SEARCH_C+=-D _NO_SPICE_CALLS_
endif

#the additional argument string for the fortran compiler
SEARCH_F=
#-fdefault-real-8 

# These definitions are inherited from Makefile.def and Makefile.conf
CC=${COMPILE.mpicxx}
AMPSLINKER=${CC}

#set linker when nvcc is used to compile the code
ifeq ($(COMPILE.mpicxx),nvcc)
	AMPSLINKER=mpicxx
endif

CWD=${MYDIR}

AMPSLINKLIB= 

EXTRALINKEROPTIONS=

ifeq ($(LINK_SWMF_SHARED_LIB),on)
	AMPSLINKER=${LINK.f90}
	AMPSLINKLIB+=./share/lib/libSHARE.a
endif

#include AVX instruction flag when compile with Intel or GCC compilers
ifeq ($(AVXMODE),on)

#NVCC: pass the AVX flags to the host compiler 
ifeq ($(COMPILE.mpicxx),nvcc)
	SEARCH_C+=-Xcompiler \"
endif

#Intel compiler
ifeq ($(COMPILE.c),icc)
ifeq ($(AVXTYPE),512)
        SEARCH_C+= -xCore-AVX512 -march=skylake-avx512 
else
        SEARCH_C+= -march=core-avx2
endif

#GCC compiler
else ifeq ($(COMPILE.c),gcc)
	SEARCH_C+= -mavx2 -mfma -march=native 
ifeq ($(AVXTYPE),512)
	SEARCH_C+= -march=skylake-avx512  
endif

#PGI compiler
else ifeq ($(COMPILE.c),pgcc)
	SEARCH_C+= -mavx2 -mfma
endif

#NVCC: pass the AVX flags to the host compiler
ifeq ($(COMPILE.mpicxx),nvcc)
	SEARCH_C+=\"
endif
endif

#include BATL-related libraries for linking
ifneq ($(BATL),nobatl)	
	AMPSLINKLIB+=${BATL}/lib/libREADAMR.a
	AMPSLINKLIB+=${BATL}/lib/libSHARE.a
	AMPSLINKER=${LINK.f90}

ifeq ($(COMPILE.f90),gfortran)  
	SEARCH_F+= -J${BATL}/share/include 
else ifeq ($(COMPILE.f90),mpif90)
	SEARCH_F+= -I${BATL}/share/include
else ifeq ($(COMPILE.f90),pgf90)
	SEARCH_F+= -module ${BATL}/share/include
else ifeq  ($(COMPILE.f90),ifort)
	SEARCH_F+= -module ${BATL}/share/include
else ifeq  ($(COMPILE.f90),nagfor)
	SEARCH_F+= -I${BATL}/share/include
endif

endif

# include interface with external FORTRAN subroutines
ifeq ($(INTERFACE),on)
	AMPSLINKER=${LINK.f90}
endif 

#when OpenMP is used add the appropriate compiler flag and library
ifeq ($(OPENMP),on)

#NVCC: pass the OpenMP flags to the host compiler
ifeq ($(COMPILE.mpicxx),nvcc)
	SEARCH_C+=-Xcompiler \"
endif

ifeq ($(COMPILE.c),pgcc)
	SEARCH_C+=-mp
	SEARCH_C_GENERAL+=-mp
	AMPSLINKLIB+=-mp
else
	SEARCH_C+=-fopenmp
	SEARCH_C_GENERAL+=-fopenmp
	AMPSLINKLIB+=-fopenmp
endif

#NVCC: pass the OpenMP flags to the host compiler
ifeq ($(COMPILE.mpicxx),nvcc)
	SEARCH_C+=\"
endif
endif


# when linking mixed C/C++ and FORTRAN code mpif90 is used for linking
# certain compilers (Intel, PGI) require an additional flag
# if main() subroutine is written in C/C++

ifeq (${AMPSLINKER},${LINK.f90})

ifeq ($(COMPILE.f90),${LINK.f90})
	AMPSLINKER=mpif90  
endif

ifeq ($(COMPILE.f90),pgf90)
	AMPSLINKLIB+= -Mnomain 
else ifeq  ($(COMPILE.f90),ifort)
	AMPSLINKLIB+= -nofor-main  
else ifeq  ($(COMPILE.f90),gfortran)
	AMPSLINKLIB+= 
endif
endif

#add liker options that are apecific cor c++ cortran linkers 
ifeq (${AMPSLINKER},mpif90) 
  AMPSLINKLIB+=${EXTRALINKEROPTIONS_F} 
else 
  AMPSLINKLIB+=${EXTRALINKEROPTIONS_CPP} 
endif 


#add KAMELEON to the list of the linked libraries
ifneq ($(KAMELEON),nokameleon)
	AMPSLINKLIB+=/Users/ccmc/miniconda2/envs/kameleon/lib/ccmc/libccmc.dylib  
endif

#add SPICE to the list of the linked libraries 
ifneq ($(SPICE),nospice)
	AMPSLINKLIB+=${SPICE}/lib/cspice.a
endif

#set the nightly test flag
ifeq ($(TESTMODE),on)  
	SEARCH_C+=-D _PIC_NIGHTLY_TEST_MODE_=_PIC_MODE_ON_
endif

install:
	rm -f output
	ln -s data/output output
	@echo "AMPS installed"


distclean:
	./Config.pl -uninstall

allclean: clean
	rm -rf main ${WSD} *.input* amps table_parser Makefile.local Makefile.test \
		.amps.conf .general.conf
	rm -f output
	rm -f AMPS.pdf

rundir:
	mkdir -p ${RUNDIR}/${COMPONENT}
	cd ${RUNDIR}/${COMPONENT}; mkdir restartIN restartOUT plots; \
		ln -s ${BINDIR}/PostIDL.exe .; \
		cp    ${SCRIPTDIR}/pIDL .;

EXE=amps

LIB_AMPS = ${WSD}/libAMPS.a

PDF:
	cd doc; make PDF

logger:
	${CC} -g ${SEARCH_C} utility/logger.cpp -o logger 
clean:
	rm -rf ${LIB_AMPS} ${WSD}
	@(if [ -d srcInterface ]; then cd srcInterface; $(MAKE) clean; fi);
	cd doc; $(MAKE) clean

tar:
	cd ../pic-tower/sources/general; rm -f *.o *.a
	cd ../pic-tower/sources/dsmc; rm -f *.o *.a
	tar -cvf sources.tar sources

${WSD}:
	./ampsConfig.pl -input ${InputFileAMPS} -no-compile 

ifeq ($(CHECKMACRO),on)
	./utility/CheckMacro.pl ${WSD} -in-place
endif

ifeq ($(COMPILE.mpicxx),nvcc)
	cd ${WSD}/pic;../../utility/change-ext cpp cu  
	cd ${WSD}/meshAMR;../../utility/change-ext cpp cu 
	cd ${WSD}/main;../../utility/change-ext cpp cu
	cd ${WSD}/general;../../utility/change-ext cpp cu
endif

AMPSLIB = ${LIBDIR}/lib${COMPONENT}.a

LIB:
	@echo "checking $(AMPSLIB)";
	@if [ `${SCRIPTDIR}/uptodate.pl ${AMPSLIB} . -not -name Makefile.def -not -name Makefile.conf` ]; \
		then $(MAKE) AMPSLIB; \
	fi

AMPSLIB:
	$(MAKE) ${WSD}
	$(MAKE) LIB_after_build
	cd srcInterface; $(MAKE) LIB SEARCH_C="${SEARCH_C}"

LIB_after_build: 
ifeq ($(INTERFACE),on)
	cd ${WSD}/interface; $(MAKE) SEARCH_C="${SEARCH_C}" SEARCH="${SEARCH_F}" 
endif
	cd ${WSD}/general;                     $(MAKE) SEARCH_C="${SEARCH_C}" 
	cd ${WSD}/meshAMR;                     $(MAKE) SEARCH_C="${SEARCH_C}" 
	cd ${WSD}/pic;                         $(MAKE) SEARCH_C="${SEARCH_C}" SEARCH="${SEARCH_F}" 
	cd ${WSD}/species;                     $(MAKE) SEARCH_C="${SEARCH_C}"

ifeq ($(COMPILE_EXOSPHERE),on)
	cd ${WSD}/models/exosphere;            $(MAKE) SEARCH_C="${SEARCH_C}"
endif

ifeq ($(COMPILE_SURFACE),on)
	cd ${WSD}/models/surface;              $(MAKE) SEARCH_C="${SEARCH_C}"
endif

ifeq ($(COMPILE_ELECTRON_IMPACT),on)
	cd ${WSD}/models/electron_impact;      $(MAKE) SEARCH_C="${SEARCH_C}"
endif

ifeq ($(COMPILE_SPUTTERING),on)
	cd ${WSD}/models/sputtering;           $(MAKE) SEARCH_C="${SEARCH_C}"
endif

ifeq ($(COMPILE_DUST),on)
	cd ${WSD}/models/dust;                 $(MAKE) SEARCH_C="${SEARCH_C}"
endif

ifeq ($(COMPILE_CHARGE_EXCHANGE),on)
	cd ${WSD}/models/charge_exchange;      $(MAKE) SEARCH_C="${SEARCH_C}"
endif

ifeq ($(COMPILE_PHOTOLYTIC_REACTIONS),on)
	cd ${WSD}/models/photolytic_reactions; $(MAKE) SEARCH_C="${SEARCH_C}"
endif

	cd ${WSD}/main; $(MAKE) SEARCH_C="${SEARCH_C}"
	cp -f ${WSD}/main/mainlib.a ${WSD}/libAMPS.a

ifeq ($(SPICE),nospice)
	cd ${WSD}; ${AR} libAMPS.a general/*.o meshAMR/*.o pic/*.o pic/*/*.o species/*.o $(LINK_DIRECTORY_LIST) 
else
	rm -rf ${WSD}/tmpSPICE
	mkdir ${WSD}/tmpSPICE
	cp ${SPICE}/lib/cspice.a ${WSD}/tmpSPICE
	cd ${WSD}/tmpSPICE; ar -x cspice.a
	cd ${WSD}; ${AR} libAMPS.a general/*.o meshAMR/*.o pic/*.o pic/*/*.o species/*.o $(LINK_DIRECTORY_LIST) tmpSPICE/*.o
endif

ifeq ($(INTERFACE),on)
	cd ${WSD}; ${AR} libAMPS.a interface/*.o
endif

.PHONY: amps
amps:
	./ampsConfig.pl -input ${InputFileAMPS} -no-compile
	$(MAKE) $(WSD)
	$(MAKE) amps_after_build

amps_after_build: LIB_after_build 
	@rm -f amps
	cd ${WSD}/main; $(MAKE) amps SEARCH_C="${SEARCH_C}"
	$(MAKE) amps_link

amps_link:
ifeq ($(COMPILE.mpicxx),nvcc)
	nvcc -o amps-link.a -dlink ${WSD}/main/main.a ${WSD}/libAMPS.a
	mpif90 -o amps -g  amps-link.a ${WSD}/main/main.a ${WSD}/libAMPS.a -lstdc++ share/lib/libSHARE.a ${EXTRALINKEROPTIONS_F} 
else
	${AMPSLINKER} -o amps ${WSD}/main/main.a ${WSD}/libAMPS.a \
		${CPPLIB} ${AMPSLINKLIB} ${EXTRALINKEROPTIONS}
endif

share_lib:
	@mkdir -p share/lib
	@cd share/Library/src;make LIB

.PHONY: test
test:
	echo "Use make test_all to run the AMPS tests"
