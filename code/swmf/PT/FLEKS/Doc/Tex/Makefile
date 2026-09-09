# Copyright (C) 2026 Regents of the University of Michigan,
# portions used with permission.
# For more information, see http://csem.engin.umich.edu/tools/swmf

include ../../Makefile.def

DOC_ROOT = ${DIR}
ifeq (${DOC_ROOT},)
DOC_ROOT = ${PCDIR}
endif

include ${DOC_ROOT}/share/build/Makefile.doc

.PHONY: help PDF clean cleanpdf

help:
	@echo "       Make the FLEKS parameter manual"
	@echo ""
	@echo "Targets:"
	@echo ""
	@echo "PDF          - produce ../USERMANUAL.pdf"
	@echo "clean        - remove LaTeX intermediates and PARAM.xmltex (both locations)"
	@echo "cleanpdf     - clean and remove ../USERMANUAL.pdf"

USERMANUAL.pdf: \
	USERMANUAL.tex \
	../../PARAM.xmltex

PDF: USERMANUAL.pdf

clean:
	rm -f *.aux *.log *.toc *.dvi *.ind *.idx *.ilg *~ PARAM.xmltex ../../PARAM.xmltex

cleanpdf: clean
	rm -f ../USERMANUAL.pdf
