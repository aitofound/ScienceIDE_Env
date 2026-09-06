!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module ModPotentialField

  use ModMpi
  use ModUtilities, ONLY: CON_stop
  use ModConst, ONLY: cPi, cTwoPi, cHalfPi
  use ModNumConst,    ONLY: cHalfPi, cPi, cDegToRad

  implicit none

  ! input parameter
  logical:: DoReadMagnetogram = .true.
  real :: LongCR = 0.0, CarRot = 0.0
  ! grid and domain parameters
  integer:: nR = 150, nThetaCoarse = 180, nPhiCoarse = 360
  real   :: rMin = 1.0, rMax = 2.5
  logical:: UseLogRadius = .false.  ! logarithmic or linear in radius

  ! wedge parameters
  logical:: UseWedge = .false.
  real   :: ThetaMin = 0, ThetaMax = cPi, PhiMin = 0, PhiMax = cTwoPi

  ! domain decomposition
  integer:: nProcTheta = 0, nProcPhi = 0

  ! solver parameters
  character(len=20):: NameSolver         = 'BICGSTAB' ! or 'GMRES' or 'AMG'
  character(len=20):: NamePreconditioner = 'ILU' ! or 'AMG' or 'MG'
  logical          :: UseHypre           = .false.
  logical          :: UsePreconditioner  = .true.
  real             :: Tolerance          = 1e-10

  logical           :: DoRemoveMonopole = .true.

  ! output paramters
  logical           :: DoSaveBxyz   = .false.
  character(len=100):: NameFileBxyz = 'potentialBxyz'
  character(len=5)  :: TypeFileBxyz = 'real8'

  logical           :: DoSaveField   = .false.
  character(len=100):: NameFileField = 'potentialfield'
  character(len=5)  :: TypeFileField = 'real8'

  logical           :: DoSavePotential   = .false.
  character(len=100):: NameFilePotential = 'potentialtest'
  character(len=5)  :: TypeFilePotential = 'real8'

  logical           :: DoSaveTecplot   = .false.
  character(len=100):: NameFileTecplot = 'potentialfield.dat'

  logical           :: DoSaveGhostFace   = .false.
  character(len=100):: NameFileGhostFace = 'ghostface'
  character(len=5)  :: TypeFileGhostFace = 'real8'

  ! testing parameters
  logical :: UseTiming = .true.
  real    :: TimeStart, TimeEnd

  logical :: DoTestMe  = .false.
  integer :: iRTest = 1, iPhiTest = 1, iThetaTest = 2

  ! local variables --------------------
  character(len=100):: NameFile

  logical :: UseBr = .true.

  real, dimension(:), allocatable :: &
       Radius_I, Theta_I, Phi_I, SinTheta_I, &
       dRadius_I, dPhi_I, dCosTheta_I, &
       RadiusNode_I, ThetaNode_I, PhiNode_I, SinThetaNode_I, &
       dRadiusNode_I, dTheta_I, dThetaNode_I, dPhiNode_I, dCosThetaNode_I

  real, allocatable::  Potential_C(:,:,:), Rhs_C(:,:,:), Br_II(:,:),&
       B0_DF(:,:,:,:), DivB_C(:,:,:), PlotVar_VC(:,:,:,:), BrLocal_II(:,:)

  ! Variables for hepta preconditioner
  real, parameter:: PrecondParam = 1.0 ! see ModLinearSolver

  ! Seven diagonals for the preconditioner
  real, dimension(:), allocatable :: &
       d_I, e_I, e1_I, e2_I, f_I, f1_I, f2_I

  integer, parameter :: iComm = MPI_COMM_WORLD
  integer :: iProc, nProc, iProcTheta, iProcPhi
  integer :: iTheta0, iPhi0
  integer :: nTheta, nPhi
  real,  allocatable :: TmpXPhi0_II(:,:),TmpXPhipi_II(:,:)
  integer :: nThetaLgr,nThetaSml,nPhiLgr,nPhiSml
  integer :: nProcThetaLgr,nProcThetaSml,nProcPhiLgr,nProcPhiSml

  real, allocatable :: &
       SendBC010_II(:,:), SendBC180_II(:,:), SendBC12_II(:,:), &
       SendBC21_II(:,:),  SendBC34_II(:,:), SendBC43_II(:,:), &
       SendBC020_II(:,:), SendBC360_II(:,:), &
       RecvBCLgr010_II(:,:), RecvBCSml010_II(:,:), &
       RecvBCLgr180_II(:,:), RecvBCSml180_II(:,:), &
       RecvBC12_II(:,:), RecvBC21_II(:,:), &
       RecvBC34_II(:,:), RecvBC43_II(:,:), &
       RecvBC020_II(:,:), RecvBC360_II(:,:)

contains
  !============================================================================

  subroutine read_fdips_param

    use ModReadParam
    use ModReadMagnetogram, ONLY: read_magnetogram_param

    character(len=lStringLine) :: NameCommand
    character(len=10):: TypeOutput
    integer:: i, iProcTest

    character(len=*), parameter:: NameSub = 'read_fdips_param'
    !--------------------------------------------------------------------------
    call read_file('FDIPS.in')
    call read_init
    if(iProc==0) call read_echo_set(.true.)

    do
       if(.not.read_line() ) EXIT
       if(.not.read_command(NameCommand)) CYCLE
       select case(NameCommand)
       case("#DOMAIN")
          call read_var('rMin', rMin)
          call read_var('rMax', rMax)
          call read_var('UseLogRadius', UseLogRadius)
          call read_var('UseWedge', UseWedge)
          if (UseWedge) then
             call read_var('wedgeLatMin',ThetaMax)
             ThetaMax = (90-ThetaMax) * cDegToRad
             call read_var('wedgeLatMax',ThetaMin)
             ThetaMin = (90-ThetaMin) * cDegToRad
             if (ThetaMax <= ThetaMin) then
                call CON_stop(NameSub//': Wedge latitude_min > latitude_max.')
             endif
             call read_var('wedgeLonMin',PhiMin)
             PhiMin = PhiMin * cDegToRad
             call read_var('wedgeLonMax',PhiMax)
             PhiMax = PhiMax * cDegToRad
             if (PhiMin >= PhiMax) then
                ! wedge over zero meridian
                call CON_stop(NameSub//': '//&
                     'Currently does not support wedge over longitude=0.')
             endif
          endif
       case("#GRID")
          call read_var('nR    ', nR)
          call read_var('nThetaCoarse', nThetaCoarse)
          call read_var('nPhiCoarse  ', nPhiCoarse)
       case("#PARALLEL")
          call read_var('nProcTheta', nProcTheta)
          call read_var('nProcPhi'  , nProcPhi)
       case("#MAGNETOGRAMFILE",'#CHANGEPOLARFIELD', '#CHANGEWEAKFIELD', "#CHEBYSHEV")
          call read_magnetogram_param(NameCommand)
       case('#REMOVEMONOPOLE')
          call read_var('DoRemoveMonopole', DoRemoveMonopole)
       case("#TIMING")
          call read_var('UseTiming', UseTiming)
       case("#TEST")
          call read_var('iProcTest', iProcTest)
          DoTestMe = iProc == iProcTest
       case("#TESTIJK")
          call read_var('iRTest'    , iRTest)
          call read_var('iPhiTest'  , iPhiTest)
          call read_var('iThetaTest', iThetaTest)
       case("#SOLVER")
          call read_var('NameSolver',         NameSolver, &
               IsUpperCase=.true.)
          call read_var('NamePreconditioner', NamePreconditioner, &
               IsUpperCase=.true.)
          call read_var('Tolerance',          Tolerance)
          UseHypre = index(NameSolver,'MG') > 0 .or. &
               index(NamePreconditioner,'MG') > 0
          UsePreconditioner = NameSolver == 'BICGSTAB' .and. &
               NamePreconditioner /= 'NONE'
       case("#HYPRE")
          call read_hypre_param
       case("#OUTPUT")
          call read_var('TypeOutput', TypeOutput, IsLowerCase=.true.)
          select case(TypeOutput)
          case('b', 'bxyz')
             DoSaveBxyz = .true.
             call read_var('NameFileBxyz', NameFileBxyz)
             call read_var('TypeFileBxyz', TypeFileBxyz)
             ! remove .out extension if present
             i = index(NameFileBxyz,'.out')
             if(i>0) NameFileBxyz = NameFileBxyz(1:i-1)
          case('field')
             DoSaveField = .true.
             call read_var('NameFileField', NameFileField)
             call read_var('TypeFileField', TypeFileField)
             ! remove .out extension if present
             i = index(NameFileField,'.out')
             if(i>0) NameFileField = NameFileField(1:i-1)
          case('potential')
             DoSavePotential = .true.
             call read_var('NameFilePotential', NameFilePotential)
             call read_var('TypeFilePotential', TypeFilePotential)
             ! remove .out extension if present
             i = index(NameFilePotential,'.out')
             if(i>0) NameFilePotential = NameFilePotential(1:i-1)
          case('tecplot')
             if(nProc > 1)call CON_stop(NameSub// &
                  ': TypeOutput=tecplot works for serial runs only')
             DoSaveTecplot = .true.
             call read_var('NameFileTecplot', NameFileTecplot)
          case('ghostface')
             DoSaveGhostFace = .true.
             call read_var('NameFileGhostFace', NameFileGhostFace)
             call read_var('TypeFileGhostFace', TypeFileGhostFace)
             ! remove .out extension if present
             i = index(NameFileGhostFace,'.out')
             if(i>0) NameFileGhostFace = NameFileGhostFace(1:i-1)
          case default
             call CON_stop(NameSub//': unknown TypeOutput='//trim(TypeOutput))
          end select
       case default
          call CON_stop(NameSub//': unknown command='//trim(NameCommand))
       end select
    end do

    if ( nProcTheta*nProcPhi /= nProc .and. &
         nProcTheta*nProcPhi /= 0 .and. iProc==0) then
       write(*,*) NameSub, ': WARNING: nProcTheta, nProcPhi, nProc=', &
            nProcTheta, nProcPhi, nProc
       write(*,*) NameSub, &
            ': Changing to default decomposition. Look I am intelligent!'
       nProcTheta = 0
       nProcPhi = 0
    end if

    ! Do timing on proc 0 only, if at all
    if(iProc > 0) UseTiming = .false.

  end subroutine read_fdips_param
  !============================================================================
  subroutine read_modified_magnetogram

    use ModReadMagnetogram, ONLY: read_orig_magnetogram, Br0_II,  &
         Lat0_I, Phi0_I, nThetaAll, nPhiAll, LongShift, CarringtonRotation

    logical :: IsPhiThetaOrder = .false.
    logical :: IsInputLonLatInDegree = .true.

    character(len=*), parameter:: NameSub = 'read_modified_magnetogram'
    !--------------------------------------------------------------------------
    call read_orig_magnetogram(IsPhiThetaOrder, UseWedge, DoRemoveMonopole, &
         nThetaCoarse, nPhiCoarse)

    allocate(Br_II(nThetaAll,nPhiAll))
    ! Converting Deg to Radians
    if (maxval(abs(Lat0_I)) > cHalfPi .or. &
         maxval(abs(Phi0_I)) > cTwoPi) then
       IsInputLonLatInDegree = .true.
    else
       IsInputLonLatInDegree = .false.
    endif
    if (IsInputLonLatInDegree) then
       Phi0_I = Phi0_I * cDegToRad
       Lat0_I = Lat0_I * cDegToRad
    endif

    Br_II=Br0_II

    ! The CR longitude of Left edge (Longitdue Shift)
    ! and the Carrington Rotation of the Central Meridian
    ! is read.
    LongCR = LongShift
    CarRot = CarringtonRotation

  end subroutine read_modified_magnetogram
  !============================================================================

  subroutine init_potential_field

    use ModConst, ONLY: cTwoPi
    use ModReadMagnetogram, ONLY: UseCosTheta, nThetaAll, nPhiAll

    integer :: iR, iTheta, iPhi
    integer :: i, j

    real:: dR, dLogR, dTheta, dPhi, dZ, z
    real:: CellShiftPhi
    real:: Err, ErrNew

    !--------------------------------------------------------------------------

    if (nProcTheta*nProcPhi == 0) then
       nProcPhi = 1
       Err = real(max(nPhiAll,nThetaAll))
       do i = 1, floor(sqrt(real(nProc)))
          j = nProc/i
          if (i*j == nProc) then
             ErrNew = abs(nPhiAll/real(i)-nThetaAll/real(j))
             if (ErrNew < Err) then
                Err = ErrNew
                nProcPhi = i
             endif
             ErrNew = abs(nPhiAll/real(j)-nThetaAll/real(i))
             if (ErrNew < Err) then
                Err = ErrNew
                nProcPhi = j
             endif
          endif
       enddo
       nProcTheta = nProc/nProcPhi
       if (iProc == 0) write(*,*) 'New nProcTheta, nProcPhi, nProc=', &
            nProcTheta, nProcPhi, nProc
    endif

    ! The processor coordinate
    iProcTheta = iProc/nProcPhi
    iProcPhi   = iProc - iProcTheta*nProcPhi

    ! Calculate the nTheta, nPhi. To distribute as even as possible,
    ! there will be two different nTheta and nPhi.
    nThetaLgr  = ceiling(real(nThetaAll)/nProcTheta)
    nThetaSml  = floor(  real(nThetaAll)/nProcTheta)
    nPhiLgr    = ceiling(real(nPhiAll)/nProcPhi)
    nPhiSml    = floor(  real(nPhiAll)/nProcPhi)

    ! Calculate the number of processors which has large/small
    ! local number of Theta/Phi.
    nProcThetaLgr = mod(nThetaAll, nProcTheta)
    nProcThetaSml = nProcTheta - nProcThetaLgr
    nProcPhiLgr = mod(nPhiAll, nProcPhi)
    nProcPhiSml = nProcPhi - nProcPhiLgr

    ! Test if the partitioning works
    if (iProc == 0) then
       write(*,*) 'nThetaLgr = ',nThetaLgr, 'nThetaSml = ', nThetaSml
       write(*,*) 'nPhiLgr   = ', nPhiLgr,  'nPhiSml   = ', nPhiSml
       write(*,*) 'Partitioning in nThetaAll gives: ', &
            nThetaLgr*nProcThetaLgr + nThetaSml*nProcThetaSml, &
            'Actual nThetaAll is: ', nThetaAll
       write(*,*) 'Partitioning in nPhiAll gives:   ', &
            nPhiLgr*nProcPhiLgr + nPhiSml*nProcPhiSml, &
            'Actual nPhiAll is:   ', nPhiAll
    end if

    ! Both iProcTheta and iProcPhi in the large region
    if (iProcTheta < nProcThetaLgr .and. iProcPhi < nProcPhiLgr) then
       nTheta = nThetaLgr
       nPhi   = nPhiLgr
       iTheta0 = iProcTheta* nThetaLgr
       iPhi0   = iProcPhi  * nPhiLgr
    end if

    ! Only iProcTheta in the large region
    if (iProcTheta < nProcThetaLgr .and. iProcPhi >= nProcPhiLgr) then
       nTheta = nThetaLgr
       nPhi   = nPhiSml
       iTheta0 = iProcTheta  * nThetaLgr
       iPhi0   = nProcPhiLgr * nPhiLgr + (iProcPhi - nProcPhiLgr)*nPhiSml
    end if

    ! Only iProcPhi in the large region
    if (iProcTheta >= nProcThetaLgr .and. iProcPhi < nProcPhiLgr) then
       nTheta  = nThetaSml
       nPhi    = nPhiLgr
       iTheta0 = nProcThetaLgr * nThetaLgr + &
            (iProcTheta - nProcThetaLgr)*nThetaSml
       iPhi0   = iProcPhi      * nPhiLgr
    end if

    ! Both iProcTheta and iProcPhi in the small region
    if (iProcTheta >= nProcThetaLgr .and. iProcPhi >= nProcPhiLgr) then
       nTheta = nThetaSml
       nPhi   = nPhiSml
       iTheta0 = nProcThetaLgr*nThetaLgr &
            + (iProcTheta - nProcThetaLgr)*nThetaSml
       iPhi0   = nProcPhiLgr  *nPhiLgr   &
            + (iProcPhi   - nProcPhiLgr)  *nPhiSml
    end if

    allocate( BrLocal_II(nTheta,nPhi), &
         Radius_I(0:nR+1), Theta_I(0:nTheta+1), Phi_I(0:nPhi+1), &
         dRadius_I(nR), dPhi_I(nPhi), &
         SinTheta_I(0:nTheta+1), dTheta_I(nTheta), dCosTheta_I(nTheta), &
         SinThetaNode_I(nTheta+1), dCosThetaNode_I(nTheta+1), &
         RadiusNode_I(nR+1), ThetaNode_I(nTheta+1), PhiNode_I(nPhi+1), &
         dRadiusNode_I(nR+1), dThetaNode_I(nTheta+1), dPhiNode_I(nPhi+1) , &
         Potential_C(nR,nTheta,nPhi), &
         Rhs_C(nR,nTheta,nPhi), &
         B0_DF(3,nR+1,nTheta+1,nPhi+1), &
         DivB_C(nR,nTheta,nPhi))

    ! Set BrLocal_II, this is used in set_boundary when UseBr is true
    BrLocal_II(:,:) = Br_II(iTheta0 + 1: iTheta0 + nTheta, &
         iPhi0   + 1: iPhi0   + nPhi)

    ! nR is the number of mesh cells in radial direction
    ! cell centered radial coordinate

    if(UseLogRadius)then
       dLogR = log(rMax/rMin)/nR
       do iR = 0, nR+1
          Radius_I(iR) = rMin*exp( (iR - 0.5)*dLogR )
       end do
       ! node based radial coordinate
       do iR = 1, nR+1
          RadiusNode_I(iR) = rMin*exp( (iR - 1)*dLogR )
       end do

    else
       dR = (rMax - rMin)/nR
       do iR = 0, nR+1
          Radius_I(iR) = rMin + (iR - 0.5)*dR
       end do
       ! node based radial coordinate
       do iR = 1, nR+1
          RadiusNode_I(iR) = rMin + (iR - 1)*dR
       end do
    end if

    dRadius_I     = RadiusNode_I(2:nR+1) - RadiusNode_I(1:nR)
    dRadiusNode_I = Radius_I(1:nR+1) - Radius_I(0:nR)

    if(UseCosTheta)then
       dZ = 2.0/nThetaAll

       do iTheta = 0, nTheta+1
          z = max(-1.0, min(1.0, 1 - (iTheta + iTheta0 - 0.5)*dZ))
          Theta_I(iTheta) = acos(z)
       end do

       ! Set the boundary condition of Theta_I
       if (iProcTheta == 0) &
            Theta_I(0) = -Theta_I(1)
       if (iProcTheta == nProcTheta-1) &
            Theta_I(nTheta+1) = cTwoPi - Theta_I(nTheta)

       ! Set ThetaNode_I
       do iTheta = 1, nTheta + 1
          z = max(-1.0, min(1.0, 1 - (iTheta + iTheta0 -1)*dZ))
          ThetaNode_I(iTheta) = acos(z)
       end do
    else
       dTheta = (ThetaMax-ThetaMin)/nThetaAll
       ! Set Theta_I
       do iTheta = 0, nTheta+1
          Theta_I(iTheta) = ThetaMin + (iTheta  + iTheta0 - 0.5)*dTheta
       end do

       ! Set ThetaNode_I
       do iTheta = 1, nTheta+1
          ThetaNode_I(iTheta) = ThetaMin + (iTheta + iTheta0 - 1)*dTheta
       end do
    end if

    dTheta_I = ThetaNode_I(2:nTheta+1) - ThetaNode_I(1:nTheta)
    SinTheta_I = sin(Theta_I)
    SinThetaNode_I = sin(ThetaNode_I)

    if(UseCosTheta)then
       dCosTheta_I     = dZ
       dCosThetaNode_I = dZ
       dThetaNode_I    = Theta_I(1:nTheta+1) - Theta_I(0:nTheta)
    else
       dCosTheta_I(1:nTheta) = SinTheta_I(1:nTheta)*dTheta
       dCosThetaNode_I       = SinThetaNode_I*dTheta
       dThetaNode_I          = dTheta
    end if

    dPhi = (PhiMax-PhiMin)/nPhiAll
    ! Set Phi_I
    do iPhi = 0, nPhi+1
       ! Phi grid is Cell Centered
       Phi_I(iPhi) = PhiMin+(iPhi + iPhi0 - 0.5)*dPhi
    end do

    PhiNode_I = Phi_I(1:nPhi+1) - 0.5*dPhi
    dPhi_I = PhiNode_I(2:nPhi+1) - PhiNode_I(1:nPhi)
    dPhiNode_I = Phi_I(1:nPhi+1) - Phi_I(0:nPhi)

    Potential_C       =   0.0
    Rhs_C             =   0.0

  end subroutine init_potential_field
  !============================================================================

  subroutine save_potential_field

    use ModIoUnit,      ONLY: UnitTmp_
    use ModPlotFile,    ONLY: save_plot_file
    use ModCoordTransform, ONLY: rot_xyz_sph
    use ModReadMagnetogram, ONLY: nThetaAll, nPhiAll, StringMagHeader

    integer:: iR, jR, iTheta, iPhi, iLat, nLat
    real   :: r, CosTheta, SinTheta, CosPhi, SinPhi
    real   :: Br, Btheta, Bphi, XyzSph_DD(3,3)
    real   :: rI, rJ, rInv
    real, allocatable :: Lat_I(:), b_DX(:,:,:,:), b_DII(:,:,:)
    real, allocatable :: Bpole_DII(:,:,:), Btotal_DII(:,:,:)
    real, allocatable :: Potential_G(:,:,:), PlotVar_VG(:,:,:,:)
    integer:: iError
    integer:: iStatus_I(mpi_status_size)
    integer:: MinPhi, MaxPhi, nPhiOut, MinLat, MaxLat, MinTheta, MaxTheta

    ! Only the last processors in the phi direction write out the ghost cells
    !--------------------------------------------------------------------------
    MinPhi = 1
    MaxPhi = nPhi

    ! Output is on an r-lon-lat grid. For sake of clarity, use iLat and nLat
    nLat = nTheta

    ! Latitude range
    MinLat = 1
    MaxLat = nLat

    if(DoSaveBxyz)then
       ! Add ghost cells for poles. Note that min theta is max lat.
       if(iProcTheta == 0)              MaxLat = nLat + 1
       if(iProcTheta == nProcTheta - 1) MinLat = 0

       if(.not.UseWedge)then
          if(iProcPhi == 0)                MinPhi = 0
          if(iProcPhi == nProcPhi - 1)     MaxPhi = nPhi + 1
       end if

       ! Average field for each radial index at the two poles
       allocate(Bpole_DII(3,nR+1,2), Btotal_DII(3,nR+1,2))
    end if
    allocate(Lat_I(MinLat:MaxLat), &
         b_DX(3,nR+1,MinPhi:MaxPhi,MinLat:MaxLat), b_DII(3,nR+1,nTheta))

    do iLat = MinLat, MaxLat
       Lat_I(iLat) = cHalfPi - max(0.0, min(cPi, Theta_I(nTheta+1-iLat)))
    end do

    ! Average the magnetic field to the R face centers

    ! For the radial component only the theta index changes
    do iPhi = 1, nPhi; do iTheta = 1, nTheta; do iR = 1, nR+1
       b_DX(1,iR,iPhi,nTheta+1-iTheta) = B0_DF(1,iR,iTheta,iPhi)
    end do; end do; end do

    ! Use radius as weights to average Bphi and Btheta
    ! Also swap phi and theta components (2,3) -> (3,2)
    do iPhi = 1, nPhi; do iTheta = 1, nTheta; do iR = 1, nR

       iLat = nTheta + 1 - iTheta

       ! Use first order approximation at lower boundary. Reduces noise.
       jR = max(1,iR-1)

       rI   = Radius_I(iR)
       rJ   = Radius_I(jR)
       rInv = 0.25/RadiusNode_I(iR)

       b_DX(2,iR,iPhi,iLat) = rInv* &
            ( rI*(B0_DF(3,iR,iTheta,iPhi) + B0_DF(3,iR,iTheta,iPhi+1)) &
            + rJ*(B0_DF(3,jR,iTheta,iPhi) + B0_DF(3,jR,iTheta,iPhi+1)) )

       b_DX(3,iR,iPhi,iLat) = rInv* &
            ( rI*(B0_DF(2,iR,iTheta,iPhi) + B0_DF(2,iR,iTheta+1,iPhi)) &
            + rJ*(B0_DF(2,jR,iTheta,iPhi) + B0_DF(2,jR,iTheta+1,iPhi)) )

    end do; end do; end do

    ! set tangential components to zero at rMax
    b_DX(2:3,nR+1,:,:) = 0.0

    ! Apply periodicity in Phi to fill the nPhi+1 ghost cell
    if (.not. UseWedge) then
       if (nProcPhi > 1) then
          ! Message pass from cell 1 to nPhi+1
          if (iProcPhi ==0 ) then
             b_DII = b_DX(:,:,1,1:nLat)
             call mpi_send(b_DII, 3*(nR+1)*nLat, MPI_REAL, &
                  iProcTheta*nProcPhi + nProcPhi-1, 21, iComm,  iError)
          end if
          if (iProcPhi == nProcPhi -1) then
             call mpi_recv(b_DII, 3*(nR+1)*nLat, MPI_REAL, &
                  iProcTheta*nProcPhi , 21, iComm, iStatus_I, iError)
             b_DX(:,:,MaxPhi,1:nLat) = b_DII
          end if
          ! Message pass from cell nPhi to 0
          if (iProcPhi == nProcPhi -1) then
             b_DII = b_DX(:,:,nPhi,1:nLat)
             call mpi_send(b_DII, 3*(nR+1)*nLat, MPI_REAL, &
                  iProcTheta*nProcPhi , 22, iComm, iError)
          end if
          if (iProcPhi ==0 ) then
             call mpi_recv(b_DII, 3*(nR+1)*nLat, MPI_REAL, &
                  iProcTheta*nProcPhi + nProcPhi-1, 22, iComm, iStatus_I, iError)
             b_DX(:,:,MinPhi,1:nLat) = b_DII
          end if
       else
          b_DX(:,:,MaxPhi,1:nLat) = b_DX(:,:,1,1:nLat)
          b_DX(:,:,MinPhi,1:nLat) = b_DX(:,:,nPhi,1:nLat)
       end if
    endif

    call  MPI_bcast(StringMagHeader, len(StringMagHeader), MPI_CHARACTER, 0, iComm, iError)
    call  MPI_bcast(LongCR, 5 , MPI_REAL, 0, iComm, iError)
    call  MPI_bcast(CarRot, 10 , MPI_REAL, 0, iComm, iError)

    if(DoSaveField)then
       ! Note the fake processor index to be used by redistribute.pl
       write(NameFile,'(a,2i2.2,a,i3.3,a)') &
            trim(NameFileField)//'_np01', nProcPhi, nProcTheta, '_', &
            iProcPhi + (nProcTheta - 1 - iProcTheta)*nProcPhi, '.out'

       call save_plot_file(NameFile, TypeFileIn=TypeFileField, &
            StringHeaderIn = &
            'Radius [Rs] Longitude [Rad] Latitude [Rad] B [G]', &
            nameVarIn = 'Radius Longitude Latitude Br Bphi Btheta' &
            //' Ro_PFSSM Rs_PFSSM LongCR CR', &
            ParamIn_I = [ rMin, rMax, LongCR, CarRot], &
            nDimIn=3, VarIn_VIII=b_DX(:,:,MinPhi:MaxPhi,1:nTheta), &
            Coord1In_I=RadiusNode_I, &
            Coord2In_I=Phi_I(MinPhi:MaxPhi), &
            Coord3In_I=Lat_I(1:nLat))
    end if

    if(DoSaveBxyz)then
       ! Convert to X,Y,Z components on the r-lon-lat grid
       do iLat = 1, nLat; do iPhi = MinPhi, MaxPhi
          XyzSph_DD = rot_xyz_sph(cHalfPi-Lat_I(iLat),Phi_I(iPhi))
          do iR = 1, nR+1
             Br     = b_DX(1,iR,iPhi,iLat)
             Btheta = b_DX(3,iR,iPhi,iLat)
             Bphi   = b_DX(2,iR,iPhi,iLat)
             b_DX(:,iR,iPhi,iLat) = matmul(XyzSph_DD, [Br, Btheta, Bphi])
          end do
       end do; end do

       ! Average values in the Phi direction for the two poles.
       ! The average value only makes sense for Cartesian components
       if (.not. UseWedge) then
          ! Initialize on all processors for the MPI_allreduce
          Bpole_DII = 0.0

          ! South pole, minimum latitude (maximum theta)
          if(iProcTheta == nProcTheta - 1) &
               Bpole_DII(:,:,1) = sum(b_DX(:,:,1:nPhi,1), DIM=3)

          ! North pole, maximum latitude (minimum theta)
          if(iProcTheta == 0) &
               Bpole_DII(:,:,2) = sum(b_DX(:,:,1:nPhi,nLat), DIM=3)

          ! Sum over processors in the phi direction
          if(nProcPhi > 1)then
             call MPI_allreduce(Bpole_DII, Btotal_DII, size(Bpole_DII), &
                  MPI_REAL, MPI_SUM, iComm, iError)
             Bpole_DII = Btotal_DII
          end if

          ! Get the average
          Bpole_DII = Bpole_DII / nPhiAll

          ! Use same value for all Phi indexes at the ghost cells at the poles
          do iPhi = MinPhi, MaxPhi
             if(iProcTheta == nProcTheta - 1) &
                  b_DX(:,:,iPhi,MinLat) = Bpole_DII(:,:,1)
             if(iProcTheta == 0)              &
                  b_DX(:,:,iPhi,MaxLat) = Bpole_DII(:,:,2)
          end do
       endif

       ! Note the fake processor index to be used by redistribute.pl
       write(NameFile,'(a,2i2.2,a,i3.3,a)') &
            trim(NameFileBxyz)//'_np01', nProcPhi, nProcTheta, '_', &
            iProcPhi + (nProcTheta - 1 - iProcTheta)*nProcPhi, '.out'

       if(UseLogRadius)then
          call save_plot_file(NameFile, TypeFileIn=TypeFileBxyz, &
               StringHeaderIn = trim(StringMagHeader),&
               nameVarIn = 'logRadius Longitude Latitude Bx By Bz rMin rMax LongCR CR', &
               ParamIn_I = [ rMin, rMax, LongCR, CarRot], &
               nDimIn=3, VarIn_VIII=b_DX, &
               Coord1In_I=log10(RadiusNode_I), &
               Coord2In_I=Phi_I(MinPhi:MaxPhi), &
               Coord3In_I=Lat_I)
       else
          call save_plot_file(NameFile, TypeFileIn=TypeFileBxyz, &
               StringHeaderIn = trim(StringMagHeader), &
               nameVarIn = 'Radius Longitude Latitude Bx By Bz rMin rMax LongCR CR', &
               ParamIn_I = [ rMin, rMax, LongCR, CarRot], &
               nDimIn=3, VarIn_VIII=b_DX, &
               Coord1In_I=RadiusNode_I, &
               Coord2In_I=Phi_I(MinPhi:MaxPhi), &
               Coord3In_I=Lat_I)
       end if
       deallocate(Bpole_DII, Btotal_DII)

    end if

    if (.not. UseWedge) then
       nPhiOut = nPhiAll+1
    else
       nPhiOut = nPhiAll
    endif
    if(DoSaveTecplot)then
       open(unit = UnitTmp_, file=NameFileTecplot, status='replace')

       write (UnitTmp_, '(a)') 'Title = "'     // 'PFSSM' // '"'
       write (UnitTmp_, '(a)') &
            'Variables = ' // trim ('"X [Rs]", "Y [Rs]", "Z [Rs]","Bx [G]",'// &
            ' "By [G]", "Bz [G]"')
       write(UnitTmp_, '(a)') 'ZONE T="Rectangular zone"'
       write(UnitTmp_, '(a,i6,a,i6,a,i6,a)') &
            ' I = ', nR+1, ', J=', nThetaAll, ', K=', nPhiOut, &
            ', ZONETYPE=Ordered'
       write(UnitTmp_, '(a)')' DATAPACKING=POINT'
       write(UnitTmp_, '(a)')' DT=(SINGLE SINGLE SINGLE SINGLE SINGLE SINGLE )'

       do iPhi = 1, nPhiOut; do iTheta = 1, nThetaAll; do iR = 1, nR+1
          Br     = b_DX(1,iR,iPhi,nThetaAll+1-iTheta)
          Btheta = b_DX(3,iR,iPhi,nThetaAll+1-iTheta)
          Bphi   = b_DX(2,iR,iPhi,nThetaAll+1-iTheta)
          r = RadiusNode_I(iR)
          SinTheta = SinTheta_I(iTheta)
          CosTheta = cos(Theta_I(iTheta))
          SinPhi   = sin(Phi_I(iPhi))
          CosPhi   = cos(Phi_I(iPhi))

          write (UnitTmp_,fmt="(6(E14.6))") &
               r*SinTheta*CosPhi, &
               r*SinTheta*SinPhi, &
               r*CosTheta, &
               Br*SinTheta*CosPhi + Btheta*CosTheta*CosPhi - Bphi*SinPhi, &
               Br*SinTheta*SinPhi + Btheta*CosTheta*SinPhi + Bphi*CosPhi, &
               Br*CosTheta        - Btheta*SinTheta
       end do; end do; end do

       close(UnitTmp_)
    end if

    deallocate(b_DX)

    if (DoSaveGhostFace) then
       allocate( Potential_G(0:nR+1,0:nTheta+1,0:nPhi+1), &
            PlotVar_VG(4,0:nR+1,0:nTheta+1,0:nPhi+1))

       MinTheta = 1
       MaxTheta = nTheta
       MinPhi   = 1
       MaxPhi   = nPhi
       if(iProcTheta == 0)               MinTheta = 0
       if(iProcTheta == nProcTheta - 1)  MaxTheta = nTheta+1
       if(iProcPhi   == 0)               MinPhi   = 0
       if(iProcPhi   == nProcPhi - 1)    MaxPhi   = nPhi+1

       call set_boundary(Potential_C, Potential_G)

       PlotVar_VG = -1.
       PlotVar_VG(1,:,:,:) = Potential_G
       PlotVar_VG(2:4,1:nR+1,1:nTheta+1,1:nPhi+1) = B0_DF

       ! Note that the processor number here is changed. For redistribute.pl,
       ! the grid is ordered such that the first dimension is shifted first,
       ! then the second dimension, then the third. Here we are outputing a
       ! [r,t,p] grid that has the processor numbered as phi first, then theta.
       write(NameFile,'(a,2i2.2,a,i3.3,a)') &
            trim(NameFileGhostFace)//'_np01', nProcPhi, nProcTheta, '_', &
            iProcTheta + iProcPhi*nProcTheta, '.out'

       ! For redistribute.pl to work, the output must contain a line of
       ! parameters, so even if rMin and rMax are not quite needed here,
       ! we do need to include them here
       call save_plot_file(NameFile, TypeFileIn=TypeFileGhostFace, &
            StringHeaderIn = &
            'Potential field with ghost cells, face centered magnetic field', &
            nameVarIn = 'Radius Theta Phi Pot Br Btheta Bphi rMin rMax', &
            ParamIn_I = [ rMin, rMax ], nDimIn=3, Coord1In_I=Radius_I, &
            Coord2In_I=Theta_I(MinTheta:MaxTheta), &
            Coord3In_I=Phi_I(MinPhi:MaxPhi), &
            VarIn_VIII=PlotVar_VG(:,:,MinTheta:MaxTheta,MinPhi:MaxPhi))

       deallocate(Potential_G, PlotVar_VG)
    endif

  end subroutine save_potential_field
  !============================================================================

  subroutine set_boundary(x_C, x_G)

    use ModReadMagnetogram, ONLY: nPhiAll

    real, intent(in):: x_C(nR,nTheta,nPhi)
    real, intent(inout):: x_G(0:nR+1,0:nTheta+1,0:nPhi+1)

    integer:: iPhi, jPhi, nShift
    integer:: IntMpiStatus_I(mpi_status_size)
    integer:: IntSendRequest010, IntSendRequest020, IntSendRequest360, &
         IntSendRequest180, IntSendRequest12, IntSendRequest21, &
         IntSendRequest34, IntSendRequest43, &
         IntRecvRequest010, IntRecvRequest020, IntRecvRequest360, &
         IntRecvRequest180, IntRecvRequest12, IntRecvRequest21, &
         IntRecvRequest34, IntRecvRequest43
    integer:: jProc
    integer:: iError

    !--------------------------------------------------------------------------
    if (.not. allocated(TmpXPhi0_II)) allocate( &
         TmpXPhi0_II(0:nR+1,nPhiAll),              &
         TmpXPhipi_II(0:nR+1,nPhiAll),             &
         SendBC010_II(0:nR+1,nPhi),        &
         SendBC180_II(0:nR+1,nPhi),        &
         SendBC12_II(0:nR+1,0:nTheta+1),   &
         SendBC21_II(0:nR+1,0:nTheta+1),   &
         SendBC34_II(0:nR+1,nPhi),         &
         SendBC43_II(0:nR+1,nPhi),         &
         SendBC020_II(0:nR+1,0:nTheta+1),  &
         SendBC360_II(0:nR+1,0:nTheta+1),  &
         RecvBCLgr010_II(0:nR+1,nPhiLgr),       &
         RecvBCLgr180_II(0:nR+1,nPhiLgr),       &
         RecvBCSml010_II(0:nR+1,nPhiSml),       &
         RecvBCSml180_II(0:nR+1,nPhiSml),       &
         RecvBC12_II(0:nR+1,0:nTheta+1),   &
         RecvBC21_II(0:nR+1,0:nTheta+1),   &
         RecvBC34_II(0:nR+1,nPhi),         &
         RecvBC43_II(0:nR+1,nPhi),         &
         RecvBC020_II(0:nR+1,0:nTheta+1),  &
         RecvBC360_II(0:nR+1,0:nTheta+1))

    !--------------------------------------------------------------------------
    ! Current solution inside
    x_G(1:nR,1:nTheta,1:nPhi) = x_C

    ! The slope is forced to be Br at the inner boundary
    if(UseBr)then
       x_G(0,1:nTheta,1:nPhi) = x_C(1,:,:) - dRadiusNode_I(1)*BrLocal_II
    else
       x_G(0,1:nTheta,1:nPhi) = x_C(1,:,:)
    end if

    ! The potential is zero at the outer boundary
    x_G(nR+1,:,:) = -x_G(nR,:,:)
    ! wedge has four side boundaries, updated later in the side boundary part

    ! ----- pole ------
    if (.not. UseWedge) then
       ! Set TmpXPhi0_II and TmpXPhipi_II which used to store the global
       ! boundary close to the pole to the root
       if (iProcTheta ==0) then
          SendBC010_II = x_G(:,1,1:nPhi)
          call MPI_ISEND(SendBC010_II, (nR+2)*nPhi,MPI_REAL, 0, 010, &
               iComm, IntSendRequest010, iError)
       end if
       if (iProcTheta == nProcTheta-1) then
          SendBC180_II = x_G(:,nTheta,1:nPhi)
          call MPI_ISEND(SendBC180_II, (nR+2)*nPhi,MPI_REAL, 0, 180, &
               iComm, IntSendRequest180, iError)
       end if

       ! The root update TmpXPhi0_II/TmpXPhipi_II from all processors
       if (iProc == 0) then
          do jProc=0, nProcPhi-1
             if (jProc < nProcPhiLgr) then
                nShift = jProc * nPhiLgr

                call MPI_IRECV(RecvBCLgr010_II, (nR+2)*nPhiLgr, MPI_REAL, &
                     jProc, &
                     010, iComm, IntRecvRequest010, iError)
                call mpi_wait(IntRecvRequest010, IntMpiStatus_I, iError)
                TmpXPhi0_II(:, nShift + 1: nShift + nPhiLgr) = RecvBCLgr010_II

                call MPI_IRECV(RecvBCLgr180_II, (nR+2)*nPhiLgr, MPI_REAL, &
                     (nProcTheta-1)*nProcPhi+jProc, &
                     180, iComm, IntRecvRequest180, iError)
                call mpi_wait(IntRecvRequest180, IntMpiStatus_I, iError)
                TmpXPhipi_II(:, nShift + 1: nShift + nPhiLgr) = RecvBCLgr180_II
             else
                nShift = nProcPhiLgr*nPhiLgr + (jProc - nProcPhiLgr)*nPhiSml

                call MPI_IRECV(RecvBCSml010_II, (nR+2)*nPhiSml,  MPI_REAL, &
                     jProc, &
                     010, iComm, IntRecvRequest010, iError)
                call mpi_wait(IntRecvRequest010, IntMpiStatus_I, iError)
                TmpXPhi0_II(:, nShift + 1: nShift + nPhiSml) = RecvBCSml010_II

                call MPI_IRECV(RecvBCSml180_II , (nR+2)*nPhiSml,  MPI_REAL, &
                     (nProcTheta-1)*nProcPhi+jProc, &
                     180, iComm, IntRecvRequest180, iError)
                call mpi_wait(IntRecvRequest180, IntMpiStatus_I, iError)
                TmpXPhipi_II(:, nShift + 1: nShift + nPhiSml) = RecvBCSml180_II
             end if
          end do
       end if

       call  MPI_bcast(TmpXPhi0_II,  (nR+2)*nPhiAll, MPI_REAL, 0, iComm, iError)
       call  MPI_bcast(TmpXPhipi_II, (nR+2)*nPhiAll, MPI_REAL, 0, iComm, iError)

       ! Symmetric in Theta but shifted by nPhiAll/2, be careful about the shift
       if (iProcTheta == 0) then
          do iPhi = 1, nPhi
             jPhi = modulo(iPhi + iPhi0 - 1 + nPhiAll/2, nPhiAll) + 1
             x_G(:,0,iPhi)        = TmpXPhi0_II(:,jPhi)
          end do
       end if
       if (iProcTheta == nProcTheta-1) then
          do iPhi = 1, nPhi
             jPhi = modulo(iPhi + iPhi0 - 1 + nPhiAll/2, nPhiAll) + 1
             x_G(:,nTheta+1,iPhi) = TmpXPhipi_II(:,jPhi)
          end do
       end if

       ! ----- wedge theta outer boundary -----
    else
       if (iProcTheta == 0)             x_G(:,0,:)        = x_G(:,1,:)
       if (iProcTheta == nProcTheta-1)  x_G(:,nTheta+1,:) = x_G(:,nTheta,:)
    endif

    ! Update the local theta boundary
    if (iProcTheta /= nProcTheta-1) then
       SendBC34_II = x_G(:,nTheta,1:nPhi)
       call MPI_ISEND(SendBC34_II, (nR+2)*nPhi, MPI_REAL, &
            (iProcTheta+1)*nProcPhi+iProcPhi, &
            34, iComm, IntSendRequest34,  iError)
    end if
    if (iProcTheta /= 0) then
       SendBC43_II = x_G(:,1,1:nPhi)
       call MPI_ISEND(SendBC43_II, (nR+2)*nPhi, MPI_REAL, &
            (iProcTheta-1)*nProcPhi+iProcPhi, &
            43, iComm,  IntSendRequest43,  iError)
    end if
    if (iProcTheta /= nProcTheta-1) then
       call MPI_IRECV(RecvBC43_II, (nR+2)*nPhi, MPI_REAL, &
            (iProcTheta+1)*nProcPhi+iProcPhi, &
            43, iComm,  IntRecvRequest43, iError)
       call mpi_wait(IntRecvRequest43, IntMpiStatus_I, iError)
       x_G(:,nTheta+1,1:nPhi) = RecvBC43_II
    end if
    if (iProcTheta /= 0) then
       call MPI_IRECV(RecvBC34_II, (nR+2)*nPhi, MPI_REAL, &
            (iProcTheta-1)*nProcPhi+iProcPhi, &
            34, iComm,  IntRecvRequest34, iError)
       call mpi_wait(IntRecvRequest34, IntMpiStatus_I, iError)
       x_G(:,0,1:nPhi) = RecvBC34_II
    end if

    ! ----- phi wrap around -----
    if (.not. UseWedge) then
       ! Periodic around phi
       ! Send boundary info
       if (iProcPhi == 0) then
          SendBC020_II = x_G(:,:,1)
          call MPI_ISEND(SendBC020_II, (nR+2)*(nTheta+2), MPI_REAL, &
               iProcTheta*nProcPhi + nProcPhi-1, &
               020, iComm, IntSendRequest020, iError)
       end if
       if (iProcPhi == nProcPhi-1) then
          SendBC360_II = x_G(:,:,nPhi)
          call MPI_ISEND(SendBC360_II, (nR+2)*(nTheta+2), MPI_REAL, &
               iProcTheta*nProcPhi ,  &
               360, iComm, IntSendRequest360, iError)
       end if

       ! Update boundary info
       if (iProcPhi == 0) then
          call MPI_IRECV(RecvBC360_II, (nR+2)*(nTheta+2), MPI_REAL, &
               iProcTheta*nProcPhi + nProcPhi-1, &
               360, iComm, IntRecvRequest360, iError)
          call mpi_wait(IntRecvRequest360, IntMpiStatus_I, iError)
          x_G(:,:,0) = RecvBC360_II
       end if
       if (iProcPhi == nProcPhi-1) then
          call MPI_IRECV(RecvBC020_II, (nR+2)*(nTheta+2), MPI_REAL, &
               iProcTheta*nProcPhi , &
               020, iComm, IntRecvRequest020, iError)
          call mpi_wait(IntRecvRequest020, IntMpiStatus_I, iError)
          x_G(:,:,nPhi+1) = RecvBC020_II
       end if

       ! ----- wedge phi outer boundary -----
    else
       if (iProcPhi == 0)               x_G(:,:,0)        = x_G(:,:,1)
       if (iProcPhi == nProcPhi-1)      x_G(:,:,nPhi+1)   = x_G(:,:,nPhi)
    endif

    ! Start to send and update the local boundary
    if (iProcPhi /= nProcPhi-1) then
       SendBC12_II = x_G(:,:,nPhi)
       call MPI_ISEND(SendBC12_II, (nR+2)*(nTheta+2), MPI_REAL, &
            iProcTheta*nProcPhi + iProcPhi+1, &
            12,  iComm, IntSendRequest12, iError)
    end if
    if (iProcPhi /= 0) then
       SendBC21_II = x_G(:,:,1)
       call MPI_ISEND(SendBC21_II, (nR+2)*(nTheta+2), MPI_REAL, &
            iProcTheta*nProcPhi + iProcPhi-1, &
            21,  iComm, IntSendRequest21, iError)
    end if
    if (iProcPhi /= nProcPhi-1) then
       call MPI_IRECV(RecvBC21_II, (nR+2)*(nTheta+2), MPI_REAL, &
            iProcTheta*nProcPhi + iProcPhi+1, &
            21,  iComm, IntRecvRequest21, iError)
       call mpi_wait(IntRecvRequest21, IntMpiStatus_I, iError)
       x_G(:,:,nPhi+1) = RecvBC21_II
    end if
    if (iProcPhi /= 0) then
       call MPI_IRECV(RecvBC12_II, (nR+2)*(nTheta+2), MPI_REAL, &
            iProcTheta*nProcPhi + iProcPhi-1, &
            12,  iComm, IntRecvRequest12, iError)
       call mpi_wait(IntRecvRequest12, IntMpiStatus_I, iError)
       x_G(:,:,0) = RecvBC12_II
    end if

  end subroutine set_boundary
  !============================================================================

end module ModPotentialField
!==============================================================================
