FC=ifort
FCFLAGS= -fpp #-g
CC=gcc
CFLAGS= -g -O2 

MPIFC=mpif90
MPILIBS=

FLAGS_CHECK = -O0 -vec-report0 -std03 -implicitnone -warn truncated_source  -warn unused -warn declarations -warn alignments -warn ignore_loc -warn usage -check all -align sequence -assume byterecl -ftz -traceback -ftrapuv 

FLAGS_CHECK = -O0 -implicitnone
FLAGS_NO_CHECK = -O3 -xHost -funroll-loops -unroll5 -vec-report0 -std03 -implicitnone -warn truncated_source  -warn unused -warn declarations -warn alignments -warn ignore_loc -warn usage -check nobounds -align sequence -assume byterecl -ftz 

FLAGS_CHECK = -O0 -implicitnone
FLAGS_NO_CHECK = -O0 -implicitnone

#FOR=$(FC) $(FCFLAGS)
FOR = $(MPIFC) $(FCFLAGS) $(FLAGS_CHECK) -DUSE_MPI  $(MPILIBS)

OBJS= spectotime_par.o allocate_array.o exit_MPI.o read.o partition_job.o save_array.o initialize.o plgndr.o comp_para_givendist.o comp_disp_strain.o sum_disp_strain.o FOURL.o comp_stress.o filters.o do_fft.o spectotime.o

all:
	make spectotime

spectotime: $(OBJS)
	 ${FOR}  ${FCFLAGS} ${OBJS} -o spectotime

%.o: %.f90
	 ${FOR} -c ${FFLAGS} ${FLAGS_NO_CHECK} $<

%.o: %.f
	 ${FC} -c ${FFLAGS} $<

%.o: %.c
	 ${CC} -c ${CFLAGS} $<


clean:
	rm -f ${OBJS} spectotime *~
