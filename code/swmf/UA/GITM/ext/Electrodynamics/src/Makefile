
default: LIB

ifeq ($(wildcard ../build/Makefile.local),)
  $(error ../build/Makefile.local not found. Run config.sh from the Electrodynamics root to configure.)
endif
include ../build/Makefile.local

include $(DIRSFILE)
ifneq ($(wildcard $(BUILDDIR)/Makefile.conf),)
  include $(BUILDDIR)/Makefile.conf
endif

include Makefile.DEPEND

MYLIB = ../lib/libIE.a

OBJECTS = \
	ModKind.o\
	ModNumConst.o\
	ModConst.o\
	ModCharSize.o\
	ModErrors.o\
	ModNewell.o\
	ModTimeAmie.o\
	ModWeimer.o\
	ModWeimer05.o\
	hmr89.o\
	ModFtaModel.o\
	ModIHP.o\
	ModIoUnit.o\
	ModAMIE_Interface.o\
	ModIE.o\
	tools.o\
	interpret_names.o

DEPEND:
	@if [ -f "${SCRIPTDIR}/depend.pl" ]; then \
		perl ${SCRIPTDIR}/depend.pl ${SEARCH} ${LIBDIR} ${OBJECTS}; \
	elif [ -f "${SCRIPTDIR}/mkdepends" ]; then \
		perl ${SCRIPTDIR}/mkdepends . ext && mv Depends Makefile.DEPEND; \
	else \
		touch Makefile.DEPEND; \
	fi

Makefile.DEPEND: DEPEND

LIB:
	@make DIRSFILE=${DIRSFILE} BUILDDIR=${BUILDDIR} ${MYLIB}
	@echo

${MYLIB}: ${OBJECTS}
	@echo
	@echo 'creating LibIE'
	@echo
	@rm -f ${MYLIB}
	${AR} ${MYLIB} ${OBJECTS}

SHARELIB: DEPEND
	@make DIRSFILE=${DIRSFILE} BUILDDIR=${BUILDDIR} ${MYLIB}
