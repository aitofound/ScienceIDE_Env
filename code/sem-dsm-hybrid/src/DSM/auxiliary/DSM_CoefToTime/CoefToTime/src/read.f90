subroutine read_para()
   use spectotime_par
   implicit none

! other variables
   character(len=80) ::para_file
   integer ::ios
   
   para_file="DATA/Par_file"
   open(unit=10,file=trim(para_file),action='read',status="old",iostat=ios)
   if(ios /= 0) then
      print *,'error:could not open parameter file'
      call exit_mpi('error opening parameter file')
   end if
   read(10,*) fluid
   read(10,*) source_type
   read(10,*) ntheta_total,ndepth
   read(10,*) lmax,nl_eachpack,m0
   read(10,*) nfrequency
   read(10,*) time_series_length
   read(10,*) omega_imag
   read(10,*) tbegin_save
   read(10,*) tend_save
   read(10,*) npt_eachpack
   read(10,*) flow,fhigh
   close(10)

   if(ntheta_total.lt.nproc) stop 'ntheta_total is smaller than nproc'
   dt=time_series_length/(2*ismooth*nfrequency)
   npack=int((tend_save-tbegin_save)/dt/(npt_eachpack-1))+1
   if(npt_eachpack.lt.2.or.npack.lt.1) stop 'Error of npt_eachpack or npack'
   npt_save=(npt_eachpack-1)*npack+1
   ipt_begin=int(tbegin_save/dt)+1
   ipt_end=ipt_begin+npt_save-1
end subroutine read_para


subroutine read_other_info()
   use spectotime_par
   implicit none

! other variables
   character(len=80) ::para_file
   integer ::idist,istat,idepth,itheta_thisset
   integer ::ios
   double precision ::theta_tmp

   para_file="DATA/material_properties"
   open(unit=10,file=trim(para_file),action='read',status="old",iostat=ios)
   if(ios /= 0) then
      print *,'error:could not open material properties file'
      call exit_mpi('error opening material properties file')
   end if
   if(fluid) then
     do idepth=1,ndepth
       read(10,*)  r_fluid(idepth)
       read(10,*)  rho_fluid(idepth)
     end do
   else
     do idepth=1,ndepth
       read(10,*)  r_solid(idepth)
       read(10,*)  A(idepth),C(idepth),F(idepth),L(idepth),N(idepth)
     end do
   end if
   close(10)


   para_file="DATA/ndist_file"
   open(unit=11,file=trim(para_file),action='read',status="old",iostat=ios)
   if(ios /= 0) then
      print *,'error:could not open parameter file'
      call exit_mpi('error opening parameter file')
   end if
   istat=0
   itheta_thisset=0
   !do idep=1,ndepth_total
!       read(11,*) ndist_foridep
       do idist=1,ntheta_total
         istat=istat+1
         read(11,*) theta_tmp
         if(istat.ge.itheta_start.and.istat.le.itheta_end) then
          itheta_thisset=itheta_thisset+1
          theta(itheta_thisset)=theta_tmp
         end if
       end do
   !end do
   if(istat.ne.ntheta_total) stop 'Error in counting  stations'
   if(itheta_thisset.ne.ntheta_thisset) stop 'Error in reading theta'
   close(11)

end subroutine

subroutine check_read_spec(order_l,l_local,nl_eachpack,&
                     TopBot,ifreq_start,fluid)
    use spectotime_par, only:ndepth,nfrequency,lmax,coef_disp_fluid,&
                      coef_dcdr_fluid,coef_disp_solid,coef_dcdr_solid
    use constants
    implicit none 
    integer,intent(in)  ::order_l,nl_eachpack
    logical,intent(in) ::TopBot,fluid
    integer,intent(out) ::l_local
    integer,dimension(nl_eachpack),intent(out) ::ifreq_start
    ! other variables
   character(len=80) ::coef_file
   integer ::ifreq,l,lbegin_read,lend_read,l_local_tmp
   integer ::ios,idepth
   logical ::read_spec


  l_local=mod(order_l,nl_eachpack)+1
  if(mod(order_l,nl_eachpack).eq.0) then
       read_spec=.true.
       lbegin_read=order_l
       lend_read=order_l+nl_eachpack-1
       if(lend_read.gt.lmax) lend_read=lmax
  else
       read_spec=.false.
  end if

!**********************************************************************
!**************************read the spectrum file**********************
  if(read_spec)then
   if(fluid) then
     coef_disp_fluid(:,:,:,:)=dcmplx(0.d0,0.d0)
     coef_dcdr_fluid(:,:,:,:)=dcmplx(0.d0,0.d0)
   else
     coef_disp_solid(:,:,:,:,:)=dcmplx(0.d0,0.d0)
     coef_dcdr_solid(:,:,:,:,:)=dcmplx(0.d0,0.d0)
   end if

   write(coef_file,"('./DATA/order_',i5.5,'to',i5.5)")lbegin_read,lend_read
!      open(unit=30,file=trim(spec_file),action='read',form="formatted",status="old",iostat=ios)
   open(unit=30,file=trim(coef_file),action='read',form="unformatted",status="old",iostat=ios)
   l_local_tmp=1
   do l=lbegin_read,lend_read
    read(30) ifreq_start(l_local_tmp)
    do ifreq=ifreq_start(l_local_tmp),nfrequency
     do idepth=1,ndepth
       if(fluid) then
         read(30) coef_disp_fluid(:,idepth,ifreq,l_local_tmp)  !velo(-m0:m0,idep,ifreq)
!Gpa to Pa
         coef_disp_fluid(:,idepth,ifreq,l_local_tmp)=&
            coef_disp_fluid(:,idepth,ifreq,l_local_tmp)*GpatoPa
         read(30) coef_dcdr_fluid(:,idepth,ifreq,l_local_tmp)  !dudr(-m0:m0,idep,ifreq)
!Gpa/Km to Pa/m
         coef_dcdr_fluid(:,idepth,ifreq,l_local_tmp)=&
            coef_dcdr_fluid(:,idepth,ifreq,l_local_tmp)*GpaPerKmtoPaPerM

       else
         read(30) coef_disp_solid(:,:,idepth,ifreq,l_local_tmp)  !velo(-m0:m0,1:3,idep,ifreq)
!km to m
         coef_disp_solid(:,:,idepth,ifreq,l_local_tmp)=&
            coef_disp_solid(:,:,idepth,ifreq,l_local_tmp)*kmtom

         read(30) coef_dcdr_solid(:,:,idepth,ifreq,l_local_tmp)  !dudr(-m0:m0,1:3,idep,ifreq)

!to be canceled
         if(ifreq.eq.nfrequency) &
           coef_disp_solid(:,:,idepth,ifreq,l_local_tmp)=dcmplx(0.d0,0.d0)
       end if
     end do !idepth
    end do !ifreq
    l_local_tmp=l_local_tmp+1
   end do !l
   close(30)
 end if !read

end subroutine
