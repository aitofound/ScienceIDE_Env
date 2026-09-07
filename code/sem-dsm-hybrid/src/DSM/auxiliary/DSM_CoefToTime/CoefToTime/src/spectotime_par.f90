module constants
   include "constants.h"
end module constants

module  spectotime_par
   implicit none
 
   integer, parameter ::ismooth=32
   double precision ::omega_imag
   complex(kind=8) ::omega

   integer ::nfrequency,ntheta_total
   integer ::itheta_start,itheta_end,ntheta_thisset
   logical ::fluid
   integer ::distset_id
   integer ::lmax,nl_eachpack,m0
   integer ::source_type
!   integer ::ista_start,ista_end
   double precision ::time_series_length,dt
   integer,parameter ::ncomp_disp=3
   integer,parameter ::ncomp_strain=6
   integer,parameter ::ncomp_pressure=1
   integer,parameter ::ncomp_chi_dot=1


   integer ::ndepth,idepth_start,idepth_end
!   integer,dimension(:),allocatable ::ndist_foridep
   double precision,dimension(:),allocatable ::theta
   double precision ::tbegin_save,tend_save
   integer ::npt_eachpack,npt_save,npack,ipt_begin,ipt_end

   double precision ::flow,fhigh
!   complex(kind=8),dimension(:),allocatable ::spec_forRead
!   complex(kind=8),dimension(:,:,:),allocatable ::spec  !,spec1

   integer ::nproc,myrank

!material properties and depth
   double precision,dimension(:),allocatable ::r_fluid,r_solid
   double precision,dimension(:),allocatable ::rho_fluid
   double precision,dimension(:),allocatable ::A,C,L,F,N

!parameters only depending on distance and order l
   double precision ::x,y,lsq
   double precision,dimension(-2:2) ::plm,plm1,plm2,dpdtheta,d2pd2theta
   double precision,dimension(-2:2) :: dplm1dtheta,factor
   complex(kind=8), dimension(-2:2)  ::dphi,Ylm
   complex(kind=8), dimension(-2:2) ::dYdphi,dYdtheta,d2Yd2theta,expimp


!array
   complex(kind=8),dimension(:,:,:,:),allocatable ::coef_disp_fluid
   complex(kind=8),dimension(:,:,:,:),allocatable ::coef_dcdr_fluid
   complex(kind=8),dimension(:,:,:,:,:),allocatable ::coef_disp_solid
   complex(kind=8),dimension(:,:,:,:,:),allocatable ::coef_dcdr_solid

   complex(kind=8),dimension(:,:),allocatable ::disp_fluid_lm
   complex(kind=8),dimension(:),allocatable ::pressure_lm
   complex(kind=8),dimension(:),allocatable ::chi_dot_lm
   complex(kind=8),dimension(:,:),allocatable ::disp_solid_lm,strain_lm

   complex(kind=8),dimension(:,:,:,:),allocatable ::spec_disp_fluid
   complex(kind=8),dimension(:,:,:),allocatable ::spec_pressure
   complex(kind=8),dimension(:,:,:),allocatable ::spec_chi_dot
   complex(kind=8),dimension(:,:,:,:),allocatable ::spec_disp_solid
   complex(kind=8),dimension(:,:,:,:),allocatable ::spec_strain


!   complex(kind=8),dimension(:,:,:),allocatable ::spec  !,spec1
   real(kind=4), dimension(:,:,:),allocatable ::array_pressure
   real(kind=4), dimension(:,:,:),allocatable ::array_chi_dot
   real(kind=4), dimension(:,:,:,:),allocatable ::array_disp_fluid
   real(kind=4), dimension(:,:,:,:),allocatable ::array_strain,array_disp_solid

   integer,dimension(:),allocatable ::ifreq_start
   logical ::TopBot
!   real(kind=4), dimension(:,:),allocatable ::sub_array
   real(kind=4), dimension(:),allocatable ::work_time
   complex(kind=8),dimension(:),allocatable ::work_spc

end module spectotime_par
