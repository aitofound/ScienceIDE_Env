!  Copyright (C) 2002 Regents of the University of Michigan, 
!  portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf
! This file contains the top level methods for ALTOR
!==========================
subroutine PIC_setup
  use PIC_ModFIeld,     ONLY: State_VGBI
  use PC_ModMpi,        ONLY: pass_density, pass_moments
  use PIC_ModProc,      ONLY: iComm
  use PIC_ModMain,      ONLY: UseSharedField, UseUniform, UseFoil
  use PIC_ModLogFile,   ONLY: open_logfile, nLogFile
  use PIC_ModOutput,    ONLY: PIC_save_files, nStepOut
  use PIC_ModParticles, ONLY: uniform, foil, DoAddVelocity_P, &
       add_initial_velocity, nPType, Energy_P, advance_particles, &
       show_density, pass_energy
  use PIC_ModLaserBeam, ONLY: UseLaserBeam, check_laser_beam

  implicit none
  integer :: iSort
  character(len=*), parameter :: NameSub='PIC_setup'
  !------------------------------------------------
  if(UseUniform)call uniform
  if(UseFoil   )call foil
  if(any(DoAddVelocity_P))then
     do iSort = 1, nPType
        if(DoAddVelocity_P(iSort))&
             call add_initial_velocity(iSort)
     end do
  end if
  if(UseLaserBeam)call check_laser_beam
  !Save the initial outputs
  Energy_P = 0.0; State_VGBI = 0.0
  if(nStepOut>1)then
     do iSort = 1, nPType
        call advance_particles(iSort,&
             DoComputeMoments = .true., DoPredictorOnly = .true.)
        call pass_moments(iSort)
        call show_density(iSort)
     end do
     call PIC_save_files('INITIAL')
  else
     do iSort = 1, nPType
        call advance_particles(iSort,&
             DoComputeMoments = .false., DoPredictorOnly = .true.)
        call pass_density(iSort)
        call show_density(iSort)
     end do
  end if
  if(nLogFile >=1) then
     call pass_energy
     call open_logfile
  end if
end subroutine PIC_setup
!==========================
subroutine PIC_init_session(iSession)
  use PIC_ModMain, ONLY:IsInitialized
  integer, intent(in) :: iSession
  character(len=*), parameter :: NameSub='PIC_init_session'
  !-------------------------------------------------------------------
  if(.not.IsInitialized)then
     call PIC_setup
     IsInitialized = .false.
  end if
end subroutine PIC_init_session
!==============================
subroutine PIC_advance(tMax)
  use PIC_ModField,     ONLY: update_magnetic, Current_GDB, State_VGBI
  use PIC_ModParticles, ONLY: Energy_P, nPType, pass_energy
  use PIC_ModParticles, ONLY: advance_particles
  use PIC_ModField,     ONLY: update_e, field_bc, E_GDB, nGF 
  use PIC_ModMain,      ONLY: tSimulation, iStep, Dt, IsPeriodicField_D
  use PIC_ModLogFile,   ONLY: write_logfile, nLogFile
  use PIC_ModOutput,    ONLY: nStepOutMin, nStepOut, PIC_save_files
  use PC_ModMpi,       ONLY: pass_current, pass_density, pass_moments
  use PC_BATL_pass_face_field, ONLY: message_pass_field
  use PC_ModHybrid,     ONLY: UseHybrid
  implicit none
  real,intent(in) :: tMax
  integer :: iSort, iBlock
  character(len=*), parameter :: NameSub='PIC_advance'
  !--------------------------------------------------------------------
  !\
  !Avoid invinitesimal time step.
  !/
  if(tSimulation > tMax - 1.0e-8*Dt)then
     tSimulation = tMax
     RETURN
  end if
  call timing_start('advance')
  !\
  ! Electric and magnetic fields and the particle coordinates are at the 
  ! beginning of the time step, particle velocities are half
  ! time step behind. 
  !/
  !\
  ! Start update through the time step
  !/  
  !1. Prepare to move particles
  Current_GDB = 0.0; Energy_P = 0.0
  !2. Move particles

  State_VGBI = 0.0
  if(nStepOut>=1.and.nStepOutMin<=iStep&
       .and.mod(iStep,nStepOut)==0.and.iStep/=0)then
     do iSort=1, nPType
        !Calculate cell-centered number density and velocity while
        !advancing the particles
        call timing_start('adv_particles')
        call advance_particles(iSort,DoComputeMoments=.TRUE.)
        call timing_stop('adv_particles')
        !Save the moments
        call pass_moments(iSort)
     end do
     call PIC_save_files('NORMAL')
  else
     do iSort=1, nPType
        !Advance the particles without calculating cell-centered
        !number velocity 
        call timing_start('adv_particles')
        call advance_particles(iSort,DoComputeMoments=.FALSE.)
        call timing_stop('adv_particles')
        call pass_density(iSort)
     end do
  end if
  !3. Collect currents at the half time step 
  if(nPType > 0) call pass_current
  !\
  ! Electromagnetic fields and the particle energies are at  
  ! the beginning of the time step. Density and the particle 
  ! coordinates are at the end of the timestep.
  ! The particle velocities are in the middle of the timestep.
  !/
  if(nLogFile>=1)then
     !All energies in the logfile are at the beginning of the timestep.
     !For test particles velocities are in the middle of the timestep,
     !coordinates are at the end of it
     if(iStep/=0.and.mod(iStep,nLogFile)==0)then
        call pass_energy
        call write_logfile
     end if
  end if
  iStep = iStep + 1
  tSimulation = tSimulation + Dt
  !\
  !4. Update Magnetic field through a half timestep
  !/
  call timing_start('adv_b')
  call update_magnetic
  call timing_stop('adv_b')
  !\
  ! Electric fields are at the beginning of the time step.
  ! The particle coordinates are at the end of the timestep.
  ! The particle velocities and magnetic fields are in  
  ! the middle of the timestep.
  !/
  !5. Advance electric field
 
  call timing_start('adv_e')
  !\
  ! 5.1 calculate time dependent external  boundary conditions
  !/
  call field_bc
  !\
  ! 5.2 advance field within the physical blocks
  !/
  call update_e
  call timing_stop('adv_e')
  !\
  ! 5.3 update ghost face field values
  !/ 
  call message_pass_field(nGF, E_GDB)


  !\
  ! Electric fields are at the end of the time step.
  ! The particle coordinates are at the end of the timestep.
  ! The particle velocities and magnetic fields are in  
  ! the middle of the timestep.
  !/
  !6. Update the magnetic field through the half time step
  call timing_start('adv_b')
  call update_magnetic
  call timing_stop('adv_b')
  !\
  ! Electromagnetic fields and the particle coordinates are at the
  !end of time step, the particle velocities are half 
  !time step behind.
  !/
  !\
  ! For hybrid scheme: advance the particle to the end of time step and 
  ! improve electric field.
  !/
  if(.not.UseHybrid)then
     call timing_stop('advance')
     RETURN
  end if
  call update_e
  call message_pass_field(nGF, E_GDB)
  Current_GDB = 0.0
  do iSort = 1, nPType
     call advance_particles(&
          iSort,DoComputeMoments=.FALSE.,DoPredictorOnly=.true.)
  end do
  call pass_current
  call update_e
  call message_pass_field(nGF, E_GDB)
  call timing_stop('advance')

end subroutine PIC_advance
!==============================
subroutine PIC_finalize
  use PIC_ModLogFile, ONLY: nLogFile, write_logfile, &
       close_logfile
  use PIC_ModProc, ONLY: iProc
  use PIC_ModOutput, ONLY: PIC_save_files, nStepOut
  use PIC_ModParticles, ONLY: Energy_P, advance_particles
  use PC_ModMpi, ONLY: pass_moments, pass_density
  use PIC_ModParticles, ONLY: pass_energy, nPType
  use PIC_ModField, ONLY: State_VGBI
  implicit none
  integer :: iSort
  character(len=*), parameter :: NameSub='PIC_finalize'
  !--------------------
  Energy_P = 0.0; State_VGBI = 0.0
  if(nStepOut>1)then
     do iSort = 1, nPType
        call advance_particles(iSort,&
             DoComputeMoments = .true., DoPredictorOnly = .true.)
        call pass_moments(iSort)
     end do
     call PIC_save_files('FINAL')
  else
     do iSort = 1, nPType
        call advance_particles(iSort,&
             DoComputeMoments = .false., DoPredictorOnly = .true.)
     end do
  end if
  if(nLogFile >=1) then
     call pass_energy
     call write_logfile
     call close_logfile
  end if 
  if(iProc==0)then
     write(*,*)
     write(*,'(a)')'    Finished Saving Output Files'
     write(*,'(a)')'    ----------------------------'
  end if
end subroutine PIC_finalize
!=====================================================================
subroutine PC_user_specify_region(iArea, iBlock, nValue, &
     NameLocation, IsInside, IsInside_I, Value_I)
  implicit none

  integer,   intent(in):: iArea        ! area index in BATL_region
  integer,   intent(in):: iBlock       ! block index
  integer,   intent(in):: nValue       ! number of output values
  character, intent(in):: NameLocation ! c, g, x, y, z, or n

  logical, optional, intent(out) :: IsInside
  logical, optional, intent(out) :: IsInside_I(nValue)
  real,    optional, intent(out) :: Value_I(nValue)

  character(len=*), parameter :: NameSub = 'user_specify_region'
  !-------------------------------------------------------------------
end subroutine PC_user_specify_region

