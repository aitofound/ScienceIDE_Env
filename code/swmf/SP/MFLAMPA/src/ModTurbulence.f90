!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModTurbulence

  use ModConst
  use SP_ModDistribution, ONLY: SpeedSi_G, Momentum_G, MomentumInjSi
  use SP_ModGrid, ONLY: nP ! iPTest, iParticleTest

  implicit none

  SAVE

  private

  public :: init, finalize, UseTurbulentSpectrum, set_dxx, read_param, Dxx
  interface Dxx
     module procedure Dxx_s    ! single iX and iP input
     module procedure Dxx_mat  ! array in X and P phase spaces
  end interface Dxx

  logical:: UseTurbulentSpectrum = .false.
  integer, parameter :: nK = nP
  real   :: dLogK

  real, allocatable :: Gamma_I(:,:)
  real, allocatable :: IPlusSi_IX(:,:), IMinusSi_IX(:,:), ICSi_X(:)
  real, allocatable :: vAlfvenSi_I(:)
  real, allocatable :: kOverBSi_I(:)
  real, allocatable :: kSi_I(:)

  ! Rate of advection in k space, neglecting
  ! a spacial advection with the Alfven speed
  real, allocatable:: DispersionA_I(:)

  ! Rate of advection in k space, for I_+/I_- wave
  real, allocatable:: DispersionPlus_I(:)
  real, allocatable:: DispersionMinus_I(:)

  ! This is the ratio of densities powered 3/2
  real, allocatable:: RhoCompression_I(:)

  !-----------------Grid in the momentum space---------------------------------
  ! iP     0     1                         nP   nP+1
  !        |     |    ....                 |     |
  ! P     P_inj P_inj*exp(dLogP)          P_Max P_Max*exp(dLogP)
  !--------|--------Grid in k-space--------|-----|-----------------------------
  ! K/B   KMax                            KMin
  ! ik     0     1                         nP   nP+1
  !----------------------------------------------------------------------------

  real,allocatable,private:: AK_II(:,:)
  real,allocatable,private:: BK_II(:,:)
  real,allocatable,private:: CFL_I(:)
  integer,allocatable::      CorrectionMode_X(:)

  ! DPerp coefficient interpolated to the points at the spherical surface
  real, public, allocatable :: DPerp_CB(:,:,:,:)   ! Along lines
  real, public, allocatable :: DPerp_IIBI(:,:,:,:) ! Uniform grid
contains
  !============================================================================
  subroutine read_param(NameCommand)

    use ModReadParam, ONLY: read_var
    character(len=*), intent(in):: NameCommand
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case('#TURBULENTSPECTRUM')
       call read_var('UseTurbulentSpectrum', UseTurbulentSpectrum)
    case default
       call CON_stop(NameSub//' Unknown command '//NameCommand)
    end select

  end subroutine read_param
  !============================================================================
  subroutine init
    ! Allocate all the allocatable vars

    use SP_ModSize, ONLY: nVertexMax
    use SP_ModProc, ONLY: iProc
    character(len=*), parameter:: NameSub = 'init'
    !--------------------------------------------------------------------------

    allocate(IPlusSi_IX(0:nP+1,1:nVertexMax), IMinusSi_IX(0:nP+1,1:nVertexMax))
    allocate(kOverBSi_I(0:nP+1), kSi_I(0:nP+1))
    allocate(ICSi_X(1:nVertexMax), CorrectionMode_X(1:nVertexMax))
    allocate(vAlfvenSi_I(1:nVertexMax))
    allocate(DispersionA_I(1:nVertexMax))

    allocate(RhoCompression_I(1:nVertexMax))

    allocate(DispersionPlus_I(1:nVertexMax))
    allocate(DispersionMinus_I(1:nVertexMax))
    allocate(CFL_I(1:nVertexMax))

    allocate(AK_II(nP,nVertexMax), BK_II(nP,nVertexMax))

  end subroutine init
  !============================================================================
  subroutine finalize

    character(len=*), parameter:: NameSub = 'finalize'
    !--------------------------------------------------------------------------
    deallocate(IPlusSi_IX, IMinusSi_IX)
    deallocate(kOverBSi_I, kSi_I)
    deallocate(ICSi_X, CorrectionMode_X)
    deallocate(vAlfvenSi_I)
    deallocate(DispersionA_I)

    deallocate(RhoCompression_I)

    deallocate(DispersionPlus_I)
    deallocate(DispersionMinus_I)
    deallocate(CFL_I)

    deallocate(AK_II, BK_II)

  end subroutine finalize
  !============================================================================
  subroutine init_spectrum(iEnd, XyzSi_DI, BSi_I, MomentumSi_I, dLogP, &
       iShock, CoefInj, AlfvenMach)
    !==============Initial spectrum of turbulence=============================
    ! We recover the initial spectrum of turbulence from the spatial
    ! distribution of the diffusion coefficient and its dependence on the
    ! particle energy.

    ! the number of active particles on the line
    integer, intent(in) :: iEnd

    ! Coordinates of Lagrangian Meshes in SI unit [m]
    real,dimension(1:3, 1:iEnd),intent(in) :: XyzSi_DI

    ! Magnetic field intensity in SI unit [T]
    real,dimension(1:iEnd),intent(in) :: BSi_I

    ! momentum in SI unit
    real,intent(in)     :: MomentumSi_I(0:nP+1)

    ! delta log p in SI unit
    real,intent(in)     :: dLogP

    ! coef of injection
    real,intent(in)     :: CoefInj

    ! Alfven March number
    real,intent(in)     :: AlfvenMach

    ! shock index
    integer, intent(in) :: iShock

    integer :: iVertex, iK
    real    :: ICOldSi, kSi
    real    :: rSi, rShockSi

    ! k = e*B/p => dlog(k) = - dlog(p)
    !--------------------------------------------------------------------------
    dLogK = dLogP

    kOverBSi_I = cElectronCharge/MomentumSi_I

    rShockSi = sqrt(sum(XyzSi_DI(:,iShock)**2))

    do iVertex = 1, iEnd
       rSi   = sqrt(sum(XyzSi_DI(:,iVertex)**2))
       kSi_I = kOverBSi_I*BSi_I(iVertex)
    end do

  end subroutine init_spectrum
  !============================================================================
  subroutine set_dxx(iEnd, BSi_I)

    integer,intent(in) :: iEnd
    real,intent(in)    :: BSi_I(iEnd)

    integer:: iVertex, iK
    real :: F01, F02, F11, F12
    real :: k0Si, k1Si
    real :: SpectralIndexAtKMax, ISumSi

    logical :: DoTestMe = .false.

    ! The coefficient of a spatial diffusion along the magnetic field is
    ! given by a formula in the SI unit:
    ! D_{xx}=v*B^2/(cPi*cMu)*(AK_I(k_{res})-k_{res}^2*BK_I(k_{res})),

    ! where AK_I=\int_{k_{res}}^\infty{d(\log k)/(k^2*(I_{+}(k) + I_{-}(k) )) }
    ! and   BK_I=\int_{k_{res}}^\infty{d(\log k)/(k^4*(I_{+}(k) + I_{-}(k) )) }

    !-----------------Grid in the momentum space-------------------------------
    ! iP     0     1                         nP   nP+1
    !        |     |    ....                 |     |
    ! P     P_inj P_inj*exp(dLogP)          P_Max P_Max*exp(dLogP)
    !--------|--------Grid in k-space--------|-----|---------------------------
    ! K/B   KMax                            KMin
    ! ik     0     1                         nP   nP+1
    !--------------------------------------------------------------------------

    do iVertex = 1, iEnd
       select case(CorrectionMode_X(iVertex))
       case(1)
          SpectralIndexAtKMax = 5.0/3.0
       case(2)
          SpectralIndexAtKMax = 1.0
       end select

       kSi_I = BSi_I(iVertex)*kOverBSi_I

       ! Initially from kMax
       k0Si = kSi_I(1)

       ! The sum of I_{plus}+I_{minus} at P_max
       ISumSi = IPlusSi_IX(1,iVertex)+IMinusSi_IX(1,iVertex)

       ! The integrand for AK_I, BK_I
       F01 = 1.0/(k0Si**2)/ISumSi
       F02 = 1.0/(k0Si**4)/ISumSi

       ! As the starting values for AK_I and BK_I at the minimum momentum,
       ! solve the integrals from K_{max} up to \infty, assuming the
       ! power law spectrum of turbulence at K>K_{max}
       AK_II(1,iVertex) = F01/(2.0-SpectralIndexAtKMax)
       BK_II(1,iVertex) = F02/(4.0-SpectralIndexAtKMax)

       do iK = 2, nP
          ! We calculate the partial sums for a set of the wave number values.
          ! The integral is taken from KRes up to infinity, so we start from
          ! the maximal wave number and add the contributions from each of
          ! the wave number intervals.

          ! The current value of the wave number
          k1Si = kSi_I(iK)

          ! The sum of I_{plus}+I_{minus} at P
          ISumSi = IPlusSi_IX(iK,iVertex)+IMinusSi_IX(iK,iVertex)

          ! The integrands at the lower value of the wave number
          F11 = 1.0/(k1Si**2)/ ISumSi
          F12 = 1.0/(k1Si**4)/ ISumSi

          ! Calculate the new partial sums
          AK_II(iK,iVertex) = AK_II(iK-1,iVertex) + 0.5*(F01+F11)*dLogK
          BK_II(iK,iVertex) = BK_II(iK-1,iVertex) + 0.5*(F02+F12)*dLogK

          ! current values saved as the initial values for the next step in
          ! the loop
          k0Si = k1Si; F01 = F11; F02 = F12
       end do
    end do

  end subroutine set_dxx
  !============================================================================
  function Dxx_s(iX, iP, BSi) result(Dxx)

    integer,intent(in) :: iX, iP
    real,   intent(in) :: BSi

    real    :: kRSi
    logical :: DoTestMe =.false.
    real    :: Dxx

    ! The coefficient of a spatial diffusion along the magnetic field is
    ! given by a formula in the SI unit:
    ! D_{xx}=v*B^2/(cPi*cMu)*(AK_I(k_{res})-k_{res}^2*BK_I(k_{res})),

    ! where AK_I=\int_{k_{res}}^\infty{d(\log k)/(k^2*(I_{+}(k) + I_{-}(k) )) }
    ! and   BK_I=\int_{k_{res}}^\infty{d(\log k)/(k^4*(I_{+}(k) + I_{-}(k) )) }

    ! The resonant wave number, kr = e*B/p in the SI unit
    !--------------------------------------------------------------------------
    kRSi = cElectronCharge*BSi/(Momentum_G(iP)*MomentumInjSi)

    ! Calculate D_{xx}: KRes-dependent part
    Dxx = BSi**2*SpeedSi_G(iP)/(cMu*cPi)*(AK_II(iP,iX)-BK_II(iP,iX)*kRSi**2)

  end function Dxx_s
  !============================================================================
  function Dxx_mat(nX, BSi_I) result(Dxx_II)

    integer,intent(in) :: nX
    real,   intent(in) :: BSi_I(1:nX)

    real    :: kRSi_II(nX, nP)
    logical :: DoTestMe =.false.
    real    :: Dxx_II(nX, nP)

    ! The coefficient of a spatial diffusion along the magnetic field is
    ! given by a formula in the SI unit:
    ! D_{xx}=v*B^2/(cPi*cMu)*(AK_I(k_{res})-k_{res}^2*BK_I(k_{res})),

    ! where AK_I=\int_{k_{res}}^\infty{d(\log k)/(k^2*(I_{+}(k) + I_{-}(k) )) }
    ! and   BK_I=\int_{k_{res}}^\infty{d(\log k)/(k^4*(I_{+}(k) + I_{-}(k) )) }

    ! The resonant wave number, kr = e*B/p in the SI unit
    !--------------------------------------------------------------------------
    kRSi_II = cElectronCharge*spread(BSi_I, DIM=2, NCOPIES=nP)/spread( &
         Momentum_G(1:nP)*MomentumInjSi, DIM=1, NCOPIES=nX)

    ! Calculate D_{xx}: KRes-dependent part
    Dxx_II = spread(BSi_I**2, DIM=2, NCOPIES=nP)* &
         spread(SpeedSi_G(1:nP), DIM=1, NCOPIES=nX)/ &
         (cMu*cPi)*(AK_II(1:nP,1:nX) - BK_II(1:nP,1:nX)*kRSi_II**2)

  end function Dxx_mat
  !============================================================================
end module SP_ModTurbulence
!==============================================================================
