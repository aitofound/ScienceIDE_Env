!  Copyright (C) 2002 Regents of the University of Michigan
! portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModTiming

  implicit none

  SAVE

  ! Timing variables
  logical :: UseTiming = .true.
  integer :: nTiming = -2
  integer :: nTimingDepth = -1
  character(len=10) :: TimingStyle = 'cumm'
contains
  !============================================================================
  subroutine read_param
    ! Read input parameters for SP component

    use ModReadParam, ONLY: read_var
    !--------------------------------------------------------------------------
    call read_var('UseTiming',UseTiming)
    if(.not.UseTiming) RETURN
    call read_var('DnTiming',nTiming)
    call read_var('nDepthTiming',nTimingDepth)
    call read_var('TypeTimingReport',TimingStyle)

  end subroutine read_param
  !============================================================================
  subroutine check
    ! Check the timing in SP
    !--------------------------------------------------------------------------
    call timing_active(UseTiming)
    call timing_step(0)
    call timing_depth(nTimingDepth)
    call timing_report_style(TimingStyle)

  end subroutine check
  !============================================================================
end module SP_ModTiming
!==============================================================================
