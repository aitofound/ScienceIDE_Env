subroutine convolution(igroup)
  use coupling_SEM_DSM_par,only:nproc,myrank,npack,fft_disp_bound_new,fft_traction_bound_new,&
                normal_vector,coord_bound,c,jacobian,ndep_acous_DSM,ndep_elas_DSM,&
                nfreq_DSM,ngroups
  use convolution_par
  use constants
  implicit none

  integer ::igroup
#ifdef USE_MPI
  include "mpif.h"
#endif

  integer ::ifreq
#ifdef USE_MPI
  integer ::istat,ierr
#endif

!  call read_par_convolution()
  if(igroup.eq.1) then
     call allocate_array_convolution()
     call read_station()
  end if

  call calcu_rotmatrix_idGreen()

  if(myrank.eq.0) then
    write(*,*) "rank0 - Reading Green Functions and convolution starts..."
    call flush(6)
  end if
    do ifreq=1,nfreq_DSM
      if(myrank.eq.0.and.mod(ifreq,10).eq.0) then
        write(*,*) "rank0 is working on ifreq=",ifreq,"(nfreq=",nfreq_DSM,")"
        call flush(6)
      end if
      if(ndep_elas_DSM.gt.0) call read_GreenFunc_elastic(ifreq)
      if(ndep_acous_DSM.gt.0) call read_GreenFunc_acoustic(ifreq)
      call do_convolution(ifreq)
    end do
  if(myrank.eq.0) then
    write(*,*) "rank0 - Convolution done..."
    call flush(6)
  end if

#ifdef USE_MPI
  if(igroup.eq.ngroups) then
     if(myrank.eq.0) then
       write(*,*) "rank0 - Do MPI_REDUCE and rotate/write seismograms..."
       call flush(6)
     end if
     do istat=1,nstation
         call MPI_REDUCE(disp(1,1,istat),final_disp(1,1,istat),nfreq_DSM*ncomp, &
                   MPI_DOUBLE_COMPLEX,MPI_SUM,0,MPI_COMM_WORLD,ierr)
         call MPI_BARRIER(MPI_COMM_WORLD,ierr)
   
         if(myrank.eq.0) then
            call XYZtoENZ(final_disp(1,1,istat),nfreq_DSM,station_lat(istat),station_lon(istat))
            call convspc(istat,nstation,time_series_length,nfreq_DSM, &
                  omega_imag,final_disp(1,1,istat),0,0,0,station_lat,station_lon, &
                               work_spc,work_time,station_name)
         end if
         call MPI_BARRIER(MPI_COMM_WORLD,ierr)
     end do
  end if
#endif

end subroutine
