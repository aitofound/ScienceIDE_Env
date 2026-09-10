
default : TEST

include $(DIRSFILE)
ifneq ($(wildcard $(BUILDDIR)/Makefile.conf),)
  include $(BUILDDIR)/Makefile.conf
endif

SEARCH = -I..

EXE = ../ie_test.exe

OBJECTS = \
	ModApex.o\
	apex_routines.o\
	apex_more.o\
	ModMain.o\
	output.o\
	main.o

${EXE}: ${OBJECTS}
	${LINK.f90} -o ${EXE} ${OBJECTS} -L../../lib -lIE 

TEST:
	@make ${EXE}

