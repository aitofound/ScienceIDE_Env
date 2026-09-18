SHELL =/bin/sh

DEFAULT_TARGET : lib  

include ../../Makefile.conf

MAINLIBOBJ = main_lib.o 

MAINOBJ = main.o curl_b.o grad_div_e.o test_force_link_all.o laplacian_e.o curl_curl_e.o curl_curl_metrics_debug.o 

lib : $(MAINLIBOBJ)
	ar -rc mainlib.a $(MAINLIBOBJ)
	@echo "Target 'main' is done." 

amps : $(MAINOBJ) 
	ar -rc main.a $(MAINOBJ)

