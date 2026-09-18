
default : GITM

include Makefile.def
include Makefile.test

# Nothing should be done parallel in this Makefile 
.NOTPARALLEL:

ABDIR   = srcSphereAB
MAINDIR = src
GLDIR   = srcGlow

PLANET=Mars

src/ModSize.f90:
	cp src/ModSize.f90.orig src/ModSize.f90

install: src/ModSize.f90
	@(if [ ! -d srcData ]; then ln -s data/input srcData; fi)

help:
	@echo Makefile targets:
	@echo "help                       - this help message"
	@echo "install                    - used by Config.pl -install"
	@echo "GITM                       - compile GITM.exe"
	@echo "POST                       - compile PostProcess.exe"
	@echo "LIB                        - compile UA library for SWMF"
	@echo "test                       - should execute all tests, it does not"
	@echo "test_gitm_mars_3d          - run 3D Mars test"
	@echo "test_gitm_mars_3d_compile  - compile GITM.exe for 3D Mars test"
	@echo "test_gitm_mars_3d_rundir   - create run directory or 3D Mars test"
	@echo "test_gitm_mars_3d_run      - run 3D Mars test on 4 cores"
	@echo "test_gitm_mars_3d_check    - check 3D Mars test results"
	@echo "nompirun                   - run GITM.exe serially"
	@echo "clean                      - clean object files etc."
	@echo "allclean                   - used by Config.pl -uninstall"
	@echo "distclean                  - run Config.pl -uninstall"	

NOMPI:
	@echo "will make NOMPI"
	@echo ${NOMPIDIR}
	@cd ${NOMPIDIR}; make LIB

GITM:
	@cd ${SHAREDIR}; make LIB
	@cd $(ABDIR);    make -j1 LIB
	@cd $(EMPIRICALIEDIR);   make LIB
	@cd ${EMPIRICALUADIR};   make LIB
	@cd $(DATAREADINDICESDIR);    make LIB
	@cd $(GLDIR);	 make -j1 LIB
	@cd $(MAINDIR);  make GITM

POST:
	@cd $(MAINDIR);  make POST

MGITM = ${MYDIR}

LIB:
	cd $(ABDIR);      make LIB
	cd $(GLDIR);      make    LIBPREV=${MGITM}/${ABDIR}/libSphere.a LIBADD
	cd $(MAINDIR);    ${MAKE} LIBPREV=${MGITM}/${GLDIR}/libUPTOGL.a libGITM.a
	cd srcInterface ; ${MAKE} LIBPREV=${MGITM}/${MAINDIR}/libUA.a   LIB

nompirun:
	make GITM
	cd ${RUNDIR}; ./GITM.exe

clean:
	@cd $(ABDIR);    make clean
	@cd $(MAINDIR);  make clean
	@cd $(GLDIR);    make clean
	@cd srcInterface;make clean
	@(if [ -d share ]; then cd share; make clean; fi);
	@(if [ -d util ];  then cd util;  make clean; fi);

distclean: 
	./Config.pl -uninstall

allclean:
	@cd $(ABDIR);     make clean
	@cd $(MAINDIR);   make distclean
	@cd srcInterface; make distclean
	rm -f *~ gitinfo.txt srcData/UAM.in
#
#       Create run directories
#
rundir:
	mkdir -p ${RUNDIR}/UA
	@(cd ${RUNDIR}; \
		if [ ! -e "EIE/README" ]; then \
			ln -s ${EMPIRICALIEDIR}/data EIE;\
		fi;)
	cd ${RUNDIR}/UA; \
		mkdir restartOUT data DataIn; \
		ln -s restartOUT restartIN; \
		ln -s ${BINDIR}/PostGITM.exe .; \
		ln -s ${MYDIR}/src/pGITM .; \
		ln -s ${MYDIR}/srcData/* DataIn; rm -f DataIn/CVS;
	@(if [ "$(STANDALONE)" != "NO" ]; then \
		cd ${RUNDIR} ; \
		ln -s ${BINDIR}/GITM.exe . ; \
		cp ${SCRIPTDIR}/Restart.pl . ; \
		cp ${SCRIPTDIR}/PostProc.pl . ; \
		touch core ; chmod 444 core ; \
		ln -s UA/* .; \
		cp DataIn/UAM.in.${PLANET} UAM.in ; \
	fi);

dist:
	make distclean
	tar cvzf gitm_`date "+%y%m%d"`.tgz Makefile* Config.pl get_info.pl \
	    share util src srcData srcDoc srcGlow srcIDL srcInterface \
	    srcPython srcMake srcSphereAB srcUser Copyright

