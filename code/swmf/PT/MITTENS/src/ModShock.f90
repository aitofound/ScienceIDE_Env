!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module PT_ModShock

  ! This module contains subroutines for determining the shock location,
  ! and steepening the density and magnetic field strength at shock front.
  use PT_ModGrid,   ONLY: nLine, nLineAll, Used_B, MinLagr, MaxLagr, &
       LagrID_, X_, Z_, State_VIB, MhData_VIB, &
       iShock_IB, NoShock_, Shock_, ShockOld_, check_line_ishock, &
       ShockUp_, ShockDown_, ShockUpOld_, ShockDownOld_

  use PT_ModSize,   ONLY: nVertexMax
  use ModUtilities, ONLY: CON_stop
  use PT_ModTime,   ONLY: DataInputTime, PTTime

  implicit none

  SAVE

  PRIVATE ! except

  ! Public members:
  public:: read_param           ! Read parameters
  public:: init                 ! Initialize arrays on the grid
  public:: get_dLogRho          ! calculate temporal change in density 
  public:: get_shock_location   ! finds shock location on all lines
  public:: sharpen_shock        ! steepen the density profile at the shock
  public:: get_test_shock

  ! If the shock wave is traced, the advance algorithms are modified
  logical, public :: DoSharpenShock = .false.
  ! divergence of velocity \vec{U}: for determining the shock locations
  real, public, allocatable :: dLogRho_II(:, :), dLogRhoOld_II(:, :)

  ! Shock algorithm parameters:
  real,    public :: dLogRhoThreshold = 0.0001 ! Empirical value
  integer, public :: nSearchMax = 50

  ! Parameters for the shock coordinates
  integer, public, parameter :: nShockVar = 2, &
     RShock_     = 1, &
     ShockSpeed_ = 2

contains
     !============================================================================
     subroutine read_param(NameCommand)

          use ModReadParam, ONLY: read_var
          character(len=*), intent(in):: NameCommand ! From PARAM.in
          character(len=*), parameter:: NameSub = 'read_param'
          !--------------------------------------------------------------------------
          select case(NameCommand)
          case('#SHOCK')
               call read_var('DoSharpenShock', DoSharpenShock)
               call read_var('nSearchMax', nSearchMax)
               call read_var('dLogRhoThreshold', dLogRhoThreshold)

          case default
               call CON_stop(NameSub//': Unknown command '//NameCommand)
          end select

     end subroutine read_param
     !============================================================================
     subroutine init

          ! initialize arrays related to the shock
          use ModUtilities, ONLY: check_allocate
          use PT_ModProc,   ONLY: iError
          character(len=*), parameter:: NameSub = 'init'
          integer :: iLine
          !--------------------------------------------------------------------------
          do iLine = 1, nLine
               iShock_IB(:, iLine) = MinLagr(iLine)
          end do

          if(allocated(dLogRho_II)) deallocate(dLogRho_II)
          allocate(dLogRho_II(1:nVertexMax, 1:nLine)) ! divU
          call check_allocate(iError, 'dLogRho_II')
          dLogRho_II = 0.0

          if(allocated(dLogRhoOld_II)) deallocate(dLogRhoOld_II)
          allocate(dLogRhoOld_II(1:nVertexMax, 1:nLine)) ! divU
          call check_allocate(iError, 'dLogRhoOld_II')
          dLogRhoOld_II = 0.0


     end subroutine init
     !============================================================================
     subroutine get_dLogRho

          ! divU = dLogRho_I for time-accurate run
          use PT_ModGrid, ONLY: Rho_, RhoOld_, MinLagrOld
          use PT_ModTime, ONLY: PTTime, DataInputTime

          ! Loop variables
          integer :: iLine, iMin, iMax
          !--------------------------------------------------------------------------

          do iLine = 1, nLine
               ! go line by line and get divU if active
               if(.not.Used_B(iLine)) then
                    iShock_IB(Shock_,iLine) = NoShock_
                    CYCLE
               end if
               iMin = MinLagr(iLine)
               iMax = MaxLagr(iLine)
               
               dLogRhoOld_II(:, iLine) = 0.0
               dLogRhoOld_II(iMin:iMax, iLine) = dLogRho_II(iMin:iMax, iLine)

               dLogRho_II(:, iLine) = 0.0
               dLogRho_II(iMin:iMax, iLine) = &
                    log(MhData_VIB(Rho_, iMin:iMax,iLine) / &
                    State_VIB(RhoOld_, iMin:iMax,iLine)) / &
                    (DataInputTime - PTTime)
               
               ! if new particles were introduced - avoid NaNs
               if(MinLagr(iLine).lt.MinLagrOld(iLine)) &
                    dLogRho_II(iMin:MinLagrOld(iLine), iLine) = 0.0

          end do
     end subroutine get_dLogRho
     !============================================================================        
     subroutine sharpen_shock
          use PT_ModGrid, only: get_density_gradient, Rho_, R_, B_, dB_, D_
          use PT_ModProc, only: iProc

          integer :: iLine
          real    :: dLogRhoSum

          ! Sum the total density jump across the detected shock zone and
          ! redeposit it on a 3-cell binomial stencil (1/4, 1/2, 1/4)
          ! centered at the shock. SDE solvers need a finite SMOOTH shock:
          ! the analytic width scan shows a 1-2 cell quasi-discontinuity
          ! biases the DSA slope (-4.6 vs -4.0 at r=4) while a smooth
          ! >= 3 cell profile recovers theory, and the finite-width
          ! residual grows linearly with width - so 3 smooth cells,
          ! never wider (Doc/shock_grid_resolution_notes.md).

          ! NO rate rescaling: the zone sum is already the compression rate
          ! performed per unit time (= Vshock*ln(r) per interval), and a cell
          ! swept by the moving stencil accumulates Sum/Vshock = ln(r)
          ! exactly - conservation requires depositing the sum unchanged.
          ! The historical factor 0.5*(iShock-iShockOld) confused this rate
          ! with an amount needing a crossing-time conversion, delivering
          ! r_eff = r**(dShock/2): HALF the log-compression at dShock=1
          ! (measured: slope -4.83 vs -3.93 on the same w10 data; cutoff
          ! 6 MeV vs 100 MeV at t=1200s). See Doc/timestep_notes.md.

          do iLine = 1, nLine

               if(.not.Used_B(iLine)) CYCLE
               ! If shock does not move - do not sharpen
               if(iShock_IB(Shock_, iLine).eq.iShock_IB(ShockOld_, iLine)) CYCLE

               dLogRhoSum = sum(dLogRho_II(iShock_IB(ShockDown_, iLine):iShock_IB(ShockUp_, iLine), iLine))

               dLogRho_II(iShock_IB(ShockDown_, iLine):iShock_IB(ShockUp_, iLine), iLine) = dLogRhoThreshold
               dLogRho_II(iShock_IB(Shock_, iLine) - 1, iLine) = dLogRhoSum * 0.25
               dLogRho_II(iShock_IB(Shock_, iLine),     iLine) = dLogRhoSum * 0.50
               dLogRho_II(iShock_IB(Shock_, iLine) + 1, iLine) = dLogRhoSum * 0.25

          end do

     end subroutine sharpen_shock
     !============================================================================
     subroutine get_shock_location

          ! find location of a shock wave on a given line (line)
          ! shock front is assumed to be location of max log(Rho/RhoOld)
          use PT_ModConst,       ONLY: cRadToDeg, cRsun
          use ModCoordTransform, ONLY: xyz_to_rlonlat
          use PT_ModGrid,        ONLY: R_, Rho_, Wave2_, iLineAll0, ROld_
          use PT_ModTime,        ONLY: PTTime, DataInputTime

          ! Do not search too close to the Sun
          real, parameter :: RShockMin = 1.2  ! *RSun
          integer         :: iShockMin
          ! Do not search too close to the heliosphere boundary
          integer :: iShockMax
          ! Misc
          integer :: iShockCandidate
          ! Loop variables
          integer :: iLine, iEnd, iShockForward, i
          real :: Rshock, RshockOld

          character(len=*), parameter:: NameSub = 'get_shock_location'
          !--------------------------------------------------------------------------

          do iLine = 1, nLine
               ! go line by line and get the shock location if active
               if(.not.Used_B(iLine)) CYCLE
               
               ! Number of the active particles on the line
               iEnd = MaxLagr(iLine)

               iShockMin = iShock_IB(ShockOld_, iLine)

               iShockMax = iEnd - 1
 
               ! get the forward grid index for iShockCandidate
               if (any(State_VIB(R_,iShockMin:iShockMax,iLine) > RShockMin .and. &
                    dLogRho_II(iShockMin:iShockMax, iLine) > dLogRhoThreshold)) then
                    iShockForward = maxloc( &
                         dLogRho_II(iShockMin:iShockMax, iLine), DIM=1, MASK= &
                         State_VIB(R_,iShockMin:iShockMax,iLine) > RShockMin .and. &
                         dLogRho_II(iShockMin:iShockMax, iLine) > dLogRhoThreshold, BACK = .true.)
               else
                    iShockForward = 0
               end if
               iShockCandidate = iShockMin - 1 + iShockForward

               ! if shock has moved forward
               if(iShockCandidate >= iShockMin) then
                    iShock_IB(Shock_, iLine) = iShockCandidate
                    
                    ! Determine upstream extent of shock
                    i = 0
                    do while(dLogRho_II(iShockCandidate + i, iLine).gt.dLogRhoThreshold.and. &
                             (iShockCandidate+i).lt.iEnd.and.i.le.nSearchMax)
                         i = i + 1
                    end do
                    iShock_IB(ShockUp_, iLine) = iShockCandidate + i + 1
                    
                    ! Determine downstream extent of shock
                    i = 0
                    do while(dLogRho_II(iShockCandidate - i, iLine).gt.dLogRhoThreshold.and. &
                            (iShockCandidate-i).gt.MinLagr(iLine).and.i.le.nSearchMax) 
                         i = i + 1
                    end do
                    iShock_IB(ShockDown_, iLine) = iShockCandidate - i - 1
               else
                    ! no change to shock location - shock does not exist
                    ! Set shock location to old shock location
                    ! Set width equal to 0
                    iShock_IB(Shock_, iLine)     = iShock_IB(ShockOld_, iLine)
                    iShock_IB(ShockUp_, iLine)   = iShock_IB(ShockOld_, iLine)
                    iShock_IB(ShockDown_, iLine) = iShock_IB(ShockOld_, iLine)
               end if
               
               ! check_line_ishock: update Used_B(iLine)
               call check_line_ishock(iLine)
               if(.not.Used_B(iLine)) CYCLE
          end do

     end subroutine get_shock_location
     !============================================================================
     subroutine get_test_shock
          use PT_ModGrid, only: U_, D_, DensityGradient

          integer :: iLine, iEnd, i, iShock
          real :: divU(nVertexMax)


          do iLine = 1, nLine
               divU = 0.0

               ! D_ and U_ are not defined at MaxLagr
               iEnd = MaxLagr(iLine) - 1
               divU(MinLagr(iLine):iEnd-1) = (State_VIB(U_, MinLagr(iLine)+1:iEnd, iLine) - &
                                              State_VIB(U_, MinLagr(iLine):iEnd-1, iLine)) / &
                                              State_VIB(D_, MinLagr(iLine):iEnd-1, iLine)

               iShock = minloc(divU(MinLagr(iLine):iEnd-1), DIM = 1) + MinLagr(iLine) - 1

               ! slighly dubious here, calculating divU in this manner for the first timestep isn't perfect
               ! location of shock from divU is actually one lagr coordinate ahead - investigate!
               iShock_IB(Shock_, iLine) = iShock - 1

               i = 0
               do while(divU(iShock + i).lt.-dLogRhoThreshold.and.(iShock+i).lt.iEnd.and.i.le.nSearchMax)
                    i = i + 1
               end do
               iShock_IB(ShockUp_, iLine) = iShock + i 
               
               i = 0
               do while(divU(iShock - i).lt.-dLogRhoThreshold.and.(iShock-i).gt.MinLagr(iLine).and.i.le.nSearchMax) 
                    i = i + 1
               end do
               iShock_IB(ShockDown_, iLine) = iShock - i - 2 

               ! Set density gradient to zero. Need to do this because the shock exists at t = 0, 
               ! skewing the calculation
               DensityGradient(:, iLine) = 0.0

          end do

     end subroutine get_test_shock
     !============================================================================
end module PT_ModShock
