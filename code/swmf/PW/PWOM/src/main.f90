!  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf
program pw

  use ModPwom
  use ModFieldLine
  use ModCommonPlanet,only:NamePlanet
  use ModMpi
  use ModReadParam
  use ModReGrid  , ONLY: DoRegrid, DtRegrid, regrid_lines,DoAdaptGrid,&
       update_remap_criteria,TypeAdaptCriteria
  use CON_planet,  ONLY: init_planet_const, set_planet_defaults,is_planet_init
  use ModUtilities,ONLY: CON_stop
  implicit none

  logical :: IsPlanetSet
  !****************************************************************************
  ! Initiallize MPI and get number of processors and rank of given processor
  !****************************************************************************

  !---------------------------------------------------------------------------
  call MPI_INIT(errcode)
  iComm = MPI_COMM_WORLD

  call MPI_COMM_RANK(iComm,iProc,errcode)
  call MPI_COMM_SIZE(iComm,nProc,errcode)


  call timing_active(.true.)
  call timing_step(0)
  call timing_start('PWOM')

  !****************************************************************************
  ! Read the input file
  !****************************************************************************
  IsStandAlone = .true.
  NameInput    = 'PARAM.in'

  call read_file('PARAM.in',iComm)
  call read_init('  ',iSessionIn=1,iLineIn=0)

  call PW_set_parameters('READ')
  ! call PW_set_parameters('CHECK')


  !\
  ! Initialize the planetary constant library and set Earth
  ! as the default planet.
  !/
  write(*,*) 'Initiallizing Planet'

  call init_planet_const

  if (NamePlanet == 'EARTH') then
     call set_planet_defaults
     IsPlanetSet = .true.
  else
     write(*,*) NamePlanet
     IsPlanetSet = is_planet_init(NamePlanet)
  endif
  
  if (.not.IsPlanetSet) then
     call CON_stop('Planet not set. Stopping PWOM')
  endif


  call PW_initialize

  !****************************************************************************
  ! Move the flux tube, solve each fieldline, and advance the time
  !****************************************************************************
  !Jr_G=0.0
  if (IsTimeAccurate) then
     TIMELOOP:do
        if (Time >= Tmax) exit TIMELOOP
        DtHorizontal = min(DtHorizontalOrig, Tmax - Time)
        if (DtHorizontal < 1.0e-6) then
           Time = Tmax
           exit TIMELOOP
        endif
        
        ! Get electrodynamics information before updating lines
        call PW_get_electrodynamics
        
        if (DoRegrid) then
           if (floor((Time+1.0e-5)/DtRegrid) /= &
                floor((Time+1.0e-5-DtHorizontal)/DtRegrid) )then 
              call timing_start('regrid_lines')
              !if (iProc==0) write(*,*) 'before',ThetaLine_I(1),PhiLine_I(1)
              if (DoAdaptGrid) then
                 !write(*,*) Jr_G
                 !write(*,*) Theta_G
                 !write(*,*) 'test1',maxval(Jr_G(1:nPhi,1:nTheta))
                 select case(TypeAdaptCriteria)
                 case('Jr')
                    call update_remap_criteria(nTheta,nPhi, Theta_G,Phi_G,&
                         abs(Jr_G))
                 case('eFlux')
                    call update_remap_criteria(nTheta,nPhi, Theta_G,Phi_G,&
                         abs(Eflux_G))
                 case DEFAULT
                    call con_stop('PW Error: no TypeAdaptCriteria supplied.')
                 end select
              endif
              call regrid_lines
              !if (iProc==0) write(*,*) 'after',ThetaLine_I(1),PhiLine_I(1)
              call timing_stop('regrid_lines')
           endif
        endif

        
        do iLine=1,nLine
           
           ! move_line moves the flux tube, then we can use the angular
           !position to get the lat and lon
           
           call move_line
           
           !  Call the flux tube to be solved
           
           call PW_advance_line
        enddo
        !Output the electrodynamics info
        if (DoPlotElectrodynamics) then
           if (floor(Time/DtPlotElectrodynamics) &
                /= floor((Time-DtHorizontal)/DtPlotElectrodynamics) ) &
                call PW_print_electrodynamics
        endif
     enddo TIMELOOP
  else
     NLOOP:do
        if (nStep >= MaxStep) exit NLOOP
        DtHorizontal = DtHorizontalOrig
        do iLine=1,nLine
           ! move_line moves the flux tube, then we can use the angular
           !position to get the lat and lon
           call move_line
           
           !  Call the flux tube to be solved
           call PW_advance_line
        enddo
        
     enddo NLOOP
  end if

  !****************************************************************************
  !  Write output, use cartesian coords for output
  !****************************************************************************

  if (nLog == -1) then
     do iLine=1,nLine
        close(iUnitOutput(iLine))
     enddo
  elseif(nLog ==0) then
     !do nothing in this case
  elseif(nLog==iLineGlobal(iLine)) then
     close(iUnitOutput(iLine))
  else
  end if

  !\
  ! Deallocate variables needed for simulation
  !/
  deallocate(r_C, State_CVI, GeoMagLat_I,GeoMagLon_I,     &
       ThetaLine_I, PhiLine_I, xLine_I, yLine_I, zLine_I, &
       xLineOld_I, yLineOld_I, zLineOld_I, UthetaLine_I,  &
       UphiLine_I, UxLine_I, UyLine_I, UzLine_I,          &
       OmegaLine_I, JrLine_I, iThetaLine_I,iPhiLine_I,    &
       NameRestartIn, NameRestart, NameGraphics,          &
       NameOutput,  iUnitRestart, iUnitRestartIn,         &
       iUnitGraphics,iUnitOutput, iLineGlobal,IsNorth_I)
  

  call timing_stop('PWOM')
    if (iProc == 0) then
     write(*,'(a)') 'Finished PWOM run, (reporting timings)'
     write(*,'(a)') '--------------------------------------'
     call timing_report
  endif


  call MPI_FINALIZE(errcode)



end program pw


