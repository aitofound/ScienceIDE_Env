subroutine resample(ipoint,ipackage)
  use coupling_SEM_DSM_par
  use constants
  use convolution_par,only: omega_imag
  implicit none
 
  integer ipoint,ipackage 
! other variables
  integer ::icomp
  integer ::it

  disp_bound_fine(:,:)=0.d0
  traction_bound_fine(:,:)=0.d0
  do icomp=1,ncomp
    call resample_one_comp(disp_bound(1,icomp,ipoint),total_nstep_SEM,deltat_SEM,disp_bound_fine(1,icomp), &
                           npts_fine,deltat_refine,npoints_taper_SEM,omega_imag)
  end do
  
  do icomp=1,ncomp
     call resample_one_comp(traction_bound(1,icomp,ipoint),total_nstep_SEM,deltat_SEM, &
                       traction_bound_fine(1,icomp),npts_fine,deltat_refine,npoints_taper_SEM,omega_imag)
  end do
  if(myrank.eq.0.and.ipoint.eq.1.and.ipackage.eq.1) then
     open(unit=40,file='OUTPUT_FILES/rank0_point1',action='write',form="formatted",status="unknown")
     do it=1,total_nstep_SEM
       write(40,*)disp_bound(it,1,ipoint)
     end do
     close(40)
     open(unit=40,file='OUTPUT_FILES/rank40_point1_resample',action='write',form="formatted",status="unknown")
     do it=1,npts_fine
       write(40,*)disp_bound_fine(it,1)
     end do
     close(40)
  end if
end subroutine resample


!**********************************************************************
subroutine resample_one_comp(x,npts,deltat_SEM,x_new,npts_new,deltat_DSM,npoints_taper_SEM,omega_imag)

  use constants
  implicit none

!imput/output
  integer ::npts,npts_new,npoints_taper_SEM
  double precision ::deltat_SEM,deltat_DSM
  double precision,dimension(npts)::x
  double precision,dimension(npts_new) ::x_new
  double precision::omega_imag


  !other local variables
  double precision ::time_series_length
  integer ::nfit,it,it_start,it_copy,it_work,npts_new_cut,npoints_taper_DSMdt
  double precision, dimension(max_nfit)::x_work,time_work
  double precision ::time,time_rela,dx
  !deal with the extended waveforms
  integer ::it_smooth
  double precision ::tail_smooth_value
  double precision ::taper,time_taper,time_length_taper

  time_series_length=npts*deltat_SEM
  npts_new_cut=int(time_series_length/deltat_DSM)
  npts_new_cut=min(npts_new_cut,npts_new)
  nfit=int(2*deltat_DSM/deltat_SEM)
  
  if(npoints_taper_SEM.gt.npts) then
    print *,'error, npoints_taper_SEM is larger than npts'
    call exit_mpi('error npoints_taper_SEM')
  end if

  if(deltat_DSM<deltat_SEM) then
          print *,'WARNING: the new deltat is smaller than deltat that may generate inacurate interpolation'
  end if
  if(nfit.gt.max_nfit) then
     !print *,'nfit=',nfit,'max_nfit=',max_nfit
     print *,'error, the number of points for interpolation must be smaller than max_nfit'
     call exit_mpi('error nfit')
  else if(nfit.lt.min_nfit_SEM_resample.and.nfit.ge.0) then
     nfit=min_nfit_SEM_resample
  else if(nfit.lt.0) then
     call exit_mpi('error nfit<0')
  end if

  do it_work=1,nfit
    time_work(it_work)=(it_work-1)*deltat_SEM
  end do
  x_new(:)=0.d0
  do it=1,npts_new_cut
     if(it.eq.1) then
        x_new(1)=x(1)
     else if(it.eq.npts_new_cut) then
        x_new(npts_new_cut)=x(npts)
     else 
       time=(it-1)*deltat_DSM
       it_start=int(time/deltat_SEM-dble(nfit)/2.0)
!       time_rela=time-(it_start-1)*deltat_SEM
       if(it_start.lt.1) it_start=1
       if(it_start.gt.npts-nfit) it_start=npts-nfit
       time_rela=time-(it_start-1)*deltat_SEM
       do it_work=1,nfit
         it_copy=it_start+(it_work-1)
         x_work(it_work)=x(it_copy)
       end do
       call updown(nfit,time_work,x_work,time_rela,x_new(it),dx)
     end if
  end do


! Averaged values used to extend the waveforms
  tail_smooth_value=0.d0
  do it_smooth=npts-npoints_taper_SEM+1,npts
     tail_smooth_value=tail_smooth_value+x(it_smooth)
  end do
  tail_smooth_value=tail_smooth_value/npoints_taper_SEM

! Taper the SEM seismogram tail to 'tail_smooth_value'
! If the coupling boundaries are close to the source, static deformation is
! not negligible, and this averaging helps get the static deformation.
  npoints_taper_DSMdt=int(npoints_taper_SEM*deltat_SEM/deltat_DSM)+1
  do it=npts_new_cut-npoints_taper_DSMdt,npts_new_cut
     taper=dcos((it-(npts_new_cut-npoints_taper_DSMdt)+0.d0)/npoints_taper_DSMdt*pi/2.0)
     x_new(it)=tail_smooth_value*(1.0-taper)+x_new(it)*taper
  end do

! Extending the waveforms using 'tail_smooth_value' (presumably static deformation)
! Also applying additonal taper for the extended part, helping reduce the
! wrapping-around effects
  time_length_taper=(npts_new-npts_new_cut-1)*deltat_DSM
  do it=npts_new_cut+1,npts_new
     time_taper=(it-npts_new_cut-1)*deltat_DSM
     taper=dcos(time_taper/time_length_taper*pi/2.0)
     x_new(it)=tail_smooth_value*taper
     !x_new(it)=0.0
  end do

! Add artificial attenuation, which will be corrected back after IFFT.
  do it=1,npts_new
     x_new(it)=x_new(it)*dexp(-omega_imag*(it-1)*deltat_DSM)
  end do

end subroutine resample_one_comp


! This subroutine is from the book "An Introduction to Computational Physics"
!by Tao Pang (1997)
SUBROUTINE UPDOWN (N,XI,FI,X,F,DF)
  use constants
!
! Subroutine performing the Lagrange interpolation with the
! upward and downward correction method.  F: interpolated
! value.  DF: error estimated.  Copyright (c) Tao Pang 1997.
!
  IMPLICIT NONE
!  INTEGER, PARAMETER :: NMAX_NFIT=131
  INTEGER, INTENT (IN) :: N
  INTEGER :: I,J,I0,J0,IT,K
  DOUBLE PRECISION, INTENT (IN) :: X
  DOUBLE PRECISION, INTENT (OUT) :: F,DF
  DOUBLE PRECISION :: DX,DXT,DT
  DOUBLE PRECISION, INTENT (IN), DIMENSION (N) :: XI,FI
  DOUBLE PRECISION, DIMENSION (MAX_NFIT,MAX_NFIT) :: DP,DM
!
  IF (N.GT.MAX_NFIT) STOP 'nfit>MAX_NFIT. Dimension of the data set is too large.'
  IF (N.LT.3) STOP 'nfit<3. Dimension of the data set is too small.'
    DX = ABS(XI(N)-XI(1))
    DO  I = 1, N
      DP(I,I) = FI(I)
      DM(I,I) = FI(I)
      DXT = ABS(X-XI(I))
      IF (DXT.LT.DX) THEN
        I0 = I
        DX = DXT
      END IF
    END DO
    J0 = I0
!
! Evaluate correction matrices
!
  DO I = 1, N-1
    DO J = 1, N-I
      K = J+I
      DT =(DP(J,K-1)-DM(J+1,K))/(XI(K)-XI(J))
      DP(J,K) = DT*(XI(K)-X)
      DM(J,K) = DT*(XI(J)-X)
    END DO
  END DO
!
! Update the approximation
!
  F = FI(I0)
  IT = 0
  IF(X.LT.XI(I0)) IT = 1
 DO I = 1, N-1
    IF ((IT.EQ.1).OR.(J0.EQ.N)) THEN
      I0 = I0-1
      DF = DP(I0,J0)
      F  = F+DF
      IT = 0
      IF (J0.EQ.N) IT = 1
    ELSE IF ((IT.EQ.0).OR.(I0.EQ.1)) THEN
      J0 = J0+1
      DF = DM(I0,J0)
      F  = F+DF
      IT = 1
      IF (I0.EQ.1) IT = 0
    END IF
  END DO
  DF = ABS(DF)
END SUBROUTINE UPDOWN
