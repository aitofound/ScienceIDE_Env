module GIMME_mag_input

  use GIMME_iono_grid, ONLY: nTheta,nPhi,Theta_G,Phi_G, UseFullSphere

  private
  real, public, allocatable :: Jr_C(:,:)

  ! integrated along the field line quantities
  real, public, allocatable:: IntInvB_C(:,:),IntRho_C(:,:),IntP_C(:,:)

  ! values from inner mag
  real, public, allocatable :: InnerMagJr_C(:,:),InnerMagEflux_C(:,:),&
       InnerMagEmean_C(:,:)

  ! for imposing boundary condition above a given theta
  integer, public, allocatable :: iThetaBC_I(:)
  real,    public, allocatable :: PotBC_C(:,:)

  ! are we coupling CIMI
  logical, public :: DoCoupleCimi = .false.

  ! OuterMagInvBAll = Buffer_IIV(:,:,2)
  ! OuterMagRhoAll  = Buffer_IIV(:,:,3)
  ! OuterMagPAll
  public :: init_mag_input
  public :: test_input
contains
  !============================================================================
  subroutine  init_mag_input
    !--------------------------------------------------------------------------
    if(.not.allocated(Jr_C)) allocate(Jr_C(nTheta,nPhi))
    if(.not.allocated(InnerMagJr_C)) allocate(InnerMagJr_C(nTheta,nPhi))
    if(.not.allocated(InnerMagEmean_C)) allocate(InnerMagEmean_C(nTheta,nPhi))
    if(.not.allocated(InnerMagEflux_C)) allocate(InnerMagEflux_C(nTheta,nPhi))
    if(.not.allocated(IntInvB_C)) allocate(IntInvB_C(nTheta,nPhi))
    if(.not.allocated(IntRho_C)) allocate(IntRho_C(nTheta,nPhi))
    if(.not.allocated(IntP_C)) allocate(IntP_C(nTheta,nPhi))

    if(.not.allocated(iThetaBC_I)) allocate(iThetaBC_I(nPhi))
    if(.not.allocated(PotBC_C)) allocate(PotBC_C(nTheta,nPhi))

  end subroutine init_mag_input
  !============================================================================
  subroutine  test_input

    integer :: iPhi, iTheta
    !--------------------------------------------------------------------------
    Jr_C=0.0
    do iPhi=1,nPhi
       do iTheta=1,nTheta
          if(iTheta>5 .and. iTheta<10) then
!             if(Phi_G(iPhi)<0.5*3.14 .or. Phi_G(iPhi)>2.*3.14*.75) then
             if (iPhi>1+7 .and. iPhi<nPhi/2-7) then
                Jr_C(iTheta,iPhi) = -0.22e-6
             elseif(iPhi>nPhi/2+7 .and. iPhi<nPhi-7) then
                Jr_C(iTheta,iPhi) = 0.22e-6
             endif
!             endif
          end if
          if(UseFullSphere .and. iTheta<nTheta-5 .and. iTheta>nTheta-10) then
!             if(Phi_G(iPhi)<0.5*3.14 .or. Phi_G(iPhi)>2.*3.14*.75) then
             if (iPhi>1+7 .and. iPhi<nPhi/2-7) then
                Jr_C(iTheta,iPhi) = -0.22e-6
             elseif(iPhi>nPhi/2+7 .and. iPhi<nPhi-7) then
                Jr_C(iTheta,iPhi) = 0.22e-6
             endif
!             endif
          end if
       enddo
    enddo

  end subroutine test_input
  !============================================================================
end module GIMME_Mag_Input
!==============================================================================
