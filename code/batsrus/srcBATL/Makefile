SHELL =/bin/sh

include ../Makefile.def
include ../Makefile.conf

-include Makefile.DEPEND
-include Makefile.RULES

install:

OBJECTS = \
	BATL_amr.o \
	BATL_amr_criteria.o \
	BATL_geometry.o \
	BATL_grid.o \
	BATL_high_order.o \
	BATL_interpolate_amr.o \
	BATL_lib.o  \
	BATL_mpi.o  \
	BATL_particles.o \
	BATL_pass_cell.o \
	BATL_pass_cell_gpu_parallel.o\
        BATL_pass_node.o \
        BATL_pass_face.o \
	BATL_pass_face_field.o \
	BATL_region.o \
	BATL_size.o \
	BATL_test.o \
	BATL_tree.o

ALLOBJECTS = \
	${OBJECTS} \
	BATL_c.o \
	BATL_unit_test.o \
	batl.o \
	advect_main.o \
	game_of_life.o

DEPEND:
	@perl ${SCRIPTDIR}/depend.pl ${SEARCHDIR} ${ALLOBJECTS}

BATL_size.f90: BATL_size_orig.f90
	cp -f BATL_size_orig.f90 BATL_size.f90

MY_LIB = libBATL.a
MY_DYN_LIB =  ${LIBDIR}/libBATL.so

LIB: DEPEND
	$(MAKE) ${MY_LIB}
	@echo
	@echo ${MY_LIB} has been brought up to date.
	@echo

${MY_LIB}: ${OBJECTS}
	rm -f ${MY_LIB}
	${AR} ${MY_LIB} ${OBJECTS}

LIBSO: DEPEND
	$(MAKE) ${MY_DYN_LIB}
	@echo
	@echo ${MY_DYN_LIB} has been brought up to date.
	@echo

${MY_DYN_LIB}: ${OBJECTS} BATL_c.o BATL_unit_test.o
	${LINK.f90} -shared -fPIC -o ${MY_DYN_LIB} ${OBJECTS} \
	BATL_c.o BATL_unit_test.o \
	-L${LIBDIR} -lTIMING -lSHARE ${LflagMpi}
BATL_dynamic:
	make DEPEND
	${COMPILE.f90} ${Cflag3} batl.f90
	${LINK.f90} -o ${BINDIR}/BATL.exe batl.o BATL_unit_test.o ${MY_DYN_LIB} \
	-L${LIBDIR} -lTIMING -lSHARE ${LflagMpi}

BATL:
	make DEPEND
	${MAKE} ${BINDIR}/BATL.exe

${BINDIR}/BATL.exe: batl.o BATL_unit_test.o ${OBJECTS}
	${LINK.f90} -o ${BINDIR}/BATL.exe batl.o BATL_unit_test.o ${OBJECTS} \
	-L${LIBDIR} -lTIMING -lSHARE ${LflagMpi}

ADVECT:
	make DEPEND
	$(MAKE) ${BINDIR}/ADVECT.exe

${BINDIR}/ADVECT.exe: advect_main.o ${OBJECTS}
	${LINK.f90} -o ${BINDIR}/ADVECT.exe advect_main.o ${OBJECTS} \
		-L${LIBDIR} -lTIMING -lSHARE ${LflagMpi}

GAME:
	make DEPEND
	$(MAKE) ${BINDIR}/GAME.exe

${BINDIR}/GAME.exe: game_of_life.o ${OBJECTS}
	${LINK.f90} -o ${BINDIR}/GAME.exe game_of_life.o ${OBJECTS} \
		-L${LIBDIR} -lTIMING -lSHARE ${LflagMpi}

clean: cleanfiles

distclean: clean
	rm -f BATL_size.f90

