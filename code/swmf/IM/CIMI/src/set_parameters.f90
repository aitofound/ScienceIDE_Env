subroutine CIMI_set_parameters(NameAction)

  ! Description:
  ! This subroutine gets the inputs for CIMI
  
  use ModIoUnit,         ONLY: UnitTmp_, io_unit_new
  use ModReadParam
  use ModUtilities,	 ONLY: lower_case
  use ModCimiInitialize, ONLY: &
       IsEmptyInitial, IsDataInitial, IsRBSPData, IsGmInitial, &
       DoLstarInitialization
  use ModCimiPlot
  use ModCimiTrace,	 ONLY: UseEllipse, UseSmooth, UseCorotation, &
       UsePotential, SmoothWindow, imod, iLatTest, iLonTest, DeltaRMax, &
       xmltlim, UseAltitudePrecip, UsePrecipEnergyLoss
  use ModCimi,		 ONLY: UseMcLimiter, BetaLimiter, time, Pmin, &
       IsStandAlone, UseStrongDiff, &
       dt, dtmax, DoCalcPrecip, DtCalcPrecip, IsStrictDrift,&
       UseDecay, DecayTimescale, useElectronDecay, UseFLC
  use ModCimiRestart,	 ONLY: IsRestart, DtSaveRestart
  use ModCimiPlanet,	 ONLY: nspec, dFactor_I, tFactor_I
  use ModImTime,	 ONLY: iStartTime_I, TimeMax
  use ModCimiBoundary,	 ONLY: &
       UseBoundaryEbihara, UseYoungEtAl
  use ModIeCimi,	 ONLY: UseWeimer
  use ModPrerunField,	 ONLY: DoWritePrerun, UsePrerun, DtRead
  use ModGmCIMI,	 ONLY: UseGm
  use CIMI_waves,	 ONLY: &
       UseWaves, UseHiss, UseChorus, &
       NameHissFile, NameChorusFile,  &
       UseKpIndex
  use ModImIndices,      ONLY: NameAeFile, read_ae_wdc_kyoto,&
       UseKpApF107IndicesFile,read_kpapf107_indices_file,&
       NameDstFile, read_dst_wdc_kyoto, UseDstKyoto,UseAeKyoto
  use ModDiagDiff,       ONLY: UseDiagDiffusion,&
                               UsePitchAngleDiffusionTest,&
                               UseEnergyDiffusionTest
   
  use ModImSat,		 ONLY: DtSatOut, DoWritePrerunSat, UsePrerunSat, &
       DtReadSat, DoWriteSats, ReadRestartSat
  use ModCimiGrid
  use ModLstar,		 ONLY: DoVerboseLstar
  use ModPlasmasphere,   ONLY: &
       DoSavePlas, DtPlasOutput, UseCorePsModel, PlasMinDensity,&
       PlasmaPauseDensity
  use ModInterFlux,      ONLY: UseHigherOrder, iOrderLat, iOrderLon
  use GIMME_cimi_interface, ONLY: DtGimmePlot, UseGimme
  use ModUtilities, ONLY: CON_stop
  
  implicit none

  logical 			:: DoEcho = .false.
  character (len=100)           :: NameCommand, StringCIMIPlot
  character (len=*), intent(in) :: NameAction
  character (len=7)             :: TypeBoundary
  character (len=3)             :: NameModel
  character (len=*), parameter  :: NameSub = 'CIMI_set_parameters'

  character (len=100) :: cTempLine
  character (len=100), dimension(100) :: cTempLines
  
  integer :: iError, iDate, iCIMIPlotType, nCIMIPlotType
  real :: DensitySW, VelSW, BxSW, BySW, BzSW, DtOutputCIMIPlot
  real :: ElectronTempRatio
  logical :: DoSaveSeparateFiles
  

  character (len=5)             :: TypeComposition='FIXED'
  integer :: iSpec
  !---------------------------------------------------------------------------
  do
     if(.not.read_line() ) EXIT
     if(.not.read_command(NameCommand)) CYCLE

     select case(NameCommand)

     case("#ECHO")
!        call check_stand_alone
        call read_var('DoEcho', DoEcho)
        if(iProc==0)call read_echo_set(DoEcho)

     case('#STOP')
        if(IsStandAlone)then
           call read_var('TimeMax',TimeMax)
        else
           write(*,*)'IM WARNING: #STOP command is ignored in the framework'
        end if

     case('#STARTTIME')
        if(IsStandAlone)then
           !read in iYear,iMonth,iDay,iHour,iMinute,iSecond into iStartTime
           do iDate=1,6
              call read_var('iStartTime',iStartTime_I(iDate))
           enddo
           
        else
           write(*,*)'IM WARNING:#STARTTIME command is ignored in the framework'
        end if

     case ("#NGDC_INDICES")
        cTempLines(1) = NameCommand
        call read_var('NameNGDCFile', cTempLine)
        cTempLines(2) = cTempLine
        cTempLines(3) = " "
        cTempLines(4) = "#END"
        
        call IO_set_inputs(cTempLines)
        call read_NGDC_Indices(iError)
        
        if (iError /= 0) then
           write(*,*) 'IM: read_NGDC_Indices iError=', iError
           call CON_stop('IM: Could not read '//trim(cTempLine))
        endif

     case ("#MHD_INDICES")
        cTempLines(1) = NameCommand
        call read_var('UpstreamFile',cTempLine)
        cTempLines(2) = cTempLine
        cTempLines(3) = " "
        cTempLines(4) = "#END"
        
        call IO_set_inputs(cTempLines)
        call read_MHDIMF_Indices(iError)
        if(iError /= 0)then
           write(*,*)'IM: read_MHDIMF_Indices iError=', iError
           call CON_stop('IM: Could not read '//trim(cTempLine))
        end if
     case ("#KYOTO_DST")
        call read_var('UseDstKyoto',UseDstKyoto)
        if(UseDstKyoto)then
           call read_var('NameDstFile',NameDstFile)

           call read_dst_wdc_kyoto(iError)
                
           if (iError /= 0) then
              write(*,*)'IM: read_dst_wdc_kyotom iError=', iError
              call CON_stop('IM: Could not read '//trim(NameDstFile))
           end if
        end if
     case ("#KYOTO_AE")
        call read_var('UseAeKyoto',UseAeKyoto)
        if(UseAeKyoto)then
           call read_var('NameAeFile',NameAeFile)
           call read_ae_wdc_kyoto(iError)
                
           if (iError /= 0) then
              write(*,*)'IM: read_ae_wdc_kyoto iError=', iError
              call CON_stop('IM: Could not read '//trim(NameAeFile))
           end if
        end if
     case ("#POTSDAM_KP_AP_F107")
        !use Kp, Ap, and F107 from Geomagnetic Observatory Niemegk, &
        !GFZ German Research Centre for Geosciences
        !note this file has all data from 1937, but does need to be periodically
        !updated from https://www.gfz-potsdam.de/en/section/geomagnetism/data-products-services/geomagnetic-kp-index
        call read_var('UseKpApF107IndicesFile',UseKpApF107IndicesFile)
        call read_kpapf107_indices_file
        
     case ("#SOLARWIND")
        call read_var('DensitySW',DensitySW)
        call read_var('VelSW',VelSW)
        call read_var('BxSW',BxSW)
        call read_var('BySW',BySW)
        call read_var('BzSW',BzSW)
        
        call IO_set_imf_bx_single(BxSW)
        call IO_set_imf_by_single(BySW)
        call IO_set_imf_bz_single(BzSW)
        call IO_set_sw_v_single(abs(VelSW))
        call IO_set_sw_n_single(abs(DensitySW))

     case('#SMOOTH')
        !do you want to smooth the solar wind for Tsy model
        call read_var('UseSmooth',UseSmooth)
        if (UseSmooth) call read_var('SmoothWindow',SmoothWindow)

     case('#BMODEL')
        call read_var('NameModel',NameModel)!t96,t04,MHD,Dip
        !call read_var('UseFixedB',UseFixedB)!T=fixed B config or 
                                            !F=changing B config
        call lower_case( NameModel )
        if (NameModel == 'dip') then
           iMod=0
           ! Checks the next two lines to see if cor and/or pot
           ! variables should be set to 0.
           call read_var('UseCorotation',UseCorotation)
           call read_var('UsePotential',UsePotential)
        elseif (NameModel == 't96') then
           iMod=1
        elseif(NameModel == 't04') then
           iMod=2
        elseif(NameModel == 'mhd')then
           iMod=3
           UseGm=.true.
        else
           call con_stop('Error: Model not found') 
        endif

     case('#IEMODEL')
        call read_var('UseWeimer',UseWeimer)

     case('#GIMME')
        call read_var('UseGimme',UseGimme)
        call read_var('DtGimmePlot',DtGimmePlot)
        
        
     case('#SAVELOG')
        call read_var('DtLogOut',DtLogOut)
        DoSaveLog = .true.

     case('#SAVEPLOT')

        call read_var( 'nCIMIPlotType', nCIMIPlotType )

        CIMI_PLOTTYPE: do iCIMIPlotType = 1, nCIMIPlotType
           
           call read_var( 'StringPlot', StringCIMIPlot )
           call lower_case( StringCIMIPlot )
           call read_var( 'DtOutput', DtOutputCIMIPlot )
           
           if ( index( StringCIMIPlot, 'fls'  ) > 0 .or. &
                index( StringCIMIPlot, 'flux' ) > 0 ) then
              
              call read_var( 'DoSaveSeparateFiles', DoSaveSeparateFiles )
              
              PLOT_FLUX_SPECIES: if &
                   ( index( StringCIMIPlot, 'all' ) > 0 ) then
                 
                 DoSaveFlux( 1 : nspec ) = .true.
                 DtFluxOutput( 1 : nspec ) = DtOutputCIMIPlot
                 DoSaveSeparateFluxFiles( 1 : nspec ) = &
                      DoSaveSeparateFiles
                 
              elseif &
                   ( index( StringCIMIPlot, 'ions' ) > 0 ) then
                 
                 DoSaveFlux( 1 : nspec - 1 ) = .true.
                 DtFluxOutput( 1 : nspec - 1 ) = DtOutputCIMIPlot
                 DoSaveSeparateFluxFiles( 1 : nspec - 1 ) = &
                      DoSaveSeparateFiles
                 
              elseif &
                   ( index( StringCIMIPlot, 'electrons' ) > 0 .or. &
                     index( StringCIMIPlot, 'e' ) > 0 ) then
                 
                 DoSaveFlux( nspec ) = .true.
                 DtFluxOutput( nspec ) = DtOutputCIMIPlot
                 DoSaveSeparateFluxFiles( nspec ) = &
                      DoSaveSeparateFiles
                 
              elseif &
                   ( index( StringCIMIPlot, 'h' ) > 0 ) then
                 
                 DoSaveFlux( 1 ) = .true.
                 DtFluxOutput( 1 ) = DtOutputCIMIPlot
                 DoSaveSeparateFluxFiles( 1 ) = &
                      DoSaveSeparateFiles
                 
              elseif &
                   ( index( StringCIMIPlot, 'o' ) > 0 ) then
                 
                 if ( nspec .ge. 3 ) then
                    
                    DoSaveFlux( 2 ) = .true. 
                    DtFluxOutput( 2 ) = DtOutputCIMIPlot
                    DoSaveSeparateFluxFiles( 2 ) = &
                         DoSaveSeparateFiles
                    
                 else
                    
                    call CON_STOP( 'O+ not configured; Recompile CIMI '// &
                         'with ModEarthHO or ModEarthHOHe options.' )
                    
                 endif
                 
              elseif &
                   ( index( StringCIMIPlot, 'he' ) > 0 ) then
                 
                 if ( nspec .gt. 3 ) then
                    
                    DoSaveFlux( 3 ) = .true. 
                    DtFluxOutput( 3 ) = DtOutputCIMIPlot
                    DoSaveSeparateFluxFiles( 3 ) = &
                         DoSaveSeparateFiles
                    
                 else
                    
                    call CON_STOP( 'He+ not configured; Recompile CIMI '//&
                         'with ModEarthHOHe option.' )
                    
                 endif
                 
              else
                 
                 call CON_STOP( 'No flux species information; STOPPING' )
                 
              endif PLOT_FLUX_SPECIES
              
           elseif &
                ( index( StringCIMIPlot, 'psd' ) > 0 ) then
              
              call read_var( 'DoSaveSeparateFiles', DoSaveSeparateFiles )
              
              PLOT_PSD_SPECIES: if &
                   ( index( StringCIMIPlot, 'all' ) > 0 ) then
                 
                 DoSavePSD( 1 : nspec ) = .true.
                 DtPSDOutput( 1 : nspec ) = DtOutputCIMIPlot
                 DoSaveSeparatePSDFiles( 1 : nspec ) = &
                      DoSaveSeparateFiles
                 
              elseif &
                   ( index( StringCIMIPlot, 'ions' ) > 0 ) then
                 
                 DoSavePSD( 1 : nspec - 1 ) = .true.
                 DtPSDOutput( 1 : nspec - 1 ) = DtOutputCIMIPlot
                 DoSaveSeparatePSDFiles( 1 : nspec - 1 ) = &
                      DoSaveSeparateFiles
                 
              elseif &
                   
                   ( index( StringCIMIPlot, 'electrons' ) > 0 .or. &
                     index( StringCIMIPlot, 'e' ) > 0 ) then
                 DoSavePSD( nspec ) = .true.
                 DtPSDOutput( nspec ) = DtOutputCIMIPlot
                 DoSaveSeparatePSDFiles( nspec ) = &
                      DoSaveSeparateFiles
                 
              elseif &
                   ( index( StringCIMIPlot, 'h' ) > 0 ) then
                 
                 DoSavePSD( 1 ) = .true.
                 DtPSDOutput( 1 ) = DtOutputCIMIPlot
                 DoSaveSeparatePSDFiles( 1 ) = &
                      DoSaveSeparateFiles
                 
              elseif &
                   ( index( StringCIMIPlot, 'o' ) > 0 ) then

                 if ( nspec .ge. 3 ) then
                    DoSavePSD( 2 ) = .true. 
                    DtPSDOutput( 2 ) = DtOutputCIMIPlot
                    DoSaveSeparatePSDFiles( 2 ) = &
                         DoSaveSeparateFiles
                 else
                    
                    call CON_STOP( 'O+ not configured; Recompile CIMI '// &
                         'with ModEarthHO or ModEarthHOHe options.' )
                    
                 endif
                 
              elseif &
                   ( index( StringCIMIPlot, 'he' ) > 0 ) then
                 
                 if ( nspec .gt. 3 ) then
                    
                    DoSavePSD( 3 ) = .true. 
                    DtPSDOutput( 3 ) = DtOutputCIMIPlot
                    DoSaveSeparatePSDFiles( 3 ) = &
                         DoSaveSeparateFiles
                    
                 else
                    
                    call CON_STOP( 'He+ not configured; Recompile CIMI '//&
                         'with ModEarthHOHe option.' )
                    
                 endif
                 
              else
                 
                 call CON_STOP( 'No PSD species information; STOPPING' )
                 
              endif PLOT_PSD_SPECIES
              
           elseif &
                ( index( StringCIMIPlot, 'vl'  ) > 0 .or. &
                  index( StringCIMIPlot, 'vldrift' ) > 0 ) then
              
              call read_var( 'DoSaveSeparateFiles', DoSaveSeparateFiles )
              
              PLOT_VLDRIFT_SPECIES: if &
                   ( index( StringCIMIPlot, 'all' ) > 0 ) then
                 
                 DoSaveVLDrift( 1 : nspec ) = .true.
                 DtVLDriftOutput( 1 : nspec ) = DtOutputCIMIPlot
                 DoSaveSeparateVLDriftFiles( 1 : nspec ) = &
                      DoSaveSeparateFiles
                 
              elseif &
                   ( index( StringCIMIPlot, 'ions' ) > 0 ) then
                 
                 DoSaveVLDrift( 1 : nspec - 1 ) = .true.
                 DtVLDriftOutput( 1 : nspec - 1 ) = DtOutputCIMIPlot
                 DoSaveSeparateVLDriftFiles( 1 : nspec - 1 ) = &
                      DoSaveSeparateFiles
                 
              elseif &
                   ( index( StringCIMIPlot, 'electrons' ) > 0 .or. &
                     index( StringCIMIPlot, 'e' ) > 0 ) then
                 
                 DoSaveVLDrift( nspec ) = .true.
                 DtVLDriftOutput( nspec ) = DtOutputCIMIPlot
                 DoSaveSeparateVLDriftFiles( nspec ) = &
                      DoSaveSeparateFiles
                 
              elseif &
                   ( index( StringCIMIPlot, 'h' ) > 0 ) then
                 
                 DoSaveVLDrift( 1 ) = .true.
                 DtVLDriftOutput( 1 ) = DtOutputCIMIPlot
                 DoSaveSeparateVLDriftFiles( 1 ) = &
                      DoSaveSeparateFiles
                 
              elseif &
                   ( index( StringCIMIPlot, 'o' ) > 0 ) then
                 
                 if ( nspec .ge. 3 ) then
                    
                    DoSaveVLDrift( 2 ) = .true. 
                    DtVLDriftOutput( 2 ) = DtOutputCIMIPlot
                    DoSaveSeparateVLDriftFiles( 2 ) = &
                         DoSaveSeparateFiles
                    
                 else
                    
                    call CON_STOP( 'O+ not configured; Recompile CIMI '// &
                         'with ModEarthHO or ModEarthHOHe options.' )
                    
                 endif
              elseif &
                   ( index( StringCIMIPlot, 'he' ) > 0 ) then
                 
                 if ( nspec .gt. 3 ) then
                    
                    DoSaveVLDrift( 3 ) = .true. 
                    DtVLDriftOutput( 3 ) = DtOutputCIMIPlot
                    DoSaveSeparateVLDriftFiles( 3 ) = &
                         DoSaveSeparateFiles
                    
                 else
                    
                    call CON_STOP( 'He+ not configured; Recompile CIMI '//&
                         'with ModEarthHOHe option.' )
                    
                 endif
                 
              else
                 
                 call CON_STOP( 'No VLDrift species information; STOPPING' )
                 
              endif PLOT_VLDRIFT_SPECIES
              
           elseif &
                ( index( StringCIMIPlot, 'vp'  ) > 0 .or. &
                  index( StringCIMIPlot, 'vpdrift' ) > 0 ) then

              call read_var( 'DoSaveSeparateFiles', DoSaveSeparateFiles )

              PLOT_VPDRIFT_SPECIES: if &
                   ( index( StringCIMIPlot, 'all' ) > 0 ) then

                 DoSaveVPDrift( 1 : nspec ) = .true.
                 DtVPDriftOutput( 1 : nspec ) = DtOutputCIMIPlot
                 DoSaveSeparateVPDriftFiles( 1 : nspec ) = &
                      DoSaveSeparateFiles

              elseif &
                   ( index( StringCIMIPlot, 'ions' ) > 0 ) then

                 DoSaveVPDrift( 1 : nspec - 1 ) = .true.
                 DtVPDriftOutput( 1 : nspec - 1 ) = DtOutputCIMIPlot
                 DoSaveSeparateVPDriftFiles( 1 : nspec - 1 ) = &
                      DoSaveSeparateFiles

              elseif &
                   ( index( StringCIMIPlot, 'electrons' ) > 0 .or. &
                     index( StringCIMIPlot, 'e' ) > 0 ) then

                 DoSaveVPDrift( nspec ) = .true.
                 DtVPDriftOutput( nspec ) = DtOutputCIMIPlot
                 DoSaveSeparateVPDriftFiles( nspec ) = &
                      DoSaveSeparateFiles

              elseif &
                   ( index( StringCIMIPlot, 'h' ) > 0 ) then

                 DoSaveVPDrift( 1 ) = .true.
                 DtVPDriftOutput( 1 ) = DtOutputCIMIPlot
                 DoSaveSeparateVPDriftFiles( 1 ) = &
                      DoSaveSeparateFiles

              elseif &
                   ( index( StringCIMIPlot, 'o' ) > 0 ) then

                 if ( nspec .ge. 3 ) then

                    DoSaveVPDrift( 2 ) = .true.
                    DtVPDriftOutput( 2 ) = DtOutputCIMIPlot
                    DoSaveSeparateVPDriftFiles( 2 ) = &
                         DoSaveSeparateFiles

                 else

                    call CON_STOP( 'O+ not configured; Recompile CIMI '// &
                         'with ModEarthHO or ModEarthHOHe options.' )

                 endif

              elseif &
                   ( index( StringCIMIPlot, 'he' ) > 0 ) then

                 if ( nspec .gt. 3 ) then

                    DoSaveVPDrift( 3 ) = .true.
                    DtVPDriftOutput( 3 ) = DtOutputCIMIPlot
                    DoSaveSeparateVPDriftFiles( 3 ) = &
                         DoSaveSeparateFiles

                 else

                    call CON_STOP( 'He+ not configured; Recompile CIMI '//&
                         'with ModEarthHOHe option.' )

                 endif

              else

                 call CON_STOP( 'No VPDrift species information; STOPPING' )

              endif PLOT_VPDRIFT_SPECIES

           elseif &
                ( index( StringCIMIPlot, 'precipitation'  ) > 0 .or. &
                  index( StringCIMIPlot, 'precip' ) > 0 .or. &
                  index( StringCIMIPlot, 'preci' ) > 0 ) then

              DoCalcPrecip = .true.
              call read_var( 'DoSaveSeparateFiles', DoSaveSeparateFiles )

              PLOT_PRECI_SPECIES: if &
                   ( index( StringCIMIPlot, 'all' ) > 0 ) then

                 DoSavePreci( 1 : nspec ) = .true.
                 DtPreciOutput( 1 : nspec ) = DtOutputCIMIPlot
                 DoSaveSeparatePreciFiles( 1 : nspec ) = &
                      DoSaveSeparateFiles

              elseif &
                   ( index( StringCIMIPlot, 'ions' ) > 0 ) then

                 DoSavePreci( 1 : nspec - 1 ) = .true.
                 DtPreciOutput( 1 : nspec - 1 ) = DtOutputCIMIPlot
                 DoSaveSeparatePreciFiles( 1 : nspec - 1 ) = &
                      DoSaveSeparateFiles

              elseif &
                   ( index( StringCIMIPlot, 'electrons' ) > 0 .or. &
                     index( StringCIMIPlot, 'e' ) > 0 ) then

                 DoSavePreci( nspec ) = .true.
                 DtPreciOutput( nspec ) = DtOutputCIMIPlot
                 DoSaveSeparatePreciFiles( nspec ) = &
                      DoSaveSeparateFiles

              elseif &
                   ( index( StringCIMIPlot, 'h' ) > 0 ) then

                 DoSavePreci( 1 ) = .true.
                 DtPreciOutput( 1 ) = DtOutputCIMIPlot
                 DoSaveSeparatePreciFiles( 1 ) = &
                      DoSaveSeparateFiles

              elseif &
                   ( index( StringCIMIPlot, 'o' ) > 0 ) then

                 if ( nspec .ge. 3 ) then

                    DoSavePreci( 2 ) = .true.
                    DtPreciOutput( 2 ) = DtOutputCIMIPlot
                    DoSaveSeparatePreciFiles( 2 ) = &
                         DoSaveSeparateFiles

                 else

                    call CON_STOP( 'O+ not configured; Recompile CIMI '// &
                         'with ModEarthHO or ModEarthHOHe options.' )

                 endif

              elseif &
                   ( index( StringCIMIPlot, 'he' ) > 0 ) then

                 if ( nspec .gt. 3 ) then

                    DoSavePreci( 3 ) = .true.
                    DtPreciOutput( 3 ) = DtOutputCIMIPlot
                    DoSaveSeparatePreciFiles( 3 ) = &
                         DoSaveSeparateFiles

                 else

                    call CON_STOP( 'He+ not configured; Recompile CIMI '//&
                         'with ModEarthHOHe option.' )

                 endif

              else

                 call CON_STOP( 'No Preci species information; STOPPING' )

              endif PLOT_PRECI_SPECIES

           elseif &
                ( index( StringCIMIPlot, '2d'  ) > 0 ) then

              PLOT_2D: if &
                   ( index( StringCIMIPlot, 'all'  ) > 0 .or. &
                     index( StringCIMIPlot, 'both'  ) > 0 ) then

                 DoSaveEq = .true.
                 DoSaveIono = .true.
                 DtOutput = DtOutputCIMIPlot
                 
              elseif &
                   ( index( StringCIMIPlot, 'equator' ) > 0 .or. &
                     index( StringCIMIPlot, 'eq' ) > 0 ) then
                 
                 DoSaveEq = .true.
                 DtOutput = DtOutputCIMIPlot
                 
              elseif &
                   ( index( StringCIMIPlot, 'ionosphere' ) > 0 .or. &
                     index( StringCIMIPlot, 'iono' ) > 0 ) then

                 DoSaveIono = .true.
                 DtOutput = DtOutputCIMIPlot
                 
              elseif &
                   ( index( StringCIMIPlot, 'lstar'  ) > 0 .or. &
                     index( StringCIMIPlot, 'l*'  ) > 0 ) then
                 
                 call read_var( 'DoSaveSeparateFiles', DoSaveSeparateFiles )
                 
                 DoSaveLstar = .true.
                 DtLstarOutput = DtOutputCIMIPlot
                 DoSaveSeparateLstarFiles = DoSaveSeparateFiles
                 
              elseif &
                   ( index( StringCIMIPlot, 'coreplas' ) > 0 ) then
                 
                 DoSavePlas = .true.
                 DtPlasOutput = DtOutputCIMIPlot
                 
              else

                 call CON_STOP( 'No CIMI 2D Plot information; STOPPING' )
                 
              endif PLOT_2D

           else
              
              call CON_STOP( 'No plot type specified.' )
              
           endif

        end do CIMI_PLOTTYPE

        DoSavePlot = .true.

!!  END OF SAVEPLOT ROUTINE        

     case('#DTSATOUT')
        call read_var('DtSatOut', DtSatOut)
        
     case('#VERBOSELSTAR')
        call read_var('DoVerboseLstar',DoVerboseLstar)
        
     case('#INITIALF2')
        call read_var('IsEmptyInitial',IsEmptyInitial)
        call read_var('IsGmInitial',   IsGmInitial)
        call read_var('IsDataInitial', IsDataInitial)
        if (IsDataInitial) &
             call read_var('IsRBSPData', IsRBSPData)
        
        !IsDataInitial only works with EarthHO or EarthH configurations
        if (nspec > 3 .and. IsDataInitial) &
             call CON_STOP('IsDataInitial only works with EarthHO or EarthH')  

     case('#INITIALLSTAR')
        call read_var('DoLstarInitialization',DoLstarInitialization)
        
     case('#TYPEBOUNDARY')
        call read_var('TypeBoundary',TypeBoundary)
        if(TypeBoundary == 'Ellipse') then
           UseEllipse = .true.
        else
           UseEllipse = .false.
        endif
        
     case('#PLASMASHEET')
        call read_var('UseYoungEtAl',UseYoungEtAl)
        if(IsStandAlone)then
           ! When Standalone specify boundary type
           call read_var('UseBoundaryEbihara',UseBoundaryEbihara)
        endif
        
     case('#RESTART')
        call read_var('IsRestart',IsRestart) !T:Continuous run, F: Initial Run
        if(IsRestart) call read_var('ReadRestartSat',ReadRestartSat)
        
     case('#SAVERESTART')
        ! when in standalone mode read restart save frequency
        if(IsStandAlone) then
           call read_var('DtSaveRestart',DtSaveRestart)
        else
           call CON_STOP('ERROR: Restart save frequency only set in standalone')
        end if

     case('#LIMITER')
        call read_var('UseMcLimiter', UseMcLimiter)
        if(UseMcLimiter) call read_var('BetaLimiter', BetaLimiter)

     ! minimum pressure in nPa passed to GM
     case('#MINIMUMPRESSURETOGM')   
        call read_var('MinimumPressureToGM', Pmin)

     ! Ratio of bulk pressure from GM that is given to electron pressure
     case('#ELECTRONTEMPERATURERATIO')
         call read_var('ElectronTempRatio', ElectronTempRatio)
         do iSpec = 1, nspec - 1
            tFactor_I(iSpec) = 1/(1 + ElectronTempRatio)
         end do
         tFactor_I(nspec) = ElectronTempRatio/(1 + ElectronTempRatio)

     ! Temperature ratios used when setting outer boundary pressures from GM
     case('#GMTEMPERATUREFACTORS')
         do iSpec = 1, nspec
            call read_var('tFactor', tFactor_I(iSpec))
         end do
        
     case('#TIMESIMULATION')
        call read_var('TimeSimulation',time)

     case('#PRERUNSAT')
        call read_var('DoWritePrerunSat',DoWritePrerunSat)
        if(.not.DoWritePrerunSat) call read_var('UsePrerunSat',UsePrerunSat)
        if(UsePrerunSat) then
           call read_var('DtReadSat',   DtReadSat)
           DtSatout = DtReadSat
           DoWriteSats = .true.
        end if
        
     case('#PRERUNFIELD')
        call read_var('DoWritePrerun',DoWritePrerun)
        if(.not.DoWritePrerun) call read_var('UsePrerun',UsePrerun)
        if(UsePrerun)          call read_var('DtRead',   DtRead)
        if(UsePrerun .or. DoWritePrerun) UseGm=.true.
     case('#DECAY')
        call read_var('UseDecay',UseDecay)
        if ( UseDecay ) &
             call read_var('DecayTimescale in seconds', DecayTimescale)
     case('#ELECTRONDECAY')
        call read_var('UseElectronDecay', UseElectronDecay)

     case('#FLC')
        call read_var('UseFLC',UseFLC)
        
     case('#COMPOSITION')
        call read_var('TypeComposition', TypeComposition, IsUpperCase=.true.)
        select case(TypeComposition)
        case('FIXED')
           do iSpec = 1, nSpec
              call read_var('DensityFraction', dFactor_I(iSpec))
           end do
        case default
           call CON_stop(NameSub//': unknown TypeComposition='//TypeComposition)
        end select

        
     case('#STRONGDIFFUSION')
        call read_var('UseStrongDiff',UseStrongDiff)
        
     case('#WAVES')
        call read_var('UseWaves',UseWaves)
        if(UseWaves) then
           call read_var('UseHiss',  UseHiss)
           call read_var('UseChorus',  UseChorus)
           call read_var('NameHissFile', NameHissFile)
           call read_var('NameChorusFile', NameChorusFile)
           call read_var('UseKpIndex',UseKpIndex)
        end if

     case('#DIAGONALIZEDDIFFUSION')
        call read_var('UseDiagDiffusion',UseDiagDiffusion)

        
     case('#DIAGDIFFUSIONTEST')
        call read_var('UsePitchAngleDiffusionTest',&
                       UsePitchAngleDiffusionTest)
        call read_var('UseEnergyDiffusionTest',&
                       UseEnergyDiffusionTest)

     case('#ENERGYGRID')
        call read_var('MinIonEnergy (in keV), MinElectronEnergy x10',&
             MinIonEnergy)
        call read_var('MaxIonEnergy (in keV), MaxElectronEnergy x10',&
             MaxIonEnergy)
        
     case('#SETENERGYGRID')
        call read_var('neng', neng)
        if (neng .lt. 2) &
             call CON_STOP('ERROR: neng must be greater than 1.')
        call read_var('UseLogEGrid', UseLogEGrid)
        call read_var('MinIonEnergy (in keV), MinElectronEnergy x10',&
             MinIonEnergy)
        call read_var('MaxIonEnergy (in keV), MaxElectronEnergy x10',&
             MaxIonEnergy)

     case('#RBSPENERGYGRID')
        call read_var('UseRBSPGrid', UseRBSPGrid)
        if (UseRBSPGrid) neng = nRBSPEnergy

     case('#ELECTRONGRIDRATIO')
        call read_var('ElectronGridRatio', ElectronGridRatio)
        
     case('#IMTIMESTEP')
        call read_var('IMDeltaT [s]',dt)
        call read_var('IMDeltaTMax [s]',dtmax)

     case('#TESTFIELDLINE')
        call read_var('iLatTest',iLatTest)
        call read_var('iLonTest',iLonTest)
        
     case('#PLASMAPAUSEDENSITY')
        call read_var('DensityP [m^3]',PlasmaPauseDensity)

     case('#COREPLASMASPHERE')
        call read_var('UseCorePsModel',UseCorePsModel)
        call read_var('PlasMinDensity',PlasMinDensity)
        
     case('#SETRB')
        call read_var('rb [R_E]', rb)
     case('#SETBOUNDARYPARAMS')
        call read_var('DeltaRMax', DeltaRMax)
        call read_var('DeltaMLTmax', xmltlim)
       
    case('#PRECIPITATION')
       call read_var('UseAltitudePrecip', UseAltitudePrecip)
       if(UseAltitudePrecip) &
          call read_var('UsePrecipEnergyLoss', UsePrecipEnergyLoss)

     case('#STRICTDRIFT')
        call read_var('IsStrictDrift',IsStrictDrift) ! .T : STOP when f2 < 0

     case('#LATITUDINALGRID')
        call read_var('DoDefineVarNpower',DoDefineVarNpower) 
        call read_var('varNpower',varNpower)   ! n in L = 1/cos(xlat)**n
        call read_var('xlatmax',xlatmax)       ! upper boundary latitude

     case('#VERBOSELATGRID')
        call read_var( 'DoVerboseLatGrid', DoVerboseLatGrid )

     case('#HIGHERORDERDRIFT')   ! use higher order drift
        call read_var('UseHigherOrder',UseHigherOrder)
        if ( UseHigherOrder ) then
           
           call read_var( 'iOrderLat', iOrderLat )   ! order in latitude
           call read_var( 'iOrderLon', iOrderLon )   ! order in longitude
           if ( iOrderLat .eq. iOrderLon ) then
              if ( ( iOrderLat .ne. 2 ) .and. ( iOrderLat .ne. 7 ) ) &
                   call CON_STOP('IM: ERROR: iOrderLat = iOrderLon = 2 '//&
                   	'or iOrderLat = iOrderLon = 7')
           else
              call CON_STOP('IM: ERROR: iOrderLat = iOrderLon = 2 '//&
                   'or iOrderLat = iOrderLon = 7')
           endif
           
        endif

     case('#DRIFTSCHEME')   ! use higher order drift
        call read_var( 'iOrderLat', iOrderLat )   ! order in latitude
        call read_var( 'iOrderLon', iOrderLon )   ! order in longitude
        if ( iOrderLat .eq. iOrderLon ) then
           if ( iOrderLat .eq. 7 ) UseHigherOrder=.true.
           if ( ( iOrderLat .ne. 2 ) .and. ( iOrderLat .ne. 7 ) ) &
                call CON_STOP('IM: ERROR: iOrderLat = iOrderLon = 2 '//&
                'or iOrderLat = iOrderLon = 7')
        else
           call CON_STOP('IM: ERROR: iOrderLat = iOrderLon = 2 '//&
                'or iOrderLat = iOrderLon = 7')
        endif
        write(*,*) "UseHigherOrder: ", UseHigherOrder

     end select
     
  enddo

end subroutine CIMI_set_parameters
