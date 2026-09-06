!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModDrift

  ! Solve the drift term in the Parker equation
  use ModMpi
#ifdef OPENACC
  use ModUtilities, ONLY: norm2
#endif
  use ModCosmicRay, ONLY: local_interstellar_spectrum
  use ModPoissonBracket, ONLY: explicit
  use ModUtilities, ONLY: CON_stop
  use SP_ModBc, ONLY: SpectralIndex, TypeLowerEndBc, TypeUpperEndBc
  use SP_ModDistribution, ONLY: Distribution_CB, Momentum_G, SpeedSi_G, &
       MomentumInjSi, Background_I
  use SP_ModPerpDiffusion, ONLY: nRPerp, nThetaPerp, nPhiPerp, &
       UseDiffusionPerp, RPerp_C, RPerp_F, ThetaPerp_F, XyzPerp_CB, Volume_CB
  use SP_ModGrid, ONLY: nP, nMu
  use SP_ModProc, ONLY: iProc, nProc, iComm, iError
  use SP_ModSize, ONLY: nDim
  use SP_ModUnit, ONLY: Si2Io_V, Io2Si_V, UnitX_, UnitEnergy_

  implicit none

  PRIVATE ! Except

  SAVE

  ! Public members:
  public :: read_param, init, iterate_drift

  ! Whether to include the drift term
  logical, public :: UseDrift = .false.
  real, allocatable, dimension(:,:,:,:) :: Bxyz12_IN, Bxyz23_IN, Bxyz13_IN

contains
  !============================================================================
  subroutine read_param(NameCommand)
    use ModReadParam, ONLY: read_var
    character(len=*), intent(in) :: NameCommand ! From PARAM.in
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case('#DRIFT')
       call read_var('UseDrift', UseDrift)
    case default
       call CON_stop('SP:'//NameSub//': unknown command '//NameCommand)
    end select
  end subroutine read_param
  !============================================================================
  subroutine init
    ! Initialize the arrays within the drift module
    character(len=*), parameter:: NameSub = 'init'
    !--------------------------------------------------------------------------
    allocate(Bxyz12_IN(nDim, 0:nPhiPerp+1, 0:nThetaPerp+1, 1:nRPerp))
    allocate(Bxyz23_IN(nDim, 1:nPhiPerp, 0:nThetaPerp+1, 0:nRPerp+1))
    allocate(Bxyz13_IN(nDim, 0:nPhiPerp+1, 1:nThetaPerp, 0:nRPerp+1))
  end subroutine init
  !============================================================================
  subroutine iterate_drift(CflIn, IsSteadyState)
    ! Calculate the source term by the drift term, in the iterative mode

    use ModConst, ONLY: cElectronCharge
    use SP_ModTriangulate, ONLY: reset_intersect_surf, intersect_surf, &
         build_trmesh, interpolate_trmesh, nTriMesh_I
    use ModCoordTransform, ONLY: xyz_to_rlonlat
    real, intent(in) :: CflIn
    logical, intent(in) :: IsSteadyState
    integer :: iPhi, iTheta, iR, iP, iMu, iPE
    real :: Br, Btheta, Bphi
    ! {f; ~H_r}_{theta, phi}, phi=1, theta=2
    real :: Hamiltonian12_N(-1:nPhiPerp+1, -1:nThetaPerp+1, 1:nRPerp)
    ! {f; ~H_phi}_{r, theta}, theta=2, r=3
    real :: Hamiltonian23_N(1:nPhiPerp, -1:nThetaPerp+1, -1:nRPerp+1)
    ! {f; ~H_theta}_{phi, r}, phi=1, r=3
    real :: Hamiltonian13_N(-1:nPhiPerp+1, 1:nThetaPerp, -1:nRPerp+1)
    ! The Distribution function in 5D
    real :: DistrPerp_5D(0:nP+1, 1:nMu, nPhiPerp, nThetaPerp, nRPerp)
    ! The lower and upper BCs
    real :: LowerEndBc_I(1:nP), UpperEndBc_I(1:nP)
    ! Extended array for distribution function
    real :: VDF_G(0:nP+1, 1:nMu, -1:nPhiPerp+2, -1:nThetaPerp+2, -1:nRPerp+2)
    ! Advection term
    real :: Source_C(nP, nMu, nPhiPerp, nThetaPerp, nRPerp)
    ! Time step
    real :: Dt_C(nP, nMu, nPhiPerp, nThetaPerp, nRPerp)
    !--------------------------------------------------------------------------

    if(.not. UseDiffusionPerp) then
       ! In the 1st call: we set up the skeleton
       ! Step 1: field lines => intersection points on multiple uniform layers
       ! here, iProc is for field lines, not for sub-slices/layers.
       ! In fact, this step of getting the intersection points is needed ONLY
       ! the 1st time, since MHData would be changed every coupling interval.
       call reset_intersect_surf(nRPerp)
       do iR = 1, nRPerp
          call intersect_surf(RPerp_C(iR)*Si2Io_V(UnitX_), 0, iR)
       end do
       if(nProc > 1) then
          do iPE = 0, nProc-1
             call MPI_BCAST(nTriMesh_I, nRPerp, &
                  MPI_INTEGER, iPE, iComm, iError)
          end do
       end if
    end if

    ! Step 2: Triangulation skeleton => Interpolate the VDF
    if(iProc == 0) then
       do iR = 1, nRPerp
          call build_trmesh(iR)
          do iTheta = 1, nThetaPerp
             do iPhi = 1, nPhiPerp
                ! Get VDF at the cell center in the uniform grid
                call interpolate_trmesh(XyzPerp_CB(:, &
                     iPhi,iTheta,iR)*Si2Io_V(UnitX_), &
                     iRIn=iR, DistrInterp_II=DistrPerp_5D(:,:,iPhi,iTheta,iR))
             end do
          end do
       end do
    end if
    ! Broadcast Dperp coefficient to all processes
    if(nProc > 1) then
       do iPE = 0, nProc-1
          call MPI_BCAST(DistrPerp_5D, nPhiPerp*nThetaPerp*nRPerp* &
               (nP+2)*nMu, MPI_REAL, iPE, iComm, iError)
       end do
    end if
    ! Get the VDF_G with extended grids with ghost cells for the drift term
    call set_VDF_uniform

    Source_C = 0.0; Dt_C = 0.0
    ! Step 3: Calculate the source term by the drift term
    do iP = 1, nP
       call calc_hamiltonian_12(iP)
       call calc_hamiltonian_23(iP)
       call calc_hamiltonian_13(iP)
       do iMu = 1, nMu
          call explicit(nPhiPerp, nThetaPerp, nRPerp, &
               VDF_G(iP,iMu,:,:,:), Volume_CB, Source_C(iP,iMu,:,:,:), &
               Hamiltonian12_N=Hamiltonian12_N, &
               Hamiltonian13_N=Hamiltonian13_N, &
               Hamiltonian23_N=Hamiltonian23_N, &
               CFLIn=CflIn, IsSteadyState=IsSteadyState, &
               DtOut_C=Dt_C(iP,iMu,:,:,:))
       end do
    end do

  contains
    !==========================================================================
    subroutine set_VDF_uniform
      ! We need the VDF on extended grids with 2 layers of ghost cells in the
      ! uniform grid to solve the drift term, like setting up VDF_G along lines

      !------------------------------------------------------------------------
      VDF_G(0:nP+1, 1:nMu, 1:nPhiPerp, 1:nThetaPerp, 1:nRPerp) = &
           DistrPerp_5D(0:nP+1, 1:nMu, 1:nPhiPerp, 1:nThetaPerp, 1:nRPerp)

      do iTheta = 1, nThetaPerp
         do iPhi = 1, nPhiPerp
            call set_VDF_lowerBC_uniform(iTheta, iPhi)
            VDF_G(1:nP, 1:nMu, iPhi, iTheta, 0) = spread( &
                 max(LowerEndBc_I, Background_I(1:nP)), DIM=2, NCOPIES=nMu)
            VDF_G(0, 1:nMu, iPhi, iTheta, 0) = max( &
                 VDF_G(0, 1:nMu, iPhi, iTheta, 1), Background_I(0))
            VDF_G(nP+1, 1:nMu, iPhi, iTheta, 0) = max( &
                 VDF_G(nP+1, 1:nMu, iPhi, iTheta, 1), Background_I(nP+1))
            call set_VDF_upperBC_uniform(iTheta, iPhi)
            VDF_G(1:nP, 1:nMu, iPhi, iTheta, nRPerp+1) = spread( &
                 max(UpperEndBc_I, Background_I(1:nP)), DIM=2, NCOPIES=nMu)
            VDF_G(0, 1:nMu, iPhi, iTheta, nRPerp+1) = max( &
                 VDF_G(0, 1:nMu, iPhi, iTheta, nRPerp), Background_I(0))
            VDF_G(nP+1, 1:nMu, iPhi, iTheta, nRPerp+1) = max( &
                 VDF_G(nP+1, 1:nMu, iPhi, iTheta, nRPerp), Background_I(nP+1))
         end do
      end do

      ! Add a second layer of the ghost cells along R:
      VDF_G(:, :, :, :,       -1) = VDF_G(:, :, :, :,        0)
      VDF_G(:, :, :, :, nRPerp+2) = VDF_G(:, :, :, :, nRPerp+1)
      ! Add two layers of the ghost cells along Theta:
      VDF_G(:, :, :, -1:0, :)  = VDF_G(:, :, :, nThetaPerp-1:nThetaPerp, :)
      VDF_G(:, :, :, nThetaPerp+1:nThetaPerp+2, :) = VDF_G(:, :, :, 1:2, :)
      ! Add two layers of the ghost cells along Phi:
      VDF_G(:, :, -1:0, :, :)  = VDF_G(:, :, nPhiPerp-1:nPhiPerp, :, :)
      VDF_G(:, :, nPhiPerp+1:nPhiPerp+2, :, :) = VDF_G(:, :, 1:2, :, :)

    end subroutine set_VDF_uniform
    !==========================================================================
    subroutine set_VDF_lowerBC_uniform(iTheta, iPhi)
      integer, intent(in) :: iTheta, iPhi
      character(len=*), parameter:: NameSub = 'set_VDF_lowerBC_uniform'
      !------------------------------------------------------------------------
      select case(trim(TypeLowerEndBc))
      case('inject')
         LowerEndBc_I = DistrPerp_5D(0, nMu, iPhi, iTheta, 1) &
              /Momentum_G(1:nP)**SpectralIndex
      case('float')
         LowerEndBc_I = DistrPerp_5D(1:nP, nMu, iPhi, iTheta, 1)
      case('escape')
         LowerEndBc_I = Background_I(1:nP)
      case default
         call CON_stop(NameSub//&
              ': Unknown type of lower end BC '//TypeLowerEndBc)
      end select
    end subroutine set_VDF_lowerBC_uniform
    !==========================================================================
    subroutine set_VDF_upperBC_uniform(iTheta, iPhi)
      integer, intent(in) :: iTheta, iPhi
      character(len=*), parameter:: NameSub = 'set_VDF_upperBC_uniform'
      !------------------------------------------------------------------------
      select case(trim(TypeUpperEndBc))
      case('float')
         UpperEndBc_I = DistrPerp_5D(1:nP, nMu, iPhi, iTheta, nRPerp)
      case('escape')
         UpperEndBc_I = Background_I(1:nP)
      case('lism')
         call local_interstellar_spectrum(     &
              nP = nP,                         & ! # of grid points
              MomentumSi_I = Momentum_G(1:nP)* &
              MomentumInjSi,                   & ! momentum (SI) in grid points
              XyzSi_D = XyzPerp_CB(:, iPhi, iTheta, nRPerp), &  ! Coords
              DistTimesP2Si_I = UpperEndBc_I)
         ! Now, in UpperEndBc_I there is Distribution[Si]*Momentum[Si]**2
         ! Our Momentum_G is MomentumSi_I/MomentumInjSi
         ! So, UpperEndBc_I is Distribution[Si]*MomentumInjSi**2*Momentum_G**2
         ! The distribution used in our code is
         ! Distribution[Si]*MomentumInjSi**2*Io2Si_V(UnitEnergy_)
         UpperEndBc_I = (UpperEndBc_I/Momentum_G(1:nP)**2)*Io2Si_V(UnitEnergy_)
      end select
    end subroutine set_VDF_upperBC_uniform
    !==========================================================================
    subroutine calc_hamiltonian_23(iP)
      ! Calculate {f; ~H_phi}_{r, theta}, theta=2, r=3; H23 = -(~H_phi)
      integer, intent(in) :: iP
      !------------------------------------------------------------------------
      ! Loop over all grid points
      do iR = 0, nRPerp+1
         do iTheta = 0, nThetaPerp+1
            do iPhi = 1, nPhiPerp
               ! Convert to spherical components
               call xyz_to_rlonlat(Bxyz23_IN(:,iPhi,iTheta,iR), &
                    Br, Btheta, Bphi)
               ! Hamiltonian23_N = -[r*sin(theta) * p*v/(3*q) * Bphi/B**2]
               Hamiltonian23_N(iPhi,iTheta,iR) = &
                    -RPerp_F(iR)*sin(ThetaPerp_F(iTheta))* &
                    Momentum_G(iP)*SpeedSi_G(iP)/(3.0*cElectronCharge)* &
                    Bphi/norm2(Bxyz23_IN(:,iPhi,iTheta,iR))
            end do
         end do
      end do
    end subroutine calc_hamiltonian_23
    !==========================================================================
    subroutine calc_hamiltonian_12(iP)
      ! Calculate {f; ~H_r}_{theta, phi}, phi=1, theta=2; H12 = -(~H_r)
      integer, intent(in) :: iP
      !------------------------------------------------------------------------
      ! Loop over all grid points
      do iR = 1, nRPerp
         do iTheta = 0, nThetaPerp+1
            do iPhi = 0, nPhiPerp+1
               ! Convert to spherical components
               call xyz_to_rlonlat(Bxyz12_IN(:,iPhi,iTheta,iR), &
                    Br, Btheta, Bphi)
               ! Hamiltonian12_N = -[p*v/(3*q) * Br/B**2]
               Hamiltonian12_N(iPhi,iTheta,iR) = &
                    -Momentum_G(iP)*SpeedSi_G(iP)/(3.0*cElectronCharge)* &
                    Br/norm2(Bxyz12_IN(:,iPhi,iTheta,iR))
            end do
         end do
      end do
    end subroutine calc_hamiltonian_12
    !==========================================================================
    subroutine calc_hamiltonian_13(iP)
      ! Calculate {f; ~H_theta}_{phi, r}, phi=1, r=3; H13 = ~H_theta
      integer, intent(in) :: iP
      !------------------------------------------------------------------------
      ! Loop over all grid points
      do iR = 0, nRPerp+1
         do iTheta = 1, nThetaPerp
            do iPhi = 0, nPhiPerp+1
               ! Convert to spherical components
               call xyz_to_rlonlat(Bxyz13_IN(:,iPhi,iTheta,iR), &
                    Br, Btheta, Bphi)
               ! Hamiltonian13_N = r* p*v/(3*q) * Btheta/B**2
               Hamiltonian13_N(iPhi,iTheta,iR) = RPerp_F(iR)* &
                    Momentum_G(iP)*SpeedSi_G(iP)/(3.0*cElectronCharge)* &
                    Btheta/norm2(Bxyz13_IN(:,iPhi,iTheta,iR))
            end do
         end do
      end do
    end subroutine calc_hamiltonian_13
    !==========================================================================
  end subroutine iterate_drift
  !============================================================================
end module SP_ModDrift
!==============================================================================
