#  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
#  For more information, see http://csem.engin.umich.edu/tools/swmf
#BOP
#!ROUTINE: doc/Tex/Makefile - produce documentation from LaTex, XML, Protex
#!DESCRIPTION:
# This Makefile produces documentation for SWMF from Latex, PARAM.XML
include ../../../build/Makefile.doc

help:
	@echo "       Make manuals for IDL visualization"
	@echo ""
	@echo "Targets:"
	@echo ""
	@echo "PDF          - produces PDF manuals in the ../ directory"
	@echo ""
	@echo "clean        - remove temporary files"
	@echo ""
	@echo "cleanpdf     - clean + remove PDF manuals"

PDF: idl.pdf

clean:
	rm -f *.out *.aux *.log *.toc *.ind *.idx *.ilg *.dvi *.ps *~ *.protex

cleanpdf: clean
	rm -f ../idl.pdf

