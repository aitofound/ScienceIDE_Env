#
# List the default target first for stand alone mode
#
DEFAULT_TARGET = ALTOR
DEFAULT_EXE    = ${DEFAULT_TARGET}.exe

default : ${DEFAULT_TARGET}

include Makefile.def
include Makefile.conf
FF = 1.5

#
# Menu of make options
#
help:
	@echo ' '
	@echo '  You can "make" the following:'
	@echo ' '
	@echo '    <default> ${DEFAULT_TARGET} in stand alone mode, help in SWMF'
	@echo ' '
	@echo '    help          (makefile option list)'
	@echo '    install       (install ALTOR)'
	@echo ' '
	@echo '    LIB           (Component library libPC for SWMF)'
	@echo '    ALTOR         (ALTernating-ORder interpolation scheme for PIC)'
	@echo '    NOMPI         (NOMPI library for compilation without MPI)'
	@echo ' '
	@echo '    rundir        (create run directory for standalone or SWMF)'
	@echo '    rundir RUNDIR=run_test (create run directory run_test)'
	@echo ' '
	@echo '    test          (run all ALTOR tests)'
	@echo '    test_altor    (run 3D test)'
	@echo '    test_altor_2d (run 2D test)'
	@echo '    test_beam     (run beam test)'
	@echo '    test_foil     (run foil test)'
	@echo ' '
	@echo '    clean         (remove temp files like: *~ *.o *.kmo *.mod *.T *.lst core)'
	@echo '    distclean     (equivalent to ./Config.pl -uninstall)'
#	@echo '    dist          (create source distribution tar file)'


install: src/PC_ModSize.f90 srcBATL/Makefile srcBATL/BATL_size.f90

srcBATL/Makefile:
	rm -rf srcBATL; mkdir srcBATL 
	cd srcBATL_orig; cp BATL*.f90 Makefile* ../srcBATL; \
	cd ../srcBATL; ${SCRIPTDIR}/Methods.pl PC *.f90; \
	${SCRIPTDIR}/Rename.pl -w -r -common=PC *.f90; \
	perl -i -pe \
		's/user_specify_region/PC_user_specify_region/' *.f90; \
	rm -f *~

src/PC_ModSize.f90: src/PC_ModSize_orig.f90
	cp -f src/PC_ModSize_orig.f90 src/PC_ModSize.f90

srcBATL/BATL_size.f90: srcBATL/BATL_size_orig.f90
	cp -f srcBATL/BATL_size_orig.f90 srcBATL/BATL_size.f90

LIB:
	cd srcBATL; make LIB
	cd src; make LIB FF=${FF}
	cd srcInterface; make LIB

ALTOR:
	cd ${SHAREDIR}; ${MAKE} LIB
	cd ${TIMINGDIR}; ${MAKE} LIB
	cd srcBATL; make LIB
	cd src; ${MAKE} LIB  FF=${FF}
	cd src; ${MAKE} ALTOR FF=${FF}

QED:
	cd ${SHAREDIR}; ${MAKE} LIB
	cd ${TIMINGDIR}; ${MAKE} LIB
	cd srcQED; ${MAKE} LIB FF=${FF}
	cd srcQED; ${MAKE} ALTOR

NOMPI:
	cd util/NOMPI/src; make LIB

COMPONENT = PC

rundir:
	mkdir -p ${RUNDIR}/${COMPONENT}
	cd ${RUNDIR}/${COMPONENT}; \
		ln -s ${PCDIR}/Param .
	@(if [ "$(STANDALONE)" != "NO" ]; then \
		touch ${DIR}/share/JobScripts/job._TMP_${MACHINE}; \
		touch ${DIR}/share/JobScripts/_TMP_.${MACHINE}.pl; \
		cp ${DIR}/share/JobScripts/job.*${MACHINE}* ${RUNDIR}/; \
		cp ${DIR}/share/JobScripts/*.${MACHINE}.pl ${RUNDIR}/; \
		rm -f ${RUNDIR}/*_TMP_* ${DIR}/share/JobScripts/*_TMP_*; \
		cp -f Param/PARAM.DEFAULT ${RUNDIR}/PARAM.in; \
		touch ${RUNDIR}/core; chmod 444 ${RUNDIR}/core; \
		cd ${RUNDIR}; ln -s ${BINDIR}/${DEFAULT_EXE} .; \
		ln -s ${COMPONENT}/* .; \
	fi);



#
# Cleaning
#

clean: cleanfiles
	cd src; make clean
	cd srcInterface; make clean
	cd srcBATL_orig; make clean
	@(if [ -d srcBATL  ]; then cd srcBATL;  make clean; fi);
	@(if [ -d util  ]; then cd util;  make clean; fi);
	@(if [ -d share ]; then cd share; make clean; fi);

distclean: cleanfiles
	rm -f test*.diff
	./Config.pl -uninstall
	rm -rf srcBATL src/PC_ModSize.f90

allclean: 
	rm -f test*.diff
	cd src; make distclean
	rm -rf srcBATL 
	cd srcInterface; make distclean
	cd srcBATL_orig; make distclean
TESTDIR = run_test

test:
	-@(${MAKE} test_altor)
	-@(${MAKE} test_beam)
	ls -lt test*.diff

test_altor:
	@echo "test_altor_compile..." > test_altor.diff
	${MAKE} test_altor_compile
	@echo "test_altor_rundir..." >> test_altor.diff
	${MAKE} test_altor_rundir
	@echo "test_altor_run..." >> test_altor.diff
	${MAKE} test_altor_run
	@echo "test_altor_check..." >> test_altor.diff
	${MAKE} test_altor_check

test_altor_compile:
	./Config.pl -g=8,8,8,10,2,10000000
	${MAKE} 

test_altor_rundir: 
	rm -rf ${TESTDIR}
	${MAKE} rundir RUNDIR=${TESTDIR} STANDALONE=YES PCDIR=`pwd`
	cd ${TESTDIR}; cp -f Param/PARAM.TEST PARAM.in; mkdir PC/plots/

test_altor_run:
	cd ${TESTDIR}; ${MPIRUN} ./ALTOR.exe | tee -a runlog

test_altor_check:
	${SCRIPTDIR}/DiffNum.pl -t -r=1e-5 -a=3e-8 \
		${TESTDIR}/PC/plots/log_n0001.log \
		Param/TestOutput/log_noise.log \
		> test_altor.diff	
	${SCRIPTDIR}/DiffNum.pl -t -r=1e-5 -a=3e-8 \
		${TESTDIR}/PC/plots/variables.outs \
		Param/TestOutput/variables.outs.gz \
		>> test_altor.diff
	ls -l test_altor.diff

test_altor_update:
	rm -f Param/TestOutput/variables.outs.gz Param/TestOutput/log_noise.log
	cp ${TESTDIR}/PC/plots/log_n0001.log Param/TestOutput/log_noise.log
	gzip -c ${TESTDIR}/PC/plots/variables.outs>Param/TestOutput/variables.outs.gz
	${MAKE} test_altor_check

test_altor_2d:
	@echo "test_altor_2d_compile..." > test_altor_2d.diff
	${MAKE} test_altor_2d_compile
	@echo "test_altor_2d_rundir..." >> test_altor_2d.diff
	${MAKE} test_altor_2d_rundir
	@echo "test_altor_2d_run..." >> test_altor_2d.diff
	${MAKE} test_altor_2d_run
	@echo "test_altor_2d_check..." >> test_altor_2d.diff
	${MAKE} test_altor_2d_check

test_altor_2d_compile:
	./Config.pl -g=16,16,1,16,1,10000000
	${MAKE} 

test_altor_2d_rundir: 
	rm -rf ${TESTDIR}
	${MAKE} rundir RUNDIR=${TESTDIR} STANDALONE=YES PCDIR=`pwd`
	cd ${TESTDIR}; cp -f Param/PARAM.TEST_2D PARAM.in; mkdir PC/plots/

test_altor_2d_run:
	cd ${TESTDIR}; ${MPIRUN} ./ALTOR.exe | tee -a runlog

test_altor_2d_check:
	${SCRIPTDIR}/DiffNum.pl -t -r=1e-5 -a=3e-8 \
		Param/TestOutput/log_Langmuir_2d.log ${TESTDIR}/PC/plots/log_n0001.log > test_altor_2d.diff	
	ls -l test_altor_2d.diff

test_beam:
	@echo "test_beam_compile..." > test_beam.diff
	${MAKE} test_beam_compile
	@echo "test_beam_rundir..." >> test_beam.diff
	${MAKE} test_beam_rundir
	@echo "test_beam_run..." >> test_beam.diff
	${MAKE} test_beam_run
	@echo "test_beam_check..." >> test_beam.diff
	${MAKE} test_beam_check

test_beam_compile:
	./Config.pl -g=800,20,1,80,0,0
	${MAKE} 

test_beam_rundir: 
	rm -rf ${TESTDIR}
	${MAKE} rundir RUNDIR=${TESTDIR} STANDALONE=YES PCDIR=`pwd`
	cd ${TESTDIR}; cp -f Param/PARAM.in.GaussianBeam PARAM.in; mkdir PC/plots/

test_beam_run:
	cd ${TESTDIR}; ${MPIRUN} ./ALTOR.exe | tee -a runlog

test_beam_check:
	${SCRIPTDIR}/DiffNum.pl -t -r=1e-5 -a=3e-8 \
		Param/TestOutput/log_beam.log \
	${TESTDIR}/PC/plots/log_n0001.log > test_beam.diff	
	ls -l test_beam.diff

test_foil:
	@echo "test_foil_compile..." > test_foil.diff
	${MAKE} test_foil_compile
	@echo "test_foil_rundir..." >> test_foil.diff
	${MAKE} test_foil_rundir
	@echo "test_foil_run..." >> test_foil.diff
	${MAKE} test_foil_run
	@echo "test_foil_check..." >> test_foil.diff
	${MAKE} test_foil_check

test_foil_compile:
	./Config.pl -g=800,20,1,8,1,1500000
	${MAKE} 

test_foil_rundir: 
	rm -rf ${TESTDIR}
	${MAKE} rundir RUNDIR=${TESTDIR} STANDALONE=YES PCDIR=`pwd`
	cd ${TESTDIR}; cp -f Param/PARAM.FOIL PARAM.in; mkdir PC/plots/

test_foil_run:
	cd ${TESTDIR}; ${MPIRUN} ./ALTOR.exe >runlog

test_foil_check:
	${SCRIPTDIR}/DiffNum.pl -t -r=1e-5 -a=3e-8 \
		Param/TestOutput/log_foil.log \
	${TESTDIR}/PC/plots/log_n0001.log > test_foil.diff	
	ls -l test_foil.diff
