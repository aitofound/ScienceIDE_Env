include ../../Makefile.def

install:

LIB:
	cd src; make LIB

rundir:
	mkdir -p ${RUNDIR}/PS/Output
	mkdir ${RUNDIR}/PS/Input
	mkdir ${RUNDIR}/PS/restartOUT
	ln -s ${PSDIR}/Input/restart_cold ${RUNDIR}/PS/restartIN

clean:
	cd src; make clean

distclean: 
	./Config.pl -uninstall

allclean:
	cd src; make distclean
	rm -f *~

test:
	@echo "PS/DGCPM has no tests"
