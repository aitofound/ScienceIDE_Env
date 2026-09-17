include ../Makefile.conf
include ../Makefile.def

install:
	@(if [ -d HYPRE ];     then cd HYPRE;      make install; fi);
	@(if [ -d DATAREAD ]; then cd DATAREAD/srcMagnetogram; \
	rm -f pyfits; \
	ln -s ${COMMONDIR}/Python/pyfits/ .; fi)
###	@(if [ -d FISHPAK ];   then cd FISHPAK/src;make LIB; fi);

test:
	rm -f */src*/*.diff
	cd DATAREAD/srcMagnetogram; make -j1 test
	cd EMPIRICAL/srcEE; make -j1 test_td22
	ls -l */src*/*.diff

clean:
	@(if [ -d NOMPI ];     then cd NOMPI/src;  make clean; fi)
	@(if [ -d TIMING ];    then cd TIMING;     make clean; fi)
	@(if [ -d DATAREAD ];  then cd DATAREAD;   make clean; fi)
	@(if [ -d EMPIRICAL ]; then cd EMPIRICAL;  make clean; fi)
	@(if [ -d CRASH ];     then cd CRASH;      make clean; fi)
	@(if [ -d HYPRE ];     then cd HYPRE;      make clean; fi)
	@(if [ -f AMREX/GNUmakefile ]; then cd AMREX;      make clean; fi)
	@(if [ -d FISHPAK ];   then cd FISHPAK/src;make clean; fi)

distclean:
	@(if [ -d NOMPI ];     then cd NOMPI/src;  make distclean; fi)
	@(if [ -d TIMING ];    then cd TIMING;     make distclean; fi)
	@(if [ -d DATAREAD ];  then cd DATAREAD;   make distclean; fi)
	@(if [ -d EMPIRICAL ]; then cd EMPIRICAL;  make distclean; fi)
	@(if [ -d CRASH ];     then cd CRASH;      make distclean; fi)
	@(if [ -d HYPRE ];     then cd HYPRE;      make distclean; fi)
	@(if [ -d AMREX/GNUmakefile ]; then cd AMREX;      make distclean; fi)
	@(if [ -d FISHPAK ];   then cd FISHPAK/src;make clean; fi)
	rm -f *~
