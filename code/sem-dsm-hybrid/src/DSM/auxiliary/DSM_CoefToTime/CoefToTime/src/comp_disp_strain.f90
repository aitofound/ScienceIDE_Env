subroutine comp_disp_strain(idepth,ifreq,itheta,l,l_local,omega,myrank)
use spectotime_par,only: factor,x,y,lsq,plm,plm1,plm2,dpdtheta,d2pd2theta,&
                         dplm1dtheta,dphi,Ylm,dYdphi,dYdtheta,d2Yd2theta,&
                         expimp,fluid,rho_fluid,r_fluid,r_solid,coef_disp_solid,&
                         coef_disp_fluid,coef_dcdr_solid,coef_dcdr_fluid,m0,&
                         pressure_lm,chi_dot_lm,disp_fluid_lm,disp_solid_lm,strain_lm
use constants, only :PI
implicit none


integer,intent(in) ::idepth,ifreq,itheta,l,l_local,myrank
!complex(kind=8),intent(out) ::disp_solid_lm(3,-m0:m0),strain_lm(6,-m0:m0)
!complex(kind=8),intent(out) ::disp_fluid_lm(3,-m0:m0),pressure_lm(-m0:m0)
complex(kind=8),intent(in) ::omega


!local parameters
integer ::m
complex(kind=8) ::c1,c2,c3,dc1dr,dc2dr,dc3dr
complex(kind=8) ::dQdr,dQdtheta,dQdphi
complex(kind=8) ::ur,utheta,uphi
complex(kind=8) ::du1dr,du1dtheta,du1dphi
complex(kind=8) ::du2dr,du2dtheta,du2dphi
complex(kind=8) ::du3dr,du3dtheta,du3dphi

if(fluid) then
  pressure_lm(:)=dcmplx(0.d0,0.d0)
  chi_dot_lm(:)=dcmplx(0.d0,0.d0)
  disp_fluid_lm(:,:)=dcmplx(0.d0,0.d0)
else
  disp_solid_lm(:,:)=dcmplx(0.d0,0.d0)
  strain_lm(:,:)=dcmplx(0.d0,0.d0)
end if

do m=max0(-l,-m0),min0(l,m0)
   if(fluid) then
         c1=coef_disp_fluid(m,idepth,ifreq,l_local)
         dc1dr=coef_dcdr_fluid(m,idepth,ifreq,l_local)
!        pressure=Q/w
         pressure_lm(m)=omega*c1*Ylm(m)
!        chi_dot_lm(m)=-iQ
         chi_dot_lm(m)=c1*Ylm(m)*dcmplx(0.0,-1.0)
         dQdr=dc1dr*Ylm(m)
         dQdtheta=c1*dYdtheta(m)
         dQdphi=c1*dYdphi(m)
         ur=-dQdr/(rho_fluid(idepth)*omega)
         utheta=-dQdtheta/(rho_fluid(idepth)*omega*r_fluid(idepth))
         uphi=dcmplx(0.d0,0.d0)
         disp_fluid_lm(1,m)=ur
         disp_fluid_lm(2,m)=utheta
         disp_fluid_lm(3,m)=uphi
   else 
         c1=coef_disp_solid(m,1,idepth,ifreq,l_local)
         c2=coef_disp_solid(m,2,idepth,ifreq,l_local)
         c3=coef_disp_solid(m,3,idepth,ifreq,l_local)
         dc1dr=coef_dcdr_solid(m,1,idepth,ifreq,l_local)
         dc2dr=coef_dcdr_solid(m,2,idepth,ifreq,l_local)
         dc3dr=coef_dcdr_solid(m,3,idepth,ifreq,l_local)
         ur=c1*Ylm(m)
         if(l.eq.0) then
             utheta=dcmplx(0.d0,0.d0)
             uphi= dcmplx(0.d0,0.d0)
         else
             utheta=(c2*dYdtheta(m)+c3*dYdphi(m)/y)/dble(lsq)
             uphi=  (-c3*dYdtheta(m)+c2*dYdphi(m)/y)/dble(lsq)
         end if
         du1dr=dc1dr*Ylm(m)
         if(l.eq.0) then
           du1dtheta=dcmplx(0.d0,0.d0)
           du1dphi=dcmplx(0.d0,0.d0)
           du2dr=dcmplx(0.d0,0.d0)
           du2dtheta=dcmplx(0.d0,0.d0)
           du2dphi=dcmplx(0.d0,0.d0)
           du3dr= dcmplx(0.d0,0.d0)
           du3dtheta=dcmplx(0.d0,0.d0)
           du3dphi=dcmplx(0.d0,0.d0)
         else
           du1dtheta=c1*dYdtheta(m)
           du1dphi=c1*dYdphi(m)
           du2dr=(dc2dr*dYdtheta(m)+dc3dr*dYdphi(m)/y)/dble(lsq)
           du2dtheta=(c2*d2Yd2theta(m)+c3*dphi(m)*&
                       (-x/(y*y)*Ylm(m)+dYdtheta(m)/y))/dble(lsq)
           du2dphi=utheta*dphi(m)
           du3dr= ( -dc3dr*dYdtheta(m)+dc2dr*dYdphi(m)/y)/dble(lsq)
           du3dtheta=( -c3*d2Yd2theta(m) + &
              c2*dphi(m)*(-x/(y*y)*Ylm(m)+dYdtheta(m)/y ) )/dble(lsq)
           du3dphi=dphi(m)*uphi
         end if
!epsilon11_lm,epsilon22_lm,epsilon33_lm
!epsilon12_lm,epsilon13_lm,epsilon23_lm
         strain_lm(1,m)=du1dr 
!         if(real(epsilon11_lm).ne.0.d0) then 
!           print *,dc1,dr,Ylm,irup,irdown
!           stop 'Error epsilon11'
!         end if
         strain_lm(2,m)=(du2dtheta+ur)/r_solid(idepth)
         strain_lm(3,m)=(du3dphi/y+utheta*x/y+ur)/r_solid(idepth)
         strain_lm(4,m)=0.5d0*(du2dr+(du1dtheta-utheta)/r_solid(idepth))
         strain_lm(5,m)=0.5d0*(du3dr+(du1dphi/y-uphi)/r_solid(idepth))
         strain_lm(6,m)=0.5d0*((du3dtheta-uphi*x/y+du2dphi/y)/r_solid(idepth))
         disp_solid_lm(1,m)=ur
         disp_solid_lm(2,m)=utheta
         disp_solid_lm(3,m)=uphi
   end if
end do

end subroutine
