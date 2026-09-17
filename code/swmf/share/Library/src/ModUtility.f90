!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf

module ModUtilities

  ! Simple methods which are used by CON and can be used
  ! by the components of the SWMF too.
  !
  ! F77 and C++ codes need an F90 interface to access these utilities.

  ! These are all the dependencies:
  ! nvfortran does not compile ModInterpolateAMR if the ONLY is present below
  ! use ModKind, ONLY: Real4_, Real8_
  use ModKind
  use ModMpi
  use ModIoUnit, ONLY: UnitTmp_, io_unit_clean
  use iso_c_binding, ONLY: c_null_char

  implicit none

  private ! except

  public:: UnitTmp_ ! inherited from ModIoUnit
  public:: make_dir
  public:: check_dir
  public:: fix_dir_name
  public:: open_file
  public:: close_file
  public:: remove_file
  public:: touch_file
  public:: flush_unit
  public:: split_string
  public:: join_string
  public:: upper_case
  public:: lower_case
  public:: string_to_char_array
  public:: char_array_to_string
  public:: write_string_tabs_name
  public:: sleep
  public:: check_allocate
  public:: greatest_common_divisor
  public:: CON_stop
  public:: CON_stop_simple
  public:: CON_set_do_test
  public:: test_mod_utility
  public:: norm2
  public:: find_cell
  public:: int_to_ascii_code
  public:: real_to_ascii_code
  public:: scientific_notation
  interface find_cell
     ! Single and double precision coordinates
     module procedure find_cell4, find_cell8
  end interface find_cell
#ifdef _OPENACC
  public:: init_gpu
#endif
  public:: i_gang

  logical, public :: DoFlush = .true. ! parameter for flush_unit
  logical, public :: DoMakeDir = .true. ! parameter for make_dir
  logical, public :: DoWriteCallSequence = .false. ! parameter for CON_stop

  ! Parameters for CON_set_do_test
  character(len=200), public :: StringTest = ''
  integer, public            :: iProcTest = 0
  integer, public            :: lVerbose = 1

  character, public:: cTab = char(9)

  integer(Int1_), public, parameter:: iChar0 = ichar('0')
  integer(Int1_), public, parameter:: iCharE = ichar('E')
  integer(Int1_), public, parameter:: iCharMinus = ichar('-')
  integer(Int1_), public, parameter:: iCharPlus = ichar('+')
  integer(Int1_), public, parameter:: iCharDot = ichar('.')
  integer(Int1_), public, parameter:: iCharSpace = ichar(' ')

  interface split_string
     module procedure split_string, split_string_simple
  end interface split_string

  interface join_string
     module procedure join_string, join_string_simple
  end interface join_string

contains
  !============================================================================
  subroutine write_string_tabs_name(String, Name)

    character(len=*), intent(in):: String, Name

    ! Write String and Name with 2 or 3 tabs in between (PARAM.in format)

    !--------------------------------------------------------------------------
    if(len_trim(String) < 8)then
       write(UnitTmp_,'(a)') trim(String)//cTab//cTab//cTab//trim(Name)
    else
       write(UnitTmp_,'(a)') trim(String)//cTab//cTab//trim(Name)
    end if

  end subroutine write_string_tabs_name
  !============================================================================
  subroutine CON_set_do_test(String, DoTest, DoTestMe)

    ! Set DoTest to true if " String " can be found in " StringTest "
    ! If the optional DoTestMe variable is present, it is set to
    ! DoTestMe = DoTest .and. iProc == iProcTest,
    ! where iProcTest is a public variable set to the test processor index.
    ! If only DoTest is present, it behaves like DoTestMe.
    !
    ! Depending on the value of the public variable lVerbose,
    ! different amount of output is printed:
    ! If String matches StringTest or lVerbose==10, the test processor prints
    !    "String CALLED"
    ! If lVerbose==100, all processors print "String CALLED by iProc=",iProc

    character(len=*),  intent(in) :: String
    logical,           intent(out):: DoTest
    logical, optional, intent(out):: DoTestMe

    logical:: IsMpiInitialized
    integer:: iProc=0, iError
    !--------------------------------------------------------------------------
    call MPI_initialized(IsMpiInitialized, iError)

    if(IsMpiInitialized) &
         call MPI_comm_rank(MPI_COMM_WORLD, iProc, iError)

    if(present(DoTestMe))then
       DoTest   = index(' '//StringTest,' '//String//' ') > 0
       DoTestMe = DoTest .and. iProc==iProcTest
    else
       if(iProc==iProcTest)then
          DoTest = index(' '//StringTest,' '//String//' ') > 0
       else
          DoTest = .false.
       end if
    end if
    if((DoTest .or. lVerbose>=10) .and. iProc==iProcTest)then
       write(*,*)String,' CALLED'
    else if(lVerbose>=100)then
       write(*,*)String,' CALLED by iProc=', iProc
    end if

  end subroutine CON_set_do_test
  !============================================================================
#ifdef _OPENACC
  subroutine CON_stop_acc(String1, String2, String3)
    !$acc routine seq nohost

    ! Write out String1 (and String2 and String3 if present).
    ! OpenAcc cannot handle arbitrary type of arguments.
    ! It cannot handle string concatenation either, so we pass
    ! string parts as individual arguments.
    ! OpenACC does not understand the ADVANCE='NO' feature either.

    character(len=*),  intent(in) :: String1
    character(len=*), optional, intent(in) :: String2, String3
    !--------------------------------------------------------------------------
    if (present(String3)) then
       write (*,*) 'ERROR: ', String1, String2, String3
    else if (present(String2)) then
       write (*,*) 'ERROR: ', String1, String2
    else
       write (*,*) 'ERROR: ', String1
    end if
    stop String1  ! this will call the OpenACC error handler

  end subroutine CON_stop_acc
  !============================================================================
#endif
  subroutine CON_stop_simple(String1, String2, String3)
    !$acc routine bind(CON_stop_acc)

    character(len=*),  intent(in) :: String1
    character(len=*), optional, intent(in) :: String2, String3
    !--------------------------------------------------------------------------
    if (present(String3)) then
       call CON_stop(String1//String2//String3)
    elseif(present(String2)) then
       call CON_stop(String1//String2)
    else
       call Con_stop(String1)
    end if
    stop "UNREACHABLE CODE"

  end subroutine CON_stop_simple
  !============================================================================
  subroutine CON_stop(String, Value1, Value2, Value3, Value4)

    character(len=*), intent(in):: String
    class(*), optional, intent(in):: Value1, Value2, Value3, Value4

    ! Stop execution after the following actions:
    !
    ! Write out error message with processor rank and String.
    ! Write out optional arguments Value1 ... Value4 of arbitrary type.
    ! Close open files.
    ! If public variable DoCreateCallSequence is true, then make a floating
    ! exception to produce call sequence (with NAG compiler in debugging mode).
    ! Abort execution with MPI_abort and stop.

    logical:: IsMpiInitialized
    integer:: iProc=0, nError=1, iError
    !--------------------------------------------------------------------------
    call MPI_initialized(IsMpiInitialized, iError)

    if(IsMpiInitialized) call MPI_comm_rank(MPI_COMM_WORLD, iProc, iError)

    write(*,*) 'ERROR: aborting execution on processor', iProc, &
         ' with message:'
    write(*,'(a)') 'ERROR: '//String
    if(present(Value1)) call write_value(Value1)
    if(present(Value2)) call write_value(Value2)
    if(present(Value3)) call write_value(Value3)
    if(present(Value4)) call write_value(Value4)

    ! Try closing all open IO units
    call io_unit_clean

    ! Create call sequence if requested
    if(DoWriteCallSequence)then
       write(*,*)'Making floating point exception to write call sequence!'
       write(*,*) sqrt(-1.0-iProc)
    end if

    ! Stop execution
    if(IsMpiInitialized) call MPI_abort(MPI_COMM_WORLD, nError, iError)
    stop

  end subroutine CON_stop
  !============================================================================
  subroutine write_value(Value, StringBefore, Advance)

    ! Write out value of type real, integer, logical, or character.
    ! If StringBefore is present, write it before the value with no advance.
    ! If Advance is present and set to ADVANCE="NO", then the Value is
    ! written out formatted with ADVANCE="NO".

    class(*),         intent(in):: Value
    character(len=*), optional, intent(in):: StringBefore
    character(len=*), optional, intent(in):: Advance

    logical:: DoAdvance
    !--------------------------------------------------------------------------
    DoAdvance = .true.
    if(present(Advance)) DoAdvance = Advance /= "NO"

    if(present(StringBefore)) write(*,'(a)',ADVANCE="NO") StringBefore
    select type (Value)
    type is (real)
       if(DoAdvance)then
          write(*,*) Value
       else
          write(*,'(es18.5)',ADVANCE="NO") Value
       end if
    type is (integer)
       if(DoAdvance)then
          write(*,*) Value
       else
          write(*,'(i10)',ADVANCE="NO") Value
       end if
    type is (logical)
       if(DoAdvance)then
          write(*,*) Value
       else
          write(*,'(l1)',ADVANCE="NO") Value
       end if
    type is (character(*))
       if(DoAdvance)then
          write(*,'(a)') Value
       else
          write(*,'(a)',ADVANCE="NO") Value
       end if
    end select

  end subroutine write_value
  !============================================================================
  subroutine make_dir(NameDir, iPermissionIn, iErrorOut, iErrorNumberOut)

    use iso_c_binding

    character(len=*), intent(in):: NameDir ! Directory name

    integer, intent(in), optional:: iPermissionIn ! Access permission

    integer, intent(out), optional:: iErrorOut    ! 0 on success, -1 otherwise
    integer, intent(out), optional:: iErrorNumberOut ! C error number

    ! Create the directory specified by NameDir. Trailing spaces are ignored.
    ! Nested directories are also allowed.
    ! If the directory already exists, this function does nothing.
    !
    ! The optional iPermissionIn parameter sets the permissions for the new
    ! directory. This should be specified in octal notation, which in
    ! Fortran is written with a capital O followed by the digits in quotes.
    ! For instance, the permissions 0755 would be written as O'0775',
    ! which is the default value (drwxr-xr-x).
    ! The actual directory will have permissions modified by
    ! the default mask (set by the Unix command umask).
    !
    ! The optional iErrorOut is set to 0 if no error occured and -1 otherwise.
    ! If iError is not present, the code stops with an error message.
    !
    ! The iErrorNumber contains the return value from the C mkdir() function.
    ! Values of iErrorNumber are found in errno.h and are not standardized,
    ! so exact values of these should not be relied upon.

    integer(c_int):: iPermission ! Octal permissions (value passed to C)
    integer:: iErrorNumber       ! Error number as returned from C
    integer:: iError             ! Return value as retrieved from C

    interface
       ! Interface for make_dir_c implemented in ModUtility_c.c
       integer(kind=c_int) function make_dir_c(path, perm, errno) bind(C)
         use iso_c_binding
         character(kind=c_char), intent(in):: path
         integer(kind=c_int), intent(in), value:: perm
         integer(kind=c_int), intent(out):: errno
       end function make_dir_c

    end interface

    integer:: i, j, k, l

    character(len=*), parameter:: NameSub = 'make_dir'
    !--------------------------------------------------------------------------
    if(.not. present(iPermissionIn)) then
       iPermission = 493 ! same as O'0755', but gfortran does not like that
    else
       iPermission = iPermissionIn
    endif

    ! Create the directory.
    ! If NameDir contains one or more /, then create the top directory first
    ! and then the subdirectories.

    l = len_trim(NameDir)
    i = 1
    do
       ! Find the next '/' in NameDir, but not the first character
       j = l
       k = index(NameDir(i+1:l), '/')
       if(k > 0) j = i + k

       ! Create (sub)directory
       iError = make_dir_c(NameDir(1:j)//C_NULL_CHAR, &
            iPermission, iErrorNumber)

       ! Check for errors
       if(iError /= 0)then
          if(present(iErrorOut))then
             iErrorOut = iError
             if(present(iErrorNumberOut)) iErrorNumberOut = iErrorNumber
             RETURN
          else
             write(*,*) NameSub,' iError, iErrorNumber=', iError, iErrorNumber
             call CON_stop(NameSub// &
                  ' failed to create directory '//trim(NameDir(1:j)))
          end if
       end if

       i = j + 1
       if(i > l) EXIT

    end do

    if(present(iErrorOut))       iErrorOut = iError
    if(present(iErrorNumberOut)) iErrorNumberOut = iErrorNumber

  end subroutine make_dir
  !============================================================================
  subroutine check_dir(NameDir, iErrorOut)

    character(len=*), intent(in) :: NameDir
    integer, optional, intent(out):: iErrorOut

    ! Check if a directory exists by trying to open a file in it.
    ! If iErrorOut is present, return the error code. Otherwise
    ! die with an error message if the directory does not exist
    ! or it is not writable.
    !
    ! {\bf This subroutine should be called by the root PE of the component
    ! only!}
    ! Calling the subroutine from multiple PE-s may result in a fatal error,
    ! namely one PE may delete the file written by the other PE, so the
    ! other PE thinks that the directory does not exist.

    integer:: iError

    ! Try to open a file in this directory
    character(len=*), parameter:: NameSub = 'check_dir'
    !--------------------------------------------------------------------------
    if(.not. DoMakeDir) RETURN
    open(UnitTmp_, file=trim(NameDir)//'/.test', status='unknown', &
         iostat = iError)

    ! Delete the file if it was created successfully
    if(iError == 0) close(UnitTmp_, status = 'DELETE')

    if(present(iErrorOut))then
       ! Simply inform the caller
       iErrorOut = iError
    elseif (iError /= 0) then
       ! Stop the run
       call CON_stop(NameSub//' ERROR: Cannot find/write into directory '&
            //trim(NameDir))
    endif

  end subroutine check_dir
  !============================================================================
  subroutine fix_dir_name(NameDir)

    character(len=*), intent(inout) :: NameDir

    ! Append a '/' at the end of the directory name if it is not there
    ! and the directory name is not zero length (empty string).
    !
    ! {\bf This subroutine should be called by all PE-s of the component!}

    integer :: i
    character(len=*), parameter:: NameSub = 'fix_dir_name'
    !--------------------------------------------------------------------------
    i = len_trim(NameDir)
    if(i == 0) RETURN
    if(NameDir(i:i) == '/') RETURN

    if(i >= len(NameDir)) call CON_stop(NameSub// &
         "ERROR cannot append / to directory name "//NameDir)

    NameDir(i+1:i+1) = '/'

  end subroutine fix_dir_name
  !============================================================================
  subroutine open_file(iUnitIn, File, Form, Status, Position, Access, Action, &
       Recl, iComm, NameCaller, iErrorOut, iUnitMpi)

    ! Interface for the Fortran open statement with error checking.
    ! If an error occurs, the code stops and writes out the unit number,
    ! the error code and the name of the file.
    ! If NameCaller is present, it is also shown.
    ! If no unit number is passed, open UnitTmp_.
    ! Default FORM is 'formatted' as in the open statement.
    ! Default STATUS is 'replace' (not unknown) as it is well defined.
    ! Default POSITION is 'rewind' (not asis) as it is well defined.
    ! Default ACCESS is 'sequential' as in the open statement.
    ! Default ACTION is 'read' if status is 'old' and position is not 'append'
    ! otherwise the default is 'readwrite'.
    ! There is no default for record length RECL.
    ! If the MPI communicator iComm is present together with Recl,
    ! the file will be opened with status='replace' on processor 0,
    ! and with status='old' on other processors with an MPI_barrier
    ! in between.

    integer, optional, intent(in):: iUnitIn
    character(len=*), optional, intent(in):: File
    character(len=*), optional, intent(in):: Form
    character(len=*), optional, intent(in):: Status
    character(len=*), optional, intent(in):: Position
    character(len=*), optional, intent(in):: Access
    character(len=*), optional, intent(in):: Action
    integer,          optional, intent(in):: Recl
    integer,          optional, intent(in):: iComm
    character(len=*), optional, intent(in):: NameCaller
    integer,          optional, intent(out):: iErrorOut
    integer,          optional, intent(inout):: iUnitMpi

    character(len=20):: &
         TypeForm, TypeStatus, TypePosition, TypeAccess, TypeAction

    integer:: iUnit
    integer:: iError, iProc, nProc

    character(len=*), parameter:: NameSub = 'open_file'
    !--------------------------------------------------------------------------
    iUnit = UnitTmp_
    if(present(iUnitIn)) iUnit = iUnitIn

    TypeForm = 'formatted'
    if(present(Form))then
       TypeForm = Form
       call lower_case(TypeForm)
    end if

    TypeStatus = 'replace'
    if(present(Status))then
       TypeStatus = Status
       call lower_case(TypeStatus)
    end if

    TypePosition = 'rewind'
    if(present(Position))then
       TypePosition = Position
       call lower_case(TypePosition)
    end if

    TypeAction = 'readwrite'
    if(TypeStatus == 'old' .and. TypePosition /= 'append') TypeAction = 'read'
    if(present(Action))then
       TypeAction = Action
       call lower_case(TypeAction)
    end if

    TypeAccess = 'sequential'
    if(present(Access))then
       TypeAccess = Access
       call lower_case(TypeAccess)
    end if

    if(present(iErrorOut)) iErrorOut = 0

    if(present(Recl))then
       if(present(iComm))then
          ! Get iProc and nProc
          call MPI_comm_size(iComm, nProc, iError)
          call MPI_comm_rank(iComm, iProc, iError)
          ! Open file with status "replace" on processor 0
          if(iProc == 0) &
               open(iUnit, FILE=File, FORM=TypeForm, STATUS='replace', &
               ACCESS=TypeAccess, ACTION=TypeAction, RECL=Recl, IOSTAT=iError)
          if(nProc > 1)then
             ! Check if open worked on processor 0
             if(iProc == 0 .and. iError /= 0)then
                write(*,*) NameSub,' iUnit, iError=', iUnit, iError
                if(present(NameCaller)) write(*,*) 'NameCaller=', NameCaller
                call CON_stop(NameSub// &
                     ' processor 0 could not open file='//trim(File))
             end if
             ! Make sure all processors wait until proc 0 has opened file
             call MPI_barrier(iComm, iError)
             ! Other processors open with status "old"
             if(iProc > 0) &
                  open(iUnit, FILE=File, FORM=TypeForm, STATUS='old', &
                  ACCESS=TypeAccess, ACTION=TypeAction, RECL=Recl, &
                  IOSTAT=iError)
          end if
       else
          open(iUnit, FILE=File, FORM=TypeForm, STATUS=TypeStatus, &
               ACCESS=TypeAccess, ACTION=TypeAction, RECL=Recl, IOSTAT=iError)
       end if
    else if(present(iUnitMpi)) then
       if(.not.present(iComm))then
          if(present(NameCaller)) write(*,*) 'NameCaller=', NameCaller
          call CON_stop(NameSub//' MPI IO requires iComm to be present')
       end if
       ! Open file with MPI I/O
       call MPI_file_open(iComm, File, MPI_MODE_WRONLY+MPI_MODE_CREATE, &
            MPI_INFO_NULL, iUnitMpi, iError)
    else
       open(iUnit, FILE=File, FORM=TypeForm, STATUS=TypeStatus, &
            POSITION=TypePosition, ACCESS=TypeAccess, ACTION=TypeAction, &
            IOSTAT=iError)
    end if

    if(iError /= 0)then
       if(present(iErrorOut))then
          iErrorOut = iError
       else
          write(*,*) NameSub,' iUnit, iError=', iUnit, iError
          if(present(NameCaller)) write(*,*) 'NameCaller=', NameCaller
          call CON_stop(NameSub//' could not open file='//trim(File))
       end if
    end if

  end subroutine open_file
  !============================================================================
  subroutine close_file(iUnitIn, Status, NameCaller)

    ! Interface for the Fortran close statement with error checking
    ! If an error occurs, the code stops and writes out the unit number,
    ! the error code and the name of the file.
    ! If NameCaller is present, it is also shown.
    ! If no unit number is passed, close UnitTmp_

    integer, optional, intent(in):: iUnitIn
    character(len=*), optional, intent(in):: Status
    character(len=*), optional, intent(in):: NameCaller

    integer:: iUnit
    integer:: iError

    character(len=*), parameter:: NameSub = 'close_file'
    !--------------------------------------------------------------------------
    iUnit = UnitTmp_
    if(present(iUnitIn)) iUnit = iUnitIn

    if(present(Status))then
       close(iUnit, STATUS=Status, IOSTAT=iError)
    else
       close(iUnit, IOSTAT=iError)
    end if

    if(iError /= 0)then
       write(*,*) NameSub,' iUnit, iError=', iUnit, iError
       if(present(NameCaller)) write(*,*) 'NameCaller=', NameCaller
       call CON_stop(NameSub//' could not close unit')
    end if

  end subroutine close_file
  !============================================================================
  subroutine remove_file(NameFile, NameCaller)

    ! Remove file NameFile if it exists
    ! Pass NameCaller to open_file and close_file in case of errors

    character(len=*), intent(in):: NameFile
    character(len=*), optional, intent(in):: NameCaller

    logical:: IsFound
    !--------------------------------------------------------------------------

    inquire(FILE=NameFile, EXIST=IsFound)
    if(.not.IsFound) RETURN

    call open_file(FILE=NameFile, NameCaller=NameCaller)
    call close_file(STATUS='DELETE', NameCaller=NameCaller)

  end subroutine remove_file
  !============================================================================
  subroutine touch_file(NameFile, NameCaller)

    ! Create file NameFile if it does not exist
    ! Pass NameCaller to open_file and close_file in case of errors

    character(len=*), intent(in):: NameFile
    character(len=*), optional, intent(in):: NameCaller

    !--------------------------------------------------------------------------
    call open_file(FILE=NameFile, NameCaller=NameCaller)
    call close_file(NameCaller=NameCaller)

  end subroutine touch_file
  !============================================================================
  subroutine flush_unit(iUnit)

    integer, intent(in) :: iUnit

    ! Flush the I/O unit iUnit if DoFlush is true
    !--------------------------------------------------------------------------
    if(DoFlush) flush(iUnit)

  end subroutine flush_unit
  !============================================================================
  subroutine split_string_simple(String, String_I, nStringOut, &
       StringSepIn, UseArraySyntaxIn, DoAddSeparator)

    character(len=*), intent(in) :: String      ! string to be split
    character(len=*), intent(out):: String_I(:) ! array of substrings
    integer,          optional, intent(out):: nStringOut ! number of substrings
    character(len=*), optional, intent(in):: StringSepIn ! separator string
    logical, optional, intent(in):: UseArraySyntaxIn     ! expand Var(10:20:2)
    logical, optional, intent(in):: DoAddSeparator ! add sep. to substrings
    !--------------------------------------------------------------------------
    call split_string(String, size(String_I), String_I, nStringOut, &
         StringSepIn, UseArraySyntaxIn, DoAddSeparator)

  end subroutine split_string_simple
  !============================================================================
  subroutine split_string(String, MaxString, String_I, nStringOut, &
       StringSepIn, UseArraySyntaxIn, DoAddSeparator)

    character(len=*),    intent(in):: String    ! string to be split
    integer,             intent(in):: MaxString ! maximum array size
    character (len=*), intent(out):: String_I(MaxString) ! array of substrings
    integer,          optional, intent(out):: nStringOut ! number of substrings
    character(len=*), optional, intent(in):: StringSepIn ! separator string
    logical, optional, intent(in):: UseArraySyntaxIn     ! expand Var(10:20:2)
    logical, optional, intent(in):: DoAddSeparator ! add sep. to substrings

    ! Cut the input string into an array of substrings. The separator
    ! string is either StringSepIn or a single space (default).
    ! Multiple consecutive separator strings are treated as one.
    ! Leading and trailing spaces are ignored. For example
    !\begin{verbatim}
    ! ' IE  GM ' --> nString=2, String\_I=(/'IE','GM'/)
    !\end{verbatim}
    ! When UseArraySyntax is present, then expand strings containing
    ! parens into an array of substrings ending with numbers, e.g.
    !\begin{verbatim}
    ! 'Var(4)'      --> nString=4,  String\_I=(/'Var1','Var2','Var3','Var4'/)
    ! 'Var(11)'     --> nString=11, String\_I=(/'Var01','Var02',...,'Var11'/)
    ! 'Var(3:5)'    --> nString=3,  String\_I=(/'Var3','Var4','Var5'/)
    ! 'Var(7:11:2)' --> nString=3,  String\_I=(/'Var07','Var09','Var11'/)
    !\end{verbatim}

    integer          :: nString
    character(len=10):: StringSep
    logical          :: UseArraySyntax

    character(len=len(String)+10) :: StringTmp
    character(:), allocatable :: StringTmp2

    integer :: i, l, lSep, lKeep

    character(len=*), parameter:: NameSub = 'split_string'
    !--------------------------------------------------------------------------
    if(present(StringSepIn))then
       StringSep = StringSepIn
       lSep      = len(StringSepIn)
    else
       StringSep = ' '
       lSep      = 1
    end if

    UseArraySyntax = .false.
    if(present(UseArraySyntaxIn)) UseArraySyntax = UseArraySyntaxIn

    lKeep = 0
    if(present(DoAddSeparator))then
       if(DoAddSeparator) lKeep = lSep
    end if

    nString   = 0
    StringTmp = String
    l         = len_trim(StringTmp)
    StringTmp = trim(StringTmp) // StringSep(1:lSep) ! Add separator to the end
    do
       StringTmp = adjustl(StringTmp)          ! Remove leading spaces
       i = index(StringTmp, StringSep(1:lSep)) ! Find end of first part
       if(i <= 1) EXIT                         ! Nothing before the separator
       nString = nString + 1                   ! Count parts

       if(lKeep>0)then                         ! Do not keep added separator
          if(i+lKeep >= len_trim(StringTmp)) lKeep = 0
       end if

       String_I(nString) = StringTmp(1:i-1+lKeep) ! Put part into string array
       StringTmp2= StringTmp(i+lSep:l+lSep) ! Delete part+separator from string
       StringTmp = StringTmp2               ! Delete part+separator from string

       if(UseArraySyntax) call expand_array(String_I(nString))

       if(nString == MaxString) EXIT        ! Check for maximum number of parts
    end do

    if(present(nStringOut)) nStringOut = nString

  contains
    !==========================================================================
    subroutine expand_array(String1)

      ! Expand String1 if it contains array syntax, e.g.
      ! "Var(04)"     to   "Var01", "Var02", "Var03", "Var04"
      ! "Var(2:4)"    to   "Var2", "Var3", "Var4"
      ! "Var(8:12:2)" to   "Var08", "Var10", "Var12"

      character(len=*), intent(inout):: String1

      character(len=len(String1)) :: String2
      character(len=6):: StringFormat

      integer:: j, k, l, m, lNum, iFirst, iLast, Di, iNum, iError
      !------------------------------------------------------------------------
      ! Find the opening paren if any
      j = index(String1,'(')
      if(j < 1) RETURN
      k = index(String1,')')

      if(k < j) &
           call CON_stop(NameSub//' missing closing paren in String='//String)

      ! Check for colon
      l = index(String1,':')
      if(l > j)then
         ! read initial index value before the first colon
         read(String1(j+1:l-1),*,IOSTAT=iError) iFirst
         if(iError /= 0 .or. iFirst < 1) call CON_stop(NameSub// &
              ' invalid initial index value in String='//String)
      else
         iFirst = 1
         l = j
      end if

      ! Check for a second colon
      m = index(String1,':',back=.true.)
      if(m > l)then
         ! read index stride value after the seecond colon
         read(String1(m+1:k-1),*,IOSTAT=iError) Di
         if(iError /= 0 .or. Di < 1) call CON_stop(NameSub// &
              ' invalid index stride value in String='//String)
      else
         Di = 1
         m  = k
      end if

      ! read the last index value between the l and m positions
      read(String1(l+1:m-1),*,IOSTAT=iError) iLast
      if(iError /= 0 .or. iLast < iFirst) call CON_stop(NameSub// &
           ' invalid maximum index value in String='//String)

      ! Set length of numerical string and the format string
      lNum = m - l - 1
      write(StringFormat,'(a,i1,a,i1,a)') "(i",lNum,".",lNum,")"

      ! Set the beginning part of the string to the variable name
      String2 = ' '
      String2(1:j-1) = String1(1:j-1)

      ! Expand variable names by repating name and adding numerical value
      nString = nString - 1
      do iNum = iFirst, iLast, Di

         write(String2(j:j+lNum),StringFormat) iNum
         nString = nString + 1
         String_I(nString) = String2

         if(nString == MaxString) RETURN

      end do
    end subroutine expand_array
    !==========================================================================
  end subroutine split_string
  !============================================================================
  subroutine join_string_simple(String_I, String, StringSepIn)

    character(len=*), intent(in) :: String_I(:)
    character(len=*), intent(out):: String
    character(len=*), optional, intent(in):: StringSepIn
    !--------------------------------------------------------------------------
    call join_string(size(String_I), String_I, String, StringSepIn)

  end subroutine join_string_simple
  !============================================================================
  subroutine join_string(nString, String_I, String, StringSepIn)

    integer,          intent(in) :: nString
    character(len=*), intent(in) :: String_I(nString)
    character(len=*), intent(out):: String
    character(len=*), optional, intent(in):: StringSepIn

    ! Join the input string array into one string. The parts are joined
    ! with spaces or the optional StringSepIn string (up to 10 characters).

    character(len=10):: StringSep

    integer :: i, l

    character(len=*), parameter:: NameSub = 'join_string'
    !--------------------------------------------------------------------------
    if(present(StringSepIn))then
       StringSep = StringSepIn
       l = len(StringSepIn)
    else
       StringSep = ' '
       l = 1
    endif
    String = String_I(1)

    do i = 2, nString
       String = trim(String) // StringSep(1:l) // String_I(i)
    end do

  end subroutine join_string
  !============================================================================
  subroutine upper_case(String)

    character (len=*), intent(inout) :: String

    ! Change characters to upper case in String

    integer, parameter :: iA=ichar('a'), iZ=ichar('z'), Di=ichar('A')-iA
    integer :: i, iC
    !--------------------------------------------------------------------------
    do i = 1, len_trim(String)
       iC = ichar(String(i:i))
       if(iC >= iA .and. iC <= iZ) String(i:i) = char(iC+Di)
    end do

  end subroutine upper_case
  !============================================================================
  subroutine lower_case(String)

    character (len=*), intent(inout) :: String

    ! Change characters to lower case in String

    integer, parameter :: iA=ichar('A'), iZ=ichar('Z'), Di=ichar('a')-iA
    integer :: i, iC
    !--------------------------------------------------------------------------
    do i = 1, len_trim(String)
       iC = ichar(String(i:i))
       if(iC >= iA .and. iC <= iZ) String(i:i) = char(iC+Di)
    end do

  end subroutine lower_case
  !============================================================================
  subroutine string_to_char_array(String, String_I)

    ! Convert Fortran string into a C-style character array
    ! Ignore trailing spaces.
    ! Add null character to the end. Return length if needed.

    character(len=*),  intent(in) :: String
    character,         intent(out):: String_I(*)

    integer:: i, n
    !--------------------------------------------------------------------------
    n = len_trim(String)
    do i = 1, n
       String_I(i) = String(i:i)
    end do
    String_I(n+1) = c_null_char

  end subroutine string_to_char_array
  !============================================================================
  subroutine char_array_to_string(String_I, String)

    ! Convert C-style character array into a Fortran string.

    character,         intent(in) :: String_I(*)
    character(len=*),  intent(out):: String

    integer:: i
    !--------------------------------------------------------------------------
    String = ' '
    do i = 1, len(String)
       if(String_I(i) == c_null_char) EXIT
       String(i:i) = String_I(i)
    end do

  end subroutine char_array_to_string
  !============================================================================
  subroutine sleep(DtCpu)

    real, intent(in) :: DtCpu  ! CPU time to sleep (in seconds)

    real(Real8_) :: tCpu0

    ! This subroutine returns after the number of seconds
    ! given in its argument.
    !--------------------------------------------------------------------------
    tCpu0 = MPI_WTIME()
    do
       if(MPI_WTIME() > tCpu0 + DtCpu) RETURN
    end do

  end subroutine sleep
  !============================================================================
  subroutine check_allocate(iError, NameArray)

    integer,intent(in)::iError
    character(LEN=*),intent(in)::NameArray

    !--------------------------------------------------------------------------
    if (iError > 0) call CON_stop('check_allocate F90_ERROR '// &
         'Could not allocate array '//NameArray)

  end subroutine check_allocate
  !============================================================================
  recursive function greatest_common_divisor(i, j) result(kGCD)
    ! Calculate the greatest common divisor of i and j
    ! with Euclid's algorithm

    integer, intent(in):: i, j
    integer:: kGCD
    !--------------------------------------------------------------------------
    if(j == 0)then
       kGCD = i
    else
       kGCD = greatest_common_divisor(j, mod(i, j))
    end if

  end function greatest_common_divisor
  !============================================================================
  subroutine test_mod_utility

    ! Unit tests

    character(len=500):: String
    character:: StringC_I(501)
    integer, parameter :: MaxString = 20
    integer :: nString
    character(len=30) :: String_I(MaxString)
    integer :: iString, l
    integer:: iError

    character(len=*), parameter:: NameSub = 'test_mod_utility'
    !--------------------------------------------------------------------------
    write(*,'(a)') 'testing check_dir'
    write(*,'(a)') 'check directory "."'
    call check_dir('.')
    write(*,'(a)') 'check_dir returned successfully'
    write(*,'(a)') 'check directory "xxx/"'
    call check_dir('xxx/', iErrorOut=iError)
    if(iError == 0) write(*,*) 'ERROR: iError should not be zero!'

    write(*,'(a)') 'testing make_dir'
    write(*,'(a)') 'make directory "xxx/yyy"'
    call make_dir('xxx/yyy')
    call check_dir('xxx') ! Should not stop
    call check_dir('xxx/yyy', iErrorOut=iError)
    if(iError /= 0) write(*,*) 'ERROR: iError should be zero! iError=', iError
    write(*,'(a)') 'make xxx/ directory again (should not produce an error)'
    call make_dir('xxx', iErrorOut=iError)
    write(*,*) iError

    write(*,'(a)') 'testing open_file and close_file'

    ! Use defaults
    call open_file(FILE='xxx/testfile.dat')
    write(UnitTmp_,'(a)') 'Some text'
    call close_file
    ! Create an error message by passing incorrect filename
    ! Since the error code varies by compiler, this is commented out
    ! call open_file(FILE='xxx/testfile.bad', STATUS='old', &
    !     iErrorOut=iError, NameCaller=NameSub)

    ! Use all arguments
    call open_file(UnitTmp_, FILE='xxx/testfile.dat', FORM='formatted', &
         STATUS='old', POSITION='append', NameCaller=NameSub)
    write(UnitTmp_,'(a)') 'More text'
    call close_file(UnitTmp_, STATUS='delete', NameCaller=NameSub)

    write(*,'(a)') 'testing touch_file and remove_file'
    call touch_file('xxx/touched', NameCaller=NameSub)
    call touch_file('xxx/touched', NameCaller=NameSub)
    call remove_file('xxx/touched', NameCaller=NameSub)
    call remove_file('xxx/touched', NameCaller=NameSub)

    write(*,'(/,a)') 'testing fix_dir_name'
    String = ' '
    call fix_dir_name(String)
    write(*,'(a)') 'fixed empty string=' // trim(String)
    String = 'GM/BATSRUS/data'
    write(*,'(a)') 'original    string=' // trim(String)
    call fix_dir_name(String)
    write(*,'(a)') 'fixed first string=' // trim(String)
    call fix_dir_name(String)
    write(*,'(a)') 'fixed again string=' // trim(String)

    write(*,'(/,a)') 'testing split_string'
    String = '  a(3)  bb(04:06) c,ddd ee,ff gg(8:12:2) '
    write(*,'(a)') 'String=' // trim(String)

    call split_string(String, MaxString, String_I, nString)
    write(*,'(a,i3,a)') 'with space separator split to', nString, ' parts:'
    do iString = 1, nString
       write(*,'(a)') trim(String_I(iString))
    end do

    call split_string(String, String_I, nString, ",")
    write(*,'(a,i3,a)') 'with "," separator split to', nString, ' parts:'
    do iString = 1, nString
       write(*,'(a)') trim(String_I(iString))
    end do

    call split_string(String, String_I, nString, ") ", DoAddSeparator=.true.)
    write(*,'(a,i3,a)') 'with ") " separator split to', nString, ' parts:'
    do iString = 1, nString
       write(*,'(a)') trim(String_I(iString))
    end do

    call split_string(String, String_I, nString, UseArraySyntaxIn=.true.)
    write(*,'(a,i3,a)') 'with UseArraySyntax split to', nString,' parts:'
    do iString = 1, nString
       write(*,'(a)') trim(String_I(iString))
    end do

    write(*,'(/,a)') 'testing join_string'
    call join_string(nString, String_I, String)
    write(*,'(a)') 'joined string='//trim(String)

    call join_string(String_I(1:4), String, '; ')
    write(*,'(a)') 'joined string='//trim(String)

    write(*,'(/,a)') 'testing upper_case and lower_case'
    String = 'abCD 123:'
    write(*,'(a)') 'mixed case string='//trim(String)
    call upper_case(String)
    write(*,'(a)') 'upper case string='//trim(String)
    call lower_case(String)
    write(*,'(a)') 'lower case string='//trim(String)

    write(*,'(/,a)') 'testing string_to_char_array'
    String = "it's a string"
    call string_to_char_array(String, StringC_I)
    l = len_trim(String)
    write(*,'(a,100a1)')'C character array: ', StringC_I(1:l)
    if(StringC_I(l+1) /= c_null_char) &
         write(*,*)'Error: null terminator is missing'

    write(*,'(/,a)') 'testing char_array_to_string'
    call char_array_to_string(StringC_I, String)
    write(*,'(a,a)')    'Fortran string   : ', trim(String)
    if(String /= "it's a string") &
         write(*,*)'Error: incorrect conversion to Fortran String'

    write(*,'(/,a)') 'testing greatest_common_divisor'
    l = greatest_common_divisor(36, 26)
    if(l /= 2) &
         write(*,*)'Error: greatest_common_divisor(36,26)=', l,' should be 2'

    l = greatest_common_divisor(26, 36)
    if(l /= 2) &
         write(*,*)'Error: greatest_common_divisor(26,36)=', l,' should be 2'

    l = greatest_common_divisor(36, 12)
    if(l /= 12) &
         write(*,*)'Error: greatest_common_divisor(36,12)=', l,' should be 12'

    l = greatest_common_divisor(12, 36)
    if(l /= 12) &
         write(*,*)'Error: greatest_common_divisor(12,36)=', l,' should be 12'

  end subroutine test_mod_utility
  !============================================================================
  function norm2(x_I) result(Norm)
    !$acc routine seq

    ! Does the same as the intrinsic norm2 function in Fortran 2008
    ! It is needed because nvfortran+ACC does not have norm2 implemented

    real, intent(in):: x_I(:)
    real:: Norm
    !--------------------------------------------------------------------------
    Norm = sqrt(sum(x_I**2))

  end function norm2
  !============================================================================
  subroutine find_cell4(MinCoord, MaxCoord, Coord, iCoord, dCoord, &
       Coord_I, DoExtrapolate, StringError, IsInside)
    !$acc routine seq

    ! Single precision coordinates.
    !
    ! Find cell index and distance from cell for either
    ! - a uniform grid with normalized coordinate (Coord_I is NOT present)
    ! - a nonuniform grid with monotone coordinates (Coord_I is present)
    !
    ! For sake of easy usage the returned coordinate index iCoord always
    ! satisfies MinCoord <= iCoord < MaxCoord.
    !
    ! If the coordinate is out of bounds, and DoExtrapolate is not present,
    ! the code stops with an error message. If DoExtrapolate is present and
    ! false, dCoord is modified to 0 or 1 so that the last grid cell is used.
    ! If DoExtrapolate is true, iCoord and dCoord are set
    ! corresponding to linear extrapolation from the last two grid values.
    !
    ! For interpolation the normalized distance dCoord measured from
    ! coordinate iCoord satisfies 0.0 <= dCoord <= 1.0
    ! but for extrapolation dCoord < 0.0 or > 1.0 is also possible.
    !---
    ! FOR THE UNIFORM CASE the normalized coordinate Coord should be equal to
    ! the index at the cell centers, therefore:
    !
    ! iCoord = max(MinCoord, min(MaxCoord-1, floor(Coord)))
    ! dCoord = Coord - iCoord
    !
    ! The optional IsInside = MinCoord <= Coord <= MaxCoord

    ! IN THE NON-UNIFORM CASE the cell iCoord that is left to coordinate Coord
    ! is found with a binary search in the Coord_I coordinates.
    !
    ! The normalized distance is set to
    !    dCoord = (Coord-Coord_I(iCoord))/(Coord_I(iCoord+1)-Coord_I(iCoord))
    !
    ! The optional IsInside = Coord_I(1) <= Coord <= Coord_I(nCoord).

    ! Example for linear interpolation on a 1D uniform grid of nX cells,
    ! DeltaX cell size and the first cell center is at DeltaX/2:
    !
    !   call find_cell(1, nX, x/DeltaX+0.5, iX, d)
    !   State_V = (1.0 - d)*State_VC(:,iX) + d*State_VC(:,iX+1)
    !
    ! Example for linear interpolation on a 1D non-uniform grid
    ! with 2 ghost cells:
    !
    !   call find_cell(-1, nI+2, x, iX, d, x_G)
    !   State_V = (1.0 - d)*State_VG(:,iX) + d*State_VG(:,iX+1)

    integer,          intent(in)           :: MinCoord, MaxCoord
    real(Real4_),     intent(in)           :: Coord
    integer,          intent(out)          :: iCoord
    real,             intent(out), optional:: dCoord
    real(Real4_),     intent(in),  optional:: Coord_I(MinCoord:)
    logical,          intent(in),  optional:: DoExtrapolate
    character(len=*), intent(in),  optional:: StringError
    logical,          intent(out), optional:: IsInside

    integer:: i, Di

    logical:: IsUniform

    character(len=*), parameter:: NameSub = 'find_cell4'
    !--------------------------------------------------------------------------
    if(present(Coord_I)) then
       IsUniform = size(Coord_I) < 2
    else
       IsUniform = .true.
    endif

    if(IsUniform)then
       ! Uniform grid case with normalized coordinate

       iCoord = min(MaxCoord-1, max(MinCoord, floor(Coord)))
       dCoord = Coord - iCoord

       if(Coord < MinCoord)then
          if(.not. (present(DoExtrapolate))) then
             if(present(StringError)) write(*,*) NameSub, ': ', StringError
             write(*,*) NameSub,': MinIndex, MaxIndex=', MinCoord, MaxCoord
             write(*,*) NameSub,': Coord=', Coord
             call CON_stop_simple(NameSub// &
                  ': normalized coordinate is too small!')
          elseif(.not.DoExtrapolate)then
             ! Use lefttmost cell (first order accurate)
             dCoord = 0.0
          end if
          if(present(IsInside)) IsInside = .false.
       elseif(Coord > MaxCoord)then
          if(.not. (present(DoExtrapolate))) then
             if(present(StringError)) write(*,*) StringError
             write(*,*) NameSub,': MinIndex, MaxIndex=', MinCoord, MaxCoord
             write(*,*) NameSub,': Coord=', Coord
             call CON_stop_simple(NameSub// &
                  ': normalized coordinate is too large!')
          elseif(.not.DoExtrapolate)then
             ! Use rightmost cell (first order accurate)
             dCoord = 1.0
          endif
          if(present(IsInside)) IsInside = .false.
       else
          if(present(IsInside)) IsInside = .true.
       end if

    elseif(Coord_I(MinCoord) < Coord_I(MaxCoord))then

       ! Monotone increasing coordinates

       if(Coord < Coord_I(MinCoord))then
          if(.not. (present(DoExtrapolate))) then
             if(present(StringError)) write(*,*) StringError
             write(*,*) NameSub,': MinIndex, MaxIndex=', MinCoord, MaxCoord
             write(*,*) NameSub,': Coord, CoordMin, CoordMax=', &
                  Coord, Coord_I(MinCoord), Coord_I(MaxCoord)
             call CON_stop_simple(NameSub//': coordinate is too small!')
          elseif(DoExtrapolate)then
             iCoord = MinCoord
             dCoord = (Coord - Coord_I(iCoord)) &
                  /   (Coord_I(iCoord+1) - Coord_I(iCoord))
          else
             iCoord = MinCoord
             dCoord = 0.0
          end if
          if(present(IsInside)) IsInside = .false.
          RETURN
       end if

       if(Coord > Coord_I(MaxCoord))then
          if(.not. (present(DoExtrapolate))) then
             if(present(StringError)) write(*,*) StringError
             write(*,*) NameSub,': MinIndex, MaxIndex=', MinCoord, MaxCoord
             write(*,*) NameSub,': Coord, CoordMin, CoordMax=', &
                  Coord, Coord_I(MinCoord), Coord_I(MaxCoord)
             call CON_stop_simple(NameSub//': coordinate is too large!')
          elseif(DoExtrapolate)then
             iCoord = MaxCoord - 1
             dCoord = (Coord - Coord_I(iCoord))  &
                  /   (Coord_I(iCoord+1) - Coord_I(iCoord))
          else
             iCoord = MaxCoord - 1
             dCoord = 1.0
          end if
          if(present(IsInside)) IsInside = .false.
          RETURN
       end if

       if(present(IsInside)) IsInside = .true.

       ! binary search
       i  = (MinCoord + MaxCoord)/2
       Di = (MaxCoord - MinCoord)/2
       do
          Di = (Di + 1)/2
          if(Coord < Coord_I(i)) then
             i = max(MinCoord, i - Di)
          elseif(Coord > Coord_I(i+1))then
             i = min(MaxCoord-1, i + Di)
          else
             EXIT
          end if
       end do
       iCoord = i
       if(Coord_I(iCoord+1) == Coord_I(iCoord))then
          dCoord = 0.0
       else
          dCoord = (Coord             - Coord_I(iCoord)) &
               /   (Coord_I(iCoord+1) - Coord_I(iCoord))
       end if
    else

       ! Monotone decreasing coordinates

       if(Coord < Coord_I(MaxCoord))then
          if(.not. (present(DoExtrapolate))) then
             if(present(StringError)) write(*,*) StringError
             write(*,*) NameSub,': MinIndex, MaxIndex=', MinCoord, MaxCoord
             write(*,*) NameSub,': Coord, CoordMin, CoordMax=', &
                  Coord, Coord_I(MaxCoord), Coord_I(MinCoord)
             call CON_stop_simple(NameSub//': coordinate is too small!')
          elseif(DoExtrapolate)then
             iCoord = MaxCoord - 1
             dCoord = (Coord_I(iCoord) - Coord) &
                  /   (Coord_I(iCoord) - Coord_I(iCoord+1))
          else
             iCoord = MaxCoord - 1
             dCoord = 1.0
          end if
          if(present(IsInside)) IsInside = .false.
          RETURN
       end if

       if(Coord > Coord_I(MinCoord))then
          if(.not. (present(DoExtrapolate))) then
             if(present(StringError)) write(*,*) StringError
             write(*,*) NameSub,': MinIndex, MaxIndex=', MinCoord, MaxCoord
             write(*,*) NameSub,': Coord, CoordMin, CoordMax=', &
                  Coord, Coord_I(MaxCoord), Coord_I(MinCoord)
             call CON_stop_simple(NameSub//': coordinate is too large!')
          elseif(DoExtrapolate)then
             iCoord = MinCoord
             dCoord = (Coord_I(iCoord) - Coord)  &
                  /   (Coord_I(iCoord) - Coord_I(iCoord+1))
          else
             iCoord = MinCoord
             dCoord = 0.0
          end if
          if(present(IsInside)) IsInside = .false.
          RETURN
       end if

       if(present(IsInside)) IsInside = .true.

       ! binary search
       i  = (MinCoord + MaxCoord)/2
       Di = (MaxCoord - MinCoord)/2
       do
          Di = (Di + 1)/2
          if(Coord > Coord_I(i)) then
             i = max(MinCoord, i - Di)
          elseif(Coord < Coord_I(i+1))then
             i = min(MaxCoord-1, i + Di)
          else
             EXIT
          end if
       end do
       iCoord = i
       if(Coord_I(iCoord+1) == Coord_I(iCoord))then
          dCoord = 0.0
       else
          dCoord = (Coord_I(iCoord) - Coord  ) &
               /   (Coord_I(iCoord) - Coord_I(iCoord+1))
       end if
    end if

  end subroutine find_cell4
  !============================================================================
  subroutine find_cell8(MinCoord, MaxCoord, Coord, iCoord, dCoord, &
       Coord_I, DoExtrapolate, StringError, IsInside)
    !$acc routine seq

    ! double precision version of the subroutine above

    integer,          intent(in)           :: MinCoord, MaxCoord
    real(Real8_),     intent(in)           :: Coord
    integer,          intent(out)          :: iCoord
    real,             intent(out), optional:: dCoord
    real(Real8_),     intent(in),  optional:: Coord_I(MinCoord:)
    logical,          intent(in),  optional:: DoExtrapolate
    character(len=*), intent(in),  optional:: StringError
    logical,          intent(out), optional:: IsInside

    integer:: i, Di, nIter, MaxIter
    real(Real8_):: Tolerance

    logical:: IsUniform

    character(len=*), parameter:: NameSub = 'find_cell8'
    !--------------------------------------------------------------------------
    if(present(Coord_I)) then
       IsUniform = size(Coord_I) < 2
    else
       IsUniform = .true.
    endif

    ! Assume that Coord is inside the range. Set false later if not.
    if(present(IsInside)) IsInside = .true.

    if(IsUniform)then
       ! Uniform grid case with normalized coordinate

       iCoord = min(MaxCoord-1, max(MinCoord, floor(Coord)))
       dCoord = Coord - iCoord
       Tolerance = (MaxCoord - MinCoord)*1d-12

       if(Coord < MinCoord - Tolerance)then
          if(.not. (present(DoExtrapolate))) then
             if(present(StringError)) write(*,*) NameSub, ': ', StringError
             write(*,*) NameSub,': MinIndex, MaxIndex=', MinCoord, MaxCoord
             write(*,*) NameSub,': Coord=', Coord
             call CON_stop_simple(NameSub// &
                  ': normalized coordinate is too small!')
          elseif(.not.DoExtrapolate)then
             ! Use lefttmost cell (first order accurate)
             dCoord = 0.0
          end if
          if(present(IsInside)) IsInside = .false.
       elseif(Coord > MaxCoord + Tolerance)then
          if(.not. (present(DoExtrapolate))) then
             if(present(StringError)) write(*,*) StringError
             write(*,*) NameSub,': MinIndex, MaxIndex=', MinCoord, MaxCoord
             write(*,*) NameSub,': Coord=', Coord
             call CON_stop_simple(NameSub// &
                  ': normalized coordinate is too large!')
          elseif(.not.DoExtrapolate)then
             ! Use rightmost cell (first order accurate)
             dCoord = 1.0
          endif
          if(present(IsInside)) IsInside = .false.
       end if

    elseif(Coord_I(MinCoord) < Coord_I(MaxCoord))then

       ! Monotone increasing coordinates
       Tolerance = (Coord_I(MaxCoord) - Coord_I(MinCoord))*1d-12

       if(Coord < Coord_I(MinCoord) - Tolerance)then
          if(.not. (present(DoExtrapolate))) then
             if(present(StringError)) write(*,*) StringError
             write(*,*) NameSub,': MinIndex, MaxIndex=', MinCoord, MaxCoord
             write(*,*) NameSub,': Coord, CoordMin, CoordMax=', &
                  Coord, Coord_I(MinCoord), Coord_I(MaxCoord)
             call CON_stop_simple(NameSub//': coordinate is too small!')
          elseif(DoExtrapolate)then
             iCoord = MinCoord
             dCoord = (Coord - Coord_I(iCoord)) &
                  /   (Coord_I(iCoord+1) - Coord_I(iCoord))
          else
             iCoord = MinCoord
             dCoord = 0.0
          end if
          if(present(IsInside)) IsInside = .false.
          RETURN
       elseif(Coord <= Coord_I(MinCoord))then
          ! Very near the edge
          iCoord = MinCoord
          dCoord = 0.0
          RETURN
       end if

       if(Coord > Coord_I(MaxCoord) + Tolerance)then
          if(.not. (present(DoExtrapolate))) then
             if(present(StringError)) write(*,*) StringError
             write(*,*) NameSub,': MinIndex, MaxIndex=', MinCoord, MaxCoord
             write(*,*) NameSub,': Coord, CoordMin, CoordMax=', &
                  Coord, Coord_I(MinCoord), Coord_I(MaxCoord)
             call CON_stop_simple(NameSub//': coordinate is too large!')
          elseif(DoExtrapolate)then
             iCoord = MaxCoord - 1
             dCoord = (Coord - Coord_I(iCoord))  &
                  /   (Coord_I(iCoord+1) - Coord_I(iCoord))
          else
             iCoord = MaxCoord - 1
             dCoord = 1.0
          end if
          if(present(IsInside)) IsInside = .false.
          RETURN
       elseif(Coord >= Coord_I(MaxCoord))then
          ! Very near the edge
          iCoord = MaxCoord - 1
          dCoord = 1.0
          RETURN
       end if

       ! binary search
       i  = (MinCoord + MaxCoord)/2
       Di = (MaxCoord - MinCoord)/2
       nIter   = 0
       MaxIter = MaxCoord - MinCoord ! Even a linear search would end
       do
          Di = (Di + 1)/2
          if(Coord < Coord_I(i)) then
             i = max(MinCoord, i - Di)
          elseif(Coord > Coord_I(i+1))then
             i = min(MaxCoord-1, i + Di)
          else
             EXIT
          end if
          nIter = nIter + 1
          if (nIter > MaxIter) then
#ifndef _OPENACC
             write(*,*) NameSub,': ERROR in monotone increasing: '
             write(*,*) 'Tolerance=', Tolerance
             write(*,*) 'Coord_I(MinCoord), Coord_I(MaxCoord), Coord=', &
                  Coord_I(MinCoord), Coord_I(MaxCoord), Coord
             write(*,*) 'i, Di, Coord_I(i:i+1)=', i, Di, Coord_I(i:i+1)
#endif
             call CON_stop_simple(NameSub//': maximum iteration exceeded!')
          end if
       end do
       iCoord = i
       if(Coord_I(iCoord+1) == Coord_I(iCoord))then
          dCoord = 0.0
       else
          dCoord = (Coord             - Coord_I(iCoord)) &
               /   (Coord_I(iCoord+1) - Coord_I(iCoord))
       end if
    else

       ! Monotone decreasing coordinates
       Tolerance = (Coord_I(MinCoord) - Coord_I(MaxCoord))*1d-12

       if(Coord < Coord_I(MaxCoord) - Tolerance)then
          if(.not. (present(DoExtrapolate))) then
             if(present(StringError)) write(*,*) StringError
             write(*,*) NameSub,': MinIndex, MaxIndex=', MinCoord, MaxCoord
             write(*,*) NameSub,': Coord, CoordMin, CoordMax=', &
                  Coord, Coord_I(MaxCoord), Coord_I(MinCoord)
             call CON_stop_simple(NameSub//': coordinate is too small!')
          elseif(DoExtrapolate)then
             iCoord = MaxCoord - 1
             dCoord = (Coord_I(iCoord) - Coord) &
                  /   (Coord_I(iCoord) - Coord_I(iCoord+1))
          else
             iCoord = MaxCoord - 1
             dCoord = 1.0
          end if
          if(present(IsInside)) IsInside = .false.
          RETURN
       elseif(Coord <= Coord_I(MaxCoord))then
          ! Very near the edge
          iCoord = MaxCoord - 1
          dCoord = 1.0
          RETURN
       end if

       if(Coord > Coord_I(MinCoord) + Tolerance)then
          if(.not. (present(DoExtrapolate))) then
             if(present(StringError)) write(*,*) StringError
             write(*,*) NameSub,': MinIndex, MaxIndex=', MinCoord, MaxCoord
             write(*,*) NameSub,': Coord, CoordMin, CoordMax=', &
                  Coord, Coord_I(MaxCoord), Coord_I(MinCoord)
             call CON_stop_simple(NameSub//': coordinate is too large!')
          elseif(DoExtrapolate)then
             iCoord = MinCoord
             dCoord = (Coord_I(iCoord) - Coord)  &
                  /   (Coord_I(iCoord) - Coord_I(iCoord+1))
          else
             iCoord = MinCoord
             dCoord = 0.0
          end if
          if(present(IsInside)) IsInside = .false.
          RETURN
       elseif(Coord >= Coord_I(MinCoord))then
          ! Very near the edge
          iCoord = MinCoord
          dCoord = 0.0
          RETURN
       end if

       ! binary search
       i  = (MinCoord + MaxCoord)/2
       Di = (MaxCoord - MinCoord)/2
       nIter = 0
       MaxIter = MaxCoord - MinCoord ! Even a linear search ends by this
       do
          Di = (Di + 1)/2
          if(Coord > Coord_I(i)) then
             i = max(MinCoord, i - Di)
          elseif(Coord < Coord_I(i+1))then
             i = min(MaxCoord-1, i + Di)
          else
             EXIT
          end if
          nIter = nIter + 1
          if (nIter > MaxIter) then
#ifndef _OPENACC
             write(*,*) NameSub,': ERROR in monotone decreasing: '
             write(*,*) 'Tolerance=', Tolerance
             write(*,*) 'Coord_I(MinCoord), Coord_I(MaxCoord), Coord=', &
                  Coord_I(MinCoord), Coord_I(MaxCoord), Coord
             write(*,*) 'i, Di, Coord_I(i:i+1)=', i, Di, Coord_I(i:i+1)
#endif
             call CON_stop_simple(NameSub//': maximum iteration exceeded!')
          end if
       end do
       iCoord = i
       if(Coord_I(iCoord+1) == Coord_I(iCoord))then
          dCoord = 0.0
       else
          dCoord = (Coord_I(iCoord) - Coord  ) &
               /   (Coord_I(iCoord) - Coord_I(iCoord+1))
       end if
    end if

  end subroutine find_cell8
  !============================================================================
  integer function i_gang(iBlock)
    !$acc routine seq
    integer, intent(in) :: iBlock
#ifndef _OPENACC
    !--------------------------------------------------------------------------
    i_gang = 1
#else
    i_gang = iBlock
#endif
  end function i_gang
  !============================================================================
#ifdef _OPENACC
  subroutine init_gpu(iComm, iProc, nGpu, iGpu)

    ! Return number of GPUs (nGPU) and the index of the selected GPU for
    ! MPI process iProc. Each MPI process should get one GPU.
    ! based on the node-local (shared-memory capable) MPI communicator

    ! Call set_acc_error_handler so the code can stop all MPI processes
    ! unless the environment variable SWMF_NOACCERRORHANDLER is set to "Y".

#ifndef NOACCMODULE
    use openacc
#endif

    integer, intent(in) :: iComm, iProc
    integer, intent(out):: nGpu, iGpu

    integer :: iLocalComm, iLocalProc, nLocalProc, iError

    character(len=1) :: StringEnvVar
    character(len=*), parameter :: NameEnvVar= 'SWMF_NOACCERRORHANDLER'

    character(len=*), parameter:: NameSub = 'init_gpu'
    !--------------------------------------------------------------------------
    call MPI_comm_split_type(iComm, MPI_COMM_TYPE_SHARED, iProc, &
         MPI_INFO_NULL, iLocalComm, iError)

    ! Determine the number of local processes and the local rank
    call MPI_comm_size(iLocalComm, nLocalProc, iError)
    call MPI_comm_rank(iLocalComm, iLocalProc, iError)

#ifdef NOACCMODULE
    nGpu = nLocalProc
#else
    ! Determine the number of GPUs
    nGpu = acc_get_num_devices(ACC_DEVICE_NVIDIA)
#endif
    if (nGpu <= 0) call CON_stop(NameSub//': No GPUs detected on the node')

    iGpu = iLocalProc
    if (nLocalProc > nGpu) then ! we have more processes than GPUs
       if (iLocalProc==0) write (*,*) NameSub, " WARNING:", &
            ' iProc, nLocalProc > nGpu=', iProc, nLocalProc, nGpu
       iGpu = mod(iLocalProc, nGpu)
    end if

    ! set the device number we will be operating on
    !$acc set device_num(iGpu)
    !$acc init device_num(iGpu)

    call MPI_Comm_free(iLocalComm, iError)

#ifndef NOACCMODULE
    ! Set the OpenACC error handler so it can stop all MPI processes
    ! unless requested otherwise for debugging purposes
    call get_environment_variable(NameEnvVar, StringEnvVar)
    if (StringEnvVar == "Y") then
       write (*,*) NameSub, ': WARNING not setting OpenACC error handler!'
    else
       call set_acc_error_handler()
    end if
#endif

  end subroutine init_gpu
  !============================================================================
  subroutine acc_error_handler() bind(C)

    ! This routine is called by OpenACC when encountering stop.
    ! It calls CON_stop, which calls MPI_abort.
    !--------------------------------------------------------------------------
    call CON_stop("OpenACC error!")

  end subroutine acc_error_handler
  !============================================================================
#ifndef NOACCMODULE
  subroutine set_acc_error_handler()

    use iso_c_binding, ONLY: c_funloc

    ! Set "subroutine acc_error_handler" to be used by OpenACC

    interface
       subroutine acc_set_error_routine(FuncPtr) BIND(C)
         use iso_c_binding, only: C_FUNPTR
         type(C_FUNPTR), intent(in), value :: FuncPtr
       end subroutine acc_set_error_routine
    end interface
    !--------------------------------------------------------------------------
    call acc_set_error_routine(c_funloc(acc_error_handler))

  end subroutine set_acc_error_handler
  !============================================================================
#endif
#endif
  subroutine int_to_ascii_code(n, nLen, iAscii_I, DoOmitSign, DoFillZero)
    !$acc routine seq
    ! Example: num = -12345 with nLen = 9.
    ! Output Ascii_I = '-00012345'
    integer, intent(in) :: n, nLen
    integer(Int1_), intent(out) :: iAscii_I(1:nLen)
    logical, optional, intent(in) :: DoOmitSign
    logical, optional, intent(in) :: DoFillZero
    integer :: i, n0
    !--------------------------------------------------------------------------
    if(present(DoFillZero)) then
          iAscii_I = iChar0
    else
       iAscii_I = iCharSpace
    end if

    n0 = abs(n)
    if (n < 0) then
       iAscii_I(1) = iCharMinus
    else
      if(.not. present(DoOmitSign)) then
         iAscii_I(1) = iCharPlus
      end if
    end if
    do i = nLen, 2, -1
      if(n0==0) EXIT
       iAscii_I(i) = mod(n0, 10) + iChar0
       n0 = n0 / 10
    end do
    if (n0 /= 0) then
       write(*,*) "Error: Number too large to fit in array"
    end if
  end subroutine int_to_ascii_code
  !============================================================================
  subroutine real_coef_to_ascii_code(Val, nFrac, iAscii_I)
    !$acc routine seq

    ! |r| < 1 is assumed!

    ! Example: r = -1.23456789 with nFrac = 6.
    ! Output Ascii_I = '-1.234568  ' (Note: the last digit is rounded)
    real, intent(in) :: Val
    integer, intent(in) :: nFrac
    integer(Int1_), intent(out) :: iAscii_I(1:nFrac+3)

    integer :: i, m
    logical :: IsNegative
    real:: V0
    !--------------------------------------------------------------------------
    iAscii_I = iCharSpace  ! Initialize all to ' '
    V0 = abs(Val)

    IsNegative = (Val < 0.0)

    V0 = V0 + 0.5*10.0**(-nFrac)  ! rounding

    ! Avoid
    if(V0 >= 10.0) V0 = V0 - 0.5*10.0**(-nFrac)

    if(IsNegative) iAscii_I(1) = iCharMinus

    m = floor(V0)
    iAscii_I(2) = m + iChar0
    iAscii_I(3) = iCharDot
    do i = 4, nFrac+3
       V0 = (V0 - m)*10.0
       m = floor(V0)
       iAscii_I(i) = m + iChar0
    end do
  end subroutine real_coef_to_ascii_code
  !============================================================================
  subroutine scientific_notation(Val, Coefficient, nExp)
    !$acc routine seq

    ! Convert a real number Val to scientific notation:
    ! Val = Coefficient * 10**Exponent,
    ! where 1.0 <= abs(Coefficient) < 10.0 unless Val is 0.0

    real, intent(in) :: Val
    real, intent(out) :: Coefficient
    integer, intent(out) :: nExp
    real, parameter :: Fractor = log(2.0)/log(10.0)

    logical :: IsNegative

    !--------------------------------------------------------------------------
    IsNegative = (Val < 0.0)

    Coefficient = abs(Val)

    nExp = floor(exponent(Coefficient)*Fractor)

    Coefficient = Coefficient * (0.1**nExp)

    if(Coefficient < 1.0 .and. Coefficient > 0.0) then
       Coefficient = Coefficient * 10.0
       nExp = nExp - 1
    end if

    if(IsNegative) Coefficient = -Coefficient
  end subroutine scientific_notation
  !============================================================================
  subroutine real_to_ascii_code(Val, nFrac, nLen, iAscii_I)
    !$acc routine seq
    ! Example: Val =    -1.234567E+03, where nFrac=6
    real, intent(in) :: Val
    integer, intent(in) :: nFrac, nLen
    integer(Int1_), intent(out) :: iAscii_I(nLen)

    integer :: i, nExp
    real :: Coefficient
    !--------------------------------------------------------------------------
    call scientific_notation(Val, Coefficient, nExp)

    iAscii_I = iCharSpace

    ! Position to start writing coefficient
    i = nLen - (nFrac + 7) + 1
    call real_coef_to_ascii_code(Coefficient, nFrac, iAscii_I(i:i+nFrac+3))

    iAscii_I(nLen-3) = iCharE
    iAscii_I(nLen-2:nLen) = iChar0
    call int_to_ascii_code(nExp, 3, iAscii_I(nLen-2:nLen), DoFillZero=.true.)
  end subroutine real_to_ascii_code
  !============================================================================
end module ModUtilities
!==============================================================================
subroutine CON_set_do_test_ext(String,DoTest,DoTestMe)

  use ModUtilities, ONLY: CON_set_do_test

  implicit none

  character (len=*), intent(in)  :: String
  logical          , intent(out) :: DoTest, DoTestMe

  ! See ModUtilities.
  !----------------------------------------------------------------------------
  call CON_set_do_test(String, DoTest, DoTestMe)

end subroutine CON_set_do_test_ext
!==============================================================================
subroutine CON_stop_ext(StringError)

  use ModUtilities, ONLY: CON_stop
  implicit none

  character (len=*), intent(in) :: StringError

  ! This subroutine is used to abort the run with an error report.
  ! It provides an external subroutine interface to ModUtilities::CON\_stop.
  ! Open I/O units are closed and empty output files are deleted before abort.
  ! This will only be done on the aborting processor(s).

  !----------------------------------------------------------------------------
  call CON_stop(StringError)
end subroutine CON_stop_ext
!==============================================================================
subroutine CON_io_unit_new_ext(iUnit)

  use ModIoUnit, ONLY: io_unit_new

  implicit none

  integer, intent(out) :: iUnit
  ! This external subroutine is an access method for non-F90 source.
  ! The file should be opened right after the unit number was obtained
  ! so that the unit number gets locked. When the file is closed, the
  ! unit number is automatically released.
  !----------------------------------------------------------------------------
  iUnit = io_unit_new()
end subroutine CON_io_unit_new_ext
!==============================================================================
subroutine CON_io_unit_tmp(iUnit)

  use ModIoUnit, ONLY: UnitTMP_

  implicit none

  integer, intent(out) :: iUnit
  ! This external subroutine is an access method for non-F90 source.
  ! The file using a temporary unit number should be closed before
  ! any other file could be opened.
  !----------------------------------------------------------------------------
  iUnit = UnitTMP_
end subroutine CON_io_unit_tmp
!==============================================================================
