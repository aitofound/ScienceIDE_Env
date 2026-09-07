FC=gfortran
#FCFLAGS= -cpp #-g
#FCFLAGS= -cpp -traceback -check nobounds #-g
FCFLAGS=  -O -cpp

MPIFC=mpif90
MPILIBS=

CC=mpicc
CFLAGS= -O

#FLAGS_CHECK = -O0 -vec-report0 -std03 -implicitnone -warn truncated_source -warn argument_checking -warn unused -warn declarations -warn alignments -warn ignore_loc -warn usage -check all -align sequence -assume byterecl -ftz -traceback -ftrapuv  -check bounds

FLAGS_CHECK = 
#FLAGS_NO_CHECK = -O3 -xHost -funroll-loops -unroll5 -vec-report0 -std03 -implicitnone -warn truncated_source -warn argument_checking -warn unused -warn declarations -warn alignments -warn ignore_loc -warn usage -check bounds -align sequence -assume byterecl -ftz 

#FOR=$(FC) $(FCFLAGS)
FOR77=$(FC) $(FCFLAGS)
FOR = $(MPIFC) $(FCFLAGS) -DUSE_MPI  $(MPILIBS)

OBJS= coupling_SEM_DSM_par.o convolution_par.o allocate_array.o exit_MPI.o read_value_parameter.o read.o partition_job.o fft.o do_fft.o  initialize.o coupling_SEM_DSM.o get_boundary_property.o sacconv.o

OBJS_convolution= allocate_convolution.o calcu_rot_matrix.o convolution.o resample.o lagrange_interp_2d.o do_convolution.o read_GreenFun.o write_fft.o sacbw.o XYZtoENZ.o

MOD= constants.mod  convolution_par.mod  coupling_sem_dsm_par.mod
all:
	make coupling_integral

coupling_integral: $(OBJS) $(OBJS_convolution)
	 ${FOR}  ${FCFLAGS} ${OBJS} $(OBJS_convolution) -o coupling_integral

%.o: %.f90
	 ${FOR} -c ${FFLAGS} ${FLAGS_NO_CHECK} $<

%.o: %.f
	${FOR77} -c ${FFLAGS} ${FLAGS_NO_CHECK} $<

%.o:%.c
	${CC} -c ${CFLAGS} $<

clean:
	rm -f ${OBJS} $(OBJS_convolution) ${MOD} coupling_integral *~
