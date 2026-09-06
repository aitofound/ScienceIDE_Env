module GIMME_conductance

  use GIMME_iono_grid, ONLY: nTheta, nPhi, Theta_G, Phi_G, dTheta, dPhi, &
       convert_lat_lon, IYD, iStartTime_I, Hour_, Minute_, Second_, &
       TimeSimulation

  implicit none
  private

  ! field aligned conductance
  real, public, allocatable :: Sigma0_G(:,:)

  ! Pederson conductance
  real, public, allocatable :: SigmaP_G(:,:)

  ! Hall conductance
  real, public, allocatable :: SigmaH_G(:,:)

  real, public :: UniformSigma0 = 1000.0 ! mho from Ridley et al, 2004
  real, public :: UniformSigmaP = 5.0
  real, public :: UniformSigmaH = 0.0

  ! min value for sigma P and H
  real, public :: SigmaPmin = 2.0
  real, public :: SigmaHmin = 2.0

  ! F107 value for solar conductance
  real :: F107=80.0

  ! character(len=100),public :: TypeConductance = 'Uniform'
  character(len=100), public :: TypeConductance = 'Solar'

  ! coeficients for calculating Matrix
  real, public,allocatable :: A_C(:,:),B_C(:,:),C_C(:,:),D_C(:,:),E_C(:,:)

  public :: init_conductance
  public :: set_conductance
  public :: calc_coefficients

contains
  !============================================================================
  subroutine init_conductance
    !--------------------------------------------------------------------------
    if (.not.allocated(Sigma0_G)) allocate(Sigma0_G(0:nTheta+1,0:nPhi+1))
    if (.not.allocated(SigmaP_G)) allocate(SigmaP_G(0:nTheta+1,0:nPhi+1))
    if (.not.allocated(SigmaH_G)) allocate(SigmaH_G(0:nTheta+1,0:nPhi+1))

    if (.not.allocated(A_C)) allocate(A_C(nTheta,nPhi))
    if (.not.allocated(B_C)) allocate(B_C(nTheta,nPhi))
    if (.not.allocated(C_C)) allocate(C_C(nTheta,nPhi))
    if (.not.allocated(D_C)) allocate(D_C(nTheta,nPhi))
    if (.not.allocated(E_C)) allocate(E_C(nTheta,nPhi))

    ! initalize values to zero
    Sigma0_G=0.0
    SigmaP_G=0.0
    SigmaH_G=0.0
  end subroutine init_conductance
  !============================================================================
  subroutine set_conductance
    !--------------------------------------------------------------------------

    select case (TypeConductance)
    case('Uniform')
       Sigma0_G=UniformSigma0
       SigmaP_G=UniformSigmaP
       SigmaH_G=UniformSigmaH
    case('Solar')
       call solar_conductance
       Sigma0_G=UniformSigma0
    case('Precip')
       write(*,*) 'not yet implemented'
       stop
    case('All')
       write(*,*) 'not yet implemented'
       stop
    end select

  end subroutine set_conductance
  !============================================================================

  ! To solve jr = laplacian_perp Pot you can rewrite this as
  ! Ri^2sin2Theta*jr = alpha*d2Pot/dTheta2 + beta*d2Pot/dPhi2 + gamma*dPot/dTheta
  !      + zeta*dPot/dPhi
  ! where alpha beta gamma and zeta can be found from Ridley et al 2004
  ! Those coefficients and be use to determin the A,B,C,D,E coef of the
  ! discretized version
  !  Ri^2sin2Theta*jr_ij =
  !              A_ij Pot_i+1,j + B_ij Pot_ij + C_ij Pot_i-1,j + D_ij Pot_i,j+1
  !              + E_ij Pot_i,j-1
  ! These are the coefficients that let us calculate the matrix A such that
  ! A * Pot =  Ri^2sin2Theta*jr

  ! This subroutine calculates those coeficients
  subroutine calc_coefficients
    integer :: iPhi,iTheta
    ! angle between rhat and bhat
    real,allocatable :: sinEpsilon_G(:),cosEpsilon_G(:),Cepsilon_G(:,:), &
         InvCepsilon_G(:,:)
    real,allocatable :: sinTheta_G(:),sin2Theta_G(:),cosTheta_G(:),cotTheta_G(:)

    real :: ThetaDeriv1, ThetaDeriv2, PhiDeriv1, PhiDeriv2
    ! first coef
    real :: alpha, beta, gamma, zeta
    !--------------------------------------------------------------------------
    ! allocate variables
    allocate(sinEpsilon_G(0:nTheta+1),cosEpsilon_G(0:nTheta+1),&
         Cepsilon_G(0:nTheta+1,0:nPhi+1),InvCepsilon_G(0:nTheta+1,0:nPhi+1))
    allocate(sinTheta_G(0:nTheta+1),sin2Theta_G(0:nTheta+1),&
         cosTheta_G(0:nTheta+1),cotTheta_G(0:nTheta+1))
    ! loop once to set key trig values
    do iPhi=0,nPhi+1
       do iTheta=0,nTheta+1
          ! set trig values
          sinTheta_G(iTheta)  = sin(Theta_G(iTheta))
          sin2Theta_G(iTheta) = sinTheta_G(iTheta)**2
          cosTheta_G(iTheta)  = cos(Theta_G(iTheta))
          cotTheta_G(iTheta)  = cosTheta_G(iTheta)/sinTheta_G(iTheta)

          ! find Epsilon and trig values of it
          cosEpsilon_G(iTheta) = -2.0*cosTheta_G(iTheta)&
               /sqrt(1.0+3.0*cosTheta_G(iTheta)**2.0)
          sinEpsilon_G(iTheta) = sinTheta_G(iTheta)&
               /sqrt(1.0+3.0*cosTheta_G(iTheta)**2.0)
          Cepsilon_G(iTheta,iPhi) = Sigma0_G(iTheta,iPhi)*cosTheta_G(iTheta)**2+&
               SigmaP_G(iTheta,iPhi)*sinTheta_G(iTheta)**2
          InvCepsilon_G(iTheta,iPhi) = 1.0/Cepsilon_G(iTheta,iPhi)
       enddo
    enddo

    ! loop again to get important derivatives (central diff)
    ! and set coef
    do iPhi=1,nPhi
       do iTheta=1,nTheta
          ThetaDeriv1 = &
               (InvCepsilon_G(iTheta+1,iPhi)*Sigma0_G(iTheta+1,iPhi)&
               *SigmaP_G(iTheta+1,iPhi) &
               - InvCepsilon_G(iTheta-1,iPhi)*Sigma0_G(iTheta-1,iPhi)&
               *SigmaP_G(iTheta-1,iPhi))/dTheta
          PhiDeriv1 = &
               (InvCepsilon_G(iTheta,iPhi+1)*Sigma0_G(iTheta,iPhi+1)&
               *SigmaH_G(iTheta,iPhi+1) *cosEpsilon_G(iTheta) &
               -InvCepsilon_G(iTheta,iPhi-1)*Sigma0_G(iTheta,iPhi-1)&
               *SigmaH_G(iTheta,iPhi-1) *cosEpsilon_G(iTheta))/dPhi
          ThetaDeriv2 = &
               (InvCepsilon_G(iTheta+1,iPhi)*Sigma0_G(iTheta+1,iPhi)&
               *SigmaH_G(iTheta+1,iPhi)&
               *(-cosEpsilon_G(iTheta+1))/sinTheta_G(iTheta+1)&
               -InvCepsilon_G(iTheta-1,iPhi)*Sigma0_G(iTheta-1,iPhi)&
               *SigmaH_G(iTheta-1,iPhi)&
               *(-(cosEpsilon_G(iTheta-1)))/sinTheta_G(iTheta-1))/dTheta
          PhiDeriv2 = &
               (SigmaP_G(iTheta,iPhi+1)&
               +InvCepsilon_G(iTheta,iPhi+1)*SigmaH_G(iTheta,iPhi+1)**2&
               *sinEpsilon_G(iTheta)**2 &
               -SigmaP_G(iTheta,iPhi-1)&
               -InvCepsilon_G(iTheta,iPhi-1)*SigmaH_G(iTheta,iPhi-1)**2&
               *sinEpsilon_G(iTheta)**2)/dPhi

          ! now set coeficients
          alpha =&
               Sigma0_G(iTheta,iPhi)*SigmaP_G(iTheta,iPhi)*sin2Theta_G(iTheta)&
               *InvCepsilon_G(iTheta,iPhi)
          beta  =&
               SigmaP_G(iTheta,iPhi) + &
               SigmaH_G(iTheta,iPhi)**2*sinEpsilon_G(iTheta)**2&
               *InvCepsilon_G(iTheta,iPhi)
          gamma =sin2Theta_G(iTheta)*&
               (ThetaDeriv1+cotTheta_G(iTheta)*Sigma0_G(iTheta,iPhi)&
               *SigmaP_G(iTheta,iPhi)*InvCepsilon_G(iTheta,iPhi))&
               + PhiDeriv1*sinTheta_G(iTheta)
          zeta  =&
               sin2Theta_G(iTheta)*ThetaDeriv2+PhiDeriv2-&
               sinTheta_G(iTheta)*cotTheta_G(iTheta)*Sigma0_G(iTheta,iPhi)&
               *SigmaH_G(iTheta,iPhi)*InvCepsilon_G(iTheta,iPhi)

          ! now calc coef for setting A matrix
          A_C(iTheta,iPhi)=alpha/dTheta**2 + 0.5*gamma/dTheta
          B_C(iTheta,iPhi)=-2.0*alpha/dTheta**2 - 2.0*beta/dPhi**2
          C_C(iTheta,iPhi)=alpha/dTheta**2 - 0.5*gamma/dTheta
          D_C(iTheta,iPhi)=beta/dPhi**2 + 0.5*zeta/dPhi
          E_C(iTheta,iPhi)=beta/dPhi**2 - 0.5*zeta/dPhi

       enddo
    enddo

    ! deallocate to save memory
    deallocate(sinEpsilon_G,cosEpsilon_G,&
         Cepsilon_G,InvCepsilon_G)
    deallocate(sinTheta_G,sin2Theta_G,&
         cosTheta_G,cotTheta_G)

  end subroutine calc_coefficients
  !============================================================================
  subroutine solar_conductance

    use ModNumConst, ONLY: cPi, cTwoPi, cDegToRad, cRadToDeg

    real :: SZA,UT, GeoLat, GeoLon, GmLat, GmLon
    integer :: iTheta, iPhi
    !--------------------------------------------------------------------------
    UT = mod(iStartTime_I(Hour_)*3600.0 + iStartTime_I(Minute_)*60.0 &
         + iStartTime_I(Second_) + TimeSimulation, 24.0*3600.0)

    do iPhi=1,nPhi
       do iTheta=1,nTheta
          ! get the geographic lat and lon
          call convert_lat_lon(Theta_G(iTheta), Phi_G(iPhi), GeoLat, &
               GeoLon, GmLat, GmLon)
          ! get the SZA
          call SOLZEN(IYD, UT, GeoLat, Geolon, SZA)
          SZA=min(SZA*cDegToRad,89.9*cDegToRad)

          ! set conductance from formulas 11 and 12 in Ridley 2004
          !(origionally from Moen and Brekke, 1993)
          SigmaH_G(iTheta,iPhi)= &
               max(F107**0.53*(0.81*cos(SZA)+0.54*sqrt(cos(SZA))), SigmaHmin)
          SigmaP_G(iTheta,iPhi)= &
               max(F107**0.49*(0.34*cos(SZA)+0.93*sqrt(cos(SZA))), SigmaPmin)

       enddo
    enddo
    ! fill Ghost cells
    SigmaH_G(0,:)=SigmaH_G(1,:)
    SigmaH_G(nTheta+1,:)=SigmaH_G(nTheta,:)

    SigmaH_G(:,0)=SigmaH_G(:,nPhi)
    SigmaH_G(:,nPhi+1)=SigmaH_G(:,1)

    SigmaP_G(0,:)=SigmaP_G(1,:)
    SigmaP_G(nTheta+1,:)=SigmaP_G(nTheta,:)

    SigmaP_G(:,0)=SigmaP_G(:,nPhi)
    SigmaP_G(:,nPhi+1)=SigmaP_G(:,1)

  end subroutine solar_conductance
  !============================================================================
  ! Returns Solar Zenith Angle SZA in degrees for specified date in form yyddd,
  ! universal time in seconds, geographic latitude and longitude in degrees.
  !
  ! S.C. Solomon, 9/88
  subroutine solzen (IDATE, UTG, GLAT, GLONG, SZA)

    use ModNumConst, ONLY: cPi

    integer, intent(in) :: IDATE
    real,    intent(in) :: UTG, GLAT, GLONG
    real,    intent(out):: SZA
    real :: RLAT, RLONG, RH, SRASN, GST, COSSZA, SDEC
    !--------------------------------------------------------------------------
    RLAT = GLAT * cPi/180.
    RLONG = GLONG * cPi/180.
    CALL SUNCOR (IDATE, UTG, SDEC, SRASN, GST)
    RH = SRASN - (GST+RLONG)
    COSSZA = SIN(SDEC)*SIN(RLAT) + COS(SDEC)*COS(RLAT)*COS(RH)
    ! ALEX sza=solar zenith angle
    SZA = ACOS(COSSZA) * 180./cPi

  end subroutine solzen
  !============================================================================
  subroutine suncor (IDATE, UTG, SDEC, SRASN, GST)

    ! Subroutine SUNCOR returns the declination SDEC and right ascension SRASN
    ! of the sun in GEI coordinates, radians, for a given date IDATE in yyddd
    ! format and universal time UTG in seconds.  Greenwich Sidereal Time GST
    ! in radians is also returned.  Reference:  C.T. Russell, Geophysical
    ! Coordinate Transforms

    use ModNumConst, ONLY: cPi

    integer, intent(in) :: IDATE
    real,    intent(in) :: UTG
    real,    intent(out):: GST, SRASN, SDEC
    real    :: FDAY, DJ, T, VL, G, SLONG, OBLIQ, SLP, SIND, COSD, SEC
    integer :: IYR, IDAY
    !--------------------------------------------------------------------------
    FDAY=UTG/86400.
    IYR=IDATE/1000
    IDAY=IDATE-IYR*1000
    DJ=365*IYR+(IYR-1)/4+IDAY+FDAY-0.5
    T=DJ/36525.
    VL=AMOD(279.696678+.9856473354*DJ,360.)
    GST=AMOD(279.696678+.9856473354*DJ+360.*FDAY+180.,360.) * cPi/180.
    G=AMOD(358.475845+.985600267*DJ,360.) * cPi/180.
    SLONG=VL+(1.91946-.004789*T)*SIN(G)+.020094*SIN(2.*G)
    OBLIQ=(23.45229-0.0130125*T) *cPi/180.
    SLP=(SLONG-.005686) * cPi/180.
    SIND=SIN(OBLIQ)*SIN(SLP)
    COSD=SQRT(1.-SIND**2)
    SDEC=ATAN(SIND/COSD)
    SRASN=3.14159-ATAN2(1./TAN(OBLIQ)*SIND/COSD,-COS(SLP)/COSD)

  end subroutine suncor
  !============================================================================

end module GIMME_conductance
!==============================================================================
