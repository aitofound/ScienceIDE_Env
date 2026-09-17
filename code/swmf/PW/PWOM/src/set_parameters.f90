!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf
subroutine PW_set_parameters(NameAction)

  !****************************************************************************
  ! This subroutine gets the inputs for PWOM
  !****************************************************************************

  use ModPwom
  use ModReadParam
  use ModCommonVariables, ONLY: F107,F107A,AP,UseStaticAtmosphere,DrBnd,&
                                UsePhotoElectronHeatFlux,UseAuroralHeatFlux, &
                                UseCuspHeatFlux, UseFluidWPI
  use ModPwTime
  use ModPwPlots, ONLY: TypePlot
  use ModPwWaves, ONLY: UseWaveAcceleration 
  use ModPhotoElectron, ONLY: PrecipEnergyMin, PrecipEnergyMax, &
       PrecipEnergyMean, PrecipEnergyFlux,UseFixedPrecip, &
       PolarRainEMin, PolarRainEMax, &
       PolarRainEMean, PolarRainEFlux,UsePolarRain, &
       DoCoupleSE, UseFeedbackFromSE,IsVerboseSE,DtGetSe
  use ModOvation, ONLY: UseOvation,DoPlotOvation,OvationEmin,OvationEmax,&
       DoPlotOvation
  use ModParticle,ONLY: UseWPI,IsVerboseParticle, TypeWPI, FracLeftHand, &
       SpectralIndexAur, rWaveRefAur, E2waveRefAur, fWaveRefAur, &
       SpectralIndexCap, rWaveRefCap, E2waveRefCap, fWaveRefCap, &
       DoSavePlotParticle,DtSaveProfile, DtSaveDF,&
       nWaveFile, NameWaveFile,nSpecGrid,&
       TypeSpecGrid, EminSpecGrid, EmaxSpecGrid
  use ModReGrid, ONLY: DoRegrid,DtRegrid,DoSavePoints,DoAdaptGrid,&
       TypeAdaptCriteria

  use ModPwIndices, ONLY: UsePwIndicesFile 
  implicit none

  character (len=*), intent(in) :: NameAction ! READ or CHECK

  character (len=100) :: StringLine
  character (len=100) :: StringLine_I(100)
  character (len=100) :: NameCommand
  real:: SwDen, Vx, Bx, Bz, By, HPI, Energy
  integer:: iDate, iError, iEnergy

  character (len=*), parameter  :: NameSub = 'PW_set_parameters'
  !---------------------------------------------------------------------------
  do
     if(.not.read_line() ) EXIT
     if(.not.read_command(NameCommand)) CYCLE
     select case(NameCommand)
     case('#STOP')
        if(IsStandAlone)then
           call read_var('Tmax', Tmax)
           call read_var('MaxStep', MaxStep)
        else
           write(*,*)'PWOM WARNING: #STOP command is ignored in the framework'
        end if

     case('#STARTTIME')
        if(IsStandAlone)then
           !read in iYear,iMonth,iDay,iHour,iMinute,iSecond into iStartTime
           do iDate = 1, 6
              call read_var('iStartTime', iStartTime(iDate))
           enddo
           
        else
           write(*,*)'PWOM WARNING: ', &
                '#STARTTIME command is ignored in the framework'
        end if

     case('#STATICATMOSPHERE')
        call read_var('UseStaticAtmosphere', UseStaticAtmosphere)

     case('#MSISPARAM')
        call read_var('F107' ,F107)
        call read_var('F107A', F107A)
        call read_var('AP1', AP(1))
        call read_var('AP2', AP(2))
        call read_var('AP3', AP(3))
        call read_var('AP4', AP(4))
        call read_var('AP5', AP(5))
        call read_var('AP6', AP(6))
        call read_var('AP7', AP(7))

     case('#TIMEACCURATE')
        call read_var('IsTimeAccurate',IsTimeAccurate)
     case('#SAVEPLOT')
        call read_var('DtSavePlot',DtOutput)
        call read_var('DnSavePlot',DnOutput)
        call read_var('SaveFirst',DoSavePlot)
        call read_var('DoAppendPlot',DoAppendPlot)

     case('#TYPEPLOT')
        call read_var('TypePlot', TypePlot)

     case('#SAVEPLOTELECTRODYNAMICS')
        call read_var('DoPlotElectrodynamics', DoPlotElectrodynamics)
        call read_var('DtPlotElectrodynamics', DtPlotElectrodynamics)

     case('#SCHEME')
        call read_var('TypeSolver', TypeSolver)
        call read_var('TypeFlux', TypeFlux)
        call read_var('DtVertical', DtVertical)
        call read_var('IsFullyImplicit'   ,IsFullyImplicit)
        if(IsFullyImplicit)then
           IsPointImplicit = .false.
           IsPointImplicitAll = .false.
        else
           call read_var('IsPointImplicit'   ,IsPointImplicit)
           call read_var('IsPointImplicitAll', IsPointImplicitAll)
        end if

     case('#VARIABLEDT')
        call read_var('IsVariableDt', IsVariableDt)

     case('#LIMITER')
        call read_var('LimiterBeta', BetaIn)
        Beta = BetaIn

     case('#RESTART')
        call read_var('IsRestart', IsRestart)

     case('#REGRID')
        call read_var('DoRegrid',DoRegrid)
        if(DoRegrid)then
           call read_var('DtRegrid',DtRegrid)
           call read_var('DoSavePoints',DoSavePoints)
           call read_var('DoAdaptGrid',DoAdaptGrid)
           if(DoAdaptGrid) then
              call read_var('TypeAdaptCriteria',TypeAdaptCriteria)
           endif
        endif

     case('#MOTION')
        call read_var('DoMoveLine', DoMoveLine)

     case('#FAC')
        call read_var('UseJr', UseJr)

     case('#AURORA')
        call read_var('UseAurora', UseAurora)

     case('#JOULEHEATING')
        call read_var('UseJouleHeating', UseJouleHeating)

     case('#ROTATION')
        call read_var('UseCentrifugal', UseCentrifugal)

     case('#WAVES')
        call read_var('UseWaveAcceleration', UseWaveAcceleration)

     case('#TIMESTEP')
        call read_var('DtHorizontal', DtHorizontal)
        DtHorizontalOrig = DtHorizontal

     case('#VERTICALGRID')
        call read_var('nPoints', nAlt)
        call read_var('DeltaR', DrBnd)

     case('#FIELDLINE')
        call read_var('nTotalLine', nTotalLine)

     case('#LOG')
        call read_var('WriteLog', nLog) ! nLog=-1 write for all lines
                                        ! nLog= 0 write for no lines
                                        ! nLog= 1..nTotalLine, write one line
     case('#TEST')
        call read_var('StringTest', StringTest)
        call read_var('iProcTest',  iProcTest)
        call read_var('iLinetest',  iLineTest)

     case('#HEAT')
        call read_var('UseIonHeat', UseIonHeat)
        call read_var('UseEleHeat', UseEleHeat)
        if (UseEleHeat) then
           call read_var('UseExplicitHeat', UseExplicitHeat)
        else
           UseExplicitHeat = .false.
        endif
        
     case('#HEATFLUX')
        call read_var('UsePhotoElectronHeatFlux', UsePhotoElectronHeatFlux)
        call read_var('UseAuroralHeatFlux', UseAuroralHeatFlux)
        call read_var('UseCuspHeatFlux', UseCuspHeatFlux)

     case ("#MHD_INDICES")
        StringLine_I(1) = NameCommand
        call read_var('UpstreamFile', StringLine)
        StringLine_I(2) = StringLine
        StringLine_I(3) = " "
        StringLine_I(4) = "#END"
        
        call IO_set_inputs(StringLine_I)
        call read_MHDIMF_Indices(iError)
        ! When reading solar wind data, use the Weimer potential
        UseWeimer = .true.
        UseConstantIMF = .false.

     case ("#SOLARWIND")
        call read_var('Bx', Bx)
        call read_var('By', By)
        call read_var('Bz', Bz)
        call read_var('Vx', Vx)
        call read_var('SwDen', SwDen)
        call IO_set_imf_by_single(By)
        call IO_set_imf_bz_single(Bz)
        call IO_set_sw_v_single(abs(Vx))
        call IO_set_SW_n_single(SwDen)
        ! When using fixed solar wind data, use the Weimer potential
        UseWeimer = .true.
        UseConstantIMF = .true.

     case ("#HPI")
        call read_var('HemisphericPower', HPI)
        call IO_set_hpi_single(HPI)
        
     case ("#NOAAHPI_INDICES")
        StringLine_I(1) = "#NOAAHPI_INDICES"
        call read_var('NameHpiFile', StringLine)
        StringLine_I(2) = StringLine
        StringLine_I(3) = " "
        StringLine_I(4) = "#END"
        call IO_set_inputs(StringLine_I)
        call read_NOAAHPI_Indices(iError)
        if (iError /= 0) &
             write(*,*) "PW_ERROR: read hpi indices was NOT successful"

     case ("#NGDC_INDICES")
        StringLine_I(1) = "#NGDC_INDICES"
        call read_var('NameNgdcFile', StringLine)
        StringLine_I(2) = StringLine
        StringLine_I(3) = " "
        StringLine_I(4) = "#END"
        
        call IO_set_inputs(StringLine_I)
        call read_NGDC_Indices(iError)

        ! F107 file                                                            
        StringLine_I(1) = "#NGDC_INDICES"
        call read_var('NameNgdcFile', StringLine)
        StringLine_I(2) = StringLine
        StringLine_I(3) = " "
        StringLine_I(4) = "#END"

        call IO_set_inputs(StringLine_I)
        call read_NGDC_Indices(iError)
        
        UseIndicies = .true.
    
        if (iError /= 0) then
           write(*,*) "PW_ERROR: read indices was NOT successful"
        endif

     case('#INDICESFILE')
        call read_var('UsePwIndicesFile',UsePwIndicesFile )
        
     case('#SE')
        call read_var('DoCoupleSE', DoCoupleSE)
        if(DoCoupleSE)then
           call read_var('UseFeedbackFromSE', UseFeedbackFromSE)
           call read_var('IsVerboseSE', IsVerboseSE)
           call read_var('DtGetSe', DtGetSe)
        end if

     case('#SETPRECIP')
        call read_var('UseFixedPrecip', UseFixedPrecip)
        if(UseFixedPrecip)then
           call read_var('PrecipEnergyMin',  PrecipEnergyMin)        
           call read_var('PrecipEnergyMax',  PrecipEnergyMax)        
           call read_var('PrecipEnergyMean', PrecipEnergyMean)        
           call read_var('PrecipEnergyFlux', PrecipEnergyFlux)        
        end if

     case('#OVATION')
        call read_var('UseOvation',   UseOvation)
        if(UseOvation)then
           call read_var('DoPlotOvation', DoPlotOvation)
           call read_var('OvationEmin',   OvationEmin)
           call read_var('OvationEmax',   OvationEmax)
        end if

     case('#POLARRAIN')
        call read_var('UsePolarRain', UsePolarRain)
        if(UsePolarRain)then
           call read_var('PolarRainEMin',  PolarRainEMin)        
           call read_var('PolarRainEMax',  PolarRainEMax)        
           call read_var('PolarRainEMean', PolarRainEMean)        
           call read_var('PolarRainEFlux', PolarRainEFlux)        
        end if

     case('#PARTICLES')
        call read_var('UseParticles',  UseParticles)
        call read_var('UseParticleFeedback', UseParticleFeedback)
        call read_var('DtCoupleParticles',  DtCoupleParticles)
        call read_var('DoInitAltParticles', DoInitAltParticles)
        call read_var('nAltParticles', nAltParticles)
        call read_var('AltMinParticles', AltMinParticles)
        call read_var('AltMaxParticles', AltMaxParticles)
        call read_var('UseWPI', UseWPI)
        call read_var('IsVerboseParticle', IsVerboseParticle)

     case('#SAVEPLOTPARTICLE')
        call read_var('DoSavePlotParticle',  DoSavePlotParticle)
        call read_var('DtSaveProfile',  DtSaveProfile)
        call read_var('DtSaveDF',  DtSaveDF)

     case('#PLOTENERGYGRID')
        call read_var('TypeSpecGrid' ,     TypeSpecGrid)
        call read_var('EminSpecGrid' ,     EminSpecGrid)
        call read_var('EmaxSpecGrid' ,     EmaxSpecGrid)
        call read_var('nSpecGrid'    ,     nSpecGrid)
                
     case('#WPI')
        call read_var('TypeWPI',  TypeWPI)
        if (TypeWPI == 'File') then
           call read_var('FracLeftHand',  FracLeftHand)
           call read_var('nWaveFile',  nWaveFile)
           call read_var('NameWaveFile',  NameWaveFile)
        else
           call read_var('FracLeftHand',  FracLeftHand)
           call read_var('SpectralIndexAur',  SpectralIndexAur)
           call read_var('rWaveRefAur', rWaveRefAur)
           call read_var('E2WaveRefAur',  E2WaveRefAur)
           call read_var('fWaveRefAur', fWaveRefAur)
           call read_var('SpectralIndexCap',  SpectralIndexCap)
           call read_var('rWaveRefCap', rWaveRefCap)
           call read_var('E2WaveRefCap',  E2WaveRefCap)
           call read_var('fWaveRefCap', fWaveRefCap)
        end if
        
     case('#FLUIDWPI')
        call read_var('UseFluidWPI',  UseFluidWPI)
        
     case('#UPPERBC')
        call read_var('NameUpperBC',  NameUpperBC)


     endselect
  enddo
  !============================================================================
end subroutine PW_set_parameters
!==============================================================================

