FC=gfortran
FCFLAGS= -O #-g
CC=gcc
CFLAGS= -g -O2 

MPIFC=mpif90
MPILIBS=

#CC     = mpicc
FFLAGS = -O3 -fallow-argument-mismatch -std=legacy

FLAGS_CHECK = 

FLAGS_NO_CHECK = 

#FOR=$(FC) $(FCFLAGS)
FOR = $(MPIFC) $(FCFLAGS) -DUSE_MPI $(MPILIBS)

OBJS= spectotime_par.o allocate_array.o exit_MPI.o read.o filters.o sacconv.o sacbw.o initialize.o spectotime.o

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
