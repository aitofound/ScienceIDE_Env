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
  
contains

  !============================================================================
  subroutine UA_set_param(CompInfo, TypeAction)

    use ModInputs, only: cInputText
    use ModReadParam, only: read_text, n_line_read

    use ModTime, ONLY: StartTime, tSimulation, CurrentTime
    use ModInputs, only: iStartTime, IsFramework, iOutputUnit_, set_defaults, &
         nInputLines
    use ModTimeConvert, ONLY: time_real_to_int
    use CON_physics,    ONLY: get_time
    use ModIoUnit
    use ModProcUA
    use ModGITM, only: iCommGITM, nProcs, iProcGITM => iProc
    use ModPlanet, only: init_planet
    use CON_comp_info
    use ModUtilities, ONLY: check_dir

    ! Arguments
    type(CompInfoType), intent(inout):: CompInfo   ! Information for this comp.
    character(len=*), intent(in)     :: TypeAction ! What to do

    character(len=*), parameter :: NameSub = 'UA_set_param'
    !-------------------------------------------------------------------------
    write(*,*) "-->Starting UA_set_param..."
    select case(TypeAction)
    case('VERSION')
       call put(CompInfo,&
            Use=.true.,                                      &
            NameVersion='Global Iono-Thermo Model (Ridley)', &
            Version=2.0)
    case('MPI')
       call get(CompInfo, iComm=iComm, iProc=iProc, nProc=nProc)

       iCommGITM = iComm
       iProcGITM = iProc
       nProcs    = nProc

       if(iProc==0)then
          call check_dir("UA/DataIn")
          call check_dir("UA/data")
          call check_dir("UA/RestartOUT")
       end if

       IsFramework = .true.

       call init_planet
       call set_defaults

    case('READ')
       call read_text(cInputText)
       cInputText(n_line_read()+1) = "#END"
       nInputLines=n_line_read()+1

       call set_inputs

    case('FILEOUT')
       call get(CompInfo,iUnitOut=iOutputUnit_)

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
    use ModInputs,    ONLY: LatStart, LatEnd, LonStart, LonEnd, AltMin, AltMax
    use ModSizeGitm,  ONLY: nLons, nLats, nAlts
    use ModInputs,    ONLY: nBlocksLat, nBlocksLon
    use ModUtilities, ONLY: check_allocate
    use ModGeometry,  ONLY: TypeGeometry

    character(len=*), parameter :: NameSub='UA_set_grid'
    !--------------------------------------------------------------------------

    if(done_dd_init(UA_))RETURN
    
    ! The 5 coupled variables are
    ! Temperature, N_CO2, N_O, EUVIonRate_O->O+, EUVIonRate_CO2->CO2+
    call set_grid_descriptor( &
         iComp  = UA_, &
         nDim = 3, &
         nRootBlock_D = (/ 1, nBlocksLon, nBlocksLat /), &
         nCell_D = (/ nAlts, nLons, nLats /), &
         XyzMin_D = (/ AltMin, LonStart, LatStart /), &
         XyzMax_D = (/ AltMax, LonEnd, LatEnd /), &
         TypeCoord = 'GEO', &
         IsPeriodic_D = (/ .false., .true., .false. /), &
         nVar = nVarCouple)

  end subroutine UA_set_grid

  !============================================================================

  subroutine UA_init_session(iSession, TimeSimulation)

    use CON_physics,    ONLY: get_time
    use ModTime, only : StartTime, iTimeArray, CurrentTime

    real, intent(in)    :: TimeSimulation
    integer, intent(in) :: iSession

    logical :: IsUninitialized = .true.
    logical :: DoTest, DoTestMe

    character(len=*), parameter :: NameSub='UA_init_session'
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

    use ModGITM,   ONLY: iProc, Dt
    use ModTime,   ONLY: StartTime, CurrentTime, EndTime, iStep
    use ModInputs, ONLY: Is1D
    use ModTimeConvert, ONLY: time_real_to_int, n_day_of_year

    real, intent(in)    :: TimeSimulationLimit ! Upper limit of simulation time
    real, intent(inout) :: TimeSimulation ! current time of component

    logical :: DoTest, DoTestMe
    
    character(len=*), parameter :: NameSub='UA_run'
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

  !==========================================================================

  subroutine UA_save_restart(TimeSimulation)

    use ModInputs

    real, intent(in) :: TimeSimulation

    character(len=*), parameter :: NameSub='UA_save_restart'
    !--------------------------------------------------------------------------
    call write_restart("UA/restartOUT/")

  end subroutine UA_save_restart

  !==========================================================================

  subroutine UA_finalize(TimeSimulation)

    real, intent(in) :: TimeSimulation

    character(len=*), parameter :: NameSub='UA_finalize'
    !--------------------------------------------------------------------------
    call finalize_gitm

  end subroutine UA_finalize

  !============================================================================
  subroutine UA_find_points(nDimIn, nPoint, Xyz_DI, iProc_I)

    use ModPlanet, ONLY: rBody
    use ModCoordTransform, ONLY: xyz_to_rlonlat

    integer, intent(in) :: nDimIn                ! dimension of positions
    integer, intent(in) :: nPoint                ! number of positions
    real,    intent(in) :: Xyz_DI(nDimIn,nPoint) ! positions
    integer, intent(out):: iProc_I(nPoint)       ! processor owning position

    ! Find array of points and return processor indexes owning them
    ! Could be generalized to return multiple processors...

    integer:: iPoint
    integer :: iiLat, iiLon, iiAlt, iiBlock, iAlt
    real :: rLon, rLat, rAlt
    real :: rLonLat_D(3), Alt

    character(len=*), parameter:: NameSub = 'UA_find_points'
    !--------------------------------------------------------------------------
    do iPoint = 1, nPoint
       call xyz_to_rlonlat(Xyz_DI(:,iPoint), rLonLat_D)
       Alt = rLonLat_D(1) - rBody

       call LocationProcIndex(rLonLat_D(2), rLonLat_D(3), Alt, &
            iiBlock, iiLon, iiLat, iAlt, rLon, rLat, rAlt, iProc_I(iPoint))
    end do

  end subroutine UA_find_points
  !============================================================================
  subroutine UA_get_grid_info(nDimOut, iGridOut, iDecompOut)

    use ModInputs, ONLY: Is1D,IsFullSphere

    integer, intent(out):: nDimOut    ! grid dimensionality
    integer, intent(out):: iGridOut   ! grid index
    integer, intent(out):: iDecompOut ! decomposition index

    character(len=*), parameter :: NameSub = 'UA_get_grid_info'

    ! Return basic grid information useful for model coupling.
    ! The decomposition index increases with load balance and AMR.
    !--------------------------------------------------------------------------
    if (Is1D) nDimOut = 1
    if (IsFullSphere) nDimOut = 3

    ! The GITM grid does not change
    iGridOut   = 1
    iDecompOut = 1

  end subroutine UA_get_grid_info
  !============================================================================
  subroutine UA_get_for_gm(IsNew, NameVar, nVarIn, nDimIn, nPoint, Xyz_DI, &
       Data_VI)

    ! Interpolate Data_VI from UA at the list of positions Xyz_DI
    ! required by GM

    use ModInputs, ONLY: AltMin, AltMax
    use ModGITM,  ONLY: iProc, Temperature, NDensityS
    use ModSizeGitm, ONLY: nLons, nLats, nAlts
    use ModEUV, ONLY: EuvIonRateS
    use ModInterpolateScalar, ONLY: bilinear, trilinear
    use ModCoordTransform, ONLY: xyz_to_rlonlat
    use ModPlanet, ONLY: rBody, iCO2_, iO_, iCO2P_, iOP_
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

    integer:: iPoint, iBlock, iProcFound
    integer :: iiLat, iiLon, iAlt, iiBlock
    real :: rAlt, rLon, rLat
    real :: Alt, Lon, Lat
    real :: rLonLat_D(3)

    real :: grav, dH, Hscale, HCO2, HO, AltMaxDomain, Tnu
    real, parameter :: NuMassCo2 = 44, NuMassO = 16
    real, parameter :: Tiny = 1e-12
    
    logical:: DoTest, DoTestMe

    character(len=*), parameter :: NameSub='UA_get_for_gm'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)

    if(IsNew)then
       if(DoTest)write(*,*) NameSub,': iProc, nPoint=', iProc, nPoint

       if(allocated(iBlockCell_DI)) deallocate(iBlockCell_DI, Dist_DI)
       allocate(iBlockCell_DI(0:nDimIn,nPoint), Dist_DI(nDimIn,nPoint))

       AltMaxDomain = AltMin + (nAlts-0.5)*(AltMax-AltMin)/nAlts
       
       ! grav=3.72/r_GB(i,j,k,iBlock)/r_GB(i,j,k,iBlock)         
       grav = 3.72/(1.0+AltMaxDomain/3396.0)/(1.0+AltMaxDomain/3396.0)
       
       do iPoint = 1, nPoint
          call xyz_to_rlonlat(Xyz_DI(:,iPoint), rLonLat_D)
          Alt = rLonLat_D(1) - rBody
          Lon = rLonLat_D(2)
          Lat = rLonLat_D(3)

          if(Alt > AltMaxDomain)then
             call LocationIndex(Lon, Lat, iiBlock, iiLon, iiLat, rLon, rLat)
             
             ! Store block and cell indexes and distances for extrapolation
             iBlockCell_DI(0,iPoint)      = iBlock
             iBlockCell_DI(1:nDimIn,iPoint) = (/ iiLon, iiLat, nAlts /)
             Dist_DI(:,iPoint)            = (/ 1.0-rLon, 1.0-rLat, 0.0 /)
          else
             call LocationProcIndex(Lon, Lat, Alt, &
                  iiBlock, iiLon, iiLat, iAlt, rLon, rLat, rAlt, iProcFound)

             if(iProcFound /= iProc)then
                write(*,*) NameSub,' ERROR: Xyz_D, iProcFound=', &
                     Xyz_DI(:,iPoint), iProcFound
                call CON_stop(NameSub//' could not find position on this proc')
             end if

             ! Store block and cell indexes and distances for interpolation
             iBlockCell_DI(0,iPoint)      = iBlock
             iBlockCell_DI(1:nDimIn,iPoint) = (/ iiLon, iiLat, iAlt /)
             Dist_DI(:,iPoint)            = (/ 1.0-rLon, 1.0-rLat, 1.0-rAlt /)
          end if
       end do
    end if

    do iPoint = 1, nPoint
       ! Use stored block and cell indexes and distances
       iBlock            = iBlockCell_DI(0,iPoint)
       iCell_D(1:nDimIn) = iBlockCell_DI(1:nDimIn,iPoint)
       Dist_D(1:nDimIn)  = Dist_DI(:,iPoint)

       Alt  = sqrt(sum(Xyz_DI(:,iPoint)**2)) - rBody
       
       if(Alt > AltMaxDomain)then
          ! Extrapolate using isothermal stratified atmosphere

          ! Neutral temperature, does not depend on height
          Tnu = bilinear(Temperature(:,:,nAlts,iBlock), &
               -1, nLons+2, -1, nLats+2, &
               DoExtrapolate=.false., iCell_D=iCell_D(:2), Dist_D=Dist_D(:2))

          Data_VI(1,iPoint) = Tnu

          dH = Alt - AltMaxDomain
          
          Hscale = cBoltzmann*Tnu/grav/cProtonMass ! in m unit

          HCO2 = Hscale/NuMassCo2/1e3
          HO   = Hscale/NuMassO/1e3

          ! N_CO2
          Data_VI(2,iPoint) = &
               bilinear(NDensityS(:,:,nAlts,iCO2_,iBlock), &
               -1, nLons+2, -1, nLats+2, &
               DoExtrapolate=.false., iCell_D=iCell_D(:2), Dist_D=Dist_D(:2)) &
               *exp(-dH/HCO2)

          ! N_O
          Data_VI(3,iPoint) = bilinear(NDensityS(:,:,nAlts,iO_,iBlock),&
               -1, nLons+2, -1, nLats+2, &
               DoExtrapolate=.false., iCell_D=iCell_D(:2), Dist_D=Dist_D(:2)) &
               *exp(-dH/HO)

          ! EUVIonRate_CO2->CO2+
          Data_VI(4,iPoint) = &
               max(bilinear(EuvIonRateS(:,:,nAlts,iCO2P_,iBlock),&
               1, nLons, 1, nLats, &
               DoExtrapolate=.true., iCell_D=iCell_D(:2), Dist_D=Dist_D(:2)), &
               Tiny)

          ! EUVIonRate_O->O+
          Data_VI(5,iPoint) = &
	       max(bilinear(EuvIonRateS(:,:,nAlts,iOP_,iBlock), &
               1, nLons, 1, nLats, &
	       DoExtrapolate=.true., iCell_D=iCell_D(:2), Dist_D=Dist_D(:2)), &
               Tiny)
       else
          ! Neutral temperature
          Data_VI(1,iPoint) = trilinear(Temperature(:,:,:,iBlock), &
               -1, nLons+2, -1, nLats+2, -1, nAlts+2, &
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
               1, nLons, 1, nLats, 1, nAlts, &
               DoExtrapolate=.true., iCell_D=iCell_D, Dist_D=Dist_D)

          ! EUVIonRate_O->O+
          Data_VI(5,iPoint) = &
               trilinear(EuvIonRateS(:,:,:,iOP_,iBlock), &
               1, nLons, 1, nLats, 1, nAlts, &
               DoExtrapolate=.true., iCell_D=iCell_D, Dist_D=Dist_D)
       end if
    end do

  end subroutine UA_get_for_gm

  !============================================================================
  subroutine UA_get_info_for_ie(nVar, NameVar_V, nMagLat, nMagLon)

    !OUTPUT ARGUMENTS:
    integer, intent(out) :: nVar
    integer, intent(out), optional :: nMagLat, nMagLon
    character(len=*), intent(out), optional :: NameVar_V(:)

    character(len=*), parameter :: NameSub='UA_get_info_for_ie'

    call CON_stop(NameSub//': UA_ERROR: empty version cannot be used!')

  end subroutine UA_get_info_for_ie

  !============================================================================
  subroutine UA_put_from_ie(Buffer_IIV, iSizeIn, jSizeIn, nVarIn, &
       NameVarIn_V, iBlock)

    !INPUT/OUTPUT ARGUMENTS:
    integer, intent(in)           :: iSizeIn, jSizeIn, nVarIn, iBlock
    real, intent(in)              :: Buffer_IIV(iSizeIn,jSizeIn,nVarIn)
    character (len=*),intent(in)  :: NameVarIn_V(nVarIn)

    character (len=*), parameter :: NameSub='UA_put_from_ie'

    call CON_stop(NameSub//': UA_ERROR: empty version cannot be used!')

  end subroutine UA_put_from_ie
  !============================================================================
  subroutine UA_get_for_ie(BufferOut_IIBV, nMltIn, nLatIn, nVarIn, NameVarIn_V)

    ! INPUT ARGUMENTS:
    integer,          intent(in) :: nMltIn, nLatIn, nVarIn
    character(len=3), intent(in) :: NameVarIn_V(nVarIn)

    ! OUTPUT ARGUMENTS:
    real, intent(out) :: BufferOut_IIBV(nMltIn, nLatIn, 2, nVarIn)

    character (len=*), parameter :: NameSub='UA_get_for_ie'

    call CON_stop(NameSub//': UA_ERROR: empty version cannot be used!')

  end subroutine UA_get_for_ie


end module UA_wrapper
