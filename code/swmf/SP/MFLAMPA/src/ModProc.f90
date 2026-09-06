!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModProc

  implicit none
  SAVE

  ! Public MPI information
  !----------------------------------------------------------------------------
  ! For MPI communicators
  integer :: iComm  = -1
  ! Total processor number
  integer :: nProc  = -1
  ! Current processor index
  integer :: iProc  = -1
  ! Error message
  integer :: iError = -1
contains
  !============================================================================
  subroutine warn_more_proc
    ! Show WARNING message when there are more processors than field lines
    !--------------------------------------------------------------------------
    write(*,*) "WARNING: More processors than field lines."
  end subroutine warn_more_proc
  !============================================================================
end module SP_ModProc
!==============================================================================
