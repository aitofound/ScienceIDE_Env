!  Copyright (C) 2002 Regents of the University of Michigan, 
!  portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf

!  $Id$
!==========================================================================
module PC_wrapper
  use CON_time
  implicit none

  private ! except

  ! Coupling with CON
  public:: PC_set_param
  public:: PC_init_session
  public:: PC_run
  public:: PC_save_restart
  public:: PC_finalize

  ! Point coupling
  public:: PC_get_grid_info 
  public:: PC_find_points

  ! GM coupling
  public:: PC_put_from_gm  
  public:: PC_get_for_gm
  public:: PC_put_from_gm_dt
  public:: PC_put_from_gm_grid_info
  public:: PC_put_from_gm_init


contains

  subroutine PC_set_param(CompInfo, TypeAction)

    use CON_comp_info
    use ModReadParam

    integer :: iComm,iProc,nProc,nThread

    ! Arguments
    type(CompInfoType), intent(inout) :: CompInfo   ! Information for this comp
    character (len=*), intent(in)     :: TypeAction ! What to do

    ! Contains the PARAM.in segment
    character(len=lStringLine), allocatable :: StringLineF_I(:) 

    character (len=*), parameter :: NameSub='PC_set_param'
    character (len=2) ComponentName
    !-------------------------------------------------------------------------

    ComponentName=CompInfo%name
    call amps_set_component_name(ComponentName)

    !call AMPS_TimeStep()


    select case(TypeAction)
    case('VERSION')
       call put(CompInfo,&
            Use        =.true., &
            NameVersion='AMPS', &
            Version    =1.0)

    case('MPI')
       call get(CompInfo, iComm=iComm, iProc=iProc, nProc=nProc, nThread=nThread)
       call AMPS_SetMpiCommunicator(iComm, iProc, nProc,nThread)

    case('CHECK')
       ! AMPS could check now the input parameters for consistency

    case('READ')
       ! get section of PARAM.in that contains the PT module
       allocate(StringLineF_I(i_line_read()+1:n_line_read()))
       call read_text(StringLineF_I)
       if(n_line_read()-i_line_read()+1 > 0) then
          call amps_read_param(StringLineF_I, n_line_read()-i_line_read(), &
               lStringLine,iProc)
       end if
    case('STDOUT')
       ! call AMPS_set_stdout

    case('FILEOUT')
       ! call AMPS_set_fileout

    case('GRID')
       ! Grid info depends on BATSRUS

    case default
       call CON_stop(NameSub//': PC_ERROR: empty version cannot be used!')
    end select

  end subroutine PC_set_param

  !============================================================================

  subroutine PC_init_session(iSession, TimeSimulation)

    !INPUT PARAMETERS:
    integer,  intent(in) :: iSession         ! session number (starting from 1)
    real,     intent(in) :: TimeSimulation   ! seconds from start time
    integer::code
    character(len=*), parameter :: NameSub='PC_init_session'

    if (DoTimeAccurate) then
       code=1
    else
       code=0
    end if

    call amps_init_session(iSession,TimeSimulation,code)

    ! call AMPS_init(iSession, TimeSimulation)

  end subroutine PC_init_session

  !============================================================================

  subroutine PC_finalize(TimeSimulation)

    !INPUT PARAMETERS:
    real,     intent(in) :: TimeSimulation   ! seconds from start time

    character(len=*), parameter :: NameSub='PC_finalize'

    call AMPS_finalize

  end subroutine PC_finalize

  !============================================================================

  subroutine PC_save_restart(TimeSimulation)

    !INPUT PARAMETERS:
    real,     intent(in) :: TimeSimulation   ! seconds from start time

    character(len=*), parameter :: NameSub='PC_save_restart'

    call amps_save_restart()
    
  end subroutine PC_save_restart

  !============================================================================

  subroutine PC_run(TimeSimulation,TimeSimulationLimit)

    implicit none

    !INPUT/OUTPUT ARGUMENTS:
    real, intent(inout):: TimeSimulation   ! current time of component

    !INPUT ARGUMENTS:
    real, intent(in):: TimeSimulationLimit ! simulation time not to be exceeded

    character(len=*), parameter :: NameSub='PC_run'
    integer::ForceReachingSimulationTimeLimit=0 

    ! call AMPS_run(TimeSimulation, TimeSimulationLimit)

    call AMPS_TimeStep(TimeSimulation, TimeSimulationLimit,& 
      ForceReachingSimulationTimeLimit) 

  end subroutine PC_run

  !============================================================================

  subroutine PC_get_grid_info(nDimOut, iGridOut, iDecompOut)
    
    ! Provide information about AMPS grid
    
    implicit none
    
    integer, intent(out):: nDimOut    ! grid dimensionality
    integer, intent(out):: iGridOut   ! grid index (increases with AMR)
    integer, intent(out):: iDecompOut ! decomposition index
    
    character(len=*), parameter :: NameSub = 'PC_get_grid_info'
    !--------------------------------------------------------------------------
    nDimOut    = 3
    !  iGridOut   = iGridOut + 1
    !  iDecompOut = iDecompOut + 1
    
    !write(*,*)namesub,' is called'
    call amps_dynamic_allocate_blocks()
    call amps_mesh_id(iDecompOut)
    !call amps_mesh_id(iGridOut)
    !write(*,*) 'iDecompOut', iDecompOut
    !write(*,*) 'iGridOut',iGridOut
    
  end subroutine PC_get_grid_info

  !==============================================================================
  subroutine PC_find_points(nDimIn, nPoint, Xyz_DI, iProc_I)

    integer, intent(in) :: nDimIn                ! dimension of position vectors
    integer, intent(in) :: nPoint                ! number of positions
    real,    intent(in) :: Xyz_DI(nDimIn,nPoint) ! positions
    integer, intent(out):: iProc_I(nPoint)       ! processor owning position

    ! Find array of points and return processor indexes owning them
    ! Could be generalized to return multiple processors...

    real::x(3) = 0.0
    integer:: iPoint, thread

    character(len=*), parameter:: NameSub = 'PC_find_points'
    !--------------------------------------------------------------------------
    do iPoint = 1, nPoint
      x(:)=0.0
      x(1:nDimIn)=Xyz_DI(:,iPoint)
      
      call amps_get_point_thread_number(thread,x)
      iProc_I(iPoint)=thread

 !      Xyz_D(1:nDimIn) = Xyz_DI(:,iPoint)*Si2No_V(UnitX_)
 !      call find_grid_block(Xyz_D, iProc_I(iPoint), iBlock)
    end do

  end subroutine PC_find_points

  !============================================================================

  subroutine PC_put_from_gm( &
       NameVar, nVar, nPoint, Data_VI, iPoint_I, Pos_DI)

    use CON_coupler, ONLY: i_proc, PC_, n_proc

    implicit none

    character(len=*), intent(inout):: NameVar ! List of variables
    integer,          intent(inout):: nVar    ! Number of variables in Data_VI
    integer,          intent(inout):: nPoint  ! Number of points in Pos_DI
    real,    intent(in), optional:: Data_VI(:,:)    ! Recv data array
    integer, intent(in), optional:: iPoint_I(nPoint)! Order of data

    real, intent(out), optional, allocatable:: Pos_DI(:,:) ! Position vectors

    integer:: iPoint, i

    character(len=*), parameter :: NameSub='PC_put_from_gm'
    !--------------------------------------------------------------------------
   
    if(.not. present(Data_VI))then
       ! call amps_dynamic_allocate_blocks()
       ! set number of grid points on this processor
       call amps_get_corner_point_number(nPoint)

       ! allocate position array
       if(allocated(Pos_DI)) deallocate(Pos_DI)
       allocate(Pos_DI(3,nPoint))

       ! get point positions from AMPS
       call amps_get_corner_point_coordinates(Pos_DI) 

    else       
       call amps_recieve_batsrus2amps_corner_point_data(&
            NameVar//char(0), nVar, Data_VI, iPoint_I)
       !    else
       !    call CON_stop(NameSub//': neither Pos_DI nor Data_VI are present!')
       call amps_dynamic_init_blocks()   
    end if

  end subroutine PC_put_from_gm
  !============================================================================

  subroutine PC_put_from_gm_grid_info(nInt, nPicGrid, nSize_I, Int_I)
    integer, intent(in)         :: nInt, nPicGrid
    integer, intent(in)         :: Int_I(nInt), nSize_I(nPicGrid)
    !-------------------------------------------------------------------------
    ! Empty function
  end subroutine PC_put_from_gm_grid_info
  !============================================================================

  subroutine PC_get_for_gm(IsNew, NameVar, nVarIn, nDimIn, nPoint, Xyz_DI, &
       Data_VI)

    ! Get data from PT to OH

    implicit none

    logical,          intent(in):: IsNew   ! true for new point array
    character(len=*), intent(in):: NameVar ! List of variables
    integer,          intent(in):: nVarIn  ! Number of variables in Data_VI
    integer,          intent(in):: nDimIn  ! Dimensionality of positions
    integer,          intent(in):: nPoint  ! Number of points in Xyz_DI

    real, intent(in) :: Xyz_DI(nDimIn,nPoint)  ! Position vectors
    real, intent(out):: Data_VI(nVarIn,nPoint) ! Data array

    !real:: Xyz_D(MaxDim), B0_D(MaxDim)
    !real:: Dist_D(MaxDim), State_V(nVar)
    !integer:: iCell_D(MaxDim)

    integer:: iPoint, iBlock, iProcFound
    
    character(len=*), parameter :: NameSub='PC_get_for_gm'
    !--------------------------------------------------------------------------


    !conver the coordinates to SI
    !do iPoint = 1, nPoint
    !  Xyz_D(:,iPoint) = Xyz_DI(:,iPoint)*Si2No_V(UnitX_)
    !end do
    !write (*,*)  NameSub,'is called'
    call amps_send_batsrus2amps_center_point_data(NameVar,nVarIn,nDimIn,nPoint,Xyz_DI,Data_VI)


    !Dist_D = -1.0
    !Xyz_D  =  0.0
    
    

    !do iPoint = 1, nPoint

    !   Xyz_D(1:nDim) = Xyz_DI(:,iPoint)*Si2No_V(UnitX_)
    !   call find_grid_block(Xyz_D, iProcFound, iBlock, iCell_D, Dist_D, &
    !        UseGhostCell = .true.)

    !   if(iProcFound /= iProc)then
    !      write(*,*)NameSub,' ERROR: Xyz_D, iProcFound=', Xyz_D, iProcFound
    !      call stop_mpi(NameSub//' could not find position on this proc')
    !   end if

    !   select case(nDim)
    !   case (1)
    !      State_V = linear(State_VGB(:,:,MinJ,MinK,iBlock), &
    !           nVar, MinI, MaxI, Xyz_D(1), iCell = iCell_D(1), Dist = Dist_D(1))
    !   case (2)
    !      State_V = bilinear(State_VGB(:,:,:,MinK,iBlock), &
    !           nVar, MinI, MaxI, MinJ, MaxJ, Xyz_D(1:2), iCell_D = iCell_D(1:2), Dist_D = Dist_D(1:2))
    !   case (3)
    !      State_V = trilinear(State_VGB(:,:,:,:,iBlock), &
    !           nVar, MinI, MaxI, MinJ, MaxJ, MinK, MaxK, Xyz_D, iCell_D = iCell_D, Dist_D = Dist_D)
    !   end select

    !   if(UseB0)then
    !      call get_b0(Xyz_D, B0_D)
    !      State_V(Bx_:Bz_) = State_V(Bx_:Bz_) + B0_D
    !   end if

    !   Data_VI(1:nVar,iPoint) = State_V*No2Si_V(iUnitCons_V)

    !end do

  end subroutine PC_get_for_gm


  subroutine PC_put_from_gm_dt(DtSiIn)

    real,    intent(in) :: DtSiIn

    character(len=*), parameter :: NameSub = 'PC_put_from_gm_dt'
    !--------------------------------------------------------------------------

    ! How the IPIC3D time step is determined?
    ! 1) PC_put_from_gm_dt calls ipic3d_set_swmf_dt to send the dt (SWMFDt),
    !    which is decided by SWMF coupling frequency, to IPIC3D.
    ! 2) PC_run calls ipic3d_cal_dt, which calculate dt (PICDt) based on PC command
    !    #TIMESTEPPING. If useSWMFDt (see InterfaceFluid.h) is true, PICDt equals to
    !    SWMFDt.
    ! 3) Inside PC_run, correct PICDt to satisfy the time limit. Call
    !    ipic3d_set_dt to set the corrected dt for PIC. 

    ! Store the time step, set it when we do PC_run
    ! DtSi = DtSiIn

    ! call ipic3d_set_swmf_dt(DtSi)

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
    ! nDim = iParam_I(1) 
    call amps_from_gm_init(iParam_I, Param_I, NameVar)

  end subroutine PC_put_from_gm_init
  !============================================================================


end module PC_wrapper
