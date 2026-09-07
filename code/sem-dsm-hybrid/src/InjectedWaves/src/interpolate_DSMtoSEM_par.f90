module extract_Injectedwaves_par 

integer ::myrank,nproc

!Controling parameters
integer ::ncomp_source
character(len=80),dimension(3) ::single_force_names
data single_force_names /'Fr','Ft','Fz'/
character(len=80),dimension(6) ::moment_comp_names
data moment_comp_names /'Mzz','Mrr','Mtt','Mzr','Mzt','Mrt'/
!data moment_comp_names /'M11','M22','M33','M12','M13','M23'/
character(len=80),dimension(1) ::explosion_name
data explosion_name /'explosion'/

integer ::max_nfreq=16384
character(len=256), parameter :: para_file = 'DATA/Par_file'
character(len=256) ::dir_DSM_input
character(len=256) ::dir_SEM_input
character(len=256) ::dir_output = 'OUTPUT_FILES'
character(len=256) ::sub_dir_disp_fluid = 'disp_fluid'
character(len=256) ::sub_dir_pressure = 'pressure'
character(len=256) ::sub_dir_chi_dot = 'chi_dot'
character(len=256) ::sub_dir_velo_solid = 'velo_solid'
character(len=256) ::sub_dir_stress = 'stress'
character(len=256) ::output_para_file = 'InjectedWaves_Par_file'
!source_type: 1 for single forces and 2 for moment tensor
integer ::source_type,nfrequency,npt_eachpack,npt_save
character(len=80),dimension(:), allocatable ::sources_selected
integer, dimension(3) ::ENZ_components
double precision ::tbegin_save,tend_save
integer ::apply_filter
double precision ::flow,fhigh

integer, parameter ::ncomp_disp=3
integer, parameter ::ncomp_velo=3
integer, parameter ::ncomp_stress=6

! nfit_dep and nfit_dist must be larger than 2 (they do not need to be the same). 
! Larger numbers give better interpolation results, but slow down the compuations.
! We recommend a number between 3-16 for nfit_dist and 3-4 for nfit_dep. If the 
! distance spacing of Green's function table is small, nfit_dist=2 is fine. Otherwise,
! use larger nift_dep and nfit_dist to get better interpolation results.
integer, parameter ::nfit_dist = 4
integer, parameter ::nfit_dep = 4
integer, parameter ::MAX_NFIT = 131

! Depth difference tolerance: If the difference between two depths is smaller
! than this value, they are considered to represent the same depth.
double precision ::depth_tolerance=1.e-4


integer ::npackage_save
double precision ::dt_DSM,dt_DSM_refined
! Due to numerical precision limitations, there may be a discrepancy of up to one data sample 
! between the computed final_tbegin_save/final_tend_save and the user-specified
! tbegin_save/tend_save.
double precision ::final_tbegin_save,final_tend_save

!**************************DSM*****************************
! Factor for zero-padding in the DSM FFT.
! Modify this value only if you fully understand its implications. 
! Zero-padding in the DSM FFT improves accuracy during subsequent interpolation steps. 
! This factor applies zero-padding in the frequency domain, which increases resolution 
! in the time domain. As a result, it enhances the precision of interpolation used in SEM 
! simulations. A higher factor provides greater resolution but demands more storage capacity.
integer ::ismooth=8

character(len=256), parameter :: DSM_para_file = 'DSM_Par_file'
character(len=256), parameter :: DSM_acous_depth_file = 'depth_fluid_list'
character(len=256), parameter :: DSM_elas_depth_file = 'depth_solid_list'


integer ::nfreq_DSM,ndep_acous_DSM,ndep_elas_DSM,ntheta_DSM,&
          npt_elas_DSM,npt_acous_DSM
double precision ::min_theta_DSM,max_theta_DSM,dtheta_DSM
double precision ::time_length_DSM,omega_imag

double precision, dimension(:), allocatable ::depths_acous_DSM
double precision, dimension(:), allocatable ::depths_elas_DSM
integer, dimension(:), allocatable ::zone_acous_DSM
integer, dimension(:), allocatable ::zone_elas_DSM

! Temporary arrays for reading the DSM database. Read one frequency each time.
complex(kind=8), dimension(:),allocatable ::velo_elas_DSM
complex(kind=8), dimension(:),allocatable ::stress_DSM
complex(kind=8), dimension(:),allocatable ::disp_acous_DSM
complex(kind=8), dimension(:),allocatable ::pressure_DSM
complex(kind=8), dimension(:),allocatable ::potential_DSM

! The ID of the first DSM depth among the nfit_dep depths for interpolations
! It is updated in each depth running
integer ::first_idep_fit_DSM
! displacement, pressure and potential in fluid media at a depth
! disp_acous_idepth_DSM(nfrequency,ntheta_DSM,ncomp_disp)
complex(kind=8), dimension(:,:,:,:),allocatable ::disp_acous_fit_DSM
complex(kind=8), dimension(:,:,:),allocatable ::pressure_fit_DSM
complex(kind=8), dimension(:,:,:),allocatable ::potential_fit_DSM
! velocity and stress in solid media at a depth
! velo_elas_fit_DSM(nfrequency,ndep_fit,ntheta_DSM,ncomp_velo)
complex(kind=8), dimension(:,:,:,:),allocatable ::velo_elas_fit_DSM
complex(kind=8), dimension(:,:,:,:),allocatable ::stress_fit_DSM
   

!**************************SEM*****************************
character(len=256), parameter :: acous_depth_file_SEM = 'depth_table_acoustic'
character(len=256), parameter :: acous_dist_file_SEM = 'dist_table_acoustic'
character(len=256), parameter :: elas_depth_file_SEM = 'depth_table_elastic'
character(len=256), parameter :: elas_dist_file_SEM = 'dist_table_elastic'

integer :: ndepths_SEM
double precision, dimension(:), allocatable ::depths_SEM
integer, dimension(:), allocatable ::zones_SEM
integer, dimension(:), allocatable :: ndistances_SEM
double precision, dimension(:,:), allocatable ::distances_SEM

! displacement, pressure and chi_dot in fluid media at a depth
! disp_acous_SEM(1:nfrequency,idist,icomp)
complex(kind=8), dimension(:,:,:),allocatable ::disp_acous_SEM
complex(kind=8), dimension(:,:),allocatable ::pressure_SEM
complex(kind=8), dimension(:,:),allocatable ::chi_dot_SEM
real(kind=8), dimension(:,:,:),allocatable ::disp_acous_time_SEM
real(kind=8), dimension(:,:),allocatable ::pressure_time_SEM
real(kind=8), dimension(:,:),allocatable ::chi_dot_time_SEM
! velocity and stress in solid media at a depth
complex(kind=8), dimension(:,:,:),allocatable ::velo_elas_SEM
complex(kind=8), dimension(:,:,:),allocatable ::stress_SEM
real(kind=8), dimension(:,:,:),allocatable ::velo_elas_time_SEM
real(kind=8), dimension(:,:,:),allocatable ::stress_time_SEM

! Temporary variables used in ifft
! We just allocate them once for all to avoid multple allocating.
complex(kind=8),dimension(:),allocatable ::work_spc,spc_zero_padding
real(kind=8), dimension(:),allocatable :: work_time
real(kind=8), dimension(:),allocatable ::time_series,data_in,data_filtered

end module extract_Injectedwaves_par
