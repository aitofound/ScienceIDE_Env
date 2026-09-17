include ../Makefile.def

SEARCHDIR = -I${SHAREDIR}

include ../Makefile.conf

MODULES = \
	ModGitmAtmos.o

METHODS = \

LIBOBJECTS = ${MODULES} ${METHODS}

EXEOBJECTS = unit_test_gitm_reader.o ${LIBOBJECTS}

DEPEND:
	@${SCRIPTDIR}/depend.pl ${SEARCH} ${EXEOBJECTS} 


MY_LIB = ${LIBDIR}/libGITMREADER.a

LIB:	DEPEND
	make ${MY_LIB}
	@echo
	@echo ${MY_LIB} has been brought up to date.
	@echo



${MY_LIB}: ${LIBOBJECTS}
	rm -f ${MY_LIB}
	${AR} ${MY_LIB} ${LIBOBJECTS}

EXE = ${BINDIR}/gitm_reader.exe

GITMREADER:	DEPEND
	@make ${EXE}
	@echo "gitm_reader.exe has been created"

${EXE}:	${EXEOBJECTS}
	@(if [ ${OS} = "Darwin" ];         then make gitmreader_for_Darwin; \
	else                                    make gitmreader_general; fi)

gitmreader_general:
	${LINK.f90} -o ${EXE} ${EXEOBJECTS} \
		-L${LIBDIR}  -lSHARE \
		${LflagMpi}

gitmreader_for_Darwin:
	rm -rf Tmp_; mkdir Tmp_
	cd Tmp_; \
		ar -x ${LIBDIR}/libSHARE.a
	${LINK.f90} -o ${EXE} ${EXEOBJECTS} Tmp_/*.o ${LBLAS} ${LflagMpi}
	rm -rf Tmp_

clean: cleanfiles

distclean: clean

