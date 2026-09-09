FC      = mpif77
#FC90    = ifort
FC90    = gfortran
#CC	= mpicc
FFLAGS  = -O 
#FFLAGS  = -O -traceback -check bounds
CFLAGS	= $(FFLAGS) 
OBJS    = param.o matrix.o interpolate.o compwave.o msub.o savespec.o check_add_node.o dsmti.o



all:
	make dsmti

dsmti: ${OBJS}
	$(FC) $(OBJS) $(FFLAGS) -o dsmti

%.o: %.f90
	$(FC90) -c $(FFLAGS) $<
%.o: %.f
	$(FC) -c -fallow-argument-mismatch $(FFLAGS) $<
.c.o:
	$(CC) -c $(CFLAGS) $<

clean:
	rm -f $(OBJS) dsmti *~
