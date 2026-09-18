!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf

module PC_wrapper

  ! Wrapper for the FLEKS (PC) component

  use ModUtilities, ONLY: CON_set_do_test, CON_stop

  implicit none

  save

  private ! except

  public:: PC_set_param
  public:: PC_init_session
  public:: PC_run
  public:: PC_save_restart
  public:: PC_finalize

  ! CON_coupler_points
  public:: PC_get_grid_info
  public:: PC_find_points

  ! GM coupler
  public:: PC_put_from_gm_dt
  public:: PC_put_from_gm_init
  public:: PC_put_from_gm_grid_info
  public:: PC_put_from_gm
  public:: PC_get_for_gm

  ! Local variables
  integer:: nDim
  integer:: iProc

contains
  !==========================================================================
  subroutine PC_set_param(CompInfo, TypeAction)

    use CON_comp_info
    use ModReadParam

    ! Arguments
    type(CompInfoType), intent(inout):: CompInfo   ! Information for this comp.
    character (len=*), intent(in)    :: TypeAction ! What to do

    ! The PARAM.in segment for FLEKS
    character(len=lStringLine), allocatable :: StringLineF_I(:)

    integer:: iComm, nProc

    logical:: DoTest, DoTestMe
    character (len=*), parameter :: NameSub='PC_set_param'
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

       ! get section of PARAM.in that contains the PC module
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
       call CON_stop(NameSub//': PC_ERROR: unknown Action='//TypeAction)
    end select

  end subroutine PC_set_param
  !============================================================================
  subroutine PC_init_session(iSession, TimeSimulation)

    !INPUT PARAMETERS:
    integer,  intent(in) :: iSession         ! session number (starting from 1)
    real,     intent(in) :: TimeSimulation   ! seconds from start time

    character(len=*), parameter :: NameSub='PC_init_session'
    !--------------------------------------------------------------------------
    call fleks_init(TimeSimulation)

  end subroutine PC_init_session
  !============================================================================
  subroutine PC_finalize(TimeSimulation)

    !INPUT PARAMETERS:
    real,     intent(in) :: TimeSimulation   ! seconds from start time

    character(len=*), parameter :: NameSub='PC_finalize'
    !-------------------------------------------------------------------------
    call fleks_end()

  end subroutine PC_finalize
  !============================================================================
  subroutine PC_save_restart(TimeSimulation)

    use CON_coupler, ONLY: NameRestartOutDirComp

    !INPUT PARAMETERS:
    real,     intent(in) :: TimeSimulation   ! seconds from start time

    integer :: LenNameRestartOutDir = 0

    character(len=*), parameter :: NameSub='PC_save_restart'
    !--------------------------------------------------------------------------
    if( NameRestartOutDirComp /= '') then
       LenNameRestartOutDir = len_trim(NameRestartOutDirComp)
    else
       LenNameRestartOutDir = 0
    end if

    call fleks_save_restart(NameRestartOutDirComp, LenNameRestartOutDir)

  end subroutine PC_save_restart
  !============================================================================
  subroutine PC_run(TimeSimulation, TimeSimulationLimit)

    !INPUT/OUTPUT ARGUMENTS:
    real, intent(inout):: TimeSimulation   ! current time of component

    !INPUT ARGUMENTS:
    real, intent(in):: TimeSimulationLimit ! simulation time not to be exceeded

    real:: Dt, PicTime

    logical:: DoTest, DoTestMe
    character(len=*), parameter :: NameSub='PC_run'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)

    if(DoTestMe)write(*,*) NameSub, &
         ' starting with TimeSimulation, TimeSimulationLimit=', &
         TimeSimulation, TimeSimulationLimit

    call fleks_run(PicTime)

    TimeSimulation = PicTime


    if(DoTestMe)write(*,*) NameSub, &
         ' finishing with TimeSimulation =', TimeSimulation

  end subroutine PC_run
  !============================================================================
  subroutine PC_get_grid_info(nDimOut, iGridOut, iDecompOut)

    integer, intent(out):: nDimOut    ! grid dimensionality
    integer, intent(out):: iGridOut   ! grid index
    integer, intent(out):: iDecompOut ! decomposition index

    logical:: IsFirstTime = .true.

    character(len=*), parameter :: NameSub = 'PC_get_grid_info'
    !--------------------------------------------------------------------------
    call fleks_get_grid_info(nDimOut, iGridOut, iDecompOut);
    nDimOut    = nDim
  end subroutine PC_get_grid_info
  !============================================================================
  subroutine PC_put_from_gm_dt(DtSiIn)

    real,    intent(in) :: DtSiIn
    character(len=*), parameter :: NameSub = 'PC_put_from_gm_dt'
    !--------------------------------------------------------------------------

  end subroutine PC_put_from_gm_dt
  !============================================================================
  subroutine PC_put_from_gm_grid_info(nInt, nGrid, nSize_I, Int_I)

    integer, intent(in)         :: nInt, nGrid
    integer, intent(in)         :: Int_I(nInt), nSize_I(nGrid)
    !--------------------------------------------------------------------------
    call fleks_set_grid_info(nInt, nSize_I, Int_I)

  end subroutine PC_put_from_gm_grid_info
  !============================================================================
  subroutine PC_put_from_gm_init(nParamInt, nParamReal, iParam_I, Param_I, &
       NameVar)

    integer, intent(in)         :: nParamInt, nParamReal! number of parameters
    integer, intent(in)         :: iParam_I(nParamInt)  ! integer parameters
    real,    intent(in)         :: Param_I(nParamReal)  ! real parameters
    character(len=*), intent(in):: NameVar              ! names of variables

    character(len=*), parameter :: NameSub = 'PC_put_from_gm_init'
    !--------------------------------------------------------------------------
    ! store GM's nDim, so it is reported as PC's nDim for the point coupler
    nDim = iParam_I(1)
    call fleks_from_gm_init(iParam_I, Param_I, NameVar)

  end subroutine PC_put_from_gm_init
  !============================================================================
  subroutine PC_find_points(nDimIn, nPoint, Xyz_DI, iProc_I)

    integer, intent(in) :: nDimIn                ! dimension of position vector
    integer, intent(in) :: nPoint                ! number of positions
    real,    intent(in) :: Xyz_DI(nDimIn,nPoint) ! positions
    integer, intent(out):: iProc_I(nPoint)       ! processor owning position

    ! Find array of points and return processor indexes owning them
    ! Could be generalized to return multiple processors...

    character(len=*), parameter:: NameSub = 'PC_find_points'
    !--------------------------------------------------------------------------

    iProc_I = -777
    call fleks_find_points(nPoint, Xyz_DI, iProc_I)

  end subroutine PC_find_points
  !============================================================================
  subroutine PC_put_from_gm( &
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
    character(len=*), parameter :: NameSub='PC_put_from_gm'
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

       ! Finishing the setup after the initial state is set from GM
       call fleks_finalize_init
    end if

  end subroutine PC_put_from_gm
  !============================================================================
  subroutine PC_get_for_gm(IsNew, NameVar, nVarIn, nDimIn, nPoint, Xyz_DI, &
       Data_VI)

    logical,          intent(in):: IsNew   ! true for new point array
    character(len=*), intent(in):: NameVar ! List of variables
    integer,          intent(in):: nVarIn  ! Number of variables in Data_VI
    integer,          intent(in):: nDimIn  ! Dimensionality of positions
    integer,          intent(in):: nPoint  ! Number of points in Xyz_DI

    real, intent(in) :: Xyz_DI(nDimIn,nPoint)  ! Position vectors
    real, intent(out):: Data_VI(nVarIn,nPoint) ! Data array

    character(len=*), parameter :: NameSub='PC_get_for_gm'
    !--------------------------------------------------------------------------
    call fleks_get_state_var( &
         nDimIn, nPoint, Xyz_DI, Data_VI, nVarIn)

  end subroutine PC_get_for_gm
  !============================================================================
end module PC_wrapper
!==============================================================================
