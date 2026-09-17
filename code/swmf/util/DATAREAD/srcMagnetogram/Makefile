#  Copyright (C) 2002 Regents of the University of Michigan, 
#  portions used with permission 
#  For more information, see http://csem.engin.umich.edu/tools/swmf

include ../../../Makefile.def

include ../../../Makefile.conf

-include Makefile.RULES

help:
	@echo 'Targets:'
	@echo ' '
	@echo 'LIB                    - ${MY_LIB} (library for the SWMF)'
	@echo 'HARMONICS              - ${BINDIR}/HARMONICS.exe (spherical harmonics solver)'
	@echo 'POTENTIAL              - POTENTIAL.exe (serial FDIPS, Finite Difference Iterative Potential field Solver)'
	@echo 'FDIPS                  - ${BINDIR}/FDIPS.exe (parallel FDIPS)'
	@echo 'FDIPS_HYPRE            - ${BINDIR}/FDIPS.exe (parallel FDIPS using HYPRE library)'
	@echo 'HYPRE                  - HYPRE.exe (test program for HYPRE library)'
	@echo 'DIPOLE11               - DIPOLE11.exe (creates a magnetogram with a rotated dipole for tests)'
	@echo 'CONVERTHARMONICS       - CONVERTHARMONICS.exe (convert harmonics to lookup table)'
	@echo 'OPENCLOSEDBOUNDARY     - OPENCLOSEDBOUNDARY.exe (calculate open/closed area/flux)'
	@echo 'test                   - run all tests'
	@echo 'test_harmonics         - run test for HARMONICS.exe'
	@echo 'test_harmonics_check   - check test_harmonics results'
	@echo 'test_potential         - run test for POTENTIAL.exe'
	@echo 'test_potential_check   - check test_potential results'
	@echo 'test_fdips             - run test for FDIPS.exe'
	@echo 'test_fdips_check       - check test_fdips results'
	@echo 'test_fdips_hypre       - run test for FDIPS.exe with HYPRE library'
	@echo 'test_fdips_hypre_check - check test_fdips_hypre results'
	@echo 'test_fdips_wedge       - run wedge test for FDIPS.exe'
	@echo 'test_fdips_wedge_check - check test_fdips_wedge results'

libSHARE:
	cd ${SHAREDIR}; $(MAKE) LIB

MY_LIB = ${LIBDIR}/libMAGNETOGRAM.a

OBJECTS = \
	ModMagnetogram.o

LIB:
	make ${MY_LIB}
	@echo
	@echo ${MY_LIB} has been brought up to date.
	@echo

${MY_LIB}: ${OBJECTS}
	rm -f ${MY_LIB}
	${AR} ${MY_LIB} ${OBJECTS}

##########################################################

POTENTIAL: libSHARE potential_field.o
	${LINK.f90} -o POTENTIAL.exe potential_field.o \
		-L${LIBDIR} -lSHARE ${Lflag}

############## FDIPS #############

FDIPSEXE = ${BINDIR}/FDIPS.exe

FDIPS: libSHARE ${OBJECTS_FDIPS} 
	make ${FDIPSEXE}
	@echo "${FDIPSEXE} has been compiled."
	ln -sf ${FDIPSEXE} .

OBJECTS_FDIPS= \
	ModReadMagnetogram.o \
	FDIPS_module.o \
	FDIPS_nohypre.o \
	FDIPS.o

FDIPS.o:\
        FDIPS_module.o \
        ModReadMagnetogram.o

FDIPS_hypre.o:\
        FDIPS_module.o \
        ModReadMagnetogram.o

FDIPS_module.o:\
        ModReadMagnetogram.o

${FDIPSEXE}: ${OBJECTS_FDIPS}
	${LINK.f90} -o ${FDIPSEXE} ${OBJECTS_FDIPS} \
		-L${LIBDIR} -lSHARE ${LflagMpi}

############## FDIPS_HYPRE #############

FDIPS_HYPRE: libSHARE
	make ${FDIPS_HYPREEXE}
	@echo ${FDIPS_HYPREEXE} has been compiled.
	ln -sf ${FDIPS_HYPREEXE} .

FDIPS_HYPREEXE = ${BINDIR}/FDIPS_HYPRE.exe

OBJECTS_FDIPS_HYPRE= \
	ModReadMagnetogram.o \
	FDIPS_module.o \
	FDIPS_hypre.o \
	FDIPS.o

${FDIPS_HYPREEXE}: ${OBJECTS_FDIPS_HYPRE}
	${LINK.f90} -o ${FDIPS_HYPREEXE} ${OBJECTS_FDIPS_HYPRE} \
		-L${LIBDIR} -lSHARE ${LflagMpi} ${HYPRELIB}

############## HYPRE ONLY #############

HYPRE: libSHARE ${HYPRELIB}/libHYPRE.a hypre.o
	${LINK.f90} -o HYPRE.exe hypre.o \
		-L${LIBDIR} -lSHARE ${LflagMpi} ${HYPRELIB}

############## HARMONICS #############

HARMONICS: libSHARE
	make ${HARMONICSEXE}
	@echo ${HARMONICSEXE} has been compiled.
	ln -sf ${HARMONICSEXE} .

HARMONICSEXE = ${BINDIR}/HARMONICS.exe

OBJECTS_HARMONICS= \
	harmonics.o \
	ModMagHarmonics.o \
	ModReadMagnetogram.o

ModMagHarmonics.o: ModReadMagnetogram.o 
harmonics.o: ModMagHarmonics.o
ModMagnetogram.o: ModReadMagnetogram.o

${HARMONICSEXE}: ${OBJECTS_HARMONICS}
	${LINK.f90} -o ${HARMONICSEXE} ${OBJECTS_HARMONICS} \
		-L${LIBDIR} -lSHARE ${Lflag}

############## CONVERTHARMONICS #############

CONVERTHARMONICS: libSHARE
	make ${CONVERTHARMONICSEXE}
	@echo ${CONVERTHARMONICSEXE} has been compiled.
	ln -sf ${CONVERTHARMONICSEXE} .

CONVERTHARMONICSEXE = ${BINDIR}/CONVERTHARMONICS.exe

OBJECTS_CONVERTHARMONICS= \
	convert_harmonics.o \
	ModMagnetogram.o

convert_harmonics.o: ModMagnetogram.o

${CONVERTHARMONICSEXE}: ${OBJECTS_CONVERTHARMONICS}
	${LINK.f90} -o ${CONVERTHARMONICSEXE} ${OBJECTS_CONVERTHARMONICS} \
		-L${LIBDIR} -lSHARE ${Lflag}

############## OPENCLOSEDBOUNDARY #############

OPENCLOSEDBOUNDARY: libSHARE
	make ${OPENCLOSEDBOUNDARYEXE}
	@echo ${OPENCLOSEDBOUNDARYEXE} has been compiled.
	ln -sf ${OPENCLOSEDBOUNDARYEXE} .

OPENCLOSEDBOUNDARYEXE = ${BINDIR}/OPENCLOSEDBOUNDARY.exe

OBJECTS_OPENCLOSEDBOUNDARY= \
	open_closed_boundary.o \
	ModOpenClosedBoundary.o

open_closed_boundary.o: ModOpenClosedBoundary.o

${OPENCLOSEDBOUNDARYEXE}: ${OBJECTS_OPENCLOSEDBOUNDARY}
	${LINK.f90} -o ${OPENCLOSEDBOUNDARYEXE} ${OBJECTS_OPENCLOSEDBOUNDARY} \
		-L${LIBDIR} -lSHARE ${Lflag}

################ DIPOLE11 ###################

DIPOLE11:
	make DIPOLE11.exe

DIPOLE11.exe: dipole11.o
	${LINK.f90} -o DIPOLE11.exe dipole11.o \
	-L${LIBDIR} -lSHARE ${Lflag}

############### cleaning ##################

clean: cleanfiles

distclean: clean
	rm -f *.dat *.out *.log *.diff FDIPS.in HARMONICS.in pyfits

### TESTS ###

# Setting BLESS=YES or Y will copy the solution into the reference solution
BLESS=NO

DIFFNUM = ${SCRIPTDIR}/DiffNum.pl -BLESS=${BLESS}

test:
	@rm -f *.diff
	-@(make test_harmonics)
	-@(make test_potential)
	-@(make test_fdips)
	-@(make test_fdips_hypre)
	cd ${DIR}; ./Config.pl -nohypre
	-@(make test_fdips_wedge)
	@echo "All test results:"
	@ls -l *.diff

test_harmonics:
	@echo "test_harmonics_compile..." > test_harmonics.diff
	make HARMONICS
	rm -f HARMONICS.exe
	ln -s ${BINDIR}/HARMONICS.exe HARMONICS.exe
	make DIPOLE11
	./DIPOLE11.exe
	@echo "test_harmonics_run..." >> test_harmonics.diff
	perl -pe 's/#CHANGEWEAKFIELD/CHANGEWEAKFIELD/; s/\d+(\s+MaxOrder)/10$$1/' HARMONICS.in.orig \
		> HARMONICS.in
	./HARMONICS.exe | tee harmonics.log
	@echo "test_harmonics_check..." >> test_harmonics.diff
	make test_harmonics_check

test_harmonics_check:
	${DIFFNUM} -t -r=1e-5 -a=1e-12 \
		harmonics11uniform.dat \
		harmonics.ref > test_harmonics.diff
	@ls -l test_harmonics.diff

###############################################################################

test_potential:
	@echo "test_potential_compile..." > test_potential.diff
	make POTENTIAL
	make DIPOLE11
	./DIPOLE11.exe
	@echo "test_potential_run..." >> test_potential.diff
	./POTENTIAL.exe > potential.log
	@echo "test_potential_check..." >> test_potential.diff
	make test_potential_check

test_potential_check:
	${DIFFNUM} -t -r=1e-5 -a=1e-12 \
		potential.log \
		potential.ref > test_potential.diff
	@ls -l test_potential.diff

###############################################################################

test_fdips:
	@echo "test_fdips_compile..." > test_fdips.diff
	make FDIPS
	make DIPOLE11
	./DIPOLE11.exe
	cp -f FDIPS.in.orig FDIPS.in
	perl -i -pe 's/real4/ascii/' FDIPS.in
	@echo "test_fdips_run..." >> test_fdips.diff
	${MPIRUN} ./FDIPS.exe > fdips.log
	@echo "test_fdips_check..." >> test_fdips.diff
	make test_fdips_check

test_fdips_check:
	-${DIFFNUM} -t -r=1e-7 -a=2e-9 \
		fdips_field.out \
		fdips_ref.out.gz > test_fdips.diff
	-${DIFFNUM} -t -r=1e-7 -a=2e-9 \
		fdips_bxyz.out \
		fdips_bxyz_ref.out.gz >> test_fdips.diff
	@ls -l test_fdips.diff

###############################################################################

test_fdips_hypre:
	cd ${DIR}; ./Config.pl -hypre
	rm -f test_fdips_hypre.diff
	@(if [ -d ${DIR}/util/HYPRE ]; then make test_fdips_hypre_do; fi)

test_fdips_hypre_do:
	@echo "test_fdips_hypre_compile..." > test_fdips_hypre.diff
	make FDIPS_HYPRE
	make DIPOLE11
	./DIPOLE11.exe
	cp -f FDIPS.in.orig FDIPS.in
	perl -i -pe 's/real4/ascii/' FDIPS.in
	perl -i -pe 's/^BiCGSTAB/GMRES/i;s/^ILU/AMG/' FDIPS.in
	@echo "test_fdips_hypre_run..." >> test_fdips_hypre.diff
	${MPIRUN} ./FDIPS_HYPRE.exe > fdips_hypre.log
	@echo "test_fdips_hypre_check..." >> test_fdips_hypre.diff
	make test_fdips_hypre_check

test_fdips_hypre_check:
	-${DIFFNUM} -t -r=1e-7 -a=1e-9 \
		fdips_field.out \
		fdips_ref.out.gz > test_fdips_hypre.diff
	@ls -l test_fdips_hypre.diff

###############################################################################

test_fdips_wedge:
	@echo "test_fdips_wedge_compile..." > test_fdips_wedge.diff
	make FDIPS
	cp -f FDIPS.in.wedge FDIPS.in
	gunzip -c fdips_wedge_input.gz > fitsfile.dat
	@echo "test_fdips_wedge_run..." >> test_fdips_wedge.diff
	${MPIRUN} ./FDIPS.exe > fdips.log
	@echo "test_fdips_check..." >> test_fdips_wedge.diff
	make test_fdips_wedge_check

test_fdips_wedge_check:
	-${DIFFNUM} -t -r=1e-8 -a=1e-9 \
		fdips_field.out \
		fdips_wedge_ref.gz > test_fdips_wedge.diff
	@ls -l test_fdips_wedge.diff

###############################################################################
test_eeggl:
	cd ${EMPIRICALEEDIR}; make FRM
	rm -f runlog.eeggl test_eeggl.diff runidl* RunFRM
	ln -sf ${BINDIR}/FRMAGNETOGRAM.exe .
	python3 ./GLSETUP.py fitsfile.fits -CMESpeed 600 \
		-LonPosIn 333.5 -LatPosIn 22.606750850296976 \
		-LonNegIn 324.5 -LatNegIn 23.996973841730536 > runlog_eeggl
	-${DIFFNUM} -t -r=1e-7 -a=1e-9 \
		CME.in CME.ref > test_eeggl.diff
	@ls -l test_eeggl.diff
