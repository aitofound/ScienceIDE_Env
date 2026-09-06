include ../Makefile.def
-include Makefile.DEPEND

SEARCH_C = -I../include -I${SHAREDIR} ${AMREXSEARCH}

# source files
SRCS := \
	FleksInterface.cpp

OBJS := $(patsubst %, %.o,$(basename $(SRCS)))
OBJECTS = ${OBJS} \
	${COMPONENT}_wrapper.o

include ../Makefile.conf

FLAGC_EXTRA = -D_${COMPONENT}_COMPONENT_


${COMPONENT}_wrapper.o: ../include/FleksInterface.h \
	FleksInterface.o \
	${LIBRARYDIR}/CON_comp_info.o

LIBFLEKS = ../src/libFLEKS.a

MYLIB    = ${LIBDIR}/lib${COMPONENT}.a

DEPEND:
	@perl ${SCRIPTDIR}/depend.pl ${SEARCH_C} ${OBJS}

LIB:    ${LIBFLEKS} DEPEND
	make ${MYLIB}
	@echo
	@echo ${MYLIB} has been brought up to date.
	@echo

${MYLIB}: ${OBJECTS} ${LIBFLEKS} 
	cp -f ${LIBFLEKS} ${MYLIB}
	${AR} ${MYLIB} ${OBJECTS} 

clean:
	-@(rm -f ${OBJECTS} ${MYLIB})
	-@(rm -rf .d)

distclean: clean
