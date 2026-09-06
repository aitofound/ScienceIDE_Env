!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModTestFunc

  ! Provides basic test functionality
  use ModReadParam, ONLY: lStringLine
  use SP_ModProc, ONLY: iProc, nProc

  implicit none
  SAVE
  private ! Except

  public:: read_param      ! read parameters for testing
  public:: test_start      ! start testing a subroutine/function
  public:: test_stop       ! stop testing a subroutine/function

  integer, public:: lVerbose = 1                       ! verbosity level
  !   lVerbose=0:   no verbose output
  !   lVerbose=1:   minimal verbose output
  !   lVerbose=10:  verbose output on test processor
  !   lVerbose=100: verbose output on all processors

  character(lStringLine), public:: StringTest = ' '    ! list of things to test
  integer, public:: iProcTest   = 0                    ! 1st test processor
  integer, public:: iProcTest2  = -1                   ! 2nd test processor
  integer, public:: iBlockTest  = 1                    ! 1st test block
  integer, public:: iBlockTest2 = 1                    ! 2nd test block

  integer, public:: iTest  = 1, jTest  = 1, kTest  = 1 ! 1st test cell index
  integer, public:: iTest2 = 1, jTest2 = 1, kTest2 = 1 ! 2nd test cell index

contains
  !============================================================================
  subroutine read_param(NameCommand)

    use ModUtilities, ONLY: CON_stop
    use ModReadParam, ONLY: read_var

    character(len=*), intent(in):: NameCommand
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case("#VERBOSE")
       call read_var('lVerbose', lVerbose)
    case default
       call CON_stop(NameSub // ': unknown command=' // NameCommand)
    end select
  end subroutine read_param
  !============================================================================
  subroutine test_start(NameSub, DoTest, iBlock, i, j, k, DoTestAll)
    !$acc routine seq

#ifdef _OPENACC
#ifndef NOACCMODULE
    use openacc
#endif
#endif

    character(len=*),  intent(in) :: NameSub   ! method to be tested
    logical,           intent(out):: DoTest    ! return true if testing is on

    integer, optional, intent(in) :: iBlock    ! block index
    integer, optional, intent(in) :: i, j, k   ! cell index
    logical, optional, intent(in) :: DoTestAll ! test on all processors
    !--------------------------------------------------------------------------
    DoTest = .false.
#ifdef _OPENACC
#ifndef NOACCMODULE
    if (acc_on_device(acc_device_host)) then
#endif
#endif
       call test_start_cpu(NameSub, DoTest, iBlock, i, j, k, DoTestAll)
#ifdef _OPENACC
#ifndef NOACCMODULE
    end if
#endif
#endif
  end subroutine test_start
  !============================================================================
  subroutine test_start_cpu(NameSub, DoTest, iBlock, i, j, k, DoTestAll)

    ! If optional block index iBlock is present, restrict all actions
    ! to the test block(s) only. If optional indexes i, j, or k are
    ! present, check against the index(es) of the test cell(s).
    !
    ! Report this call on all processors if lVerbose == 100
    ! or DoTestAll is present and true.
    !
    ! Report on the test processor(s) if lVerbose == 10 or
    ! NameSub matches StringTest and lVerbose /= 0.
    !
    ! In the latter case set the optional DoTestOut to true
    ! on the test processor or possibly on all processors

    character(len=*),  intent(in) :: NameSub   ! method to be tested
    logical,           intent(out):: DoTest    ! return true if testing is on

    integer, optional, intent(in) :: iBlock    ! block index
    integer, optional, intent(in) :: i, j, k   ! cell index
    logical, optional, intent(in) :: DoTestAll ! test on all processors

    ! Start value for early returns

    !--------------------------------------------------------------------------
    DoTest = .false.
    if(lVerbose == 0) RETURN
    if(lVerbose == 1 .and. StringTest == '') RETURN

    ! Check block index if present
    if(present(iBlock))then
       if(  (iProc /= iProcTest  .or. iBlock /= iBlockTest) .and. &
            (iProc /= iProcTest2 .or. iBlock /= iBlockTest2)) &
            RETURN

       ! Check if cell indexes are present and equal with test cell(s)
       if(iProc == iProcTest .and. iBlock == iBlockTest)then
          if(present(i))then
             if(i /= iTest) RETURN
          end if
          if(present(j))then
             if(j /= jTest) RETURN
          end if
          if(present(k))then
             if(k /= kTest) RETURN
          end if
       end if

       if(iProc == iProcTest2 .and. iBlock == iBlockTest2)then
          if(present(i))then
             if(i /= iTest2) RETURN
          end if
          if(present(j))then
             if(j /= jTest2) RETURN
          end if
          if(present(k))then
             if(k /= kTest2) RETURN
          end if
       end if

    end if

    DoTest = index(' '//StringTest//' ', ' '//NameSub//' ') > 0

    if(DoTest)then
       if(present(DoTestAll))then
          if(.not.DoTestAll) &
               DoTest = iProc == iProcTest .or. iProc == iProcTest2
       else
          DoTest = iProc == iProcTest .or. iProc == iProcTest2
       end if
    end if

    if(lVerbose == 100 .or. ((lVerbose == 10 .or. DoTest) &
         .and. (iProc == iProcTest .or. iProc == iProcTest2)))then
       if(present(iBlock))then
          write(*,*) NameSub,' is starting for iProc, iBlock=', iProc, iBlock
       elseif(nProc > 1 .and. (lVerbose == 100 .or. iProcTest2 >= 0))then
          write(*,*) NameSub,' is starting on iProc=', iProc
       else
          write(*,*) NameSub,' is starting'
       end if
    end if

  end subroutine test_start_cpu
  !============================================================================
  subroutine test_stop(NameSub, DoTest, iBlock, i, j, k)
    !$acc routine seq

    ! If optional block index iBlock is present, restrict all actions
    ! to the test block(s) only.
    ! Write out a "finished" message if DoTest is true

    character(len=*),  intent(in):: NameSub
    logical,           intent(in):: DoTest
    integer, optional, intent(in):: iBlock
    integer, optional, intent(in):: i, j, k
    !--------------------------------------------------------------------------
#ifndef _OPENACC
    if(lVerbose == 0) RETURN
    if(lVerbose == 1 .and. StringTest == '') RETURN

    ! Check block index if present
    if(present(iBlock))then
       if(  (iProc /= iProcTest  .or. iBlock /= iBlockTest) .and. &
            (iProc /= iProcTest2 .or. iBlock /= iBlockTest2)) &
            RETURN

       ! Check if cell indexes are present and equal with test cell(s)
       if(iProc == iProcTest .and. iBlock == iBlockTest)then
          if(present(i))then
             if(i /= iTest) RETURN
          end if
          if(present(j))then
             if(j /= jTest) RETURN
          end if
          if(present(k))then
             if(k /= kTest) RETURN
          end if
       end if

       if(iProc == iProcTest2 .and. iBlock == iBlockTest2)then
          if(present(i))then
             if(i /= iTest2) RETURN
          end if
          if(present(j))then
             if(j /= jTest2) RETURN
          end if
          if(present(k))then
             if(k /= kTest2) RETURN
          end if
       end if

    end if

    if(lVerbose == 100 .or. ((lVerbose == 10 .or. DoTest) &
         .and. (iProc == iProcTest .or. iProc == iProcTest2)))then
       if(present(iBlock))then
          write(*,*) NameSub,' is finished for iProc, iBlock=', iProc, iBlock
       elseif(nProc > 1 .and. (lVerbose == 100 .or. iProcTest2 >= 0))then
          write(*,*) NameSub,' is finished on iProc=', iProc
       else
          write(*,*) NameSub,' is finished'
       end if
    end if
#endif
  end subroutine test_stop
  !============================================================================
end module SP_ModTestFunc
!==============================================================================
