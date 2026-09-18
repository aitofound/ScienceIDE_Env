!  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf
subroutine PW_write_restart(&
     nAlt,RAD,SmLat,SmLon,Time,DT,nStep,NameRestart,     &
     State_CV)

  !Description: Write out restart file for PW
  
  use ModParameters,   ONLY: maxGrid
  use ModCommonPlanet, ONLY: nVar,nIon,iRho_I,iU_I,iP_I,iT_I
  use ModIoUnit, ONLY: UnitTmp_
  use ModUtilities, ONLY: open_file, close_file
  
  implicit none
  integer, intent(in) :: nAlt,nStep
  real   , intent(in) :: SmLat,SmLon,Time,DT
  real   , intent(in) :: RAD(nAlt)
  real   , intent(in) :: State_CV(1:nAlt,nVar)
  
  character*100,intent(in)   :: NameRestart
  
  integer :: K,iIon
  character(len=*), parameter:: NameSub = 'PW_write_restart'
  !____________________________________________________________________________


  call open_file(file=NameRestart, NameCaller=NameSub)
  
  write (UnitTmp_,*) TIME,DT,nAlt,NSTEP
  write (UnitTmp_,*) SmLat, SmLon
  do iIon=1,nIon
     WRITE (UnitTmp_,2002)&
          (RAD(K),State_CV(K,iU_I(iIon)),State_CV(K,iP_I(iIon)),&
          State_CV(K,iRho_I(iIon)) ,State_CV(K,iT_I(iIon)) ,K=1,nAlt)        
  enddo
  
  call close_file
  
!2001 format(2(1PE16.6),I10)
2002 format(5(1PE25.16))


end subroutine PW_write_restart
