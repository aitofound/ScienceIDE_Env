subroutine comp_stress(ndepth,ntheta_thisset,nfrequency,myrank)
use spectotime_par,only:A,C,F,L,N,spec_strain
implicit none
integer,intent(in) ::ntheta_thisset,myrank,ndepth,nfrequency

!local parameters
integer ::idepth,itheta,ifreq
complex(kind=8)::sigma11,sigma22,sigma33,sigma12,&
                 sigma13,sigma23
complex(kind=8)::epsilon11,epsilon22,epsilon33,&
                 epsilon12,epsilon13,epsilon23
double precision::A_tmp,C_tmp,F_tmp,L_tmp,N_tmp

do idepth=1,ndepth
  do itheta=1,ntheta_thisset
   do ifreq=1,nfrequency
     A_tmp=A(idepth)
     C_tmp=C(idepth)
     F_tmp=F(idepth)
     L_tmp=L(idepth)
     N_tmp=N(idepth)
     epsilon11=spec_strain(1,itheta,idepth,ifreq)
     epsilon22=spec_strain(2,itheta,idepth,ifreq)
     epsilon33=spec_strain(3,itheta,idepth,ifreq)
     epsilon12=spec_strain(4,itheta,idepth,ifreq)
     epsilon13=spec_strain(5,itheta,idepth,ifreq)
     epsilon23=spec_strain(6,itheta,idepth,ifreq)

!Hooke's law
     sigma11=epsilon11*C_tmp+(epsilon22+epsilon33)*F_tmp
     sigma22=epsilon11*F_tmp+epsilon22*A_tmp+epsilon33*(A_tmp-2*N_tmp)
     sigma33=epsilon11*F_tmp+epsilon22*(A_tmp-2*N_tmp)+epsilon33*A_tmp
     sigma12=2.0*epsilon12*L_tmp
     sigma13=2.0*epsilon13*L_tmp
     sigma23=2.0*epsilon23*N_tmp
    
!copy
     spec_strain(1,itheta,idepth,ifreq)=sigma11
     spec_strain(2,itheta,idepth,ifreq)=sigma22
     spec_strain(3,itheta,idepth,ifreq)=sigma33
     spec_strain(4,itheta,idepth,ifreq)=sigma12
     spec_strain(5,itheta,idepth,ifreq)=sigma13
     spec_strain(6,itheta,idepth,ifreq)=sigma23
   end do !ifreq
  end do !itheta
end do !idepth

end subroutine
