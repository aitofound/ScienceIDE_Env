  !  Copyright (C) 2002 Regents of the University of Michigan,
  !  portions used with permission
  !  For more information, see http://csem.engin.umich.edu/tools/swmf
module PT_ModParticle
   use PT_ModConst, ONLY: cProtonRestEnergy, cProtonMass, cLightSpeed, &
                          cMu, cMeV, ckeV
   use PT_ModSize, ONLY: nDim
   use PT_ModTime, ONLY: PTTime

   use PT_ModSolver, ONLY: euler_sde

   implicit none
   SAVE

   real, allocatable :: Particle_IV(:,:)
   integer :: nInject
   integer, allocatable :: nParticleOnLine(:)

   integer, parameter :: LagrCoord_    = 1,  &
                         Momentum_     = 2,  & ! p^3 / 3
                         Time_         = 3,  &
                         R_            = 4,  &
                         Weight_       = 5,  &
                         HasSplit_     = 6,  &
                         SplitLevel_   = 7,  &
                         nSplitVar     = 7,  & ! number of variables if splitting
                         nVar          = 5     ! number of variables if not splitting

   integer      :: nSplit
   real         :: SplitEnergyMin ! = 1.d0*MeV      ! energy of first split level
   real         :: SplitEnergyMax ! = 20000.d0*MeV  ! energy of last split level
   real, allocatable :: SplitEnergy_I(:), SplitMomentum_I(:)
   logical :: UseSplit = .false.

contains
  !============================================================================
   subroutine read_param(NameCommand)

      use ModReadParam, ONLY: read_var
      use ModUtilities, ONLY: CON_stop

      character(len=*), intent(in):: NameCommand ! From PARAM.in
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
      select case(NameCommand)
      case('#PARTICLE')
         call read_var('nInject', nInject)
         call read_var('UseSplit', UseSplit)
         call read_var('nSplit', nSplit)
         call read_var('SplitEnergyMin', SplitEnergyMin)
         call read_var('SplitEnergyMax', SplitEnergyMax)

         ! Convert to joules
         SplitEnergyMax = SplitEnergyMax*cMeV
         SplitEnergyMin = SplitEnergyMin*cMeV

      case default
         call CON_stop(NameSub//' Unknown command '//NameCommand)
      end select

   end subroutine read_param
  !============================================================================
   subroutine init
      use PT_ModGrid, ONLY: nLine
      use PT_ModSize, ONLY: nVertexMax
      integer :: MaxTotalParticles

    !--------------------------------------------------------------------------
      MaxTotalParticles = nVertexMax*nInject

      if(UseSplit) then
         MaxTotalParticles = MaxTotalParticles + MaxTotalParticles*nSplit
         allocate(Particle_IV(1:MaxTotalParticles, 1:nSplitVar))
         call init_split_grid
      else
         allocate(Particle_IV(1:MaxTotalParticles, 1:nVar))
      end if

      allocate(nParticleOnLine(1:nLine))
      nParticleOnLine = 0

   end subroutine init
  !============================================================================
   subroutine init_split_grid()

      use PT_ModProc, ONLY: iProc
      use PT_ModUnit, ONLY: kinetic_energy_to_momentum

      character(len=*), parameter :: OutputDir = 'PT/IO2/'
      character(len=*), parameter :: SplitFile = 'split_energies.dat'

      real :: dLogEsplit
      ! Loop variable:
      integer :: iSplit
    !--------------------------------------------------------------------------

      if(.not.allocated(SplitEnergy_I)) allocate(SplitEnergy_I(1:nSplit+1))
      if(.not.allocated(SplitMomentum_I)) allocate(SplitMomentum_I(1:nSplit+1))

      SplitEnergy_I(1)   = SplitEnergyMin

      SplitMomentum_I(1) = &
         kinetic_energy_to_momentum(SplitEnergyMin)**3.0/3.0

      dLogEsplit = log10(SplitEnergyMax/SplitEnergyMin)/real(nSplit)

      do iSplit = 2, nSplit+1
         SplitEnergy_I(iSplit) = &
            10**(log10(SplitEnergyMin) + dLogEsplit*(iSplit-1))
         SplitMomentum_I(iSplit) = &
            kinetic_energy_to_momentum(SplitEnergy_I(iSplit))**3.0 / 3.0
      end do

      ! Write energy splitting grid to file
      if(iProc==0)then
         open(45,file=OutputDir//splitFile,status='unknown')
         do iSplit = 1, nSplit+1
            write(45,*)iSplit, SplitEnergy_I(iSplit)/cMeV
         end do
         close(45)
      end if

      ! array no longer needed
      deallocate(SplitEnergy_I)

   end subroutine init_split_grid
  !============================================================================
   subroutine inject_particles(iLine, Time, LagrCoord)
      use PT_ModFieldline, ONLY: get_particle_location, calc_weight
      use PT_ModDistribution, ONLY: increase_total_weight, InjPcubed, InjEnergy
      use PT_ModUnit, ONLY: kinetic_energy_to_momentum

      integer, intent(in) :: iLine
      real, intent(in) :: Time, LagrCoord
      integer :: i, iStart, iEnd

    !--------------------------------------------------------------------------
      iStart = nParticleOnLine(iLine) + 1
      iEnd = nParticleOnLine(iLine) + nInject

      do i = iStart, iEnd
         ! Set psuedo-particle initial time, radial distance,
         ! and lagrangian coord on fieldline
         Particle_IV(i, Time_) = Time
         Particle_IV(i, LagrCoord_) = LagrCoord
         call get_particle_location(Time, Particle_IV(i, LagrCoord_), Particle_IV(i, R_))

         ! Set psuedo-particle energy/momentum
         Particle_IV(i, Momentum_) = InjPcubed

         ! statistical weight of psuedo-particle is:
         ! thermal energy density at the shock * dSoverB
         call calc_weight(Particle_IV(i, Time_), &
                          Particle_IV(i, LagrCoord_), &
                          Particle_IV(i, Weight_))

         call increase_total_weight(Particle_IV(i, Weight_))

         ! particle splitting variables
         ! HasSplit = whether this particle has already split
         ! SplitLevel = index of energy splitting grid to check if particle has crossed
         if(UseSplit) then
            Particle_IV(i, HasSplit_) = 0
            Particle_IV(i, SplitLevel_) = 1
         end if

         nParticleOnLine(iLine) = nParticleOnLine(iLine) + 1
      end do

   end subroutine inject_particles
  !============================================================================
   subroutine check_split(iParticle, DoSplit)

      use PT_ModUnit, ONLY: momentum_to_kinetic_energy
      integer, intent(in) :: iParticle
      logical, intent(out) :: DoSplit

      integer :: SplitLevel, ParentNumChildren
      real    :: Momentum

    !--------------------------------------------------------------------------
      DoSplit = .false.

      ! current energy threshold index of splitting
      SplitLevel = int(Particle_IV(iParticle, SplitLevel_))

      ! if max split energy threshold has not yet been reached
      ! if particle crosses next energy threshold
      ! if particle has not yet split
      Momentum = Particle_IV(iParticle, Momentum_)
      if(SplitLevel <= nSplit &
         .and.Momentum > SplitMomentum_I(SplitLevel) &
         .and.Particle_IV(iParticle, HasSplit_) == 0) DoSplit = .true.

   end subroutine check_split
  !============================================================================
   subroutine split_particle(iParticle, iLine)

      ! index of particle being split, total number of current particles in simulation
      integer, intent(in) :: iParticle, iLine
      integer :: ParentIndex
      integer :: SplitInd

      ! index of newly split particle
    !--------------------------------------------------------------------------
      SplitInd = nParticleOnLine(iLine) + 1

      ! increase split level of current particle
      Particle_IV(iParticle, SplitLevel_) = Particle_IV(iParticle, SplitLevel_) + 1

      ! Copy particle information
      Particle_IV(SplitInd, :) = Particle_IV(iParticle, :)

      ! Record particle has split - (new particle can still split again)
      Particle_IV(iParticle, HasSplit_) = 1

      ! adjust weights of split particles
      ! conserves total weight
      Particle_IV(SplitInd, Weight_) = Particle_IV(SplitInd, Weight_) * 0.5
      Particle_IV(iParticle, Weight_) = Particle_IV(iParticle, Weight_) * 0.5

      ! Increase number of particles on fieldline
      nParticleOnLine(iLine) = nParticleOnLine(iLine) + 1

   end subroutine split_particle
  !============================================================================
   subroutine advance_particles(iLine, TimeLimit, BinTime)

      use PT_ModDistribution, ONLY: bin_particle, BinTimeWindow
      use PT_ModFieldline, ONLY: check_boundary_conditions
      use PT_ModUnit, ONLY: momentum_to_kinetic_energy

      integer, intent(in) :: iLine
      real, intent(in) :: TimeLimit, BinTime
      integer :: iParticle, nLagr, iShock, iShockOld, nProgress, iProgress
      logical :: DoSplit, IsOutside
      real    :: Energy, Timestep

      ! Loop over particles on this line
      ! with particle splitting, new particles can be added - while loop needed
    !--------------------------------------------------------------------------
      iParticle = 1

      PARTICLE: do while(iParticle <= nParticleOnLine(iLine))

         ! particle time loop
         ! adaptive timestepping
         TIME: do while(Particle_IV(iParticle, Time_) < TimeLimit)
            ! move particle one timestep
            call advance_particle(iParticle, TimeLimit, Timestep)

            ! check if particle left spatial boundaries
            call check_boundary_conditions(Particle_IV(iParticle, Time_),      &
                                           Particle_IV(iParticle, LagrCoord_), &
                                           Particle_IV(iParticle, R_),         &
                                           IsOutside)

            ! bin particle in space/time/momentum at end of time step
            ! Particle weight is its initial weight multipled fraction of binning time spent in bin
            if(Particle_IV(iParticle, Time_) >= (BinTime-BinTimeWindow)) then
               call bin_particle(Particle_IV(iParticle, LagrCoord_), &
                                 Particle_IV(iParticle, Momentum_),    &
                                 Particle_IV(iParticle, Weight_) * Timestep / BinTimeWindow)
            end if

            if(IsOutside) then
               ! Remove particle from simulation.
               ! Shift all particles down one index -
               ! subtract one from iParticle so that shifted particle is not skipped in loop

               call remove_particle_from_sim(iParticle, iLine)
               iParticle = iParticle - 1
               EXIT TIME
            else
               ! particle splitting:
               ! total weight is conserved
               if(UseSplit) then
                  call check_split(iParticle, DoSplit)
                  if(DoSplit) then
                     call split_particle(iParticle, iLine)
                  end if
               end if
            end if

         end do TIME ! particle time loop
         ! move to next particle
         iParticle = iParticle + 1
      end do PARTICLE

   end subroutine advance_particles
  !============================================================================
   subroutine advance_particle(iParticle, tStepMax, Timestep)
      use PT_ModFieldline, ONLY: get_particle_location
      use PT_ModUnit, ONLY: momentum_to_kinetic_energy
      integer, intent(in) :: iParticle
      real, intent(in) :: tStepMax
      real, intent(out) :: Timestep

      ! Vectors solved by SDE
      real :: XOld(nDim), XNew(nDim)

      ! Equation advances lagrcoord and p^3/3 not p
    !--------------------------------------------------------------------------
      XOld(LagrCoord_) = Particle_IV(iParticle, LagrCoord_)
      XOld(Momentum_)  = Particle_IV(iParticle, Momentum_)

      ! Advance psuedo-particle one time step
      call euler_sde(XOld, tStepMax, Particle_IV(iParticle, Time_), &
                     Timestep, XNew)

      ! Update Lagrangian coordinate and momentum
      Particle_IV(iParticle, LagrCoord_) = XNew(LagrCoord_)
      Particle_IV(iParticle, Momentum_)  = XNew(Momentum_)

      ! Update particle time
      Particle_IV(iParticle, Time_) = Particle_IV(iParticle, Time_) + Timestep

      ! Update radial distance (R)

      call get_particle_location(Particle_IV(iParticle, Time_), &
                                 Particle_IV(iParticle, LagrCoord_), &
                                 Particle_IV(iParticle, R_))

   end subroutine advance_particle
  !============================================================================
   subroutine remove_particle_from_sim(iParticle, iLine)
      integer, intent(in) :: iParticle, iLine
      integer :: i

    !--------------------------------------------------------------------------
      if(iParticle < nParticleOnLine(iLine)) then
         ! Shift all particles down one index
         Particle_IV(iParticle:nParticleOnLine(iLine)-1, :) = &
            Particle_IV(iParticle+1:nParticleOnLine(iLine), :)
      end if

      ! set last particle to zero
      Particle_IV(nParticleOnLine(iLine), :) = 0
      ! reduce number of particles on this line
      nParticleOnLine(iLine) = nParticleOnLine(iLine) - 1

   end subroutine remove_particle_from_sim
  !============================================================================
   subroutine get_random_initial_energy(Energy)
      use PT_ModRandom, ONLY: get_random_uniform
      real, intent(out) :: Energy
      real :: RandUniform
      real :: Emin, Emax
      real, parameter :: cThreeHalf = 1.5
      ! outputs random energy in [J]
      ! f(E) is proportional to v^-5 or E^(-5/2)
      ! Random selection is done in energy to get better statistics
      ! at higher energies

      ! energy limits [keV]
    !--------------------------------------------------------------------------
      Emin = 10.0
      Emax = 300.0

      call get_random_uniform(RandUniform)

      Energy = (1.0 - RandUniform) * Emin**(-cThreeHalf) + &
               RandUniform * Emax**(-cThreeHalf)
      Energy = Energy**(-1.0/cThreeHalf) * ckeV

   end subroutine get_random_initial_energy
  !============================================================================
   end module PT_ModParticle
!==============================================================================
