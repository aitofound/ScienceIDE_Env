subroutine read_GreenFunc_elastic(ifreq)
    use convolution_par
    use coupling_SEM_DSM_par, only:myrank,do_coupling_RTZ,dir_DSM_input,force_comp,disp_elas_DSM,&
          stress_DSM
    use constants
    implicit none

! other variables
    character(len=MAX_STRING_LEN) ::green_disp_file,green_stress_file,file_tmp
    integer ::icomp,icomp_force,ifreq,ios


   do icomp_force=1,ncomp
    if(do_coupling_RTZ(icomp_force)) then
      write(file_tmp,"(a2,'/OUTPUT_FILES/disp_solid/freq_',i5.5)") force_comp(icomp_force),ifreq-1
      green_disp_file=trim(dir_DSM_input(1:len_trim(dir_DSM_input)))// '/' // &
         trim(file_tmp(1:len_trim(file_tmp)))
!     open(unit=15,file=trim(green_disp_file),status='unknown', form='formatted',iostat=ios)
      open(unit=15,file=trim(green_disp_file),status='unknown', form='unformatted',iostat=ios)
      if(ios /= 0) stop 'error reading disp_solid'
      do icomp=1,3
        read(15) disp_elas_DSM(:,icomp,icomp_force)
      end do
      close(15)
    end if
   end do

   do icomp_force=1,3
    if(do_coupling_RTZ(icomp_force)) then
      write(file_tmp,"(a2,'/OUTPUT_FILES/stress/freq_',i5.5)") force_comp(icomp_force),ifreq-1
      green_stress_file=trim(dir_DSM_input(1:len_trim(dir_DSM_input)))// '/' // &
         trim(file_tmp(1:len_trim(file_tmp)))

!      open(unit=16,file=trim(green_stress_file),status='unknown',form='formatted',iostat=ios)
       open(unit=16,file=trim(green_stress_file),status='unknown',form='unformatted',iostat=ios)
       if(ios /= 0) stop 'error reading GREENS_EPSILON'
       do icomp=1,ncomp_stress
          read(16)stress_DSM(:,icomp,icomp_force)
       end do
       close(16)
     end if
    end do


end subroutine read_GreenFunc_elastic

subroutine read_GreenFunc_acoustic(ifreq)
    use convolution_par
    use coupling_SEM_DSM_par, only:myrank,do_coupling_RTZ,dir_DSM_input,force_comp,&
        disp_elas_DSM,stress_DSM,disp_acous_DSM,pressure_DSM
    use constants
    implicit none

! other variables
    character(len=MAX_STRING_LEN) ::green_disp_file,green_pressure_file,file_tmp
    integer ::icomp,icomp_force,ifreq,ios


   do icomp_force=1,ncomp
    if(do_coupling_RTZ(icomp_force)) then
      write(file_tmp,"(a2,'/OUTPUT_FILES/disp_fluid/freq_',i5.5)") force_comp(icomp_force),ifreq-1
      green_disp_file=trim(dir_DSM_input(1:len_trim(dir_DSM_input)))// '/' // &
         trim(file_tmp(1:len_trim(file_tmp)))

!     open(unit=15,file=trim(green_disp_file),status='unknown', form='formatted',iostat=ios)
      open(unit=15,file=trim(green_disp_file),status='unknown', form='unformatted',iostat=ios)
      if(ios /= 0) stop 'error reading disp_fluid'
      do icomp=1,3
        read(15) disp_acous_DSM(:,icomp,icomp_force)
      end do
      close(15)
    end if
   end do

   do icomp_force=1,ncomp
    if(do_coupling_RTZ(icomp_force)) then
      write(file_tmp,"(a2,'/OUTPUT_FILES/pressure/freq_',i5.5)") force_comp(icomp_force),ifreq-1
      green_pressure_file=trim(dir_DSM_input(1:len_trim(dir_DSM_input)))// '/' // &
         trim(file_tmp(1:len_trim(file_tmp)))

!     open(unit=16,file=trim(green_pressure_file),status='unknown',form='formatted',iostat=ios)
      open(unit=16,file=trim(green_pressure_file),status='unknown',form='unformatted',iostat=ios)
      if(ios /= 0) stop 'error reading GREENS_EPSILON'
      read(16)pressure_DSM(:,icomp_force)
      close(16)
    end if
  end do

end subroutine read_GreenFunc_acoustic
