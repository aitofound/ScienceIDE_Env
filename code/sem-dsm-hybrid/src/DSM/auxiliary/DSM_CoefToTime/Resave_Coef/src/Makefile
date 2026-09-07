FC=ifort
FCFLAGS= -fpp #-g
CC=gcc
CFLAGS= -g -O2 

MPIFC=mpif90
MPILIBS=

FLAGS_CHECK = -O0  -std03 -implicitnone -warn truncated_source  -warn unused -warn declarations -warn alignments -warn ignore_loc -warn usage -check all -align sequence -assume byterecl -ftz -traceback -ftrapuv 

#FLAGS_NO_CHECK = -O3 -xHost -funroll-loops -unroll5  -std03 -implicitnone -warn truncated_source -warn unused -warn declarations -warn alignments -warn ignore_loc -warn usage -check bounds -align sequence -assume byterecl -ftz 
FLAGS_NO_CHECK = -O3 -xHost  -check bounds

#FOR=$(FC) $(FCFLAGS)
FOR = $(MPIFC) $(FCFLAGS) -DUSE_MPI  $(MPILIBS)

OBJS= resave_par.o allocate_array.o exit_MPI.o read.o partition_job.o  initialize.o save_coef.o resave.o

all:
	make resave

resave: $(OBJS)
	 ${FOR}  ${FCFLAGS} ${OBJS} -o resave

%.o: %.f90
	 ${FOR} -c ${FFLAGS} ${FLAGS_NO_CHECK} $<

%.o: %.f
	 ${FC} -c ${FFLAGS} $<

%.o: %.c
	 ${CC} -c ${CFLAGS} $<


clean:
	rm -f ${OBJS} resave *~
