subroutine sum_disp_strain(l,idepth,ifreq,itheta,myrank)

use spectotime_par,only :spec_disp_fluid,spec_disp_solid,&
                         spec_pressure,spec_chi_dot,spec_strain,m0,fluid,&
                         disp_fluid_lm,pressure_lm,chi_dot_lm,disp_solid_lm,strain_lm
implicit none

integer,intent(in) ::l,idepth,ifreq,itheta,myrank
!complex(kind=8),intent(in) ::disp_solid_lm(3,-m0:m0),strain_lm(6,-m0:m0)
!complex(kind=8),intent(in) ::disp_fluid_lm(3,-m0:m0),pressure_lm(-m0:m0)




!local parameters
integer ::m

 do m=max0(-l,-m0),min0(l,m0)
   if(fluid) then
      spec_disp_fluid(:,itheta,idepth,ifreq)= &
          spec_disp_fluid(:,itheta,idepth,ifreq)+disp_fluid_lm(:,m)

      spec_pressure(itheta,idepth,ifreq)= &
          spec_pressure(itheta,idepth,ifreq) + pressure_lm(m)
      spec_chi_dot(itheta,idepth,ifreq)= &
          spec_chi_dot(itheta,idepth,ifreq) + chi_dot_lm(m)
   else
      spec_disp_solid(:,itheta,idepth,ifreq)= &
          spec_disp_solid(:,itheta,idepth,ifreq)+disp_solid_lm(:,m)
      spec_strain(:,itheta,idepth,ifreq)= &
          spec_strain(:,itheta,idepth,ifreq)+strain_lm(:,m)
   end if
 end do !m
end subroutine
