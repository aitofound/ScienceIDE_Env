default: EXE

.PHONY: default help GITINFO EXE FLEKS install LIB CONVERTER PIDL PDF rundir \
	clean distclean allclean compile_commands

-include Makefile.conf
-include Makefile.def

help:
	@echo Makefile targets:
	@echo
	@echo 'make EXE                    - compile standalone executable'
	@echo 'make FLEKS                  - same as make EXE'
	@echo 'make LIB                    - compile libPC.a for SWMF'
	@echo 'make PIDL                   - compile PostIDL.exe for post-processing'
	@echo 'make PDF                    - build Doc/USERMANUAL.pdf from PARAM.XML'
	@echo 'make clean                  - remove object files'
	@echo 'make distclean              - remove all files not part of CVS'
	@echo

include/show_git_info.h: include/show_git_info.h.orig
	cp -f include/show_git_info.h.orig include/show_git_info.h

include/Constants.h: include/Constants.h.orig
	cp -f include/Constants.h.orig include/Constants.h 

Makefile.def Makefile.conf:
	./Config.pl -install

GITINFO: include/show_git_info.h
	${SCRIPTDIR}/gitall -r=c > include/show_git_info.h

SHARE_LIB = $(LIBDIR)/libSHARE.a

define prepare_exe
	@if [ ! -d $(SHAREDIR) ]; then \
		echo "--- Configuring FLEKS dependencies (missing $(SHAREDIR)) ---"; \
		./Config.pl -install; \
	fi
	@if [ ! -f Makefile.conf ]; then \
		echo "--- Configuring FLEKS ---"; \
		./Config.pl -install; \
	fi
	@if [ ! -f $(SHARE_LIB) ]; then \
		echo "--- Building libSHARE.a ---"; \
		$(MAKE) -C $(SHAREDIR) LIB; \
	fi
endef

EXE: include/show_git_info.h compile_commands
	+$(call prepare_exe)
	$(MAKE) GITINFO
	$(MAKE) -C src EXE
	$(MAKE) CONVERTER

FLEKS: EXE

bin:
	mkdir -p bin

install: bin include/Constants.h

LIB: bin include/Constants.h compile_commands
	@if [ ! -f $(SHARE_LIB) ]; then \
		echo ""; \
		echo "ERROR: Cannot make LIB outside a built SWMF tree."; \
		echo "       Use 'make EXE' for standalone."; \
		echo ""; \
		exit 1; \
	fi
	@if [ ! -f $(INCLDIR)/con_comp_param.mod ]; then \
		echo ""; \
		echo "ERROR: Cannot make LIB outside a built SWMF tree."; \
		echo "       Use 'make EXE' for standalone."; \
		echo ""; \
		exit 1; \
	fi
	$(MAKE) -C src LIB
	$(MAKE) -C srcInterface LIB
	$(MAKE) CONVERTER

CONVERTER: bin
	+$(call prepare_exe)
	$(MAKE) -C src CONVERTER

PIDL:
	cd ${SHAREDIR}; $(MAKE) PIDL
	@echo ' '
	@echo Program PostIDL has been brought up to date.
	@echo ' '

PDF:
	$(MAKE) -C Doc/Tex cleanpdf
	$(MAKE) -C Doc/Tex PDF

rundir:
	mkdir -p ${RUNDIR}/${COMPONENT}
	(cd ${RUNDIR}/${COMPONENT}; \
		mkdir restartIN restartOUT plots;\
		ln -s ${BINDIR}/PostIDL.exe .; \
		cp    ${SCRIPTDIR}/pIDL .)
	(cd ${RUNDIR}; \
		if [ -f ${BINDIR}/FLEKS.exe ]; then ln -s ${BINDIR}/FLEKS.exe .; fi; \
		cp    ${SCRIPTDIR}/PostProc.pl .)


clean:
	$(MAKE) -C src clean
	$(MAKE) -C srcInterface clean
	rm -rf bin/*

distclean:
	-@./Config.pl -uninstall

allclean:
	-@(cd src; $(MAKE) distclean)
	-@(cd srcInterface; $(MAKE) distclean)
	-@$(MAKE) -C Doc/Tex cleanpdf
	-@rm -rf *~ ./bin lib ${TESTDIR} include/Constants.h
	-@rm -f test*.diff

compile_commands:
	-rm -f compile_commands.json
	-python3 tools/generate_compile_commands.py
