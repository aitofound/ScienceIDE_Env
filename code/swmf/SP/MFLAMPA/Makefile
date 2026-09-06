#  Copyright (C) 2002 Regents of the University of Michigan,
#  portions used with permission
#  For more information, see http://csem.engin.umich.edu/tools/swmf

DEFAULT_TARGET = MFLAMPA
DEFAULT_EXE    = MFLAMPA.exe

default : ${DEFAULT_TARGET}

include Makefile.def
include Makefile.conf

# Menu of make options
help:
	@echo ' '
	@echo '  You can "make" the following:'
	@echo ' '
	@echo '    <default> ${DEFAULT_TARGET} in stand alone mode'
	@echo ' '
	@echo '    BLESS=        (update the reference outputs or just compare)'
	@echo '    help          (show makefile option list)'
	@echo '    install       (install MFLAMPA)'
	@echo ' '
	@echo '    LIB           (component library libSP for SWMF)'
	@echo '    MFLAMPA       (make MFLAMPA.exe)'
	@echo '    MPIRUN=       (specify processor number and run tests serially)'
	@echo '    NOMPI         (NOMPI library for compilation without MPI)'
	@echo ' '
	@echo '    rundir        (create run directory for standalone or SWMF)'
	@echo '    rundir RUNDIR=run_test (create run directory run_test)'
	@echo ' '
	@echo '    test          (run all tests)'
	@echo '    test_show     (show all tests)'
	@echo '    TESTDIR=      (specify test directory)'
	@echo ' '
	@echo '    clean         (remove temp files like: *~ *.o etc)'
	@echo '    distclean     (equivalent to ./Config.pl -uninstall)'

# Menu of all test details
test_show:
	@echo ' '
	@echo '  You can "make test_*" the following:'
	@echo ' '
	@echo '  In night test'
	@echo '    test_mflampa  (test M-FLAMPA with upwind scheme)'
	@echo '    test_poisson  (test time-accurate Poisson bracket scheme)'
	@echo '    test_steady   (test steady-state Poisson bracket scheme)'
	@echo ' '
	@echo '  Not in night test'
	@echo '    test_spectra  (test the spectra outputs in M-FLAMPA)'
	@echo '    test_mpi      (test when nProc > nLineAll in M-FLAMPA)'

install: src/ModSize.f90

src/ModSize.f90: src/ModSize_orig.f90
	cp -f src/ModSize_orig.f90 src/ModSize.f90

LIB:    install
	cd src;          make LIB
	cd srcInterface; make LIB

MFLAMPA:
	cd ${SHAREDIR}; ${MAKE} LIB
	cd ${EMPIRICALCRDIR}; ${MAKE} LIB
	cd ${TIMINGDIR}; ${MAKE} LIB
	cd src; ${MAKE} LIB
	cd src; ${MAKE} MFLAMPA

NOMPI:
	cd util/NOMPI/src; make LIB

COMPONENT = SP

rundir:
	mkdir -p ${RUNDIR}/SP
	cd ${RUNDIR}/SP; \
		mkdir restartIN restartOUT IO2; \
		ln -s ${SPDIR}/Param .; \
		ln -s ${DIR}/GM/BATSRUS/data/TRAJECTORY .
	@(if [ "$(STANDALONE)" != "NO" ]; then \
		touch ${DIR}/share/JobScripts/job._TMP_${MACHINE}; \
		touch ${DIR}/share/JobScripts/_TMP_.${MACHINE}.pl; \
		cp ${DIR}/share/JobScripts/job.*${MACHINE}* ${RUNDIR}/; \
		cp ${DIR}/share/JobScripts/*.${MACHINE}.pl ${RUNDIR}/; \
		rm -f ${RUNDIR}/*_TMP_* ${DIR}/share/JobScripts/*_TMP_*; \
		cp -f Param/PARAM.in.test ${RUNDIR}/PARAM.in; \
		touch ${RUNDIR}/core; chmod 444 ${RUNDIR}/core; \
		cd ${RUNDIR}; ln -s ${BINDIR}/${DEFAULT_EXE} .; \
	fi);

clean:  install
	@(if [ -r "Makefile.conf" ]; then \
		cd src; make clean; \
		cd ../srcInterface; make clean; \
	fi)

distclean:
	./Config.pl -uninstall

allclean:
	cd src; $(MAKE) distclean
	cd srcInterface; $(MAKE) distclean
	cd Doc/Tex; $(MAKE) cleanpdf

# Testing

# Moved here from share/Library as it does not belong there
# optimization fails with gcc12 + nagfor
test_poisson_bracket.o: ./src/test_poisson_bracket.f90
	${COMPILE.f90} ${Cflag0} ./src/test_poisson_bracket.f90

test_poisson_bracket_exe:
	@(cd ${SHAREDIR}; ${MAKE} LIB)
	@(cd ${EMPIRICALCRDIR}; ${MAKE} LIB)
	@(cd src; ${MAKE} LIB)
	@make test_poisson_bracket.o
	rm -rf Tmp_; mkdir Tmp_
	cp src/ModPoissonBracket.o Tmp_/.
	cd Tmp_; \
		ar -x ${LIBDIR}/libSHARE.a
	${LINK.f90} -o test_poisson.exe test_poisson_bracket.o \
		Tmp_/*.o ${Lflag}
	rm -rf Tmp_

test_poisson_bracket:
	rm -f test_poisson*.out test_dsa_*.out
	@echo test_poisson_bracket starting	>  test_poisson.diff
	@make test_poisson_bracket_exe		>> test_poisson.diff
	@echo './test_poisson.exe'		>> test_poisson.diff
	-@(./test_poisson.exe > test_poisson.diff)
	@make test_poisson_bracket_check

test_poisson_bracket_check:
	-@(${SCRIPTDIR}/DiffNum.pl -b -r=1e-6 -a=1e-7 \
		test_poisson.out ./output/test_poisson.ref.out.gz > test_poisson.diff)
	-@(${SCRIPTDIR}/DiffNum.pl -b -r=1e-6 -a=1e-7 \
		test_poisson2d.out ./output/test_poisson2d.ref.out.gz \
		>> test_poisson.diff)
	-@(${SCRIPTDIR}/DiffNum.pl -b -a=1e-7 \
		test_dsa_sa_mhd.out ./output/test_dsa_poisson.ref \
		>> test_poisson.diff)
	@ls -l test_poisson.diff

#######################################################################


TESTDIR = run_test
BLESS=NO

test:
	rm -f test*.diff
	-@(${MAKE} test_mflampa)
	-@(${MAKE} test_poisson TESTDIR=run_poisson)
	-@(${MAKE} test_steady TESTDIR=run_steady)
	ls -l test*.diff

# Same for tests without pitch angle dependency
test_compile:
	./Config.pl -g=20000
	${MAKE}

# Same for tests with pitch angle dependency
test_compile_mu:
	./Config.pl -g=20000,100,5
	${MAKE}

# Same for all tests
test_run:
	cd ${TESTDIR}; ${MPIRUN} ./MFLAMPA.exe | tee runlog

### MFLAMPA test ###
test_mflampa:
	@echo "test_mflampa_compile..." > test_mflampa.diff
	${MAKE} test_mflampa_compile
	@echo "test_mflampa_rundir..." >> test_mflampa.diff
	${MAKE} test_mflampa_rundir
	@echo "test_mflampa_run..." >> test_mflampa.diff
	${MAKE} test_mflampa_run
	@echo "test_mflampa_check..." >> test_mflampa.diff
	${MAKE} test_mflampa_check

test_mflampa_compile: test_compile

test_mflampa_rundir: 
	rm -rf ${TESTDIR}
	${MAKE} rundir RUNDIR=${TESTDIR} STANDALONE=YES SPDIR=`pwd`
	cd ${TESTDIR}; cp -f SP/Param/PARAM.in.test PARAM.in
	cd ${TESTDIR}; tar xzf ../data/input/test_mflampa/MH_data_e20120123.tgz

test_mflampa_run: test_run

test_mflampa_check:
	cat ${TESTDIR}/SP/IO2/MH_data_{*???_???,*n000006}.out \
		> ${TESTDIR}/SP/IO2/MH_data.outs
	${SCRIPTDIR}/DiffNum.pl -BLESS=${BLESS} -t -r=1e-6 -a=1e-6 \
		${TESTDIR}/SP/IO2/MH_data.outs \
		data/output/test_mflampa/MH_data.ref.gz \
		> test_mflampa.diff
	ls -l test_mflampa.diff

### Poisson test ###
test_poisson:
	@echo "test_poisson_compile..." > test_poisson.diff
	${MAKE} test_poisson_compile
	@echo "test_poisson_rundir..." >> test_poisson.diff
	${MAKE} test_poisson_rundir
	@echo "test_poisson_run..." >> test_poisson.diff
	${MAKE} test_poisson_run
	@echo "test_poisson_check..." >> test_poisson.diff
	${MAKE} test_poisson_check

test_poisson_compile: test_compile

test_poisson_rundir:
	rm -rf ${TESTDIR}
	${MAKE} rundir RUNDIR=${TESTDIR} STANDALONE=YES SPDIR=`pwd`
	cd ${TESTDIR}; cp -f SP/Param/PARAM.in.test.poisson PARAM.in
	cd ${TESTDIR}; tar xzf ../data/input/test_mflampa/MH_data_e20120123.tgz

test_poisson_run: test_run

test_poisson_check:
	cat ${TESTDIR}/SP/IO2/MH_data_{*???_???,*n000006}.out \
		> ${TESTDIR}/SP/IO2/MH_data.outs
	${SCRIPTDIR}/DiffNum.pl -BLESS=${BLESS} -t -r=1e-6 -a=1e-6 \
		${TESTDIR}/SP/IO2/MH_data.outs \
		data/output/test_mflampa/MH_poisson_data.ref.gz \
		> test_poisson.diff
	ls -l test_poisson.diff

### Steady state test ###
test_steady:
	@echo "test_steady_compile..." > test_steady.diff
	${MAKE} test_steady_compile
	@echo "test_steady_rundir..." >> test_steady.diff
	${MAKE} test_steady_rundir
	@echo "test_steady_run..." >> test_steady.diff
	${MAKE} test_steady_run
	@echo "test_steady_restart..." >> test_steady.diff
	${MAKE} test_steady_restart
	@echo "test_steady_check..." >> test_steady.diff
	${MAKE} test_steady_check

test_steady_compile: test_compile

test_steady_rundir:
	rm -rf ${TESTDIR}
	${MAKE} rundir RUNDIR=${TESTDIR} STANDALONE=YES SPDIR=`pwd`
	cd ${TESTDIR}; cp -f SP/Param/PARAM.in.test.steady.start PARAM.in
	cd ${TESTDIR}; tar xzf ../data/input/test_mflampa/MH_data_e20120123.tgz

test_steady_run: test_run

test_steady_restart:
	cd ${TESTDIR}; ${SCRIPTDIR}/Restart.pl; rm -f PARAM.in; \
	cp -f SP/Param/PARAM.in.test.steady.restart PARAM.in
	${MAKE} test_run

test_steady_check:
	cat ${TESTDIR}/SP/IO2/MH_data_*n000020.out \
		> ${TESTDIR}/SP/IO2/MH_data.outs
	${SCRIPTDIR}/DiffNum.pl -BLESS=${BLESS} -t -r=1e-6 -a=1e-6 \
		${TESTDIR}/SP/IO2/MH_data.outs \
		data/output/test_mflampa/MH_steady_data.ref.gz \
		> test_steady.diff
	ls -l test_steady.diff

### Spectra output test ###
test_spectra:
	@echo "test_spectra..." > test_spectra.diff
	${MAKE} test_spectra_compile
	@echo "test_spectra_rundir..." >> test_spectra.diff
	${MAKE} test_spectra_rundir
	@echo "test_spectra_run..." >> test_spectra.diff
	${MAKE} test_spectra_run
	@echo "test_spectra_trj_check..." >> test_spectra.diff
	${MAKE} test_spectra_trj_check
	@echo "test_spectra_check..." >> test_spectra.diff
	${MAKE} test_spectra_check

test_spectra_compile: test_compile

test_spectra_rundir:
	rm -rf ${TESTDIR}
	${MAKE} rundir RUNDIR=${TESTDIR} STANDALONE=YES SPDIR=`pwd`
	cd ${TESTDIR}; cp -f SP/Param/PARAM.in.test.spectra PARAM.in
	cd ${TESTDIR}; tar xzf ../data/input/test_mflampa/MH_data_e20120123.tgz

test_spectra_run: test_run

test_spectra_trj_check:
	cat ${TESTDIR}/SP/IO2/def_earth_*.out \
		> ${TESTDIR}/SP/IO2/Distraj_data.outs
	${SCRIPTDIR}/DiffNum.pl -BLESS=${BLESS} -t -r=1e-6 -a=1e-6 \
		${TESTDIR}/SP/IO2/Distraj_data.outs \
		data/output/test_mflampa/Distraj_spectra_data.ref.gz \
		> test_spectra.diff
	ls -l test_spectra.diff

test_spectra_check:
	cat ${TESTDIR}/SP/IO2/MH_data_{*???_???,*n000006}.out \
		> ${TESTDIR}/SP/IO2/MH_data.outs
	${SCRIPTDIR}/DiffNum.pl -BLESS=${BLESS} -t -r=1e-6 -a=1e-6 \
		${TESTDIR}/SP/IO2/MH_data.outs \
		data/output/test_mflampa/MH_spectra_data.ref.gz \
		> test_spectra.diff

	cat ${TESTDIR}/SP/IO2/Distr_def_R{*???_???,_002_*_n000006}.out \
		> ${TESTDIR}/SP/IO2/Distr_data.outs
	${SCRIPTDIR}/DiffNum.pl -BLESS=${BLESS} -t -r=1e-6 -a=1e-6 \
		${TESTDIR}/SP/IO2/Distr_data.outs \
		data/output/test_mflampa/Distr_spectra_data.ref.gz \
		> test_spectra.diff
	ls -l test_spectra.diff

### MPI test: nProc > nLineAll, take test_poisson as an example ###
### Here we use 3*1 (nLat*nLon) field lines and 8 processors (nProc)  ###
test_mpi:
	@echo "test_mpi..." > test_mpi.diff
	${MAKE} test_mpi_compile
	@echo "test_mpi_prerundir..." >> test_mpi.diff
	${MAKE} test_mpi_prerundir
	@echo "test_mpi_prerun..." >> test_mpi.diff
	${MAKE} test_mpi_prerun
	@echo "test_mpi_reference..." >> test_mpi.diff
	${MAKE} test_mpi_reference
	@echo "test_mpi_rundir..." >> test_mpi.diff
	${MAKE} test_mpi_rundir
	@echo "test_mpi_run..." >> test_mpi.diff
	${MAKE} test_mpi_run
	@echo "test_mpi_check..." >> test_mpi.diff
	${MAKE} test_mpi_check

test_mpi_compile: test_compile

test_mpi_prerundir: test_poisson_rundir

test_mpi_prerun: test_poisson_run

test_mpi_reference:
	cat ${TESTDIR}/SP/IO2/MH_data_*001_00{1,2,3}{.out,*n000006.out} \
		> ${TESTDIR}/SP/IO2/MH_data.outs
	${SCRIPTDIR}/DiffNum.pl -BLESS=YES -t -r=1e-6 -a=1e-6 \
		${TESTDIR}/SP/IO2/MH_data.outs \
		data/output/test_mflampa/MH_poisson_mpi3_data.ref.gz \
		> test_mpi.diff

test_mpi_rundir:
	rm -rf ${TESTDIR}
	${MAKE} rundir RUNDIR=${TESTDIR} STANDALONE=YES SPDIR=`pwd`
	cd ${TESTDIR}; cp -f SP/Param/PARAM.in.test.mpi PARAM.in
	cd ${TESTDIR}; tar xzf ../data/input/test_mflampa/MH_data_e20120123.tgz

test_mpi_run:
	cd ${TESTDIR}; mpiexec -np 8 ./MFLAMPA.exe | tee runlog

test_mpi_check:
	cat ${TESTDIR}/SP/IO2/MH_data_*001_00{1,2,3}{.out,*n000006.out} \
		> ${TESTDIR}/SP/IO2/MH_data.outs
	${SCRIPTDIR}/DiffNum.pl -BLESS=${BLESS} -t -r=1e-6 -a=1e-6 \
		${TESTDIR}/SP/IO2/MH_data.outs \
		data/output/test_mflampa/MH_poisson_mpi3_data.ref.gz \
		> test_mpi.diff
	ls -l test_mpi.diff
