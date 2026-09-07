
module convolution_par
   use constants, only: MAX_STRING_LEN
!   integer ::myrank,nproc
   complex(kind=8), dimension(:,:,:),allocatable ::disp,final_disp
   double precision, dimension(:),allocatable :: station_lat,station_lon
   double precision :: source_lat,source_lon
   character(len=256), dimension(:),allocatable ::station_name
   double precision, dimension(:,:), allocatable ::station_coord


!   complex(kind=8), dimension(:,:,:),allocatable ::disp_Green_elas
!   complex(kind=8), dimension(:,:,:),allocatable ::epsilon_Green
!   complex(kind=8), dimension(:,:,:),allocatable ::disp_Green_acous
!   complex(kind=8), dimension(:,:),allocatable ::pressure_Green

!   character(len=MAX_STRING_LEN), parameter :: DSM_para_file = 'DSM_Par_file'
!   character(len=MAX_STRING_LEN), parameter :: DSM_acous_depth_file = 'depth_fluid_list'
!   character(len=MAX_STRING_LEN), parameter :: DSM_elas_depth_file = 'depth_solid_list'

!   integer ::nfreq_Green,ndep_acous_DSM,ndep_elas_DSM,ntheta,&
!             ntheta_refine,nstat_green_elas,nstat_green_acous
!   double precision ::min_theta,dtheta,dtheta_refine

   !The below vectors are parameters used for interpolations of Green's functions.
   !They only appear in the subroutine 'do_convolution', so declaring them there 
   !is better for readability. However, we declare them here as global parameters 
   !and allocate their memories only once in 'allocate_array_convolution', that 
   !helps avoid the unnecessary repeating memory allocations.
   double precision, dimension(:), allocatable::depths_fit_in,dists_fit_in
   double precision, dimension(:,:), allocatable::imag_z_in,real_z_in




   double precision, dimension(:,:,:,:),allocatable ::rot_matrix_bound
   double precision, dimension(:,:,:,:),allocatable ::rot_matrix_station
   double precision, dimension(:,:,:),allocatable ::rot_matrix_bound_toSrc
!   double precision, dimension(:,:),allocatable ::gcarc
!   integer, dimension(:,:),allocatable ::id_Green
   double precision, dimension(:,:),allocatable ::distance
   double precision, dimension(:),allocatable ::distance_toSrc
   integer, dimension(:,:),allocatable ::first_idep_fit_DSM,first_idist_fit_DSM
   integer, dimension(:),allocatable ::first_idep_fit_DSM_toSrc,first_idist_fit_DSM_toSrc
       
   

   integer nstation,max_nstation
   double precision ::time_series_length,omega_imag
   double precision ::time_series_length_toSrc,omega_imag_toSrc
   real,dimension(:),allocatable ::work_time
   complex(kind=8), dimension(:),allocatable ::work_spc


end module
