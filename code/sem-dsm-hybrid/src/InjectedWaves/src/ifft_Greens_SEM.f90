subroutine ifft_Greens_SEM(idepth,media_type)
  use extract_Injectedwaves_par, only:ndistances_SEM,ncomp_disp,ncomp_velo,ncomp_stress,&
        disp_acous_SEM,pressure_SEM,chi_dot_SEM,velo_elas_SEM,stress_SEM,ismooth,&
        apply_filter,flow,fhigh,omega_imag,disp_acous_time_SEM,pressure_time_SEM,chi_dot_time_SEM,&
        velo_elas_time_SEM,stress_time_SEM,work_spc,work_time,nfrequency,time_length_DSM
  implicit none

  integer, intent(in)::idepth,media_type

  !local parameters
  integer ::icomp,idist

  do idist=1,ndistances_SEM(idepth)
    ! Fluid media
    if(media_type.eq.1) then
      !displacement
      do icomp=1,ncomp_disp
        work_spc(1:nfrequency) = disp_acous_SEM(1:nfrequency,idist,icomp)
        call domain_freq_to_time(work_spc,nfrequency,ismooth,time_length_DSM,work_time,&
           apply_filter,flow,fhigh,omega_imag)
        disp_acous_time_SEM(:,idist,icomp)=work_time
      end do
      !pressure
      work_spc(1:nfrequency)=pressure_SEM(1:nfrequency,idist)
      call domain_freq_to_time(work_spc,nfrequency,ismooth,time_length_DSM,work_time,&
           apply_filter,flow,fhigh,omega_imag)
      pressure_time_SEM(:,idist)=work_time

      !chi_dot 
      work_spc(1:nfrequency)=chi_dot_SEM(1:nfrequency,idist)
      call domain_freq_to_time(work_spc,nfrequency,ismooth,time_length_DSM,work_time,&
           apply_filter,flow,fhigh,omega_imag)
      chi_dot_time_SEM(:,idist)=work_time


    ! Solid media
    else
      !velocity seismograms
      do icomp=1,ncomp_velo
        work_spc(1:nfrequency)=velo_elas_SEM(1:nfrequency,idist,icomp)
        call domain_freq_to_time(work_spc,nfrequency,ismooth,time_length_DSM,work_time,&
           apply_filter,flow,fhigh,omega_imag)
        velo_elas_time_SEM(:,idist,icomp)=work_time
      end do

      !stress
      do icomp=1,ncomp_stress
        work_spc(1:nfrequency)=stress_SEM(1:nfrequency,idist,icomp)
        call domain_freq_to_time(work_spc,nfrequency,ismooth,time_length_DSM,work_time,&
           apply_filter,flow,fhigh,omega_imag)
        stress_time_SEM(:,idist,icomp)=work_time
      end do
    end if
  end do

end subroutine ifft_Greens_SEM


subroutine domain_freq_to_time(work_spc,nfrequency,ismooth,time_length_DSM,work_time,&
           apply_filter,flow,fhigh,omega_imag)

  use  extract_Injectedwaves_par, only:spc_zero_padding,time_series,data_in,data_filtered
  implicit none

  integer ::nfrequency,ismooth,apply_filter
  double precision ::time_length_DSM,flow,fhigh,omega_imag
  complex(kind=8), dimension(nfrequency) ::work_spc
  real(kind=8), dimension(2*ismooth*nfrequency) ::work_time
  

  !local parameters
  integer ::ifreq,n1,m1,nn,isign,it
  double precision ::time,dt_tmp

  ! Copy frequency domain solution
  spc_zero_padding(1:nfrequency)=work_spc(1:nfrequency) 
  spc_zero_padding(nfrequency+1:ismooth*nfrequency+1) = cmplx(0.d0,0.d0)

  do ifreq=1,ismooth*nfrequency-1
     n1 = ismooth*nfrequency + ifreq + 1
     m1 = ismooth*nfrequency - ifreq + 1
     spc_zero_padding(n1) = conjg(spc_zero_padding(m1) )
  end do
  nn = 2 * ismooth*nfrequency

  !IFFT
  isign = 1
  call four1(spc_zero_padding,nn,isign)
  do it=-nn+1,nn
     time = time_length_DSM * dble(it-1) / dble(nn)
     ! Amplitude correction
     time_series(it+nn) = real( dble(spc_zero_padding( mod(it-1+nn,nn)+1 ) ) &
                              * dexp( omega_imag * time) )
     ! FFT normalization
     time_series(it+nn) = time_series(it+nn) / real( time_length_DSM)
  end do

  data_in(:)=time_series(:)
  ! Apply Butterworth filter
  if(apply_filter.eq.1) then
    dt_tmp=time_length_DSM/nn
    call bwfilt(data_in,data_filtered,dt_tmp,2*nn,0,4,flow,fhigh)
    time_series(:)=data_filtered
  end if

  work_time = time_series(2*ismooth*nfrequency+1:4*ismooth*nfrequency)

end subroutine domain_freq_to_time
