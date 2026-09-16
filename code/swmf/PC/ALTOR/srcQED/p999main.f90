program focus_atto
  use constants
  use fields
  use mask_, only : mask
  use particles
  use variables_data
  use grid, only : grid_
  use particles_initiate , only : particles_foil__, add_particles__ !???
  use particles_move, only : move_particles_
  use fields_initiate, only : field_init_constant__,field_init__, &
       prof, phase
  use fields_advance, only : field_B__,field_E__, &
       field_bc_B__,field_bc_E__,field_energy__,move_fields__
  use mover, only : mover__,reduce_current__
  use density, only : density__,reduce_density__
  !
  use moving_box
  !
  use variables_out
  use output_field, only : output_field__, energy_density, output_density_, &
       output_current__ 
  use output_phase,only: output_phase_plane,output_phase_volume &
       ,output_phase_out,output_phase_all_out
  !
  use i_chi_, only : i_chi
  !
  use MPImodule
  use real_kind
  !
  implicit none
  integer :: k,j,numLst
  logical :: logField
  logical :: logPartTest=.false. !.true. !.true.!
  logical :: stop_run
  logical :: logCpuField, logCpuPart, logCpuExch, logCpuMove, logCpuOut  
  real :: tmp 
  real :: xPartMin(1),xPartMinGlob(1) !only for start
  real :: enerCompare,enerCompare0
  real (real_8) :: cpuStart,cpuMax,cpuDt,cpuBefore,cpuAfter,cpuEnd,cpu1,cpu2
  real (real_8) :: cpuField, cpuPart, cpuExch, cpuMove, cpuOut
  !
  logCpuField=.true.; logCpuPart=.true.; logCpuExch=.true.; &
       logCpuMove=.true.; logCpuOut=.true.
  !logCpuField=.false.; logCpuPart=.false.; logCpuExch=.false.; &
   !    logCpuMove=.false.; logCpuOut=.false.
  !
  numLst=30
  rFreqMax=alog(FreqMax)

  !write(*,*) sign(0.5,1.),sign(0.5,-1.)
  call MPIinit !????Warning: Floating underflow occurred???
  call MPIcomm_rank
  call MPIcomm_size


  !mpiHost=0
  cpuStart=MPI_WTIME()

  if (mpiProc == mpiHost) then 
     !open(unit=numLst,file='0000.lst) !,form='FORMATTED')
     !write(numLst,*)
     write(*,*)' Start...'
     write(*,*)'cpuStart=',cpuStart
     write(numLst,*) !int(4.4),aint(4.4),anint(4.3),ceiling(4.2),int(4.2)
     !write(numLst,*) !int(4.6),aint(4.6),anint(4.6), &
      !    floor(4.6),nint(4.6),nint(4.2)
  end if

  !call MPIproc_compare
  !call MPIcart_create

  nameLocal='MAIN'
  logPrint=.true.
  logOutput=.true.
  !write(*,*)' >>>> PE ',mpiProc
  !call MPI_BARRIER(MPI_COMM_WORLD,k)
  !call MPI_FINALIZE
  !stop
  cpuField=ZERO; cpuPart=ZERO; cpuExch=ZERO; cpuMove=ZERO; cpuOut=ZERO
  !
  call grid_
  !===========================ionization initiate
  if (logIoniz) then 
     nCharge=0
  else 
     nCharge=1
  endif
  !===========================ionization initiate end

  logPrint=.false.
  if(logPrint .and. (mpiProc == mpiHost)) &
       write(*,*) 'PE',mpiProc,' nDim,nMax=',nDim,nMax
  !call MPI_BARRIER(MPI_COMM_WORLD,k)
  !
  !
  logField=.true. !.false. !
  !
  if(logPartTest) then !---------------------------------initiate particles
     !
     np1(1:nPartSorts)=1; np2(1:nPartSorts)=1
     R(1:nDim+3, 1)=ZERO
     R(nDim, 1)=0.0 !2D: y
     R(1, 1)=10.0 ! x
     R(nDim+1, 1)=-783.0 !-10.0 ! Px
     !R(1:nDim+3, 1)=(/15.,10., -10.,0.,0./) !for test
     if(mpiProc == mpiHost) write(*,*) 'PE',mpiProc,&
          ' Particle test: ',R(1:nDim+3, 1),'  nPartSorts=',nPartSorts
     xPartMin=minval(R(1,np1(1):np2(1))) 
     xPartMinGlob=xPartMin !nothing to exchange
     !
     logCpuField=.false.; logCpuPart=.false.; logCpuExch=.false.; &
          logCpuMove=.false.; logCpuOut=.false.
     !
  else
     np1(1:nPartSorts)=1; np2(1:nPartSorts)=0
     call particles_foil__
     xPartMin=minval(R(1,np1(1):np2(1)))
     call MPIallreduce_min(xPartMin,xPartMinGlob,1)
     !
  end if !-------------------------------------------end initiate particles
  !
  write(*,*)'>>>>>PE',mpiProc,' np1(1),np2(1)=',np1(1),np2(1)
  if(logPrint) write(*,*) &
       'PE',mpiProc,xMax,np1(1),np2(1),np1(2),np2(2),nPartMax
  if(logPrint) write(*,*) &
       'PE',mpiProc,(minval(R(k,np1(1):np2(1))),&
       maxval(R(k,np1(1):np2(1))),k=1,3)
  !
  !------------------------------------------------------------------------
  do j=0,nChi-1 
     radChi(j+1)=i_chi(ChiMax/(nChi-1)*j)
  end do
    if(mpiProc == mpiHost) write(*,*)'>>>>>PE',mpiProc,' >>ChiMax=',&
         ChiMax,' >>I_CHI',&
         radChi(1),radChi(nChi),maxval(radChi)
  !------------------------------------------------------------------------
  !
  logPrint=.true.
  !logPrint=.false.
  !
  logOutput=.true. ! .and. (mpiProc == mpiHost)

  iTime=0
  time=ZERO
  rad=ZERO !prepare to sum radiation
  radAngle=ZERO !prepare to sum angular radiation
  !
  if(iLogRad /= 0) RD=ZERO
  !
  !write(*,*) 'Particles: ',iTime,j,sum(nCharge(np1(j):np2(j)))
  call field_init_constant__

  if ( nMask (1) > 0 ) call mask
  !write(*,*) 'MASK>>',nMask
  !write(*,*) rMask
  !write(*,*) 'MASK>>'

  !output of selected particles distribution <for test at PE0>

  if(mpiProc == mpiHost) then 
     open(unit=2,file='part.00',form='formatted')
     write(2,*) nDim,int(ONE/dt),timeMax
     !if (np1(1)<=np2(1) .and.logPartTest)write(2,*)iTime,time,R(:,1),RD(1:6,1)
     open(unit=3,file='ener.00',form='formatted')
     write(3,*) nDim,int(ONE/dt),timeMax &
          ,nPartSortMax,nPartSorts,TWO/nPartCell*omega**2*mass &
          ,4*nPartSortMax+1 
  end if
  !
  cpuBefore=MPI_WTIME()
  !
  do while(time < timeMax) !=======================main cycle
     !
     time=time+dt
     iTime=iTime+1
     xStart=xStart+dt !???
     iStart=xStart/dx(1)+3
     if (iStart > nMax(1)) iStart=nMax(1)
     if (mpiProc == mpiHost .and. .false.) then
        write(*,*) ' MAIN ?',time,iTime,xStart,iStart
     end if

     !
     !if (logIoniz .and. (time  >= 6.0) .and. (time  <= 6.0+dt) )  nCharge=1
     !
     if (logTest) C=ZERO
     !     
     if(logField) then
        !
        if(logCpuField) cpu1=MPI_WTIME()
        !
        if (logAntenna) then 
           do jBeam=1,nBeam 
              call field_init__(time)
           end do
        end if

        call field_B__
        call field_bc_B__

        call field_E__
        call field_bc_E__
        
        if (logAntenna) then 
           do jBeam=1,nBeam 
              call field_init__(time+dtHalf)
           end do
        end if

        call field_B__
        call field_bc_B__
        !
        if(logCpuField) cpu2=MPI_WTIME()
        if(logCpuField) cpuField=cpuField+cpu2-cpu1
        !
     end if
     !
     !if(logPrint) write(*,*)' current...'
     !
     logPrint=.false. !.true. !.false.
     C=ZERO 
     !if (time  >= 12.0 .and. time <= 24.0 ) then ! simple ionization model
     !
     if(logCpuPart) cpu1=MPI_WTIME()
     !
     if (xStart >= xPartMinGlob(1)) then !move particles! 
           !exit->>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
           !write(*,*)' >>>> PE ',mpiProc
           !call MPI_BARRIER(MPI_COMM_WORLD,iError)
           !call MPI_FINALIZE
           !stop
           !exit end->>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

        !
        do j=1,nPartSorts !*0
           logRadIn=(iLogRad/=0).and.(j==1)
           if (np1(j) <= np2(j)) &
!           if (np1(j) <= np2(j).and.xStart >= 53.) & !TEST+++++++++QED
                call mover__(np1(j),np2(j),dtChargeMassInvHalf(j),charge(j), &
                mom(j,1:3),enerKin(j),nPartWork(j))
        end do
        !
        if( logPartTest ) C=ZERO !only for test particles!
        !
        if(logCpuPart) cpu2=MPI_WTIME()
        if(logCpuPart) cpuPart=cpuPart+cpu2-cpu1
        !
        if( .not.logPartTest ) then !------------------------------exchange
           !
           if (mpiProcN > 1 .and. any( (np2-np1) > 1)  ) then 
              if(logCpuExch) cpu1=MPI_WTIME()
              call reduce_current__
              if(logCpuExch) cpu2=MPI_WTIME()
              if(logCpuExch) cpuExch=cpuExch+cpu2-cpu1
           end if
           !
        end if !-----------------------------------------------end exchange
        !
     end if
     !
     ! Moving frame along x-axis
     !
     if (logMove) then 
        !
        logPrint=.false. !.true. !.false.
        !
        if(logCpuMove) cpu1=MPI_WTIME()
        !
        if (nxMove /= 0 .and. mod(iTime,ntMove) == 0 .and. time > tMove) then
           xMin(1)=xMin(1)+dx(1)*nxMove
           xMax(1)=xMax(1)+dx(1)*nxMove
           call move_fields__
           if (logPrint) &
                write(*,*) 'PE',mpiProc,'MOVE>>time-> ',iTime,time &
                ,xMin(1),xMax(1)
           !
           do j=1,nPartSorts
              !
              if (np1(j) <= np2(j) .and. logPrint) & !== for test
                   write(*,*) 'PE',mpiProc, &
                   'MOVE>>1 Ini ',' j=',j,'  n=',np1(j),np2(j)
              !
              call move_particles_(np1(j),np2(j),1-2*(j-1))
              !
              if (logPrint) &
                   write(*,*) 'PE',mpiProc, &
                   'MOVE>>2 Cut ',' j=',j,'  n=',np1(j),np2(j)
           end do
           !
           if (logAdd .and. time < tAddStop &
                .and. .not.logPartTest &
                ) then 
              iBound1(1)=nx-iBound2(1)-nxMove
              call add_particles__
              j=1
              if (logPrint) &
                   write(*,*) 'PE',mpiProc, &
                   'MOVE>>3 Add  j=',j,'  n=',np1(j),np2(j)
              j=2
              if (np2(j) >= np1(j) .and. logPrint) &
                   write(*,*) 'PE',mpiProc, &
                   'MOVE>>3 Add  j=',j,'  n=',np1(j),np2(j)
           end if
           !write(*,*) 'MOVE>>',iTime,xStart,xMin(1),xMax(1),&
           !    ( ( np2(j)-np1(j) ), j=1,nPartSorts )
        end if
        !
        logPrint=.true.
        !
        if(logCpuMove) cpu2=MPI_WTIME()
        if(logCpuMove) cpuMove=cpuMove+cpu2-cpu1
        !
     end if
     !
     ! >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     ! Output of selected particles distribution <for test at PE0>
     ! >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     !
     if(mpiProc == mpiHost .and. np1(1) <= np2(1) &
          .and. logPartTest &
          .and. R(1,1) <= xStart+dx(1) &
          ) &
          write(2,*) iTime,time,R(:,1) ,RD(:,1)
     !
     ! >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     ! Output of energy evolution <at PE0>
     ! >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     !
     if(iTime/nOutputTime*nOutputTime == iTime) then
        ! sum particle energy and momentum
        !
        if(logCpuOut) cpu1=MPI_WTIME()
        !
        nameLocal='MAIN OUTPUT:'
        !
        call energy_density
        tmp=maxval(Z) ! max value for W=E^2+B^2
        !
        rCommArray(1:4*nPartSortMax+1)=(/enerKin,mom,rad/)
        iCommArray(1:nPartSortMax)=(/nPartWork/)
        call MPIreduce_sum_r(rCommArray,rCommArrayGlob,4*nPartSortMax+1)
        call MPIreduce_sum_i(iCommArray,iCommArrayGlob,nPartSortMax)
        if(mpiProc == mpiHost) then
           call field_energy__
           rCommArrayGlob(1:nPartSortMax)= rCommArrayGlob(1:nPartSortMax) &
                *TWO/nPartCell*omega**2*mass
           rCommArrayGlob(4*nPartSortMax+1)=rCommArrayGlob(4*nPartSortMax+1) &
                *TWO/nPartCell*omega**2*mass(1) !!! radiation coef
           write(3,*) iTime,time,tmp,enerFieldE,enerFieldB,&
                rCommArrayGlob(1:4*nPartSortMax+1),&
                iCommArrayGlob(1:nPartSortMax)
        end if
        !
        if(logCpuOut) cpu2=MPI_WTIME()
        if(logCpuOut) cpuOut=cpuOut+cpu2-cpu1
        !
     end if
     !
     ! >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     ! Output of fields and currents 
     ! >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     !
     if(logOutput .and. iTime/nOutputField*nOutputField == iTime &
          .and. xStart >= xPartMinGlob(1) &
          .and. time > 0.009 ) then 
        !
        if(logCpuOut) cpu1=MPI_WTIME()
        !
        if(mpiProc == mpiHost) write(*,*)'iTime,time=',iTime,time,&
             xStart,xPartMinGlob(1),logPartTest
        write(nameProcOut,'(I2.2)') mpiProc
        write(nameTimeStep,'(I6.6)') iTime
        !
        if(mpiProc == mpiHost) then 
           Z=ZERO
           call output_field__ ! same in all procs
           call output_current__ !exchanged => same in all procs
        end if
        !
        if(logCpuOut) cpu2=MPI_WTIME()
        if(logCpuOut) cpuOut=cpuOut+cpu2-cpu1
        !
     end if
     !
     ! >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     ! Output of densities 
     ! >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     !
     if(logOutput .and. iTime/nOutputField*nOutputField == iTime &
         .and. xStart >= xPartMinGlob(1) &
          !.and. .not.logPartTest &
          ) then 
        !
        if(logCpuOut) cpu1=MPI_WTIME()
        !
        logPrint=.false.!logPrint=.true.
        !
        !non-mpi works, mpi to check
        do j=1,nPartSorts
           if (np1(j) <= np2(j)) then
              Z=ZERO
              !call MPI_BARRIER(MPI_COMM_WORLD,k)
              call density__(np1(j),np2(j))
              call reduce_density__
              if(mpiProc == mpiHost) then 
                 call output_density_(namePart(j),charge(j))
                 !write(*,*) 'Particles: ',iTime,j,logIoniz, &
                  !    sum(nCharge(np1(j):np2(j)))
              end if
           end if
           !
        end do
        !
        ! writing radiation
        !
        if(iLogRad/=0) then
           !
           include "p999rad.fh"
           !
        end if
        !
        !
        if(logCpuOut) cpu2=MPI_WTIME()
        if(logCpuOut) cpuOut=cpuOut+cpu2-cpu1
        !
     end if
     !
     ! >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     ! Output of phase spaces
     ! >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
     !
     if(logOutput .and. iTime/nOutputPhase*nOutputPhase == iTime &
          .and. xStart >= xPartMinGlob(1) &
          .and. .not.logPartTest &
          ) then 
        !
        if(logCpuOut) cpu1=MPI_WTIME()
        !
        if(logPrint) write(*,*)nPhase
        !
        do j=1,nPartSorts
           if (np1(j) < np2(j)) then
              !call MPI_BARRIER(MPI_COMM_WORLD,k)
              call output_phase_out(np1(j),np2(j),namePart(j))
              if(iTime/nOutputPhaseAll*nOutputPhaseAll == iTime) &
                   call output_phase_all_out(np1(j),np2(j),namePart(j))
           end if
        end do
        !
        if(logCpuOut) cpu2=MPI_WTIME()
        if(logCpuOut) cpuOut=cpuOut+cpu2-cpu1
        !
     end if
     !
     ! checking stability if beamSize < xLength
     !
     if(mpiProc == mpiHost  &
          .and. all(beamSize(1:nBeam,1) < nMax(1)*dx(1)) &
        ) then  
        if (iTime <= nx/mx*mt) then 
           enerCompare0=sum(enerFieldE+enerFieldB)
        else 
           enerCompare=sum(enerFieldE+enerFieldB)
           if (enerCompare > enerCompare0*1.05) then
              open(unit=20,file='stop.run',form='formatted')
              write(20,*)'STOP: iTime,time=',iTime,time,nx/mx*mt,iTime,&
                   enerCompare,enerCompare0
              close(20)
           end if
        end if
     end if
     !
     cpuAfter=MPI_WTIME()
     cpuDt=cpuAfter-cpuBefore
     cpuBefore=cpuAfter
     !
     if(mpiProc == mpiHost .and. .not.logPartTest) &
          write(2,*) iTime,cpuDt,cpuAfter,&
          cpuField,cpuPart,cpuExch,cpuMove,cpuOut
     if(mpiProc == mpiHost .and. logPartTest) then    !--------------->
        if( np1(1) > np2(1) &
!             .or. R(nDim+1,1) >= ZERO & !TEST ONLY QED !!!!CAREFUL
             .or. any(R(1:nDim,1) <= xMin) .or. any(R(1:nDim,1) >= xMax) &
             ) then 
              open(unit=20,file='stop.run',form='formatted')
              write(20,*)'STOP: iTime,time=',iTime,time,' PartTest',R(:,1)  
        end if
     end if
     ! if (cpuAfter > cpuLimit) 
     !
     call MPI_BARRIER(MPI_COMM_WORLD,iError)
     !
     inquire(file='stop.run',exist=stop_run)
     if(stop_run) exit
     !
  end do !============================================end main cycle
  !
  if(mpiProc == mpiHost) then 
     close(2)
     close(3)
  end if
  !
  !
  ! writing radiation
  !
  if(iLogRad/=0) then
     !
     include "p999rad.fh"
     write(20,*)'Radiation written: iTime,time=',iTime,time 
  
  end if
  !
  !
  !
  cpuEnd=MPI_WTIME()
  !
  if(mpiProc == mpiHost) then
     write(*,*)'END: iTime,time=',iTime,time
     !write(*,*)'cpu, cpuStart, cpuEnd=',cpuEnd-cpuStart, cpuStart, cpuEnd
     write(*,*)'cpu, cDt=',cpuEnd-cpuStart, cpuDt
     tmp = cpuField + cpuPart + cpuExch + cpuMove + cpuOut
     write(*,*)'cpuSeparate=',tmp,tmp/(cpuEnd-cpuStart)*100,'%'
     !write(*,FMT="(10A,F12.2,F6.2,1A/)") &
     if(logCpuField) write(*,*) 'cpuField= ',cpuField,'  ',cpuField/tmp*100,'%'
     if(logCpuPart) write(*,*) 'cpuPart=  ',cpuPart,'  ',cpuPart/tmp*100,'%'
     if(logCpuExch) write(*,*) 'cpuExch=  ',cpuExch,'  ',cpuExch/tmp*100,'%'
     if(logCpuMove) write(*,*) 'cpuMove=  ',cpuMove,'  ',cpuMove/tmp*100,'%'
     if(logCpuOut) write(*,*) 'cpuOut=   ',cpuOut,'  ',cpuOut/tmp*100,'%'     
     !close(numLst)
  end if
  !
  call MPIfinalize
  !
end program focus_atto
!
subroutine CON_stop(StringError)
  use ModMpi
  implicit none
  !-------------------
  integer:: iError=0
  character (len=*), intent(in) :: StringError
  write(*,'(a)')StringError
  
  !Finalize MPI
  call MPI_Finalize(iError)
  
  stop
end subroutine CON_stop
!============================================================================
subroutine CON_set_do_test(String,DoTest,DoTestMe)
  implicit none
  character (len=*), intent(in)  :: String
  logical          , intent(out) :: DoTest, DoTestMe
end subroutine CON_set_do_test

