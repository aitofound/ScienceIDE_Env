subroutine do_fft(ndepth_fft,ntheta_fft,myrank)
use spectotime_par,only:ismooth,ncomp_disp,ncomp_pressure,&
                        ncomp_chi_dot,ncomp_strain,flow,fhigh,&
                        time_series_length,nfrequency,ipt_begin,npt_save,&
                        fluid,omega_imag,work_spc,work_time, &
                        spec_pressure,spec_chi_dot,spec_disp_fluid,spec_strain,&
                        spec_disp_solid,&
                        array_pressure,array_chi_dot,array_disp_fluid,array_strain,&
                        array_disp_solid
use constants
implicit none
integer ::ndepth_fft,ntheta_fft,myrank

!local parameters
integer ::idepth,itheta,icomp,ncomp_total
integer ::i_freq,n1,m1,nn,isign,i
integer ::ipt_begin_wrk,ipt_end_wrk
double precision ::t

! WENBO 
double precision:: work_in(4*ismooth*nfrequency)
double precision:: work_out(4*ismooth*nfrequency)
double precision:: dt_tmp
!double precision:: flow,fhigh
complex*16 iMulOmega


!integer,parameter ::ncomp_disp=3
!integer,parameter ::ncomp_pressure=1
!integer,parameter ::ncomp_strain=6


if(fluid) then
  ncomp_total=ncomp_pressure+ncomp_chi_dot+ncomp_disp
else
  ncomp_total=ncomp_strain+ncomp_disp
end if

  do idepth=1,ndepth_fft
   do itheta=1,ntheta_fft
    do icomp=1,ncomp_total
       work_spc(1)=dcmplx(0.d0,0.d0)
       do i_freq=1,nfrequency
          if(fluid) then
            if(icomp.eq.1) then
              work_spc(i_freq+1)  = spec_pressure(itheta,idepth,i_freq)
            else if(icomp.eq.2) then
              work_spc(i_freq+1)  = spec_chi_dot(itheta,idepth,i_freq)
            else
              work_spc(i_freq+1)  =  spec_disp_fluid(icomp-2,itheta,idepth,i_freq)
             iMulOmega=dcmplx(0.0,2.d0*PI* &
                  dble(i_freq)/dble(time_series_length))

!Turn it to velocity
!             work_spc(i_freq+1)  =  work_spc(i_freq+1)*iMulOmega
            end if
          else
            if(icomp.le.6) then
              work_spc(i_freq+1)  = spec_strain(icomp,itheta,idepth,i_freq)
            else
              work_spc(i_freq+1)  = spec_disp_solid(icomp-6,itheta,idepth,i_freq)
             iMulOmega=dcmplx(0.0,2.d0*PI* &
                  dble(i_freq)/dble(time_series_length))
             work_spc(i_freq+1)  =  work_spc(i_freq+1)*iMulOmega
            end if
          end if
       end do !i_frequency
       do i_freq=nfrequency+1,ismooth*nfrequency+1
           work_spc(i_freq) = dcmplx(0.d0,0.d0)
       end do
       do i_freq=1,ismooth*nfrequency-1
           n1 = ismooth*nfrequency + i_freq + 1
           m1 = ismooth*nfrequency - i_freq + 1
           work_spc(n1) = dconjg( work_spc(m1) )
       end do
       nn = 2 * ismooth*nfrequency
       isign = 1
       call four1( work_spc,nn,isign )
!debug. to be canceled
       do i=-nn+1,nn
          t = time_series_length &
                            * dble(i-1) / dble(nn)
              work_time(i+nn) &
               = real( &
                   dble( work_spc( mod(i-1+nn,nn)+1 ) ) &
                   * dexp( omega_imag * t ) &
                 )
! amplitude correction
! --- for FFT
              work_time(i+nn) = work_time(i+nn) &
                                 / real( time_series_length )
! --- km -> m
!              work_time(i+nn) = work_time(i+nn) * 1.e3
       end do   !i
! filter
            work_in(:)=work_time(:)
            dt_tmp=time_series_length/nn
!            flow=0.02
!            fhigh=0.4
!            call bwfilt(work_in,work_out,dt_tmp,2*nn,0,4,flow,fhigh)
!            work_time(:)=work_out(:)

     ipt_begin_wrk=2*ismooth*nfrequency+ipt_begin
     ipt_end_wrk=2*ismooth*nfrequency+ipt_begin+npt_save-1
     if(ipt_end_wrk.gt.4*ismooth*nfrequency) stop 'Error of ipt_end_wrk'
     if(fluid) then
        if(icomp.eq.1) then
            array_pressure(:,idepth,itheta)= &
               work_time(ipt_begin_wrk:ipt_end_wrk)
        else if(icomp.eq.2) then
            array_chi_dot(:,idepth,itheta)= &
               work_time(ipt_begin_wrk:ipt_end_wrk)
        else
            array_disp_fluid(:,idepth,itheta,icomp-2)= &
               work_time(ipt_begin_wrk:ipt_end_wrk)
        end if
     else
        if(icomp.le.6) then
            array_strain(:,idepth,itheta,icomp)= &
               work_time(ipt_begin_wrk:ipt_end_wrk)
        else
            array_disp_solid(:,idepth,itheta,icomp-6)= &
               work_time(ipt_begin_wrk:ipt_end_wrk)
        end if
     end if


     end do !icomp
   end do !itheta
  end do !idepth

end subroutine
