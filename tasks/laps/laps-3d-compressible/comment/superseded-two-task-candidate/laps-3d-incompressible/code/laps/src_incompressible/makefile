FORTRAN=mpif90

sysname=$(uname -n)
UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
	OPTIONS =  -O3 -fdefault-real-8 -I/usr/local/include -L/usr/local/lib -lfftw3
endif
ifeq ($(UNAME), Linux)
	OPTIONS =   -O3  -r8 -I/opt/fftw/3.3.4/intel/mvapich2_ib/include -L/opt/fftw/3.3.4/intel/mvapich2_ib/lib -lfftw3
endif


fftwpath=/cm/shared/apps/spack/cpu/opt/spack/linux-centos8-zen2/intel-19.1.1.217/fftw-3.3.8-aeockdon5bukmh3bmztjsfvnpmtlluyx
# OPTIONS = -O3 -r8 -I$(fftwpath)/include -L$(fftwpath)/lib -lfftw3
# fftwpath=/usr/local
OPTIONS = -O3 -r8 -I$(fftwpath)/include -L$(fftwpath)/lib -lfftw3 # -L/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib -lSystem


EXE = mhd.exe 
MAIN = mhd.f90 
SRC4 = mhdrhs.f90 restart.f90
SRC3 = fftw.f90 mhdoutput.f90 rktmod.f90 dealiasing.f90 mhdrms.f90 AEBmod.f90
SRC2 = mhdinit.f90
SRC1 = parallel.f90 

OBJMAIN = ${MAIN:.f90=.o}
OBJ1  = ${SRC1:.f90=.o}
OBJ2  = ${SRC2:.f90=.o}
OBJ3  = ${SRC3:.f90=.o}
OBJ4  = ${SRC4:.f90=.o}
OBJ   = $(OBJMAIN) $(OBJ4) $(OBJ3) $(OBJ2) $(OBJ1)

$(EXE): $(OBJ)
	$(FORTRAN) -o $(EXE) $(OBJ) $(OPTIONS)
$(OBJMAIN): $(OBJ4) $(OBJ3) $(OBJ2) $(MAIN)
	$(FORTRAN) -c $(MAIN) $(OPTIONS)
$(OBJ4): $(OBJ1) $(OBJ2) $(OBJ3) $(SRC4)
	$(FORTRAN) -c $(SRC4) $(OPTIONS)
$(OBJ3): $(OBJ1) $(OBJ2) $(SRC3)
	$(FORTRAN) -c $(SRC3) $(OPTIONS)
$(OBJ2): $(OBJ1) $(SRC2)
	$(FORTRAN) -c $(SRC2) $(OPTIONS)
$(OBJ1): $(SRC1) 
	$(FORTRAN) -c $(SRC1) $(OPTIONS)

clean:
	rm *.o *.mod








