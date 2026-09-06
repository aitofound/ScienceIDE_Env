!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf

module PT_wrapper

  ! Wrapper for the FLEKS (PT) component

  use ModUtilities, ONLY: CON_set_do_test, CON_stop

  implicit none

  save

  private ! except

  public:: PT_set_param
  public:: PT_init_session
  public:: PT_run
  public:: PT_save_restart
  public:: PT_finalize

  ! CON_coupler_points
  public:: PT_get_grid_info
  public:: PT_find_points

  ! OH coupler
  public:: PT_put_from_oh
  public:: PT_get_for_oh

  ! SC/IH/GM coupling. Have not implemented. Empty functions
  public:: PT_put_from_sc
  public:: PT_put_from_ih
  public:: PT_put_from_gm

  ! Provides interfaces, but the following functions are empty.
  public:: PT_do_extract_lines
  public:: PT_put_coupling_param
  public:: PT_adjust_lines


  ! Local variables
  integer:: nDim
  integer:: iProc

contains
  !==========================================================================
  subroutine PT_set_param(CompInfo, TypeAction)

    use CON_comp_info
    use ModReadParam

    ! Arguments
    type(CompInfoType), intent(inout):: CompInfo   ! Information for this comp.
    character (len=*), intent(in)    :: TypeAction ! What to do

    ! The PARAM.in segment for FLEKS
    character(len=lStringLine), allocatable :: StringLineF_I(:)

    integer:: iComm, nProc

    logical:: DoTest, DoTestMe
    character (len=*), parameter :: NameSub='PT_set_param'
    !-------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)
    if(DoTestMe) write(*,*)NameSub,': TypeAction: ',TypeAction

    select case(TypeAction)
    case('VERSION')
       call put(CompInfo,&
            Use        = .true., &
            NameVersion= 'FLEKS', &
            Version    = 1.0)
    case('MPI')
       call get(CompInfo, iComm=iComm, iProc=iProc, nProc=nProc)
       call fleks_init_mpi(iComm, iProc, nProc)
    case('READ')

       ! get section of PARAM.in that contains the PT module
       allocate(StringLineF_I(i_line_read()+1:n_line_read()))
       call read_text(StringLineF_I)

       if(n_line_read()-i_line_read() > 0) then
          call fleks_read_param(StringLineF_I, &
               n_line_read()-i_line_read(), lStringLine,iProc)
       end if
    case('CHECK')

    case('STDOUT')

    case('FILEOUT')

    case('GRID')
       ! Grid depends on BATSRUS

    case default
       call CON_stop(NameSub//': PT_ERROR: unknown Action='//TypeAction)
    end select

  end subroutine PT_set_param
  !============================================================================

  subroutine PT_init_session(iSession, TimeSimulation)

    !INPUT PARAMETERS:
    integer,  intent(in) :: iSession         ! session number (starting from 1)
    real,     intent(in) :: TimeSimulation   ! seconds from start time

    character(len=*), parameter :: NameSub='PT_init_session'
    !--------------------------------------------------------------------------

    call fleks_init(TimeSimulation)

  end subroutine PT_init_session
  !============================================================================

  subroutine PT_finalize(TimeSimulation)

    !INPUT PARAMETERS:
    real,     intent(in) :: TimeSimulation   ! seconds from start time

    character(len=*), parameter :: NameSub='PT_finalize'
    !-------------------------------------------------------------------------
    call fleks_end()

  end subroutine PT_finalize
  !============================================================================

  subroutine PT_save_restart(TimeSimulation)

    use CON_coupler, ONLY: NameRestartOutDirComp

    !INPUT PARAMETERS:
    real,     intent(in) :: TimeSimulation   ! seconds from start time

    integer :: nChar

    character(len=*), parameter :: NameSub='PT_save_restart'
    !--------------------------------------------------------------------------
    nChar = 0
    if( NameRestartOutDirComp /= '') &
         nChar = len(NameRestartOutDirComp)

    call fleks_save_restart(NameRestartOutDirComp, nChar)

  end subroutine PT_save_restart
  !============================================================================

  subroutine PT_run(TimeSimulation, TimeSimulationLimit)

    !INPUT/OUTPUT ARGUMENTS:
    real, intent(inout):: TimeSimulation   ! current time of component

    !INPUT ARGUMENTS:
    real, intent(in):: TimeSimulationLimit ! simulation time not to be exceeded

    real:: Dt, PicTime

    logical:: DoTest, DoTestMe
    character(len=*), parameter :: NameSub='PT_run'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)

    if(DoTestMe)write(*,*) NameSub, &
         ' starting with TimeSimulation, TimeSimulationLimit=', &
         TimeSimulation, TimeSimulationLimit

    PicTime = TimeSimulationLimit

    call fleks_run(PicTime)

    TimeSimulation = PicTime


    if(DoTestMe)write(*,*) NameSub, &
         ' finishing with TimeSimulation =', TimeSimulation

  end subroutine PT_run
  !============================================================================

  subroutine PT_get_grid_info(nDimOut, iGridOut, iDecompOut)

    integer, intent(out):: nDimOut    ! grid dimensionality
    integer, intent(out):: iGridOut   ! grid index
    integer, intent(out):: iDecompOut ! decomposition index

    logical:: IsFirstTime = .true.

    logical:: DoTest, DoTestMe
    character(len=*), parameter :: NameSub = 'PT_get_grid_info'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)

    call fleks_get_grid_info(nDim, iGridOut, iDecompOut);
    nDimOut = nDim;
  end subroutine PT_get_grid_info
  !============================================================================
  subroutine PT_find_points(nDimIn, nPoint, Xyz_DI, iProc_I)

    integer, intent(in) :: nDimIn                ! dimension of position vector
    integer, intent(in) :: nPoint                ! number of positions
    real,    intent(in) :: Xyz_DI(nDimIn,nPoint) ! positions
    integer, intent(out):: iProc_I(nPoint)       ! processor owning position

    ! Find array of points and return processor indexes owning them
    ! Could be generalized to return multiple processors...

    logical:: DoTest, DoTestMe
    character(len=*), parameter:: NameSub = 'PT_find_points'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)

    iProc_I = -777
    call fleks_find_points(nPoint, Xyz_DI, iProc_I)

  end subroutine PT_find_points
  !============================================================================

  subroutine PT_put_from_oh( &
       NameVar, nVar, nPoint, Data_VI, iPoint_I, Pos_DI)

    character(len=*), intent(inout):: NameVar ! List of variables
    integer,          intent(inout):: nVar    ! Number of variables in Data_VI
    integer,          intent(inout):: nPoint  ! Number of points in Pos_DI
    real,    intent(in), optional  :: Data_VI(:,:)    ! Recv data array
    integer, intent(in), optional  :: iPoint_I(nPoint)! Order of data
    real, intent(out), allocatable, optional:: Pos_DI(:,:)  ! Position vectors

    ! Finish the initialization after the first coupling
    logical:: IsFirstTime = .true.

    integer:: nChar

    logical:: DoTest, DoTestMe
    character(len=*), parameter :: NameSub='PT_put_from_oh'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)
    if(.not. present(Data_VI))then
       call fleks_get_ngridpoints(nPoint)

       if(DoTest)write(*,*)NameSub,': iProc, nPoint = ', iProc, nPoint

       if(allocated(Pos_DI)) deallocate(Pos_DI)
       allocate(Pos_DI(nDim,nPoint))

       call fleks_get_grid(Pos_DI,nDim*nPoint)

       RETURN
    end if
    nChar = len(NameVar)
    call fleks_set_state_var(Data_VI, iPoint_I, nVar, NameVar, nChar)

    if(IsFirstTime)  then
       IsFirstTime = .false.

       ! Finishing the setup after the initial state is set from OH
       call fleks_finalize_init
    end if

  end subroutine PT_put_from_oh
  !============================================================================

  subroutine PT_get_for_oh(IsNew, NameVar, nVarIn, nDimIn, nPoint, Xyz_DI, &
       Data_VI)
    logical,          intent(in):: IsNew   ! true for new point array
    character(len=*), intent(in):: NameVar ! List of variables
    integer,          intent(in):: nVarIn  ! Number of variables in Data_VI
    integer,          intent(in):: nDimIn  ! Dimensionality of positions
    integer,          intent(in):: nPoint  ! Number of points in Xyz_DI

    real, intent(in) :: Xyz_DI(nDimIn,nPoint)  ! Position vectors
    real, intent(out):: Data_VI(nVarIn,nPoint) ! Data array

    logical:: DoTest, DoTestMe
    character(len=*), parameter :: NameSub='PT_get_for_oh'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)


    ! Initialize the data array to zero, since the corresponding locations
    ! may be outside of the FLEKS domain for PT->OH coupling.
    Data_VI = 0.0
    call fleks_get_for_oh( &
         nDimIn, nPoint, Xyz_DI, Data_VI, nVarIn)
  end subroutine PT_get_for_oh
  !============================================================================

  subroutine PT_put_from_gm(NameVar, nVar, nPoint, Data_VI, iPoint_I, Pos_DI)
    character(len=*), intent(inout):: NameVar ! List of variables
    integer,          intent(inout):: nVar    ! Number of variables in Data_VI
    integer,          intent(inout):: nPoint  ! Number of points in Pos_DI
    real,    intent(in), optional:: Data_VI(:,:)    ! Recv data array
    integer, intent(in), optional:: iPoint_I(nPoint)! Order of data

    real, intent(out), optional, allocatable:: Pos_DI(:,:) ! Position vectors

    character(len=*), parameter:: NameSub = 'PT_put_from_gm'
    !--------------------------------------------------------------------------
    call CON_stop(NameSub//' has not been implemented!')
  end subroutine PT_put_from_gm
  !============================================================================

  subroutine PT_put_from_sc(NameVar, nVar, nPoint, Data_VI, iPoint_I, Pos_DI)
    character(len=*), intent(inout):: NameVar ! List of variables
    integer,          intent(inout):: nVar    ! Number of variables in Data_VI
    integer,          intent(inout):: nPoint  ! Number of points in Pos_DI
    real,    intent(in), optional:: Data_VI(:,:)    ! Recv data array
    integer, intent(in), optional:: iPoint_I(nPoint)! Order of data

    real, intent(out), optional, allocatable:: Pos_DI(:,:) ! Position vectors

    character(len=*), parameter:: NameSub = 'PT_put_from_sc'
    !--------------------------------------------------------------------------
    call CON_stop(NameSub//' has not been implemented!')
  end subroutine PT_put_from_sc
  !============================================================================
  subroutine PT_put_from_ih(NameVar, nVar, nPoint, Data_VI, iPoint_I, Pos_DI)
    character(len=*), intent(inout):: NameVar ! List of variables
    integer,          intent(inout):: nVar    ! Number of variables in Data_VI
    integer,          intent(inout):: nPoint  ! Number of points in Pos_DI
    real,    intent(in), optional:: Data_VI(:,:)    ! Recv data array
    integer, intent(in), optional:: iPoint_I(nPoint)! Order of data

    real, intent(out), optional, allocatable:: Pos_DI(:,:) ! Position vectors

    character(len=*), parameter:: NameSub = 'PT_put_from_ih'
    !--------------------------------------------------------------------------
    call CON_stop(NameSub//' has not been implemented!')
  end subroutine PT_put_from_ih
  !============================================================================
  subroutine PT_put_from_ih_dt(Dt)
    real,    intent(in):: Dt
    character(len=*), parameter:: NameSub = 'PT_put_from_ih_dt'
    !--------------------------------------------------------------------------
    call CON_stop(NameSub//' has not been implemented!')
  end subroutine PT_put_from_ih_dt
  !============================================================================
  subroutine PT_do_extract_lines(DoExtract)
    logical, intent(out):: DoExtract
    character(len=*), parameter:: NameSub = 'PT_do_extract_lines'
    !--------------------------------------------------------------------------
    call CON_stop(NameSub//' has not been implemented!')
  end subroutine PT_do_extract_lines
  !============================================================================
  subroutine PT_put_coupling_param(Source_, TimeIn)
    integer,        intent(in) :: Source_
    real,           intent(in) :: TimeIn
    character(len=*), parameter:: NameSub = 'PT_put_coupling_param'
    !--------------------------------------------------------------------------
    call CON_stop(NameSub//' has not been implemented!')
  end subroutine PT_put_coupling_param
  !============================================================================
  subroutine PT_adjust_lines(Source_)
    integer, intent(in) :: Source_
    character(len=*), parameter:: NameSub = 'PT_adjust_lines'
    !--------------------------------------------------------------------------
    call CON_stop(NameSub//' has not been implemented!')
  end subroutine PT_adjust_lines
  !============================================================================
end module PT_wrapper
!==============================================================================
