SHELL =/bin/sh

DEFAULT_TARGET : interface

include ../../Makefile.conf
include ../../Makefile.def
include makefile.cell_centered_linear_interpolation
include makefile.geopack2008
include makefile.T96
include makefile.T05 
include makefile.KMAG
include makefile.T01
include makefile.TA15
include makefile.TA16


INTERFACES=

interface :
	@cd ${SHAREDIR}; make DEPEND
	${MAKE} ${INTERFACES}
	${AR} interface.a ./*.o
	@echo "Target 'interface' is done." 
