!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf

module UA_wrapper

  ! Wrapper for GITM Upper Atmosphere (UA) component

  use ModUtilities, ONLY: CON_set_do_test, CON_stop

  implicit none

  private ! except

  public:: UA_set_param
  public:: UA_init_session
  public:: UA_run
  public:: UA_save_restart
  public:: UA_finalize

  ! Point coupler interface
  public:: UA_find_points
  public:: UA_get_grid_info

  ! UA-GM coupler
  public:: UA_get_for_gm

  ! IE Coupler (non-functional, for backward compatibility)
  public :: UA_get_info_for_ie
  public :: UA_get_for_ie
  public :: UA_put_from_ie

  ! real:: XyzTest_D(3) = [401037.03, 4071804.0, 402979.53]

contains
  !============================================================================
  subroutine UA_set_param(CompInfo, TypeAction)

    use ModInputs, ONLY: cInputText
    use ModReadParam, ONLY: read_text

    use ModTime, ONLY: StartTime, tSimulation, CurrentTime
    use ModInputs, ONLY: iStartTime, IsFramework, iOutputUnit_, set_defaults, &
         nInputLines
    use ModTimeConvert, ONLY: time_real_to_int
    use CON_physics, ONLY: get_time
    use ModIoUnit
    use ModProcUA
    use ModGITM, ONLY: iCommGITM, nProcs, iProcGITM => iProc
    use GITM_planet, ONLY: init_planet
    use CON_comp_info
    use ModUtilities, ONLY: check_dir

    ! Arguments
    type(CompInfoType), intent(inout):: CompInfo   ! Information for this comp.
    character(len=*), intent(in)     :: TypeAction ! What to do

    character(len=*), parameter:: NameSub = 'UA_set_param'
    !--------------------------------------------------------------------------
    select case(TypeAction)
    case('VERSION')
       call put(CompInfo, Use=.true., NameVersion='MGITM')
    case('MPI')
       call get(CompInfo, iComm=iComm, iProc=iProc, nProc=nProc)

       iCommGITM = iComm
       iProcGITM = iProc
       nProcs    = nProc

       if(iProc==0)then
          call check_dir("UA/DataIn")
          call check_dir("UA/data")
          call check_dir("UA/restartOUT")
       end if

       IsFramework = .true.

       call init_planet
       call set_defaults

    case('READ')
       call read_text(cInputText, nInputLines)
       nInputLines = nInputLines + 1
       cInputText(nInputLines) = "#END"

       call set_inputs

    case('STDOUT', 'FILEOUT', 'CHECK')
       ! Nothing to do?

    case('GRID')
       call UA_set_grid

    case default
       call CON_stop(NameSub//' UA_ERROR: invalid TypeAction='//TypeAction)

    end select

  end subroutine UA_set_param
  !============================================================================
  subroutine UA_set_grid

    use CON_Coupler
    use CON_comp_param, ONLY: UA_
    use ModInputs, ONLY: LatStart, LatEnd, LonStart, LonEnd, AltMin, AltMax
    use ModSizeGitm, ONLY: nLons, nLats, nAlts
    use ModInputs, ONLY: nBlocksLat, nBlocksLon
    use ModUtilities, ONLY: check_allocate
    use ModGeometry, ONLY: TypeGeometry

    character(len=*), parameter:: NameSub = 'UA_set_grid'
    !--------------------------------------------------------------------------
    if(done_dd_init(UA_))RETURN

    ! The 5 coupled variables are
    ! Temperature, N_CO2, N_O, EUVIonRate_O->O+, EUVIonRate_CO2->CO2+
    call set_grid_descriptor( &
         iComp  = UA_, &
         nDim = 3, &
         nRootBlock_D = [ 1, nBlocksLon, nBlocksLat ], &
         nCell_D = [ nAlts, nLons, nLats ], &
         XyzMin_D = [ AltMin, LonStart, LatStart ], &
         XyzMax_D = [ AltMax, LonEnd, LatEnd ], &
         TypeCoord = 'GEO', &
         IsPeriodic_D = [ .false., .true., .false. ], &
         nVar = nVarCouple)

  end subroutine UA_set_grid
  !============================================================================
  subroutine UA_init_session(iSession, TimeSimulation)

    use CON_physics, ONLY: get_time
    use ModTime, ONLY: StartTime, iTimeArray, CurrentTime

    real, intent(in)    :: TimeSimulation
    integer, intent(in) :: iSession

    logical :: IsUninitialized = .true.
    logical :: DoTest, DoTestMe

    character(len=*), parameter:: NameSub = 'UA_init_session'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)

    if(IsUninitialized)then
       ! Set time related variables for UA
       call get_time(tStartOut = StartTime)

       CurrentTime = StartTime + TimeSimulation
       call time_real_to_int(StartTime, iTimeArray)

       call fix_vernal_time

       call initialize_gitm(CurrentTime)
       call write_output

       IsUninitialized = .false.
    endif

    if(DoTest)write(*,*)NameSub,' finished for session ',iSession

  end subroutine UA_init_session
  !============================================================================
  subroutine UA_run(TimeSimulation, TimeSimulationLimit)

    use ModGITM, ONLY: iProc, Dt
    use ModTime, ONLY: StartTime, CurrentTime, EndTime, iStep
    use ModInputs, ONLY: Is1D
    use ModTimeConvert, ONLY: time_real_to_int, n_day_of_year

    real, intent(in)    :: TimeSimulationLimit ! Upper limit of simulation time
    real, intent(inout) :: TimeSimulation ! current time of component

    logical :: DoTest, DoTestMe

    character(len=*), parameter:: NameSub = 'UA_run'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)

    if(DoTest)write(*,*)NameSub,' called with tSim, tSimLimit, iProc=',&
         TimeSimulation, TimeSimulationLimit, iProc

    CurrentTime = StartTime + TimeSimulation
    EndTime     = StartTime + TimeSimulationLimit

    ! Why is calc_pressure here !!!
    call calc_pressure

    Dt = 1.e32

    call calc_timestep_vertical
    if (.not. Is1D) call calc_timestep_horizontal

    ! we should advance till EndTime with updating Dt every step
    call advance

    iStep = iStep + 1

    call write_output

    TimeSimulation = CurrentTime - StartTime

  end subroutine UA_run
  !============================================================================
  subroutine UA_save_restart(TimeSimulation)

    use ModInputs

    real, intent(in) :: TimeSimulation

    character(len=*), parameter:: NameSub = 'UA_save_restart'
    !--------------------------------------------------------------------------
    call write_restart("UA/restartOUT/")

  end subroutine UA_save_restart
  !============================================================================
  subroutine UA_finalize(TimeSimulation)

    real, intent(in) :: TimeSimulation

    character(len=*), parameter:: NameSub = 'UA_finalize'
    !--------------------------------------------------------------------------
    call finalize_gitm

  end subroutine UA_finalize
  !============================================================================
  subroutine UA_find_points(nDimIn, nPoint, Xyz_DI, iProc_I)

    use ModGitm, ONLY: iProc
    use GITM_planet, ONLY: rBody
    use GITM_location, ONLY: find_lonlat
    use ModCoordTransform, ONLY: xyz_to_lonlat

    integer, intent(in) :: nDimIn                ! dimension of positions
    integer, intent(in) :: nPoint                ! number of positions
    real,    intent(in) :: Xyz_DI(nDimIn,nPoint) ! positions
    integer, intent(out):: iProc_I(nPoint)       ! processor owning position

    ! Find array of points and return processor indexes owning them

    integer:: iPoint
    real :: Lon, Lat

    logical:: DoTest, DoTestMe, DoTestPoint
    character(len=*), parameter:: NameSub = 'UA_find_points'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)
    if(DoTest)write(*,*) NameSub,' iProc, nPoint=', iProc, nPoint

    do iPoint = 1, nPoint
       call xyz_to_lonlat(Xyz_DI(:,iPoint), Lon, Lat)

       ! Find processor obtaining Lon, Lat
       call find_lonlat(Lon, Lat, iProc_I(iPoint))

       ! if(sum(abs(Xyz_DI(:,iPoint) - XyzTest_D)) < 100.0) &
       !     write(*,*) NameSub,': iPoint, Lon, Lat, iProc_I=', &
       !     iPoint, Lon, Lat, iProc_I(iPoint)

       if(iProc_I(iPoint) < 0)then
          write(*,*) NameSub,' Xyz, Lon, Lat=', Xyz_DI(:,iPoint), Lon, Lat
          call CON_stop(NameSub//': point not found')
       end if
    end do

  end subroutine UA_find_points
  !============================================================================
  subroutine UA_get_grid_info(nDimOut, iGridOut, iDecompOut)

    use CON_time, ONLY: tSimulation
    use ModInputs, ONLY: Is1D, IsFullSphere

    integer, intent(out):: nDimOut    ! grid dimensionality
    integer, intent(out):: iGridOut   ! grid index
    integer, intent(out):: iDecompOut ! decomposition index

    ! Return basic grid information useful for model coupling.
    ! The decomposition index increases with load balance and AMR.

    real:: tSimulationLast = -1.0
    integer:: iDecomp = 0

    logical:: DoTest, DoTestMe
    character(len=*), parameter:: NameSub = 'UA_get_grid_info'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)

    if (Is1D) nDimOut = 1
    if (IsFullSphere) nDimOut = 3

    ! The GITM grid does not change
    iGridOut   = 1

    ! The UA grid is rotating relative to the GM grid, so in effect it changes
    if(tSimulation > tSimulationLast)then
       tSimulationLast = tSimulation
       iDecomp = modulo(iDecomp, 10) + 1
    end if
    iDecompOut = iDecomp
    if(DoTestMe)write(*,*) NameSub,' tSim, iDecomp=', tSimulation, iDecomp

  end subroutine UA_get_grid_info
  !============================================================================
  subroutine UA_get_for_gm(IsNew, NameVar, nVarIn, nDimIn, nPoint, Xyz_DI, &
       Data_VI)

    ! Interpolate Data_VI from UA at the list of positions Xyz_DI
    ! required by GM

    use ModGITM, ONLY: iProc, Temperature, NDensityS, Altitude_GB, TempUnit
    use ModSizeGitm, ONLY: nLons, nLats, nAlts, nBlocks
    use ModEUV, ONLY: EuvIonRateS
    use ModInterpolate, ONLY: bilinear, trilinear
    use ModCoordTransform, ONLY: xyz_to_rlonlat
    use GITM_location, ONLY: LocationIndex, LocationProcIndex
    use GITM_planet, ONLY: rBody, Gravitational_Constant, &
         iCO2_, iO_, iCO2P_, iOP_
    use ModConst, ONLY: cBoltzmann, cProtonMass

    logical,          intent(in) :: IsNew   ! true for new point array
    character(len=*), intent(in) :: NameVar ! List of variables
    integer,          intent(in) :: nVarIn  ! Number of variables in Data_VI
    integer,          intent(in) :: nDimIn  ! Dimensionality of positions
    integer,          intent(in) :: nPoint  ! Number of points in Xyz_DI

    real, intent(in) :: Xyz_DI(nDimIn,nPoint)  ! Position vectors
    real, intent(out):: Data_VI(nVarIn,nPoint) ! Data array

    real:: Dist_D(3)
    integer:: iCell_D(3)

    integer, allocatable, save:: iBlockCell_DI(:,:)
    real,    allocatable, save:: Dist_DI(:,:)
    real, allocatable, save:: Tneu_GB(:,:,:,:)

    integer:: iPoint, iLat, iLon, iAlt, iBlock, iProcFound
    real:: rAlt, rLon, rLat
    real:: Alt, Lon, Lat
    real:: rLonLat_D(3)

    real :: Grav, dH, HscaleCO2, HscaleO, AltMaxDomain, Tnu
    real, parameter:: NuMassCo2 = 44, NuMassO = 16
    real, parameter:: Tiny = 1e-12

    logical:: DoTest, DoTestMe ! , DoTestCell

    character(len=*), parameter:: NameSub = 'UA_get_for_gm'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)

    ! Altitude of the last grid cell (assume that iBlock=1 exists).
    AltMaxDomain = Altitude_GB(1,1,nAlts,1)
    ! Grav is the gravitational acceleration at the top of the domain
    ! used to calculate scale height.
    ! Gravitational_Constant is the acceleration at the surface
    Grav = Gravitational_Constant/(1 + AltMaxDomain/rBody)**2

    if(IsNew)then
       if(DoTest) write(*,*) NameSub,': iProc, nPoint=', iProc, nPoint
       if(DoTestMe)write(*,*) NameSub,': AltMaxDomain, Grav=', &
            AltMaxDomain, Grav

       if(allocated(iBlockCell_DI)) deallocate(iBlockCell_DI, Dist_DI)
       allocate(iBlockCell_DI(0:nDimIn,nPoint), Dist_DI(nDimIn,nPoint))

       do iPoint = 1, nPoint

          ! DoTestCell = sum(abs(Xyz_DI(:,iPoint) - XyzTest_D)) < 100.0
          ! if(DoTestCell)write(*,*) NameSub,': iProc, iPoint=', iProc, iPoint

          call xyz_to_rlonlat(Xyz_DI(:,iPoint), rLonLat_D)

          Alt = rLonLat_D(1) - rBody
          Lon = rLonLat_D(2)
          Lat = rLonLat_D(3)

          ! if(DoTestCell)write(*,*) NameSub,': rBody, Alt, Lon, Lat=', &
          !     rBody, Alt, Lon, Lat

          if(Alt > AltMaxDomain)then
             call LocationIndex(Lon, Lat, iBlock, iLon, iLat, rLon, rLat)
             if(iBlock < 1)then
                write(*,*) NameSub,' ERROR: Xyz_D, iProcFound=', &
                     Xyz_DI(:,iPoint)
                call CON_stop(NameSub//' could not find 2D position')
             end if

             ! Store block and cell indexes and distances for extrapolation
             iBlockCell_DI(0,iPoint)      = iBlock
             iBlockCell_DI(1:nDimIn,iPoint) = [ iLon, iLat, nAlts ]
             Dist_DI(:,iPoint)            = [ 1.0-rLon, 1.0-rLat, 0.0 ]

             ! if(DoTestCell) write(*,*) NameSub, &
             !     ': iBlock, iLon, iLat, nAlts=', iBlock, iLon, iLat, nAlts
          else
             call LocationProcIndex(Lon, Lat, Alt, &
                  iBlock, iLon, iLat, iAlt, rLon, rLat, rAlt, iProcFound)

             if(iProcFound /= iProc)then
                write(*,*) NameSub,' ERROR: Xyz_D, iProcFound=', &
                     Xyz_DI(:,iPoint), iProcFound
                call CON_stop(NameSub//' could not find 3D position')
             end if

             ! Store block and cell indexes and distances for interpolation
             iBlockCell_DI(0,iPoint)      = iBlock
             iBlockCell_DI(1:nDimIn,iPoint) = [ iLon, iLat, iAlt ]
             Dist_DI(:,iPoint)            = [ 1.0-rLon, 1.0-rLat, 1.0-rAlt ]

             ! if(DoTestCell) write(*,*) NameSub, &
             !     ': iBlock, iLon, iLat, iAlt=', iBlock, iLon, iLat, iAlt
          end if
       end do
    end if

    ! Calculate neutral temperature in [K]
    if(.not.allocated(Tneu_GB)) &
         allocate(Tneu_GB(-1:nLons+2,-1:nLats+2,-1:nAlts+2,nBlocks))
    do iBlock = 1, nBlocks
       call calc_rates(iBlock)
       Tneu_GB(:,:,:,iBlock) = Temperature(:,:,:,iBlock)*TempUnit
    end do

    do iPoint = 1, nPoint
       ! Use stored block and cell indexes and distances
       iBlock            = iBlockCell_DI(0,iPoint)
       iCell_D(1:nDimIn) = iBlockCell_DI(1:nDimIn,iPoint)
       Dist_D(1:nDimIn)  = Dist_DI(:,iPoint)

       ! We could use info in iCell_D and Dist_D instead of
       ! calculating Alt etc.
       Alt  = sqrt(sum(Xyz_DI(:,iPoint)**2)) - rBody

       ! DoTestCell = sum(abs(Xyz_DI(:,iPoint) - XyzTest_D)) < 100.0
       ! if(DoTestCell) write(*,*) NameSub, &
       !    ' iProc, iPoint, iBlock, iCell_D=', iProc, iPoint, iBlock, iCell_D

       if(Alt > AltMaxDomain)then
          ! Extrapolate using isothermal stratified atmosphere

          ! Neutral temperature, does not depend on height
          Tnu = bilinear(Tneu_GB(:,:,nAlts,iBlock), -1, nLons+2, -1, nLats+2, &
               DoExtrapolate=.false., iCell_D=iCell_D(:2), Dist_D=Dist_D(:2))

          ! Scale heights for CO2 and O in m
          HscaleCO2 = cBoltzmann*Tnu/(Grav*cProtonMass*NuMassCo2)
          HscaleO   = cBoltzmann*Tnu/(Grav*cProtonMass*NuMassO)

          Data_VI(1,iPoint) = Tnu

          ! Distance from top of domain in m
          dH = Alt - AltMaxDomain

          ! N_CO2
          Data_VI(2,iPoint) = &
               bilinear(NDensityS(:,:,nAlts,iCO2_,iBlock), &
               -1, nLons+2, -1, nLats+2, &
               DoExtrapolate=.false., iCell_D=iCell_D(:2), Dist_D=Dist_D(:2)) &
               *exp(-dH/HscaleCO2)

          ! if(DoTestCell)write(*,*) NameSub, &
          !     ': NCO2, NCO2(iCell), dH, HscaleCO2=', Data_VI(2,iPoint), &
          !     NDensityS(iCell_D(1), iCell_D(2), nAlts,iCO2_,iBlock), &
          !     dH, HscaleCO2

          ! N_O
          Data_VI(3,iPoint) = bilinear(NDensityS(:,:,nAlts,iO_,iBlock),&
               -1, nLons+2, -1, nLats+2, &
               DoExtrapolate=.false., iCell_D=iCell_D(:2), Dist_D=Dist_D(:2)) &
               *exp(-dH/HscaleO)

          ! EUVIonRate_CO2->CO2+
          Data_VI(4,iPoint) = max(Tiny, &
               bilinear(EuvIonRateS(:,:,nAlts,iCO2P_,iBlock), &
               -1, nLons+2, -1, nLats+2, &
               DoExtrapolate=.true., iCell_D=iCell_D(:2), Dist_D=Dist_D(:2)))

          ! EUVIonRate_O->O+
          Data_VI(5,iPoint) = max(Tiny, &
	       bilinear(EuvIonRateS(:,:,nAlts,iOP_,iBlock), &
               -1, nLons+2, -1, nLats+2, &
	       DoExtrapolate=.true., iCell_D=iCell_D(:2), Dist_D=Dist_D(:2)))

       else
          ! Neutral temperature
          Data_VI(1,iPoint) = &
               trilinear(Tneu_GB(:,:,:,iBlock), -1, nLons+2, -1, nLats+2, -1, nAlts+2, &
               DoExtrapolate=.false., iCell_D=iCell_D, Dist_D=Dist_D)

          ! N_CO2
          Data_VI(2,iPoint) = trilinear(NDensityS(:,:,:,iCO2_,iBlock), &
               -1, nLons+2, -1, nLats+2, -1, nAlts+2, &
               DoExtrapolate=.false., iCell_D=iCell_D, Dist_D=Dist_D)

          ! N_O
          Data_VI(3,iPoint) = trilinear(NDensityS(:,:,:,iO_,iBlock), &
               -1, nLons+2, -1, nLats+2, -1, nAlts+2, &
               DoExtrapolate=.false., iCell_D=iCell_D, Dist_D=Dist_D)

          ! EUVIonRate_CO2->CO2+
          Data_VI(4,iPoint) = &
               trilinear(EuvIonRateS(:,:,:,iCO2P_,iBlock),&
               -1, nLons+2, -1, nLats+2, 1, nAlts, &
               DoExtrapolate=.true., iCell_D=iCell_D, Dist_D=Dist_D)

          ! EUVIonRate_O->O+
          Data_VI(5,iPoint) = &
               trilinear(EuvIonRateS(:,:,:,iOP_,iBlock), &
               -1, nLons+2, -1, nLats+2, 1, nAlts, &
               DoExtrapolate=.true., iCell_D=iCell_D, Dist_D=Dist_D)
       end if
    end do

  end subroutine UA_get_for_gm
  !============================================================================
  subroutine UA_get_info_for_ie(nVar, nVarSpec, nEngUA, NameVar_V, &
                                NameVarSpec_V, nMagLat, nMagLon, EngUA)

    integer, intent(out) :: nVar, nVarSpec, nEngUA
    integer, intent(out), optional :: nMagLat, nMagLon
    real, intent(out), optional :: EngUA(:)
    character(len=*), intent(out), optional :: NameVar_V(:), NameVarSpec_V(:)

    character(len=*), parameter :: NameSub='UA_get_info_for_ie'

    call CON_stop(NameSub//': UA_ERROR: empty version cannot be used!')

  end subroutine UA_get_info_for_ie
  !============================================================================
  subroutine UA_put_from_ie(Buffer_IIV, iSizeIn, jSizeIn, nVarIn, &
                            NameVarIn_V, nVarSpecIn, Buffer_IIIV, &
                            NameVarSpecIn_V)

    integer, intent(in)           :: iSizeIn, jSizeIn, nVarIn
    real, intent(in)              :: Buffer_IIV(iSizeIn, jSizeIn, nVarIn)
    character(len=*), intent(in)  :: NameVarIn_V(nVarIn)
    integer, intent(in) :: nVarSpecIn
    real, intent(in), optional    :: Buffer_IIIV(iSizeIn, jSizeIn, 0, &
                                                nVarSpecIn)
    character(len=*), intent(in), optional :: NameVarSpecIn_V(nVarSpecIn)

    character(len=*), parameter:: NameSub = 'UA_put_from_ie'
    !--------------------------------------------------------------------------
    call CON_stop(NameSub//': UA_ERROR: empty version cannot be used!')

  end subroutine UA_put_from_ie
  !============================================================================
  subroutine UA_get_for_ie(BufferOut_IIBV, nMltIn, nLatIn, nVarIn, NameVarIn_V)

    integer,          intent(in) :: nMltIn, nLatIn, nVarIn
    character(len=3), intent(in) :: NameVarIn_V(nVarIn)

    real, intent(out) :: BufferOut_IIBV(nMltIn, nLatIn, 2, nVarIn)

    character(len=*), parameter:: NameSub = 'UA_get_for_ie'
    !--------------------------------------------------------------------------
    call CON_stop(NameSub//': UA_ERROR: empty version cannot be used!')

  end subroutine UA_get_for_ie
  !============================================================================
end module UA_wrapper
!==============================================================================
