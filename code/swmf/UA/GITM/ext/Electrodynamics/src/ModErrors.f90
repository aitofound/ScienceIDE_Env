!  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf

module ModErrors

  use ModCharSize

  implicit none

  integer, parameter :: nErrorsMax = 1000, nWarningsMax = 1000
  character(len=iCharLenIE_), dimension(nErrorsMax) :: cErrorCodes, cWarningCodes

  integer :: nErrors = 0, nWarnings = 0
  logical :: isOk = .true.

contains

  subroutine set_error(cError)
    character(len=*), intent(in) :: cError
    nErrors = nErrors + 1
    cErrorCodes(nErrors) = cError
    isOk = .false.
  end subroutine set_error

  subroutine report_errors()
    integer :: iError
    if (nErrors == 0) write(*, *) "No errors to report!"
    do iError = 1, nErrors
      write(*, *) "--> Error : ", trim(cErrorCodes(iError))
    enddo
  end subroutine report_errors

  ! -- This is for things that should not stop GITM, but notify user now & later. -- !
  subroutine raise_warning(cWarning)
    character(len=*), intent(in) :: cWarning
    nWarnings = nWarnings + 1
    cWarningCodes(nWarnings) = cWarning
    write(*, *) " -> Warning: ", trim(cWarningCodes(nWarnings))
  end subroutine raise_warning

  subroutine report_warnings()
    integer :: iWarning
    if (nWarnings == 0) write(*, *) "No errors to report!"
    do iWarning = 1, nWarnings
      write(*, *) "--> Error : ", trim(cWarningCodes(iWarning))
    enddo
  end subroutine report_warnings

end module ModErrors
