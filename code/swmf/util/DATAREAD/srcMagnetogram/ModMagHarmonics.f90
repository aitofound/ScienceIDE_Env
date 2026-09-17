!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module ModMagHarmonics

  use ModNumConst
  use ModUtilities, ONLY: CON_stop
  use ModReadMagnetogram, ONLY: dPhi, dTheta, dSinTheta

  implicit none

  ! **********************Choice of this parameter**********************
  ! *Sin(Latitude): WSO : http://wso.stanford.edu                      *
  ! *   MDI: see http://soi.stanford.edu/magnetic/index6.html          *
  ! *   SOLIS:http://solis.nso.edu/vsm/map_info.html                   *
  ! *   GONG: http://gong.nso.edu/data/magmap/index.html               *
  ! ********************************************************************

  ! This Module reads a raw (RADIAL, not LOS!!!) magnetogram data file and
  ! generates a magnetogram file in the form of spherical
  ! harmonics to be used by the SWMF.

  ! ************************ Data Links ********************************
  ! * MDI:   http://soi.stanford.edu/magnetic/index6.html              *
  ! * WSO:   http://wso.stanford.edu/forms/prgs.html                   *
  ! * GONG:  http://gong.nso.edu/data/magmap/QR/mqs/                   *
  ! * SOLIS: ftp://solis.nso.edu/synoptic/level3/vsm/merged/carr-rot   *
  ! * MWO:   ftp://howard.astro.ucla.edu/pub/obs/synoptic_charts       *
  ! ********************************************************************
  ! * Field in Gauss: MDI,GONG,SOLIS                                   *
  ! * Field in microTesla(0.01Gs): WSO, MWO                            *
  ! ********************************************************************

  ! Name of output file
  character (len=100):: NameFileOut='harmonics.dat'

  integer, public:: i,n,m,iTheta,iPhi,iR,mm,nn
  real, allocatable:: Br_II(:,:)
  real:: dR=1.0
  integer:: nPhi=72, nTheta=29

  integer, parameter:: MaxHarmonics = 1800
  integer:: nHarmonics = MaxHarmonics, nHarmonicsIn = MaxHarmonics

  real:: CosTheta,SinTheta
  real:: stuff1,stuff2,stuff3
  real:: Theta,Phi

  real :: SumArea,da
  real :: NormalizationFactor
  integer :: iUnit2, iNM
  real,allocatable,dimension(:) :: gArray, hArray
  integer,allocatable,dimension(:) :: nArray, mArray
  integer :: SizeOfnm, ArrPerProc,EndProc,SizeLastProc
  real, allocatable, dimension(:,:)   :: p_nm, CosMPhi_II,SinMPhi_II
  real, allocatable, dimension(:,:,:) :: PNMTheta_III

  real:: SinThetaM, SinThetaM1
  integer:: delta_m0
  real, allocatable:: FactRatio1(:)
  integer:: MaxInt
  real, allocatable:: Sqrt_I(:)

  real, allocatable, dimension(:) :: ChebyshevWeightE_I, ChebyshevWeightW_I
  ! For integrating on subgrid in Theta, should be even integer:
  integer :: nSubGrid = 1
  ! If subgrid is used the polar fix can be applied reducing the error in the
  ! polar domain
contains
  !============================================================================

  subroutine read_harmonics_param

    use ModReadParam
    use ModReadMagnetogram, ONLY: read_magnetogram_param
    use ModMpi,             ONLY: MPI_COMM_SELF

    character(len=lStringLine) :: NameCommand

    character(len=*), parameter:: NameSub = 'read_harmonics_param'
    !--------------------------------------------------------------------------
    call read_file('HARMONICS.in', iCommIn = MPI_COMM_SELF)
    call read_init
    call read_echo_set(.true.)

    do
       if(.not.read_line() ) EXIT
       if(.not.read_command(NameCommand)) CYCLE
       select case(NameCommand)
       case("#HARMONICS")
          call read_var('nHarmonics', nHarmonicsIn)
       case("#OUTPUT")
          call read_var('NameFileOut', NameFileOut)
       case("#MAGNETOGRAMFILE", "#CHEBYSHEV", '#CHANGEWEAKFIELD',&
            '#CHANGEPOLARFIELD','#USEMAGNETOGRAMDATE')
          call read_magnetogram_param(NameCommand)
       case("#SUBGRID")
          call read_var('nSubGrid', nSubGrid)
       case default
          call CON_stop(NameSub//': unknown command='//trim(NameCommand))
       end select
    end do

  end subroutine read_harmonics_param
  !============================================================================
  subroutine read_modified_magnetogram

    use ModReadMagnetogram, ONLY: read_orig_magnetogram, nTheta0, nPhi0, &
         nThetaorig, nPhiorig

    character(len=*), parameter:: NameSub = 'read_modified_magnetogram'
    !--------------------------------------------------------------------------
    call read_orig_magnetogram

    nTheta = nTheta0
    nPhi   = nPhi0
    write(*,*)NameSub,': Original nTheta, nPhi =', nThetaorig, nPhiorig
    write(*,*)NameSub,': Remeshed nTheta, nPhi =', nTheta0, nPhi0

    ! Setting the order on harmonics to be equal to the
    ! latitudinal resolution.
    if(nHarmonicsIn > 0 .and. nHarmonicsIn < MaxHarmonics)then
       nHarmonics = nHarmonicsIn
    else
       nHarmonics = min(nThetaorig, MaxHarmonics)
    endif
    write(*,*)'Order of harmonics: ',nHarmonics

    ! Allocate the harmonic coefficients arrays
    allocate( &
         p_nm(nHarmonics+1,nHarmonics+1), &
         FactRatio1(nHarmonics+1))

    p_nm = 0.0

  end subroutine read_modified_magnetogram
  !============================================================================
  subroutine calc_harmonics

    ! This subroutine calculates the spherical harmonics from the
    !  modified - remesed magnetogram data

    use ModPlotFile, ONLY: save_plot_file
    use ModReadMagnetogram, ONLY: Br0_II, nTheta0, nPhi0, UseChebyshevNode, &
         MagnetogramTimeCR, LongShift, UseCosTheta, nThetaOrig, nPhiorig, &
         ChebyshevWeightE_I, ChebyshevWeightW_I, dPhi, dTheta, dSinTheta, &
         StringMagHeader

    integer :: iTheta, iPhi, m,inm, nn, mm
    real    :: dThetaChebyshev, dLon = 0.0
    real    :: CRFraction, CRNumber
    real, allocatable:: Br_II(:,:), Coord_DII(:,:,:), Var_VI(:,:)
    ! Subgrid computations:
    real    :: PolarField
    integer :: iSubGrid, nSubGrid2
    !--------------------------------------------------------------------------
    write(*,*)'Calculating harmonic coefficients'

    allocate(Sqrt_I(nHarmonics**2))
    MaxInt = nHarmonics**2
    ! Calculate sqrt(integer) from 1 to 10000::
    do m=1,MaxInt
       Sqrt_I(m) = sqrt(real(m))
    end do

    ! Calculate the ratio sqrt(2m!)/(2^m*m!)::
    factRatio1(:) = 0.0; factRatio1(1) = 1.0
    do m=1,nHarmonics
       factRatio1(m+1) = factRatio1(m)*Sqrt_I(2*m-1)/Sqrt_I(2*m)
    enddo

    ! If Chebyshev is done then remeshed theta & phi are used, else
    ! original values are used.
    if(UseChebyshevNode) then
       nTheta = nTheta0
       nPhi   = nPhi0
    else
       nTheta = nThetaorig
       nPhi   = nPhiorig
    endif

    ! LongShift is the starting Carrington longitude of the magnetogram
    ! Add the longitude width of a half cell (=180/nPhi) for cell centers
    ! dLon rotates the longitude into actual HGR/Carrington longitude
    dLon = LongShift + 180.0/nPhi
    if(nSubGrid==2*(nSubGrid/2))then
       nTheta = nTheta*nSubGrid
       dSinTheta = dSinTheta/nSubGrid
       nSubGrid2 = nSubGrid/2
       allocate(Br_II(0:nPhi-1,0:nTheta-1))
       ! Fix Southern pole value:
       ! PolarField = sum(Br0_II(:,1),1)/nPhi
       do iSubGrid = 0, nSubGrid2 - 1
          Br_II(:,iSubGrid) = Br0_II(:,1)
          ! (  (0.50 + iSubgrid)*Br0_II(:,1) +&
          ! (nSubgrid2 -iSubgrid - 0.5)*PolarField ) / nSubgrid2
       end do
       do iTheta = 1, nThetaOrig-1
          do iSubGrid = 0, nSubGrid - 1
             Br_II(:,iTheta*nSubGrid - nSubGrid2 + iSubGrid) = (&
                  (0.50 + iSubgrid)*Br0_II(:,iTheta+1) + &
                  (nSubgrid - iSubgrid - 0.5)*Br0_II(:,iTheta))/nSubGrid
          end do
       end do
       ! Fix Northern pole value:
       ! PolarField = sum(Br0_II(:,nThetaOrig),1)/nPhi
       do iSubGrid = 0, nSubGrid2 - 1
          Br_II(:,nTheta-nSubGrid2+iSubGrid) = Br0_II(:,nThetaOrig)
          !((0.50 + iSubgrid)*PolarField &
          !+ (nSubgrid2 -iSubgrid - 0.5)*Br0_II(:,nThetaOrig))/nSubgrid2
       end do
    else
       allocate(Br_II(0:nPhi-1,0:nTheta-1))
       Br_II=Br0_II
    end if

    if(UseChebyshevNode) then
       allocate(PNMTheta_III(nHarmonics+1,nHarmonics+1,0:nTheta-1))
       PNMTheta_III = 0.0
       dThetaChebyshev = cPi/(nTheta-1)
       do iTheta=0,nTheta-1
          Theta=cPi-iTheta*dThetaChebyshev
          CosTheta=cos(Theta)
          SinTheta=max(sin(Theta), 1E-9)
          call calc_legendre_polynomials
          PNMTheta_III(:,:,iTheta) = p_nm
       end do
    else
       allocate(PNMTheta_III(nHarmonics+1,nHarmonics+1,0:nTheta-1))
       PNMTheta_III = 0.0
       do iTheta=0,nTheta-1
          if(UseCosTheta)then
             Theta = cPi*0.5 - asin((real(iTheta)+0.5)*dSinTheta-1.0)
          else
             Theta = cPi*0.5 - ((iTheta + 0.50)*dTheta - cPi*0.50)
          end if
          CosTheta=cos(Theta)
          SinTheta=max(sin(Theta), 1E-9)
          call calc_legendre_polynomials
          PNMTheta_III(:,:,iTheta) = p_nm
       end do
    end if
    allocate(CosMPhi_II(0:nPhi-1,0:nHarmonics),&
         SinMPhi_II(0:nPhi-1,0:nHarmonics) )
    ! Save sins-cosins

    do iPhi=0,nPhi-1
       Phi=(iPhi)*dPhi
       do m=0,nHarmonics
          CosMPhi_II(iPhi,m) = cos(m*Phi)
          SinMPhi_II(iPhi,m) = sin(m*Phi)
       end do
    end do
    ! Creating an array with the size of SizeOfnm (the total g_nm and h_nm) for
    ! parallel calculation of the coefficients
    SizeOfnm=1
    do iNM=1,nHarmonics
       SizeOfnm=SizeOfnm+(iNM+1)
    enddo

    ! Allocating and initializing arrays
    if(allocated(gArray))deallocate(gArray)
    allocate(gArray(0:SizeOfnm-1))
    if(allocated(hArray))deallocate(hArray)
    allocate(hArray(0:SizeOfnm-1))
    if(allocated(nArray))deallocate(nArray)
    allocate(nArray(0:SizeOfnm-1))
    if(allocated(mArray))deallocate(mArray)
    allocate(mArray(0:SizeOfnm-1))

    gArray=0.0; hArray=0.0
    nArray=0.0; mArray=0.0

    ! Create arrays for n and m for parallelization
    iNM=0
    do nn=0,nHarmonics
       iNM=iNM+nn
       do mm=0,nn
          nArray(iNM+mm)=nn
          mArray(iNM+mm)=mm
       enddo
    end do

    ! Each processor gets part of the array
    do iNM = 0,SizeOfnm-1

       ! The proper normalization factor is (2n+1)/R_n,
       ! where R_n=n+1+n(1/Rs)**(2n+1).
       ! However, in this code the coefficients are normalized
       ! only with 2n+1 to reproduce
       ! the coefficients provided by Stanford. The division by R_n
       ! is done after the coefficients are been read in ModMagnetogram.f90.
       ! R_n=(nArray(iNM)+1.0)+nArray(iNM)*(1.0/Rs_PFSSM)**(2*nArray(iNM)+1)
       NormalizationFactor=(2*nArray(iNM)+1)

       SumArea=0.0

       do iTheta=0,nTheta-1

          if (UseChebyshevNode) then
             ! Use Chebyshev Weight in theta direction
             da = ChebyshevWeightE_I(iTheta)*ChebyshevWeightW_I(iTheta)*dPhi
          else
             if(UseCosTheta)then
                Theta = cPi*0.5 - asin((real(iTheta)+0.5)*dSinTheta-1.0)
                SinTheta = max(sin(Theta), 0.0)
                da = dSinTheta*dPhi
             else
                Theta = cPi*0.5 - ((iTheta + 0.50)*dTheta - cPi*0.50)
                SinTheta = max(sin(Theta), 0.0)
                da = SinTheta*dTheta*dPhi
             endif
          end if

          ! Calculate the set of Legendre polynomials (with Schmidt normalization)
          ! for a given CosTheta, SinTheta.
          ! For non-radial magnetogram (LOS), a division in SinTheta is needed.

          gArray(iNM) = gArray(iNM)+&
               sum(Br_II(:,iTheta)*&
               CosMPhi_II(:,mArray(iNM)) )*da*PNMTheta_III(&
               nArray(iNM)+1,mArray(iNM)+1,iTheta)!/SinTheta
          hArray(iNM) = hArray(iNM)+&
               sum(Br_II(:,iTheta)*&
               SinMPhi_II(:,mArray(iNM)) )*da*PNMTheta_III(&
               nArray(iNM)+1,mArray(iNM)+1,iTheta)!/SinTheta
          SumArea = SumArea+da*nPhi
       end do
       gArray(iNM) = NormalizationFactor*gArray(iNM)/SumArea
       hArray(iNM) = NormalizationFactor*hArray(iNM)/SumArea
    end do

    ! Leave out monopole (n=0) term::
    gArray(0) = 0.0

    write(*,*)'Done Calculating harmonic coefficients'

    ! Writing spherical harmonics file

    write(*,*)'Writing harmonic coefficients file, named ',NameFileOut

    allocate(Coord_DII(2,SizeOfNm,1), Var_VI(2,SizeOfNm))
    Coord_DII(1,:,1) = nArray
    Coord_DII(2,:,1) = mArray
    Var_VI(1,:)      = gArray
    Var_VI(2,:)      = hArray
    CRNumber = int(MagnetogramTimeCR)
    CRFraction = MagnetogramTimeCR - CRNumber
    call save_plot_file(NameFileOut, &
         StringHeaderIn=StringMagHeader, &
         TimeIn = CRFraction, &
         ParamIn_I=[ real(nHarmonics), CRNumber, dLon ], &
         IsCartesianIn = .false., &
         nDimIn = 2, &
         CoordIn_DII = Coord_DII, &
         VarIn_VI = Var_VI, &
         NameVarIn = "n m g h nOrder CR dLon", &
         StringFormatIn = "(2f5.0, 2f20.10)", &
         StringFormatParamIn = "(f5.0, 2f20.10)" &
         )

    deallocate(Coord_DII, Var_VI)

  end subroutine calc_harmonics
  !============================================================================

  subroutine calc_legendre_polynomials

    ! Calculate the Legendre polynomials for a particular latitude

    ! Calculate polynomials with appropriate normalization
    ! for Theta_PFSSMa::

    !--------------------------------------------------------------------------
    SinThetaM  = 1.0
    SinThetaM1 = 1.0
    p_nm(:,:)  = 0.0

    do m=0,nHarmonics
       if (m == 0) then
          delta_m0 = 1
       else
          delta_m0 = 0
       endif
       ! Eq.(27) from Altschuler et al. 1976::
       p_nm(m+1,m+1) = factRatio1(m+1)*Sqrt_I((2-delta_m0)*(2*m+1))* &
            SinThetaM
       ! Eq.(28) from Altschuler et al. 1976::
       if (m < nHarmonics) p_nm(m+2,m+1) = p_nm(m+1,m+1)*Sqrt_I(2*m+3)* &
            CosTheta

       SinThetaM1 = SinThetaM
       SinThetaM  = SinThetaM*SinTheta

    enddo
    do m=0,nHarmonics-2; do n=m+2,nHarmonics
       ! Eq.(29) from Altschuler et al. 1976::
       stuff1         = Sqrt_I(2*n+1)/Sqrt_I(n**2-m**2)
       stuff2         = Sqrt_I(2*n-1)
       stuff3         = Sqrt_I((n-1)**2-m**2)/Sqrt_I(2*n-3)
       p_nm(n+1,m+1)  = stuff1*(stuff2*CosTheta*p_nm(n,m+1)-  &
            stuff3*p_nm(n-1,m+1))

    enddo; enddo
    ! Apply Schmidt normalization::
    do m=0,nHarmonics; do n=m,nHarmonics
       ! Eq.(33) from Altschuler et al. 1976::
       stuff1 = 1.0/Sqrt_I(2*n+1)
       ! Eq.(34) from Altschuler et al. 1976::
       p_nm(n+1,m+1)  = p_nm(n+1,m+1)*stuff1
    enddo; enddo

  end subroutine calc_legendre_polynomials
  !============================================================================

end module ModMagHarmonics
!==============================================================================
