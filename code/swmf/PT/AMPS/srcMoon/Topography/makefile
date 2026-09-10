SHELL =/bin/sh
DEFAULT_TARGET : LOLA 

LOLA:
	mpicxx -c -o lola.o  -g -I ../../srcTemp/models/exosphere -I ../../srcTemp/general -I ../../srcTemp/pic -I ../../srcTemp/meshAMR -I ../../share/Library/src -I ../../srcInterface  lola.cpp 
	mpicxx lola.o ../../srcTemp/libAMPS.a



