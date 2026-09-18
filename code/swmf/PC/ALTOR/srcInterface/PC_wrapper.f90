!  Copyright (C) 2002 Regents of the University of Michigan, 
!  portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf
!=============================================================!
module PC_wrapper
 
  use ModNumConst, ONLY: cHalfPi
 
 implicit none

  save

  private ! except

  public:: PC_set_param
  public:: PC_init_session
  public:: PC_run
  public:: PC_save_restart
  public:: PC_finalize

  ! CON_coupler_points
  public:: PC_find_points
  public:: PC_get_grid_info

 ! GM coupler
  public:: PC_get_for_gm
  public:: PC_put_from_gm_grid_info
  public:: PC_put_from_gm_dt
  public:: PC_put_from_gm_init
  public:: PC_put_from_gm


  ! variables requested via coupling: coordinates, 
  ! field line and particles indexes
  character(len=*), parameter:: NameVarRequest = 'xx yy zz fl id'
  integer,          parameter:: nVarRequest = 5


contains

  subroutine PC_run(TimeSimulation,TimeSimulationLimit)
    use PIC_ModMain,      ONLY: tSimulation
    real,intent(inout)::TimeSimulation
    real,intent(in)::TimeSimulationLimit
    !---------------------------------------------------------------------
    tSimulation = TimeSimulation
    call PIC_advance(TimeSimulationLimit)
    TimeSimulation = tSimulation
  end subroutine PC_run

  !========================================================================

  subroutine PC_init_session(iSession,TimeSimulation)
    use PIC_ModMain,      ONLY: tSimulation
    integer,  intent(in) :: iSession         ! session number (starting from 1)
    real,     intent(in) :: TimeSimulation   ! seconds from start time
    !----------------------
    call PIC_init_session(iSession)
    tSimulation = TimeSimulation
  end subroutine PC_init_session

  !======================================================================

  subroutine PC_finalize(TimeSimulation)
    real,intent(in)::TimeSimulation
    !--------------------------------------------------------------------------
    call PIC_finalize
  end subroutine PC_finalize

  !=========================================================

  subroutine PC_set_param(CompInfo,TypeAction)
    use CON_comp_info
    use PIC_ModProc, ONLY: iComm, iProc, nProc

    type(CompInfoType),intent(inout):: CompInfo
    character(len=*),  intent(in)   :: TypeAction

    character(len=*), parameter :: NameSub='PC_set_param'
    !-------------------------------------------------------------------------
    select case(TypeAction)
    case('VERSION')
       call put(CompInfo,&
            Use        =.true., &
            NameVersion='ALTOR', &
            Version    =1.0)
    case('MPI')
       call get(CompInfo, iComm=iComm, iProc=iProc, nProc=nProc)
    case('STDOUT')
       ! placeholder
    case('CHECK','READ')
       call PIC_set_param(TypeAction)
    case('GRID')
       call PC_set_grid
    case default
       call CON_stop('Can not call PC_set_param for '//trim(TypeAction))
    end select
  end subroutine PC_set_param
  !==========================
  !ROUTINE: PC_set_grid - intialize, set and broadcast adaptive block grid
  !INTERFACE:
  subroutine PC_set_grid
    !USES:
    use PC_domain_decomposition
    use CON_coupler
    use CON_comp_param,  ONLY: PC_
    use PC_BATL_lib,        ONLY: CoordMin_D, CoordMax_D

    logical:: DoTest,DoTestMe
    character(len=*), parameter:: NameSub = 'PC_set_grid'
    !---------------------------------------------------------
    DoTest=.false.;DoTestMe=.false.

    if(done_dd_init(PC_))return
    call init_decomposition(PC_,PC_,3,.true.)

    ! Note: for Cartesian grid RadiusMin=xMin and RadiusMax=xMax
    call set_coord_system(PC_, &
         TypeCoord = 'GSE',    &
         TypeGeometry = 'cartesian',  &
         Coord1_I     = (/ CoordMin_D(1), CoordMax_D(1) /), &
         Coord2_I     = (/ CoordMin_D(2), CoordMax_D(2) /), &
         Coord3_I     = (/ CoordMin_D(3), CoordMax_D(3) /)  )

    if(is_proc(PC_))then
       call init_decomposition(&
            PC_Domain,PC_,3,.true.)
       call PC_get_root_decomposition(PC_Domain)
       call PC_update_local_decomposition(PC_Domain)
       PC_Domain%IsLocal=.true.
    end if
    call CON_set_do_test('test_grids',DoTest,DoTestMe)


    if(is_proc0(PC_))call PC_get_root_decomposition(PC_)

    call bcast_decomposition(PC_)

    call synchronize_refinement(PC_,PC_Domain)
  end subroutine PC_set_grid
  !=========================
  subroutine PC_save_restart(TimeSimulation) 

    real,     intent(in) :: TimeSimulation 
    call CON_stop('Can not call PC_save restart')
  end subroutine PC_save_restart
  !=========================
 subroutine PC_get_grid_info(nDimOut, iGridOut, iDecompOut)

    integer, intent(out):: nDimOut    ! grid dimensionality
    integer, intent(out):: iGridOut   ! grid index
    integer, intent(out):: iDecompOut ! decomposition index

    logical:: IsFirstTime = .true.

    character(len=*), parameter :: NameSub = 'PC_get_grid_info'
  end subroutine PC_get_grid_info
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

    logical:: DoTest, DoTestMe
    character(len=*), parameter :: NameSub='PC_put_from_gm'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)
  end subroutine PC_put_from_gm
  !============================================================================

  subroutine PC_put_from_gm_grid_info(nInt, nPicGrid, nSize_I, Int_I)
    integer, intent(in)         :: nInt, nPicGrid
    integer, intent(in)         :: Int_I(nInt), nSize_I(nPicGrid)    
    character(len=*), parameter :: NameSub='PC_put_from_gm_grid_info'
    !--------------------------------------------------------------------------

    call CON_stop(NameSub//': PC_ERROR: empty version cannot be used!')

  end subroutine PC_put_from_gm_grid_info
 
  !============================================================================
  subroutine PC_put_from_gm_dt(DtSiIn)

    real, intent(in) :: DtSiIn

    character(len=*), parameter :: NameSub = 'PC_put_from_gm_dt'
    !--------------------------------------------------------------------------
  end subroutine PC_put_from_gm_dt
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
  end subroutine PC_put_from_gm_init
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

    character(len=*), parameter :: NameSub='GM_get_for_pc'
    !--------------------------------------------------------------------------
  end subroutine PC_get_for_gm
  !===================
end module PC_wrapper
