! Maximum degree for Lagrange interpolations
integer, parameter ::max_nfit=20

! Minimum number of data points required for SEM seismogram down-sampling 
! during interpolation
integer, parameter ::min_nfit_SEM_resample = 8

! Maximum number of SEM output packages to be processed by one processor
integer, parameter ::max_npackages=4000

! Tiny value threshold to avoid numerical issues
integer, parameter ::TINY=1.e-9

! Dimension of the elastic property matrix
integer, parameter ::N_ELAS_COEF=21

! Number of components (e.g., x, y, z or N-S, E-W, Vertical)
integer, parameter ::ncomp=3

! The number of packages of one group processed by each processor
integer, parameter ::npack_per_proc=10

! Maximum array size (depends on the available memory on the HPC system)
integer, parameter ::max_array_size=700000000

! Number of stress component
integer, parameter ::ncomp_stress=6

! Mathematical constants
double precision,parameter ::pi=3.1415926535897932d0
double precision,parameter ::degtorad=pi/180.d0
double precision,parameter ::r_earth=6371.0*1.0e3
double precision,parameter ::radtodeg=180.d0/pi
double precision,parameter ::km=1000.d0

! Maximum allowed length for string variables
integer, parameter :: MAX_STRING_LEN = 512

! Flags for handling input files
logical, parameter :: IGNORE_JUNK = .true.,DONT_IGNORE_JUNK = .false.

! File IDs for input and output files
integer, parameter :: IIN = 40,IOUT = 41


