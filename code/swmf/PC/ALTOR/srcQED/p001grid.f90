module grid 
  use constants
  use variables_data
  use variables_box
  use variables_particles, only : nPart,nPartCell,nPartSorts
  use particles, only : iLogRad
  use MPImodule, only : mpiProc,mpiHost,MPIerror_report
  use variables_out, only : logPrint
  !use moving_box, only : ntMove !temporal
  implicit none
contains 
  subroutine grid_ ! should work for 1D,2D,3D
    integer :: k,j
    real :: tmp,amplitude0
    !
    nameLocal='GRID: DATA'
    !
    fieldInit=(/ZERO,ZERO,ZERO, ZERO,ZERO,ZERO/)
    !fieldInit=(/-ONE,ZERO,ZERO, ZERO,ZERO,ZERO/)*3000 !TEST
    logAntenna=.true. !.true. !.false. ! from the left boundary
    phaseShift=(/ZERO,ZERO/)   !ZERO !HALF !QUARTER !EIGHTH !THREE_FOURTH
    logTest=.false.!.false. !.true. ! for test particle motion: zero-th current
    !
    dx=ONE/mx
    if(nDim == 2) dx(nDim)=ONE/mx2
    dxHalf=dx*HALF
    !
    dt=ONE/mt !dx(1)*HALF
    dtHalf=dt*HALF
   !================================================================
    constLambda=constVelLight*constLaserT          !\lambda=cT
    !
    !constRad= ( \tau_0 / T) /dt
    constRad=(6.26e-24 /constLaserT) /dt
    !
    ! constCompton= (3/2) * ( \lambdabar_C / \lambda) /dt
    constCompton=THREE*HALF*(3.8616e-11/ConstLambda)/dt
    !
    ! constComptonInv=( \lambda / \lambdabar_C ) / (2\pi) 
    constComptonInv=(constLambda /3.8616e-11) / PI2 
    !=================================================================
    !
    dxInv=ONE/dx
    dtdxInv=dt/dx
    dtdxInvHalf=dt/dx*HALF 
    !
    omega=sqrt(30.0)   !00!40!0.0554257!45!0.0587878  
    nPartCell=product(nPart)
    weight=PI2*omega**2/nPartCell !???
    dxWeight(nDim+1:3)=weight; dxWeight(1:nDim)=weight*dx(1:nDim) 
    !
    xMin=ZERO+dx*nMask
    xMax=ZERO+dx*(nMax-nMask) !!!??? was xMin+dx*nMax
    xStart=xMin(1) !???
    iStart=xStart/dx(1)+3
    !
    charge=(/-1.,1./)
    mass=(/1.,1836.0 * TWO /) !!! attention !!!
    dtChargeMassInvHalf=PI2*dt*charge/mass*HALF ! 
    !
    focus(1,1:3)=(/ 11.0*ONE+2000.0*ZERO+xMin(1), 0., 0. /) ! for 2D,3D ???
    !focus(2,1:3)=(/ 4000.0+xMin(1), 0., 0. /) 
    focus(2,1:3)=focus(1,1:3)
    !
    forall (jBeam=1:nBeamMax) polarization(jBeam,:)=(/ONE,ZERO/)
    !f/1 focus=11, t=10. rk=0.478254 beamSize=(/5...
    !0.025 ionization
    !*0.721278  !10.*0.721278 !factor for f/1, focus=4
    !*0.608465 : focus=6, fact:5,  
    !*0.686210 : focus=10; beamSize=(/6.,5.,5./), tMax,xMax=+7, d=1.360
    !*0.793653 : focus=10; beamSize=(/6.,4.,4./), tMax,xMax=+5, d=1.460
    !*0.729371 : focus=12; beamSize=(/6.,5.,5./), tMax,xMax=+8, d=1.5400
    !*0.646171 :focus=(12,8) fact:7.5,5 beamSize=(/6.,5./) 33-35^o t=7 d=1.75
    !*0.574235:focus=(12,12) fact:5.5,5.85 beamSize=(/6.,5./) 45^0 t=8 d=1.85
    amplitude=(/1.0, 1.0/) *300.0 !/sqrt(2.0) !/sqrt(TWO)
                             !*0.6666667!/0.36 !2 pulses close
    ! (/0.12, 0.88/) *0.702374 !*0.398353 
    !24TW:1.67419!30TW:1.87180!12TW:1.18383
    amplitude0=maxval(amplitude)
    forall (jBeam=1:nBeamMax) &
         polarization(jBeam,:)=polarization(jBeam,:)*amplitude(jBeam)
    !
    beamSize(1, 1:3)=(/50.0, 25.5, 10./) ! d=20/0.8*1.7=42.5 !19x42
    beamSize(2, 1:3)=beamSize(1, 1:3) !(/18.0, 20.0, 10./) !beamSize(1, 1:3)
    !
    beamSize(1, 2:3)=focus(1,1)*TWO*tan(21./180.*PI)!21^0 0.767728 in y,z:f/1
    !
    !beamSize(1, 2:3)=focus(1,1)*TWO*tan(14./180.*PI)!21^0 0.767728 in y,z:f/1 
    !
    !ang=15,foc=10 W=4.5: *0.666667 x=8 d=1.35     
    !ang=14, foc=10 W=4.05; *0.702374 x=8 d=1.4
    !
    beamSize(2, 2:3)=beamSize(1, 2:3)
    !
    !for Fronts
    beamFront(1, 1:2)=(/2., 2./)
    !beamFront(2, 1:2)=beamFront(1, 1:2)
    beamFrontCoef(1, 1:2)=(/1.0, 1.0/)
    !
    !beamLimit - angle dependent !careful! 
    !for Gauss (/0,2/)
    !for Fronts(/0,1/) 
    !
    !for Gauss and Fronts
    do j=1,nBeam
       if (nProfile(j) == 1) then 
          beamLimit(j, 1:2)=(/0.,2./)*beamSize(j,1) !+xMin(1) !in time
       else
          beamLimit(j, 1:2)=(/0.,1./)*beamSize(j,1) !+xMin(1) !
       end if
    end do
    !
    !beamLimit(2, 1:2)=(/0.,2./)*beamSize(1,1) + 0.0 !!!!
    ! 
    !for Gauss and Fronts
    do j=1,nBeam
       if (nProfile(j) == 1) then 
          beamMiddle(j, 1:3)=(/-1.0, 0.5, 0.5/) 
       else
          beamMiddle(j, 1:3)=(/-0.5, 0.5, 0.5/) 
       end if
    end do    
    !
    do j=1,nBeam 
       !if (nProfile(j) == 1) then 
          beamMiddle(j,2:nDim)=beamMiddle(j,2:nDim)*&
               (xMax(2:nDim)-xMin(2:nDim))+xMin(2:nDim) !in box size
          beamMiddle(j,1)=beamMiddle(j,1)*beamSize(j,1) !+xMin(1)!in beam size
       !end if
    end do
    !
    !case with prepulse
    beamLimit(2, 1:2)=beamLimit(2, 1:2)-beamMiddle(1,1)+beamMiddle(2,1)
    beamMiddle(2, 1:3)=beamMiddle(1, 1:3)
    !
    !beamMiddle(1:2, 2)=beamMiddle(1:2, 2) - 5.0 !shift with the target/p202
    !
    ! Phase shift btw beams
    !beamMiddle(1,1)=beamMiddle(1,1)+0.5
    ! beam shift along Y
    ! ----!
    if(nBeam == 2) then 
       beamMiddle(1, 2)=beamMiddle(1, 2)-1.0
       beamMiddle(2, 2)=beamMiddle(1, 2)+1.0
    end if
    !
    if (logPrint .and. mpiProc == mpiHost) then 
       write(*,*) nameLocal,'dx,dxHalf=',dx,dxHalf
       write(*,*) nameLocal,'dt,dtHalf=',dt,dtHalf
       if(iLogRad /= 0) then 
          write(*,*) nameLocal,'constLambda=',constLambda
          write(*,*) nameLocal,'constRad=',constRad
          write(*,*) nameLocal,'constCompton=',constCompton
          write(*,*) nameLocal,'constComptonInv=',constComptonInv
       endif
       write(*,*) nameLocal,'dxInv=',dxInv
       write(*,*) nameLocal,'dtdxInv=',dtdxInv
       write(*,*) nameLocal,'dtdxInvHalf=',dtdxInvHalf
       write(*,*) nameLocal,'weight,mass,dxWeight=',weight,mass,dxWeight
       write(*,*) nameLocal,'nPhase=',nPhase
       write(*,*) nameLocal,'xMin,xMax=',xMin,xMax
       write(*,*) nameLocal,'xStart=',xStart
       write(*,*) nameLocal, &
            'dtChargeMassInvHalf=',dtChargeMassInvHalf
       write(*,*) nameLocal,'focus, polarization=',focus, polarization
       do j=1,nBeam
          write(*,*) nameLocal,'Beam ',j
          write(*,*) nameLocal,'beamSize,beamMiddle=',&
               beamSize(j,1:nDim),beamMiddle(j,1:nDim)
       write(*,*) nameLocal,'beamLimit=',beamLimit(j,:)
       end do
    end if
    !
    ! stability check ---------------------------------------------------
    nameLocal='GRID: STABILITY'
    !
    if (logPrint .and. mpiProc == mpiHost) then 
       do j=1,nPartSorts
          if(dtChargeMassInvHalf(j)*amplitude0 > ONE) then 
             write(*,*) nameLocal,' PE=',mpiProc
             write(*,*) nameLocal,j,'dtChargeMassInvHalf*amplitude=',&
                  dtChargeMassInvHalf(j)*amplitude0
             call MPIerror_report(11,nameLocal,&
                  'dtChargeMassInvHalfE is too big')
          end if
       end do
       !
       ! Courant's stability criterium
       !
       tmp=ONE/sqrt(sum(ONE/dx**2))
       write(*,*) nameLocal, &
            'dtmax/2 < dt < dtmax: ',tmp/2,dt,tmp
       !IDL: print, sqrt(total(1./dx^2))
       if(dt > tmp) call MPIerror_report(1,nameLocal,'dt > dt_max')
       if(dt < tmp*HALF) call MPIerror_report(2,nameLocal,'dt < dt_max/2')
       !
       ! particle integration stability
       !
       tmp=dt*omega*PI2
       write(*,*) nameLocal,'dt*omega_p < 1: ',tmp
       if(tmp >= ONE) &
            call MPIerror_report(3,nameLocal,&
            'particle integration is not stable')
       !
       tmp=THIRD*(dt*omega*PI2*HALF)**3/dt*10.
       write(*,*) nameLocal, &
            'phase error of particle integration in 10 cycles: ',tmp
       !
       tmp=THIRD*(dt*weight*amplitude0*PI2*HALF/sqrt(ONE+amplitude0**2))**3
       write(*,*) nameLocal,'rotation error < 1:  ',tmp
       if(dt >= ONE) &
            call MPIerror_report(4,nameLocal,&
            'dt or B is big, the rotation error')
    end if
  end subroutine grid_
  !
end module grid
