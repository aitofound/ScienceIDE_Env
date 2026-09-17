#  Copyright (C) 2002 Regents of the University of Michigan,
#  portions used with permission 
#  For more information, see http://csem.engin.umich.edu/tools/swmf

include ../../../Makefile.def

SEARCHDIR = -I${SHAREDIR} -I${MAGNETOGRAMDIR}

include ../../../Makefile.conf

MY_LIB = ${LIBDIR}/libEMPIRICALEE.a

-include Makefile.DEPEND
-include Makefile.RULES

OBJECTS = \
	EEE_ModCommonVariables.o \
	EEE_ModMain.o \
	EEE_ModTD99.o \
	EEE_ModGL98.o \
	EEE_ModMc18.o \
	EEE_ModArch.o \
	EEE_ModGetB0.o \
	EEE_ModShearFlow.o \
	EEE_ModCms.o

OBJECTS_EXE= flux_rope_magnetogram.o eliminate_cme_br.o

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

.NOTPARALLEL:

FRM:	LIB
	make ${BINDIR}/FRMAGNETOGRAM.exe

${BINDIR}/FRMAGNETOGRAM.exe: flux_rope_magnetogram.o
	${LINK.f90} -o ${BINDIR}/FRMAGNETOGRAM.exe \
		flux_rope_magnetogram.o \
		-L${LIBDIR} -lEMPIRICALEE -lMAGNETOGRAM -lSHARE ${Lflag}

CMEBR:	LIB
	make ${BINDIR}/ELIMINATECMEBR.exe

${BINDIR}/ELIMINATECMEBR.exe: eliminate_cme_br.o
	${LINK.f90} -o ${BINDIR}/ELIMINATECMEBR.exe \
		eliminate_cme_br.o \
		-L${LIBDIR} -lEMPIRICALEE -lMAGNETOGRAM -lSHARE ${Lflag}

#######################################################################
# Setting BLESS=YES or Y will copy the solution into the reference solution
BLESS=NO

DIFFNUM = ${SCRIPTDIR}/DiffNum.pl -BLESS=${BLESS}

test_td22_exe: test_td22.o
	${LINK.f90} -o test_td22.exe test_td22.o \
		-L${LIBDIR} -lEMPIRICALEE -lSHARE ${Lflag}

test_td22_check:
	-@(${DIFFNUM} -b -r=1e-7 \
		test_fields.out test_fields.ref > test_td22.diff)
	-@(${DIFFNUM} -b -r=1e-7 \
		test_parabolic.out test_parabolic.ref >> test_td22.diff)
	-@(${DIFFNUM} -b -r=1e-7 \
		test_currents.out test_currents.ref >> test_td22.diff)
	-@(${DIFFNUM} -b -r=1e-5 \
		test_toroidal.out test_toroidal.ref >> test_td22.diff)
	@ls -l test_td22.diff

test_td22: LIB
	@echo "test_td22_compile..." >  test_td22.diff
	@make test_td22_exe	     >> test_td22.diff
	@echo "test_td22_run..."     >> test_td22.diff
	-@(rm -f test_fields.out test_parabolic.out \
		test_currents.out test_toroidal.out)
	-@(./test_td22.exe           >> test_td22.diff)
	@echo "test_td22_check..."   >> test_td22.diff
	-@(make test_td22_check)
#######################################################################

clean: cleanfiles

distclean: clean

