default: PWOM

include Makefile.def
include Makefile.planet

help:
	@echo Makefile targets:
	@echo
	@echo 'make                        - compile PWOM.exe'
	@echo 'make LIB                    - compile libPW.a for SWMF'
	@echo 'make run                    - create run directory'
	@echo 'make test                   - test PWOM in stand alone mode'
	@echo 'make test_earth             - run Earth test only'
	@echo 'make test_saturn            - run Saturn test only'
	@echo 'make test_jupiter           - run Jupiter test only'
	@echo 'make test_restart           - run restart test for the previous test'
	@echo 'make clean                  - remove object files'
	@echo 'make distclean              - remove all files not part of CVS'
	@echo

bin:
	mkdir bin

INSTALLFILES =  src/Makefile.DEPEND \
		src/Makefile.RULES \
		srcInterface/Makefile.DEPEND\
		srcGITMREADER/Makefile.DEPEND\
		srcGITMREADER/Makefile.RULES\
		srcSTET/Makefile.DEPEND\
		srcSTET/Makefile.RULES\
		srcTWOSTREAM/Makefile.DEPEND\
		srcTWOSTREAM/Makefile.RULES


install: bin
	touch ${INSTALLFILES}

PWOM:
	cd ${SHAREDIR};           make LIB
	cd ${TIMINGDIR};          make LIB
	cd ${EMPIRICALIEDIR};     make LIB
	cd ${EMPIRICALUADIR};     make  LIB
	cd ${DATAREADINDICESDIR}; make LIB
	cd srcGITMREADER;	  make LIB
	cd srcTWOSTREAM;	  make LIB
	cd src;                   make PWOM

PWOMSTET:
	cd ${SHAREDIR};           make LIB
	cd ${TIMINGDIR};          make LIB
	cd ${EMPIRICALIEDIR};     make LIB
	cd ${EMPIRICALUADIR};     make  LIB
	cd ${DATAREADINDICESDIR}; make LIB
	cd srcSTET; 		  make LIB
	cd src;                   make PWOM

STET:
	cd ${SHAREDIR};           make LIB
	cd ${TIMINGDIR};          make LIB
	cd ${EMPIRICALIEDIR};     make LIB
	cd ${EMPIRICALUADIR};     make  LIB
	cd ${DATAREADINDICESDIR}; make LIB
	cd srcSTET;               make STET

TWOSTREAM:
	cd ${SHAREDIR};           make LIB
	cd ${TIMINGDIR};          make LIB
	cd ${EMPIRICALIEDIR};     make LIB
	cd ${EMPIRICALUADIR};     make  LIB
	cd ${DATAREADINDICESDIR}; make LIB
	cd srcTWOSTREAM;          make TWOSTREAM

GITMREADER:
	cd ${SHAREDIR};           make LIB
	cd srcGITMREADER;         make GITMREADER

serialrun: PWOM
	cd ${RUNDIR}; ${SERIAL} ./PWOM.exe

LIB:
	cd srcGITMREADER; make LIB
	cd srcTWOSTREAM; make LIB
	cd src; make LIB
	cd srcInterface; make LIB



# Default PARAM.in file name (can be overwritten)
PARAMIN = PARAM.in
TESTDIR = run_test
CODE    = PWOM

rundir:
	mkdir -p ${RUNDIR}/PW
	@(cd ${RUNDIR}; \
		if [ ! -e "EIE/README" ]; then \
			ln -s ${EMPIRICALIEDIR}/data EIE;\
		fi;)
	cd ${RUNDIR}/PW; \
		mkdir restartOUT plots; \
		rm -f restartIN; ln -s ${MYDIR}/data/input/${PLANET}/restartfiles restartIN ;\
		cp ${MYDIR}/data/input/${PLANET}/*.dat .;\
		cp ${MYDIR}/data/input/${PLANET}/*.txt .;\
		cp -r ${MYDIR}/data/IRI_DATA .;\
		cp -r ${MYDIR}/data/crossection_data/*dat .;\
		cp -r ${MYDIR}/data/nightside_fluxes/*dat .;\
		cp ${MYDIR}/data/input/*dat .
	@(if [ "$(NCIRCLE)" != "" ]; then \
		cd ${RUNDIR}/PW; \
		mkdir RESTART0; cp restartIN/restart_iline0001.dat RESTART0; \
		rm -f restartIN; ln -s ${RUNDIR}/PW/RESTART0 restartIN; \
		cd RESTART0; ${MYDIR}/Scripts/CreateRestart.pl ${NCIRCLE}; \
	fi)
	@(if [ "$(STANDALONE)" != "NO" ]; then \
		cd ${RUNDIR}; \
			ln -s ${BINDIR}/PWOM.exe .; \
			cp ${MYDIR}/input/${PLANET}/${PARAMIN} PARAM.in; \
			touch core ; chmod 444 core ; \
	fi)

rundir_gitmreader:
	rm -rf ${TESTDIR}
	mkdir -p ${TESTDIR}/PW/UAfiles
	cd ${TESTDIR}; \
		ln -s ${BINDIR}/gitm_reader.exe;\
		cp ${MYDIR}/srcGITMREADER/gitm_20110614_040000_PWOM.dat.tgz PW/UAfiles/ ;\
		cd PW/UAfiles;\
		tar xvfz gitm_20110614_040000_PWOM.dat.tgz
.NOTPARALLEL: test

test:
	-@(make test_saturn TESTDIR=run_test_saturn)
	-@(make test_restart TESTDIR=run_test_saturn)
	rm -f src/neutral_atmosphere_planet.f90
	-@(make test_earth TESTDIR=run_test_earth)

test_orig:
	-@(make test_jupiter)
	-@(make test_saturn)
	-@(make test_restart)
	rm -f src/neutral_atmosphere_planet.f90
	-@(make test_earth)
	-@(make test_restart)
	-@(make test_earth_twostream)
	-@(make test_jupiter_twostream)
	-@(make clean)
	-@(make test_earth_stet)


test_earth:	
	@echo "starting..." > test_Earth_plots.diff
	@echo "test_compile..." > test_Earth.diff
	make test_compile PLANET=Earth
	@echo "test_rundir..." >> test_Earth.diff
	make test_rundir
	@echo "test_run..." >> test_Earth.diff
	make test_run
	@echo "test_check..." >> test_Earth.diff
	make test_check

test_earth_twostream:	
	@echo "starting..." > test_Earth_twostream_plots.diff
	@echo "test_compile..." > test_Earth_twostream.diff
	make test_compile PLANET=Earth
	@echo "test_rundir..." >> test_Earth_twostream.diff
	make test_rundir PARAMIN=PARAM.in.twostream
	@echo "test_run..." >> test_Earth_twostream.diff
	make test_run
	@echo "test_check..." >> test_Earth_twostream.diff
	make test_check SEDIR=TwoStream MYTEST=_twostream

test_jupiter_twostream:	
	@echo "starting..." > test_Jupiter_twostream_plots.diff
	@echo "test_compile..." > test_Jupiter_twostream.diff
	make test_compile PLANET=Jupiter
	@echo "test_rundir..." >> test_Jupiter_twostream.diff
	make test_rundir PARAMIN=PARAM.in.twostream
	@echo "test_run..." >> test_Jupiter_twostream.diff
	make test_run
	@echo "test_check..." >> test_Jupiter_twostream.diff
	make test_check SEDIR=TwoStream MYTEST=_twostream

test_earth_stet:	
	@echo "starting..." > test_Earth_stet_plots.diff
	@echo "test_compile..." > test_Earth_stet.diff
	make test_compile PLANET=Earth CODE=PWOMSTET
	@echo "test_rundir..." >> test_Earth_stet.diff
	make test_rundir PARAMIN=PARAM.in.stet
	@echo "test_run..." >> test_Earth_stet.diff
	make test_run
	@echo "test_check..." >> test_Earth_stet.diff
	make test_check SEDIR=Stet MYTEST=_stet

test_saturn:
	@echo "starting..." > test_Saturn_plots.diff
	@echo "test_compile..." > test_Saturn.diff
	make test_compile PLANET=Saturn
	@echo "test_rundir..." >> test_Saturn.diff
	make test_rundir
	@echo "test_run..." >> test_Saturn.diff
	make test_run
	@echo "test_check..." >> test_Saturn.diff
	make test_check

test_jupiter:
	@echo "starting..." > test_Jupiter_plots.diff
	@echo "test_compile..." > test_Jupiter.diff
	make test_compile PLANET=Jupiter
	@echo "test_rundir..." >> test_Jupiter.diff
	make test_rundir
	@echo "test_run..." >> test_Jupiter.diff
	make test_run
	@echo "test_check..." >> test_Jupiter.diff
	make test_check

test_compile:
	./Config.pl -${PLANET}
	make ${CODE}

test_rundir:
	rm -rf ${TESTDIR}
	make rundir RUNDIR=${TESTDIR} STANDALONE="YES" PWDIR=`pwd`

test_run:
	cd ${TESTDIR}; ${MPIRUN} ./PWOM.exe | tee runlog

BLESS=NO

DIFFNUM = ${SCRIPTDIR}/DiffNum.pl -BLESS=${BLESS}

test_check:
	-@(${DIFFNUM} -b -r=1e-9 \
		${TESTDIR}/PW/restartOUT/restart_iline0001.dat \
		data/output/${PLANET}/${SEDIR}/restart_iline0001.dat \
		> test_${PLANET}${MYTEST}.diff)
	-@(${DIFFNUM} -b -r=1e-9 \
		${TESTDIR}/PW/restartOUT/restart_iline0002.dat \
		data/output/${PLANET}/${SEDIR}/restart_iline0002.dat \
		>> test_${PLANET}${MYTEST}.diff)
	-@(${DIFFNUM} -b -r=1e-9 \
		${TESTDIR}/PW/restartOUT/restart_iline0003.dat \
		data/output/${PLANET}/${SEDIR}/restart_iline0003.dat \
		>> test_${PLANET}${MYTEST}.diff)
	-@(${DIFFNUM} -b -r=1e-9 \
		${TESTDIR}/PW/restartOUT/restart_iline0004.dat \
		data/output/${PLANET}/${SEDIR}/restart_iline0004.dat \
		>> test_${PLANET}${MYTEST}.diff)
	-@(${DIFFNUM} -b -r=1e-9 \
		${TESTDIR}/PW/restartOUT/restart_iline0005.dat \
		data/output/${PLANET}/${SEDIR}/restart_iline0005.dat \
		>> test_${PLANET}${MYTEST}.diff)
	-@(${DIFFNUM} -b -r=1e-9 \
		${TESTDIR}/PW/restartOUT/restart_iline0006.dat \
		data/output/${PLANET}/${SEDIR}/restart_iline0006.dat \
		>> test_${PLANET}${MYTEST}.diff)
	-@(${DIFFNUM} -b -r=1e-9 \
		${TESTDIR}/PW/restartOUT/restart_iline0007.dat \
		data/output/${PLANET}/${SEDIR}/restart_iline0007.dat \
		>> test_${PLANET}${MYTEST}.diff)
	-@(${DIFFNUM} -b -r=1e-9 \
		${TESTDIR}/PW/restartOUT/restart_iline0008.dat \
		data/output/${PLANET}/${SEDIR}/restart_iline0008.dat \
		>> test_${PLANET}${MYTEST}.diff)
	-@(${DIFFNUM} -b -r=1e-7 \
		${TESTDIR}/PW/plots/north_plots_iline0001.out \
		data/output/${PLANET}/${SEDIR}/north_plots_iline0001.out \
		> test_${PLANET}${MYTEST}_plots.diff)
	ls -l *.diff

test_restart:
	@echo "test_restart_save..." > test_${PLANET}_restart.diff
	make   test_restart_save
	@echo "test_restart_read..." >> test_${PLANET}_restart.diff
	make   test_restart_read
	@echo "test_restart_check..." >> test_${PLANET}_restart.diff
	make   test_check MYTEST=_restart

test_restart_save:
	cp input/${PLANET}/PARAM.in.restartsave ${TESTDIR}/PARAM.in
	cd ${TESTDIR}/PW/; rm -f restartIN; ln -s data/input/${PLANET}/restartfiles restartIN
	cd ${TESTDIR}; ${MPIRUN} ./PWOM.exe | tee runlog_restart_save
	cd ${TESTDIR}/PW; rm -f restartIN; ln -s restartOUT restartIN
	cd ${TESTDIR}/PW; rm -rf plot_save; mv plots plots_save; mkdir plots

test_restart_read:
	cp input/${PLANET}/PARAM.in.restartread ${TESTDIR}/PARAM.in
	cd ${TESTDIR}; ${MPIRUN} ./PWOM.exe | tee runlog_restart_read
	cd ${TESTDIR}/PW; \
	rm -rf plots_read; mv plots plots_read; mv plots_save plots
	cd ${TESTDIR}/PW; \
	cat plots_read/north_plots_iline0001.out >> plots/north_plots_iline0001.out;\
	cat plots_read/north_plots_iline0002.out >> plots/north_plots_iline0002.out;\
	cat plots_read/north_plots_iline0003.out >> plots/north_plots_iline0003.out;\
	cat plots_read/north_plots_iline0004.out >> plots/north_plots_iline0004.out

PwIDL:
	cd ${SHAREDIR}; make LIB
	cd srcPostProc; make PwIDL
	@echo ' '
	@echo Program PostPwIDL has been brought up to date.
	@echo ' '

PDF:
	@cd doc/Tex; make PDF

clean:
	@touch ${INSTALLFILES}
	cd src; make clean
	cd srcSTET; make clean
	cd srcTWOSTREAM; make clean
	cd srcGITMREADER; make clean
	cd srcInterface; make clean
	cd doc/Tex; make clean
	cd srcPostProc; make clean
	@(if [ -d share ]; then cd share; make clean; fi);
	@(if [ -d util ];  then cd util;  make clean; fi);

distclean: 
	./Config.pl -uninstall

allclean:
	@touch ${INSTALLFILES}
	cd src; make distclean
	cd srcInterface; make distclean
	cd srcTWOSTREAM; make distclean
	cd srcSTET; make distclean
	cd doc/Tex; make distclean
	rm -rf config.log Makefile.planet *~ bin ${INSTALLFILES}
