!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
subroutine get_chromo_state(Xyz_D, BSi_D, TeSi, USi, HeatFluxSi, NeSi)
  use ModUtilities, ONLY: CON_stop
  implicit none
  ! INPUTS:
  !
  ! Dimensionless, normalized per R_S:
  real, intent(in) :: Xyz_D(3) ! x,y,z coordinates of the HGR frame
  ! In T;
  real, intent(in) :: BSi_D(3) ! x,y,z coomponrents of magnetic field
  ! In Kelvins:
  real, intent(in) :: TeSi     ! electron temperature at x,y,z point
  ! In m/s:
  real, intent(in) :: USi      ! speed of ion outflow along the MF
  ! In W/m^2:
  real, intent(out) :: HeatFluxSi ! Electron heat conduction flux from SC
  ! In particles per m^3:
  real, intent(out) :: NeSi
  character(len=*), parameter:: NameSub = 'get_chromo_state'
  !----------------------------------------------------------------------------
  call CON_stop('This is an empty version of '//NameSub)
end subroutine get_chromo_state
!==============================================================================

