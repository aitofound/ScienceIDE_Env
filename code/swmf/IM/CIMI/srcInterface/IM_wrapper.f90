module IM_wrapper

  ! Wrapper for CIMI Internal Magnetosphere (IM) component

  use ModUtilities, ONLY: CON_set_do_test, CON_stop

  implicit none

  private ! except

  public:: IM_set_param
  public:: IM_init_session
  public:: IM_run
  public:: IM_save_restart
  public:: IM_finalize

  ! Coupling with IE
  public:: IM_get_info_for_ie
  public:: IM_get_for_ie
  public:: IM_get_for_ie_mpi
  public:: IM_put_from_ie_mpi
  public:: IM_put_from_ie
  public:: IM_put_from_ie_complete

  ! Coupling with GM
  public:: IM_get_for_gm
  public:: IM_put_from_gm
  public:: IM_put_from_gm_line
  public:: IM_put_from_gm_crcm
  public:: IM_put_sat_from_gm

contains
  !============================================================================
  subroutine IM_set_param(CompInfo, TypeAction)

    ! Set CIMI parameters from IM section of PARAM.in

    use CON_comp_info
    use ModUtilities
    use ModReadParam
    use CON_coupler, ONLY: Couple_CC, GM_, IM_, IE_
    use ModGmCimi,  ONLY: UseGm
    use ModIeCimi,  ONLY: UseIE
    use ModCimiGrid, ONLY: iProc,nProc,iComm
    use ModCimi,    ONLY: DtCalcPrecip

    character (len=*), intent(in)     :: TypeAction ! which action to perform
    type(CompInfoType), intent(inout) :: CompInfo   ! component information

    logical:: DoTest, DoTestMe
    character(len=*), parameter:: NameSub = 'IM_set_param'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)

    UseGm = Couple_CC(GM_, IM_) % DoThis
    UseIE = Couple_CC(IE_, IM_) % DoThis

    if(DoTestMe) write(*,*) NameSub,' TypeAction= ',TypeAction

    select case(TypeAction)
    case('VERSION')
       call put(CompInfo,                                     &
            Use=.true.,                                       &
            NameVersion='CIMI, M. Fok and A. Glocer', &
            Version=1.0)
    case('MPI')
       call get(CompInfo, iComm=iComm, iProc=iProc, nProc=nProc)
    case('STDOUT')
       ! iUnitOut=STDOUT_
       ! StringPrefix='RB:'
    case('FILEOUT')
       ! call get(CompInfo,iUnitOut=iUnitOut)
       ! StringPrefix=''
    case('READ')
       call CIMI_set_parameters('READ')
    case('CHECK')
       if (Couple_CC(IM_, IE_) % Dothis) then
         if (Couple_CC(IM_, IE_) % Dt < DtCalcPrecip) then
            DtCalcPrecip = Couple_CC(IM_, IE_) % Dt
         end if
       end if
       ! We should check and correct parameters here
       ! if(iProc==0)then
       !   call IM_write_prefix;  write(iUnitOut,*)&
       !        NameSub,': CHECK iSession =',i_session_read()
       ! end if
    case('GRID')
       call IM_set_grid
    case default
       call CON_stop(NameSub//' IM_ERROR: invalid TypeAction='//TypeAction)
    end select

  end subroutine IM_set_param
  !============================================================================
  subroutine IM_set_grid

    use ModCimiGrid,      ONLY: np, nt, xlat, phi
    use ModCimiPlanet,    ONLY: NameVarCouple, nVarImToGm
    use ModNumConst,      ONLY: cDegToRad
    use CON_coupler,      ONLY: set_grid_descriptor, IM_

    real:: Radius_I(1)
    logical:: IsInitialized=.false.
    logical:: DoTest, DoTestMe
    character(len=*), parameter:: NameSub = 'IM_set_grid'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)
    if(DoTest)write(*,*)'IM_set_grid_descriptor called, IsInitialized=',&
         IsInitialized
    if(IsInitialized) RETURN
    IsInitialized=.true.

    ! radial size of the ionosphere in meters
    Radius_I(1) = (6375.0+120.0)*1000.0

    if(DoTest)then
       write(*,*)NameSub,' ir,ip=',np,nt
       write(*,*)NameSub,' size(xlat)=',size(xlat),' size(phi)=',size(phi)
       write(*,*)NameSub,' xlat      =',xlat
       write(*,*)NameSub,' phi       =',phi
       write(*,*)NameSub,' rIono     =',Radius_I(1)
    end if

    ! RB grid size in generalized coordinates
    call set_grid_descriptor( IM_,                 & ! component index
         nDim=2,                                   & ! dimensionality
         nRootBlock_D=[1,1],                     & ! single block
         nCell_D=[np, nt],                       & ! size of cell based grid
         XyzMin_D=[0.5,    0.5],                 & ! min gen.coords for cells
         XyzMax_D=[np+0.5, nt+0.5],              & ! max gen.coords for cells
         TypeCoord='SMG',                          & ! solar magnetic coord
         Coord1_I=(90.0-xlat(1:np))*cDegToRad,     & ! colatitude in radians
         Coord2_I=phi,                             & ! longitude in radians
         Coord3_I=Radius_I,                        & ! radial size in meters
         IsPeriodic_D=[.false.,.true.],          & ! periodic in longitude
         nVar = nVarImToGm,                        & ! number of "fluid" vars
         NameVar = NameVarCouple) ! names of "fluid" vars

  end subroutine IM_set_grid
  !============================================================================
  subroutine IM_init_session(iSession, TimeSimulation)

    ! Initialize CIMI for this session
    ! GM info needed before initialization just set up latitude/longitude grid

    use ModCimiMethods,   ONLY: cimi_init
    use ModCimi,          ONLY: init_mod_cimi, Time
    use ModCimiTrace,     ONLY: init_mod_field_trace
    use ModCimiPlanet,    ONLY: init_cimi_planet_const
    use ModImTime
    use ModTimeConvert,   ONLY: time_real_to_int
    use CON_physics,      ONLY: get_time

    integer,  intent(in) :: iSession         ! session number (starting from 1)
    real,     intent(in) :: TimeSimulation   ! seconds from start time

    logical:: DoTest, DoTestMe
    character(len=*), parameter:: NameSub = 'IM_init_session'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)

    call init_mod_cimi
    call init_mod_field_trace
    call init_cimi_planet_const

    call cimi_init
    ! call init_gm_cimi

    call get_time(tStartOut = StartTime)
    Time = TimeSimulation
    CurrentTime = StartTime + Time
    call time_real_to_int(StartTime, iStartTime_I)

  end subroutine IM_init_session
  !============================================================================
  subroutine IM_run(TimeSimulation, TimeSimulationLimit)

    ! Run CIMI2 for 2 time steps

    use CON_time, ONLY: DoTimeAccurate
    use ModCimi, ONLY: dt, dtmax
    use ModCimiMethods, ONLY: cimi_init, cimi_run

    real, intent(in):: TimeSimulationLimit ! simulation time not to be exceeded
    real, intent(inout):: TimeSimulation   ! current time of component

    Logical:: IsInitialized = .false.

    logical:: DoTest, DoTestMe
    character(len=*), parameter:: NameSub = 'IM_run'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)
    if(DoTestMe)write(*,*) NameSub,': IsInitialized=', IsInitialized

    if (.not. IsInitialized) then
       call cimi_init
       IsInitialized = .true.
    endif

    if( .not. DoTimeAccurate)then
       ! steady state mode
       dt = dtmax
       call cimi_run(2*dt)
    else
       ! time accurate mode
       dt = min(dtmax, 0.5*(TimeSimulationLimit - TimeSimulation))
       call cimi_run(TimeSimulationLimit - TimeSimulation)
       ! return time at the end of the time step to CON
       TimeSimulation   = TimeSimulationLimit
    end if

  end subroutine IM_run
  !============================================================================
  subroutine IM_finalize(TimeSimulation)

    real, intent(in) :: TimeSimulation   ! seconds from start time

    logical:: DoTest, DoTestMe
    character(len=*), parameter:: NameSub = 'IM_finalize'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)

  end subroutine IM_finalize
  !============================================================================
  subroutine IM_save_restart(TimeSimulation)

    ! Save CIMI restart files including plasma sphere if needed

    use CON_coupler,     ONLY: NameRestartOutDirComp
    use ModCimiRestart,  ONLY: cimi_write_restart, NameRestartOutDir
    use ModPlasmasphere, ONLY: UseCorePsModel, save_restart_plasmasphere
    use ModCimiGrid, ONLY: iProc
    real,     intent(in) :: TimeSimulation   ! seconds from start time

    logical:: DoTest, DoTestMe
    character(len=*), parameter:: NameSub = 'IM_save_restart'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)
    if(NameRestartOutDirComp /= '') NameRestartOutDir = NameRestartOutDirComp
    if(DoTestMe)write(*,*) NameSub, &
         ': NameRestartOutDir=', trim(NameRestartOutDir)

    call cimi_write_restart
    if (UseCorePsModel .and. iProc == 0) call save_restart_plasmasphere

  end subroutine IM_save_restart
  !============================================================================
  subroutine IM_put_from_gm_crcm( &
       Buffer_IIV, BufferKp, BufferAe, iSizeIn, jSizeIn, nVarIn,&
       BufferLine_VI, nVarLine, nPointLine, NameVar, BufferSolarWind_V,&
       tSimulation)

    use ModGmCIMI,    ONLY: nVar, KpGm, UseGmKp, AeGm,UseGmAe, iLineIndex_II, &
         DoneGmCoupling, UseTotalRhoGm, UseTotalPGm, UseTotalPparGm, &
         UseMultiRhoGm, UseMultiPGm, UseMultiPparGm, DoFeedbackPs, UsePeGm,  &
         TotalRho_, TotalP_, TotalPpar_, TotalPe_, &
         iBufferRho_I, iBufferP_I, iBufferPpar_I,  &
         StateLine_VI, StateBmin_IIV, nPoint, nVarBmin,rBodyGm,iLatMin
    use ModCimiGrid,  ONLY: nLat => np, nLon => nt, iProc,xlatr
    use ModCimiPlanet, ONLY: rEarth => re_m, NameVarCouple,nVarImToGm,&
         Sw_, H_, O_
    use ModTsyInput,  ONLY: xnswa,vswa,bxw,byw,bzw,nsw,iyear,iday,UseSmooth
    use ModPrerunField, ONLY: DoWritePrerun, save_prerun
    use CON_coupler, ONLY: nVarBuffer, GM_, IM_, &
         iVarTarget_VCC, lComp_I
    use ModUtilities, ONLY: split_string, lower_case
    !  use ModPrerunField,ONLY: DoWritePrerun, save_prerun

    integer, intent(in) :: iSizeIn, jSizeIn, nVarIn
    real,    intent(in) :: &
         Buffer_IIV(iSizeIn,jSizeIn,nVarIn), BufferKp, BufferAe
    integer, intent(in) :: nVarLine, nPointLine
    real,    intent(in) :: BufferLine_VI(nVarLine, nPointLine)
    real,    intent(in) :: BufferSolarWind_V(8)

    character (len=*),intent(in) :: NameVar
    real, intent(in) :: tSimulation

    real, parameter :: noValue=-99999.
    real :: SwDensMax, SwVelMax, SwDensMin, SwVelMin
    integer:: n, iLat, iLon
    logical:: IsFirstCall = .true.

    integer :: iRho, iP, iPpar, iVarBuffer

    real :: LatBodyGM
    ! indexes of registered components
    integer :: iGm=0, iIm=0
    ! indexes of passed variables
    integer, allocatable :: iVarTarget_V(:)

    integer :: nVarCouple
    ! integer,parameter :: nVarMax=
    character(len=15), allocatable :: NameVarCouple_V(:)
    character(len=15) :: NameVar1

    ! The offset of the Buffer_IIV defining how many variables are passed before
    ! you get to densities and pressures
    integer, parameter :: Offset_ = 20

    logical :: DoTest, DoTestMe
    character(len=*), parameter:: NameSub = 'IM_put_from_gm_crcm'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)

    ! if BufferKp >0 then GM is passing Kp and we will store it
    if (BufferKp > 0) then
       KpGm = BufferKp
       UseGmKp = .true.
    else
       KpGm = -1.0
       UseGmKp = .false.
    endif

    ! if BufferAe >0 then GM is passing Ae and we will store it
    if (BufferAe > 0) then
       AeGm = BufferAe
       UseGmAe = .true.
    else
       AeGm = -1.0
       UseGmAe = .false.
    endif

    if(DoTestMe)write(*,*) NameSub,': allocated(iVarTarget_V)=', &
         allocated(iVarTarget_V)

    if(.not.allocated(iVarTarget_V))then
       iGm = lComp_I(GM_)
       iIm = lComp_I(IM_)
       allocate(iVarTarget_V(nVarBuffer))
       iVarTarget_V = iVarTarget_VCC(1:nVarBuffer,iGm,iIm)

       !       do iVarBuffer = 1, nVarBuffer
       !          iVarTarget = iVarTarget_V(iVarBuffer)
       !
       !          iSpecies = iVarTarget / 3 + 1
       !          iType    = modulo(iVarTarget,3) + 1
       !
       !          select case(iType)
       !          case(1)
       !             if(IsMultiDensityGm)then
       !                Density(...,iSpecies) = Buffer_IIV(iLon,iLat,iVarBuffer)
       !             else
       !                ! Split total density among species
       !             end if
       !          case(2)
       !
       !          if(iSpecies == nSpec) iType = iType + 1

       ! Based on CON_couple_gm_im we need to take the paired variables between
       ! IM and GM and match them up with the IM variables. Also fill in the
       ! index arrays to link each incomming mass density and pressure to
       ! an element of Buffer_IIV

       ! counters for filling index arrays
       iRho=1
       iP=1
       iPpar=1

       allocate(NameVarCouple_V(nVarImToGm))
       call split_string(NameVarCouple, NameVarCouple_V, nStringOut=nVarCouple)

       !       if (nVarCouple /= nVarBuffer) then
       !          write(*,*) 'ERROR: in:', NameSub
       !          write(*,*) 'nVarBuffer does not match number of &
       !               &variables in NameVarCouple'
       !          write(*,*) 'nVarBuffer = ', nVarBuffer
       !          write(*,*) 'nVarCouple = ', nVarCouple
       !          call con_stop('')
       !       endif

       ! write(*,*)len(NameVarCouple_V)
       ! do iVarBuffer = 1,nVarBuffer
       ! write(*,*) 'buffer in im', iVarTarget_V(iVarBuffer), NameVarCouple_V(iVarTarget_V(iVarBuffer))
       ! enddo
       ! write(*,*)''

       if(DoTestMe)write(*,*) NameSub, &
            ': nVarBuffer, nVarCouple=', nVarBuffer, nVarCouple

       do iVarBuffer = 1, nVarBuffer
          NameVar1 = NameVarCouple_V(iVarTarget_V(iVarBuffer))
          if(DoTestMe) write(*,*) NameSub,': iVarBuffer, NameVarCouple=', &
               iVarBuffer, NameVar1
          call lower_case(NameVar1)
          select case(NameVar1)
          case('rho')
             TotalRho_ = iVarBuffer + Offset_
             UseTotalRhoGm = .true.
          case('p')
             TotalP_ = iVarBuffer + Offset_
             UseTotalPGm = .true.
          case('ppar')
             TotalPpar_ = iVarBuffer + Offset_
             UseTotalPparGm = .true.
          case('pe')
             TotalPe_ = iVarBuffer + Offset_
             UsePeGm = .true.
          case('hprho')
             ! write(*,*) 'HpRho=',iVarBuffer
             UseMultiRhoGm = .true.
             iBufferRho_I(H_) = iVarBuffer+Offset_
             iRho=iRho+1
          case('hpp')
             UseMultiPGm = .true.
             iBufferP_I(H_) = iVarBuffer+Offset_
             iP = iP + 1
          case('hppar')
             UseMultiPparGm = .true.
             iBufferPpar_I(H_) = iVarBuffer+Offset_
             iPpar = iPpar + 1
          case('oprho')
             UseMultiRhoGm = .true.
             iBufferRho_I(O_) = iVarBuffer+Offset_
             iRho = iRho + 1
          case('opp')
             UseMultiPGm = .true.
             iBufferP_I(O_) = iVarBuffer+Offset_
             iP = iP + 1
          case('opppar')
             UseMultiPparGm = .true.
             iBufferPpar_I(O_) = iVarBuffer+Offset_
             iPpar = iPpar + 1
          case('hpswrho')
             UseMultiRhoGm = .true.
             iBufferRho_I(Sw_) = iVarBuffer+Offset_
             iRho=iRho+1
          case('hpswp')
             UseMultiPGm = .true.
             iBufferP_I(Sw_) = iVarBuffer+Offset_
             iP = iP + 1
          case('hpswppar')
             UseMultiPparGm = .true.
             iBufferPpar_I(Sw_) = iVarBuffer+Offset_
             iPpar=iPpar+1
          case('hppsrho')
             DoFeedbackPs = .true.
          case('hppsp')
             !do nothing for incomming plasmasphere pressure
          case default
             call CON_stop(NameSub//': unknown NameVarCouple='//trim(NameVar1))
          end select
       end do

    endif

    if(nVarLine /= nVar) then
       write(*,*)'nVarLine=',nVarLine
       call CON_stop(NameSub//' invalid nVarLine (should be 4)')
    end if

    DoneGmCoupling=.true.

    if(DoTestMe)then
       write(*,*)NameSub,' iSizeIn,jSizeIn,nVarIn=',&
            iSizeIn,jSizeIn,nVarIn
       write(*,*)NameSub,' nVarLine,nPointLine=',nVarLine,nPointLine
       write(*,*)NameSub,' Buffer_IIV(22,1,:)=',Buffer_IIV(22,1,:)
       write(*,*)NameSub,' Buffer_IIV(58,16,:)=',Buffer_IIV(58,16,:)
       write(*,*)NameSub,' BufferLine_VI(:,1) =',BufferLine_VI(:,1)
       write(*,*)NameSub,' BufferLine_VI(:,2) =',BufferLine_VI(:,2)
       write(*,*)NameSub, &
            ' Total: UseRho,iRho,UseP,iP,UsePpar,iPpar,UsePe,iPe=',&
            UseTotalRhoGm, TotalRho_, UseTotalPGm, TotalP_, UseTotalPparGm, &
            TotalPpar_, UsePeGm, TotalPe_
       write(*,*)NameSub,' UseMultiRhoGm, UseMultiPGm, UseMultiPparGm=', &
            UseMultiRhoGm, UseMultiPGm, UseMultiPparGm
       write(*,*)NameSub,' H_, O_, Sw_, DoPs=', H_, O_, Sw_, DoFeedBackPs
       write(*,*)NameSub,' iBufferRho_I=', iBufferRho_I
       write(*,*)NameSub,' iBufferP_I=', iBufferP_I
       write(*,*)NameSub,' iBufferPpar_I=', iBufferPpar_I
       write(*,*)NameSub,' IMF: Density  = ',BufferSolarWind_V(1)
       write(*,*)NameSub,' IMF: Velocity = ',BufferSolarWind_V(2)
       write(*,*)NameSub,' IMF: Bx       = ',BufferSolarWind_V(5)
       write(*,*)NameSub,' IMF: By       = ',BufferSolarWind_V(6)
       write(*,*)NameSub,' IMF: Bz       = ',BufferSolarWind_V(7)
    end if

    if (allocated(StateLine_VI)) deallocate(StateLine_VI,StateBmin_IIV)

    if (.not.allocated(StateLine_VI)) allocate( &
         StateLine_VI(nVarLine,nPointLine),&
         StateBmin_IIV(iSizeIn,jSizeIn,nVarIn))

    StateLine_VI      = BufferLine_VI
    StateBmin_IIV(:,:,:) = Buffer_IIV(:,:,:)

    ! convert unit of locations X and Y
    StateBmin_IIV(:,:,1:2) = StateBmin_IIV(:,:,1:2)/rEarth ! m --> Earth Radii

    ! convert unit of points around bmin for curvature
    StateBmin_IIV(:,:,6:20) = StateBmin_IIV(:,:,6:20)/rEarth ! m --> Earth Radii


    nPoint    = nPointLine
    nVarBmin = nVarIn
    ! Convert Units
    StateLine_VI(2,:) = StateLine_VI(2,:) / rEarth ! m --> Earth Radii
    StateLine_VI(3,:) = StateLine_VI(3,:) / rEarth ! m --> Earth Radii

    ! save the GM inner Boundary which is taken to be the
    ! lowest field trace point
    rBodyGM=minval(StateLine_VI(3,:))
    LatBodyGm = acos(sqrt(1.0/(rBodyGM)))
    ! set iLatMin for Minimum latitude in MHD boundary
    FIND_iLatMin: do iLat=1,nLat
       if (abs(xlatr(iLat))>=LatBodyGm) then
          iLatMin=iLat
          EXIT FIND_iLatMin
       end if
    end do FIND_iLatMin
    if(DoTestMe)then
       write(*,*) NameSub,': iLatMin, rBodyGm, StateBmin(iLatMin,1,:)', &
            iLatMin, rBodyGm, StateBmin_IIV(iLatMin,1,:)
       write(*,*) NameSub,': iBufferRho_I=', iBufferRho_I
    end if

    ! for anisopressure coupling
    ! if(DoTest .and. DoAnisoPressureGMCoupling)then
    !   write(NameOut,"(a,f6.1)") 'StateBmin_t_',tSimulation
    !   open(UnitTmp_,FILE=NameOut)
    !   write(UnitTmp_,"(a)") &
    !        'IM_put_from_gm_crcm, StateBmin_IIV, last index 1:5 '
    !   write(UnitTmp_,"(a)") &
    !        'Xeq Yeq Beq rho p ppar'
    !   do iLat =iSizeIn,1,-1
    !      do iLon =1,jSizeIn
    !         write(UnitTmp_,"(100es18.10)")StateBmin_IIV(iLat,iLon,1:5)
    !      enddo
    !   enddo
    !   close(UnitTmp_)
    ! end if

    !  if (nProc == 1) then
    !     write(NameOut,"(a,i2.2)") 'StateBmin1_0_iproc_',iProc
    !     open(UnitTmp_,FILE=NameOut)
    !     do iLat =1,nLat
    !        write(UnitTmp_,"(100es18.10)")StateBmin_IIV(iLat,1:nLon,1)
    !     enddo
    !     close(UnitTmp_)
    !     write(*,*)'!!! iProc, StateBmin_IIV(21,25)',iProc, StateBmin_IIV(21,25,1)
    !     write(*,*) '!!! iSizeIn,jSizeIn',iSizeIn,jSizeIn
    !     call con_stop('')
    !  else
    !     write(NameOut,"(a,i2.2)") 'StateBmin1_iproc_',iProc
    !     open(UnitTmp_,FILE=NameOut)
    !     do iLat =1,nLat
    !        write(UnitTmp_,"(100es18.10)")StateBmin_IIV(iLat,1:nLon,1)
    !     enddo
    !     close(UnitTmp_)
    !     write(*,*)'!!! iProc, StateBmin_IIV(21,25)',iProc, StateBmin_IIV(21,25,1)
    !     write(*,*) '!!! iSizeIn,jSizeIn',iSizeIn,jSizeIn
    !     call con_stop('')
    !  endif

    ! Solar wind values
    if(IsFirstCall .or. (.not. UseSmooth)) then
       xnswa(1) = BufferSolarWind_V(1)*1e-6                 ! m^-3 -->/cc
       vswa (1) = sqrt(sum(BufferSolarWind_V(2:4)**2))*1e-3 ! m/s-->km/s
    else
       ! Update Solar wind value, but do not let them change
       ! more than 5 percent per update
       SwDensMax = 1.05*xnswa(1)
       SwDensMin = 0.95*xnswa(1)
       SwVelMax  = 1.05*vswa(1)
       SwVelMin  = 0.95*vswa(1)
       xnswa(1) = min(SwDensMax,BufferSolarWind_V(1)*1e-6)
       xnswa(1) = max(SwDensMin,xnswa(1))
       vswa(1)  = min(SwVelMax,sqrt(sum(BufferSolarWind_V(2:4)**2))*1e-3)
       vswa(1)  = max(SwVelMin,vswa(1))
    endif
    bxw(1) = BufferSolarWind_V(5)*1e9      ! T --> nT
    byw(1) = BufferSolarWind_V(6)*1e9      ! T --> nT
    bzw(1) = BufferSolarWind_V(7)*1e9      ! T --> nT

    nsw = 1

    iyear=2002
    iday=1

    ! create an index array on the first call
    if (IsFirstCall) then
       n = 0
       do iLon = 1, nLon
          do iLat = 1, nLat
             n = n+1
             iLineIndex_II(iLon,iLat) = n
          end do
       end do
       IsFirstCall = .false.
    endif

    if (iProc==0 .and. DoWritePrerun) call save_prerun(tSimulation)

  end subroutine IM_put_from_gm_crcm
  !============================================================================
  subroutine IM_put_from_gm_line(nRadiusIn, nLonIn, Map_DSII, &
       nVarLineIn, nPointLineIn, BufferLine_VI, NameVar)

    integer, intent(in) :: nRadiusIn, nLonIn
    real,    intent(in) :: Map_DSII(3,2,nRadiusIn,nLonIn)
    integer, intent(in) :: nVarLineIn, nPointLineIn
    real,    intent(in) :: BufferLine_VI(nVarLineIn,nPointLineIn)
    character(len=*), intent(in) :: NameVar

    character(len=*), parameter:: NameSub = 'IM_put_from_gm_line'
    !--------------------------------------------------------------------------
    call CON_stop(NameSub//' should not be called for IM/CIMI')

  end subroutine IM_put_from_gm_line
  !============================================================================
  subroutine IM_put_from_gm(Buffer_IIV,BufferKp,iSizeIn,jSizeIn,nVarIn,NameVar)

    integer, intent(in) :: iSizeIn,jSizeIn,nVarIn
    real,    intent(in) :: BufferKp
    real, dimension(iSizeIn,jSizeIn,nVarIn), intent(in) :: Buffer_IIV
    character (len=*),intent(in)       :: NameVar

    character(len=*), parameter:: NameSub = 'IM_put_from_gm'
    !--------------------------------------------------------------------------
    call CON_stop(NameSub//' should not be called for IM/CIMI')

  end subroutine IM_put_from_gm
  !============================================================================
  subroutine IM_put_from_ie_mpi(nTheta, nPhi, Potential_II)

    integer, intent(in):: nTheta, nPhi
    real,    intent(in):: Potential_II(nTheta, nPhi)

    character(len=*), parameter:: NameSub = 'IM_put_from_ie_mpi'
    !--------------------------------------------------------------------------
    call CON_stop(NameSub//' cannot be used by CIMI!')

  end subroutine IM_put_from_ie_mpi
  !============================================================================
  subroutine IM_put_sat_from_gm(nSats, Buffer_I, Buffer_III)

    ! Puts satellite locations and names from GM into IM.
    use ModImSat, ONLY: nImSats, DoWriteSats, ReadRestartSat, &
         NameSat_I, SatLoc_3I
    use ModCimiGrid,   ONLY: iProc
    use ModIoUnit,     ONLY: UnitTmp_

    ! Arguments
    integer, intent(in)            :: nSats
    real, intent(in)               :: Buffer_III(4,2,nSats)
    character(len=100), intent(in) :: Buffer_I(nSats)

    ! Internal variables
    integer :: iError, iSat, l1, l2, iRow
    character(len=*), parameter:: NameSub = 'IM_put_sat_from_gm'
    !--------------------------------------------------------------------------
    ! Activate satellite writing in RCM
    DoWriteSats = .true.
    nImSats = nSats

    ! Check allocation of sat tracing variables
    if(allocated(SatLoc_3I)) deallocate(SatLoc_3I)
    if(allocated(NameSat_I)) deallocate(NameSat_I)

    allocate(SatLoc_3I(4,2,nImSats), stat=iError)
    allocate(NameSat_I(nImSats),     stat=iError)

    if (iProc == 0 .and. ReadRestartSat) then
       open(UnitTmp_,file="IM/restartIN/restart.sat",&
            status="old", form="unformatted")

       read(UnitTmp_) nImSats

       do iSat=1,nImSats

          read(UnitTmp_) NameSat_I(iSat)

       end do

       do iSat=1,nImSats

          do iRow=1,2

             read(UnitTmp_) SatLoc_3I(1:4,iRow,iSat)

          end do

       end do

       close(UnitTmp_)

       ReadRestartSat = .false.

    else

       ! Assign incoming values, remove path and extension from name.
       SatLoc_3I = Buffer_III
       SatLoc_3I(4,2,:)=SatLoc_3I(4,2,:)
       do iSat=1, nSats
          l1 = index(Buffer_I(iSat), '/', back=.true.) + 1
          l2 = index(Buffer_I(iSat), '.') - 1
          if (l1-1<=0) l1=1
          if (l2+1<=0) l2=len_trim(Buffer_I(iSat))
          NameSat_I(iSat) = Buffer_I(iSat)(l1:l2)
       end do

    endif

  end subroutine IM_put_sat_from_gm
  !============================================================================
  subroutine IM_get_for_gm(Buffer_IIV, iSizeIn, jSizeIn, nVar, NameVar)

    ! use CON_time, ONLY : get_time
    use ModCimiGrid,  ONLY: iSize=>np, jSize=>nt
    use ModCimi,      ONLY: Pressure_IC, PressurePar_IC, Bmin_C, Pmin
    use ModGmCimi,    ONLY: Den_IC, iLatMin!, DoMultiFluidGMCoupling, &
    ! DoAnisoPressureGMCoupling
    use ModCimiTrace, ONLY: iba
    use ModCimiPlanet, ONLY: &
         nspec, amu_I, NameVarCouple, nVarImToGm, Sw_, H_, O_, e_
    use ModConst,     ONLY: cProtonMass, cBoltzmann
    use ModUtilities,       ONLY: split_string
    use ModPlasmasphere, ONLY: PlasDensity_C
    !use DensityTemp,     ONLY: density

    integer, intent(in)         :: iSizeIn, jSizeIn, nVar
    real, intent(out)           :: Buffer_IIV(iSizeIn,jSizeIn,nVar)
    character(len=*), intent(in):: NameVar

    ! local variables:

    ! integer, parameter :: pres_=1, dens_=2, parpres_=3, bmin_=4,&
    !     Hpres_=3, Opres_=4, Hdens_=5, Odens_=6,Swdens_=7,Swpres_=8

    integer :: i,j
    ! logical :: DoCoupleSw
    ! temp for one way testing
    ! logical DoMultiFluidGmCoupling, DoAnisoPressureGmCoupling

    character (len=15),allocatable :: NameVarCouple_V(:)
    integer :: nVarCouple, iVarCimi

    ! assumed plasmapshere temperature in kelvin
    real :: Tplas=11000.0

    logical :: DoTest, DoTestMe
    character(len=*), parameter:: NameSub = 'IM_get_for_gm'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)
    if (DoTestMe) &
         write(*,*)NameSub,' starting with iSizeIn,jSizeIn,nVar,NameVar=',&
         iSizeIn, jSizeIn, nVar, NameVar

    if(iSizeIn /= iSize .or. jSizeIn /= jSize)then
       write(*,*)NameSub//' incorrect buffer size=',iSizeIn,jSizeIn
       call CON_stop(NameSub//' SWMF_ERROR')
    end if

    Buffer_IIV = 0.

    allocate(NameVarCouple_V(nVarImToGm))
    call split_string(NameVarCouple, NameVarCouple_V, nStringOut=nVarCouple)

    if(DoTestMe)write(*,*) NameSub, &
         ': nVarImToGm, nVarCouple=', nVarImToGm, nVarCouple

    do iVarCimi = 1, nVarImToGm
       if(DoTestMe) write(*,*) NameSub,': iVarCimi, NameVarCouple=', &
            iVarCimi, NameVarCouple_V(iVarCimi)
       select case(NameVarCouple_V(iVarCimi))
       case('Rho')
          do i=1,iSize; do j=1,jSize
             if( i<iLatMin .or.  i > iba(j) ) then
                Buffer_IIV(i,j,iVarCimi) = -1.
             else
                Buffer_IIV(i,j,iVarCimi) = &
                     sum(Den_IC (1:nspec-1,i,j)*cProtonMass*amu_I(1:nspec-1))
             end if
          enddo; enddo
       case('p')
          do i = 1, iSize; do j = 1, jSize
             if( i < iLatMin .or.  i > iba(j) ) then
                Buffer_IIV(i,j,iVarCimi) = -1.
             else
                ! make sure pressure passed to GM is not lower than Pmin [nPa]
                ! to avoid too low GM pressure (only ion pressure)
                Buffer_IIV(i,j,iVarCimi) = &
                     max(sum(Pressure_IC(1:nspec-1,i,j)), Pmin)*1e-9
             end if
          enddo; enddo
          ! write(*,*) 'IM max min p', &
          ! maxval(Buffer_IIV(:,:,iVarCimi)),minval(Buffer_IIV(:,:,iVarCimi))
       case('Ppar')
          do i=1,iSize; do j=1,jSize
             if( i<iLatMin .or.  i > iba(j) ) then
                Buffer_IIV(i,j,iVarCimi) = -1.
             else
                ! make sure pressure passed to GM is not lower than Pmin [nPa]
                ! to avoid too low GM pressure
                Buffer_IIV(i,j,iVarCimi) = &
                     max(sum(PressurePar_IC(1:nspec-1,i,j)), Pmin)*1e-9
             end if
          enddo; enddo
          ! write(*,*) 'IM max min ppar', &
          ! maxval(Buffer_IIV(:,:,iVarCimi)),minval(Buffer_IIV(:,:,iVarCimi))
       case('Pe')
          do i=1,iSize; do j=1,jSize
             if( i<iLatMin .or.  i > iba(j) ) then
                Buffer_IIV(i,j,iVarCimi) = -1.
             else
                ! make sure pressure passed to GM is not lower than Pmin [nPa]
                ! to avoid too low GM pressure (only ion pressure)
                Buffer_IIV(i,j,iVarCimi) = &
                     max(Pressure_IC(e_,i,j), Pmin)*1e-9
             end if
          enddo; enddo
       case('HpRho')
          do i=1,iSize; do j=1,jSize
             if( i<iLatMin .or.  i > iba(j) ) then
                Buffer_IIV(i,j,iVarCimi) = -1.
             else
                Buffer_IIV(i,j,iVarCimi) = &
                     Den_IC (H_,i,j)*cProtonMass*amu_I(H_)
             end if
          enddo; enddo
       case('HpP')
          do i=1,iSize; do j=1,jSize
             if( i<iLatMin .or.  i > iba(j) ) then
                Buffer_IIV(i,j,iVarCimi) = -1.
             else
                ! make sure pressure passed to GM is not lower than Pmin [nPa]
                ! to avoid too low GM pressure (only ion pressure)
                Buffer_IIV(i,j,iVarCimi) = &
                     max(Pressure_IC(H_,i,j), Pmin)*1e-9
             end if
          enddo; enddo
       case('HpPpar')
          do i=1,iSize; do j=1,jSize
             if( i<iLatMin .or.  i > iba(j) ) then
                Buffer_IIV(i,j,iVarCimi) = -1.
             else
                ! make sure pressure passed to GM is not lower than Pmin [nPa]
                ! to avoid too low GM pressure
                Buffer_IIV(i,j,iVarCimi) = &
                     max(PressurePar_IC(H_,i,j), Pmin)*1e-9
             end if
          enddo; enddo
       case('OpRho')
          do i=1,iSize; do j=1,jSize
             if( i<iLatMin .or.  i > iba(j) ) then
                Buffer_IIV(i,j,iVarCimi) = -1.
             else
                Buffer_IIV(i,j,iVarCimi) = &
                     Den_IC (O_,i,j)*cProtonMass*amu_I(O_)
             end if
          enddo; enddo
       case('OpP')
          do i=1,iSize; do j=1,jSize
             if( i<iLatMin .or.  i > iba(j) ) then
                Buffer_IIV(i,j,iVarCimi) = -1.
             else
                ! make sure pressure passed to GM is not lower than Pmin [nPa]
                ! to avoid too low GM pressure (only ion pressure)
                Buffer_IIV(i,j,iVarCimi) = &
                     max(Pressure_IC(O_,i,j), Pmin)*1e-9
             end if
          enddo; enddo
       case('OpPpar')
          do i=1,iSize; do j=1,jSize
             if( i<iLatMin .or.  i > iba(j) ) then
                Buffer_IIV(i,j,iVarCimi) = -1.
             else
                ! make sure pressure passed to GM is not lower than Pmin [nPa]
                ! to avoid too low GM pressure
                Buffer_IIV(i,j,iVarCimi) = &
                     max(PressurePar_IC(O_,i,j), Pmin)*1e-9
             end if
          enddo; enddo
       case('HpSwRho')
          do i=1,iSize; do j=1,jSize
             if( i<iLatMin .or.  i > iba(j) ) then
                Buffer_IIV(i,j,iVarCimi) = -1.
             else
                Buffer_IIV(i,j,iVarCimi) = &
                     Den_IC(Sw_,i,j)*cProtonMass*amu_I(Sw_)
             end if
          enddo; enddo
       case('HpSwP')
          do i=1,iSize; do j=1,jSize
             if( i<iLatMin .or.  i > iba(j) ) then
                Buffer_IIV(i,j,iVarCimi) = -1.
             else
                ! make sure pressure passed to GM is not lower than Pmin [nPa]
                ! to avoid too low GM pressure (only ion pressure)
                Buffer_IIV(i,j,iVarCimi) = &
                     max(Pressure_IC(Sw_,i,j), Pmin)*1e-9
             end if
          enddo; enddo
       case('HpSwPpar')
          do i=1,iSize; do j=1,jSize
             if( i<iLatMin .or.  i > iba(j) ) then
                Buffer_IIV(i,j,iVarCimi) = -1.
             else
                ! make sure pressure passed to GM is not lower than Pmin [nPa]
                ! to avoid too low GM pressure
                Buffer_IIV(i,j,iVarCimi) = &
                     max(PressurePar_IC(Sw_,i,j), Pmin)*1e-9
             end if
          enddo; enddo
       case('HpPsRho')
          do i=1,iSize; do j=1,jSize
             if( i<iLatMin .or.  i > iba(j) &
                  .or. .not.allocated(PlasDensity_C)) then
                Buffer_IIV(i,j,iVarCimi) = -1.
             else
                Buffer_IIV(i,j,iVarCimi) = PlasDensity_C(i,j)*cProtonMass
                ! PlasDensity_C (i,j)*cProtonMass
             end if
          enddo; enddo
       case('HpPsP')
          do i=1,iSize; do j=1,jSize
             if( i<iLatMin .or.  i > iba(j) &
                  .or. .not.allocated(PlasDensity_C)) then
                Buffer_IIV(i,j,iVarCimi) = -1.
             else
                Buffer_IIV(i,j,iVarCimi) = &
                     PlasDensity_C(i,j) * Tplas * cBoltzmann * 1e-9
                ! PlasDensity_C (i,j) * Tplas * cBoltzmann * 1e-9
             end if
          enddo; enddo
       case('PePar')
          do i=1,iSize; do j=1,jSize
             if( i<iLatMin .or.  i > iba(j) ) then
                Buffer_IIV(i,j,iVarCimi) = -1.
             else
                ! make sure pressure passed to GM is not lower than Pmin [nPa]
                ! to avoid too low GM pressure (only ion pressure)
                Buffer_IIV(i,j,iVarCimi) = &
                     max(PressurePar_IC(e_,i,j), Pmin)*1e-9
             end if
          enddo; enddo
       case default
          call CON_stop(NameSub//': unknown NameVar=' &
               //trim(NameVarCouple_V(iVarCimi)))
       end select
       if(DoTestMe) write(*,*) NameSub,' min,max Buffer=', &
            minval(Buffer_IIV(:,:,iVarCimi)), maxval(Buffer_IIV(:,:,iVarCimi))
    end do

    do i = 1, iSize; do j = 1, jSize
       if( i < iLatMin .or.  i > iba(j) ) then
          Buffer_IIV(i,j,nVarImToGm+1) = -1.
       else
          ! After cimi vars add the bmin which is needed for the coupling
          Buffer_IIV(i,j,nVarImToGm+1) = Bmin_C(i,j)
       end if
    enddo; enddo
    if(DoTestMe) write(*,*) NameSub,' min,max Buffer(Bmin)=', &
         minval(Buffer_IIV(:,:,nVarImToGm+1)), &
         maxval(Buffer_IIV(:,:,nVarImToGm+1))

    !  write(*,*) 'IM max min',maxval(Buffer_IIV(:,:,nVarImToGm+1)),minval(Buffer_IIV(:,:,nVarImToGm+1))

    ! add nan checking?
    !       ! Only a not-a-number can be less than zero and larger than one
    !       if(  .not. Buffer_IIV(i,j,pres_) > 0 .and. &
    !            .not. Buffer_IIV(i,j,pres_) < 1) then
    !          write(*,*)NameSub,': ERROR IN PRESSURE'
    !          write(*,*)NameSub,': i,j,Buffer =',i,j,Buffer_IIV(i,j,pres_)
    !          call CON_stop(NameSub//': Not a number found in IM pressure!')
    !       end if
    !       if(  .not. Buffer_IIV(i,j,dens_) > 0 .and. &
    !            .not. Buffer_IIV(i,j,dens_) < 1) then
    !          write(*,*)NameSub,': ERROR IN DENSITY'
    !          write(*,*)NameSub,': i,j,Buffer =',i,j,Buffer_IIV(i,j,dens_)
    !          call CON_stop(NameSub//': Not a number found in IM density!')
    !       end if
    !
    !       ! multi-fluid
    !       if(DoMultiFluidGMCoupling)then
    !          if(  .not. Buffer_IIV(i,j,Hpres_) > 0 .and. &
    !               .not. Buffer_IIV(i,j,Hpres_) < 1) then
    !             write(*,*)NameSub,': ERROR IN PRESSURE'
    !             write(*,*)NameSub,': i,j,Buffer =',i,j,Buffer_IIV(i,j,Hpres_)
    !             call CON_stop(NameSub//': Not a number found in IM Hp pressure!')
    !          end if
    !          if(  .not. Buffer_IIV(i,j,Hdens_) > 0 .and. &
    !               .not. Buffer_IIV(i,j,Hdens_) < 1) then
    !             write(*,*)NameSub,': ERROR IN DENSITY'
    !             write(*,*)NameSub,': i,j,Buffer =',i,j,Buffer_IIV(i,j,Hdens_)
    !             call CON_stop(NameSub//': Not a number found in IM Hp density!')
    !          end if
    !          if(  .not. Buffer_IIV(i,j,Opres_) > 0 .and. &
    !               .not. Buffer_IIV(i,j,Opres_) < 1) then
    !             write(*,*)NameSub,': ERROR IN PRESSURE'
    !             write(*,*)NameSub,': i,j,Buffer =',i,j,Buffer_IIV(i,j,Opres_)
    !             call CON_stop(NameSub//': Not a number found in IM Op pressure!')
    !          end if
    !          if(  .not. Buffer_IIV(i,j,Odens_) > 0 .and. &
    !               .not. Buffer_IIV(i,j,Odens_) < 1) then
    !             write(*,*)NameSub,': ERROR IN DENSITY'
    !             call CON_stop(NameSub//': Not a number found in IM Op density!')
    !          end if
    !       end if
    !
    !       ! aniso pressure
    !       if(DoAnisoPressureGMCoupling)then
    !          if(  .not. Buffer_IIV(i,j,parpres_) > 0 .and. &
    !               .not. Buffer_IIV(i,j,parpres_) < 1) then
    !             write(*,*)NameSub,': ERROR IN PRESSURE'
    !             write(*,*)NameSub,': i,j,Buffer =',i,j,Buffer_IIV(i,j,parpres_)
    !             call CON_stop(NameSub// &
    !                  ': Not a number found in IM parallel pressure !')
    !          end if
    !       endif
    !    end do; end do

    !    ! for anisopressure IM coupling
    !    if(DoTest .and. iProc == 0) then
    !       write(NameOut,"(a,f6.1,a)") 'IM_Buffer_IIV_t', Time, '.out'
    !       open(UnitTmp_,FILE=NameOut)
    !       write(UnitTmp_,"(a)") 'IM_get_for_GM, Buffer_IIV:'
    !       !    write(UnitTmp_,"(a)") ' '
    !       !    write(UnitTmp_,"(a)") ' '
    !       !    write(UnitTmp_,"(a)") ' '
    !       write(UnitTmp_,"(a)") 'bmin rho p ppar'
    !       do i = iSizeIn, 1, -1
    !          do j =1,jSizeIn
    !             write(UnitTmp_,"(4es18.10)") &
    !                  Buffer_IIV(i,j,bmin_), Buffer_IIV(i,j,dens_), &
    !                  Buffer_IIV(i,j,pres_), Buffer_IIV(i,j,parpres_)
    !          end do
    !       end do
    !       close(UnitTmp_)
    !    end if

    ! Units of rcm_mass_density are kg/m3
    !  where(Buffer_IIV(:,:,dens_) > 0.0) &
    !       Buffer_IIV(:,:,dens_) = Buffer_IIV(:,:,dens_)

    !  if(DoMultiFluidGMCoupling)then
!!!! ADD MultiFluid Coupling Later !!!
    !     ! MultiFluid
    !     where(Buffer_IIV(:,:,Hpres_) > 0.0) &
    !          Buffer_IIV(:,:,Hpres_) = Buffer_IIV(:,:,Hpres_) * 1.67E-35
    !     where(Buffer_IIV(:,:,Opres_) > 0.0) &
    !          Buffer_IIV(:,:,Opres_) = Buffer_IIV(:,:,Opres_) * 1.67E-35
    !     ! Units of rcm_mass_density are kg/m3
    !     where(Buffer_IIV(:,:,Hdens_) > 0.0) &
    !          Buffer_IIV(:,:,Hdens_) = Buffer_IIV(:,:,Hdens_) / 6.37E+15
    !     where(Buffer_IIV(:,:,Odens_) > 0.0) &
    !          Buffer_IIV(:,:,Odens_) = Buffer_IIV(:,:,Odens_) / 6.37E+15
    !  endif

    if(DoTestMe)write(*,*) NameSub,' finished'

  end subroutine IM_get_for_gm
  !============================================================================
  subroutine IM_get_for_gm_old(Buffer_IIV,iSizeIn,jSizeIn,nVar,NameVar)

    ! use CON_time, ONLY : get_time
    use ModCimiGrid,  ONLY: iSize=>np, jSize=>nt, iProc
    use ModCimi,      ONLY: Pressure_IC, PressurePar_IC, Bmin_C, Time, Pmin
    use ModGmCimi,    ONLY: Den_IC, iLatMin!, DoMultiFluidGMCoupling, &
    ! DoAnisoPressureGMCoupling
    use ModCimiTrace, ONLY: iba
    use ModCimiPlanet, ONLY: nspec,amu_I
    use ModConst,     ONLY: cProtonMass
    use ModIoUnit, ONLY: UnitTmp_

    integer, intent(in)                                :: iSizeIn,jSizeIn,nVar
    real, dimension(iSizeIn,jSizeIn,nVar), intent(out) :: Buffer_IIV
    character (len=*),intent(in)                       :: NameVar

    ! local variables

    integer, parameter :: pres_=1, dens_=2, parpres_=3, bmin_=4,&
         Hpres_=3, Opres_=4, Hdens_=5, Odens_=6,Swdens_=7,Swpres_=8

    integer :: i,j
    logical :: DoTest, DoTestMe
    character(len=100) :: NameOut
    logical :: DoCoupleSw
    ! temp for one way testing
    character(len=*), parameter:: NameSub = 'IM_get_for_gm_old'
    !--------------------------------------------------------------------------
    logical DoMultiFluidGmCoupling, DoAnisoPressureGmCoupling
    !--------------------------------------------------------------------------

    call CON_set_do_test(NameSub, DoTest, DoTestMe)
    if (DoTestMe) &
         write(*,*)NameSub,' starting with iSizeIn,jSizeIn,nVar,NameVar=',&
         iSizeIn,jSizeIn,nVar,NameVar

    DoMultiFluidGMCoupling = .false.
    DoAnisoPressureGMCoupling = .false.
    DoCoupleSw=.false.
    if(NameVar == 'p:rho:Hpp:Opp:Hprho:Oprho')then
       DoMultiFluidGMCoupling = .true.
    elseif(NameVar == 'p:rho:Hpp:Opp:Hprho:Oprho:Swprho:Swpp')then
       DoMultiFluidGMCoupling = .true.
       DoCoupleSw=.true.
    elseif(NameVar == 'p:rho:ppar:bmin')then
       DoAnisoPressureGMCoupling = .true.
    elseif(NameVar /= 'p:rho')then
       call CON_stop(NameSub//' invalid NameVar='//NameVar)
    end if

    if(iSizeIn /= iSize .or. jSizeIn /= jSize)then
       write(*,*)NameSub//' incorrect buffer size=',iSizeIn,jSizeIn
       call CON_stop(NameSub//' SWMF_ERROR')
    end if

    Buffer_IIV = 0.

    ! Fill pressure and density
    do i=1,iSize; do j=1,jSize
       if( i<iLatMin .or.  i > iba(j) ) then
          Buffer_IIV(i,j,pres_) = -1.
          Buffer_IIV(i,j,dens_) = -1.
          if(DoAnisoPressureGMCoupling)then
             Buffer_IIV(i,j,parpres_) = -1.
             Buffer_IIV(i,j,bmin_) = -1.
          end if
          if(DoMultiFluidGMCoupling)then
             !  Multifluid case
             Buffer_IIV(i,j,Hpres_) = -1.
             Buffer_IIV(i,j,Opres_) = -1.
             Buffer_IIV(i,j,Hdens_) = -1.
             Buffer_IIV(i,j,Odens_) = -1.
             if (DoCoupleSw) then
                Buffer_IIV(i,j,Swdens_) = -1.
                Buffer_IIV(i,j,Swpres_) = -1.
             endif
          end if
       else
          ! make sure pressure passed to GM is not lower than Pmin [nPa]
          ! to avoid too low GM pressure
          Buffer_IIV(i,j,pres_) = max(sum(Pressure_IC(:,i,j)), Pmin)*1e-9
          Buffer_IIV(i,j,dens_) = &
               sum(Den_IC (1:nspec-1,i,j)*cProtonMass*amu_I(1:nspec-1))
          if(DoAnisoPressureGMCoupling)then
             Buffer_IIV(i,j,parpres_) = &
                  max(sum(PressurePar_IC(:,i,j)), Pmin)*1e-9
             ! fill minimum B
             Buffer_IIV(i,j,bmin_) = Bmin_C(i,j)
          end if
          if(DoMultiFluidGMCoupling)then
             Buffer_IIV(i,j,Hpres_) = max(Pressure_IC(1,i,j), Pmin)*1e-9
             Buffer_IIV(i,j,Hdens_) = &
                  Den_IC (1,i,j)*cProtonMass*amu_I(1)
             Buffer_IIV(i,j,Opres_) = max(Pressure_IC(2,i,j), Pmin)*1e-9
             Buffer_IIV(i,j,Odens_) = &
                  Den_IC (2,i,j)*cProtonMass*amu_I(2)
             if (DoCoupleSw) then
                Buffer_IIV(i,j,Swdens_) = &
                     Den_IC (1,i,j)*cProtonMass*amu_I(1)
                Buffer_IIV(i,j,Swpres_) = max(Pressure_IC(1,i,j), Pmin)*1e-9
                Buffer_IIV(i,j,Hpres_) = max(Pressure_IC(2,i,j), Pmin)*1e-9
                Buffer_IIV(i,j,Hdens_) = &
                     Den_IC (2,i,j)*cProtonMass*amu_I(2)
                Buffer_IIV(i,j,Opres_) = max(Pressure_IC(3,i,j), Pmin)*1e-9
                Buffer_IIV(i,j,Odens_) = &
                     Den_IC (3,i,j)*cProtonMass*amu_I(3)
             endif
          end if
       end if

       ! Only a not-a-number can be less than zero and larger than one
       if(  .not. Buffer_IIV(i,j,pres_) > 0 .and. &
            .not. Buffer_IIV(i,j,pres_) < 1) then
          write(*,*)NameSub,': ERROR IN PRESSURE'
          write(*,*)NameSub,': i,j,Buffer =',i,j,Buffer_IIV(i,j,pres_)
          call CON_stop(NameSub//': Not a number found in IM pressure!')
       end if
       if(  .not. Buffer_IIV(i,j,dens_) > 0 .and. &
            .not. Buffer_IIV(i,j,dens_) < 1) then
          write(*,*)NameSub,': ERROR IN DENSITY'
          write(*,*)NameSub,': i,j,Buffer =',i,j,Buffer_IIV(i,j,dens_)
          call CON_stop(NameSub//': Not a number found in IM density!')
       end if

       ! multi-fluid
       if(DoMultiFluidGMCoupling)then
          if(  .not. Buffer_IIV(i,j,Hpres_) > 0 .and. &
               .not. Buffer_IIV(i,j,Hpres_) < 1) then
             write(*,*)NameSub,': ERROR IN PRESSURE'
             write(*,*)NameSub,': i,j,Buffer =',i,j,Buffer_IIV(i,j,Hpres_)
             call CON_stop(NameSub//': Not a number found in IM Hp pressure!')
          end if
          if(  .not. Buffer_IIV(i,j,Hdens_) > 0 .and. &
               .not. Buffer_IIV(i,j,Hdens_) < 1) then
             write(*,*)NameSub,': ERROR IN DENSITY'
             write(*,*)NameSub,': i,j,Buffer =',i,j,Buffer_IIV(i,j,Hdens_)
             call CON_stop(NameSub//': Not a number found in IM Hp density!')
          end if
          if(  .not. Buffer_IIV(i,j,Opres_) > 0 .and. &
               .not. Buffer_IIV(i,j,Opres_) < 1) then
             write(*,*)NameSub,': ERROR IN PRESSURE'
             write(*,*)NameSub,': i,j,Buffer =',i,j,Buffer_IIV(i,j,Opres_)
             call CON_stop(NameSub//': Not a number found in IM Op pressure!')
          end if
          if(  .not. Buffer_IIV(i,j,Odens_) > 0 .and. &
               .not. Buffer_IIV(i,j,Odens_) < 1) then
             write(*,*)NameSub,': ERROR IN DENSITY'
             call CON_stop(NameSub//': Not a number found in IM Op density!')
          end if
       end if

       ! aniso pressure
       if(DoAnisoPressureGMCoupling)then
          if(  .not. Buffer_IIV(i,j,parpres_) > 0 .and. &
               .not. Buffer_IIV(i,j,parpres_) < 1) then
             write(*,*)NameSub,': ERROR IN PRESSURE'
             write(*,*)NameSub,': i,j,Buffer =',i,j,Buffer_IIV(i,j,parpres_)
             call CON_stop(NameSub// &
                  ': Not a number found in IM parallel pressure !')
          end if
       endif
    end do; end do

    ! for anisopressure IM coupling
    if(DoTest .and. iProc == 0) then
       write(NameOut,"(a,f6.1,a)") 'IM_Buffer_IIV_t', Time, '.out'
       open(UnitTmp_,FILE=NameOut)
       write(UnitTmp_,"(a)") 'IM_get_for_GM, Buffer_IIV:'
       !    write(UnitTmp_,"(a)") ' '
       !    write(UnitTmp_,"(a)") ' '
       !    write(UnitTmp_,"(a)") ' '
       write(UnitTmp_,"(a)") 'bmin rho p ppar'
       do i = iSizeIn, 1, -1
          do j =1,jSizeIn
             write(UnitTmp_,"(4es18.10)") &
                  Buffer_IIV(i,j,bmin_), Buffer_IIV(i,j,dens_), &
                  Buffer_IIV(i,j,pres_), Buffer_IIV(i,j,parpres_)
          end do
       end do
       close(UnitTmp_)
    end if

    ! Units of rcm_mass_density are kg/m3
    !  where(Buffer_IIV(:,:,dens_) > 0.0) &
    !       Buffer_IIV(:,:,dens_) = Buffer_IIV(:,:,dens_)

    !  if(DoMultiFluidGMCoupling)then
!!!! ADD MultiFluid Coupling Later !!!
    !     ! MultiFluid
    !     where(Buffer_IIV(:,:,Hpres_) > 0.0) &
    !          Buffer_IIV(:,:,Hpres_) = Buffer_IIV(:,:,Hpres_) * 1.67E-35
    !     where(Buffer_IIV(:,:,Opres_) > 0.0) &
    !          Buffer_IIV(:,:,Opres_) = Buffer_IIV(:,:,Opres_) * 1.67E-35
    !     ! Units of rcm_mass_density are kg/m3
    !     where(Buffer_IIV(:,:,Hdens_) > 0.0) &
    !          Buffer_IIV(:,:,Hdens_) = Buffer_IIV(:,:,Hdens_) / 6.37E+15
    !     where(Buffer_IIV(:,:,Odens_) > 0.0) &
    !          Buffer_IIV(:,:,Odens_) = Buffer_IIV(:,:,Odens_) / 6.37E+15
    !  endif

    if(DoTestMe)write(*,*) NameSub,' finished'

  end subroutine IM_get_for_gm_old
  !============================================================================
  subroutine IM_put_from_ie( &
       nPoint, iPointStart, Index, Weight, DoAdd, Buff_V, nVar)

    use ModCimiGrid,  ONLY: np, nt
    use CON_router,   ONLY: IndexPtrType, WeightPtrType
    use ModIeCimi,    ONLY: Pot!, birk_mhd, iSize, jSize, sigmaH_mhd,sigmaP_mhd

    integer,intent(in)            :: nPoint, iPointStart, nVar
    real, intent(in)              :: Buff_V(nVar)
    type(IndexPtrType),intent(in) :: Index
    type(WeightPtrType),intent(in):: Weight
    logical,intent(in)            :: DoAdd
    integer ::i,j
    character(len=*), parameter:: NameSub = 'IM_put_from_ie'
    !--------------------------------------------------------------------------
    if(nPoint>1)then
       write(*,*)NameSub,': nPoint,iPointStart,Weight=',&
            nPoint,iPointStart,Weight % Weight_I
       call CON_stop(NameSub//': should be called with 1 point')
    end if
    if(DoAdd)then
       write(*,*)NameSub,': nPoint,iPointStart,Weight=',&
            nPoint,iPointStart,Weight % Weight_I
       write(*,*)NameSub,': WARNING DoAdd is true'
    end if

    i = Index % iCB_II(1,iPointStart)
    j = Index % iCB_II(2,iPointStart)

    if(i<1.or.i>np.or.j<1.or.j>nt)then
       write(*,*)'i,j,DoAdd=',i,j,DoAdd
       call CON_stop('IM_put_from_ie: index out of range')
    end if

    if(DoAdd)then
       Pot(i,j)        = pot(i,j)        + Buff_V(1)
       !     birk_mhd(i,j) = birk_mhd(i,j) + Buff_V(2)
       !     sigmaH_mhd(i,j) = sigmaH_mhd(i,j) + Buff_V(3)
       !     sigmaP_mhd(i,j) = sigmaP_mhd(i,j) + Buff_V(4)
    else
       pot(i,j)        = Buff_V(1)
       !     birk_mhd(i,j) = Buff_V(2)
       !     sigmaH_mhd(i,j) = Buff_V(3)
       !     sigmaP_mhd(i,j) = Buff_V(4)
    end if

  end subroutine IM_put_from_ie
  !============================================================================
  subroutine IM_put_from_ie_complete

    use ModCimiGrid,  ONLY: np,nt,iComm, iProc
    use ModIeCimi,    ONLY: Pot
    use ModPrerunField, ONLY: DoWritePrerun, save_prerun_IE
    use ModCimi,    ONLY: Time

    use ModMpi
    integer:: iError,nsize
    !--------------------------------------------------------------------------
    ! Bcast  potential to all procs
    nsize=np*nt
    call MPI_bcast(pot,nsize,MPI_REAL,0,iComm,iError)

    if (iProc == 0 .and. DoWritePrerun) call save_prerun_IE(Time)
    RETURN
  end subroutine IM_put_from_ie_complete
  !============================================================================
  subroutine IM_get_info_for_ie(nEngIM, EngIM)
      ! Tell IE how many energy bins are used in IM output
      use ModCimiGrid, ONLY: neng
      use ModCimi, ONLY: energy
      use ModCimiPlanet,ONLY: nspec

      integer, intent(out) :: nEngIM
      real, intent(out), optional :: EngIM(:, :)

      !------------------------------------------------------------------------
      nEngIM = neng
      if (present(EngIM)) then
         EngIM(1, :) = energy(1, :) ! Hydrogen Energy Grid
         EngIM(2, :) = energy(nspec, :) ! Electron Energy Grid
      endif

  end subroutine IM_get_info_for_ie
  !============================================================================

  subroutine IM_get_for_ie(nPoint,iPointStart,Index,Weight,Buff_V,nVar)

    ! Provide current for IE
    ! The value should be interpolated from nPoints with
    ! indexes stored in Index and weights stored in Weight
    ! The variables should be put into Buff_V

    use ModCimi,      ONLY: FAC_C, nLat=>np, nLon=>nt, Eje1, preP, preF
    use ModCimiPlanet,ONLY: H_, O_, e_, He_, Sw_
    use ModCimiGrid,  ONLY: neng
    use CON_router,   ONLY: IndexPtrType, WeightPtrType
    use ModCimiTrace, ONLY: iba

    integer,intent(in)            :: nPoint, iPointStart, nVar
    real,intent(out)              :: Buff_V(nVar)
    type(IndexPtrType),intent(in) :: Index
    type(WeightPtrType),intent(in):: Weight


    integer :: iLat, iLon, iBlock, iPoint
    real    :: w

    character(len=*), parameter:: NameSub = 'IM_get_for_ie'
    !--------------------------------------------------------------------------
    Buff_V = 0.0

    do iPoint = iPointStart, iPointStart + nPoint - 1

       iLat   = Index % iCB_II(1,iPoint)
       iLon   = Index % iCB_II(2,iPoint)
       iBlock = Index % iCB_II(3,iPoint)
       w      = Weight % Weight_I(iPoint)

       if(iBlock/=1)then
          write(*,*)NameSub,': iPoint,Index % iCB_II=',&
               iPoint,Index%iCB_II(:,iPoint)
          call CON_stop(NameSub//&
               ' SWMF_ERROR iBlock should be 1=North in IM-IE coupling')
       end if

       if (iLon > nLon) iLon = mod(iLon,nLon)
       if(iLat<1 .or. iLat>nLat*2 .or. iLon<0 .or. iLon>nLon)then
          write(*,*)'iLat,iLon=',iLat, nLat, iLon, nLon
          call CON_stop(NameSub//' SWMF_ERROR index out of range')
       end if

       ! Only worry about the northern hemisphere....  IE can fix the southern hemisphere.
       ! For now, southern hemisphere will be implemented soon
       if (iLat <= nLat .and. iLon <= nLon) then
          if(nVar >= 4) then
              ! Put ions
              Buff_V(1) = Buff_V(1) + w * PreF(H_, iLat, iLon, neng+2)
              Buff_V(2) = Buff_V(2) + w * Eje1(H_, iLat, iLon)
              ! Put Electrons
              Buff_V(3) = Buff_V(3) + w * PreF(e_, iLat, iLon, neng+2)
              Buff_V(4) = Buff_V(4) + w * Eje1(e_, iLat, iLon)
          end if
          if(nVar >= 5) then
              ! Put boundary location
              if(iLat <= iba(iLon)) Buff_V(5) = Buff_V(5) + w
          end if
          if(nVar >= 6) then
              ! Put FAC
              Buff_V(6) = Buff_V(6) - w * FAC_C(iLat, iLon)/2.0
          end if
          if(nVar==(6 + 2 * neng)) then
              ! Put ions
              Buff_V(6:5+neng) = Buff_V(6:5+neng) + w * &
                      PreP(H_, iLat, iLon, 1:neng)
              ! Put Electrons
              Buff_V(6+neng:5+2*neng) = Buff_V(6+neng:5+2*neng) + w * &
                      PreP(e_, iLat, iLon, 1:neng)
          else if(nVar >= 4 .and. nVar <= 6) then
            CYCLE
          else
              call CON_stop(NameSub//' CIMI coupling currently only uses '//&
                            'nVar=4,5 or 5+2*neng')
          end if
          ! OLD IMPLEMENTATION
          !Buff_V(1) = Buff_V(1) - w * FAC_C(iLat,iLon)/2.0 ! / 1.0e6
          !! Fill with -1 for now. In the future we will determine these values
          !Buff_V(2) = -1.0
          !Buff_V(3) = -1.0
          !        Buff_V(1) = Buff_V(1) - w * birk(iLat,iLon)/2 / 1.0e6
          !        Buff_V(2) = Buff_V(2) + w * eflux(iLat,iLon,1)
          !        Buff_V(3) = Buff_V(3) + w * eavg(iLat,iLon,1) / 1000.0
       endif

       !     if (iLat > nLat .and. iLon <= nLon) &
       !          Buff_V(1) = Buff_V(1) + w * birk(2*nLat-iLat+1,iLon)/2 / 1.0e6

       !     if (iLat > IONO_nTheta .and. iLon <= IONO_nPsi) &
       !          Buff_V(1) = Buff_V(1) + w * IONO_SOUTH_RCM_JR(2*IONO_nTheta-iLat+1,iLon)

    end do

  end subroutine IM_get_for_ie
  !============================================================================
  subroutine IM_get_for_ie_mpi(nTheta, nPhi, Buffer_IIV, &
                                              nVarImIe, NameVarImIe_V)

    integer, intent(in) :: nTheta, nPhi, nVarImIe
    real, intent(out) :: Buffer_IIV(nTheta, nPhi, nVarImIe)
    character(len=3), intent(out) :: NameVarImIe_V(nVarImIe)

    character(len=*), parameter :: NameSub = 'IM_get_for_ie_mpi'
    !-------------------------------------------------------------------------
    call CON_stop(NameSub//' cannot be used by CIMI!')

  end subroutine IM_get_for_ie_mpi
  !============================================================================
end module IM_wrapper
!==============================================================================
