FC=gfortran
FCFLAGS= -g -cpp -fbacktrace -fimplicit-none -ffree-line-length-none
CC=gcc
CFLAGS= -g -O2 

MPIFC=mpif90
MPILIBS=

FLAGS_NO_CHECK =

#-O3 -xHost  -check bounds -traceback

#FOR=$(FC) $(FCFLAGS)
FOR = $(MPIFC) $(FCFLAGS) $(FLAGS_NO_CHECK) -DUSE_MPI  $(MPILIBS)

OBJS= interpolate_DSMtoSEM_par.o exit_MPI.o read_input.o filters.o job_partition.o interpolate_DSMtoSEM.o initialize.o do_fft.o lagrange_interp_2d.o do_interpolation.o ifft_Greens_SEM.o cut_save_GreensFunc_SEM.o

MOD= extract_injectedwaves_par.mod
all:
	make extract_Injectedwaves

extract_Injectedwaves: $(OBJS)
	 ${FOR}  ${FCFLAGS} ${OBJS} -o extract_Injectedwaves

%.o: %.f90
	 ${FOR} -c ${FFLAGS} ${FLAGS_NO_CHECK} $<

%.o: %.f
	 ${FC} -c ${FFLAGS} $<

%.o: %.c
	 ${CC} -c ${CFLAGS} $<


clean:
	rm -f ${OBJS} ${MOD} extract_Injectedwaves *~
