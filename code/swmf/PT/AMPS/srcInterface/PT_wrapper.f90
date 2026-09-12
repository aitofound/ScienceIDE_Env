!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module PT_wrapper
  use ModConst, ONLY: cDegToRad
  use CON_coupler, ONLY: OH_,IH_,PT_,SC_, Couple_CC, Grid_C, &
       iCompSourceCouple, set_coord_system
  use CON_time
  use,intrinsic :: ieee_arithmetic
  implicit none
  SAVE

  private ! except

  ! Coupling with CON
  public:: PT_set_param
  public:: PT_init_session
  public:: PT_run
  public:: PT_save_restart
  public:: PT_finalize

  ! Point coupling
  public:: PT_get_grid_info
  public:: PT_find_points

  ! GM coupling
  public:: PT_put_from_gm

  ! OH coupling
  public:: PT_put_from_oh
  public:: PT_get_for_oh

  ! IH coupling
  public:: PT_put_from_ih

  ! SC coupling
  public:: PT_put_from_sc

  ! codes describing status of coupling with the SWMF components (OH, Ih, Sc)
  integer:: IhCouplingCode
  integer:: OhCouplingCode
  integer:: ScCouplingCode

  ! coupling operation counter (need for debugging)
  integer:: nRecvFromOH=0
  integer:: nSentToOH=0

  !----------------------------Coupling with field lines ----------------------
  ! Coupling via field line grid  with MHD components
  public:: PT_do_extract_lines
  public:: PT_put_coupling_param
  public:: PT_adjust_lines

  ! Parameters for coupling to MHD via moving lagrangian grid
  real             :: DataInputTime = 0.0, PTTime = 0.0
  ! MHD data array MHData_VIB(LagrID_:nMHData, 1:nVertexMax, 1:nLine)
  real,    pointer :: MHData_VIB(:, :, :)
  ! Number of actally used grid vertexes per each line, nVertex_B(1:nLine)
  integer, pointer :: nVertex_B(:)
  ! Grid:
  ! Mxx point number on the magnetic field line
  integer          :: nVertexMax=20000
  ! Dimensions of the grid formed by the line intersections with the spherical
  ! "origin" surface nLon*nLat, uniform in latitude grid:
  integer          :: nLon = 4, nLat = 4
  ! The radius of said origin surface, in UnitX as used in PT_set_param:
  real             :: ROrigin = 2.50
  ! Size of angular grid, in latitude and longitude, at origin
  ! surface R=ROrigin, in radians
  real             :: LonMin = -10.0*cDegToRad
  real             :: LonMax =  10.0*cDegToRad
  real             :: LatMin =  25.0*cDegToRad
  real             :: LatMax =  90.0*cDegToRad
  logical          :: DoCheck = .true., DoInit = .true.
contains
  !============================================================================
  subroutine PT_set_param(CompInfo, TypeAction)

    use CON_comp_info
    use ModReadParam
    use CON_bline,   ONLY: BL_set_grid, UseBLine_C
    use ModConst,    ONLY: rSun
    use CON_physics, ONLY: get_time
    
    
    ! Arguments
    type(CompInfoType), intent(inout) :: CompInfo   ! Information for this comp
    character (len=*), intent(in)     :: TypeAction ! What to do

    ! Contains the PARAM.in segment
    character(len=lStringLine), allocatable :: StringLineF_I(:)

    integer :: iComm, iProc, nProc, nThread, iTrue, nVar
    character(len=20):: NameVar
    
    character(len=*), parameter:: NameSub = 'PT_set_param'
    !--------------------------------------------------------------------------
    if(CompInfo%use) call amps_set_component_name('PT')

    select case(TypeAction)
    case('VERSION')
       call put(CompInfo,&
            Use        =.true., &
            NameVersion='AMPS', &
            Version    =1.0)

    case('MPI')
       call get(CompInfo, iComm=iComm, iProc=iProc, nProc=nProc, &
            nThread=nThread)
       call AMPS_SetMpiCommunicator(iComm, iProc, nProc, nThread)

    case('CHECK')
       ! AMPS could check now the input parameters for consistency
       if(UseBLine_C(PT_).and.DoCheck)then
          DoCheck = .false.   !To do this only once
          call get_time(tSimulationOut = PTTime)
          DataInputTime = PTTime
       end if
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
       if(UseBLine_C(PT_))then
          !
          ! Test version;  UnitX=1.0. If the choice of UnitX, which is the
          ! unit of coordinates in the BL coupler, is hardwired, UnitX
          ! should be set to rSun from
          !
          ! use ModConst, ONLY: rSun
          !
          ! Otherwise, it should be set to some value, to be read and
          ! provided by PT/AMPS
          !
          call BL_set_grid(TypeCoordSystem='HGR', UnitX=rSun)
       else
          nVar = 0
          NameVar = " "
          call amps_get_divu_status(iTrue)
          if (iTrue==1) then 
             nVar = 1
             NameVar = " divu"
          end if

          call amps_get_divudx_status(iTrue)
          if (iTrue==1) then
             nVar = nVar + 1
             NameVar = trim(NameVar)//" divudx"
          end if
          call set_coord_system(PT_, TypeCoord='HGI', nVar=nVar, NameVar=NameVar)
       end if
    case default
       call CON_stop(NameSub//': PT_ERROR: unknown TypeAction='//TypeAction)
    end select

  end subroutine PT_set_param
  !============================================================================

  subroutine PT_init_session(iSession, TimeSimulation)
    use CON_bline,  ONLY: BL_init, UseBLine_C, BL_get_origin_points
    !INPUT PARAMETERS:
    integer,  intent(in) :: iSession         ! session number (starting from 1)
    real,     intent(in) :: TimeSimulation   ! seconds from start time

    integer::code

    character(len=*), parameter:: NameSub = 'PT_init_session'
    !--------------------------------------------------------------------------
    if (DoTimeAccurate) then
       code=1
    else
       code=0
    end if
    if(UseBLine_C(PT_).and.DoInit)then
       DoInit = .false.   ! Do this only once
       !
       ! Initialize and connect to the data



      call amps_bl_nlon(nLon)
      call amps_bl_nlat(nLat)
      call amps_bl_rorigin(ROrigin);
      call amps_bl_lon_min_max(LonMin,LonMax)
      call amps_bl_lat_min_max(LatMin,LatMax)



       nullify(MHData_VIB); nullify(nVertex_B)
       call BL_init(nVertexMax, nLon, nLat,  &
            MHData_VIB, nVertex_B)
       call BL_get_origin_points(ROrigin, LonMin, LonMax, LatMin, LatMax)
    end if
    call amps_init_session(iSession,TimeSimulation,code)
  end subroutine PT_init_session
  !============================================================================

  subroutine PT_finalize(TimeSimulation)
    use CON_bline, ONLY: save_mhd
    !INPUT PARAMETERS:
    real,     intent(in) :: TimeSimulation   ! seconds from start time

    character(len=*), parameter:: NameSub = 'PT_finalize'
    !--------------------------------------------------------------------------
    if(DataInputTime > PTTime)then
       PTTime = DataInputTime
       call save_mhd(PTTime)
    end if
    call AMPS_finalize

  end subroutine PT_finalize
  !============================================================================

  subroutine PT_save_restart(TimeSimulation)
    use CON_bline, ONLY: save_mhd
    !INPUT PARAMETERS:
    real,     intent(in) :: TimeSimulation   ! seconds from start time

!!! PT should save restart files !!!

    character(len=*), parameter:: NameSub = 'PT_save_restart'
    !--------------------------------------------------------------------------
    if(DataInputTime > PTTime)then
       PTTime = DataInputTime
       call save_mhd(PTTime)
    end if
    call amps_save_restart()

  end subroutine PT_save_restart
  !============================================================================

  subroutine PT_run(TimeSimulation, TimeSimulationLimit)
    use CON_bline, ONLY:  nLine, NameVar_V, nMHData, UseBLine_C, BL_update_r
    use CON_bline, ONLY: save_mhd
    !INPUT/OUTPUT ARGUMENTS:
    real, intent(inout):: TimeSimulation   ! current time of component

    !INPUT ARGUMENTS:
    real, intent(in):: TimeSimulationLimit ! simulation time not to be exceeded

    real:: xEarth(3)
    character(len=*), parameter:: NameSub = 'PT_run'
    integer::ForceReachingSimulationTimeLimit=0 
    !--------------------------------------------------------------------------

    ! update the location of the Earth in the coupled on the AMPS side when
    ! coupling with IH as active
    if (IhCouplingCode==1) then
       call GetEarthLocation(xEarth)
       call set_earth_locaton_hgi(xEarth)
    end if
    !
    ! if UseBLine_C(PT_), available: DataInputTime, MHData_VIB, nVertex_B
    ! stub  for amps_get_bline is put to the end of the file
    if (UseBLine_C(PT_)) then
       call BL_update_r

       if (DataInputTime > PTTime) then
          PTTime = DataInputTime
    !     call save_mhd(PTTime)
       end if

       call amps_get_bline(&
         DataInputTime, & ! real, intent in
         nVertexMax   , & ! integer, intent in
         nLine        , & ! integer, intent in
         nVertex_B    , & ! integer, dimension(1:nLine), intent in
         nMHData      , & ! integer, intent it
         NameVar_V    , & ! character(len=10), dimension(0:nMHData) intent in
         MHData_VIB)! real,intent in,dimension(0:nMHData,1:nVertexMax,1:nLine)

       ForceReachingSimulationTimeLimit=1

!      TimeSimulation = TimeSimulationLimit  !For test purpose only
!      RETURN                                !For test purpose only
    end if

    ! actually filled in part is for second index ranging 1:nVertex_B(1:nLine)
    ! call AMPS
    call AMPS_timestep(TimeSimulation, TimeSimulationLimit,&
      ForceReachingSimulationTimeLimit)

  end subroutine PT_run
  !============================================================================

  subroutine PT_get_grid_info(nDimOut, iGridOut, iDecompOut)

    ! Provide information about AMPS grid. Set number of ion fluids.

    integer, intent(out):: nDimOut    ! grid dimensionality
    integer, intent(out):: iGridOut   ! grid index (increases with AMR)
    integer, intent(out):: iDecompOut ! decomposition index

    integer:: nVarCouple
    integer:: nCommunicatedFluids

    character(len=*), parameter:: NameSub = 'PT_get_grid_info'
    !--------------------------------------------------------------------------
    if(iCompSourceCouple /= PT_)then
       nVarCouple = Grid_C(iCompSourceCouple)%nVar
       ! write(*,*)'!!!', NameSub, i_proc(), ' nVarCouple=', nVarCouple

       IhCouplingCode=0
       OhCouplingCode=0
       ScCouplingCode=0

       nCommunicatedFluids=nVarCouple / 5

       if (Couple_CC(OH_,PT_)%DoThis) then
          OhCouplingCode=1
          Grid_C(PT_)%TypeCoord='HGI'
       end if

       if (Couple_CC(IH_,PT_)%DoThis) then
          IhCouplingCode=1
          Grid_C(PT_)%TypeCoord='HGI'
       end if

       if (Couple_CC(SC_,PT_)%DoThis) ScCouplingCode=1

       if (iCompSourceCouple == IH_) nCommunicatedFluids=1
       if (iCompSourceCouple == SC_) nCommunicatedFluids=1

       ! Pass number of fluids to this incorrectly named subroutine
       call amps_from_oh_init(nCommunicatedFluids, OhCouplingCode, &
            IhCouplingCode)
    end if

    nDimOut    = 3
    iGridOut   = 1
    iDecompOut = 1

    call amps_mesh_id(iDecompOut)

  end subroutine PT_get_grid_info
  !============================================================================

  subroutine PT_find_points(nDimIn, nPoint, Xyz_DI, iProc_I)

    integer, intent(in) :: nDimIn                ! dimension of position vector
    integer, intent(in) :: nPoint                ! number of positions
    real,    intent(in) :: Xyz_DI(nDimIn,nPoint) ! positions
    integer, intent(out):: iProc_I(nPoint)       ! processor owning position

    ! Find array of points and return processor indexes owning them
    ! Could be generalized to return multiple processors...

    real:: Xyz_D(3) = 0.0
    integer:: iPoint, iProcFound

    character(len=*), parameter:: NameSub = 'PT_find_points'
    !--------------------------------------------------------------------------
    do iPoint = 1, nPoint
       Xyz_D(:)=0.0
       Xyz_D(1:nDimIn) = Xyz_DI(:,iPoint)

       call amps_get_point_thread_number(iProcFound, Xyz_D)
       iProc_I(iPoint) = iProcFound

    end do

  end subroutine PT_find_points
  !============================================================================

  subroutine PT_put_from_gm( &
       NameVar, nVar, nPoint, Data_VI, iPoint_I, Pos_DI)

    character(len=*), intent(inout):: NameVar ! List of variables
    integer,          intent(inout):: nVar    ! Number of variables in Data_VI
    integer,          intent(inout):: nPoint  ! Number of points in Pos_DI
    real,    intent(in), optional:: Data_VI(:,:)    ! Recv data array
    integer, intent(in), optional:: iPoint_I(nPoint)! Order of data

    real, intent(out), optional, allocatable:: Pos_DI(:,:) ! Position vectors
    real::PTTime 

    character(len=*), parameter:: NameSub = 'PT_put_from_gm'
    !--------------------------------------------------------------------------
    if(present(Pos_DI))then
       ! set number of grid points on this processor
       call amps_get_center_point_number(nPoint)

       ! allocate position array
       allocate(Pos_DI(3,nPoint))

       ! get point positions from AMPS
       call amps_get_center_point_coordinates(Pos_DI)

    elseif(present(Data_VI))then
       call get_time(tSimulationOut = PTTime) 

       call amps_recieve_batsrus2amps_center_point_data(&
            NameVar//char(0), nVar, Data_VI, iPoint_I,PTTime)
    else
       call CON_stop(NameSub//': neither Pos_DI nor Data_VI are present!')
    end if

  end subroutine PT_put_from_gm
  !============================================================================

  subroutine PT_put_from_oh( &
       NameVar, nVar, nPoint, Data_VI, iPoint_I, Pos_DI)

    character(len=*), intent(inout):: NameVar ! List of variables
    integer,          intent(inout):: nVar    ! Number of variables in Data_VI
    integer,          intent(inout):: nPoint  ! Number of points in Pos_DI
    real,    intent(in), optional:: Data_VI(:,:)    ! Recv data array
    integer, intent(in), optional:: iPoint_I(nPoint)! Order of data
    integer::i,j,jj
    integer::DataBufferSize=0 

    real, intent(out), optional, allocatable:: Pos_DI(:,:) ! Position vectors
    real::PTTime

    character(len=*), parameter:: NameSub = 'PT_put_from_oh'
    !--------------------------------------------------------------------------

    DataBufferSize=0

    if(present(Pos_DI))then
       ! set number of grid points on this processor
       call amps_get_center_point_number_oh(nPoint)

       ! allocate position array
       allocate(Pos_DI(3,nPoint))

       ! get point positions from AMPS
       call amps_get_center_point_coordinates_oh(Pos_DI)

    elseif(present(Data_VI))then

       do i = 1,nVar
          do j=1,nPoint
             jj=iPoint_I(j)

             if (jj>0) then
                DataBufferSize=DataBufferSize+1

                if (ieee_is_nan(Data_VI(i,jj))) then
                   call CON_stop(NameSub//': nan')
                end if

                if (.not.ieee_is_finite(Data_VI(i,jj))) then
                   call CON_stop(NameSub//': not finite')
                end if
             end if
          end do
       end do


       call get_time(tSimulationOut = PTTime)

       call amps_recieve_batsrus2amps_center_point_data_oh(&
            NameVar//char(0), nVar, Data_VI, iPoint_I,PTTime)

       !call amps_recv_oh_checksum(Data_VI,DataBufferSize,nRecvFromOH)
    else
       call CON_stop(NameSub//': neither Pos_DI nor Data_VI are present!')
    end if

   nRecvFromOH=nRecvFromOH+1

  end subroutine PT_put_from_oh
  !============================================================================

  subroutine PT_put_from_ih( &
       NameVar, nVar, nPoint, Data_VI, iPoint_I, Pos_DI)

    character(len=*), intent(inout):: NameVar ! List of variables
    integer,          intent(inout):: nVar    ! Number of variables in Data_VI
    integer,          intent(inout):: nPoint  ! Number of points in Pos_DI
    real,    intent(in), optional:: Data_VI(:,:)    ! Recv data array
    integer, intent(in), optional:: iPoint_I(nPoint)! Order of data

    real, intent(out), optional, allocatable:: Pos_DI(:,:) ! Position vectors
    real::PTTime

    character(len=*), parameter:: NameSub = 'PT_put_from_ih'
    !--------------------------------------------------------------------------
    if(present(Pos_DI))then
       ! set number of grid points on this processor
       call amps_get_center_point_number_ih(nPoint)

       ! allocate position array
       allocate(Pos_DI(3,nPoint))

       ! get point positions from AMPS
       call amps_get_center_point_coordinates_ih(Pos_DI)

    elseif(present(Data_VI))then
       call get_time(tSimulationOut = PTTime)

       call amps_recieve_batsrus2amps_center_point_data_ih(&
            NameVar//char(0), nVar, Data_VI, iPoint_I,PTTime)
    else
       call CON_stop(NameSub//': neither Pos_DI nor Data_VI are present!')
    end if

  end subroutine PT_put_from_ih
  !============================================================================

  subroutine PT_put_from_sc(NameVar, nVar, nPoint, Data_VI, iPoint_I, Pos_DI)
    character(len=*), intent(inout):: NameVar ! List of variables
    integer,          intent(inout):: nVar    ! Number of variables in Data_VI
    integer,          intent(inout):: nPoint  ! Number of points in Pos_DI
    real,    intent(in), optional:: Data_VI(:,:)    ! Recv data array
    integer, intent(in), optional:: iPoint_I(nPoint)! Order of data

    real, intent(out), optional, allocatable:: Pos_DI(:,:) ! Position vectors
    real::PTTime

    character(len=*), parameter:: NameSub = 'PT_put_from_sc'
    !--------------------------------------------------------------------------
    if (present(Pos_DI)) then
       ! set number of grid points on this processor
       call amps_get_center_point_number_sc(nPoint)

       ! allocate position array
       allocate(Pos_DI(3,nPoint))

       ! get point positions from AMPS
       call amps_get_center_point_coordinates_sc(Pos_DI)
    elseif (present(Data_VI)) then
       call get_time(tSimulationOut = PTTime)

       call amps_recieve_batsrus2amps_center_point_data_sc(&
            NameVar//char(0), nVar, Data_VI, iPoint_I,PTTime)
    else
       call CON_stop(NameSub//': neither Pos_DI nor Data_VI are present!')
    end if
  end subroutine PT_put_from_sc
  !============================================================================
  subroutine PT_get_for_oh(IsNew, NameVar, nVarIn, nDimIn, nPoint, Xyz_DI, &
       Data_VI)

    ! Get data from PT to OH

    logical,          intent(in):: IsNew   ! true for new point array
    character(len=*), intent(in):: NameVar ! List of variables
    integer,          intent(in):: nVarIn  ! Number of variables in Data_VI
    integer,          intent(in):: nDimIn  ! Dimensionality of positions
    integer,          intent(in):: nPoint  ! Number of points in Xyz_DI

    real, intent(in) :: Xyz_DI(nDimIn,nPoint)  ! Position vectors
    real, intent(out):: Data_VI(nVarIn,nPoint) ! Data array

    integer::i,j

    character(len=*), parameter:: NameSub = 'PT_get_for_oh'
    !--------------------------------------------------------------------------
    call amps_send_batsrus2amps_center_point_data( &
         NameVar, nVarIn, nDimIn, nPoint, Xyz_DI, Data_VI)

    nSentToOH=nSentToOH+1

    do i = 1,nVarIn
       do j=1,nPoint
          if (ieee_is_nan(Data_VI(i,j))) then
             call CON_stop(NameSub//': nan')
          end if

          if (.not.ieee_is_finite(Data_VI(i,j))) then
             call CON_stop(NameSub//': not finite')
          end if
       end do
    end do

  end subroutine PT_get_for_oh
  !============================================================================
  subroutine PT_do_extract_lines(DoExtract)
    ! Interface routine to be called from super-structure on all PEs
    use CON_coupler, ONLY: i_proc0, i_comm, is_proc0
    use ModMpi
    logical, intent(out):: DoExtract

    integer :: iError

    ! when restarting, line data is available, i.e. ready to couple with mh;
    ! get value at SP root and broadcast to all SWMF processors

    ! The logical to control if trace the field lines originally
    ! (DoExtract=.true.)
    ! or not should be shaped on the root PE of the PT model and broadcast
    ! over all PEs of the SWMF

    character(len=*), parameter:: NameSub = 'PT_do_extract_lines'
    !--------------------------------------------------------------------------
    if(is_proc0(PT_)) DoExtract = .true.
    call MPI_Bcast(DoExtract, 1, MPI_LOGICAL, i_proc0(PT_), i_comm(), iError)
  end subroutine PT_do_extract_lines
  !============================================================================
  subroutine PT_put_coupling_param(Source_, TimeIn)
    use CON_bline,  ONLY: Lower_
    integer,        intent(in) :: Source_
    real,           intent(in) :: TimeIn
    !--------------------------------------------------------------------------
    if(DataInputTime >= TimeIn)RETURN
    ! New coupling time, get it and save old state
    DataInputTime = TimeIn
    if(Source_==Lower_)then
       ! Do what is needed with the MHD data about to be gone
       ! call do_something_with_MHData_VIB_array
       MHData_VIB(1:, :, :) = 0.0
    else
       call CON_stop("Time in IH-PT coupling differs from that in SC-PT")
    end if
  end subroutine PT_put_coupling_param
  !============================================================================
  ! Called from coupler after the updated grid point lo<cation are
  ! received from the other component (SC, IH). Determines whether some
  ! grid points should be added/deleted
  subroutine PT_adjust_lines(Source_)
    use CON_bline,          ONLY: &
         iOffset_B, BL_adjust_lines,  Lower_, Upper_, nLine
    integer, intent(in) :: Source_

    integer :: iLine  ! Loop variable

    character(len=*), parameter:: NameSub = 'PT_adjust_lines'
    !--------------------------------------------------------------------------
    call BL_adjust_lines(Source_)
    if(Source_ == Lower_)then
       do iLine = 1, nLine
          ! Offset the array, allocated at the lagrangian grid, if needed
          ! call offset(iBlock, iOffset=iOffset_B(iBlock))
       end do
    end if
    ! Called after the grid points are received from the
    ! component, nullify offset.
    if(Source_ == Upper_)iOffset_B(1:nLine) = 0
  end subroutine PT_adjust_lines
  !============================================================================
end module PT_wrapper
subroutine ConvertX_HGI_HGR(xHGR,xHGI)

  use  CON_axes

  real, intent(in) :: xHGI(3)  ! Position vectors
  real, intent(out) :: xHGR(3)  ! Position vectors

  !----------------------------------------------------------------------------
  xHGR=matmul(HgrHgi_DD,xHGI)

  !  xHGR=matmul(xHGI,HgiHgr_DD)
end subroutine ConvertX_HGI_HGR
subroutine ConvertX_HGR_HGI(xHGI,xHGR)
  use  CON_axes, ONLY:HgrHgi_DD

  implicit none

  real, intent(out) :: xHGI(3)  ! Position vectors
  real, intent(in) :: xHGR(3)  ! Position vectors

  !----------------------------------------------------------------------------
  xHGI=matmul(xHGR,HgrHgi_DD)
end subroutine ConvertX_HGR_HGI

subroutine ConvertVel_HGR_HGI(vHGI,vHGR,xHGR,TimeSim)
  use  CON_axes, ONLY:transform_velocity

  implicit none

  real, intent(out) :: vHGI(3) ! Position vector

  real, intent(in) :: xHGR(3)  ! Position vector
  real, intent(in) :: vHGR(3)  ! Velocity vector
  real, intent(in) :: TimeSim  ! Simulation time

  !----------------------------------------------------------------------------
  vHGI=transform_velocity(TimeSim,vHGR,xHGR,'HGR','HGI')
end subroutine ConvertVel_HGR_HGI
!==============================================================================
subroutine GetEarthLocation(xEarthHgi_D)

  use CON_axes, ONLY: XyzPlanetHgi_D
  use CON_time, ONLY: tSimulation

  implicit none

  real, intent(out):: xEarthHgi_D(3)

  ! Get the planet position in HGI
  !----------------------------------------------------------------------------
  xEarthHgi_D = XyzPlanetHgi_D

end subroutine GetEarthLocation
!=========Stub, to be removed, when the C routine is available=================
subroutine amps_get_bline(&
         DataInputTime, & ! real, intent in
         nVertexMax   , & ! integer, intent in
         nLine        , & ! integer, intent in
         nVertex_B    , & ! integer, dimension(1:nLine), intent in
         nMHData      , & ! integer, intent it
         NameVar_V    , & ! character(len=10), dimension(0:nMHData) intent in
         MHData_VIB)! real,intent in,dimension(0:nMHData,1:nVertexMax,1:nLine)
  use CON_coupler, ONLY:  PT_, is_proc0
  implicit none
  real,              intent(in) :: DataInputTime
  integer,           intent(in) :: nVertexMax
  integer,           intent(in) :: nLine
  integer,           intent(in) :: nVertex_B(1:nLine)
  integer,           intent(in) :: nMHData
  character(len=10), intent(in) :: NameVar_V(0:nMHData)
  real,              intent(in) :: MHData_VIB(0:nMHData, 1:nVertexMax, 1:nLine)
  !----------------------------------------------------------------------------

  integer :: ImportPointStep=1

  call get_bl_import_point_step(ImportPointStep)

  call amps_get_bline_c(DataInputTime,nVertexMax/ImportPointStep,nLine,nVertex_B(1:nLine)/ImportPointStep,&
    nMHData,NameVar_V,MHData_VIB(0:nMHData, 1:nVertexMax:ImportPointStep, 1:nLine))

  RETURN
end subroutine amps_get_bline
