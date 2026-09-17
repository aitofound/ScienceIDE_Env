#  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
#  For more information, see http://csem.engin.umich.edu/tools/swmf
SHELL = /bin/sh

help:
	@echo 'Targets for the documentation of the TIMING utility:'
	@echo ' '
	@echo 'help              - this help (default target)'
	@echo ' '
	@echo 'PDF               - MAN_TIMING.pdf manual'
	@echo ' '
	@echo 'clean             - /bin/rm -f *.log *.aux *~'
	@echo 'distclean         - make clean; /bin/rm -f MAN_TIMING.pdf'

PDF: TIMING.pdf

TIMING.pdf: MAN_TIMING.tex TIMING.tex
	pdflatex MAN_TIMING.tex
	pdflatex MAN_TIMING.tex

clean:
	/bin/rm -f *.log *.aux *~

distclean: clean
	/bin/rm -f MAN_TIMING.pdf

