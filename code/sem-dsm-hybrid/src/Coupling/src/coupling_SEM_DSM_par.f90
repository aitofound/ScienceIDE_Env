module constants
   include "constants.h"
end module constants

module coupling_SEM_DSM_par
  use constants, only: MAX_STRING_LEN
  integer ::nproc,myrank
  
  !*****************SEM**********************
  character(len=MAX_STRING_LEN), parameter :: acous_depth_file_SEM = &
                                               'depth_table_acoustic_inner'
  character(len=MAX_STRING_LEN), parameter :: elas_depth_file_SEM = &
                                               'depth_table_elastic_inner'
  integer :: ndepths_solid_SEM,ndepths_fluid_SEM
  double precision, dimension(:), allocatable ::depths_solid_SEM,depths_fluid_SEM
  integer, dimension(:), allocatable ::zones_solid_SEM,zones_fluid_SEM


  character(len=MAX_STRING_LEN), parameter :: package_list_file_SEM = 'package_list'
  integer ::npackage_SEM,npoints_per_pack_SEM
  
  !To avoid memory issue, split the packages into N groups. Each group
  !contains nproc*npack_per_proc.
  integer ::ngroups
  integer, dimension(:), allocatable ::npack,ipack_start,ipack_end
  integer ::total_nstep_SEM,double_nfreq_DSM,npoints_taper_SEM,npts_fine
  integer ::nsection_SEM,nstep_each_section_SEM
  double precision ::deltat_SEM,deltat_DSM,deltat_refine
  logical,dimension(3) ::do_coupling_RTZ
  character(len=MAX_STRING_LEN) ::dir_SEM_input,dir_DSM_input

  !****************DSM***********************
  complex(kind=8), dimension(:,:,:),allocatable ::disp_elas_DSM
  complex(kind=8), dimension(:,:,:),allocatable ::stress_DSM
  complex(kind=8), dimension(:,:,:),allocatable ::disp_acous_DSM
  complex(kind=8), dimension(:,:),allocatable ::pressure_DSM

  character(len=MAX_STRING_LEN), parameter :: DSM_para_file = 'DSM_Par_file'
  character(len=MAX_STRING_LEN), parameter :: DSM_acous_depth_file = 'depth_fluid_list'
  character(len=MAX_STRING_LEN), parameter :: DSM_elas_depth_file = 'depth_solid_list'

  double precision, dimension(:),allocatable ::depths_acous_DSM,depths_elas_DSM
  integer, dimension(:),allocatable ::zones_acous_DSM,zones_elas_DSM
  integer ::nfreq_DSM,ndep_acous_DSM,ndep_elas_DSM,ndist_DSM,&
             nloc_elas_DSM,nloc_acous_DSM
  double precision ::min_dist_DSM,max_dist_DSM,ddist_DSM

  integer ::nfreq_DSMsrc,ndep_acous_DSM_toSrc,ndep_elas_DSM_toSrc,ndist_DSM_toSrc,&
            nloc_elas_DSM_toSrc,nloc_acous_DSM_toSrc
  double precision ::min_dist_DSM_toSrc,max_dist_DSM_toSrc,ddist_DSM_toSrc

  ! nfit_dep and nfit_dist must be larger than 2 (they do not need to be the same). 
  ! Larger numbers give better interpolations, but they slows down the compuation.
  ! We recommend a number between 3-16 for nfit_dist and 3-4 for nfit_dep. If the distance 
  ! spacing of Green's function table is small, nfit_dist=2 is fine. Otherwise,
  ! use larger nift_dep and nfit_dist to get better interpolation.
  integer ::nfit_dep,nfit_dist
  ! depth difference tolerence. If smaller than this number, the depths can be
  ! identified as the same depth.
  double precision ::depth_tolerence=0.05  !km

  integer, parameter ::FORCE_NAME_LEN = 2
  character(len=FORCE_NAME_LEN),dimension(3) ::force_comp
  data force_comp /'Fz','Fr','Ft'/
  
  integer, dimension(:),allocatable ::in_iproc,global_pack_id,local_pack_id
  integer, dimension(:),allocatable ::npoint_pack
  integer                           ::npoints
  integer                           ::npoints_max
  integer, dimension(:),allocatable  ::ipoint_start
  double precision,dimension(:,:,:), allocatable ::disp_bound,traction_bound
  double precision,dimension(:,:,:), allocatable ::disp_bound_new,traction_bound_new
 
  ! Temporary array for resampling SEM seismograms and performing FFT calculations
  ! for one boundary coupling position. The goal is to achieve an optimal sampling
  ! rate for the resampled data. 
  
  ! During resampling, the refined sampling interval ('deltat_refine') is selected 
  ! from the series delta_t_DSM / 2^n (where n = 1, 2, 3, ...). Each option corresponds 
  ! to dividing the DSM sampling interval (delta_t_DSM) by powers of 2 (e.g., 
  ! delta_t_DSM/2, delta_t_DSM/4, delta_t_DSM/8, etc.). 
  ! The refined interval ('deltat_refine' in read.f90) is chosen to be the closest match to the 
  ! original SEM sampling interval ('deltat_SEM' in read.f90). This strategy ensures
  ! compatibility between the SEM and DSM data while minimizing resampling artifacts.
  ! By aligning deltat_refine with delta_t_DSM/power_2, we can directly use the
  ! first nfreq_DSM frequencies from the FFT of the SEM seismograms for convolution 
  ! with the FFT results of the DSM.
  double precision,dimension(:,:),allocatable ::disp_bound_fine,traction_bound_fine
  complex(kind=8),dimension(:,:),allocatable::fft_disp_bound_fine,fft_traction_bound_fine

! Save fft of all the boundary coupling posions.
  complex(kind=8),dimension(:,:,:),allocatable ::fft_disp_bound_new,fft_traction_bound_new

  integer, dimension(:), allocatable ::id_depth_SEM
  logical, dimension(:), allocatable ::is_elastic,is_acoustic
  double precision,dimension(:,:), allocatable ::normal_vector,coord_bound,c
  double precision,dimension(:), allocatable   ::jacobian
end module coupling_SEM_DSM_par
