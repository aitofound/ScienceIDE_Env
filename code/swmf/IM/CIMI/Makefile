default : CIMI

include Makefile.def

install: 
	./Config.pl -EarthHO -GridDefault
	@(if [ ! -d input ];  then ln -f -s data/input  input;  fi)
	@(if [ ! -d output ]; then ln -f -s data/output output; fi)

help:
	@echo ' '
	@echo '  You can "make" the following:'
	@echo ' '
	@echo '    <default>                     CIMI'
	@echo ' '
	@echo '    help                          (makefile option list)'
	@echo '    PDF                           (doc/CIMI.pdf user manual)'
	@echo '    install                       (install BATSRUS)'
	@echo ' '
	@echo '    CIMI                          (bin/cimi.exe CIMI)'
#	@echo '    CIMI_SAMI                     (bin/cimi_sami.exe CIMI+SAMI3)'
#	@echo '    SAMI3	                 (srcSAMI3/SAMI3.exe SAMI)'
	@echo '    LIB                           (lib/libIM.a IM library)'
	@echo '    NOMPI                         (lib/NOMPI.a NOMPI library)'
	@echo ' '
	@echo '    rundir                        (run directory for CIMI)'
#	@echo '    rundir_cimi_sami              (run directory for CIMI+SAMI)'
#	@echo '    rundir_sami                   (run directory for SAMI3)'
	@echo ' '
	@echo '    test                          (perform nightly test)'
	@echo '    test_compile                  (compile nightly test)'
	@echo '    test_rundir                   (create run directory for nightly test)'
	@echo '    test_run                      (run nightly test)'
	@echo '    test_check                    (check nightly test)'
	@echo '    test_check BLESS=YES          (bless nightly test results)'
	@echo '    test                          (nightly test)'
	@echo '    test_UniformL                 (test uniform L)'
	@echo '    test_WAVES                    (test waves)'
	@echo '    test_dipole                   (test dipole)'
	@echo '    test_flux                     (test flux)'
	@echo '    test_drift                    (test drift)'
	@echo '    test_Prerun                   (test Prerun)'
	@echo '    test_Highorder                (test high order scheme)'
	@echo '    test_Highorder_Default'
	@echo '    test_all                      (all tests)'
	@echo '    test_rundir_all               (create run directory for all tests)'
	@echo '    test_check_all                (check all tests)'
	@echo ' '
	@echo '    PLASMASPHERE                  (perform plasma sphere unit test)'
	@echo '    PLASMASPHERE_compile          (compile plasma sphere test)'
	@echo '    PLASMASPHERE_rundir           (create rundir plasma sphere test)'
	@echo '    LOCALWAVE                     (perform local wave unit test)'
	@echo '    LOCALWAVE_compile             (compile local wave test)'
	@echo '    LOCALWAVE_rundir              (create rundir local wave test)'
	@echo ' '
	@echo '    INTEGRATION             		 (perform integration test)'



#
#       General Housekeeping
#

CIMI:
	rm -f ${BINDIR}/cimi.exe
	@cd ${SHAREDIR};  	make LIB
	@cd ${NOMPIDIR};	make LIB
	@cd ${TIMINGDIR}; 	make LIB 
	@cd ${EMPIRICALIEDIR};	make LIB
	@cd ${EMPIRICALGMDIR};	make LIB
	@cd ${DATAREADINDICESDIR};make LIB
	@cd srcGIMME;	make LIB
	@cd src;	make LIB
	@cd src;	make CIMI

#CIMI_SAMI:
#	@cd ${SHAREDIR};  	make LIB
#	@cd ${NOMPIDIR};	make LIB
#	@cd ${TIMINGDIR}; 	make LIB 
#	@cd ${EMPIRICALIEDIR};	make LIB
#	@cd ${EMPIRICALGMDIR};	make LIB
#	@cd ${DATAREADINDICESDIR};make LIB
#	@cd srcSAMI3;	make LIB
#	@cd src;	make LIB
#	@cd src;	make CIMI_SAMI
#
#
#SAMI3:
#	@cd ${SHAREDIR};  	make LIB
#	@cd ${TIMINGDIR}; 	make LIB 
#	@cd srcSAMI3;	make LIB
#	@cd srcSAMI3;	make SAMI3

NOMPI:
	cd util/NOMPI/src; $(MAKE) LIB

LIB:
	cd srcGIMME; make LIB
	cd src; make LIB
	cd srcInterface; make LIB

TESTDIR = run_test

test:
	@echo "test_compile..." > test_cimi.diff
	make   test_compile
	@echo "test_rundir..." >> test_cimi.diff
	make   test_rundir
	@echo "test_run..."    >> test_cimi.diff
	make   test_run
	@echo "test_check..."  >> test_cimi.diff
	make   test_check

test_all:
	@echo "test_compile..." > test_cimi.diff
	make   test_compile
	@echo "test_rundir..." >> test_cimi.diff
	make   test_rundir_all
	@echo "test_run..."    >> test_cimi.diff
	make   test_run
	@echo "test_check..."  >> test_cimi.diff
	make   test_check_all
	@echo "test_rundir_WAVES..." >> test_cimi.diff
	make   test_rundir_WAVES
	@echo "test_run..."    >> test_cimi.diff
	make   test_run
	@echo "test_check_WAVES..."  >> test_cimi.diff
	make   test_check_WAVES
	@echo "test_rundir_dipole..." >> test_cimi.diff
	make   test_rundir_dipole
	@echo "test_run..."    >> test_cimi.diff
	make   test_run
	@echo "test_check_dipole..."  >> test_cimi.diff
	make   test_check_dipole

	@echo "test_compile_Prerun..." >> test_cimi.diff
	make   test_compile_Prerun
	@echo "test_rundir_Prerun..." >> test_cimi.diff
	make   test_rundir_Prerun
	@echo "test_run..."    >> test_cimi.diff
	make   test_run
	@echo "test_check_Prerun..."  >> test_cimi.diff
	make   test_check_Prerun
	ls -l test_*.diff

test_UniformL:
	@echo "test_compile..." > test_cimi.diff
	make   test_compile_UniformL
	@echo "test_rundir_UniformL..." >> test_cimi.diff
	make   test_rundir_UniformL
	@echo "test_run..."    >> test_cimi.diff
	make   test_run
	@echo "test_check_UniformL..."  >> test_cimi.diff
	make   test_check_UniformL
	ls -l test_*.diff

test_WAVES:
	@echo "test_compile..." > test_cimi.diff
	make   test_compile
	@echo "test_rundir_WAVES..." >> test_cimi.diff
	make   test_rundir_WAVES
	@echo "test_run..."    >> test_cimi.diff
	make   test_run
	@echo "test_check_WAVES..."  >> test_cimi.diff
	make   test_check_WAVES
	ls -l test_*.diff

test_dipole:
	@echo "test_compile..." > test_cimi.diff
	make   test_compile
	@echo "test_rundir_dipole..." >> test_cimi.diff
	make   test_rundir_dipole
	@echo "test_run..."    >> test_cimi.diff
	make   test_run
	@echo "test_check_dipole..."  >> test_cimi.diff
	make   test_check_dipole
	ls -l test_*.diff

test_flux:
	@echo "test_compile..." > test_cimi.diff
	make   test_compile
	@echo "test_rundir_flux..." >> test_cimi.diff
	make   test_rundir_flux
	@echo "test_run..."    >> test_cimi.diff
	make   test_run
	@echo "test_check_flux..."  >> test_cimi.diff
	make   test_check_flux
	ls -l test_*.diff

test_drift:
	@echo "test_compile..." > test_cimi.diff
	make   test_compile
	@echo "test_rundir_drift..." >> test_cimi.diff
	make   test_rundir_drift
	@echo "test_run..."    >> test_cimi.diff
	make   test_run
	@echo "test_check_drift..."  >> test_cimi.diff
	make   test_check_drift
	ls -l test_*.diff

test_Prerun:
	@echo "test_compile_Prerun..." > test_cimi.diff
	make   test_compile_Prerun
	@echo "test_rundir_Prerun..." >> test_cimi.diff
	make   test_rundir_Prerun
	@echo "test_run..."    >> test_cimi.diff
	make   test_run
	@echo "test_check_Prerun..."  >> test_cimi.diff
	make   test_check_Prerun
	ls -l test_*.diff

test_Highordelsr:
	@echo "test_compile..." > test_cimi.diff
	make   test_compile_UniformL
	@echo "test_rundir_Highorder..." >> test_cimi.diff
	make   test_rundir_Highorder
	@echo "test_run..."    >> test_cimi.diff
	make   test_run
	@echo "test_check_flux..."  >> test_cimi.diff
	make   test_check_flux_Highorder

test_compile:
	./Config.pl -EarthHO -GridDefault -show
	make CIMI

test_compile_Prerun:
	./Config.pl -EarthHO -GridExpanded -show
	make CIMI

test_compile_UniformL:
	./Config.pl -EarthHO -GridUniformL -show
	make CIMI

test_rundir:
	rm -rf ${TESTDIR}
	make rundir RUNDIR=${TESTDIR} STANDALONE=YES IMDIR=`pwd`
	cp input/testfiles/*.dat ${TESTDIR}/
	cp input/testfiles/PARAM.in.test.NOWAVES ${TESTDIR}/PARAM.in

test_rundir_all:
	rm -rf ${TESTDIR}
	make rundir RUNDIR=${TESTDIR} STANDALONE=YES IMDIR=`pwd`
	cp input/testfiles/*.dat ${TESTDIR}/
	cp input/testfiles/PARAM.in.test.all ${TESTDIR}/PARAM.in

test_rundir_WAVES:
	rm -rf ${TESTDIR}
	make rundir RUNDIR=${TESTDIR} STANDALONE=YES IMDIR=`pwd`
	cp input/testfiles/*.dat ${TESTDIR}/
	cp input/testfiles/PARAM.in.test.WAVES ${TESTDIR}/PARAM.in

test_rundir_UniformL:
	rm -rf ${TESTDIR}
	make rundir RUNDIR=${TESTDIR} STANDALONE=YES IMDIR=`pwd`
	cp input/testfiles/*.dat ${TESTDIR}/
	cp input/testfiles/PARAM.in.test.UniformL ${TESTDIR}/PARAM.in

test_rundir_flux:
	rm -rf ${TESTDIR}
	make rundir RUNDIR=${TESTDIR} STANDALONE=YES IMDIR=`pwd`
	cp input/testfiles/*.dat ${TESTDIR}/
	cp input/testfiles/PARAM.in.test.flux ${TESTDIR}/PARAM.in

test_rundir_drift:
	rm -rf ${TESTDIR}
	make rundir RUNDIR=${TESTDIR} STANDALONE=YES IMDIR=`pwd`
	cp input/testfiles/*.dat ${TESTDIR}/
	cp input/testfiles/PARAM.in.test.drift ${TESTDIR}/PARAM.in

test_rundir_dipole:
	rm -rf ${TESTDIR}
	make rundir RUNDIR=${TESTDIR} STANDALONE=YES IMDIR=`pwd`
	cp input/testfiles/*.dat ${TESTDIR}/
	cp input/testfiles/PARAM.in.test.dipole ${TESTDIR}/PARAM.in

test_rundir_Prerun:
	rm -rf ${TESTDIR}
	make rundir RUNDIR=${TESTDIR} STANDALONE=YES IMDIR=`pwd`
	cp input/testfiles/imf.dat.Prerun ${TESTDIR}/imf.dat
	cp input/testfiles/Indices.dat.Prerun ${TESTDIR}/Indices.dat
	cp input/testfiles/Prerun/* ${TESTDIR}/IM/.
	cp input/testfiles/PARAM.in.test.Prerun ${TESTDIR}/PARAM.in

test_rundir_Highorder:
	rm -rf ${TESTDIR}
	make rundir RUNDIR=${TESTDIR} STANDALONE=YES IMDIR=`pwd`
	cp input/testfiles/*.dat ${TESTDIR}/
	cp input/testfiles/PARAM.in.test.HighOrder ${TESTDIR}/PARAM.in
	cp input/gaussian_test.fin ${TESTDIR}/IM/quiet_e.fin
	cp input/gaussian_test.fin ${TESTDIR}/IM/quiet_h.fin
	cp input/gaussian_test.fin ${TESTDIR}/IM/quiet_o.fin

test_rundir_DiagDiff:
	rm -rf ${TESTDIR}
	make rundir RUNDIR=${TESTDIR} STANDALONE=YES IMDIR=`pwd`
	cp input/testfiles/*.dat ${TESTDIR}/
	cp input/testfiles/PARAM.in.test.DiagDiff ${TESTDIR}/PARAM.in
	cp tools/constq.pro ${TESTDIR}/

test_run:
	cd ${TESTDIR}; ${MPIRUN} ./cimi.exe | tee runlog 

# reduced set of checks for the SWMF nightly tests
test_check:
	-make test_check_eq
	ls -l test_cimi*.diff

# complete set of checks
test_check_all:
	-make test_check_flux
	-make test_check_psd
	-make test_check_eq
	-make test_check_drift
	-make test_check_log


BLESS=NO

DIFFNUM = ${SCRIPTDIR}/DiffNum.pl -BLESS=${BLESS}

test_check_WAVES:
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiFlux_n00000000_e.fls \
		output/CimiFlux_e.fls.WAVES.gz \
		> test_cimi_waves.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiPSD_n00000000_e.psd \
		output/CimiPSD_e.psd.WAVES.gz \
		>> test_cimi_waves.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CIMIeq_n00000000.outs \
		output/CIMIeq.outs.WAVES.gz \
		>> test_cimi_waves.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CIMI_n00000000.log \
		output/CIMI.log.WAVES \
		>> test_cimi_waves.diff

test_check_UniformL:
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CIMIeq_n00000000.outs \
		output/CIMIeq.outs.UniformL.gz \
		> test_cimi_UniformL.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiPSD_n00000000_h.psd \
		output/CimiPSD_h.psd.UniformL.gz \
		>> test_cimi_UniformL.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiPSD_n00000000_o.psd \
		output/CimiPSD_o.psd.UniformL.gz \
		>> test_cimi_UniformL.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiPSD_n00000000_e.psd \
		output/CimiPSD_e.psd.UniformL.gz \
		>> test_cimi_UniformL.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiFlux_n00000000_h.fls \
		output/CimiFlux_h.fls.UniformL.gz \
		>> test_cimi_UniformL.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiFlux_n00000000_o.fls \
		output/CimiFlux_o.fls.UniformL.gz \
		>> test_cimi_UniformL.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiFlux_n00000000_e.fls \
		output/CimiFlux_e.fls.UniformL.gz \
		>> test_cimi_UniformL.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiDrift_n00000000_h.vp \
		output/CimiDrift_h.vp.UniformL.gz \
		>> test_cimi_UniformL.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiDrift_n00000000_o.vp \
		output/CimiDrift_o.vp.UniformL.gz \
		>> test_cimi_UniformL.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiDrift_n00000000_e.vp \
		output/CimiDrift_e.vp.UniformL.gz \
		>> test_cimi_UniformL.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CIMI_n00000000.log \
		output/CIMI.log.UniformL \
		>> test_cimi_UniformL.diff

test_check_flux:
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiFlux_n00000000_h.fls \
		output/CimiFlux_h.fls.gz \
		> test_cimi_flux.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiFlux_n00000000_o.fls \
		output/CimiFlux_o.fls.gz \
		>> test_cimi_flux.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiFlux_n00000000_e.fls \
		output/CimiFlux_e.fls.gz \
		>> test_cimi_flux.diff

test_check_flux_Highorder:
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiFlux_n00000000_h.fls \
		output/CimiFlux_h.fls.Highorder.gz \
		> test_cimi_flux.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiFlux_n00000000_o.fls \
		output/CimiFlux_o.fls.Highorder.gz \
		>> test_cimi_flux.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiFlux_n00000000_e.fls \
		output/CimiFlux_e.fls.Highorder.gz \
		>> test_cimi_flux.diff

test_check_psd:
	-${DIFFNUM} -t -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiPSD_n00000000_h.psd \
		output/CimiPSD_h.psd.gz \
		> test_cimi_psd.diff
	-${DIFFNUM} -t -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiPSD_n00000000_o.psd \
		output/CimiPSD_o.psd.gz \
		>> test_cimi_psd.diff
	-${DIFFNUM} -t -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiPSD_n00000000_e.psd \
		output/CimiPSD_e.psd.gz \
		>> test_cimi_psd.diff

test_check_drift:
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiDrift_n00000000_h.vp \
		output/CimiDrift_h.vp.gz \
		> test_cimi_drift.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiDrift_n00000000_o.vp \
		output/CimiDrift_o.vp.gz \
		>> test_cimi_drift.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiDrift_n00000000_e.vp \
		output/CimiDrift_e.vp.gz \
		>> test_cimi_drift.diff

test_check_dipole:
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiDrift_n00000000_h.vp \
		output/CimiDrift_h.vp.dipole.gz \
		> test_cimi_dipole.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiDrift_n00000000_o.vp \
		output/CimiDrift_o.vp.dipole.gz \
		>> test_cimi_dipole.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CimiDrift_n00000000_e.vp \
		output/CimiDrift_e.vp.dipole.gz \
		>> test_cimi_dipole.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CIMI_n00000000.log \
		output/CIMI.log.dipole \
		>> test_cimi_dipole.diff

test_check_eq:
	-${DIFFNUM} -r=0.001 -a=1e-10 -B=${BLESS} \
		${TESTDIR}/IM/plots/CIMIeq_n00000000.outs \
		output/CIMIeq.outs.gz \
		> test_cimi.diff

test_check_log:
	-${DIFFNUM} -r=0.001 -a=1e-10 -B=${BLESS} \
		${TESTDIR}/IM/plots/CIMI_n00000000.log \
		output/CIMI.log \
		> test_cimi_log.diff

test_check_Prerun:
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/sat_sat01_eflux_t000060.sat \
		output/sat_sat01_eflux_t000060.sat \
		> test_cimi_Prerun.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CIMIeq_n00000000.outs \
		output/CIMIeq.outs.Prerun.gz \
		>> test_cimi_Prerun.diff
	-${DIFFNUM} -r=0.001 -a=1e-10 \
		${TESTDIR}/IM/plots/CIMI_n00000000.log \
		output/CIMI.log.Prerun \
		>> test_cimi_Prerun.diff

PDF:
	@cd doc/Tex; make PDF

clean:
	cd src; make clean
#	cd srcSAMI3; make clean
	cd srcGIMME; make clean
	cd srcInterface; make clean
	cd doc/Tex; make clean
	@(if [ -d util ];  then cd util;  make clean; fi);
	@(if [ -d share ]; then cd share; make clean; fi);

distclean:
	./Config.pl -uninstall

allclean:
	cd src; make distclean
#	cd srcSAMI3; make distclean
	cd srcInterface; make distclean
	cd doc/Tex; make distclean
	rm -f config.log *~
	@(if [ -h input ]; then rm -f input; fi)
	@(if [ -h output ]; then rm -f output; fi)

#
#       Create run directories
#
rundir:
	mkdir -p ${RUNDIR}/IM
	@(cd ${RUNDIR}; \
		if [ ! -e "EIE/README" ]; then \
			ln -s ${EMPIRICALIEDIR}/data EIE;\
		fi;)
	cd ${RUNDIR}/IM; \
		cp ${IMDIR}/input/quiet*fin . ;\
		cp ${IMDIR}/input/IndicesKpApF107.dat . ;\
		cp ${IMDIR}/input/WaveData/*dat . ;\
		mkdir plots restartIN restartOUT
	@(if [ "$(STANDALONE)" != "NO" ]; then \
		cd ${RUNDIR} ; \
		ln -s ${BINDIR}/cimi.exe .   ; \
		touch core ; chmod 444 core;\
	fi);

#rundir_cimi_sami:
#	mkdir -p ${RUNDIR}/IM
#	@(cd ${RUNDIR}; \
#		if [ ! -e "EIE/README" ]; then \
#			ln -s ${EMPIRICALIEDIR}/data EIE;\
#		fi;)
#	cd ${RUNDIR}/IM; \
#		cp ${IMDIR}/input/quiet*fin . ;\
#		cp ${IMDIR}/input/IndicesKpApF107.dat . ;\
#		cp ${IMDIR}/input/WaveData/*dat . ;\
#		mkdir plots restartIN restartOUT plotsSAMI3
#	@(if [ "$(STANDALONE)" != "NO" ]; then \
#		cp input/testfiles/*.dat ${RUNDIR}/ ;\
#		cp input/testfiles/PARAM.in.test.WAVES ${RUNDIR}/;\
#		cp srcSAMI3/sami3_mpi-1.98.namelist ${RUNDIR}/ ;\
#		cp srcSAMI3/*.inp ${RUNDIR}/ ;\
#		cd ${RUNDIR} ; \
#		ln -s ${BINDIR}/cimi_sami.exe .   ; \
#		touch core ; chmod 444 core;\
#	fi);
#
#
#rundir_sami:
#	mkdir -p ${RUNDIR}/IM
#	cd ${RUNDIR}/IM; \
#		mkdir restartIN restartOUT plotsSAMI3
#	@(if [ "$(STANDALONE)" != "NO" ]; then \
#		cp srcSAMI3/sami3_mpi-1.98.namelist ${RUNDIR}/ ;\
#		cp srcSAMI3/*.inp ${RUNDIR}/ ;\
#		ln -s srcSAMI3/sami3.exe ${RUNDIR}/SAMI3.exe ;\
#		cd ${RUNDIR} ; \
#		touch core ; chmod 444 core;\
#	fi);

########## UNIT Tests
PLASMASPHERE:
	make PLASMASPHERE_compile
	make PLASMASPHERE_rundir
	@cd ${TESTDIR};   ./unit_test_plasmasphere.exe

PLASMASPHERE_compile:
	@cd ${SHAREDIR};        make LIB
	@cd src;        make unit_test_plasmasphere_compile

PLASMASPHERE_rundir:
	rm -rf ${TESTDIR}
	@make rundir RUNDIR=${TESTDIR}
	@cd ${TESTDIR}; ln -s ${BINDIR}/unit_test_plasmasphere.exe

#PLASMASPHERE_check:
#        @echo "test_check..." >> test_plasmasphere.diff
#        -@(${DIFFNUM} -b -r=1e-8 \
#                ${TESTDIR}/plasmasphere.out \
#                data/output/plasmasphere.out \
#                > test_plasmasphere.diff)

LOCALWAVE:
	make LOCALWAVE_compile
	make LOCALWAVE_rundir
	@cd ${TESTDIR};   ./unit_test_localwave.exe

LOCALWAVE_compile:
	@cd ${SHAREDIR};        make LIB
	@cd src;        make unit_test_localwave_compile

LOCALWAVE_rundir:
	rm -rf ${TESTDIR}
	@make rundir RUNDIR=${TESTDIR}
	@cd ${TESTDIR}; ln -s ${BINDIR}/unit_test_localwave.exe

########## Integration Tests
INTEGRATION:
	rm -f ${BINDIR}/cimi.exe
	@cd ${SHAREDIR};  	make LIB
	@cd ${NOMPIDIR};	make LIB
	@cd ${TIMINGDIR}; 	make LIB 
	@cd ${EMPIRICALIEDIR};	make LIB
	@cd ${EMPIRICALGMDIR};	make LIB
	@cd ${DATAREADINDICESDIR};make LIB
	@cd srcGIMME;	make LIB
	@cd src;	make LIB_INTEGRATION
	@cd src;	make INTEGRATION
