!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
!==============================================================================
module PT_ModAdvance

    implicit none
    SAVE

contains
    !--------------------------------------------------------------------------
    subroutine advance(TimeLimit)  
        
        use PT_ModGrid,         only: nLine, Used_B, iShock_IB, Shock_, &
                                      ShockOld_
        use PT_ModParticle,     only: advance_particles, inject_particles
        use PT_ModFieldline,    only: set_fieldline
        use PT_ModDistribution, only: set_lagr_bins, update_integral_over_finj
        use PT_ModTime,         only: iIter, PTTime, DataInputTime
        use PT_ModProc,         only: iComm, iError, iProc

        real, intent(in) :: TimeLimit
        integer :: iLine, iProgress, iShock, iShockOld, nProgress, iShockNew
        real :: TotalDt, DtProgress, Time1,  Time2, Alpha, LagrInject

        ! Total timestep between MHD states
        TotalDt = DataInputTime - PTTime

        ! Loop over magnetic field lines
        LINE: do iLine = 1, nLine

            if(.not.Used_B(iLine)) CYCLE LINE

            ! set fieldline number in ModFieldline
            call set_fieldline(iLine)
            ! set lagr phase space bins for this time step
            call set_lagr_bins(iLine)

            ! indices of shock during previous and current MHD state
            iShock = iShock_IB(Shock_, iLine)
            iShockOld = iShock_IB(ShockOld_, iLine)

            ! how many vertices (lagrangian coordinates) the shock moved
            ! If the shock doesn't move - set nProgress = 1
            nProgress = max(1, iShock - iShockOld)

            if(iProc.eq.0) write(*,'(a, i5, i5, i5)') 'ShockOld, ShockNew, dShock: ', &
                                      iShockOld, iShock, iShock - iShockOld
            
            ! timestep for each subinterval where the shock moves one 
            ! lagrangian coordinate
            DtProgress = TotalDt / nProgress
        
            ! loop over subintervals where the shock moves one 
            ! lagrangian coordinate
            PROGRESS: do iProgress = 1, nProgress
                
                ! new shock vertex
                iShockNew = iShockOld + iProgress
                ! initial time for this subinterval
                Time1 = PTTime + (iProgress-1)*DtProgress
                ! final time for this subinterval
                Time2 = PTTime + (iProgress*DtProgress)
                ! inject particles at shock location
                LagrInject = iShockNew

                ! If shock did not move then do not inject psuedo-particles
                if(iShock.gt.iShockOld) then

                    ! inject particles at shock location at start of subinterval
                    call inject_particles(iLine, Time1, LagrInject)
                    ! integrate injected distribution for later normalization
                    if(iProc.eq.0) call update_integral_over_finj(Time1, LagrInject)

                end if

                ! advance psuedo-particles in time until NextTimeStep
                call advance_particles(iLine, min(TimeLimit, Time2), TimeLimit)

                if(Time2.ge.TimeLimit) EXIT PROGRESS

            end do PROGRESS

        end do LINE

    end subroutine advance
    !--------------------------------------------------------------------------
end module PT_ModAdvance
!==============================================================================