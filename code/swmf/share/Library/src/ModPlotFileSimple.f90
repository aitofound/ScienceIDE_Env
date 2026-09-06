!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module ModPlotFileSimple

  ! Save or read VAC/IDL type plotfiles from 1 up to 3 dimensions.
  ! ASCII, single (real4), or double (real8) precision binary file formats
  ! can be used.
  ! The plot file contains 4 or 5 header lines:
  !
  !    Header
  !    nStep Time nDim nParam nVar
  !    n1 .. nNDim
  !    Param1 .. ParamNParam       # this line is omitted if nParam=0
  !    NameVar
  !
  ! Header   (string) describes the plot. It is up to 500 characters.
  ! nStep    (integer) is the number of time steps/iterations etc.
  ! Time     (real) is the simulation time.
  ! nDim     (integer) number of dimensions. Negative for non-Cartesian grids.
  ! nParam   (integer) number of parameters (for example adiabatic index)
  ! n1 ..    (nDim integers) grid sizes in the nDim dimensions
  ! Param1.. (nParam reals) parameter values (omitted if nParam is zero)
  ! NameVar  (string) space separated list of names for the
  !                   nDim coordinates, nVar variables and nParam parameters
  !
  ! The header is followed by the coordinate/variable data.
  ! In ASCII files each line contains the coordinates+variables for one cell.
  ! The cells are ordered such that the first coordinate index changes fastest.
  ! In binary files the coordinates are saved in a single array, followed by
  ! the variables, saved as one record per variable.
  !
  ! For save_plot_file the coordinates can be either given as full arrays,
  ! or for Cartesian grids they can be given as 1D arrays for each dimension,
  ! or for uniform Cartesian grids they can be given with min and max values.
  ! The number of dimensions, the size of the grid and the number of the
  ! variables is determined from the size of the variable array.
  !
  ! For read_plot_file the number of dimensions, variables, parameters, grid
  ! size are optionaal parameters.

  implicit none

  private ! except

  ! Read and save plot files
  public:: read_plot_file
  public:: save_plot_file

  ! Unit test
  public:: test_plot_file

  ! more contained subroutines that are useful

  ! Open and close files with improved default behavior and
  ! meaningful error messages
  public:: open_file, close_file

  ! Split string into array and join string array into string
  public:: split_string, join_string

  ! Convert to lower or upper case
  public:: lower_case, upper_case

  interface split_string
     module procedure split_string, split_string_simple
  end interface

  interface join_string
     module procedure join_string, join_string_simple
  end interface

  ! This unit number should not be in continuous use
  integer, parameter, public :: UnitTmp_ = 9

  ! Kind values for 4 and 8-byte reals
  integer, parameter, public :: Real4_ = selected_real_kind(6,30)
  integer, parameter, public :: Real8_ = selected_real_kind(12,100)

  ! The length of the header string in the plot files
  integer, parameter, public:: lStringPlotFile = 500

  ! The maximum dimensionality of the plot files
  integer, parameter:: MaxDim = 5

contains
  !============================================================================
  subroutine save_plot_file(NameFile, TypePositionIn, &
       TypeFileIn, StringHeaderIn, nStepIn, TimeIn, &
       ParamIn_I, NameVarIn, NameVarIn_I, NameUnitsIn,&
       IsCartesianIn, &
       nDimIn,&
       CoordMinIn_D, CoordMaxIn_D, &
       Coord1In_I, Coord2In_I, Coord3In_I, &
       CoordIn_I, CoordIn_DII, CoordIn_DIII,&
       VarIn_I,  VarIn_II,  VarIn_III,  &
       VarIn_VI, VarIn_VII, VarIn_VIII, &
       VarIn_IV, VarIn_IIV, VarIn_IIIV, &
       Var4In_I,  Var4In_II,  Var4In_III,  &
       Var4In_VI, Var4In_VII, Var4In_VIII, &
       Var4In_IV, Var4In_IIV, Var4In_IIIV, &
       StringFormatIn, StringFormatParamIn)

    character(len=*),           intent(in):: NameFile       ! name of plot file
    character(len=*), optional, intent(in):: TypePositionIn ! rewind/append
    character(len=*), optional, intent(in):: TypeFileIn     ! ascii/real8/real4
    character(len=*), optional, intent(in):: StringHeaderIn ! header line
    character(len=*), optional, intent(in):: StringFormatIn ! ascii data format
    character(len=*), optional, intent(in):: StringFormatParamIn ! param format
    integer,          optional, intent(in):: nStepIn        ! number of steps
    real,             optional, intent(in):: TimeIn         ! simulation time
    real,             optional, intent(in):: ParamIn_I(:)   ! parameters
    character(len=*), optional, intent(in):: NameVarIn      ! list of names
    character(len=*), optional, intent(in):: NameVarIn_I(:) ! list of names
    character(len=*), optional, intent(in):: NameUnitsIn    ! list of units
    logical,          optional, intent(in):: IsCartesianIn  ! Cartesian grid?
    integer,          optional, intent(in):: nDimIn         ! grid dimensions
    real,             optional, intent(in):: CoordIn_I(:)   ! coords in 1D
    real,             optional, intent(in):: CoordIn_DII(:,:,:)       ! 2D
    real,             optional, intent(in):: CoordIn_DIII(:,:,:,:)    ! 3D
    real,             optional, intent(in):: Coord1In_I(:)  ! coords for axis 1
    real,             optional, intent(in):: Coord2In_I(:)  ! coords for axis 2
    real,             optional, intent(in):: Coord3In_I(:)  ! coords for axis 3
    real,             optional, intent(in):: CoordMinIn_D(:)! min coordinates
    real,             optional, intent(in):: CoordMaxIn_D(:)! max coordinates
    real,             optional, intent(in):: VarIn_I(:)     ! variable  in 1D
    real,             optional, intent(in):: VarIn_II(:,:)               ! 2D
    real,             optional, intent(in):: VarIn_III(:,:,:)            ! 3D
    real,             optional, intent(in):: VarIn_VI(:,:)  ! variables in 1D
    real,             optional, intent(in):: VarIn_VII(:,:,:)            ! 2D
    real,             optional, intent(in):: VarIn_VIII(:,:,:,:)         ! 3D
    real,             optional, intent(in):: VarIn_IV(:,:)  ! variables in 1D
    real,             optional, intent(in):: VarIn_IIV(:,:,:)            ! 2D
    real,             optional, intent(in):: VarIn_IIIV(:,:,:,:)         ! 3D
    real(Real4_),     optional, intent(in):: Var4In_I(:)    ! Real4 var in 1D
    real(Real4_),     optional, intent(in):: Var4In_II(:,:)               ! 2D
    real(Real4_),     optional, intent(in):: Var4In_III(:,:,:)            ! 3D
    real(Real4_),     optional, intent(in):: Var4In_VI(:,:)  ! variables in 1D
    real(Real4_),     optional, intent(in):: Var4In_VII(:,:,:)            ! 2D
    real(Real4_),     optional, intent(in):: Var4In_VIII(:,:,:,:)         ! 3D
    real(Real4_),     optional, intent(in):: Var4In_IV(:,:)  ! variables in 1D
    real(Real4_),     optional, intent(in):: Var4In_IIV(:,:,:)            ! 2D
    real(Real4_),     optional, intent(in):: Var4In_IIIV(:,:,:,:)         ! 3D

    ! True if Var4In* is present (single precision)
    logical:: UseReal4

    character(len=10)  :: TypePosition
    character(len=10)  :: TypeStatus
    character(len=20), allocatable  :: NameVar_I(:), NameUnits_I(:)
    character(len=20)  :: TypeFile
    character(len=lStringPlotFile) :: StringHeader
    character(len=40)  :: StringFormat, StringFormatParam
    character(len=lStringPlotFile) :: NameVar
    integer :: nStep, nDim, nParam, nVar, n1, n2, n3
    real    :: Time, Coord

    logical :: IsCartesian
    real,         allocatable:: &
         Param_I(:), Coord_ID(:,:), Var_I(:), Var_IV(:,:)
    real(Real4_), allocatable:: &
         Param4_I(:), Coord4_ID(:,:), Var4_I(:), Var4_IV(:,:)

    integer :: n_D(0:MaxDim)
    integer :: i, j, k, i_D(3), iDim, iVar, n, nDimOut

    character(len=*), parameter:: NameSub = 'save_plot_file'
    !--------------------------------------------------------------------------
    TypePosition = 'rewind'
    if(present(TypePositionIn))TypePosition = TypePositionIn
    TypeStatus = 'replace'
    if(TypePosition == 'append')TypeStatus = 'unknown'

    TypeFile = 'ascii'
    if(present(TypeFileIn)) TypeFile = TypeFileIn
    StringHeader = 'No header info'
    if(present(StringHeaderIn)) StringHeader = StringHeaderIn
    StringFormat = '(100es18.10)'
    if(present(StringFormatIn)) StringFormat = StringFormatIn
    StringFormatParam = '(100es18.10)'
    if(present(StringFormatParamIn)) StringFormatParam = StringFormatParamIn
    nStep = 0
    if(present(nStepIn)) nStep = nStepIn
    Time = 0.0
    if(present(TimeIn)) Time = TimeIn

    if(present(ParamIn_I))then
       nParam = size(ParamIn_I)
       allocate(Param_I(nParam))
       Param_I = ParamIn_I
    else
       nParam = 0
    end if

    ! Figure out precision of input variable
    UseReal4 = &
         present(Var4In_I) .or.present(Var4In_II).or.present(Var4In_III)  &
         .or. &
         present(Var4In_VI).or.present(Var4In_VII).or.present(Var4In_VIII) &
         .or.  &
         present(Var4In_IV).or.present(Var4In_IIV).or.present(Var4In_IIIV)

    ! Figure out grid dimensions and number of variables. Default is 1.
    n_D = 1
    if(present(VarIn_I))then
       nDim = 1
       n_D(1:1) = shape(VarIn_I)
    elseif(present(VarIn_II)) then
       nDim = 2
       n_D(1:2) = shape(VarIn_II)
    elseif(present(VarIn_III)) then
       nDim = 3
       n_D(1:3) = shape(VarIn_III)
    elseif(present(VarIn_VI))then
       nDim = 1
       n_D(0:1) = shape(VarIn_VI)
    elseif(present(VarIn_VII))then
       nDim = 2
       n_D(0:2) = shape(VarIn_VII)
    elseif(present(VarIn_VIII))then
       nDim = 3
       n_D(0:3) = shape(VarIn_VIII)
       ! For IV, IIV, IIIV types
    elseif(present(VarIn_IV))then
       nDim = 1
       n_D(0:1) = shape(VarIn_IV)
       n_D(0:1) = cshift(n_D(0:1), -1)   ! shift nVar/n_D(1) to n_D(0)
    elseif(present(VarIn_IIV))then
       nDim = 2
       n_D(0:2) = shape(VarIn_IIV)
       n_D(0:2) = cshift(n_D(0:2), -1)   ! shift nVar/n_D(2) to n_D(0)
    elseif(present(VarIn_IIIV))then
       nDim = 3
       n_D(0:3) = shape(VarIn_IIIV)
       n_D = cshift(n_D, -1)        ! shift nVar/n_D(3) to n_D(0)
    elseif(present(Var4In_I))then
       nDim = 1
       n_D(1:1) = shape(Var4In_I)
    elseif(present(Var4In_II)) then
       nDim = 2
       n_D(1:2) = shape(Var4In_II)
    elseif(present(Var4In_III)) then
       nDim = 3
       n_D(1:3) = shape(Var4In_III)
    elseif(present(Var4In_VI))then
       nDim = 1
       n_D(0:1) = shape(Var4In_VI)
    elseif(present(Var4In_VII))then
       nDim = 2
       n_D(0:2) = shape(Var4In_VII)
    elseif(present(Var4In_VIII))then
       nDim = 3
       n_D(0:3) = shape(Var4In_VIII)
       ! For IV, IIV, IIIV types
    elseif(present(Var4In_IV))then
       nDim = 1
       n_D(0:1) = shape(Var4In_IV)
       n_D(0:1) = cshift(n_D(0:1), -1)   ! shift nVar/n_D(1) to n_D(0)
    elseif(present(Var4In_IIV))then
       nDim = 2
       n_D(0:2) = shape(Var4In_IIV)
       n_D(0:2) = cshift(n_D(0:2), -1)   ! shift nVar/n_D(2) to n_D(0)
    elseif(present(Var4In_IIIV))then
       nDim = 3
       n_D(0:3) = shape(Var4In_IIIV)
       n_D = cshift(n_D, -1)        ! shift nVar/n_D(3) to n_D(0)
    else
       call CON_stop(NameSub // &
            ': none of the VarIn_* or Var4In_* variables are present')
    endif
    ! Extract information
    nVar = n_D(0)
    n1   = n_D(1)
    n2   = n_D(2)
    n3   = n_D(3)

    ! The plot dimension may be different from the dimensionality of VarIn
    if(present(nDimIn))then
       nDim = nDimIn
       if(n1 == 1 .and. n2 == 1)then
          n_D(1:3) = [ n3, 1, 1]
       elseif(n1 == 1)then
          n_D(1:3) = [ n2, n3, 1]
       elseif(n2 == 1)then
          n_D(1:3) = [ n1, n3, 1]
       end if
    end if
    IsCartesian = .true.
    if(present(IsCartesianIn)) IsCartesian = IsCartesianIn

    ! nDim is saved with a negative sign for non-Cartesian grid
    nDimOut = nDim
    if(.not. IsCartesian) nDimOut = -nDim

    ! Set variable names
    if(present(NameVarIn))then
       NameVar = NameVarIn
    else if(present(NameVarIn_I)) then
       call join_string(NameVarIn_I, NameVar)
    else
       ! Create some arbitrary variable names
       NameVar = 'x1'
       do i = 2, nDim
          write(NameVar, "(a, i1)") trim(NameVar) // ' x', i
       end do
       do i = 1, nVar
          write(NameVar, "(a, i2.2)") trim(NameVar) // ' v', i
       end do
       do i = 1, nParam
          write(NameVar, "(a, i2.2)") trim(NameVar) // ' p', i
       end do
    end if

    ! Create a variable name array
    allocate(NameVar_I(nDim + nVar + nParam))
    call split_string(NameVar, NameVar_I, i, UseArraySyntaxIn=.true.)
    if(i /= nDim + nVar + nParam)then
       write(*,*) NameSub,': NameFile=', trim(NameFile)
       write(*,*) NameSub,': NameVar=', trim(NameVar)
       write(*,*) NameSub,': number of substrings=', i
       write(*,*) NameSub,': nDim, nVar, nParam, sum=',&
            nDim, nVar, nParam, nDim + nVar + nParam
       call CON_stop(NameSub// &
            ': number of names in NameVar does not match nDim+nVar+nParam !')
    end if

    ! Allocate arrays with a shape that is convenient for saving data
    allocate(Coord_ID(n1*n2*n3,nDim))
    if(UseReal4)then
       allocate(Var4_IV(n1*n2*n3,nVar))
    else
       allocate(Var_IV(n1*n2*n3,nVar))
    end if
    ! Fill in the Coord_ID coordinate array using the available information
    do iDim = 1, nDim
       n = 0
       do k = 1, n3; do j = 1, n2; do i = 1, n1
          n = n + 1
          Coord = huge(1.0)
          if(present(CoordMinIn_D)) then
             i_D = [i, j, k]
             Coord = CoordMinIn_D(iDim) + (i_D(iDim)-1)* &
                  ((CoordMaxIn_D(iDim) - CoordMinIn_D(iDim)) &
                  / max(1, n_D(iDim)-1) )
          end if
          if(present(CoordIn_I))    Coord = CoordIn_I(i)
          if(present(CoordIn_DII))  Coord = CoordIn_DII(iDim,i,j)
          if(present(CoordIn_DIII)) Coord = CoordIn_DIII(iDim,i,j,k)
          if(present(Coord1In_I) .and. iDim==1) Coord = Coord1In_I(i)
          if(present(Coord2In_I) .and. iDim==2) Coord = Coord2In_I(j)
          if(present(Coord3In_I) .and. iDim==3) Coord = Coord3In_I(k)
          Coord_ID(n, iDim) = Coord
       end do; end do; end do;
    end do

    ! Check if all coordinates were set
    if(any(Coord_ID == huge(1.0))) call CON_stop(NameSub // &
         ' coordinates were not defined')

    ! Fill in the Var_IV/Var4_IV variable array using
    ! the available information
    if(UseReal4)then
       Var4_IV = huge(1.0_Real4_)
    else
       Var_IV = huge(1.0)
    end if
    do iVar = 1, nVar
       n = 0
       do k = 1, n3; do j = 1, n2; do i = 1,n1
          n = n + 1
          if(present(VarIn_I))    Var_IV(n,iVar) = VarIn_I(i)
          if(present(VarIn_II))   Var_IV(n,iVar) = VarIn_II(i,j)
          if(present(VarIn_III))  Var_IV(n,iVar) = VarIn_III(i,j,k)
          if(present(VarIn_VI))   Var_IV(n,iVar) = VarIn_VI(iVar,i)
          if(present(VarIn_VII))  Var_IV(n,iVar) = VarIn_VII(iVar,i,j)
          if(present(VarIn_VIII)) Var_IV(n,iVar) = VarIn_VIII(iVar,i,j,k)
          if(present(VarIn_IV))   Var_IV(n,iVar) = VarIn_IV(i,iVar)
          if(present(VarIn_IIV))  Var_IV(n,iVar) = VarIn_IIV(i,j,iVar)
          if(present(VarIn_IIIV)) Var_IV(n,iVar) = VarIn_IIIV(i,j,k,iVar)
          if(present(Var4In_I))    Var4_IV(n,iVar) = Var4In_I(i)
          if(present(Var4In_II))   Var4_IV(n,iVar) = Var4In_II(i,j)
          if(present(Var4In_III))  Var4_IV(n,iVar) = Var4In_III(i,j,k)
          if(present(Var4In_VI))   Var4_IV(n,iVar) = Var4In_VI(iVar,i)
          if(present(Var4In_VII))  Var4_IV(n,iVar) = Var4In_VII(iVar,i,j)
          if(present(Var4In_VIII)) Var4_IV(n,iVar) = Var4In_VIII(iVar,i,j,k)
          if(present(Var4In_IV))   Var4_IV(n,iVar) = Var4In_IV(i,iVar)
          if(present(Var4In_IIV))  Var4_IV(n,iVar) = Var4In_IIV(i,j,iVar)
          if(present(Var4In_IIIV)) Var4_IV(n,iVar) = Var4In_IIIV(i,j,k,iVar)
       end do; end do; end do;
    end do

    ! Check if all variables were set
    if(UseReal4)then
       if(any(Var4_IV == huge(1.0_Real4_))) call CON_stop(NameSub // &
            ' variables were not defined')
    else
       if(any(Var_IV == huge(1.0))) call CON_stop(NameSub // &
            ' variables were not defined')
    end if

    ! Treat unit names:
    if(present(NameUnitsIn))then
       select case(TypeFile)
       case('tec')
          ! Unit names should be merged with those for variables:
          allocate(NameUnits_I(nDim + nVar))
          call split_string(NameUnitsIn, NameUnits_I, i)
          if(i /= nDim + nVar)then
             write(*,*) NameSub,': NameFile=', trim(NameFile)
             write(*,*) NameSub,': NameUnitsIn=', trim(NameUnitsIn)
             write(*,*) NameSub,': number of substrings=', i
             write(*,*) NameSub,': nDim, nVar, sum=',&
                  nDim, nVar, nDim + nVar
             call CON_stop(NameSub// &
                  ': number of names in NameUnitsIn does not match'//&
                  'nDim+nVar!')
          end if
          do i  = 1, nDim + nVar
             NameVar_I(i) = trim(NameVar_I(i))//'_['//&
                  trim(NameUnits_I(i))//']'
          end do
       case default
          ! Just merge the units list to the header:
          StringHeader = trim(StringHeader)//' Units: '//trim(NameUnitsIn)
       end select
    end if
    select case(TypeFile)
    case('tec')
       call open_file(FILE=NameFile, POSITION=TypePosition, STATUS=TypeStatus)
       if(StringHeader(1:11)=="VARIABLES =")then
          write(UnitTmp_, "(a)", ADVANCE="NO") 'VARIABLES='
          if(n3 > 1) write(UnitTmp_, "(a)", ADVANCE="NO") '"K", '
          if(n2 > 1) write(UnitTmp_, "(a)", ADVANCE="NO") '"J", '
          if(n1 > 1) write(UnitTmp_, "(a)", ADVANCE="NO") '"I", '
          write(UnitTmp_, "(a)") StringHeader(12:len_trim(StringHeader))
       else
          if(present(StringHeaderIn))&
               write(UnitTmp_, "(a)")'TITLE="'//trim(StringHeader)//'"'
          write(UnitTmp_, "(a)", ADVANCE="NO") 'VARIABLES='
          if(n3 > 1) write(UnitTmp_, "(a)", ADVANCE="NO") '"K", '
          if(n2 > 1) write(UnitTmp_, "(a)", ADVANCE="NO") '"J", '
          if(n1 > 1) write(UnitTmp_, "(a)", ADVANCE="NO") '"I", '
          call join_string(NameVar_I(1:nDim+nVar), NameVar, '", "')
          write(UnitTmp_, "(a)") '"'//trim(NameVar)//'"'
       end if
       write(UnitTmp_,'(a,i6,a,i6,a,i6,a)') &
            'ZONE T="STRUCTURED GRID", I=', &
            n1,', J=',n2,', K=',n3,', F=POINT'
       write(UnitTmp_,'(a,i8,a)')      'AUXDATA ITER="', nStep, '"'
       write(UnitTmp_,'(a,es18.10,a)') 'AUXDATA TIMESIM="', Time, '"'
       write(UnitTmp_,'(a,i3,a)')      'AUXDATA NDIM="', nDimOut, '"'
       write(UnitTmp_,'(a,i3,a)')      'AUXDATA NPARAM="', nParam, '"'
       write(UnitTmp_,'(a,i3,a)')      'AUXDATA NVAR="', nVar, '"'
       do i = 1, nParam
          write(UnitTmp_,'(a,es18.10,a)') &
               'AUXDATA '//trim(NameVar_I(nDim+nVar+i))//'="', Param_I(i), '"'
       end do

       ! write out coordinates and variables line by line
       n = 0
       do k = 1, n3; do j = 1, n2; do i = 1, n1
          if(n3 > 1)write(UnitTmp_, "(i6)", ADVANCE="NO") k
          if(n2 > 1)write(UnitTmp_, "(i6)", ADVANCE="NO") j
          if(n1 > 1)write(UnitTmp_, "(i8)", ADVANCE="NO") i
          n = n + 1
          if(UseReal4)then
             write(UnitTmp_, StringFormat) Coord_ID(n,:), Var4_IV(n,:)
          else
             write(UnitTmp_, StringFormat) Coord_ID(n,:), Var_IV(n,:)
          end if
       end do; end do; end do

       call close_file
    case('formatted', 'ascii')
       call open_file(FILE=NameFile, POSITION=TypePosition, STATUS=TypeStatus)

       write(UnitTmp_, "(a)")             trim(StringHeader)
       write(UnitTmp_, "(i10,es18.10,3i3)") nStep, Time, nDimOut, nParam, nVar
       write(UnitTmp_, "(3i8)")           n_D(1:nDim)
       if(nParam > 0) &
            write(UnitTmp_, StringFormatParam) Param_I
       write(UnitTmp_, "(a)")             trim(NameVar)

       where(abs(Var_IV) < 1d-99) Var_IV = 0.0

       ! write out coordinates and variables line by line
       n = 0
       do k = 1, n3; do j = 1, n2; do i = 1, n1
          n = n + 1
          if(UseReal4)then
             write(UnitTmp_, StringFormat) Coord_ID(n,:), Var4_IV(n,:)
          else
             write(UnitTmp_, StringFormat) Coord_ID(n,:), Var_IV(n,:)
          end if
       end do; end do; end do
       call close_file
    case('real8')
       call open_file(FILE=NameFile, FORM='unformatted', &
            POSITION=TypePosition, STATUS=TypeStatus)
       write(UnitTmp_) StringHeader
       write(UnitTmp_) nStep, Time, nDimOut, nParam, nVar
       write(UnitTmp_) n_D(1:nDim)
       if(nParam > 0)write(UnitTmp_) Param_I
       write(UnitTmp_) NameVar
       write(UnitTmp_) Coord_ID
       ! write out variables 1 by 1 to avoid segmentation fault
       ! for very large Var_IV array
       if(UseReal4)then
          allocate(Var_I(n1*n2*n3))
          do iVar = 1, nVar
             Var_I = Var4_IV(:,iVar) ! convert to default precision
             write(UnitTmp_) Var_I
          end do
          deallocate(Var_I)
       else
          do iVar = 1, nVar
             write(UnitTmp_) Var_IV(:,iVar)
          end do
       end if
       call close_file
    case('real4')
       call open_file(FILE=NameFile, FORM='unformatted', &
            POSITION=TypePosition, STATUS=TypeStatus)

       write(UnitTmp_) StringHeader
       write(UnitTmp_) nStep, real(Time, Real4_), nDimOut, nParam, nVar
       write(UnitTmp_) n_D(1:nDim)
       if(nParam > 0)then
          allocate(Param4_I(nParam))
          Param4_I = Param_I
          write(UnitTmp_) Param4_I
          deallocate(Param4_I)
       end if
       write(UnitTmp_) NameVar
       ! Copy into single precision arrays to avoid compiler issues.
       allocate(Coord4_ID(n1*n2*n3, nDim))
       Coord4_ID = Coord_ID
       write(UnitTmp_) Coord4_ID
       deallocate(Coord4_ID)
       if(UseReal4)then
          do iVar = 1, nVar
             write(UnitTmp_) Var4_IV(:,iVar)
          end do
       else
          allocate(Var4_I(n1*n2*n3))
          do iVar = 1, nVar
             Var4_I = Var_IV(:,iVar) ! convert to single precision
             write(UnitTmp_) Var4_I
          end do
          deallocate(Var4_I)
       end if
       call close_file
    case default
       call CON_stop(NameSub // ' unknown TypeFile =' // trim(TypeFile))
    end select

    if(allocated(Param_I))   deallocate(Param_I)
    if(allocated(NameVar_I)) deallocate(NameVar_I)
    if(allocated(NameUnits_I)) deallocate(NameUnits_I)
    if(allocated(Coord_ID))  deallocate(Coord_ID)
    if(allocated(Var_IV))    deallocate(Var_IV)
    if(allocated(Var4_IV))   deallocate(Var4_IV)

  end subroutine save_plot_file
  !============================================================================
  subroutine read_plot_file(NameFile, iUnitIn,         &
       TypeFileIn, StringHeaderOut,                    &
       nStepOut, TimeOut, nDimOut, nParamOut, nVarOut, &
       IsCartesianOut,                                 &
       n1Out, n2Out, n3Out, n4Out, n5Out, nOut_D,      &
       ParamOut_I, NameVarOut,                         &
       CoordMinOut_D, CoordMaxOut_D,                   &
       CoordOut_DI,                                    &
       Coord1Out_I, Coord2Out_I, Coord3Out_I, Coord4Out_I, Coord5Out_I, &
       CoordOut_I, CoordOut_DII, CoordOut_DIII, CoordOut_DI4, CoordOut_DI5, &
       VarOut_I,  VarOut_II,  VarOut_III,  VarOut_I4,  VarOut_I5,           &
       VarOut_VI, VarOut_VII, VarOut_VIII, VarOut_VI4, VarOut_VI5,          &
       VarOut_IV, VarOut_IIV, VarOut_IIIV, VarOut_I4V, VarOut_I5V,          &
       Var4Out_I,  Var4Out_II,  Var4Out_III,  Var4Out_I4,  Var4Out_I5,      &
       Var4Out_VI, Var4Out_VII, Var4Out_VIII, Var4Out_VI4, Var4Out_VI5,     &
       Var4Out_IV, Var4Out_IIV, Var4Out_IIIV, Var4Out_I4V, Var4Out_I5V,     &
       iErrorOut)

    ! VarOut_VI, Var4Out_VI and CoordOut_DI can be used in 1D, 2D, and 3D

    character(len=*),           intent(in) :: NameFile
    integer,          optional, intent(in) :: iUnitIn
    character(len=*), optional, intent(in) :: TypeFileIn
    character(len=*), optional, intent(out):: StringHeaderOut
    character(len=*), optional, intent(out):: NameVarOut
    real,             optional, intent(out):: TimeOut
    integer,          optional, intent(out):: nStepOut
    integer,          optional, intent(out):: nDimOut   ! number of dimensions
    integer,          optional, intent(out):: nParamOut ! number of parameters
    integer,          optional, intent(out):: nVarOut   ! number of variables
    integer,          optional, intent(out):: n1Out, n2Out, n3Out ! grid size
    integer,          optional, intent(out):: n4Out, n5Out
    integer,          optional, intent(out):: nOut_D(:) ! grid size array
    logical,          optional, intent(out):: IsCartesianOut ! Cartesian grid?
    real,             optional, intent(out):: ParamOut_I(:)  ! parameters
    real,             optional, intent(out):: CoordMinOut_D(:)
    real,             optional, intent(out):: CoordMaxOut_D(:)
    real,             optional, intent(out):: CoordOut_DI(:,:) ! for 1D,2D,3D
    real,             optional, intent(out):: Coord1Out_I(:)
    real,             optional, intent(out):: Coord2Out_I(:)
    real,             optional, intent(out):: Coord3Out_I(:)
    real,             optional, intent(out):: Coord4Out_I(:)
    real,             optional, intent(out):: Coord5Out_I(:)
    real,             optional, intent(out):: CoordOut_I(:)               ! 1D
    real,             optional, intent(out):: CoordOut_DII(:,:,:)         ! 2D
    real,             optional, intent(out):: CoordOut_DIII(:,:,:,:)      ! 3D
    real,             optional, intent(out):: CoordOut_DI4(:,:,:,:,:)     ! 4D
    real,             optional, intent(out):: CoordOut_DI5(:,:,:,:,:,:)   ! 5D
    real,             optional, intent(out):: VarOut_I(:)    ! variable  in 1D
    real,             optional, intent(out):: VarOut_II(:,:)              ! 2D
    real,             optional, intent(out):: VarOut_III(:,:,:)           ! 3D
    real,             optional, intent(out):: VarOut_I4(:,:,:,:)          ! 4D
    real,             optional, intent(out):: VarOut_I5(:,:,:,:,:)        ! 5D
    real,             optional, intent(out):: VarOut_VI(:,:) ! variables in 1D
    real,             optional, intent(out):: VarOut_VII(:,:,:)           ! 2D
    real,             optional, intent(out):: VarOut_VIII(:,:,:,:)        ! 3D
    real,             optional, intent(out):: VarOut_VI4(:,:,:,:,:)       ! 4D
    real,             optional, intent(out):: VarOut_VI5(:,:,:,:,:,:)     ! 5D
    real,             optional, intent(out):: VarOut_IV(:,:)              ! 1D
    real,             optional, intent(out):: VarOut_IIV(:,:,:)           ! 2D
    real,             optional, intent(out):: VarOut_IIIV(:,:,:,:)        ! 3D
    real,             optional, intent(out):: VarOut_I4V(:,:,:,:,:)       ! 4D
    real,             optional, intent(out):: VarOut_I5V(:,:,:,:,:,:)     ! 5D
    real(Real4_),     optional, intent(out):: Var4Out_I(:) ! Real4 scalar in 1D
    real(Real4_),     optional, intent(out):: Var4Out_II(:,:)             ! 2D
    real(Real4_),     optional, intent(out):: Var4Out_III(:,:,:)          ! 3D
    real(Real4_),     optional, intent(out):: Var4Out_I4(:,:,:,:)         ! 4D
    real(Real4_),     optional, intent(out):: Var4Out_I5(:,:,:,:,:)       ! 5D
    real(Real4_),     optional, intent(out):: Var4Out_VI(:,:) ! Real4 vector 1D
    real(Real4_),     optional, intent(out):: Var4Out_VII(:,:,:)          ! 2D
    real(Real4_),     optional, intent(out):: Var4Out_VIII(:,:,:,:)       ! 3D
    real(Real4_),     optional, intent(out):: Var4Out_VI4(:,:,:,:,:)      ! 4D
    real(Real4_),     optional, intent(out):: Var4Out_VI5(:,:,:,:,:,:)    ! 5D
    real(Real4_),     optional, intent(out):: Var4Out_IV(:,:)             ! 1D
    real(Real4_),     optional, intent(out):: Var4Out_IIV(:,:,:)          ! 2D
    real(Real4_),     optional, intent(out):: Var4Out_IIIV(:,:,:,:)       ! 3D
    real(Real4_),     optional, intent(out):: Var4Out_I4V(:,:,:,:,:)      ! 4D
    real(Real4_),     optional, intent(out):: Var4Out_I5V(:,:,:,:,:,:)    ! 5D
    integer,          optional, intent(out):: iErrorOut            ! I/O error

    ! True if Var4Out* is present (single precision)
    logical:: UseReal4

    integer            :: iUnit
    character(len=20)  :: TypeFile
    logical            :: DoReadHeader = .true.
    character(len=lStringPlotFile) :: StringHeader
    character(len=lStringPlotFile) :: NameVar
    integer            :: nStep, nDim, nParam, nVar
    integer            :: n1, n2, n3, n4, n5, n_D(MaxDim)
    real               :: Time, Coord
    real(Real4_)       :: Time4
    logical            :: IsCartesian
    real(Real4_), allocatable:: &
         Param4_I(:), Coord4_ID(:,:), Var4_I(:), Var4_IV(:,:)
    real,         allocatable:: &
         Param_I(:),  Coord_ID(:,:),  Var_I(:), Var_IV(:,:)
    real    :: TecIndex_I(MaxDim)
    integer :: nTecIndex
    integer :: i, j, k, l, m, iDim, iVar, n

    ! Remember these values after reading header
    save :: nDim, nVar, n1, n2, n3, n4, n5, n_D, TypeFile, iUnit

    ! Variables to convert integer date into double precision time

    character(len=*), parameter:: NameSub = 'read_plot_file'
    !--------------------------------------------------------------------------
    iUnit = UnitTmp_
    if(present(iUnitIn)) iUnit = iUnitIn

    TypeFile = 'ascii'
    if(present(TypeFileIn)) TypeFile = TypeFileIn

    if(present(iErrorOut)) iErrorOut = 0

    if(DoReadHeader) call read_header
    DoReadHeader = .false.

    ! Check for single precision output
    UseReal4= present(Var4Out_I)   .or. present(Var4Out_II) &
         .or. present(Var4Out_III) .or. present(Var4Out_I4) &
         .or. present(Var4Out_I5) &
         .or. present(Var4Out_VI)  .or. present(Var4Out_VII) &
         .or. present(Var4Out_VIII).or. present(Var4Out_VI4) &
         .or. present(Var4Out_VI5) &
         .or. present(Var4Out_IV)  .or. present(Var4Out_IIV) &
         .or. present(Var4Out_IIIV).or. present(Var4Out_I4V) &
         .or. present(Var4Out_I5V)

    ! No data is read. Leave file open !
    if(.not. ( UseReal4 &
         .or. present(VarOut_I)   .or. present(VarOut_II) &
         .or. present(VarOut_III) .or. present(VarOut_I4) &
         .or. present(VarOut_I5) &
         .or. present(VarOut_VI)  .or. present(VarOut_VII) &
         .or. present(VarOut_VIII).or. present(VarOut_VI4) &
         .or. present(VarOut_VI5) &
         .or. present(VarOut_IV)  .or. present(VarOut_IIV) &
         .or. present(VarOut_IIIV).or. present(VarOut_I4V) &
         .or. present(VarOut_I5V))) RETURN

    if((present(VarOut_I) .or. present(VarOut_II) .or. present(VarOut_III) &
         .or. present(VarOut_I4) .or. present(VarOut_I5)) .and. nVar /= 1)then
       write(*,*) NameSub,': the number of variables is ', nVar, &
            ' (larger than 1) in file ', NameFile
       call CON_stop(NameSub//' called with scalar variable argument')
    end if

    ! If data is read, next header needs to be read
    DoReadHeader = .true.

    ! Read coordinates and variables into suitable 2D arrays
    allocate(Coord_ID(n1*n2*n3*n4*n5,nDim))
    if(UseReal4)then
       allocate(Var4_IV(n1*n2*n3*n4*n5,nVar))
    else
       allocate(Var_IV(n1*n2*n3*n4*n5,nVar))
    end if
    select case(TypeFile)
    case('tec')
       nTecIndex = count(n_D>1, 1)
       n = 0
       if(UseReal4)then
          do m = 1, n5; do l = 1, n4; do k = 1, n3; do j = 1, n2; do i = 1, n1
             n = n + 1
             read(iUnit, *, ERR=77, END=77) &
                  TecIndex_I(1:nTecIndex), Coord_ID(n,:), Var4_IV(n,:)
          end do; end do; end do; end do; end do
       else
          do m = 1, n5; do l = 1, n4; do k = 1, n3; do j = 1, n2; do i = 1, n1
             n = n + 1
             read(iUnit, *, ERR=77, END=77) &
                  TecIndex_I(1:nTecIndex), Coord_ID(n,:), Var_IV(n,:)
          end do; end do; end do; end do; end do
       end if

    case('ascii', 'formatted')
       n = 0
       if(UseReal4)then
          do m = 1, n5; do l = 1, n4; do k = 1, n3; do j = 1, n2; do i = 1, n1
             n = n + 1
             read(iUnit, *, ERR=77, END=77) Coord_ID(n,:), Var4_IV(n,:)
          end do; end do; end do; end do; end do
       else
          do m = 1, n5; do l = 1, n4; do k = 1, n3; do j = 1, n2; do i = 1, n1
             n = n + 1
             read(iUnit, *, ERR=77, END=77) Coord_ID(n,:), Var_IV(n,:)
          end do; end do; end do; end do; end do
       end if

    case('real8')
       read(iUnit, ERR=77, END=77) Coord_ID
       if(UseReal4)then
          allocate(Var_I(n1*n2*n3*n4*n5))
          do iVar = 1, nVar
             read(iUnit, ERR=77, END=77) Var_I
             Var4_IV(:,iVar) = Var_I ! convert to single precision
          end do
          deallocate(Var_I)
       else
          do iVar = 1, nVar
             read(iUnit, ERR=77, END=77) Var_IV(:,iVar)
          end do
       end if

    case('real4')
       allocate(Coord4_ID(n1*n2*n3*n4*n5,nDim))
       read(iUnit, ERR=77, END=77) Coord4_ID
       Coord_ID = Coord4_ID ! convert to default precision
       deallocate(Coord4_ID)
       if(UseReal4) then
          do iVar = 1, nVar
             read(iUnit, ERR=77, END=77) Var4_IV(:, iVar)
          end do
       else
          allocate(Var4_I(n1*n2*n3*n4*n5))
          do iVar = 1, nVar
             read(iUnit, ERR=77, END=77) Var4_I
             Var_IV(:,iVar) = Var4_I ! copy into default precision variable
          end do
          deallocate(Var4_I)
       end if

    end select

    ! if iUnitIn is passed, keep file connected
    if(.not.present(iUnitIn)) close(iUnit)

    if(present(CoordMinOut_D)) CoordMinOut_D(1:nDim) = minval(Coord_ID, DIM=1)
    if(present(CoordMaxOut_D)) CoordMaxOut_D(1:nDim) = maxval(Coord_ID, DIM=1)

    ! Fill in output coordinate arrays
    do iDim = 1, nDim
       n = 0
       do m = 1, n5; do l = 1, n4; do k = 1, n3; do j = 1, n2; do i = 1, n1
          n = n + 1
          Coord = Coord_ID(n, iDim)
          if(present(CoordOut_DI))   CoordOut_DI(iDim,n)          = Coord
          if(present(CoordOut_I))    CoordOut_I(i)                = Coord
          if(present(CoordOut_DII))  CoordOut_DII(iDim,i,j)       = Coord
          if(present(CoordOut_DIII)) CoordOut_DIII(iDim,i,j,k)    = Coord
          if(present(CoordOut_DI4))  CoordOut_DI4(iDim,i,j,k,l)   = Coord
          if(present(CoordOut_DI5))  CoordOut_DI5(iDim,i,j,k,l,m) = Coord
          if(present(Coord1Out_I) .and. iDim==1 &
               .and. j==1 .and. k==1 .and. l==1 .and. m==1) &
               Coord1Out_I(i) = Coord
          if(present(Coord2Out_I) .and. iDim==2 &
               .and. i==1 .and. k==1 .and. l==1 .and. m==1) &
               Coord2Out_I(j) = Coord
          if(present(Coord3Out_I) .and. iDim==3 &
               .and. i==1 .and. j==1 .and. l==1 .and. m==1) &
               Coord3Out_I(k) = Coord
          if(present(Coord4Out_I) .and. iDim==4 &
               .and. i==1 .and. j==1 .and. k==1 .and. m==1) &
               Coord4Out_I(l) = Coord
          if(present(Coord5Out_I) .and. iDim==5 &
               .and. i==1 .and. j==1 .and. k==1 .and. l==1) &
               Coord5Out_I(m) = Coord
       end do; end do; end do; end do; end do
    end do

    ! Reduce nVar, if some variables are not needed
    if(present(VarOut_VI))   nVar = min(nVar,size(VarOut_VI  ,1))
    if(present(VarOut_VII))  nVar = min(nVar,size(VarOut_VII ,1))
    if(present(VarOut_VIII)) nVar = min(nVar,size(VarOut_VIII,1))
    if(present(VarOut_VI4))  nVar = min(nVar,size(VarOut_VI4, 1))
    if(present(VarOut_VI5))  nVar = min(nVar,size(VarOut_VI5, 1))
    if(present(VarOut_IV))   nVar = min(nVar,size(VarOut_IV  ,2))
    if(present(VarOut_IIV))  nVar = min(nVar,size(VarOut_IIV ,3))
    if(present(VarOut_IIIV)) nVar = min(nVar,size(VarOut_IIIV,4))
    if(present(VarOut_I4V))  nVar = min(nVar,size(VarOut_I4V, 5))
    if(present(VarOut_I5V))  nVar = min(nVar,size(VarOut_I5V, 6))

    if(present(Var4Out_VI))   nVar = min(nVar,size(Var4Out_VI  ,1))
    if(present(Var4Out_VII))  nVar = min(nVar,size(Var4Out_VII ,1))
    if(present(Var4Out_VIII)) nVar = min(nVar,size(Var4Out_VIII,1))
    if(present(Var4Out_VI4))  nVar = min(nVar,size(Var4Out_VI4, 1))
    if(present(Var4Out_VI5))  nVar = min(nVar,size(Var4Out_VI5, 1))
    if(present(Var4Out_IV))   nVar = min(nVar,size(Var4Out_IV  ,2))
    if(present(Var4Out_IIV))  nVar = min(nVar,size(Var4Out_IIV ,3))
    if(present(Var4Out_IIIV)) nVar = min(nVar,size(Var4Out_IIIV,4))
    if(present(Var4Out_I4V))  nVar = min(nVar,size(Var4Out_I4V, 5))
    if(present(Var4Out_I5V))  nVar = min(nVar,size(Var4Out_I5V, 6))

    ! Fill in output variable arrays
    do iVar = 1, nVar
       n = 0
       do m = 1, n5; do l = 1, n4; do k = 1, n3; do j = 1, n2; do i = 1, n1
          n = n + 1
          if(present(VarOut_I))    VarOut_I(n)                = Var_IV(n,iVar)
          if(present(VarOut_II))   VarOut_II(i,j)             = Var_IV(n,iVar)
          if(present(VarOut_III))  VarOut_III(i,j,k)          = Var_IV(n,iVar)
          if(present(VarOut_I4))   VarOut_I4(i,j,k,l)         = Var_IV(n,iVar)
          if(present(VarOut_I5))   VarOut_I5(i,j,k,l,m)       = Var_IV(n,iVar)
          if(present(VarOut_VI))   VarOut_VI(iVar,n)          = Var_IV(n,iVar)
          if(present(VarOut_VII))  VarOut_VII(iVar,i,j)       = Var_IV(n,iVar)
          if(present(VarOut_VIII)) VarOut_VIII(iVar,i,j,k)    = Var_IV(n,iVar)
          if(present(VarOut_VI4))  VarOut_VI4(iVar,i,j,k,l)   = Var_IV(n,iVar)
          if(present(VarOut_VI5))  VarOut_VI5(iVar,i,j,k,l,m) = Var_IV(n,iVar)
          if(present(VarOut_IV))   VarOut_IV(i,iVar)          = Var_IV(n,iVar)
          if(present(VarOut_IIV))  VarOut_IIV(i,j,iVar)       = Var_IV(n,iVar)
          if(present(VarOut_IIIV)) VarOut_IIIV(i,j,k,iVar)    = Var_IV(n,iVar)
          if(present(VarOut_I4V))  VarOut_I4V(i,j,k,l,iVar)   = Var_IV(n,iVar)
          if(present(VarOut_I5V))  VarOut_I5V(i,j,k,l,m,iVar) = Var_IV(n,iVar)

          if(present(Var4Out_I))   Var4Out_I(n)               = Var4_IV(n,iVar)
          if(present(Var4Out_II))  Var4Out_II(i,j)            = Var4_IV(n,iVar)
          if(present(Var4Out_III)) Var4Out_III(i,j,k)         = Var4_IV(n,iVar)
          if(present(Var4Out_I4))  Var4Out_I4(i,j,k,l)        = Var4_IV(n,iVar)
          if(present(Var4Out_I5))  Var4Out_I5(i,j,k,l,m)      = Var4_IV(n,iVar)
          if(present(Var4Out_VI))  Var4Out_VI(iVar,n)         = Var4_IV(n,iVar)
          if(present(Var4Out_VII)) Var4Out_VII(iVar,i,j)      = Var4_IV(n,iVar)
          if(present(Var4Out_VIII))Var4Out_VIII(iVar,i,j,k)   = Var4_IV(n,iVar)
          if(present(Var4Out_VI4)) Var4Out_VI4(iVar,i,j,k,l)  = Var4_IV(n,iVar)
          if(present(Var4Out_VI5)) Var4Out_VI5(iVar,i,j,k,l,m)= Var4_IV(n,iVar)
          if(present(Var4Out_IV))  Var4Out_IV(i,iVar)         = Var4_IV(n,iVar)
          if(present(Var4Out_IIV)) Var4Out_IIV(i,j,iVar)      = Var4_IV(n,iVar)
          if(present(Var4Out_IIIV))Var4Out_IIIV(i,j,k,iVar)   = Var4_IV(n,iVar)
          if(present(Var4Out_I4V)) Var4Out_I4V(i,j,k,l,iVar)  = Var4_IV(n,iVar)
          if(present(Var4Out_I5V)) Var4Out_I5V(i,j,k,l,m,iVar)= Var4_IV(n,iVar)

       end do; end do; end do; end do; end do
    end do

    deallocate(Coord_ID)
    if(allocated(Var_IV))  deallocate(Var_IV)
    if(allocated(Var4_IV)) deallocate(Var4_IV)

    RETURN

77  if(.not.present(iErrorOut)) call CON_stop(NameSub // &
         ' could not read data from file=' // trim(NameFile))

    iErrorOut = 3
    close(iUnit)
    if(allocated(Coord_ID))  deallocate(Coord_ID)
    if(allocated(Coord4_ID)) deallocate(Coord4_ID)
    if(allocated(Var_IV))    deallocate(Var_IV)
    if(allocated(Var4_IV))   deallocate(Var4_IV)

  contains
    !==========================================================================
    subroutine read_header

      ! Read header information

      character(len=200):: StringMisc
      logical:: DoAddSpace
      integer:: i
      !------------------------------------------------------------------------
      n_D = 1
      select case(TypeFile)
      case('ascii', 'formatted')
         open(iUnit, file=NameFile, status='old', ERR=66)

         read(iUnit, '(a)', ERR=77, END=77) StringHeader
         read(iUnit, *    , ERR=77, END=77) nStep, Time, nDim, nParam, nVar
         read(iUnit, *    , ERR=77, END=77) n_D(1:abs(nDim))
         if(nParam > 0)then
            allocate(Param_I(nParam))
            read(iUnit, *    , ERR=77, END=77) Param_I
         end if
         read(iUnit, '(a)', ERR=77, END=77) NameVar
      case('tec')
         open(iUnit, file=NameFile, status='old', ERR=66)
         ! skip title line
         read(iUnit,'(a)') StringHeader
         ! read NameVar into StringHeader
         read(iUnit,'(a)') StringHeader
         ! read n_D
         read(iUnit,'(a28,i6)', ADVANCE="NO")StringMisc, n_D(1)
         read(iUnit, '(a4,i6)', ADVANCE="NO")StringMisc, n_D(2)
         read(iUnit, '(a4,i6)'              )StringMisc, n_D(3)
         ! read nStep, Time, nDim, nParam, nVar
         read(iUnit,'(a14,i8)')     StringMisc, nStep
         read(iUnit,'(a17,es18.10)')StringMisc, Time
         read(iUnit,'(a14,i3)')     StringMisc, nDim
         read(iUnit,'(a16,i3)')     StringMisc, nParam
         read(iUnit,'(a14,i3)')     StringMisc, nVar
         ! read Param_I
         if(nParam > 0)then
            allocate(Param_I(nParam))
         end if
         do i = 1, nParam
            read(iUnit,'(a)') StringMisc
            read(StringMisc(len_trim(StringMisc)-18:len_trim(StringMisc)-1),&
                 '(es18.10)') Param_I(i)
         end do

         ! NameVar is stored in the header => process it
         DoAddSpace = .false.
         NameVar = ''
         n = 11 + 5 * count(n_D>1, 1)
         do i = n, len_trim(StringHeader)
            select case(StringHeader(i:i))
            case('"',' ',',')
               DoAddSpace = .true.
            case default
               if(DoAddSpace)then
                  NameVar = trim(NameVar)//' '//StringHeader(i:i)
                  DoAddSpace = .false.
               else
                  NameVar = trim(NameVar)//StringHeader(i:i)
               end if
            end select
         end do

         ! reset header
         StringHeader = ''

      case('real8')
         open(iUnit, file=NameFile, status='old', form='unformatted', ERR=66)

         read(iUnit, ERR=77, END=77) StringHeader
         read(iUnit, ERR=77, END=77) nStep, Time, nDim, nParam, nVar
         read(iUnit, ERR=77, END=77) n_D(1:abs(nDim))
         if(nParam > 0)then
            allocate(Param_I(nParam))
            read(iUnit, ERR=77, END=77) Param_I
         end if
         read(iUnit, ERR=77, END=77) NameVar
      case('real4')
         open(iUnit, file=NameFile, status='old', form='unformatted', ERR=66)

         read(iUnit, ERR=77, END=77) StringHeader
         read(iUnit, ERR=77, END=77) nStep, Time4, nDim, nParam, nVar

         Time = Time4
         read(iUnit, ERR=77, END=77) n_D(1:abs(nDim))
         if(nParam > 0)then
            allocate(Param_I(nParam), Param4_I(nParam))
            read(iUnit, ERR=77, END=77) Param4_I
            Param_I = Param4_I
            deallocate(Param4_I)
         end if
         read(iUnit, ERR=77, END=77) NameVar
      case default
         call CON_stop(NameSub // ' unknown TypeFile =' // trim(TypeFile))
      end select

      IsCartesian = nDim > 0
      nDim = abs(nDim)
      n1 = n_D(1); n2 = n_D(2); n3 = n_D(3); n4 = n_D(4); n5 = n_D(5)

      if(present(StringHeaderOut)) StringHeaderOut = trim(StringHeader)
      if(present(NameVarOut))      NameVarOut      = trim(NameVar)
      if(present(TimeOut))         TimeOut         = Time
      if(present(nStepOut))        nStepOut        = nStep
      if(present(nDimOut))         nDimOut         = nDim
      if(present(nParamOut))       nParamOut       = nParam
      if(present(nVarOut))         nVarOut         = nVar
      if(present(n1Out))           n1Out           = n1
      if(present(n2Out))           n2Out           = n2
      if(present(n3Out))           n3Out           = n3
      if(present(n4Out))           n4Out           = n4
      if(present(n5Out))           n5Out           = n5
      if(present(nOut_D))then
         nOut_D          = 1
         nOut_D(1:nDim)  = n_D(1:nDim)
      end if
      if(present(IsCartesianOut))  IsCartesianOut  = IsCartesian
      if(present(ParamOut_I) .and. nParam > 0) &
           ParamOut_I(1:nParam) = Param_I

      if(allocated(Param_I)) deallocate(Param_I)

      RETURN

66    if(.not.present(iErrorOut)) call CON_stop(NameSub // &
           ' could not open '//trim(TypeFile)//' file=' // trim(NameFile))

      iErrorOut = 1
      RETURN

77    if(.not.present(iErrorOut)) call CON_stop(NameSub // &
           ' could not read header from file=' // trim(NameFile))

      iErrorOut = 2
      close(iUnit)
      if(allocated(Param_I)) deallocate(Param_I)
      if(allocated(Param4_I)) deallocate(Param4_I)
      RETURN

    end subroutine read_header
    !==========================================================================
  end subroutine read_plot_file
  !============================================================================
  subroutine test_plot_file

    ! Set up a hydro shock tube initial condition on a 2D Cartesian grid
    ! Save plot file then read it and check consistency
    ! Do this multiple times with various settings

    character(len=*), parameter:: StringHeaderIn = "test_hd22"
    real,    parameter :: TimeIn = 25.0
    integer, parameter :: nStepIn = 10, nDimIn = 2, nParamIn = 2, nVarIn = 4
    integer, parameter :: n1In= 10, n2In = 2
    real,    parameter :: CoordMinIn_D(nDimIn) = [ 0.5, -0.5 ]
    real,    parameter :: CoordMaxIn_D(nDimIn) = [ 9.5,  0.5 ]
    real,    parameter :: ParamIn_I(nParamIn) = [ 1.667, 2.5 ]
    character(len=*), parameter:: NameVarIn = "x y rho ux uy p gamma rbody"
    real    :: CoordIn_DII(nDimIn, n1In, n2In)
    real    :: VarIn_VII(nVarIn, n1In, n2In)
    real    :: CoordIn_DIII(nDimIn, n1In, 1, n2In)
    real    :: VarIn_VIII(nVarIn, n1In, 1, n2In)
    real    :: VarIn_IIV(n1In, n2In, nVarIn)

    ! Do tests with ascii/real8/real4 files,
    ! Cartesian/non-Cartesian coordinates
    ! 2D/3D input arrays
    integer, parameter:: nTest = 15
    character(len=5)  :: TypeFileIn_I(nTest) = &
         [ 'ascii', 'real8', 'real4', 'ascii', 'real8', 'real4', &
         'ascii', 'real8', 'real4', 'ascii', 'real8', 'real4', &
         'ascii', 'real8', 'real4' ]
    logical           :: IsCartesianIn_I(nTest) = &
         [ .true.,   .true., .true.,  .false.,   .false., .false.,&
         .true.,   .true., .true.,  .false.,   .false., .false.,&
         .true., .false., .false. ]

    ! Input and output of tests
    character(len=80)    :: NameFile
    character(len=20)    :: TypeFileIn
    character(len=100)   :: StringHeaderOut
    real                 :: TimeOut
    integer              :: nStepOut, nDimOut, nParamOut, nVarOut
    integer              :: nOut_D(3)
    real                 :: ParamOut_I(100)
    character(len=100)   :: NameVarOut
    logical              :: IsCartesianIn, IsCartesianOut
    real                 :: CoordMinOut_D(nDimIn), CoordMaxOut_D(nDimIn)
    real                 :: Coord1Out_I(n1In), Coord2Out_I(n2In)
    real                 :: CoordOut_DII(nDimIn, n1In, n2In)
    real                 :: VarOut_VII(nVarIn, n1In, n2In)

    ! Tolerance for errors
    real :: Eps

    ! Indexes
    integer :: i, j, iTest

    ! Initialize coordinates and variables: shock tube on a 2D uniform grid
    character(len=*), parameter:: NameSub = 'test_plot_file'
    !--------------------------------------------------------------------------
    do j = 1, n2In; do i = 1, n1In
       CoordIn_DII(1, i, j) = CoordMinIn_D(1) &
            + (i-1)*((CoordMaxIn_D(1)-CoordMinIn_D(1))/(n1In - 1))
       CoordIn_DII(2, i, j) = CoordMinIn_D(2) &
            + (j-1)*((CoordMaxIn_D(2)-CoordMinIn_D(2))/(n2In - 1))
       CoordIn_DIII(1, i, 1, j) = CoordIn_DII(1,i,j)
       CoordIn_DIII(2, i, 1, j) = CoordIn_DII(2,i,j)

       if(i <= n1In/2)then
          VarIn_VII(:, i, j) = [ 1.0, 0.0, 0.0, 1.0 ]
          VarIn_IIV(i, j, :) = [ 1.0, 0.0, 0.0, 1.0 ]
          VarIn_VIII(:, i, 1, j) = [ 1.0, 0.0, 0.0, 1.0 ]
       else
          VarIn_VII(:, i, j) = [ 0.1, 0.0, 0.0, 0.125 ]
          VarIn_IIV(i, j, :) = [ 0.1, 0.0, 0.0, 0.125 ]
          VarIn_VIII(:, i, 1, j) = [ 0.1, 0.0, 0.0, 0.125 ]
       end if
    end do; end do

    ! Test ascii, real8 and real4 files
    do iTest = 1, nTest
       write(NameFile, '(a,i2.2,a)') 'test_plot_file',iTest,'.out'
       write(*,*) NameSub, ' writing file=', trim(NameFile)

       TypeFileIn    = TypeFileIn_I(iTest)
       IsCartesianIn = IsCartesianIn_I(iTest)

       if(TypeFileIn == 'real4')then
          Eps = 1e-5
       else
          Eps = 1e-12
       end if

       ! Test saving it
       select case(iTest)
       case(1)
          ! Use coordinate ranges
          call save_plot_file(NameFile,      &
               TypeFileIn     = TypeFileIn,     &
               StringHeaderIn = StringHeaderIn, &
               StringFormatIn      = '(10f8.3)',&
               StringFormatParamIn = '(10f6.3)',&
               nStepIn        = nStepIn,        &
               TimeIn         = TimeIn,         &
               ParamIn_I      = ParamIn_I,      &
               NameVarIn      = NameVarIn,      &
               IsCartesianIn  = IsCartesianIn,  &
               CoordMinIn_D   = CoordMinIn_D,   &
               CoordMaxIn_D   = CoordMaxIn_D,   &
               VarIn_IIV      = VarIn_IIV)
       case(2)
          ! Use 1D coordinate arrays
          call save_plot_file(NameFile,      &
               TypeFileIn     = TypeFileIn,     &
               StringHeaderIn = StringHeaderIn, &
               nStepIn        = nStepIn,        &
               TimeIn         = TimeIn,         &
               ParamIn_I      = ParamIn_I,      &
               NameVarIn      = NameVarIn,      &
               IsCartesianIn  = IsCartesianIn,  &
               Coord1In_I     = CoordIn_DII(1,:,1), &
               Coord2In_I     = CoordIn_DII(2,1,:), &
               VarIn_VII      = VarIn_VII)
       case(3:6)
          ! Use full coordinate array
          call save_plot_file(NameFile,      &
               TypeFileIn     = TypeFileIn,     &
               StringHeaderIn = StringHeaderIn, &
               nStepIn        = nStepIn,        &
               TimeIn         = TimeIn,         &
               ParamIn_I      = ParamIn_I,      &
               NameVarIn      = NameVarIn,      &
               IsCartesianIn  = IsCartesianIn,  &
               CoordIn_DII    = CoordIn_DII,    &
               VarIn_VII      = VarIn_VII)
       case default
          ! Test 3D input array
          ! Use full coordinate array
          call save_plot_file(NameFile,      &
               TypeFileIn     = TypeFileIn,     &
               StringHeaderIn = StringHeaderIn, &
               nStepIn        = nStepIn,        &
               TimeIn         = TimeIn,         &
               ParamIn_I      = ParamIn_I,      &
               NameVarIn      = NameVarIn,      &
               IsCartesianIn  = IsCartesianIn,  &
               nDimIn         = nDimIn,         &
               CoordIn_DIII   = CoordIn_DIII,   &
               VarIn_VIII     = VarIn_VIII)

       end select

       if(iTest == 13 .or. iTest == 14 .or. iTest == 15)then
          ! Read header and data separately
          call read_plot_file(NameFile,        &
               TypeFileIn      = TypeFileIn,      &
               StringHeaderOut = StringHeaderOut, &
               nStepOut        = nStepOut,        &
               TimeOut         = TimeOut,         &
               nDimOut         = nDimOut,         &
               nParamOut       = nParamOut,       &
               nVarOut         = nVarOut,         &
               ParamOut_I      = ParamOut_I,      &
               NameVarOut      = NameVarOut,      &
               IsCartesianOut  = IsCartesianOut)
          call read_plot_file(NameFile,        &
               TypeFileIn      = TypeFileIn,      &
               CoordOut_DII    = CoordOut_DII,    &
               Coord1Out_I     = Coord1Out_I,     &
               Coord2Out_I     = Coord2Out_I,     &
               CoordMinOut_D   = CoordMinOut_D,   &
               CoordMaxOut_D   = CoordMaxOut_D,   &
               VarOut_VII      = VarOut_VII)
       else
          call read_plot_file(NameFile,        &
               TypeFileIn      = TypeFileIn,      &
               StringHeaderOut = StringHeaderOut, &
               nStepOut        = nStepOut,        &
               TimeOut         = TimeOut,         &
               nDimOut         = nDimOut,         &
               nParamOut       = nParamOut,       &
               nVarOut         = nVarOut,         &
               ParamOut_I      = ParamOut_I,      &
               NameVarOut      = NameVarOut,      &
               IsCartesianOut  = IsCartesianOut,  &
               CoordOut_DII    = CoordOut_DII,    &
               Coord1Out_I     = Coord1Out_I,     &
               Coord2Out_I     = Coord2Out_I,     &
               CoordMinOut_D   = CoordMinOut_D,   &
               CoordMaxOut_D   = CoordMaxOut_D,   &
               VarOut_VII      = VarOut_VII)

       end if

       if(nStepOut /= nStepIn)then
          write(*,*)'nStepIn=', nStepIn,' nStepOut=', nStepOut
          call CON_stop(NameSub)
       end if

       if(abs(TimeOut - TimeIn) > Eps)then
          write(*,*)'TimeIn=', TimeIn,' TimeOut=', TimeOut
          call CON_stop(NameSub)
       end if

       if(nDimOut /= nDimIn)then
          write(*,*)'nDimIn=', nDimIn,' nDimOut=', nDimOut
          call CON_stop(NameSub)
       end if

       if(nParamOut /= nParamIn)then
          write(*,*)'nParamIn=', nParamIn,' nParamOut=', nParamOut
          call CON_stop(NameSub)
       end if

       if(nVarOut /= nVarIn)then
          write(*,*)'nVarIn=', nVarIn,' nVarOut=', nVarOut
          call CON_stop(NameSub)
       end if

       if(any(abs(ParamOut_I(1:nParamIn) - ParamIn_I) > Eps))then
          write(*,*)'ParamIn=', ParamIn_I,' ParamOut=', ParamOut_I(1:nParamIn)
          call CON_stop(NameSub)
       end if

       if(IsCartesianOut .neqv. IsCartesianIn)then
          write(*,*)'IsCartesianIn, Out=', IsCartesianIn, IsCartesianOut
          call CON_stop(NameSub)
       end if

       if(NameVarOut /= NameVarIn)then
          write(*,*)'NameVarIn=', NameVarIn,' NameVarOut=', NameVarOut
          call CON_stop(NameSub)
       end if

       ! To simplify, replace the 3D input array with 2D
       if(iTest > 6)then
          CoordIn_DII = CoordIn_DIII(:,:,1,:)
          VarIn_VII = VarIn_VIII(:,:,1,:)
       end if
       do j = 1, n2In; do i = 1, n1In
          if(any(abs(CoordIn_DII(:,i,j) - CoordOut_DII(:,i,j)) > Eps))then
             write(*,*)'i,j=', i, j
             write(*,*)'CoordIn =', CoordIn_DII(:,i,j)
             write(*,*)'CoordOut=', CoordOut_DII(:,i,j)
             call CON_stop(NameSub)
          end if
          if(abs(CoordIn_DII(1,i,j) - Coord1Out_I(i)) > Eps )then
             write(*,*)'i,j=', i, j
             write(*,*)'CoordIn(1)=', CoordIn_DII(1,i,j)
             write(*,*)'Coord1Out =', Coord1Out_I(i)
             call CON_stop(NameSub)
          end if
          if(abs(CoordIn_DII(2,i,j) - Coord2Out_I(j)) > Eps )then
             write(*,*)'i,j=', i, j
             write(*,*)'CoordIn(2)=', CoordIn_DII(2,i,j)
             write(*,*)'Coord2Out =', Coord2Out_I(j)
             call CON_stop(NameSub)
          end if
          if(any(abs(VarIn_VII(:,i,j) - VarOut_VII(:,i,j)) > Eps))then
             write(*,*)'i,j=', i, j
             write(*,*)'VarIn =', VarIn_VII(:,i,j)
             write(*,*)'VarOut=', VarOut_VII(:,i,j)
             call CON_stop(NameSub)
          end if
          if(any(abs(VarIn_IIV(i,j,:) - VarOut_VII(:,i,j)) > Eps))then
             write(*,*)'i,j=', i, j
             write(*,*)'VarIn =', VarIn_IIV(i,j,:)
             write(*,*)'VarOut=', VarOut_VII(:,i,j)
             call CON_stop(NameSub)
          end if
       end do; end do

       if(abs(CoordMinOut_D(1) -  minval(CoordIn_DII(1,:,:))) >Eps)then
          write(*,*)'CoordMinOut_D(1)     =',CoordMinOut_D(1)
          write(*,*)'minval(CoordIn_DII(1)=',minval(CoordIn_DII(1,:,:))
          call CON_stop(NameSub)
       end if
       if(abs(CoordMinOut_D(2) -  minval(CoordIn_DII(2,:,:))) > Eps)then
          write(*,*)'CoordMinOut_D(2)     =',CoordMinOut_D(2)
          write(*,*)'minval(CoordIn_DII(2)=',minval(CoordIn_DII(2,:,:))
          call CON_stop(NameSub)
       end if
       if(abs(CoordMaxOut_D(1) -  maxval(CoordIn_DII(1,:,:))) > Eps )then
          write(*,*)'CoordMaxOut_D(1)     =',CoordMaxOut_D(1)
          write(*,*)'maxval(CoordIn_DII(1)=',maxval(CoordIn_DII(1,:,:))
          call CON_stop(NameSub)
       end if
       if(abs(CoordMaxOut_D(2) - maxval(CoordIn_DII(2,:,:))) >Eps)then
          write(*,*)'CoordMaxOut_D(2)     =',CoordMaxOut_D(2)
          write(*,*)'maxval(CoordIn_DII(2)=',maxval(CoordIn_DII(2,:,:))
          call CON_stop(NameSub)
       end if

    end do

    ! Test using defaults for 2D input array
    NameFile = 'test_plot_file16.out'
    call save_plot_file(NameFile, VarIn_VII=VarIn_VII, CoordIn_DII=CoordIn_DII)

    call read_plot_file(NameFile, &
         StringHeaderOut=StringHeaderOut, NameVarOut=NameVarOut, &
         nDimOut=nDimOut, nVarOut=nVarOut, nParamOut=nParamOut, &
         IsCartesianOut=IsCartesianOut, nOut_D=nOut_D)

    if( StringHeaderOut /= 'No header info')call CON_stop(NameSub // &
         ' incorrect value for default StringHeaderOut='//StringHeaderOut)
    if( NameVarOut /= 'x1 x2 v01 v02 v03 v04') call CON_stop(NameSub // &
         ' incorrect value for default NameVarOut='//NameVarOut)
    if(.not.IsCartesianOut) call CON_stop(NameSub // &
         ' incorrect value for IsCartesianOut: should be true')
    if( any(nOut_D(1:nDimOut) /= [ n1In, n2In ]) ) then
       write(*,*) 'n1In, n2In=', n1In, n2In
       write(*,*) 'nOut_D    =',nOut_D
       call CON_stop(NameSub)
    end if

    ! Test using defaults for 3D input array
    NameFile = 'test_plot_file17.out'
    call save_plot_file(NameFile,nDimIn = nDimIn, VarIn_VIII = VarIn_VIII,&
         CoordIn_DIII = CoordIn_DIII)

    ! Read header info
    call read_plot_file(NameFile, &
         StringHeaderOut = StringHeaderOut, NameVarOut = NameVarOut, &
         nDimOut = nDimOut, nVarOut = nVarOut, nParamOut = nParamOut, &
         IsCartesianOut = IsCartesianOut, nOut_D = nOut_D)

    if( StringHeaderOut /= 'No header info')call CON_stop(NameSub // &
         ' incorrect value for default StringHeaderOut='//StringHeaderOut)
    if( NameVarOut /= 'x1 x2 v01 v02 v03 v04') call CON_stop(NameSub // &
         ' incorrect value for default NameVarOut='//NameVarOut)
    if(.not.IsCartesianOut) call CON_stop(NameSub // &
         ' incorrect value for IsCartesianOut: should be true')
    if( any(nOut_D(1:nDimOut) /= [ n1In, n2In ]) ) then
       write(*,*) 'n1In, n2In=', n1In, n2In
       write(*,*) 'nOut_D    =',nOut_D
       call CON_stop(NameSub)
    end if

    ! Now that we have the dimensions, we could allocate coordinate and
    ! variable arrays and read them

  end subroutine test_plot_file
  !============================================================================
  subroutine CON_stop(String)

    ! use mpi

    character(len=*), intent(in):: String

    ! Stop execution after writing out String

    integer:: iProc=0
    ! call MPI_initialized(IsMpiInitialized, iError)
    ! if(IsMpiInitialized) call MPI_comm_rank(MPI_COMM_WORLD, iProc, iError)

    !--------------------------------------------------------------------------
    write(*,*) 'ERROR: aborting execution on processor', iProc, &
         ' with message:'
    write(*,'(a)') 'ERROR: '//String

    ! Stop execution
    ! if(IsMpiInitialized) call MPI_abort(MPI_COMM_WORLD, nError, iError)
    stop

  end subroutine CON_stop
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
  subroutine open_file(iUnitIn, File, Form, Status, Position, Access, Recl, &
       iComm, NameCaller, iErrorOut)

    ! Interface for the Fortran open statement with error checking.
    ! If an error occurs, the code stops and writes out the unit number,
    ! the error code and the name of the file.
    ! If NameCaller is present, it is also shown.
    ! If no unit number is passed, open UnitTmp_.
    ! Default format is 'formatted' as in the open statement.
    ! Default status is 'replace' (not unknown) as it is well defined.
    ! Default position is 'rewind' (not asis) as it is well defined.
    ! Default access is 'sequential' as in the open statement.
    ! There is no default record length Recl.
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
    integer,          optional, intent(in):: Recl
    integer,          optional, intent(in):: iComm
    character(len=*), optional, intent(in):: NameCaller
    integer,          optional, intent(out):: iErrorOut

    character(len=20):: TypeForm, TypeStatus, TypePosition, TypeAccess

    integer:: iUnit
    integer:: iError, iProc, nProc

    character(len=*), parameter:: NameSub = 'open_file'
    !--------------------------------------------------------------------------
    iUnit = UnitTmp_
    if(present(iUnitIn)) iUnit = iUnitIn

    TypeForm = 'formatted'
    if(present(Form)) TypeForm = Form

    TypeStatus = 'replace'
    if(present(Status)) TypeStatus = Status

    TypePosition = 'rewind'
    if(present(Position)) TypePosition = Position

    TypeAccess = 'sequential'
    if(present(Access)) TypeAccess = Access

    if(present(iErrorOut))iErrorOut = 0

    if(present(Recl))then
       if(present(iComm))then
          ! Get iProc and nProc
          call MPI_comm_size(iComm, nProc, iError)
          call MPI_comm_rank(iComm, iProc, iError)
          ! Open file with status "replace" on processor 0
          if(iProc == 0) &
               open(iUnit, FILE=File, FORM=TypeForm, STATUS='replace', &
               ACCESS=TypeAccess, RECL=Recl, IOSTAT=iError)
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
                  ACCESS=TypeAccess, RECL=Recl, IOSTAT=iError)
          end if
       else
          open(iUnit, FILE=File, FORM=TypeForm, STATUS=TypeStatus, &
               ACCESS=TypeAccess, RECL=Recl, IOSTAT=iError)
       end if
    else
       open(iUnit, FILE=File, FORM=TypeForm, STATUS=TypeStatus, &
            POSITION=TypePosition, ACCESS=TypeAccess, IOSTAT=iError)
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
end module ModPlotFileSimple
!==============================================================================
