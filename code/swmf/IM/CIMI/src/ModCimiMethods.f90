module ModCimiMethods

    implicit none

    private ! except

    public:: cimi_init
    public:: cimi_run

contains
subroutine cimi_run(delta_t)

  ! The main cimi code to update by one delta_t

  use ModConst,	ONLY: cLightSpeed, cElectronCharge
  use ModCimiInitialize, ONLY:	&
       xmm, xk, dphi, dmm, dk, dmu, xjac,&
       DoLstarInitialization
  use ModCimi, ONLY:	&
       f2, dt, dtmax,Time, phot, Ppar_IC, Pressure_IC, &
       PressurePar_IC, FAC_C, Bmin_C, &
       OpDrift_, OpBfield_, OpChargeEx_, &
       OpWaves_, OpStrongDiff_, OpLossCone_, &
       OpDecay_, OpFLC_, UseDecay, UseFLC, &
       rbsumLocal, rbsumGlobal, &
       rcsumLocal, rcsumGlobal, &
       driftin, driftout, IsStandAlone, &
       energy, Ebound, delE, &
       preP, preF, Eje1, UseStrongDiff, &
       eChangeOperator_VICI, nOperator, &
       eChangeLocal, eChangeGlobal, &
       DoCalcPrecip, DtCalcPrecip, &
       vdr_q3, eng_q3, vexb, dif_q3, Part_phot, &
       achar, vl, vp, fb, vlEa, vpEa, flux, psd
  use ModCimiPlanet, ONLY: &
       re_m, dipmom, Hiono, rc, nspec, amu_I, dFactor_I, tFactor_I
  use ModCimiTrace, ONLY: &
       fieldpara, &
       brad => ro, ftv => volume, xo, yo, rb, irm, &
       ekev, iba, bo, pp, Have, sinA, vel, alscone, iw2, xmlto, bm, phi2o, &
       gather_field_trace, bcast_field_trace, ionization
  use ModGmCimi, ONLY: Den_IC, UseGm, UseGmKp, KpGm, UseGmAe, AeGm
  use ModIeCimi, ONLY: UseWeimer, pot
  use ModCimiPlot
  use ModCimiRestart, ONLY: IsRestart
  use ModImTime
  use ModTimeConvert, ONLY: time_real_to_int
  use ModImSat,	ONLY: &
       nImSats, write_im_sat, DoWriteSats, DtSatOut
  use ModCimiGrid, ONLY:	&
       iProc, nProc, iComm, nLonPar, nLonPar_P, nLonBefore_P, &
       MinLonPar, MaxLonPar, nt, np, neng, npit, nm, nk, dlat, &
       phi, sinao, xlat, xlatr, xmlt
  use ModCimiBoundary, ONLY:	&
       cimi_set_boundary_mhd, cimi_set_boundary_empirical,&
       CIMIboundary, Outputboundary
  use ModMpi
  use CIMI_waves, ONLY:	&
       UseWaves, UseKpIndex, ReadDiffCoef, WavePower
  use ModImIndices, ONLY:   interpolate_ae,&
       UseAeKyoto,UseKpApF107IndicesFile,get_im_indices_Kp
  use ModCoupleSami, ONLY:	DoCoupleSami
  use ModIndicesInterfaces
  use ModLstar,	ONLY:	&
       Lstar_C, Lstarm, Lstar_max, Lstarm_max, &
       calc_Lstar1, calc_Lstar2
  use ModPlasmasphere, ONLY:	&
       UseCorePsModel, PlasSpinUpTime, init_plasmasphere, &
       advance_plasmasphere, DoSavePlas, DtPlasOutput,&
       PlasDensity_C, save_plot_plasmasphere, &
       cimi_put_to_plasmasphere,PlasMinDensity
  use ModDiagDiff, ONLY:	&
       UseDiagDiffusion, calc_DQQ, interpol_D_coefK, &
       mapPSDtoQ, mapPSDtoE, diffuse_Q1, diffuse_Q2, &
       UsePitchAngleDiffusionTest,&
       UseEnergyDiffusionTest, init_diag_diff
  use ModWaveDiff, ONLY:   &
       diffuse_aa, diffuse_EE, diffuse_aE
  use ModCurvScatt, ONLY: calc_FLC_para,FLC_loss
  use ModUtilities, ONLY: CON_stop
  use GIMME_cimi_interface, ONLY: &
       UseGimme, init_gimme_from_cimi, gimme_potential_to_cimi

  implicit none

  ! regular variables
  integer:: n, nstep
  integer, save :: ib0(nt)
  real:: delta_t
  integer:: iLat, iLon, iSpecies, iSat, iOperator
  logical:: IsFirstCall =.true.
  real::  AE_temp = 0., Kp_temp = 0.
  integer:: iError

  real, allocatable:: ekevSEND_IIII(:,:,:,:),ekevRECV_IIII(:,:,:,:)
  real, allocatable:: sinaSEND_III(:,:,:),sinaRECV_III(:,:,:)
  real, allocatable:: F2SEND_IIIII(:,:,:,:,:),f2RECV_IIIII(:,:,:,:,:)

  integer:: tmp_I(6)
  !----------------------------------------------------------------------------
  dt=dtmax
  if (dt==0) then
     nstep = 0
     dt = 0.0
  else
     nstep=nint(delta_t/dt)
     dt=delta_t/nstep         ! new dt
  endif

  ! Update CurrentTime and iCurrentTime_I
  CurrentTime = StartTime+Time
  call time_real_to_int(CurrentTime,iCurrentTime_I)

  ! do field line integration and determine vel, ekev, momentum (pp), etc.
  call timing_start('cimi_fieldpara')
  call fieldpara(Time,dt,cLightSpeed,cElectronCharge,xlat,xmlt,phi,xk,&
       IsRestart)
  call timing_stop('cimi_fieldpara')

  if(UseFLC) then
     call timing_start('cimi_calc_FLC_para')
     do iLon =MinLonPar, MaxLonPar
        do iLat = 1,iba(iLon)
           call calc_FLC_para(iLat,iLon)
        enddo
     enddo
     call timing_stop('cimi_calc_FLC_para')
  endif
  
  ! Checks to see if this is the first call of CIMI, so as to gather
  ! all the relevant information to initialize CIMI f2 as a function
  ! of L*.  Necessary variables are bo and bm in ModFieldTrace.
  if ( IsFirstCall ) then

     ! Gather Field Trace variables to root for calculating Lstar
     if (nProc > 1 ) then
        call gather_field_trace
     endif

     ! Allocate lstar variables on all processors
     if(.not. allocated(Lstar_C)) allocate(Lstar_C(np,nt), Lstar_max, &
          Lstarm(np,nt,nk), Lstarm_max(nk))

     ! Trace lstar variables for initial_f2.
     if ( iProc == 0 ) then

        call timing_start('calc_Lstar1')
        call calc_Lstar1
        call timing_stop('calc_Lstar1')

        call timing_start('calc_Lstar2')
        call calc_Lstar2
        call timing_stop('calc_Lstar2')

     endif

     ! Broadcast Lstar variable to all PEs
     if (nProc > 1 ) then
        call MPI_bcast(Lstar_C,np*nt,MPI_REAL,0,iComm,iError)
        call MPI_bcast(Lstarm,np*nt*nk,MPI_REAL,0,iComm,iError)
     endif

  endif

  ! interpolate a0 of const. Q2 curve correspoding K
  if ( .NOT.( IsFirstCall ) .AND. &
       UseWaves .and. UseDiagDiffusion ) then
     call init_diag_diff
     call timing_start('cimi_interpol_D_coefK')
     call interpol_D_coefK
     call timing_stop('cimi_interpol_D_coefK')
  endif

  ! when using 2D plasmasphere
  if (UseCorePsModel .and. IsFirstCall) then
     ! gather info needed for core plasmasphere model
     call core_ps_gather
     if (iProc==0)then
        ! initial the 2d core plasmasphere model
        call init_plasmasphere(np,nt,xlatr,phi,iba,&
             brad,phi2o,ftv,pot,IsRestart)
        write(*,*) 'CIMI: IsRestart',IsRestart
        if (.not.IsRestart) then
           ! spin up the core plasmasphere model by running for one day
           call advance_plasmasphere(PlasSpinUpTime)
        endif
     else
        ! for iProc>0 make sure PlasDensity_C is allocated for broadcast
        if (allocated(PlasDensity_C)) deallocate(PlasDensity_C)
        allocate(PlasDensity_C(np,nt))

     endif
  endif
  if (UseCorePsModel .and. nProc>1) then
     call MPI_bcast(PlasDensity_C,np*nt,MPI_REAL,0,iComm,iError)
  endif
!  if (UseCorePsModel) then
!     !when using the core PS model overwrite the density in the
!     !wave calculation
!     where (PlasDensity_C > PlasMinDensity)
!        density=PlasDensity_C
!     elsewhere
!        density=PlasMinDensity
!     end where
!  endif
  
  ! get Bmin, needs to be passed to GM for anisotropic pressure coupling
  Bmin_C = bo

  ! set the boundary and temperature and density (also sets the interior
  ! density and temperature for I.C. but that is only if initial_f2 is called)
  if(.not.UseGm) then
     call cimi_set_boundary_empirical
  else
     call cimi_set_boundary_mhd
  endif

  ! Bcast DoWriteSats on firstcall
  if (IsFirstCall .and. nProc > 1) then
     call MPI_bcast(DoWriteSats,1,MPI_LOGICAL,0,iComm,iError)
  endif

  if (UseWaves) then
     
     ! calculate wave power for the first time; the second time is in the loop
     if (UseKpIndex) then
        if (UseGmKp) then
           Kp_temp=KpGm
        else
           if (UseKpApF107IndicesFile) then
              call get_im_indices_Kp(CurrentTime, Kp_temp)
           else
              call get_kp(CurrentTime, Kp_temp, iError)
           endif
        endif
     else
        if (UseGmAe) then
           AE_temp=AeGm
        elseif(UseAeKyoto) then
           call interpolate_ae(CurrentTime, AE_temp)
        else
           call CON_stop('IM error: Kp not used and no AE option.'//&
                'One of the two is needed for waves.')
        endif
     endif
     ! Determines if the simple plasmasphere model needs to be used.
     if ( .not. DoCoupleSami .and. .not. UseCorePsModel) then
        if (UseGmKp) then
           Kp_temp=KpGm
        else
           if (UseKpApF107IndicesFile) then
              call get_im_indices_Kp(CurrentTime, Kp_temp)
           else
              call get_kp(CurrentTime, Kp_temp, iError)
           endif
        endif
        call timing_start('simple_plasmasphere')
        call simple_plasmasphere(Kp_temp)
        call timing_stop('simple_plasmasphere')
     end if

     
     !  read wave diffcoef 
     if (IsFirstCall) call ReadDiffCoef(np,nt)
     
     call WavePower(np,nt,Time,PlasDensity_C,AE_temp,Kp_Temp, &
          Bo,brad,xmlto,iba,MinLonPar,MaxLonPar)

     if (IsFirstCall .and. UseDiagDiffusion) then
        call init_diag_diff
        call calc_DQQ
     endif
  end if


  
 
  !initialize the potential solver
  if(IsFirstCall .and. UseWeimer .and. UseGimme) then
     if(iProc==0) call init_gimme_from_cimi(iStartTime_I, np, nt, xlatr, phi)
  endif

  ! setup initial distribution
  if (IsFirstCall .and. .not.IsRestart) then
     ! set initial state when no restarting
     call initial_f2(nspec,np,nt,iba,amu_I,vel,xjac,ib0)
     call initial_extra
     IsFirstCall=.false.
  elseif(IsFirstCall .and. IsRestart) then
     ib0(:)=iba(:)
     IsFirstCall=.false.
  endif

  ! Calculate rbsumLocal and Global on first time and get energy
  ! contribution from Bfield change
  call sume_cimi(OpBfield_)

  ! calculate boundary flux (fb) at the CIMI outer boundary at the equator
  call boundaryIM(nspec,neng,np,nt,nm,nk,iba,irm,amu_I,xjac,energy,vel,fb)
  

  ! calculate the ionospheric potential (if not using MHD potential)
  if (UseWeimer) then
     call timing_start('set_cimi_potential')
     call set_cimi_potential(CurrentTime) 
     call timing_stop('set_cimi_potential')

     if (iProc==0 .and. UseGimme) &
          call gimme_potential_to_cimi(Time, np, nt, pot, FAC_C, iba,delta_t)
     if (nProc>1 .and. UseGimme) &
          call MPI_bcast(pot,np*nt,MPI_REAL,0,iComm,iError)

  endif

  ! calculate the drift velocity
  call timing_start('cimi_driftV')
  call driftV(nspec,np,nt,nm,nk,irm,re_m,Hiono,dipmom,dphi,xlat,dlat, &
       ekev,pot,vl,vp)
  call timing_stop('cimi_driftV')

  ! calculate the depreciation factor, achar, due to charge exchange loss
  call timing_start('cimi_ceparaIM')
  call ceparaIM(nspec,np,nt,nm,nk,irm,dt,vel,ekev,Have,achar)
  call timing_stop('cimi_ceparaIM')

  if (UseStrongDiff) then
     ! Calculate the strong diffusion lifetime for electrons
     call timing_start('cimi_StDiTime')
     call StDiTime(dt,vel,ftv,iba)
     call timing_stop('cimi_StDiTime')
  endif

  ! save the initial point (needs to be fixed and so not called)
  if ( Time == 0.0 .and. DoSavePlot ) then
     call timing_start('cimi_output')
     call cimi_output( &
          np, nt, nm, nk, nspec, neng, npit, iba, ftv, f2, ekev, &
          sinA, energy, sinAo, delE, dmu, amu_I, xjac, pp, xmm, dmm, &
          dk, xlat, dphi, re_m, Hiono, vl, vp, flux, FAC_C, phot, &
          Ppar_IC, Pressure_IC, PressurePar_IC, vlEa, vpEa, psd,brad,xmlto )
     call timing_stop('cimi_output')

     if ( nProc > 1 ) call cimi_gather( delta_t, psd, flux, vlea, vpea )

     if ( iProc == 0 ) then

        if ( DoSaveEq ) then

           call timing_start('cimi_plot_eq')
           call Cimi_plot_eq( np, nt, xo, yo, &
                Pressure_IC, PressurePar_IC, phot, Ppar_IC, Den_IC,&
                bo, ftv, pot, FAC_C, Time, dt )
           call timing_stop('cimi_plot_eq')

        endif

        if ( DoSaveIono ) then

           call timing_start('cimi_plot_iono')
           call Cimi_plot_iono( np, nt, xo, yo, &
                Pressure_IC, PressurePar_IC, phot, Ppar_IC, Den_IC,&
                bo, ftv, pot, FAC_C, Time, dt )
           call timing_stop('cimi_plot_iono')

        endif

        if ( DoSaveLog ) then

           call timing_start('cimi_plot_log')
           call cimi_plot_log(Time)
           call timing_stop('cimi_plot_log')

        endif

        if ( DoSaveLstar ) then

           call Cimi_plot_Lstar(xk,time)

        endif

        if ( DoSavePlas .and. UseCorePsModel) then
           call save_plot_plasmasphere(time,floor(real(time)/dtmax),IsRestart)
        endif

        do iSpecies = 1, nspec

           if ( DoSaveFlux( iSpecies ) ) &
                call Cimi_plot_fls( flux( iSpecies, :, :, :, : ), &
                iSpecies, time )
           if ( DoSavePSD( iSpecies ) ) &
                call Cimi_plot_psd( psd( iSpecies, :, :, :, : ), &
                iSpecies, time, xmm, xk )
           if ( DoSaveVLDrift( iSpecies ) ) &
                call Cimi_plot_vl( vlEa( iSpecies, :, :, :, : ), &
                iSpecies, time)
           if ( DoSaveVPDrift( iSpecies ) ) &
                call Cimi_plot_vp( vpEa( iSpecies, :, :, :, : ), &
                iSpecies, time)

        enddo

     endif

  endif

  ! time loop
  do n=1,nstep

     ! when using 2D plasmasphere
     if (UseCorePsModel) then
        ! gather variable for core plasmasphere
        call core_ps_gather

        if (iProc==0) then
           ! update cimi values in the plasmasphere module
           call cimi_put_to_plasmasphere(np,nt,brad,phi2o,ftv,&
                pot,iba)

           ! advance plasmasphere by dt
           call advance_plasmasphere(dt)
        endif
     endif

     if (nProc>1 .and. UseCorePsModel) &
          call MPI_bcast(PlasDensity_C,np*nt,MPI_REAL,0,iComm,iError)

     !if (UseCorePsModel) then
     !   !when using the core PS model overwrite the density in the
     !   !wave calculation
     !   where (PlasDensity_C > PlasMinDensity)
     !      density=PlasDensity_C
     !   elsewhere
     !      density=PlasMinDensity
     !   end where
     !endif

     call timing_start('cimi_driftIM')
     call driftIM(iw2,nspec,np,nt,nm,nk,dt,dlat,dphi,brad,rb,vl,vp, &
          fb,f2,driftin,driftout,ib0)
     call sume_cimi(OpDrift_)
     call timing_stop('cimi_driftIM')

     call timing_start('cimi_charexchange')
     call charexchangeIM(np,nt,nm,nk,nspec,iba,achar,f2)
     call sume_cimi(OpChargeEx_)
     call timing_stop('cimi_charexchange') 
     
     if ( UseWaves ) then
        call timing_start('cimi_WaveDiffusion')

        if (UsePitchAngleDiffusionTest.or.&
             UseEnergyDiffusionTest) then
           write(*,*) 'UsePitchAngleDiffusionTest',UsePitchAngleDiffusionTest
           write(*,*) 'UseEnergyDiffusionTest',UseEnergyDiffusionTest
           call CON_stop('IM ERROR: For diag diffusion test, "make DIFFUSIONTEST"')
        endif

        if ( UseDiagDiffusion ) then

           call init_diag_diff

           ! map PSD from M to Q2
           call timing_start('cimi_mapPSDtoQ')
           call mapPSDtoQ
           call timing_stop('cimi_mapPSDtoQ')

           ! calculate diffusion in Q2 at fixed Q1
           call timing_start('cimi_Diffuse_Q2')
           call diffuse_Q2
           call timing_stop('cimi_Diffuse_Q2')

           ! calculate diffusion in Q1 at fixed Q2
           call timing_start('cimi_Diffuse_Q1')
           call diffuse_Q1
           call timing_stop('cimi_Diffuse_Q1')

           ! map back PSD from Q2 to M
           call timing_start('cimi_mapPSDtoE')
           call mapPSDtoE
           call timing_stop('cimi_mapPSDtoE')
        else
           call timing_start('cimi_Diffuse_aa')
           call diffuse_aa(f2,dt,xjac,iba,iw2)
           call timing_stop('cimi_Diffuse_aa')

           call timing_start('cimi_Diffuse_EE')
           call diffuse_EE(f2,dt,xmm,xjac,iw2,iba)
           call timing_stop('cimi_Diffuse_EE')
        endif

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!                                                  !!!
!!!     THIS NEEDS TO BE UNCOMMENTED ONCE            !!!
!!!     CROSS-DIFFUSION IS FIXED.  :::COLIN:::       !!!
!!!                                                  !!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

!!$        call timing_start('cimi_Diffuse_aE')
!!$        call diffuse_aE(f2,dt,xjac,iw2,iba,Time)
!!$        call timing_stop('cimi_Diffuse_aE')

        call sume_cimi(OpWaves_)
        call timing_stop('cimi_WaveDiffusion')

        if (.not.UseKpIndex) then
            if (UseGmAe) then
                AE_temp=AeGm
            else
                call interpolate_ae(CurrentTime, AE_temp)
            endif
        endif

        if ( .not. DoCoupleSami .and. .not. UseCorePsModel) then
           if (UseGmKp) then
              Kp_temp=KpGm
           else
              if (UseKpApF107IndicesFile) then
                 call get_im_indices_Kp(CurrentTime, Kp_temp)
              else
                 call get_kp(CurrentTime, Kp_temp, iError)
              endif
           endif
           call timing_start('simple_plasmasphere')
           call simple_plasmasphere(Kp_temp)
           call timing_stop('simple_plasmasphere')
        end if
        
        call WavePower(np,nt,Time,PlasDensity_C,AE_temp,Kp_Temp, &
          Bo,Brad,xmlto,iba,MinLonPar,MaxLonPar)
     endif

     if ( UseStrongDiff ) then

        call timing_start('cimi_StrongDiff')
        call StrongDiff(iba)
        call sume_cimi(OpStrongDiff_)
        call timing_stop('cimi_StrongDiff')

     endif

     if ( UseDecay ) then

        call timing_start('cimi_Decay')
        call CalcDecay_cimi(dt)
        call sume_cimi(OpDecay_)
        call timing_stop('cimi_Decay')

     endif

     call timing_start('cimi_lossconeIM')
     call lossconeIM(iba,alscone,f2,ionization,ekev)
     call sume_cimi(OpLossCone_)
     call timing_stop('cimi_lossconeIM')

     if (UseFLC) then
        call timing_start('cimi_FLC_loss')
        call FLC_loss(Dt)
        call sume_cimi(OpFLC_)
        call timing_stop('cimi_FLC_loss')
     endif
     
     
     Time = Time+dt
     ! Update CurrentTime and iCurrentTime_I
     CurrentTime = StartTime+Time
     call time_real_to_int(CurrentTime,iCurrentTime_I)
  enddo

  ! calculate precipitations accumulated over some time interval DtPreCalc
  if( DoCalcPrecip .and. &
       ( floor( ( Time + 1.0e-5 ) / DtCalcPrecip ) ) /= &
       floor( ( Time + 1.0e-5 - delta_t ) / DtCalcPrecip ) ) then
     call timing_start('cimi_precip_calc')
     call cimi_precip_calc( DtCalcPrecip )
     call timing_stop('cimi_precip_calc')
  endif

  call timing_start('cimi_output')
  call cimi_output( &
       np, nt, nm, nk, nspec, neng, npit, iba, ftv, f2, ekev, &
       sinA, energy, sinAo, delE, dmu, amu_I, xjac, pp, xmm, dmm, &
       dk, xlat, dphi, re_m, Hiono, vl, vp, flux, FAC_C, phot, &
       Ppar_IC, Pressure_IC, PressurePar_IC, vlEa, vpEa, psd,brad,xmlto )
  call timing_stop('cimi_output')

  ! When nProc >1 gather all relevant output variables.
  if ( nProc > 1 ) call cimi_gather( delta_t, psd, flux, vlea, vpea )

  if ( iProc == 0 ) then

     call timing_start('calc_Lstar1')
     call calc_Lstar1
     call timing_stop('calc_Lstar1')

     ! Plot CIMI parameters at the equator
     if ( ( DoSaveEq ) .and. &
          ( floor( ( Time + 1.0e-5 ) / DtEqOutput ) /= &
            floor( ( Time + 1.0e-5 - delta_t ) / DtEqOutput ) ) ) &
     then
        
        call timing_start('cimi_plot_eq')
        call Cimi_plot_eq( np, nt, xo, yo, &
             Pressure_IC, PressurePar_IC, phot, Ppar_IC, &
             Den_IC, bo, ftv, pot, FAC_C, Time, dt )
        call timing_stop('cimi_plot_eq')

     endif

     ! Plot CIMI parameters at the ionosphere
     if ( ( DoSaveIono ) .and. &
          ( floor( ( Time + 1.0e-5 ) / DtIonoOutput ) /= &
            floor( ( Time + 1.0e-5 - delta_t ) / DtIonoOutput ) ) ) &
     then
        
        call timing_start('cimi_plot_iono')
        call Cimi_plot_iono( np, nt, xo, yo, &
             Pressure_IC, PressurePar_IC, phot, Ppar_IC, &
             Den_IC, bo, ftv, pot, FAC_C, Time, dt )
        call timing_stop('cimi_plot_iono')

     endif

     if ( DoSaveLstar .and. &
           ( floor( ( Time + 1.0e-5 ) / DtLstarOutput ) /= &
             floor( ( Time + 1.0e-5 - delta_t ) / DtLstarOutput ) ) ) &
     then
        
        call timing_start('calc_Lstar2')
        call calc_Lstar2
        call timing_stop('calc_Lstar2')

        call Cimi_plot_Lstar( xk, time )

     endif

     if ( DoSavePlas .and. UseCorePsModel .and. iProc==0) then
        call save_plot_plasmasphere(time,floor(real(time)/dtmax),IsRestart)
     endif

     do iSpecies = 1, nspec

        ! Output Species' Flux
        if ( DoSaveFlux( iSpecies ) .and. &
             ( floor( ( Time + 1.0e-5 ) / &
             DtFluxOutput( iSpecies ) ) /= &
             floor( ( Time + 1.0e-5 - delta_t ) / &
             DtFluxOutput( iSpecies ) ) ) ) then
           !write(*,*) iSpecies,DoSaveFlux(iSpecies),DtFluxOutput(iSpecies),iProc,time
           call Cimi_plot_fls( flux( iSpecies, :, :, :, : ), &
             	iSpecies, time )
        endif
        
        ! Output Species' PSD
        if ( DoSavePSD( iSpecies ) .and. &
             ( floor( ( Time + 1.0e-5 ) / &
             DtPSDOutput( iSpecies ) ) /= &
             floor( ( Time + 1.0e-5 - delta_t ) / &
             DtPSDOutput( iSpecies ) ) ) ) &
             call Cimi_plot_psd( psd( iSpecies, :, :, :, : ), &
             iSpecies, time, xmm, xk )

        ! Output Species' radial drift
        if ( DoSaveVLDrift( iSpecies ) .and. &
             ( floor( ( Time + 1.0e-5 ) / &
             DtVLDriftOutput( iSpecies ) ) /= &
             floor( ( Time + 1.0e-5 - delta_t ) / &
             DtVLDriftOutput( iSpecies ) ) ) ) &
             call Cimi_plot_vl( vlEa( iSpecies, :, :, :, : ), &
             iSpecies, time )

        ! Output Species' poloidal drift
        if ( DoSaveVPDrift( iSpecies ) .and. &
             ( floor( ( Time + 1.0e-5 ) / &
             DtVPDriftOutput( iSpecies ) ) /= &
             floor( ( Time + 1.0e-5 - delta_t ) / &
             DtVPDriftOutput( iSpecies ) ) ) ) &
             call Cimi_plot_vp( vpEa( iSpecies, :, :, :, : ), &
             iSpecies, time )

        ! Write precipitation file
        if ( DoSavePreci( iSpecies ) .and. &
             ( floor( ( Time + 1.0e-5) / &
             DtPreciOutput( iSpecies ) ) ) /= &
             floor( ( Time + 1.0e-5 - delta_t ) / &
             DtPreciOutput( iSpecies ) ) ) then

           call timing_start('cimi_plot_precip')
           call cimi_plot_precip( iSpecies, Time )
           call timing_stop('cimi_plot_precip')

        endif

     enddo

     if ( OutputBoundary ) call Cimi_plot_boundary_check( Time )

     ! Write Sat Output
     if( DoWriteSats .and. DoSavePlot .and. &
          ( floor( ( Time + 1.0e-5 ) / DtSatOut ) ) /= &
          floor( ( Time + 1.0e-5 - delta_t ) / DtSatOut ) ) then

        do iSat=1, nImSats

           call timing_start('cimi_write_im_sat')
           call write_im_sat( iSat, np, nt, neng, npit, flux )
           call timing_stop('cimi_write_im_sat')

        enddo

     endif

     ! Write Logfile
     if( DoSaveLog .and. &
          ( floor( ( Time + 1.0e-5 ) / DtLogOut ) ) /= &
          floor( ( Time + 1.0e-5 - delta_t ) / DtLogOut ) ) then

        call timing_start('cimi_plot_log')
        call cimi_plot_log( Time )
        call timing_stop('cimi_plot_log')

     endif

  endif

end subroutine Cimi_run
!==============================================================================
subroutine cimi_init
  !---------------------------------------------------------------------------
  ! Routine does CIMI initialization: fill arrays
  !
  ! Input: np,nt,neng,npit,nspec,re_m,dipmom,Hiono
  ! Output: xlat,xmlt,energy,sinAo (through augments)
  !         xmm1,xk1,phi1,dlat1,dphi1,dmm1,dk1,delE1,dmu1,xjac,d4,amu (through
  !         common block cinitialization

  use ModInterFlux,	ONLY: set_nghostcell_scheme
  use ModPlanetConst,	ONLY: Earth_,DipoleStrengthPlanet_I,rPlanet_I
  use ModConst,		ONLY: cElectronCharge, cLightSpeed, cProtonMass
  use ModNumConst,	ONLY: cDegToRad,cRadToDeg,cPi
  use ModCimiPlanet,	ONLY: re_m, dipmom, Hiono, amu_I, nspec
  use ModCimiInitialize
  use ModCimiRestart,	ONLY: IsRestart, cimi_read_restart
  use ModImTime
  use ModCimiGrid
  use ModCimi, 		ONLY: energy, Ebound, dele
  use ModTimeConvert,	ONLY: time_int_to_real, time_real_to_int
  use ModMpi
  use CIMI_aurora, ONLY: init_mod_aurora


  implicit none

  integer:: i, n, k, m, iPe, iError

  ! Variables for specifying the Energy Grid
  real:: aloga, eratio, delta_E, E0, &
       energy_ion( 1 : neng ), energy_ele( 1 : neng )

  ! Variables determining the spacing of the mu and K grids
  real:: rw, rw1, rsi, rs1

  real:: sina0, sina1
  real:: xjac1, xjac2, xjacL, sqrtm
  real:: d2

  !----------------------------------------------------------------------------

  call init_mod_aurora

  ! Set up proc distribution

  ! NB/CK: In most cases mod( nt, nProc ) == 0. For example, when nt =
  ! 	48, the number of processors is typically an integer divisor
  ! 	of 48, e.g., [1, 2, 3, 4, 6, 8, 12, 16, 24 ], nLonPar is the
  ! 	same among all processors (properly load-balanced).  However,
  ! 	when nProc = 10 with nt = 48, then nLonPar is not constant
  ! 	among processors (not properly load-balanced). Could this
  ! 	discrepency trigger problems with ghostcells, especially as we
  ! 	begin exploring different MLT grids and/or drift schemes?

  if (iProc < mod(nt,nProc))then
     nLonPar=(nt+nProc-1)/nProc
  else
     ! nLonPar is the number of points per proccess
     nLonPar=nt/nProc
  endif

  ! Figure out the min and max longitude range for each proc
  if (.not.allocated(nLonBefore_P)) allocate(nLonBefore_P(0:nProc-1))
  if (.not.allocated(nLonPar_P))    allocate(nLonPar_P(0:nProc-1))
  call MPI_allgather(nLonPar,1,MPI_INTEGER,nLonPar_P,1,MPI_INTEGER,iComm,iError)
  ! nLonPar_P is an array, it contains nLonPar gathered from all
  ! processes; this is synced and could be used to define
  ! MinLonPar/MaxLonPar for any iProc

  nLonBefore_P(0) = 0
  do iPe = 1, nProc - 1
     nLonBefore_P(iPe) = sum(nLonPar_P(0:iPe-1))
  end do

  if (iProc == 0) then
     MinLonPar=1
     MaxLonPar=nLonPar
  else
     MinLonPar=sum(nLonPar_P(0:iProc-1))+1
     MaxLonPar=minLonPar+nLonPar-1
  endif

  ! Define neighbors and ghost cell indicies
  iProcLeft=iProc-1
  iLonLeft=MinLonPar-1
  if (iProcLeft < 0) then
     iProcLeft=nProc-1
     iLonLeft = nt
  endif
  iProcRight=iProc+1
  iLonRight=MaxLonPar+1
  if (iProcRight == nProc) then
     iProcRight=0
     iLonRight=1
  endif

  ! Define and iLonMidnightiProcMidnight, needed for setting iw2 in fieldpara
  iLonMidnight=nt/2
  if (nProc>1) then
     PROCLIST: do iPe=0,nProc-1
        if (nLonBefore_P(iPe)<iLonMidnight .and. &
             nLonBefore_P(iPe)+nLonPar_P(iPe)>=iLonMidnight)then
           iProcMidnight=iPe
        endif
     enddo PROCLIST
  else
     iProcMidnight=0
  endif

  ! set the number of ghostcells in lon based on the order of the scheme
  call set_nghostcell_scheme

  ! Set start time

  call time_int_to_real(iStartTime_I,CurrentTime)
  StartTime=CurrentTime

  ! Define constants
!!$  re_m = rPlanet_I(Earth_)                            ! earth's radius (m)
!!$  dipmom=abs(DipoleStrengthPlanet_I(Earth_)*re_m**3)  ! earth's dipole moment
!!$  write(*,*) "dipmom: ", dipmom

  ! Define latitude grid
  call init_lat

  if ( DoVerboseLatGrid .and. iProc == 0) then

     write(*,*) 'IM0: xlat grid  (deg)'
     write(*,'(10f8.2)') xlat(1:np)
     write(*,*) 'IM0: L-shell / ri  (RE)'
     write(*,'(10f8.3)') varL(1:np)**(2./varNpower)

  endif

  ! CIMI xmlt grid
  dphi=2.*cPi/nt
  do i=1,nt
     phi(i)=(i-1)*dphi
     xmlt(i)=mod(phi(i)*12.0/cPi + 12.0,24.0)
  enddo

  ! CIMI output grids: energy, sinAo, delE1, dmu1

  ! Checks to set the energy grid to van allen probe MagEIS and REPT
  ! energies
  if ( UseRBSPGrid ) then

     energy_ele( 1 : neng ) = energy_RBSP( 1 : neng )
     energy_ion( 1 : neng ) = energy_RBSP( 1 : neng ) / 10.

  else

     energy_ion( 1 ) = MinIonEnergy

     ! Checks if the grid is to be logarithmic in Energy. (DEFAULT)
     if ( UseLogEGrid ) then

        aloga = &
             LOG10( MaxIonEnergy / MinIonEnergy ) / ( neng - 1 )
        eratio = 10. ** aloga

        do k = 2, neng

           energy_ion( k ) = energy_ion( k - 1 ) * eratio

        enddo

     else

        delta_E = &
             ( MaxIonEnergy - MinIonEnergy ) / ( neng - 1 )

        do k = 2, neng

           energy_ion( k ) = energy_ion( k - 1 ) + delta_E

        enddo

     endif ! end UseLogEGrid if statement

     energy_ele( 1 : neng ) = ElectronGridRatio * energy_ion( 1 : neng )

  endif ! end UseRBSPGrid if statement

  sinAo = &
       [ 0.009417, 0.019070, 0.037105, 0.069562, 0.122536, 0.199229, &
       0.296114, 0.405087, 0.521204, 0.638785, 0.750495, 0.843570, &
       0.910858, 0.952661, 0.975754, 0.988485, 0.995792, 0.998703 ]

  do m=1,npit
     if (m == 1) sina0=0.
     if (m > 1) sina0=0.5*(sinAo(m)+sinAo(m-1))
     if (m < npit) sina1=0.5*(sinAo(m)+sinAo(m+1))
     if (m == npit) sina1=1.
     dmu(m)=sqrt(1.-sina0*sina0)-sqrt(1.-sina1*sina1)
  enddo

  !  do k=2,neng
  !     Ebound(k)=sqrt(energy(k-1)*energy(k))
  !  enddo
  !  Ebound(1) = energy(1)**2/Ebound(2)
  !  Ebound(neng+1)=energy(neng)**2/Ebound(neng)

  ! setup energies:
  do n = 1, nspec - 1
     energy( n, 1 : neng )  = energy_ion( 1 : neng )
     delE( n, 1 : neng )    = 0.5243 * energy_ion( 1 : neng )
  enddo
  energy( nspec, 1 : neng ) = energy_ele( 1 : neng )
  delE( nspec, 1 : neng )   = 0.5243 * energy_ele( 1 : neng )

  ! From CIMI:
  ! Setup Ebound
  do n = 1, nspec
     do k = 1, neng - 1
        Ebound( n, k ) = SQRT( energy( n, k ) * energy( n, k + 1 ) )
     enddo
     Ebound( n, neng ) = energy( n, neng ) * energy( n, neng ) / &
          Ebound( n, neng - 1 )
  enddo

  ! Initialization of the CIMI xmm (mu) grid.  Accounts for
  ! relativistic effects in the calculation of mu for all species
  ! (even for ring current ions which are non-relativistic.) Sets the
  ! scaling factor rw such that the lowest energy particles have mu at
  ! the planetary surface in a dipolar magnetic; the maximum mu value
  ! has the most energetic particles fit at well beyond the simulation
  ! boundary.
  do n = 1, nspec

     ! Calculates the rest mass energy [ keV ] of the particle
     ! species.
     E0 = amu_I( n ) * cProtonMass * cLightSpeed ** 2 / &
          cElectronCharge / 1000.

     ! Convert energy from keV to J to calculate mu in [ J / Tesla ]
     ! The minimum of the mu grid should fit the lowest energy
     ! particles at the Earth's surface.

     xmm( n, 01 ) = &
          energy( n, 1 ) * 1000. * cElectronCharge * &
          ( energy( n, 1 ) + 2. * E0 ) / E0 / &
          ( 2 * dipmom / (  1.0 * re_m ) ** 3 )

     ! The maximum of mu grid should fit the highest energy particles
     ! at 45 R_E well beyond the extent of the simulation grid.
     xmm( n, nm ) = &
          energy( n, neng ) * 1000. * cElectronCharge * &
          ( energy( n, neng ) + 2. * E0 ) / E0 / &
          ( 2 * dipmom / ( 45.0 * re_m ) ** 3 )

     rw = ( xmm( n, nm ) / xmm( n, 01 ) ) ** ( 1. / FLOAT( nm ) )

     rw1 = ( rw - 1. ) / SQRT( rw )
     xmm( n, 0 ) = xmm( n, 1 ) / rw

     ! This setup makes:
     ! 		xmm( k + 0.5 ) = SQRT( xmm( k ) * xmm( k + 1 ) )
     do i = 1, nm

        xmm( n, i ) = xmm( n, i - 1 ) * rw
        dmm( n, i ) = xmm( n, i ) * rw1

     enddo

     xmm( n, nm + 1 ) = xmm( n, nm ) * rw

  enddo

  !----------------------------------------------------------------------------

  ! OLDER Initialization of CIMI magnetic moment, xmm1 (pre-April 2018)
  ! This setup makes xmm(k+0.5)=sqrt(xmm(k)*xmm(k+1))

!!$  do n=1,nspec
!!$     xmm(n,1)=energy(n,1)*cElectronCharge/(dipmom/(2*re_m)**3)
!!$     rw=1.55
!!$     rw1=(rw-1.)/sqrt(rw)
!!$     xmm(n,0)=xmm(n,1)/rw
!!$     do i=1,nm   ! This setup makes xmm(k+0.5)=sqrt(xmm(k)*xmm(k+1))
!!$        xmm(n,i)=xmm(n,i-1)*rw
!!$        dmm(n,i)=xmm(n,i)*rw1
!!$     enddo
!!$     xmm(n,nm+1)=xmm(n,nm)*rw
!!$  enddo

  !----------------------------------------------------------------------------

  ! OLDEST INITIALIZATION of CIMI magnetic moment, xmm1 (pre-2016)
  ! This setup has been saved and commented out here for comparison
  ! with older simulations

!!$  do n=1,nspec
!!$     xmm(n,1)=energy(n,1)*cElectronCharge/(dipmom/(2*re_m)**3)
!!$     dmm(n,1)=xmm(n,1)*2.
!!$     rw=1.55
!!$     do i=2,nm
!!$        dmm(n,i)=dmm(n,1)*rw**(i-1)
!!$        xmm(n,i)=xmm(n,i-1)+0.5*(dmm(n,i-1)+dmm(n,i))
!!$     enddo
!!$  enddo

  !---------------------------------------------------------------------------
  ! CIMI K grid, xk: Minimum value is 40 T^0.5 / m
  ! (~89 degrees at L = 1 R_E or ~87 degrees at L = 7.)


  xk( 0 ) = 40.
  xk(nk+1) = 4e6
  rsi = (xk(nk+1)/xk(0))**(1/(real(nk)+1))
  rs1 = ( rsi - 1. ) / SQRT( rsi )

  ! In the following sutup:
  ! 		xk( i + 0.5 ) = SQRT( xk( i ) * xk( i + 1 ) )
  do i = 1, nk
     xk( i ) = xk( i - 1 ) * rsi
     dk( i ) = xk( i ) * rs1
  enddo

  ! Calculate Lfactor, which is used in subroutine driftV
  do i=0,np+1
     Lfactor(i)=0.5*varNpower*varL(i)**(1.+2./varNpower)
     Lfactor1(i)=0.5*varNpower*(varL(i)+0.5*dvarL)**(1.+2./varNpower)
  enddo

  ! Calculate Jacobian, xjac
  do n=1,nspec
     xjac1=4.*sqrt(2.)*cPi*(1.673e-27*amu_I(n))*dipmom/(re_m+Hiono*1000.)
     sqrtm=sqrt(1.673e-27*amu_I(n))
     do i=1,np

        if (DoUseUniformLGrid) then

           ! Jacobian from varL
           !! xjacL=1./varL(i)**(1.+1./varNpower)/sin(xlatr(i))/varNpower
           xjac2 = 1./Lfactor(i)

        else

           !! xjacL = 1.
           xjac2 = sin(2.*abs(xlatr(i)))

        endif

        !! xjac2 = sin(2.*xlatr(i))*xjacL

        do k=1,nm

           xjac(n,i,k) = xjac1 * xjac2 * sqrt( xmm(n,k) ) * sqrtm

        enddo
     enddo
  enddo

  ! Calculate d4Element_C: dlat*dphi*dmm*dk
!!$  d2=dvarL*dphi
  do n=1,nspec
     do i=1,np
        d2=dlat(i)*dphi
        do k=1,nm
           do m=1,nk
              d4Element_C(n,i,k,m)=d2*dmm(n,k)*dk(m)
           enddo
        enddo
     enddo
  enddo

  if(IsRestart) then
     ! set initial state when restarting
     call cimi_read_restart
  endif

end subroutine cimi_init
!==============================================================================
subroutine initial_f2(nspec,np,nt,iba,amu_I,vel,xjac,ib0)

  ! Set up initial distribution.

  ! Input: nspec,np,nt,iba,Den_IC,Temp_IC,amu,vel,xjac
  ! Output: ib0,f2,rbsum,xleb,xled,xlel,xlee,xles,driftin,driftout
  !         (through common block cinitial_f2)

  use ModIoUnit, ONLY: UnitTmp_
  use ModGmCimi, ONLY: Den_IC, Temp_IC, Temppar_IC
  use ModCimi,   ONLY: f2
  use ModCimiInitialize,   ONLY: IsEmptyInitial, IsDataInitial, IsRBSPData, &
       IsGmInitial, IsInitialElectrons, DoLstarInitialization, &
       NameFinFile, FinFilePrefix
  use ModCimiGrid, ONLY: nm,nk,MinLonPar,MaxLonPar,iProc,nProc,iComm, &
       d4Element_C,neng
  use ModCimiPlanet,		ONLY: 	NameSpeciesExtension_I
  use ModCimiTrace, ONLY: sinA,ro, ekev,pp,iw2,irm
  use ModCimiUtil, ONLY: lintp2IM

  use ModMpi

  use ModLstar, 		ONLY:	&
       Lstar_C, Lstarm

  implicit none

  integer, intent(in):: nspec,np,nt,iba(nt)
  real, intent(in):: amu_I(nspec), vel(nspec,np,nt,nm,nk), xjac(nspec,np,nm)
  integer, intent(out):: ib0(nt)

  integer:: n, j, i, k, m, iError
  real:: velperp2, velpar2
  real:: pi,xmass,chmass,f21,vtchm
  real:: Tempperp_IC(nspec,np,nt)
  real:: xleb(nspec),xled(nspec),xlel(nspec),xlee(nspec),xles(nspec)

  ! Variables needed for data initialization
  integer:: il, ie, iunit
  real, allocatable :: roi(:), ei(:), fi(:,:)
  real:: roii, e1,x, fluxi,psd2,etemp
  !----------------------------------------------------------------------------
  pi=acos(-1.)

  ib0(:)=iba(:)
  ! Set initial f2 to a small number; this makes IsEmptyInitial
  ! default
  f2(:,:,:,:,:)=1.0e-40

  if(IsGmInitial) then
     ! Set initial f2 based on Maxwellian or bi-Maxwellian
     Tempperp_IC(:,:,:) = (3*Temp_IC(:,:,:) - Temppar_IC(:,:,:))/2.
     do n=1,nspec
        xmass=amu_I(n)*1.673e-27
        chmass=1.6e-19/xmass
        do j=MinLonPar,MaxLonPar
           do i=1,iba(j)
              ! write(*,*) n,i,j,iba(j),Den_IC(n,i,j), Temppar_IC(n,i,j), Tempperp_IC(n,i,j)
              f21=Den_IC(n,i,j)/(2.*pi*xmass*Temppar_IC(n,i,j)*1.6e-19)**0.5 &
                   /(2.*pi*xmass*Tempperp_IC(n,i,j)*1.6e-19)
              ! following lines were for when we had isotropic option
              ! else
              !   f21=Den_IC(n,i,j)/(2.*pi*xmass*Temp_IC(n,i,j)*1.6e-19)**1.5
              ! end if
              do k=1,nm
                 do m=1,nk
                    velperp2 = (vel(n,i,j,k,m)*sinA(i,j,m))**2
                    velpar2 = vel(n,i,j,k,m)**2 - velperp2
                    vtchm = -velpar2/(2*Temppar_IC(n,i,j)*chmass) &
                         -velperp2/(2*Tempperp_IC(n,i,j)*chmass)
                    ! from when having an isotropic option
                    ! else
                    !   vtchm = -vel(n,i,j,k,m)**2/(2*Temp_IC(n,i,j)*chmass)
                    ! end if
                    f2(n,i,j,k,m)=xjac(n,i,k)*f21*exp(vtchm)
                 end do
              end do
           end do
        end do
     end do
  elseif(IsDataInitial) then
     do n=1,nspec
        if (IsRBSPData) then
           FinFilePrefix = 'RBSP'
        else
           FinFilePrefix = 'quiet'
        endif
        ! set the file name, open it and read it
        NameFinFile  = &
             TRIM( FinFilePrefix ) // &
             NameSpeciesExtension_I( n ) // '.fin'
        open(unit=UnitTmp_,file='IM/'//NameFinFile,status='old')
        read(UnitTmp_,*) il,ie
        allocate (roi(il),ei(ie),fi(il,ie))
        ! iunit values: 1=flux in (cm2 s sr keV)^-1, 2=in (cm2 s MeV)^-1
        read(UnitTmp_,*) iunit
        read(UnitTmp_,*) roi
        read(UnitTmp_,*) ei      ! ei in keV
        read(UnitTmp_,*) fi
        close(UnitTmp_)
        if(iunit == 2) fi(:,:)=fi(:,:)/4./pi/1000. ! con.To(cm^2 s sr keV)^-1

        ei(:)=log10(ei(:))                      ! take log of ei
        fi(:,:)=log10(fi(:,:))                  ! take log of fi

        ! interpolate data from quiet.fin files to CIMI grid
        do j=MinLonPar,MaxLonPar
           do i=1,irm(j)
              do m=1,nk
                 if ( DoLstarInitialization ) then
                    roii=Lstarm(i,j,m)
                 else
                    roii=ro(i,j)
                 endif
                 do k=1,iw2(n,m)
                    e1=log10(ekev(n,i,j,k,m))
                    if (e1 <= ei(ie)) then
                       ! if (e1 >= ei(ie)) e1=ei(ie)    ! flat dist at high E
                       if (e1 < ei(1)) e1=ei(1)    ! flat dist. at low E
                       if (roii < roi(1)) roii=roi(1) ! flat dist @ lowL
                       if (roii > roi(il)) roii=roi(il) ! flat @ high L
                       call lintp2IM(roi,ei,fi,il,ie,roii,e1,x)
                       fluxi=10.**x          ! flux in (cm^2 s sr keV)^-1
                       psd2=fluxi/(1.6e19*pp(n,i,j,k,m))/pp(n,i,j,k,m)
                   
                       f2(n,i,j,k,m)=psd2*xjac(n,i,k)*1.e20*1.e19
                       ! kludge
                       ! if (j>0 .and. j<24) f2(n,i,j,k,m) = 0.0
                       ! if (j>36) f2(n,i,j,k,m) = 10.0*f2(n,i,j,k,m)
                    endif
                 enddo                            ! end of k loop
              enddo                               ! end of m loop

           enddo                                  ! end of i loop
        enddo                                     ! end of j loop
        deallocate (roi,ei,fi)
        ! f2(:,1,:,:,:)=f2(:,2,:,:,:)
     enddo                                        ! end of n loop
  end if

end subroutine initial_f2
!==============================================================================
subroutine initial_extra

  use ModCimiPlanet, 	ONLY: nSpec
  use ModCimiGrid,   	ONLY: nP, nT, nEng
  use ModCimi,   	ONLY: nOperator, driftin, driftout, &
       eChangeOperator_VICI, echangeLocal, eChangeGlobal, &
       pChangeOperator_VICI, eTimeAccumult_ICI, pTimeAccumult_ICI, &
       rbsumLocal, rbsumGlobal, rcsumLocal, rcsumGlobal, &
       SDtime, phot, Ppar_IC, Pressure_IC, PressurePar_IC, FAC_C, Bmin_C
  !----------------------------------------------------------------------------
  ! Initialize allocated variables in ModCimi to 0.
  phot(1:nspec,1:np,1:nt) = 0.0
  Ppar_IC(1:nspec,1:np,1:nt) = 0.0
  Pressure_IC(1:nspec,1:np,1:nt) = 0.0
  PressurePar_IC(1:nspec,1:np,1:nt) = 0.0
  FAC_C(1:np,1:nt) = 0.0
  Bmin_C(1:np,1:nt) = 0.0

  ! Setup variables for energy gain/loss from each process
  eChangeOperator_VICI(1:nspec,1:np,1:nt,1:neng+2,1:nOperator)=0.0
  pChangeOperator_VICI(1:nspec,1:np,1:nt,1:neng+2,1:nOperator)=0.0
  eTimeAccumult_ICI(1:nspec,1:np,1:nt,1:neng+2) = 0.0
  pTimeAccumult_ICI(1:nspec,1:np,1:nt,1:neng+2) = 0.0

  eChangeLocal(1:nspec, 1:nOperator) = 0.
  eChangeGlobal(1:nspec, 1:nOperator) = 0.

  ! Total energy for the simulation domain
  rbsumLocal(1:nspec) = 0.
  rbsumGlobal(1:nspec) = 0.

  ! Total energy for L < 6.6 R_E
  rcsumLocal(1:nspec) = 0.
  rcsumGlobal(1:nspec) = 0.

  driftin(1:nspec)=0.      ! energy gain due injection
  driftout(1:nspec)=0.     ! energy loss due drift-out loss

end subroutine initial_extra
!==============================================================================
subroutine boundaryIM(nspec,neng,np,nt,nm,nk,iba,irm,amu_I,xjac,energy,vel,fb)

  !  *Relativistic version of boundary PSD.*
  ! Set up the boundary distribution for the CIMI. Distribution
  ! at the boundary is assumed to be Maxwellian for ions and a kappa
  ! (kappa=3) distribution for electrons. Boundary temperature and
  ! density are from MHD (or empirical models if Weimer/Tsyganenko fields are
  ! used)

  ! Input: nspec,np,nt,nm,nk,iba,irm,amu,xjac,Den_IC,Temp_IC,vel
  ! Output: fb

  use ModGmCimi, ONLY:  Temp_IC, Temppar_IC
  use ModCimi,        ONLY: MinLonPar,MaxLonPar, f2
  use ModCimiGrid, ONLY: MinLonPar,MaxLonPar
  use ModCimiTrace,  ONLY: sinA,ekev,iw2,pp
  use ModCimiBoundary, ONLY: BoundaryDens_IC,BoundaryTemp_IC,BoundaryTempPar_IC
  use ModCimiUtil,     ONLY: ln_gamma

  implicit none

  integer, intent(in):: nspec,neng,np,nt,nm,nk,iba(nt),irm(nt)
  real, intent(in):: amu_I(nspec), xjac(nspec,np,nm), energy(nspec,neng)
  real, intent(in):: vel(nspec,np,nt,nm,nk)
  real, intent(out):: fb(nspec,nt,nm,nk)

  integer:: j,n,k,m,ib1,iLat
  real:: pi, xmass, chmass, fb1, fb_temp, vtchm
  real:: velperp2, velpar2,fbb,fbb1,parE1,perE1,den1,temp32,y2,x2,Psq,&
       Pper2,Ppar2
  real:: erpp,Vsq,Vper2,Vpar2
  real:: BoundaryTempperp_IC(nspec,nt)
  real:: kappa,kappa_plus_one,kappa_minus_half,ln_gamma_diff,gamma_ratio
  real:: e_min

  !real, external:: ln_gamma
  !----------------------------------------------------------------------------
  kappa=3.
  ! zk1=zkappa+1.
  kappa_plus_one=kappa+1.
  kappa_minus_half=kappa-0.5
  ln_gamma_diff=ln_gamma(kappa_plus_one)-ln_gamma(kappa_minus_half)
  gamma_ratio=exp(ln_gamma_diff)

  pi=acos(-1.)

  !   defining Pperp and Ppar for both cases, anisotropic and isotropic.
  !   Advantages: use the same form of Maxwellian/kappa dist
  !   since in the case of isotropic PSD

  do n=1,nspec
     do j=MinLonPar,MaxLonPar
        BoundaryTempperp_IC(n,j) = &
             (3*BoundaryTemp_IC(n,j) - BoundaryTempPar_IC(n,j))/2
     enddo
  enddo

  fb(1:nspec,MinLonPar:MaxLonPar,1:nm,1:nk)=0.
  do n=1,nspec
     e_min=0.
     if (n==nspec) e_min=0.5*energy(n,1)
     xmass=amu_I(n)*1.673e-27
     chmass=1.6e-19/xmass
     do j=MinLonPar,MaxLonPar
        parE1=BoundaryTempPar_IC(n,j)/1000.      ! characteristic in keV
        perE1=BoundaryTempPerp_IC(n,j)/1000.     ! characteristic in keV
        den1 =BoundaryDens_IC(n,j)                     ! density in m-3
        temp32=sqrt(parE1)*perE1*1.6e-16**1.5
        ib1=iba(j)+1   ! difference bewteen cimi standalone where ib1=iba(j)
        if (ib1 > irm(j)) ib1=irm(j)
        if (n == nspec) then    !    kappa, for electrons only
           fbb1=den1*gamma_ratio/temp32/(2.*pi*kappa*xmass)**1.5
        else
           fbb1=den1/temp32/(2.*pi*xmass)**1.5 ! Maxwellian
        endif

        !      do k=1,nm   ! magnetic moment
        do m=1,nk   ! K invariant
           y2=sinA(ib1,j,m)*sinA(ib1,j,m)
           x2=1.-y2
           do k=1,iw2(n,m)   ! magnetic moment
              fbb=0.
              if ((ekev(n,ib1,j,k,m) > e_min) .and. &
                   (BoundaryDens_IC(n,j) > 0.)) then

                 if (n == nspec) then   ! KAPPA FOR ELECTRONS
                    Psq=pp(n,ib1,j,k,m)*pp(n,ib1,j,k,m)
                    Ppar2=Psq*x2
                    Pper2=Psq*y2
                    erpp=(Ppar2/parE1+Pper2/perE1)/2./xmass/1.6e-16
                    fbb=fbb1/(1.+erpp/kappa)**kappa_plus_one

                 else ! MAXWELLIAN OTHERWISE:
                    Vsq=vel(n,ib1,j,k,m)*vel(n,ib1,j,k,m)
                    Vpar2=Vsq*x2
                    Vper2=Vsq*y2
                    erpp=(Vpar2/parE1+Vper2/perE1)/2./1000./chmass
                    fbb=0.
                    if (erpp < 500.) fbb=fbb1/exp(erpp)
                 endif
              endif      ! end of ekev(n,ib,j,k,m) > e_min.and.den1 > 0

              fb(n,j,k,m)=fbb*xjac(n,ib1,k)

              do iLat=ib1,np
                 f2(n,iLat,j,k,m)=fbb*xjac(n,ib1,k) ! f2 beyond rb
              enddo

              !        if ((n == nspec).and.(j == 1).and.(m == 10)) write(*,*) fbb,xjac(n,ib1,k), &
              !        ekev(n,ib1,j,k,m), vel(n,ib1,j,k,m),xmass,chmass, &
              !        Psq,Ppar2,Vsq,Vpar2, 'f,jac,e,vel,m,chm,Psq,Ppar,Vsq,Vpar'   !   NB Jan 20 2017
           enddo                ! end of m loop
        enddo                   ! end of k loop
     enddo                      ! end of j loop
  enddo                         ! end of n loop

end subroutine boundaryIM
!==============================================================================
subroutine ceparaIM(nspec,np,nt,nm,nk,irm,dt,vel,ekev,Have,achar)

  ! Calculate the depreciation factor of H+, achar, due to charge
  ! exchange loss
  !
  ! Input: irm,nspec,np,nt,nm,nk,dt,vel,ekev,Have     ! Have: bounce-ave [H]
  ! Output: achar

  use ModCimiPlanet,  ONLY: a0_I,a1_I,a2_I,a3_I,a4_I
  use ModCimi,       ONLY: MinLonPar,MaxLonPar

  implicit none

  integer, intent(in):: np, nt, nspec, nk, irm(nt), nm
  real, intent(in):: vel(nspec,np,nt,nm,nk), ekev(nspec,np,nt,nm,nk), &
       Have(np,nt,nk)
  real, intent(out):: achar(nspec,np,nt,nm,nk)

  integer:: i, j, k, m, n
  real:: dt, Havedt, x, d, sigma, alpha 
  !----------------------------------------------------------------------------
  do n=1,nspec-1
     do j=MinLonPar,MaxLonPar
        do i=1,irm(j)
           do m=1,nk
              Havedt=Have(i,j,m)*dt
              do k=1,nm
                 x=log10(ekev(n,i,j,k,m))
                 if (x < -2.) x=-2.
                 d=a0_I(n)+a1_I(n)*x+a2_I(n)*x**2+a3_I(n)*x**3+a4_I(n)*x**4
                 sigma=10.**d      ! charge exchange cross section of H+ in m2
                 alpha=vel(n,i,j,k,m)*sigma*Havedt
                 achar(n,i,j,k,m)=exp(-alpha) ! charge. exchange decay rate
              enddo
           enddo
        enddo
     enddo
  enddo

end subroutine ceparaIM
!==============================================================================
subroutine set_cimi_potential(CurrentTime)

  ! Set the ionospheric potentials from weimer when not using MHD input

  use ModCimiGrid,	ONLY: xlatr, xmlt, np, nt
  use ModIeCimi,	ONLY: pot
  use ModMpi
  use EIE_ModWeimer,	ONLY: setmodel00, &
       boundarylat00, epotval00
  use ModNumConst,	ONLY: cPi, cRadToDeg
  use ModIndicesInterfaces
  use ModCimiTrace,	ONLY: UsePotential
  use ModCimiPlanet,	ONLY: rc
  use ModUtilities, ONLY: CON_stop

  implicit none

  real, intent(in) :: CurrentTime

  logical:: UseAL
  real:: ALindex
  real:: xnsw,vsw,bx,by,bz ! solar wind values at current time
  real:: Tilt, angle, Bt, gLAT, gMLT, BnLat

  integer:: i, iError, j

  ! geopack common block stuff
  COMMON /GEOPACK/ ST0,CT0,SL0,CL0,CTCL,STCL,CTSL,STSL,SFI,CFI,SPS,&
       CPS,SHI,CHI,HI,PSI,XMUT,A11,A21,A31,A12,A22,A32,A13,A23,A33,DS3,&
       K,IY,CGST,SGST,BA(6)
  real::ST0,CT0,SL0,CL0,CTCL,STCL,CTSL,STSL,SFI,CFI,SPS,&
       CPS,SHI,CHI,HI,PSI,XMUT,A11,A21,A31,A12,A22,A32,A13,A23,A33,DS3,&
       CGST,SGST,BA
  integer:: K,IY
  !----------------------------------------------------------------------------
  UseAL=.false.
  ALindex=10.          ! arbitrary value

  !  Setup for Weimer's electric field model if iconvect = 1
  Tilt=psi*cRadToDeg          ! dipole tilt angle in degree
  !  if (UseGm)then
  !     ! When using GM, it is assumed that the solar wind will be passed
  !     xnsw=xnswa(1)
  !     vsw=vswa(1)
  !     bx=bxw(1)
  !     by=byw(1)
  !     bz=bzw(1)
  !  else
  ! When GM is not used set the solar wind parameters form the imf.dat file
  ! defined in the PARAM.in

  call get_IMF_Bz(CurrentTime, bz, iError)

  call get_IMF_By(CurrentTime, by, iError)

  call get_SW_V(CurrentTime, vsw, iError)

  call get_SW_N(CurrentTime, xnsw, iError)

  ! only use the absolute value of sw velocity
  vsw=abs(vsw)

  if (iError /= 0) then
     call CON_stop&
          ("IM_ERROR: Problem setting solar wind in set_cimi_potential")
  end if
  !  endif
  angle=atan2(by,bz)*180./cPi       ! degrees from northward toward +Y
  Bt=sqrt(by*by+bz*bz)             ! Magnitude of IMF in Y-Z plane in nT
  call SetModel00(angle,Bt,Tilt,vsw,xnsw,ALindex,UseAL)

  !  Find potential (in Volt) at the ionosphere
  do i=1,np
     gLAT=acos(cos(xlatr(i))/sqrt(rc))*cRadToDeg ! invariant lat. in degree
     do j=1,nt
        gMLT=xmlt(j)                 ! convert mlt from radians to hour 0-24.
        BnLat=BoundaryLat00(gMLT)    ! boundary latitude
        if (UsePotential) then
           pot(i,j)=0.0
           if (gLAT > BnLat) pot(i,j)=EpotVal00(gLAT,gMLT)*1000.  ! Volt
        else
           pot(i,j)=0.0
        endif
     enddo
  enddo

end subroutine set_cimi_potential
!==============================================================================
subroutine driftV(nspec,np,nt,nm,nk,irm,re_m,Hiono,dipmom,dphi,xlat,dlat, &
     ekev,pot,vl,vp)

  ! Calculate the drift velocities

  use ModCimiGrid, ONLY: iProc, nProc, iComm, MinLonPar, MaxLonPar, &
       iProcLeft, iLonLeft, iProcRight, iLonRight, DoUseUniformLGrid,&!, xlatr
       varL,dvarL,Lfactor,Lfactor1
  use ModCimiInitialize, ONLY:
  use ModMpi
  use ModCimiTrace, ONLY: UseCorotation

  implicit none

  integer, intent(in):: nspec, np, nt, nm, nk
  integer, intent(out):: irm(nt)
  real, intent(in):: re_m, Hiono, dipmom, dphi
  real, intent(in):: xlat(np),dlat(np),ekev(nspec,np,nt,nm,nk),pot(np,nt)
  real, intent(out):: vl(nspec,0:np,nt,nm,nk), vp(nspec,0:np,nt,nm,nk)

  integer:: n,i,ii,j,k,m,i0,i2,j0,j2,icharge
  real:: kfactor,ksai,ksai1,xlat1,sf0,sf2,dlat2,pi,dphi2,cor
  real:: ham(np,nt), xlatr(np)
  ! MPI status variable
  integer:: iStatus_I(MPI_STATUS_SIZE), iError
  !----------------------------------------------------------------------------
  pi=acos(-1.)
  dphi2=dphi*2.
  kfactor = dipmom / ( re_m + Hiono * 1000. )
  if (UseCorotation) then
     cor=2.*pi/86400.                        ! corotation speed in rad/s
  else
     cor=0.
  endif
  xlatr = xlat * pi / 180.

  nloop: do n=1,nspec
     if (n < nspec) then
        icharge=1
     else
        icharge=-1
     endif

     mloop: do m=1,nk
        kloop: do k=1,nm

           ! ham: Hamiltonian/q
           ham(1:np,1:nt)=icharge*ekev(n,1:np,1:nt,k,m)*1000.+pot(1:np,1:nt)

           ! When nProc>1 exchange ghost cell info for ham and irm
           if (nProc >1) then
              ! send to neigboring Procs
              call MPI_send(ham(1:np,MaxLonPar),np,MPI_REAL,iProcRight,&
                   1,iComm,iError)
              call MPI_send(ham(1:np,MinLonPar),np,MPI_REAL,iProcLeft,&
                   2,iComm,iError)
              call MPI_send(irm(MaxLonPar),1,MPI_INTEGER,iProcRight,&
                   3,iComm,iError)
              call MPI_send(irm(MinLonPar),1,MPI_INTEGER,iProcLeft,&
                   4,iComm,iError)
              ! recieve from neigboring Procs
              call MPI_recv(ham(1:np,iLonLeft),np,MPI_REAL,iProcLeft,&
                   1,iComm,iStatus_I,iError)
              call MPI_recv(ham(1:np,iLonRight),np,MPI_REAL,iProcRight,&
                   2,iComm,iStatus_I,iError)
              call MPI_recv(irm(iLonLeft),1,MPI_INTEGER,iProcLeft,&
                   3,iComm,iStatus_I,iError)
              call MPI_recv(irm(iLonRight),1,MPI_INTEGER,iProcRight,&
                   4,iComm,iStatus_I,iError)

           endif

           ! calculate drift velocities vl and vp
           iloop: do i=0,np
              ii=i
              if (i == 0) ii=1
              if ( DoUseUniformLGrid ) then
                 ksai = kfactor / Lfactor(i)
                 ksai1 = kfactor / Lfactor1(i)
              else
                 if ( i >= 1 ) &
                      ksai = kfactor * sin( 2. * xlatr(i) )
                 if ( i < np ) &
                      xlat1 = 0.5 * ( xlatr(ii) + xlatr(i+1) )! xlat(i+0.5)
                 ksai1 = kfactor * sin( 2. * xlat1 )          ! ksai at i+0.5
              endif
              jloop: do j=MinLonPar,MaxLonPar
                 j0=j-1
                 if (j0 < 1) j0=j0+nt
                 j2=j+1
                 if (j2 > nt) j2=j2-nt

                 ! calculate vl
                 if (irm(j0) > i.and.irm(j2) > i) then
                    sf0=0.5*ham(ii,j0)+0.5*ham(i+1,j0)
                    sf2=0.5*ham(ii,j2)+0.5*ham(i+1,j2)
                    vl(n,i,j,k,m)=-(sf2-sf0)/dphi2/ksai1   ! vl at (i+0.5,j)
                 else
                    vl(n,i,j,k,m)=vl(n,i-1,j,k,m)
                 endif

                 ! calculate vp
                 if (i >= 1) then
                    if (irm(j2) > i) then
                       i0=i-1
                       if (i == 1) i0=1
                       i2=i+1
                       if (i == np) i2=np
                       if ( DoUseUniformLGrid ) then
                          dlat2 = dvarL * 2.
                       else
                          dlat2 = xlatr(i2) - xlatr(i0)
                       endif
                       sf0=0.5*(ham(i0,j2)+ham(i0,j))
                       sf2=0.5*(ham(i2,j2)+ham(i2,j))
                       vp(n,i,j,k,m)=cor+(sf2-sf0)/dlat2/ksai ! vp at (i,j+0.5)
                    else
                       vp(n,i,j,k,m)=vp(n,i-1,j,k,m)
                    endif
                 endif
              enddo jloop
           enddo iloop
        enddo kloop
     enddo mloop
  enddo nloop

end subroutine driftV
!==============================================================================
subroutine driftIM(iw2,nspec,np,nt,nm,nk,dt,dlat,dphi,brad,rb,vl,vp, &
     fb,f2,driftin,driftout,ib0)

  ! Update f2 due to drift
  !
  ! Calculates dF/dt = d(vl * F)/dxlat + d(vp * F)/dphi
  !       or   dF/dt = d(vl * F)/dvarL + d(vp * F)/dphi
  ! where F = PSD * xjac
  !
  ! Input: iw2,nspec,np,nt,nm,nk,iba,dt,dlat,dphi,brad,rb,vl,vp,fbi
  ! Input/Output: f2,ib0,driftin,driftout

  use ModCimiGrid,	ONLY: MinLonPar, MaxLonPar, DoUseUniformLGrid, dvarL
  use ModCimi,		ONLY: IsStrictDrift
  use ModCimiTrace,	ONLY: iba, ekev
  use ModCimiGrid,	ONLY: iProc, nProc, iComm, MinLonPar, MaxLonPar, &
       iProcLeft, iLonLeft, iProcRight, iLonRight, d4Element_C, &
       nLonPar, nLonPar_P, nLonBefore_P
  use ModInterFlux,	ONLY: &
       UseHigherOrder, FLS_2D_ho, nGhostLonLeft, nGhostLonRight
  use ModMpi
  use ModUtilities,	ONLY: CON_stop

  implicit none

  integer, intent(in):: nk, nspec, np, nt, nm, iw2(nspec,nk)
  real, intent(in):: dt,dlat(np),dphi,brad(np,nt), rb
  real, intent(in):: vl(nspec,0:np,nt,nm,nk),vp(nspec,0:np,nt,nm,nk)
  real, intent(in):: fb(nspec,nt,nm,nk)
  integer, intent(out):: ib0(nt)
  real, intent(inout):: f2(nspec,np,nt,nm,nk), driftin(nspec), driftout(nspec)
  
  integer:: n,i,j,k,m,j1,j_1,j_2,ibaj,ib,ibo,nrun,nn

  real:: f2d(np,nt),f2d0(np,nt),cmax,cl1,cp1,cmx,dt1,fb0(nt), &
       fb1(nt),fo_log,fb_log,f_log
  real:: slope,cl(np,nt),cp(np,nt),fal(0:np,nt),fap(np,nt),&
       fupl(0:np,nt),fupp(np,nt)
  real:: dEner, dPart, dEnerLocal_C(nt), dPartLocal_C(nt)
  real:: f_upwind
  logical:: UseUpwind = .false.

  ! MPI status variable
  integer:: iStatus_I(MPI_STATUS_SIZE), iSendCount
  integer:: iError, iReceiveCount_P( nProc ), iDisplacement_P( nProc )
  real, allocatable :: cmax_P(:)

  real:: buf2D_send( np , MaxLonPar - MinLonPar + 1 )
  real:: buf2D_recv( np, nt ), buf2D_send_1( np, 2 ), buf2D_recv_1( np, 2 )

  ! number of cells for current proc; should be the same
  ! MaxLonPar-MinLonPar+1
  !----------------------------------------------------------------------------
  iSendCount = np * nLonPar
  ! number of cells per proc; function from proc no
  iReceiveCount_P = np * nLonPar_P
  ! number of 1st cell in a given proc; function from proc no
  iDisplacement_P = np * nLonBefore_P

  if (.not.allocated(cmax_P) .and. nProc>1) allocate(cmax_P(nProc))

  ! When nProc>1 pass iba from neighboring procs
  if (nProc >1) then
     ! send to neigboring Procs
     call MPI_send(iba(MinLonPar),1,MPI_INTEGER,iProcLeft,&
          1,iComm,iError)
     ! recieve from neigboring Procs
     call MPI_recv(iba(iLonRight),1,MPI_INTEGER,iProcRight,&
          1,iComm,iStatus_I,iError)
  endif

  nloop: do n=1,nspec
     mloop: do m=1,nk
        kloop: do k=1,iw2(n,m)
           f2d(1:np,1:nt)=f2(n,1:np,1:nt,k,m)         ! initial f2
           ! find nrun and new dt (dt1)
           cmax=0.
           do j=MinLonPar,MaxLonPar
              j1=j+1
              if (j1 > nt) j1=j1-nt
              ibaj=max(iba(j),iba(j1))
              do i=1,ibaj
                 if (DoUseUniformLGrid) then
                    cl1=dt/dvarL*vl(n,i,j,k,m)  ! Courant number in L, unitless
                 else
                    cl1=dt/dlat(i)*vl(n,i,j,k,m)  ! Courant number in L
                 endif
                 cp1=dt/dphi*vp(n,i,j,k,m)   ! Courant number in phi
                 cmx=max(abs(cl1),abs(cp1))
                 cmax=max(cmx,cmax)
              enddo
           enddo

           ! get same cmax on all procs !!! MPI_allreduce !!!
           if (nProc > 1) then
              call MPI_allgather( &
                   cmax, 1, MPI_REAL, cmax_P, 1, MPI_REAL, iComm, iError)
              cmax = maxval(cmax_P)
           endif

           ! NOTE: if Courant number > 1, drift speed in a time step is greater
           !       than a grid size and makes the code unstable.
           !       Thus, Courant number must be < 1 and time step is small
           !       enough such that Courant number < 1.
           nrun=ifix(cmax/0.50)+1     ! nrun to limit the Courant number
           dt1=dt/nrun                ! new dt

           ! Setup boundary fluxes and Courant numbers
           do j=MinLonPar,MaxLonPar
              ib=iba(j)
              ibo=ib0(j)
              fb0(j)=f2d(1,j)                 ! f2 at inner boundary
              fb1(j)=fb(n,j,k,m)              ! f2 at outer boundary
              if (ib > ibo) then             ! during dipolarization
                 fo_log=-50.
                 if (f2d(ibo,j) > 1.e-50) fo_log=log10(f2d(ibo,j))
                 fb_log=-50.
                 if (fb1(j) > 1.e-50) fb_log=log10(fb1(j))
                 slope=(fo_log-fb_log)/(brad(ibo,j)-rb)
                 do i=ibo+1,ib
                    f_log=fo_log+slope*(brad(i,j)-brad(ibo,j))
                    f2d(i,j)=10.**f_log
                 enddo
              endif
              ! Recalculate Courant numbers
              do i=1,np
                 if (DoUseUniformLGrid) then
                    cl(i,j)=dt1/dvarL*vl(n,i,j,k,m)
                 else
                    cl(i,j)=dt1/dlat(i)*vl(n,i,j,k,m)
                 endif
                 cp(i,j)=dt1/dphi*vp(n,i,j,k,m)
              enddo
           enddo

           ! run drift nrun times
           do nn=1,nrun
              UseUpwind=.false.
              ! When nProc>1, pass fb0, fb1, and f2d
              !send to neigboring Procs

              !this case should be eliminated after testing 
!             if (nProc>1 .and. .not.UseHigherOrder) then
!                 !send f2d ghostcells
!                 call MPI_send(f2d(1:np,MaxLonPar),np,MPI_REAL,iProcRight,&
!                      3,iComm,iError)
!                 call MPI_send(f2d(1:np,MinLonPar:MinLonPar+1),2*np,MPI_REAL,&
!                      iProcLeft,4,iComm,iError)
!                 !recieve f2d ghostcells from neigboring Procs
!                 call MPI_recv(f2d(1:np,iLonLeft),np,MPI_REAL,iProcLeft,&
!                      3,iComm,iStatus_I,iError)
!                 call MPI_recv(f2d(1:np,iLonRight:iLonRight+1),2*np,MPI_REAL,&
!                      iProcRight,4,iComm,iStatus_I,iError)
!
!                 !send fb0 ghostcells
!                 call MPI_send(fb0(MinLonPar:MinLonPar+1),2,MPI_REAL,&
!                      iProcLeft,5,iComm,iError)
!                 call MPI_send(fb0(MaxLonPar),1,MPI_REAL,iProcRight,&
!                      6,iComm,iError)
!                 !recieve fb0 from neigboring Procs
!                 call MPI_recv(fb0(iLonRight:iLonRight+1),2,MPI_REAL,&
!                      iProcRight,5,iComm,iStatus_I,iError)
!                 call MPI_recv(fb0(iLonLeft),1,MPI_REAL,iProcLeft,&
!                      6,iComm,iStatus_I,iError)
!
!                 !send fb1 ghostcells
!                 call MPI_send(fb1(MinLonPar:MinLonPar+1),2,MPI_REAL,&
!                      iProcLeft,7,iComm,iError)
!                 call MPI_send(fb1(MaxLonPar),1,MPI_REAL,iProcRight,&
!                      8,iComm,iError)
!                 !recieve fb1 from neigboring Procs
!                 call MPI_recv(fb1(iLonRight:iLonRight+1),2,MPI_REAL,&
!                      iProcRight,7,iComm,iStatus_I,iError)
!                 call MPI_recv(fb1(iLonLeft),1,MPI_REAL,iProcLeft,&
!                      8,iComm,iStatus_I,iError)
!
!              elseif(nProc>1 .and. UseHigherOrder) then
              !send f2d ghostcells

              if(nProc>1 ) then

                 if (nGhostLonLeft == 1 .and. nGhostLonRight == 2) then
                    call MPI_Sendrecv( &
                        f2d(1:np,MaxLonPar-nGhostLonLeft+1:MaxLonPar), &
                        nGhostLonLeft*np, MPI_REAL, iProcRight, 3, &
                        f2d(1:np,iLonLeft-nGhostLonLeft+1:iLonLeft),    &
                        nGhostLonLeft*np, MPI_REAL, iProcLeft,  3, &
                        iComm, iStatus_I, iError)

                    call MPI_Sendrecv( &
                        f2d(1:np,MinLonPar:MinLonPar+nGhostLonRight-1), &
                        nGhostLonRight*np, MPI_REAL, iProcLeft,  4, &
                        f2d(1:np,iLonRight:iLonRight+nGhostLonRight-1), &
                        nGhostLonRight*np, MPI_REAL, iProcRight, 4, &
                        iComm, iStatus_I, iError)

                 else
                    ! Prepare the buffer on each process to send
                    buf2D_send( :, 1 : ( MaxLonPar - MinLonPar + 1 ) ) = &
                        f2d( :, MinLonPar : MaxLonPar )
                    !! Gather buffer from all processes
                    call MPI_ALLGATHERV( buf2D_send, iSendCount, MPI_REAL, &
                        buf2D_recv, iReceiveCount_P, iDisplacement_P, &
                        MPI_REAL, iComm, iError )
                    !! Store the entire buffer on each process
                    f2d( :, : ) = buf2D_recv( :, : )
                 end if
            
                 !send fb0 ghostcells
                 call MPI_send(fb0(MinLonPar:MinLonPar+nGhostLonRight-1),&
                      nGhostLonRight,MPI_REAL,&
                      iProcLeft,5,iComm,iError)
                 call MPI_send(fb0(MaxLonPar-nGhostLonLeft+1:MaxLonPar),&
                      nGhostLonLeft,MPI_REAL,iProcRight,&
                      6,iComm,iError)
                 ! receive fb0 from neigboring Procs
                 call MPI_recv(fb0(iLonRight:iLonRight+nGhostLonRight-1),&
                      nGhostLonRight,MPI_REAL,&
                      iProcRight,5,iComm,iStatus_I,iError)
                 call MPI_recv(fb0(iLonLeft-nGhostLonLeft+1:iLonLeft),&
                      nGhostLonLeft,MPI_REAL,iProcLeft,&
                      6,iComm,iStatus_I,iError)

                 ! send fb1 ghostcells
                 call MPI_send(fb1(MinLonPar:MinLonPar+nGhostLonRight-1),&
                      nGhostLonRight,MPI_REAL,&
                      iProcLeft,7,iComm,iError)
                 call MPI_send(fb1(MaxLonPar-nGhostLonLeft+1:MaxLonPar),&
                      nGhostLonLeft,MPI_REAL,&
                      iProcRight,8,iComm,iError)
                 ! receive fb1 from neigboring Procs
                 call MPI_recv(fb1(iLonRight:iLonRight+nGhostLonRight-1),&
                      nGhostLonRight,MPI_REAL,&
                      iProcRight,7,iComm,iStatus_I,iError)
                 call MPI_recv(fb1(iLonLeft-nGhostLonLeft+1:iLonLeft),&
                      nGhostLonLeft,MPI_REAL,&
                      iProcLeft,8,iComm,iStatus_I,iError)
              endif

              ! calculate fluxes at the cell surface for finite volume scheme.
              !  dF/dt     = d(vl * F) / dL  +  d(vp * F) / dphi
              !  F_(t+1,i,j) =
              !		F_(t,i,j)	+  cl_(iup,j)	* F_(t,iup,j)
              !				-  cl(idown,j)	* F(t,idown,j)
              !				+  cp_(i,jup)	* F_(t,i,jup)
              !				-  cl(i,jdown)	* F(t,i,jdown)
              !
              !  iup: upstream from i, i-1 < iup < i when cl > 0,
              !		i < iup < i+1 when cl < 0
              !  idown: downstream from i, i < idown < i+1 when cl > 0,
              !		i-1 < iup < i when cl < 0
              !  jup: upstream from j, j-1 < jup < j when cp > 0,
              !		j < jup < j+1 when cl < 0
              !  jdown: downstream from j, j < jdown < j+1 when cp > 0,
              !		j-1 < jup < j when cp < 0

              if (.not.UseHigherOrder) then
                 call FLS_2D(np,nt,iba,fb0,fb1,cl,cp,f2d,fal,fap,fupl,fupp)
              else
                 call FLS_2D_ho(np,nt,iba,fb0,fb1,cl,cp,f2d,fal,fap,fupl,fupp)
              endif

              fal( 0, 1 : nt ) = f2d( 1, 1 : nt )
              ! When nProc>1 pass needed ghost cell info for fap,
              ! 	fupp, cp, and cl
              if (nProc>1) then

                 ! Old MPI calls for sending fap ghostcells
!!$                 call MPI_send(fap(:,MaxLonPar-1:MaxLonPar),2*np,MPI_REAL,&
!!$                      iProcRight,9,iComm,iError)
!!$                 !recieve fap from neigboring Procs
!!$                 call MPI_recv(fap(:,iLonLeft-1:iLonLeft),2*np,MPI_REAL,&
!!$                      iProcLeft,9,iComm,iStatus_I,iError)


!!$                 MPI_SENDRECV Template
!!$                 call MPI_SENDRECV(sendbuf, sendcount, sendtype, &
!!$                      dest, sendtag, recvbuf, recvcount, recvtype, &
!!$                      source, recvtag, comm, status, ierror)

                 ! NEW MPI calls utilizing sendrecv for sending fap
                 ! ghostcells
                 buf2D_send_1( :, 1 : 2 ) = &
                      fap( :, MaxLonPar - 1 : MaxLonPar )
                 buf2D_recv_1( :, 1 : 2 ) = -1.
                 call MPI_sendrecv( buf2D_send_1, 2 * np, MPI_REAL, &
                      iProcRight, 10, buf2D_recv_1, 2 * np, MPI_REAL, &
                      iProcLeft, 10, iComm, iStatus_I, iError)
                 fap( :, iLonLeft - 1 : iLonLeft ) = buf2D_recv_1( :, 1 : 2 )

!!$                 ! Old MPI calls for sending fupp ghostcells
!!$                 call MPI_send(fupp(:,MaxLonPar-1:MaxLonPar),2*np,MPI_REAL,&
!!$                      iProcRight,10,iComm,iError)
!!$                 !recieve fupp from neigboring Procs
!!$                 call MPI_recv(fupp(:,iLonLeft-1:iLonLeft),2*np,MPI_REAL,&
!!$                      iProcLeft,10,iComm,iStatus_I,iError)

                 ! NEW MPI calls utilizing sendrecv for sending fupp
                 ! ghostcells
                 buf2D_send_1( :, 1 : 2 ) = &
                      fupp( :, MaxLonPar - 1 : MaxLonPar )
                 buf2D_recv_1( :, 1 : 2 ) = -1.
                 call MPI_sendrecv( buf2D_send_1, 2 * np, MPI_REAL, &
                      iProcRight, 10, buf2D_recv_1, 2 * np, MPI_REAL, &
                      iProcLeft, 10, iComm, iStatus_I, iError )
                 fupp( :, iLonLeft - 1 : iLonLeft ) = buf2D_recv_1( :, 1 : 2 )

!!$                 ! Old MPI calls for sending cp ghostcells
!!$                 call MPI_send(cp(:,MaxLonPar-1:MaxLonPar),2*np,MPI_REAL,&
!!$                      iProcRight,11,iComm,iError)
!!$                 !recieve cp from neigboring Procs
!!$                 call MPI_recv(cp(:,iLonLeft-1:iLonLeft),2*np,MPI_REAL,&
!!$                      iProcLeft,11,iComm,iStatus_I,iError)

                 ! NEW MPI calls utilizing sendrecv for sending cp
                 ! ghostcells
                 buf2D_send_1( :, 1 : 2 ) = &
                      cp( :, MaxLonPar - 1 : MaxLonPar )
                 buf2D_recv_1( :, 1 : 2 ) = -1.
                 call MPI_sendrecv( buf2D_send_1, 2 * np, MPI_REAL, &
                      iProcRight, 10, buf2D_recv_1, 2 * np, MPI_REAL, &
                      iProcLeft, 10, iComm, iStatus_I, iError )
                 cp(:, iLonLeft - 1:iLonLeft ) = buf2D_recv_1( :, 1 : 2 )

                 ! NEW MPI calls utilizing sendrecv for sending cl
                 ! ghostcells
                 buf2D_send_1( :, 1 : 2 ) = &
                      cl( :, MaxLonPar - 1 : MaxLonPar )
                 buf2D_recv_1( :, 1 : 2 ) = -1.
                 call MPI_sendrecv( buf2D_send_1, 2 * np, MPI_REAL, &
                      iProcRight, 10, buf2D_recv_1, 2 * np, MPI_REAL, &
                      iProcLeft, 10, iComm, iStatus_I, iError )
                 cl(:, iLonLeft - 1:iLonLeft ) = buf2D_recv_1( :, 1 : 2 )

              endif

              f2d0(:,:)=f2d(:,:)   ! save f2 in the current time step
              jloop: do j=MinLonPar,MaxLonPar
                 j_1=j-1
                 j_2=j-2
                 if (j_1 < 1) j_1=j_1+nt
                 if (j_2 < 1) j_2=j_2+nt
                 iloop: do i=2,iba(j)
                    if ( DoUseUniformLGrid ) then
                       f2d(i,j) = f2d0(i,j) + &
                            cl(i-1,j) * fal(i-1,j) - cl(i,j) * fal(i,j) + &
                            cp(i,j_1) * fap(i,j_1) - cp(i,j) * fap(i,j)
                    else
                       f2d(i,j) = f2d0(i,j) + &
                            dt1 / dlat(i) * &
                            ( vl(n,i-1,j,k,m) * fal( i-1, j ) - &
                            vl(n,  i,j,k,m)   * fal( i  , j ) ) + &
                            cp(i,j_1) * fap(i,j_1) - cp(i,j) * fap(i,j)
                    endif
                    if (f2d(i,j) < 0.) then
                       if (f2d(i,j) > -1e-30) then
                          f2d(i,j) = 0.
                       else
                          if (.not.UseHigherOrder) then
                             write(*,*) 'IM WARNING1: ', &
                                  'f2d<0 in drift at n,i,j,k,m,f2=', &
                                  n, i , j, k, m, f2d(i,j)
                             if(UseUpwind) call CON_stop('CIMI ERROR1: f2d<0')
                             write(*,*)'IM WARNING: ', &
                                  'Retrying step with upwind scheme'
                             UseUpwind=.true.
                             EXIT jloop
                          else
                             ! calculate with upwind scheme
                             fal(i  ,j)=fupl(i  ,j)
                             fal(i-1,j)=fupl(i-1,j)
                             fap(i  ,j)=fupp(i  ,j)
                             fap(i,j_1)=fupp(i,j_1)
                             f2d(i,j) = f2d0(i,j) + &
                                  cl(i-1,j) * fal(i-1,j) &
                                  - cl(i,j) * fal(i,j) + &
                                  cp(i,j_1) * fap(i,j_1) &
                                  - cp(i,j) * fap(i,j)
                             if (f2d(i,j) < 0.) then
                                write(*,*) &
                                     'IM WARNING2: f2d<0 in upwind drift',&
                                     ' at n,i,j,k,m,f2d =',n,i,j,k,m,f2d(i,j)
                                write(*,*)'IM WARNING: '//&
                                     'upwind scheme failed, making iba(j)=i'
                                write(*,*)'IM WARNING: '//&
                                     'repeated failure may need to be examined'
                                if ( IsStrictDrift ) then
                                   write(*,'(a,i4)') 'iba =',iba(j)
                                   write(*,'(a,1p3E11.3)') &
                                        'IM: f2d(i,j),f2d0(i,j),f2=', &
                                        f2d(i,j),f2d0(i,j),f2(n,i,j,k,m)
                                   write(*,'(a,1p2E11.3)') &
                                        'IM: fupl(i-1,j),fupl(i,j)= ', &
                                        fupl(i-1,j),fupl(i,j)
                                   write(*,'(a,1p2E11.3)') &
                                        'IM:   cl(i-1,j),  cl(i,j)= ', &
                                        cl(i-1,j),cl(i,j)
                                   write(*,'(a,1p2E11.3)') &
                                        'IM: fupp(i,j-1),fupp(i,j)= ', &
                                        fupp(i,j-1),fupp(i,j)
                                   write(*,'(a,1p2E11.3)') &
                                        'IM:   cp(i,j-1),  cp(i,j)= ', &
                                        cl(i,j-1),cl(i,j)
                                   call CON_stop('IM: CIMI ERROR2 in driftIM')
                                else
                                   f2d(i,j)=0.0
                                   iba(j)=i
                                endif
                             endif
                             if (i >= 3) then
                                f_upwind = f2d0(i-1,j) + &
                                     cl(i-2,j) * fal(i-2,j) &
                                     - cl(i-1,j) * fal(i-1,j) + &
                                     cp(i-1,j_1) * fap(i-1,j_1) &
                                     - cp(i-1,j) * fap(i-1,j)
                                if (f_upwind > 0.) f2d(i-1,j) = f_upwind
                             endif
                             f_upwind = f2d0(i,j_1) + &
                                  cl(i-1,j_1) * fal(i-1,j_1) &
                                  - cl(i,j_1) * fal(i,j_1) + &
                                  cp(i,j_2) * fap(i,j_2) &
                                  - cp(i,j_1) * fap(i,j_1)
                             if (f_upwind > 0.) f2d(i-1,j) = f2d(i,j_1)
                          endif
                       endif
                    endif
                 enddo iloop
                 ! Calculate gain or loss at the outer boundary
                 if ( DoUseUniformLGrid ) then
                    dPartLocal_C(j) = &
                         -cl( iba(j), j ) * fal( iba(j), j ) * &
                         d4Element_C(n,iba(j),k,m)
                 else
                    dPartLocal_C(j) = &
                         -dt1 / dlat( iba(j) ) * &
                         vl(n,iba(j),j,k,m) * fal( iba(j), j ) * &
                         d4Element_C(n,iba(j),k,m)
                 endif
                 dEnerLocal_C(j)=ekev(n,iba(j),j,k,m)*dPartLocal_C(j)
              enddo jloop

              ! When regular scheme fails, try again with upwind scheme before
              ! returning an error
              if (UseUpwind) then
                 fupl(0,1:nt)=f2d(1,1:nt)
                 do j=MinLonPar,MaxLonPar
                    j_1=j-1
                    if (j_1 < 1) j_1=j_1+nt
                    iLoopUpwind: do i=2,iba(j)
                       if ( DoUseUniformLGrid ) then
                          f2d(i,j) = &
                               f2d0(i,j) + &
                               cl(i-1, j ) * fupl(i-1, j ) - &
                               cl(  i, j ) * fupl(  i, j ) + &
                               cp(i,j_1) * fupp(i,j_1) - cp(i,j) * fupp(i,j)
                       else
                          f2d(i,j) = &
                               f2d0(i,j) + &
                               dt1 / dlat(i) * &
                               ( vl(n,i-1,j,k,m) * fupl(i-1,j) - &
                               vl(n,i,j,k,m)   * fupl(i,j) ) + &
                               cp(i,j_1) * fupp(i,j_1) - cp(i,j) * fupp(i,j)
                       endif
                       if (f2d(i,j) < 0.) then
                          if (f2d(i,j) > -1e-30) then
                             f2d(i,j)=0.
                          else
                             write(*,*)'IM WARNING3: f2d<0 in drift ', &
                                  'n,i,j,k,m,f2=', n, i, j, k, m, f2d(i,j)
                             write(*,*)'IM WARNING: '//&
                                  'upwind scheme failed, making iba(j)=i'
                             write(*,*)'IM WARNING: '//&
                                  'repeated failure may need to be examined'
                             if ( IsStrictDrift ) then
                                write(*,'(a,1p3E11.3)') &
                                     'IM: f2d(i,j),f2d0(i,j),f2=', &
                                     f2d(i,j),f2d0(i,j),f2(n,i,j,k,m)
                                write(*,'(a,1p2E11.3)') &
                                     'IM: fupl(i-1,j),fupl(i,j)= ', &
                                     fupl(i-1,j),fupl(i,j)
                                write(*,'(a,1p2E11.3)') &
                                     'IM:   cl(i-1,j),  cl(i,j)= ', &
                                     cl(i-1,j),cl(i,j)
                                write(*,'(a,1p2E11.3)') &
                                     'IM: fupp(i-1,j),fupp(i,j)= ', &
                                     fupp(i,j-1),fupp(i,j)
                                write(*,'(a,1p2E11.3)') &
                                     'IM:   cp(i-1,j),  cp(i,j)= ', &
                                     cl(i,j-1),cl(i,j)
                                call CON_stop('IM: CIMI ERROR3 in driftIM')
                             else
                                f2d(i,j)=0.0
                                iba(j)=i
                             endif
                             EXIT iLoopUpwind
                          endif
                       endif
                    enddo iLoopUpwind
                    ! Calculate gain or loss at the outer boundary
                    if ( DoUseUniformLGrid ) then
                       dPartLocal_C(j) = &
                            -cl(iba(j),j) * fupl(iba(j),j) * &
                            d4Element_C(n,iba(j),k,m)
                    else
                       dPartLocal_C(j) = &
                            -dt1 / dlat(iba(j)) * &
                            vl(n,iba(j),j,k,m) * fupl(iba(j),j) * &
                            d4Element_C(n,iba(j),k,m)
                    endif
                    dEnerLocal_C(j)=ekev(n,iba(j),j,k,m)*dPartLocal_C(j)
                 enddo
              endif
              ! sum all dEner to root proc
              if(nProc>1) then
                 call MPI_REDUCE (sum(dPartLocal_C(MinLonPar:MaxLonPar)), &
                      dPart, 1, MPI_REAL, MPI_SUM, 0, iComm, iError)
                 call MPI_REDUCE (sum(dEnerLocal_C(MinLonPar:MaxLonPar)), &
                      dEner, 1, MPI_REAL, MPI_SUM, 0, iComm, iError)
              else
                 dPart=sum(dPartLocal_C)
                 dEner=sum(dEnerLocal_C)
              endif

              if( iProc == 0 ) then
                 if (dPart > 0.) driftin(n)=driftin(n)+dEner
                 if (dPart < 0.) driftout(n)=driftout(n)+dEner
              else
                 driftin(n)=0.
                 driftout(n)=0.
              endif
           enddo          ! end of do nn=1,nrun
           !f2(n,1:np,1:nt,k,m)=f2d(1:np,1:nt)
           where(f2d(1:np,1:nt).eq.0.0) f2(n,1:np,1:nt,k,m)=1e-50
           where(f2d(1:np,1:nt).gt.0.0) f2(n,1:np,1:nt,k,m)=f2d(1:np,1:nt)
        enddo kloop
     enddo mloop
  enddo nloop

  ! Update ib0
  ib0(1:nt) = iba(1:nt)

end subroutine driftIM
!==============================================================================
subroutine charexchangeIM(np,nt,nm,nk,nspec,iba,achar,f2)

  ! Update f2 due to charge exchange loss

  use ModCimi,       ONLY: MinLonPar,MaxLonPar

  implicit none

  integer, intent(in):: np, nt, nm, nk, nspec, iba(nt)
  real, intent(in):: achar(nspec,np,nt,nm,nk)
  real, intent(inout):: f2(nspec,np,nt,nm,nk)

  integer:: n, i, j
  !----------------------------------------------------------------------------
  do n=1,nspec-1
     do j=MinLonPar,MaxLonPar
        do i=1,iba(j)
           f2(n,i,j,1:nm,1:nk)=f2(n,i,j,1:nm,1:nk)*achar(n,i,j,1:nm,1:nk)
        enddo
     enddo
  enddo

end subroutine charexchangeIM
!==============================================================================
subroutine StDiTime(dt,vel,volume,iba)

  ! Calculate the strong diffusion lifetime for electrons.

  use ModCimi,       ONLY: SDtime
  use ModCimiGrid,   ONLY: np,nt,nm,nk, xlatr,MinLonPar,MaxLonPar
  use ModCimiPlanet, ONLY: nspec, rc, re_m, xme => dipmom

  implicit none
  
  real:: vel(nspec,np,nt,nm,nk), volume(np,nt)
  real:: eb, xmer3, sinlat2, bi, vBe, sdtime1, dt
  integer:: i, j, k, m, iba(nt)
  !----------------------------------------------------------------------------
  eb = 0.25                         ! fraction of back scatter e-
  xmer3 = xme/(rc*re_m)**3

  do j = MinLonPar,MaxLonPar
     do i = 1,iba(j)
        !              xlat2=xlati(i)*xlati(i)!- from M.-Ch., Aug 1 2007
        !              Bi=xmer3*sqrt(3.*xlat2+1.)
        sinlat2=sin(xlatr(i))*sin(xlatr(i))
        Bi=xmer3*sqrt(3.*sinlat2+1.)      ! magnetic field at ionosphere

        vBe=2.*volume(i,j)*Bi/(1.-eb)
        do k=1,nm
           do m=1,nk
              SDtime1=vBe/vel(nspec,i,j,k,m) ! strong diff T,(gamma*mo/p = 1/v)
              SDtime(i,j,k,m)=exp(-dt/SDtime1)
           enddo
        enddo
     enddo
  enddo

end subroutine StDiTime
!==============================================================================
subroutine StrongDiff(iba)

  ! Calculate the change of electron psd (f2) by strong diffusion

  use ModCimi,       ONLY: SDtime,f2
  use ModCimiGrid,   ONLY: np,nt,nm,nk,MinLonPar,MaxLonPar
  use ModCimiPlanet, ONLY: nspec

  implicit none

  integer:: iba(nt),i,j,k,m
  !----------------------------------------------------------------------------
  do j=MinLonPar,MaxLonPar
     do i=2,iba(j)
        do m=1,nk
           do k=1,nm
              f2(nspec,i,j,k,m)=f2(nspec,i,j,k,m)*SDtime(i,j,k,m)
           enddo
        enddo
     enddo
  enddo

end subroutine StrongDiff
!==============================================================================
subroutine CalcDecay_cimi(deltaT)

  ! Calculate the change of Ring Current phase space density
  ! (PSD - f2 variable) resulting from an exponential decay rate
  ! specified by DecayTimescale (in seconds) in PARAM.in by the user.
  ! The Decay term is only applied to the ion species; electrons are
  ! not affected since electron PSD contains both ring current and
  ! radiation belt electrons.  Rapid loss of electron PSD is controlled
  ! with the StrongDiff routine immediately above.
  !
  ! Version History:
  ! 2018-02-20 CMK: Added and tested

  use ModCimi,       	ONLY: f2, DecayTimescale, UseElectronDecay
  use ModCimiGrid,   	ONLY: np, nt, nm, nk, MinLonPar, MaxLonPar
  use ModCimiPlanet, 	ONLY: nspec
  use ModCimiTrace, 	ONLY: iba

  implicit none

  real, intent(in) :: deltaT

  integer:: n,i,j,k,m, maxSpec
  real:: DecayRate
  !----------------------------------------------------------------------------
  DecayRate = EXP( -( deltaT / DecayTimescale ) )
  
  maxSpec = nspec - 1
  if (UseElectronDecay) maxSpec = nspec

  f2(1:maxSpec,:,MinLonPar:MaxLonPar,:,:) = &
         f2(1:maxSpec,:,MinLonPar:MaxLonPar,:,:) * DecayRate

end subroutine CalcDecay_cimi
!==============================================================================
subroutine lossconeIM(iba,alscone,f2,ionization,ekev)

  ! Calculate the change of f2 due to lossconeIM loss

  use ModCimi, ONLY: MinLonPar,MaxLonPar, nspec, np, nt, nm, nk, neng, energy,&
        precipEnergyLoss, precipNumberLoss
  use ModCimiTrace, ONLY: UsePrecipEnergyLoss
  use ModCimiGrid, ONLY: d4Element_C

  implicit none

  integer, intent(in):: iba(nt)
  real, intent(in):: alscone(nspec,np,nt,nm,nk), &
                     ionization(nspec,np,nt,nm,nk), ekev(nspec,np,nt,nm,nk)
  real, intent(inout):: f2(nspec,np,nt,nm,nk)

  integer:: n, i, j, k, m, m2, iLow, iHigh
  real :: newf2(nm), newEnergy, weightLow, weightHigh, egrid(nspec, neng+1)
  !----------------------------------------------------------------------------
  egrid(:,1:neng) = energy(:,1:neng)
  egrid(:,neng+1) = 1e10
  precipEnergyLoss(:,:,MinLonPar:MaxLonPar,:) = 0.0
  precipNumberLoss(:,:,MinLonPar:MaxLonPar,:) = 0.0
  if (UsePrecipEnergyLoss) then
    do k=1,nk; do j = MinLonPar,MaxLonPar; do i=1,iba(j); do n=1,nspec
      newf2 = 0
      Minv: do m=1,nm
        if (ionization(n,i,j,m,k) <= 0.0) then
          newf2(m) = newf2(m) + f2(n,i,j,m,k)
          cycle Minv
        end if
        ! new energy is fraction of original energy
        newEnergy = ekev(n,i,j,m,k) * &
            (1 - ionization(n,i,j,m,k))
        ! reinterpolate f2 grid based on new energy grid assuming that
        ! pitch angle does not change, meaning that only the 
        ! m invariant grid is changing

        ! This flux always stays put
        ! Note that more flux will still get added to newf2(m) if ionization
        ! is close to 0
        newf2(m) = newf2(m) + f2(n,i,j,m,k) * alscone(n,i,j,m,k)
        
        if (newEnergy >= ekev(n,i,j,1,k)) then
          mInvScan: do m2 = 1, nm-1
            if (newEnergy >= ekev(n,i,j,m2,k) .and. &
                newEnergy < ekev(n,i,j,m2+1,k)) then
              iLow = m2; iHigh = m2 + 1
              exit mInvScan
            else if (m2 == nm-1) then
              ! Handle edge case that should never happen just in case
              iLow = nm - 1
              iHigh = nm  
            end if
          end do mInvScan

          weightLow  = (newEnergy - ekev(n,i,j,iLow,k)) / &
                      (ekev(n,i,j,iHigh,k) - ekev(n,i,j,iLow,k))
          weightHigh = 1. - weightLow
          weightLow  = weightLow  * (1 - alscone(n,i,j,m,k))
          weightHigh = weightHigh * (1 - alscone(n,i,j,m,k))

          newf2(iLow)  = newf2(iLow)  + weightLow  * f2(n,i,j,m,k)
          newf2(iHigh) = newf2(iHigh) + weightHigh * f2(n,i,j,m,k)
        end if
        
        ! if below lowest energy bin, add precip losses to lowest bin
        if (ekev(n,i,j,m,k) < energy(n,1)) then
          precipEnergyLoss(n,i,j,1) = precipEnergyLoss(n,i,j,1) + &
              f2(n,i,j,m,k) * (1 - alscone(n,i,j,m,k)) * &
              (newEnergy - ekev(n,i,j,m,k)) * d4Element_C(n,i,k,m)
          precipNumberLoss(n,i,j,1) = precipNumberLoss(n,i,j,1) - &
              f2(n,i,j,m,k) * (1-alscone(n,i,j,m,k)) * &
              ionization(n,i,j,m,k) * d4Element_C(n,i,k,m)
        else
          ! Otherwise, loop through energy bins to find where to add losses
          SavePrecip: do m2 = 1, neng
            if (ekev(n,i,j,m,k) >= energy(n,m2) .and. &
                ekev(n,i,j,m,k) < energy(n,m2+1)) then
              weightLow = (ekev(n,i,j,m,k) - energy(n,m2)) / &
                  (energy(n,m2+1) - energy(n,m2))
              weightHigh = 1. - weightLow
              ! Add precip losses to energy bins that border current energy
              ! Energy losses = f2 * (1-alscone) * (newEnergy-ekev) which is 
              ! equivalent to f2 * (1-alscone) * ionization * ekev
              ! Number losses = - f2 * (1-alscone) * ionization
              ! Both values are negative
              precipEnergyLoss(n,i,j,m2) = precipEnergyLoss(n,i,j,m2) + &
                  f2(n,i,j,m,k) * (1-alscone(n,i,j,m,k)) * &
                  (newEnergy - ekev(n,i,j,m,k)) * weightLow * &
                  d4Element_C(n,i,k,m)
              precipEnergyLoss(n,i,j,m2+1) = precipEnergyLoss(n,i,j,m2+1) + &
                  f2(n,i,j,m,k) * (1-alscone(n,i,j,m,k)) * &
                  (newEnergy - ekev(n,i,j,m,k)) * weightHigh * &
                  d4Element_C(n,i,k,m)
              precipNumberLoss(n,i,j,m2) = precipNumberLoss(n,i,j,m2) - &
                  f2(n,i,j,m,k) * (1-alscone(n,i,j,m,k)) * &
                  ionization(n,i,j,m,k) * weightLow * d4Element_C(n,i,k,m)
              precipNumberLoss(n,i,j,m2+1) = precipNumberLoss(n,i,j,m2+1) - &
                  f2(n,i,j,m,k) * (1-alscone(n,i,j,m,k)) * &
                  ionization(n,i,j,m,k) * weightHigh * d4Element_C(n,i,k,m)
              exit SavePrecip
            end if
          end do SavePrecip 
        end if
        ! Also add precip losses to summed value
        precipEnergyLoss(n,i,j,neng+2) = precipEnergyLoss(n,i,j,neng+2) + &
            f2(n,i,j,m,k) * (1-alscone(n,i,j,m,k)) * &
            (newEnergy - ekev(n,i,j,m,k)) * d4Element_C(n,i,k,m)
        precipNumberLoss(n,i,j,neng+2) = precipNumberLoss(n,i,j,neng+2) - &
            f2(n,i,j,m,k) * (1-alscone(n,i,j,m,k)) * ionization(n,i,j,m,k) &
            * d4Element_C(n,i,k,m)
      end do Minv
      f2(n,i,j,:,k) = newf2(:)
    enddo; enddo; enddo; enddo;
    return 

  end if

  do k=1,nk
    do m=1,nm
      do j=MinLonPar,MaxLonPar
        do i=1,iba(j)
          do n=1,nspec
            if (alscone(n,i,j,m,k) < 1.) &
                f2(n,i,j,m,k)=f2(n,i,j,m,k)*alscone(n,i,j,m,k)
          enddo
        enddo
      enddo
    enddo
  enddo

end subroutine lossconeIM
!==============================================================================
subroutine sume(xle)

  ! Update rbsum and xle
  !
  ! Input: f2,ekev,iba
  ! Input/Output: rbsum,xle

  use ModCimi,       ONLY: rbsumLocal
  use ModCimiTrace, ONLY: iba
  use ModCimiGrid,   ONLY: nProc,iProc,iComm
  use ModCimiPlanet, ONLY: nspec
  use ModMPI

  implicit none

  real, intent(inout):: xle(nspec)

  integer:: n,i,j,k,m,iError
  real:: rbsumLocal0,xleChange,xleChangeLocal
  !----------------------------------------------------------------------------
  do n=1,nspec
     rbsumLocal0=rbsumLocal(n)

     call calc_rbsumlocal(n)

     xleChangeLocal=rbsumLocal(n)-rbsumLocal0

     if (nProc >1) call MPI_REDUCE (xleChangeLocal, xleChange, 1, MPI_REAL, &
          MPI_SUM, 0, iComm, iError)

     if( iProc == 0 ) then
        xle(n)=xle(n)+xleChange
     else
        xle(n)=0.0
     endif
  enddo

end subroutine sume
!==============================================================================
subroutine calc_rbsumlocal(iSpecies)

  use ModCimi,       ONLY: f2,rbsumLocal
  use ModCimiGrid,   ONLY: np,nm,nk,MinLonPar,MaxLonPar,d4Element_C
  use ModCimiTrace, ONLY: iba, ekev

  implicit none

  integer, intent(in) :: iSpecies

  real::     weight
  integer:: i,j,k,m
  !----------------------------------------------------------------------------
  rbsumLocal(iSpecies)=0.
  do j=MinLonPar,MaxLonPar
     do i=1,iba(j)
        do k=1,nm
           do m=1,nk
              weight=f2(iSpecies,i,j,k,m)*d4Element_C(iSpecies,i,k,m) &
                   *ekev(iSpecies,i,j,k,m)
              rbsumLocal(iSpecies)=rbsumLocal(iSpecies)+weight  ! rbsum in keV
           enddo
        enddo
     enddo
  enddo

end subroutine calc_rbsumlocal
!==============================================================================
subroutine sume_cimi(OperatorName)

  ! Update rbsum and xle
  !
  ! Input: f2,ekev,iba
  ! Input/Output: rbsum,xle

  ! in CIMI: eChangeOperator=xle(ns,ir,ip,nOperator,je+2) Here we create
  !   	one array for all operators (plus one dimension)
  !
  ! eTimeAccumultv=esum(ns,ir,ip,je+2) No need for additional dimension
  ! 	because it's total energy

  use ModCimi,       	ONLY: &
       f2, rbsum => rbsumLocal, rbsumGlobal, &
       rcsum => rcsumLocal, rcsumGlobal, &
       xle => eChangeOperator_VICI, ple => pChangeOperator_VICI, &
       eChangeGlobal, eChangeLocal, &
       esum => eTimeAccumult_ICI, psum => pTimeAccumult_ICI
  use ModCimiTrace, 	ONLY: iba, ekev, ro, iw2
  use ModCimiGrid,   	ONLY: &
       nProc, iProc, iComm, MinLonPar, MaxLonPar, d4Element_C, &
       ip => np, ir => nt, im => nm, ik => nk, je => neng
  use ModCimi, 		ONLY: Energy, Ebound
  use ModCimiPlanet, 	ONLY: nspec
  use ModMPI


  implicit none

  integer, intent(in):: OperatorName   ! defined in ModCimi

  real::    weight, ekev1, weighte, dee, dpe
  integer:: n, i, j, k, m, iError, kk
  real:: gride1(0:je+1), e0(ip,ir,je+2), p0(ip,ir,je+2)
  !----------------------------------------------------------------------------
  gride1(0)=0.           ! Set up gride1(0) and gride1(je+1)
  gride1(je+1)=1.e10     ! arbitrary large number

  ! Calculate esum, psum, etc.
  do n=1,nspec
     rbsum(n)=0.
     rcsum(n)=0.
     eChangeLocal(n, OperatorName) = 0.
     e0(1:ip,MinLonPar:MaxLonPar,1:je+2)=&
          esum(n,1:ip,MinLonPar:MaxLonPar,1:je+2)
     p0(1:ip,MinLonPar:MaxLonPar,1:je+2)=&
          psum(n,1:ip,MinLonPar:MaxLonPar,1:je+2)
     esum(n,1:ip,MinLonPar:MaxLonPar,1:je+2)=0.
     psum(n,1:ip,MinLonPar:MaxLonPar,1:je+2)=0.
     gride1(1:je)=Ebound(n,1:je)

     do j=MinLonPar,MaxLonPar
        do i=1,iba(j)
           do m=1,ik
              do k=1,iw2(n,m)
                 ekev1=ekev(n,i,j,k,m)
                 weight=d4Element_C(n,i,k,m)*f2(n,i,j,k,m)
                 weighte=ekev1*weight
                 psum(n,i,j,je+2)=psum(n,i,j,je+2)+weight
                 esum(n,i,j,je+2)=esum(n,i,j,je+2)+weighte
                 rbsum(n)=rbsum(n)+weighte
                 if (ro(i,j) <= 6.6) rcsum(n)=rcsum(n)+weighte
                 kkloop: do kk=1,je+1
                    if ( ekev1 > gride1(kk-1) .and. &
                         ekev1 <= gride1(kk) ) then
                       psum(n,i,j,kk)=psum(n,i,j,kk)+weight
                       esum(n,i,j,kk)=esum(n,i,j,kk)+weighte
                       EXIT kkloop
                    endif
                 enddo kkloop
              enddo
           enddo

           do kk=1,je+2
              dee = esum(n,i,j,kk) - e0(i,j,kk)
              xle(n,i,j,kk,OperatorName) = &
                   xle(n,i,j,kk,OperatorName) + dee
              dpe=psum(n,i,j,kk)-p0(i,j,kk)
              ple(n,i,j,kk,OperatorName) = &
                   ple(n,i,j,kk,OperatorName) + dpe
           enddo

           eChangeLocal(n, OperatorName) = &
                eChangeLocal(n, OperatorName) + &
                xle(n,i,j,je+2,OperatorName)

        enddo                ! end of do i=1,iba(j)
     enddo                   ! end of do j=MinLonPar,MaxLonPar

     if (nProc > 1) then

        call MPI_REDUCE(&
             rbsum(n), rbsumGlobal(n), 1, &
             MPI_REAL, MPI_SUM, 0, iComm, iError)
        call MPI_REDUCE(&
             rcsum(n), rcsumGlobal(n), 1, &
             MPI_REAL, MPI_SUM, 0, iComm, iError)
        call MPI_REDUCE(&
             eChangeLocal(n, OperatorName), &
             eChangeGlobal(n, OperatorName), 1, &
             MPI_REAL, MPI_SUM, 0, iComm, iError)

     else

        rbsumGlobal(n) = rbsum(n)
        rcsumGlobal(n) = rcsum(n)
        eChangeGlobal(n,OperatorName) = &
             eChangeLocal(n,OperatorName)

     endif

  enddo                      ! end of do n=1,nSpecies
  
end subroutine sume_cimi
!==============================================================================
subroutine cimi_output( &
     np, nt, nm, nk, nspec, neng, npit, iba, ftv, f2, ekev, &
     sinA, energy, sinAo, delE, dmu, amu_I, xjac, pp, xmm, dmm, &
     dk, xlat, dphi, re_m, Hiono, vl, vp, flux, fac, phot, &
     Ppar_IC, Pressure_IC, PressurePar_IC, vlEa, vpEa, psd,ro,xmlto )
  !----------------------------------------------------------------------------
  ! Routine calculates CIMI output, flux, fac and phot from f2
  ! Routine also converts the particle drifts from (m,K) space to (E,a) space
  !
  ! Input: np,nt,nm,nk,nspec,neng,npit,iba,ftv,f2,ekev,sinA,energy,sinAo,xjac
  !        delE,dmu,amu_I,pp,xmm,dmm,dk,xlat,dphi,re_m,Hiono,vl,vp
  ! Output: flux,fac,phot,Ppar_IC,Den_IC,Temp_IC,vlEa,vpEa,psd

  use ModGmCimi, ONLY: Den_IC, Temp_IC
  use ModConst,   ONLY: cProtonMass
  use ModNumConst, ONLY: cPi, cDegToRad
  use ModCimiGrid, ONLY: iProc,nProc,iComm,MinLonPar,MaxLonPar,&
       iProcLeft, iLonLeft, iProcRight, iLonRight
  use ModMpi
  use ModCimiBoundary, ONLY: CIMIboundary, Outputboundary
  use ModCimi, ONLY: vdr_q1,vdr_q3,vgyr_q1,vgyr_q3,eng_q1, &
       eng_q3,vexb,dif_q1,dif_q3,Part_phot
  use ModCimiUtil, ONLY: lintp2aIM

  implicit none

  integer, intent(in):: np, nt, nm, nk, nspec, neng, npit, iba(nt)
  real, intent(in):: &
       ftv(np,nt), f2(nspec,np,nt,nm,nk), sinA(np,nt,nk), &
       energy(nspec,neng), sinAo(npit), xjac(nspec,np,nm), &
       delE(nspec,neng), dmu(npit), amu_I(nspec), pp(nspec,np,nt,nm,nk), &
       xmm(nspec,0:nm+1),dmm(nspec,nm), dk(nk), xlat(np),dphi, re_m, Hiono, &
       vl(nspec,0:np,nt,nm,nk), vp(nspec,0:np,nt,nm,nk),&
       ro(np,nt),xmlto(np,nt)

  real, intent(inout):: &
       flux(nspec,np,nt,neng,npit), fac(np,nt), phot(nspec,np,nt), &
       Ppar_IC(nspec,np,nt), Pressure_IC(nspec,np,nt), &
       PressurePar_IC(nspec,np,nt), &
       vlEa(nspec,np,nt,neng,npit), vpEa(nspec,np,nt,neng,npit), &
       psd(nspec,np,nt,nm,nk)

  real:: ftv1, aloge(nspec,neng), rion, ekev(nspec,np,nt,nm,nk)
  real:: flux2D(nm,nk), vl2D(nm,nk), vp2D(nm,nk)
  real:: sinA1D(nk),cosA2(nk),flx,ekev2D(nm,nk),flx_lo,pf(nspec), &
       delEE(nspec,neng),cosAo2(npit)
  real:: vl_lo,vp_lo
  real:: sina1,sina0,dcosa
  real:: amu1,psd1,fave(nspec,np,nt,neng)
  real:: xlatr(np),eta(nspec,np,nt,nm,nk)
  real:: detadi,detadj,dwkdi,dwkdj
  real:: Pressure0, Pressure1, PressurePar1, Coeff
  integer:: iStatus_I(MPI_STATUS_SIZE), iError
  logical, parameter:: DoCalcFac=.true.

  integer:: i, j, k, m, n, j1, j_1
!!!!!!!!!!!
  !  real:: vdr_q1(nspec,np,nt),vdr_q3(nspec,np,nt),vgyr_q1(nspec,np,nt),vgyr_q3(nspec,np,nt)
  !  real:: eng_q1(nspec,np,nt),eng_q3(nspec,np,nt),vexb(nspec,np,nt),dif_q1(nspec,np,nt)
  !  real:: Part_phot(nspec,np,nt,neng),dif_q3(nspec,np,nt)
  integer:: k_q1,k_q3
  real:: p_min,p_max,p_q1,p_q3
!!!!!!!!!!
  !----------------------------------------------------------------------------
  flux=0.
  fac=0.
  eta=0.
  phot=0.
  Ppar_IC = 0.
  PressurePar_IC = 0.
  Pressure_IC = 0.
  psd = 0.

  ! Some constants for pressure, fac calculations
  rion=re_m+Hiono*1000.                      ! ionosphere distance in meter
  do n=1,nspec
     ! phot(nPa)
     pf(n)=4*cPi*1e4/3*sqrt(2*cProtonMass*amu_I(n))*sqrt(1.6e-16)*1e9
     delEE(n,1:neng) = delE(n,1:neng) * sqrt(energy(n,1:neng))
     aloge(n,1:neng) = log10(energy(n,1:neng))
  enddo
  !  delEE=delE*sqrt(energy)
  xlatr=xlat*cDegToRad

  ! Calculate CIMI ion density (m^-3), Den_IC,
  ! and flux (cm^-2 s^-1 keV^-1 sr^-1)
  ! at fixed energy & pitch-angle grids

  ! aloge=log10(energy)
  jloop1: do j=MinLonPar,MaxLonPar
     iloop1: do i=1,iba(j)
        ftv1=ftv(i,j)     ! ftv1: flux tube volume in m^3/Wb
        nloop: do n=1,nspec
           Pressure0=0.0
           Pressure1=0.0
           PressurePar1=0.0
           Den_IC(n,i,j)=0.0
           amu1=amu_I(n)**1.5
!!!! Calculate Den_IC, and 2D flux, fl2D(log), ekev2D(log) and sinA1D
           do m=1,nk
              sinA1D(m) = sinA(i,j,m)
              cosA2(m) = 1 - sinA1D(m)**2
           end do
           do m=1,nk
              if (m == 1) sina0=1.
              if (m > 1) sina0=0.5*(sinA1D(m)+sinA1D(m-1))
              if (m == nk) sina1=0.
              if (m < nk) sina1=0.5*(sinA1D(m)+sinA1D(m+1))
              dcosa=sqrt(1.-sina1*sina1)-sqrt(1.-sina0*sina0)
              do k=1,nm
                 ! write(*,*) 'n,i,k,xjac(n,i,k)',n,i,k,xjac(n,i,k)
                 psd1=f2(n,i,j,k,m)/1.e20/1.e19/xjac(n,i,k)  ! mug^-3cm^-6s^3
                 flx=psd1*(1.6e19*pp(n,i,j,k,m))*pp(n,i,j,k,m)
                 flux2D(k,m)=-50.
                 if (flx > 1.e-50) flux2D(k,m)=log10(flx)
                 ekev2D(k,m)=log10(ekev(n,i,j,k,m))
                 eta(n,i,j,k,m)=amu1*1.209*psd1*sqrt(xmm(n,k))*dmm(n,k)*dk(m)
                 psd(n,i,j,k,m)=psd1

                 ! Stores drift velocities in (m,k)
                 ! NOTE:: vl is calculated at dlambda/dt [rad/s] in
                 ! ionosphere.  This variable still needs to be
                 ! converted to radial velocities [km/s].
                 vl2D(k,m)=vl(n,i,j,k,m)
                 vp2D(k,m)=vp(n,i,j,k,m)

                 ! The old rho and p calculation based on RCM method:
                 !   Den_IC(n,i,j) = Den_IC(n,i,j)+eta(n,i,j,k,m)/ftv1
                 !   Pressure0     = eta(n,i,j,k,m)*ekev(i,j,k,m)/ftv1
                 ! might be incorrect, giving different results from the
                 ! following calculation based on integration of flux.

                 ! Number density comes from the integration of "psd":
                 ! n = int(psd*dp^3) = int(flx/p^2*4*pi*p^2*sinA*dpdA)
                 ! with M = p^2/(2*m0*Bm) --> dp = p/2M*dM
                 ! so n = 2*pi*int(flx*p/M*dcosAdM)
                 !
                 ! Total pressure and parallel pressure are from
                 !   P    = 4*pi/3*int(E*flx*p/M*dcosAdM)
                 !   Ppar = 4*pi*int(E*flx*p/M*(cosA)^2*dcosAdM)

                 Den_IC(n,i,j) = Den_IC(n,i,j) &
                      + flx*pp(n,i,j,k,m)/xmm(n,k)*dmm(n,k)*dcosa
                 Pressure0 = ekev(n,i,j,k,m)*flx*pp(n,i,j,k,m)/xmm(n,k) &
                      *dmm(n,k)*dcosa
                 Pressure1 = Pressure1 + Pressure0
                 PressurePar1 = PressurePar1 + 3.*Pressure0*cosA2(m)
              enddo
           enddo

           Den_IC(n,i,j) = Den_IC(n,i,j)*2.0*cPi/1.6e-20   ! density in m^-3
           ! Coeff = 1.6e-16*2./3.*1.e9                   ! for the old p
           Coeff = 4.*cPi/3.*1.e4*1.e9
           Pressure_IC(n,i,j) = Pressure1*Coeff          ! pressure in nPa
           PressurePar_IC(n,i,j) = PressurePar1*Coeff

!!!! Map flux to fixed energy and pitch-angle grids (energy, sinAo)
           do k=1,neng
              do m=1,npit
!!$                 if ( aloge(n,k) < minval(ekev2D) &
!!$                      .or. aloge(n,k) > maxval(ekev2D) &
!!$                      .or. sinAo(m) < minval(sinA1D) &
!!$                      .or. sinAo(m) > maxval(sinA1D) ) then
!!$                    flx_lo=-50.0
!!$                    vl_lo=0.0
!!$                    vp_lo=0.0
!!$                 else
                 call lintp2aIM(ekev2D,sinA1D,flux2D,nm,nk,aloge(n,k),&
                      sinAo(m),flx_lo)

!!!! Map radial drift to fixed energy and pitch-angle grids (energy, sinAo)
                 call lintp2aIM(ekev2D,sinA1D,vl2D,nm,nk,aloge(n,k),&
                      sinAo(m),vl_lo)

!!!! Map poloidal drift to fixed energy and pitch-angle grids (energy, sinAo)
                 call lintp2aIM(ekev2D,sinA1D,vp2D,nm,nk,aloge(n,k),&
                      sinAo(m),vp_lo)

                 flux(n,i,j,k,m)=10.**flx_lo
                 vlEa(n,i,j,k,m)=&
                      vl_lo*re_m   ! bounce-averaged vl  PROJECTED to ionosphere 
                 vpEa(n,i,j,k,m)= &
                      vp_lo*re_m*cos(xlatr(i))    !  bounce averaged vp PROJECTED to ionosphere

              enddo
           enddo
        enddo nloop
     enddo iloop1
  enddo jloop1

  Part_phot=0.

  ! Calculate pressure of the 'hot' ring current phot, and temperature Temp_IC
  jloop2: do j=MinLonPar,MaxLonPar
     iloop2: do i=1,iba(j)
!!!! calculate pitch-angle averaged flux
        do n=1,nspec
           do k=1,neng
              fave(n,i,j,k)=0.
              do m=1,npit
                 fave(n,i,j,k)=fave(n,i,j,k)+flux(n,i,j,k,m)*dmu(m)
              enddo
           enddo
        enddo
!!!! calculate pressure and temperature
        do n=1,nspec
           do k=1,neng
              phot(n,i,j)=phot(n,i,j)+fave(n,i,j,k)*delEE(n,k)*pf(n) ! phot in nPa
              if (CIMIboundary) Part_phot(n,i,j,k) = fave(n,i,j,k)*delEE(n,k)*pf(n) 
           enddo
           Temp_IC(n,i,j)=0.
           if (Den_IC(n,i,j) > 0.) &
                Temp_IC(n,i,j)=phot(n,i,j)*1.e-9/Den_IC(n,i,j)/1.6e-19   ! eV
        enddo
!!!! calculate parallel pressure
        cosAo2(1:npit) = 1-sinAo(1:npit)**2  ! store 1-sinAo^2
        do n=1,nspec
           do k=1,neng
              do m=1,npit
                 Ppar_IC(n,i,j) = Ppar_IC(n,i,j) + flux(n,i,j,k,m) &
                      *cosAo2(m)*dmu(m)*delEE(n,k)*pf(n)*3.
              enddo
           enddo
        enddo
        ! boundary calculations:
        if (CIMIboundary) then
           do n=1,nspec
              p_min = 0.
              p_max = 0.
              p_q1 = Phot(n,i,j) * 0.25
              p_q3 = Phot(n,i,j) * 0.75
              do k=1,neng
                 if (p_min < p_q1) then
                    p_min = Part_phot(n,i,j,k) + p_min
                    eng_q1(n,i,j)=energy(n,k)   ! in kev(?) - check
                    k_q1=k
                 endif
                 if (p_max < p_q3) then
                    p_max = Part_phot(n,i,j,k) + p_max
                    eng_q3(n,i,j)=energy(n,k)
                    k_q3=k
                 endif
              enddo
           enddo
           
           vexb(1,i,j) = vpEa(1,i,j,1,1)*vpEa(1,i,j,1,1)
           vexb(1,i,j) = sqrt(vlEa(1,i,j,1,1)*vlEa(1,i,j,1,1) + vexb(1,i,j))
           
           vdr_q1(1,i,j) = sqrt(vlEa(1,i,j,k_q1,1)*vlEa(1,i,j,k_q1,1)&
                +vpEa(1,i,j,k_q1,1)*vpEa(1,i,j,k_q1,1))
           vdr_q3(1,i,j) = sqrt(vlEa(1,i,j,k_q3,1)*vlEa(1,i,j,k_q3,1)&
                +vpEa(1,i,j,k_q3,1)*vpEa(1,i,j,k_q3,1))
           vgyr_q1(1,i,j) = 440. *sqrt(energy(1,k_q1))
           vgyr_q3(1,i,j) = 440. *sqrt(energy(1,k_q3))
           vexb(1,i,j) = sqrt(vlEa(1,i,j,1,1)*vlEa(1,i,j,1,1)&
                +vpEa(1,i,j,1,1)*vpEa(1,i,j,1,1))
           
           dif_q1(1,i,j) = (vlEa(1,i,j,k_q1,1)-vlEa(1,i,j,1,1))&
                *(vlEa(1,i,j,k_q1,1)-vlEa(1,i,j,1,1))
           dif_q1(1,i,j) = dif_q1(1,i,j)+(vpEa(1,i,j,k_q1,1)&
                -vpEa(1,i,j,1,1))*(vpEa(1,i,j,k_q1,1)-vpEa(1,i,j,1,1))
           dif_q1(1,i,j)  = sqrt(dif_q1(1,i,j))/vexb(1,i,j)   ! relative difference between vdr and vExB, q1
           
           dif_q3(1,i,j) = (vlEa(1,i,j,k_q3,1)-vlEa(1,i,j,1,1))&
                *(vlEa(1,i,j,k_q3,1)-vlEa(1,i,j,1,1))
           dif_q3(1,i,j) = dif_q3(1,i,j)+(vpEa(1,i,j,k_q3,1)&
                -vpEa(1,i,j,1,1))*(vpEa(1,i,j,k_q3,1)-vpEa(1,i,j,1,1))
           dif_q3(1,i,j)  = sqrt(dif_q3(1,i,j))/vexb(1,i,j)   ! relative difference between vdr and vExB, q3
        endif
     enddo iloop2
  enddo jloop2

  if (DoCalcFac) then
     ! Calculate field aligned current, fac
     ! First get ghost cell info for eta and ekev when nProc>1
     if (nProc>1) then
        ! send ekev ghostcells
        do k=1,nm
           do m=1,nk
              ! call MPI_send(ekev(:,MaxLonPar,k,m),np,MPI_REAL,iProcRight,&
              !      1,iComm,iError)
              ! call MPI_send(ekev(:,MinLonPar,k,m),np,MPI_REAL,iProcLeft,&
              !      2,iComm,iError)
              ! recieve ekev from neigboring Procs
              ! call MPI_recv(ekev(:,iLonLeft,k,m),np,MPI_REAL,iProcLeft,&
              !      1,iComm,iStatus_I,iError)
              ! call MPI_recv(ekev(:,iLonRight,k,m),np,MPI_REAL,iProcRight,&
              !      2,iComm,iStatus_I,iError)
              do n=1,nspec
                 call MPI_send(ekev(n,:,MaxLonPar,k,m),np,MPI_REAL,iProcRight,&
                      1,iComm,iError)
                 call MPI_send(ekev(n,:,MinLonPar,k,m),np,MPI_REAL,iProcLeft,&
                      2,iComm,iError)
                 ! recieve ekev from neigboring Procs
                 call MPI_recv(ekev(n,:,iLonLeft,k,m),np,MPI_REAL,iProcLeft,&
                      1,iComm,iStatus_I,iError)
                 call MPI_recv(ekev(n,:,iLonRight,k,m),np,MPI_REAL,iProcRight,&
                      2,iComm,iStatus_I,iError)
                 ! send eta ghostcells
                 call MPI_send(eta(n,:,MaxLonPar,k,m),np,MPI_REAL,iProcRight,&
                      1,iComm,iError)
                 call MPI_send(eta(n,:,MinLonPar,k,m),np,MPI_REAL,iProcLeft,&
                      2,iComm,iError)
                 ! recieve eta from neigboring Procs
                 call MPI_recv(eta(n,:,iLonLeft,k,m),np,MPI_REAL,iProcLeft,&
                      1,iComm,iStatus_I,iError)
                 call MPI_recv(eta(n,:,iLonRight,k,m),np,MPI_REAL,iProcRight,&
                      2,iComm,iStatus_I,iError)
              enddo
           enddo
        enddo
     endif
     jloop3: do j=MinLonPar,MaxLonPar
        j1=j+1
        j_1=j-1
        if (j1 > nt) j1=j1-nt          !! periodic boundary condition
        if (j_1 < 1) j_1=j_1+nt        !!
        iloop3: do i=2,iba(j)-1
           do k=1,nm
              do m=1,nk
                 do n=1,nspec
                    dwkdi= ( ekev(n,i+1,j,k,m) &
                           - ekev(n,i-1,j,k,m))/(xlatr(i+1)-xlatr(i-1))
                    dwkdj=(ekev(n,i,j1,k,m)-ekev(n,i,j_1,k,m))/(2.*dphi)
                    detadi=(eta(n,i+1,j,k,m) &
                         -  eta(n,i-1,j,k,m))/(xlatr(i+1)-xlatr(i-1))
                    detadj=(eta(n,i,j1,k,m)-eta(n,i,j_1,k,m))/(2.*dphi)
                    fac(i,j)=fac(i,j)+(detadi*dwkdj-detadj*dwkdi)
                 enddo
              enddo
           enddo
           fac(i,j)=1.6e-16*fac(i,j)/cos(xlatr(i))/rion**2    ! fac in Amp/m^2
        enddo iloop3
     enddo jloop3
  else
     fac(:,:)=0.0
  endif

end subroutine cimi_output
!==============================================================================
subroutine cimi_gather( delta_t, psd, flux, vlea, vpea )

  use ModCimi,		ONLY:	&
       Time, FAC_C, Pressure_IC, PressurePar_IC, Bmin_C, &
       phot, Ppar_IC, preP, preF, Eje1,&
       vdr_q3, eng_q3, vexb, dif_q3, Part_phot

  use ModCimiBoundary,	ONLY: 	Outputboundary
  use ModCimiGrid,	ONLY: 	&
       iProc, nProc, iComm, nLonPar, nLonPar_P, nLonBefore_P, &
       MinLonPar, MaxLonPar, nt, np, neng, npit, nm, nk, dlat, &
       phi, sinao, xlat, xmlt
  use ModCimiPlanet,	ONLY:	nspec
  use ModCimiPlot
  use ModCimiTrace,	ONLY:	gather_field_trace
  use ModCoupleSami,	ONLY:	DoCoupleSami
  use ModGmCimi,	ONLY:	Den_IC
  use ModImSat,		ONLY:	DoWriteSats, DtSatOut
  use ModMPI

  implicit none
  
  real, intent(in) :: delta_t
  real:: flux( nspec, np, nt, neng, npit ), psd( nspec, np, nt, nm, nk ), &
       vlEa( nspec, np, nt, neng, npit ), vpEa (nspec, np, nt, neng, npit )

  ! Vars for mpi passing
  integer, allocatable :: iBufferSend_I(:), iBufferRecv_I(:)
  integer, allocatable :: iReceiveCount_P(:), iDisplacement_P(:)
  integer:: iSendCount, iM, iK, iLon1, iError, iEnergy, iPit, &
       iRecvLower, iRecvUpper, iPe, iSpecies
  real:: BufferSend_C( np, nt ), BufferRecv_C( np, nt )
  integer:: BufferSend_I( nt ), BufferRecv_I( nt )
  integer:: iStatus_I( MPI_STATUS_SIZE )
  !----------------------------------------------------------------------------
  allocate( iReceiveCount_P( nProc ), iDisplacement_P( nProc ) )

  ! Gather to root
  iSendCount = np * nLonPar
  iReceiveCount_P = np * nLonPar_P
  iDisplacement_P = np * nLonBefore_P

  BufferSend_C(:,:) = FAC_C(:,:)
  call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar), iSendCount, &
       MPI_REAL, BufferRecv_C, iReceiveCount_P, iDisplacement_P, &
       MPI_REAL, 0, iComm, iError)
  if (iProc==0) FAC_C(:,:)=BufferRecv_C(:,:)

  do iSpecies=1,nspec
     BufferSend_C(:,:)=Pressure_IC(iSpecies,:,:)
     call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar), iSendCount, &
          MPI_REAL, BufferRecv_C, iReceiveCount_P, iDisplacement_P, &
          MPI_REAL, 0, iComm, iError)
     if (iProc==0) Pressure_IC(iSpecies,:,:)=BufferRecv_C(:,:)

     BufferSend_C(:,:)=PressurePar_IC(iSpecies,:,:)
     call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar), iSendCount, &
          MPI_REAL, BufferRecv_C, iReceiveCount_P, iDisplacement_P, &
          MPI_REAL, 0, iComm, iError)
     if (iProc==0) PressurePar_IC(iSpecies,:,:)=BufferRecv_C(:,:)

     BufferSend_C(:,:)=phot(iSpecies,:,:)
     call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar), iSendCount, &
          MPI_REAL, BufferRecv_C, iReceiveCount_P, iDisplacement_P, &
          MPI_REAL, 0, iComm, iError)
     if (iProc==0) phot(iSpecies,:,:)=BufferRecv_C(:,:)

     BufferSend_C(:,:)=Ppar_IC(iSpecies,:,:)
     call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar), iSendCount, &
          MPI_REAL, BufferRecv_C, iReceiveCount_P, iDisplacement_P, &
          MPI_REAL, 0, iComm, iError)
     if (iProc==0) Ppar_IC(iSpecies,:,:)=BufferRecv_C(:,:)

     BufferSend_C(:,:)=Den_IC(iSpecies,:,:)
     call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar), iSendCount, &
          MPI_REAL, BufferRecv_C, iReceiveCount_P, iDisplacement_P, &
          MPI_REAL, 0, iComm, iError)
     if (iProc==0) Den_IC(iSpecies,:,:)=BufferRecv_C(:,:)

  enddo

  BufferSend_C(:,:) = Bmin_C(:,:)
  call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar), iSendCount, &
       MPI_REAL, BufferRecv_C, iReceiveCount_P, iDisplacement_P, &
       MPI_REAL, 0, iComm, iError)
  if (iProc==0) Bmin_C(:,:)=BufferRecv_C(:,:)

  call gather_field_trace
  
  do  iSpecies = 1, nspec

     do iM = 1, nm

        do iK = 1, nk

           BufferSend_C(:,:)=psd(iSpecies,:,:,im,iK)
           call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar),iSendCount, &
                MPI_REAL, BufferRecv_C,iReceiveCount_P, iDisplacement_P, &
                MPI_REAL, 0, iComm, iError)
           if (iProc==0) psd(iSpecies,:,:,im,ik)=BufferRecv_C(:,:)

        end do

     end do

     do iEnergy = 1, neng
        do iPit = 1, nPit

           BufferSend_C(:,:)=flux(iSpecies,:,:,iEnergy,iPit)
           call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar),iSendCount, &
                MPI_REAL, BufferRecv_C,iReceiveCount_P, iDisplacement_P, &
                MPI_REAL, 0, iComm, iError)
           if (iProc==0) flux(iSpecies,:,:,iEnergy,iPit)=BufferRecv_C(:,:)

           BufferSend_C(:,:)=vlEa(iSpecies,:,:,iEnergy,iPit)
           call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar),iSendCount, &
                MPI_REAL, BufferRecv_C,iReceiveCount_P, iDisplacement_P, &
                MPI_REAL, 0, iComm, iError)
           if (iProc==0) vlEa(iSpecies,:,:,iEnergy,iPit)=BufferRecv_C(:,:)

           BufferSend_C(:,:)=vpEa(iSpecies,:,:,iEnergy,iPit)
           call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar),iSendCount, &
                MPI_REAL, BufferRecv_C,iReceiveCount_P, iDisplacement_P, &
                MPI_REAL, 0, iComm, iError)
           if (iProc==0) vpEa(iSpecies,:,:,iEnergy,iPit)=BufferRecv_C(:,:)

        enddo
     enddo
  enddo

  do  iSpecies=1,nspec

     do iEnergy = 1, neng + 2

        BufferSend_C(:,:)=preP(iSpecies,:,:,iEnergy)
        call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar),iSendCount, &
             MPI_REAL, BufferRecv_C,iReceiveCount_P, iDisplacement_P, &
             MPI_REAL, 0, iComm, iError)
        if (iProc==0) preP(iSpecies,:,:,iEnergy)=BufferRecv_C(:,:)

        BufferSend_C(:,:)=preF(iSpecies,:,:,iEnergy)
        call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar),iSendCount, &
             MPI_REAL, BufferRecv_C,iReceiveCount_P, iDisplacement_P, &
             MPI_REAL, 0, iComm, iError)
        if (iProc==0) preF(iSpecies,:,:,iEnergy)=BufferRecv_C(:,:)

     enddo ! Do loop over iEnergy

     BufferSend_C(:,:)=Eje1(iSpecies,:,:)
     call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar),iSendCount, &
          MPI_REAL, BufferRecv_C,iReceiveCount_P, iDisplacement_P, &
          MPI_REAL, 0, iComm, iError)
     if (iProc==0) Eje1(iSpecies,:,:)=BufferRecv_C(:,:)

  enddo ! Do loop over Species

  ! gather information for boundary output
  if ( OutputBoundary ) then

     do  iSpecies=1, nspec

        do iEnergy=1,neng

           BufferSend_C(:,:) = Part_phot(iSpecies,:,:,iEnergy)
           call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar),iSendCount, &
                MPI_REAL, BufferRecv_C,iReceiveCount_P, iDisplacement_P, &
                MPI_REAL, 0, iComm, iError)
           if (iProc==0) &
                Part_phot(iSpecies,:,:,iEnergy)=BufferRecv_C(:,:)

        enddo ! Do loop over iEnergy

        BufferSend_C(:,:)=eng_q3(iSpecies,:,:)
        call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar),iSendCount, &
             MPI_REAL, BufferRecv_C,iReceiveCount_P, iDisplacement_P, &
             MPI_REAL, 0, iComm, iError)
        if (iProc==0) eng_q3(iSpecies,:,:)=BufferRecv_C(:,:)

        BufferSend_C(:,:)=vexb(iSpecies,:,:)
        call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar),iSendCount, &
             MPI_REAL, BufferRecv_C,iReceiveCount_P, iDisplacement_P, &
             MPI_REAL, 0, iComm, iError)
        if (iProc==0) vexb(iSpecies,:,:)=BufferRecv_C(:,:)

        BufferSend_C(:,:)=vdr_q3(iSpecies,:,:)
        call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar),iSendCount, &
             MPI_REAL, BufferRecv_C,iReceiveCount_P, iDisplacement_P, &
             MPI_REAL, 0, iComm, iError)
        if (iProc==0) vdr_q3(iSpecies,:,:)=BufferRecv_C(:,:)

        BufferSend_C(:,:)=dif_q3(iSpecies,:,:)
        call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar),iSendCount, &
             MPI_REAL, BufferRecv_C,iReceiveCount_P, iDisplacement_P, &
             MPI_REAL, 0, iComm, iError)
        if (iProc==0) dif_q3(iSpecies,:,:)=BufferRecv_C(:,:)

     enddo

  endif

  deallocate( iReceiveCount_P, iDisplacement_P )

end subroutine cimi_gather
!==============================================================================
subroutine core_ps_gather

  ! gather subset of variables needed for core plasmasphere

  use ModCimiGrid,	ONLY: &
       iProc, nProc, iComm, nLonPar, nLonPar_P, nLonBefore_P, &
       MinLonPar, MaxLonPar,np,nt
  use ModCimiTrace,	ONLY: &
       ftv => volume, phi2o, brad => ro, iba
  use ModMPI

  implicit none
  
  ! Vars for mpi passing
  integer, allocatable :: iBufferSend_I(:), iBufferRecv_I(:)
  integer, allocatable :: iReceiveCount_P(:), iDisplacement_P(:)
  integer:: iSendCount, iError,  &
       iRecvLower, iRecvUpper
  real:: BufferSend_C( np, nt ), BufferRecv_C( np, nt )
  integer:: BufferSend_I( nt ), BufferRecv_I( nt )
  integer:: iStatus_I( MPI_STATUS_SIZE )
  !----------------------------------------------------------------------------
  allocate( iReceiveCount_P( nProc ), iDisplacement_P( nProc ) )

  ! Gather to root
  iSendCount = np * nLonPar
  iReceiveCount_P = np * nLonPar_P
  iDisplacement_P = np * nLonBefore_P

  BufferSend_I(:) = iba(:)
  call MPI_GATHERV(BufferSend_I(MinLonPar:MaxLonPar), nLonPar, &
       MPI_INTEGER, BufferRecv_I, nLonPar_P, nLonBefore_P, &
       MPI_INTEGER, 0, iComm, iError)
  if (iProc==0) iba(:)=BufferRecv_I(:)

  BufferSend_C(:,:)=ftv(:,:)
  call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar), iSendCount, &
       MPI_REAL, BufferRecv_C, iReceiveCount_P, iDisplacement_P, &
       MPI_REAL, 0, iComm, iError)
  if (iProc==0) ftv(:,:)=BufferRecv_C(:,:)

  BufferSend_C(:,:)=phi2o(:,:)
  call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar), iSendCount, &
       MPI_REAL, BufferRecv_C, iReceiveCount_P, iDisplacement_P, &
       MPI_REAL, 0, iComm, iError)
  if (iProc==0) phi2o(:,:)=BufferRecv_C(:,:)

  BufferSend_C(:,:)=brad(:,:)
  call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar), iSendCount, &
       MPI_REAL, BufferRecv_C, iReceiveCount_P, iDisplacement_P, &
       MPI_REAL, 0, iComm, iError)
  if (iProc==0) brad(:,:)=BufferRecv_C(:,:)

end subroutine core_ps_gather
!==============================================================================
subroutine cimi_precip_calc(dsec)

  use ModCimi,			ONLY:	&
       preF, preP, Eje1, &
       xlel => eChangeOperator_VICI, plel => pChangeOperator_VICI, &
       OpLossCone_, OpLossCone0_, OpFLC_, OpFLC0_, &
       precipEnergyLoss, precipNumberLoss
  use ModCimiTrace, 		ONLY:	iba, UsePrecipEnergyLoss
  use ModCimiGrid,		ONLY:	&
       nProc,iProc,iComm,MinLonPar,MaxLonPar,nt,np,neng,xlatr,xmlt,dlat
  use ModCimiPlanet,		ONLY: 	nspec, re_m, rc
  use ModMPI
  use ModCimiInitialize, 	ONLY: 	dphi

  implicit none

  real:: dsec, dlel, dplel, dlel_lc, dplel_lc, dlel_flc, dplel_flc, area, &
         area1, Asec
  integer:: n, i, j, k
  logical :: useFlc, useLossCone
  !----------------------------------------------------------------------------
  preF(1:nspec,1:np,1:nt,1:neng+2)=0.
  preP(1:nspec,1:np,1:nt,1:neng+2)=0.
  Eje1(1:nspec,1:np,1:nt)=0.

  area1=rc*rc*re_m*re_m*dphi

  do n=1,nspec
     do j=MinLonPar,MaxLonPar
        do i=1,iba(j)
           area=area1*cos(xlatr(i))*dlat(i)            ! area in m^2
           Asec=area*dsec
           Energy: do k=1,neng+2
              if(UsePrecipEnergyLoss) then
                if (precipEnergyLoss(n,i,j,k) >= 0. .or. &
                    precipNumberLoss(n,i,j,k) >= 0.) then
                  preF(n,i,j,k)=0.
                  preP(n,i,j,k)=0.     
                  if (k == neng+2) Eje1(n,i,j)=0.             
                else
                  preF(n,i,j,k)=-precipEnergyLoss(n,i,j,k)*1.6e-13/Asec
                  preP(n,i,j,k)=-precipNumberLoss(n,i,j,k)/Asec
                  ! meanE for E>gride(je)
                  if (k == neng+2) Eje1(n,i,j)=precipEnergyLoss(n,i,j,k) &
                                              /precipNumberLoss(n,i,j,k)
                end if
              else
                dlel_lc=xlel(n,i,j,k,OpLossCone_)-xlel(n,i,j,k,OpLossCone0_)
                dplel_lc=plel(n,i,j,k,OpLossCone_)-plel(n,i,j,k,OpLossCone0_)
                dlel_flc=xlel(n,i,j,k,OpFLC_)-xlel(n,i,j,k,OpFLC0_)
                dplel_flc=plel(n,i,j,k,OpFLC_)-plel(n,i,j,k,OpFLC0_)
                useFlc = (dlel_flc < 0. .and. dplel_flc < 0.)
                useLossCone = (dlel_lc < 0. .and. dplel_lc < 0.)
                dlel = 0
                dplel = 0
                if (useLossCone) then
                    dlel = dlel + dlel_lc
                    dplel = dplel + dplel_lc
                end if
                if (useFlc) then
                    dlel = dlel + dlel_flc
                    dplel = dplel + dplel_flc
                end if
                if (useLossCone .or. useFlc) then
                  preF(n,i,j,k)=-dlel*1.6e-13/Asec     ! E flux in mW/m2
                  preP(n,i,j,k)=-dplel/Asec            ! N flux in 1/m2/s

                  ! meanE for E>gride(je)
                  if (k == neng+2) Eje1(n,i,j)=dlel/dplel
                end if

              endif
           enddo Energy
        enddo
     enddo
  enddo

  ! Overwrites the OpLossCone0_ array with the current time information.
  xlel(:,:,:,:,OpLossCone0_) = xlel(:,:,:,:,OpLossCone_)
  plel(:,:,:,:,OpLossCone0_) = plel(:,:,:,:,OpLossCone_)
  ! Do same for OpFLC0_ array
  xlel(:,:,:,:,OpFLC0_) = xlel(:,:,:,:,OpFLC_)
  plel(:,:,:,:,OpFLC0_) = plel(:,:,:,:,OpFLC_)

end subroutine cimi_precip_calc
!==============================================================================
subroutine FLS_2D(np,nt,iba,fb0,fb1,cl,cp,f2d,fal,fap,fupl,fupp)

  !  Calculate the inter-flux, fal(idown,j) and fap(i,jdown),
  !  using 2nd order flux limited scheme with super-bee flux limiter
  !  method where
  !   	fal( i - 1up , 	   j 	) = fal(   idown    ,	   j 	) and
  !	fal(  idown  ,	   j 	) = fal(  i + 1up   ,	   j 	)
  !   	fap( 	i    , 	j - 1up ) = fap(    i       ,	 jdown 	) and
  !	fap( 	i    ,	 jdown	) = fap(    i       , 	j + 1up	)
  !
  !   iup: upstream from i, i-1 < iup < i when cl > 0,
  !	i < iup < i+1 when cl < 0
  !   idown: downstream from i, i < idown < i+1 when cl > 0,
  !	i-1 < iup < i when cl < 0
  !   jup: upstream from j, j-1 < jup < j when cp > 0,
  !	j < jup < j+1 when cl < 0
  !   jdown: downstream from j, j < jdown < j+1 when cp > 0,
  !	j-1 < jup < j when cp < 0
  !
  !  Input: np,nt,iba,fb0,fb1,cl,cp,f2d
  !  Output: fal,fap

  use ModCimi, ONLY: UseMcLimiter, BetaLimiter
  use ModCimi,       ONLY: MinLonPar,MaxLonPar

  implicit none

  integer, intent(in):: np,nt,iba(nt)
  real, intent(in):: cl(np,nt), cp(np,nt), f2d(np,nt)
  real, intent(inout):: fal(0:np,nt), fap(np,nt), fupl(0:np,nt), fupp(np,nt)

  integer:: i,j,j_1,j1,j2,ib
  real:: fwbc(0:np+2,nt), fb0(nt),fb1(nt),x,fup,flw,xsign,corr,xlimiter,r
  !----------------------------------------------------------------------------
  fwbc(1:np,1:nt)=f2d(1:np,1:nt)        ! fwbc is f2d with boundary condition

  ! Set up boundary condition
  fwbc(0,1:nt)=fb0(1:nt)
  do j=MinLonPar,MaxLonPar
     ib=iba(j)
     fwbc(ib+1:np+2,j)=fb1(j)
  enddo

  ! NB: Below we will be using fwbc from neighbouring procs; How to
  ! guarantee that fwbc will be ready when it is needed?
  ! find fal and fap
  jloop: do j=MinLonPar,MaxLonPar
     j_1=j-1
     j1=j+1
     j2=j+2
     if (j_1 < 1) j_1=j_1+nt
     if (j1 > nt) j1=j1-nt
     if (j2 > nt) j2=j2-nt
     iloop: do i=1,np
        ! find fal
        xsign=sign(1.,cl(i,j))
        ! Upwind Scheme
        ! fupl(idown,j) = fwbc(i,j) when cl(idown,j) > 0
        !                 fwbc(i+1,j) when cl(idwon,j) < 0
        fupl(i,j)=0.5*(1.+xsign)*fwbc(i,j)+0.5*(1.-xsign)*fwbc(i+1,j) ! upwind
        flw=0.5*(1.+cl(i,j))*fwbc(i,j)+0.5*(1.-cl(i,j))*fwbc(i+1,j)   ! LW
        x=fwbc(i+1,j)-fwbc(i,j)
        if (abs(x) <= 1.e-27) fal(i,j)=fupl(i,j)
        if (abs(x) > 1.e-27) then
           if (xsign == 1.) r=(fwbc(i,j)-fwbc(i-1,j))/x
           if (xsign == -1.) r=(fwbc(i+2,j)-fwbc(i+1,j))/x
           if (r <= 0.) fal(i,j)=fupl(i,j)
           if (r > 0.) then
              if(UseMcLimiter)then
                 xlimiter = min(BetaLimiter*r, BetaLimiter, 0.5*(1+r))
              else
                 xlimiter = max(min(2.*r,1.),min(r,2.))
              end if
              corr=flw-fupl(i,j)
              fal(i,j)=fupl(i,j)+xlimiter*corr
           endif
        endif
        ! find fap
        xsign=sign(1.,cp(i,j))
        fupp(i,j)=0.5*(1.+xsign)*fwbc(i,j)+0.5*(1.-xsign)*fwbc(i,j1) ! upwind
        flw=0.5*(1.+cp(i,j))*fwbc(i,j)+0.5*(1.-cp(i,j))*fwbc(i,j1)   ! LW
        x=fwbc(i,j1)-fwbc(i,j)
        if (abs(x) <= 1.e-27) fap(i,j)=fupp(i,j)
        if (abs(x) > 1.e-27) then
           if (xsign == 1.) r=(fwbc(i,j)-fwbc(i,j_1))/x
           if (xsign == -1.) r=(fwbc(i,j2)-fwbc(i,j1))/x
           if (r <= 0.) fap(i,j)=fupp(i,j)
           if (r > 0.) then
              if(UseMcLimiter)then
                 xlimiter = min(BetaLimiter*r, BetaLimiter, 0.5*(1+r))
              else
                 xlimiter = max(min(2.*r,1.),min(r,2.))
              end if
              corr=flw-fupp(i,j)
              fap(i,j)=fupp(i,j)+xlimiter*corr
           endif
        endif
     enddo iloop
  enddo jloop

end subroutine FLS_2D
!==============================================================================

! Old CLOSED SUBROUTINE
! subroutine closed(n1,n2,yy,dx,ss)

!  ! Numerical integration using closed form.
!  !
!  ! Input: n1,n2,yy,dx
!  ! Output: ss
!
!  implicit none
!
!  integer:: n1,n2,i
!  real:: yy(n2),dx(n2),ss
!  !--------------------------------------------------------------------------!
!  ss=0.
!  do i=n1,n2
!     ss=ss+yy(i)*dx(i)
!  enddo
!
! end subroutine closed

end module ModCimiMethods
