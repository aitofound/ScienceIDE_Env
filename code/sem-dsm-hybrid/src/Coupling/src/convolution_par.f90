
module convolution_par
   use constants, only: MAX_STRING_LEN

   ! Global arrays for storing displacement data
   complex(kind=8), dimension(:,:,:),allocatable ::disp,final_disp

   ! Arrays for station coordinates and metadata
   double precision, dimension(:),allocatable :: station_lat,station_lon
   character(len=256), dimension(:),allocatable ::station_name
   double precision, dimension(:,:), allocatable ::station_coord

   ! Parameters for Green's function interpolation
   ! These arrays are used in the 'do_convolution' subroutine for interpolation.
   ! Declaring them globally and allocating them once in
   ! 'allocate_array_convolution'
   ! reduces repeated memory allocations and improves performance.
   double precision, dimension(:), allocatable::depths_fit_in,dists_fit_in
   double precision, dimension(:,:), allocatable::imag_z_in,real_z_in

   ! Rotation matrices for coordinate transformations
   double precision, dimension(:,:,:,:),allocatable ::rot_matrix_bound
   double precision, dimension(:,:,:,:),allocatable ::rot_matrix_station

   ! Distance array and interpolation indices
   double precision, dimension(:,:),allocatable ::distance
   integer, dimension(:,:),allocatable ::first_idep_fit_DSM,first_idist_fit_DSM

   ! General parameters
   integer nstation,max_nstation
   double precision ::time_series_length,omega_imag

   ! Workspace arrays for temporary computations
   real,dimension(:),allocatable ::work_time ! Time-domain workspace array
   complex(kind=8), dimension(:),allocatable ::work_spc ! Frequency-domain workspace array
end module
