!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module EEE_ModGetB0
  implicit none
  save

  private

  public :: EEE_get_B0

contains
  !============================================================================
  subroutine EEE_get_B0(x_D,B0_D, TimeSimulation)

    use EEE_ModCommonVariables, ONLY: UseArch, UseTD, tStartCme
    use EEE_ModArch, ONLY: get_arch
    use EEE_ModTD99, ONLY: compute_TD99_BqField
    real, intent(in)  :: x_D(3)
    real, intent(out) :: B0_D(3)
    real, optional, intent(in) :: TimeSimulation
    real :: B_D(3)
    !--------------------------------------------------------------------------
    ! initialize state variables
    B0_D = 0.0

    if(UseArch)then
       call get_arch(x_D,B_D)

       B0_D = B0_D + B_D
    elseif(UseTD .and. TimeSimulation > tStartCme)then
       call compute_TD99_BqField(x_D, B_D, TimeSimulation)
       B0_D = B0_D + B_D
    end if

  end subroutine EEE_get_B0
  !============================================================================
end module EEE_ModGetB0
!==============================================================================
