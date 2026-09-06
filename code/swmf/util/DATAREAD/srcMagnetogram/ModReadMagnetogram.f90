!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf

module ModReadMagnetogram

  ! Read the raw magnetogram and modify the field as needed.

  use ModNumConst
  use ModUtilities, ONLY: CON_stop
  use ModConst, ONLY: cHalfPi, cPi, cTwoPi

  implicit none

  private ! except

  public:: read_magnetogram_param ! reads the input parameters
  public:: read_orig_magnetogram  ! reads and modifies the magnetogram

  real,    public, allocatable:: Br0_II(:,:) ! Final Magnetic field
  integer, public:: nThetaAll, nPhiAll    ! Size of remeshed grid for FDIPS
  integer, public:: nTheta0, nPhi0        ! size passed to harmonics & fdips
  integer, public:: nThetaorig, nPhiorig  ! original magnetogram theta and phi
  logical, public:: UseCosTheta = .true.  ! To check the latitude grid
  real,    public:: dPhi=1.0, dTheta=1.0, dSinTheta=1.0
  logical, public:: UseChebyshevNode = .true.
  real,    public,  allocatable:: ChebyshevWeightE_I(:), ChebyshevWeightW_I(:)

  ! Carrington rotation # plus  fraction of the synodic Carrington rotation
  ! period passed since the CR start till the time of magnetogram.
  ! For a particular case of the GONG synoptic map this equals CR#+0.5.
  ! For a particular case of the GONG hourly magnetogram, this parameter
  ! equals CRNOW as provided in the fitsfile header. Once read from the
  ! magnetogram file, this parameter is converted to the fromat similar to
  ! #STARTTIME and saved to MAGNETOGRAMTIME.in file. It is strongly
  ! recommended to check if this saved time looks reasonable
  real, public:: MagnetogramTimeCR = 0.

  ! Carrington longitude of the left margin of the map ("leading longitude")
  real, public:: LongShift = 0.

  ! Phi, Lat passed to FDIPS
  real, public, allocatable:: Phi0_I(:), Lat0_I(:)

  ! Header
  character(len=500), public :: StringMagHeader

  ! Input parameters
  character (len=100):: NameFileIn = 'fitsfile.out'
  real:: BrMax = 3500.0

  ! Optional enhancement of the polar magnetic field with a factor
  !  1 + (PolarFactor-1)*abs(sin(Latitude))^PolarExponent
  logical           :: DoChangePolarField = .false.
  real              :: PolarFactor = 1.0
  real              :: PolarExponent = 2.0

  ! Parameters used to apply modified scaling to the raw magnetogram
  ! Apply a scaling factor to small magnetic fields, to compensate
  ! the measurement error for the coronal hole (=very low) field
  logical :: DoChangeWeakField = .false.
  real    :: BrFactor = 1.0, BrMin = 0.0

  ! To indicate if fdips is done or harmonics
  logical :: IsFdips = .false.
  logical :: UseMagnetogramDate = .false.

contains
  !============================================================================

  subroutine read_magnetogram_param(NameCommand)

    use ModReadParam, ONLY: read_var

    character(len=*), intent(in):: NameCommand
    character(len=*), parameter:: NameSub = 'read_magnetogram_param'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case("#MAGNETOGRAMFILE")
       call read_var('NameFileIn', NameFileIn)
       call read_var('BrMax',      BrMax)
    case("#CHANGEPOLARFIELD")
       DoChangePolarField = .true.
       call read_var('PolarFactor',   PolarFactor)
       call read_var('PolarExponent', PolarExponent)
    case("#CHANGEWEAKFIELD")
       DoChangeWeakfield = .true.
       call read_var('BrFactor', BrFactor)
       call read_var('BrMin', BrMin)
    case("#CHEBYSHEV", "#UNIFORMTHETA")
       call read_var('UseChebyshevNode', UseChebyshevNode)
    case('#USEMAGNETOGRAMDATE')
       call read_var('UseMagnetogramDate', UseMagnetogramDate)
    case default
       call CON_stop(NameSub//': unknown command='//trim(NameCommand))
    end select

  end subroutine read_magnetogram_param
  !============================================================================

  subroutine read_orig_magnetogram(&
       IsPhiThetaOrder, UseWedge, DoRemoveMonopole, nThetaCoarse, nPhiCoarse)

    use ModPlotFile, ONLY: read_plot_file, save_plot_file
    use ModConst, ONLY: cDegToRad, tStartCarringtonRotation, &
         CarringtonSynodicPeriod
    use ModTimeConvert, ONLY: time_real_to_int, time_int_to_real
    use ModIoUnit, ONLY: UnitTmp_
    use ModUtilities,  ONLY: open_file, close_file, cTab

    logical, optional,  intent(in):: IsPhiThetaOrder
    logical, optional,  intent(in):: UseWedge
    logical, optional,  intent(in):: DoRemoveMonopole
    integer, optional,  intent(in):: nThetaCoarse, nPhiCoarse

    real:: Param_I(2)
    real:: BrAverage
    real, allocatable :: BrTmp_II(:,:), BrTrans_II(:,:), Var_VII(:,:,:)
    real :: MagnetogramTime, CRFraction
    integer :: iTime_I(7), iPos=0

    integer:: iError, nParam, iTheta, iPhi, nThetaRatio, nPhiRatio
    integer:: iTheta0, iTheta1, jPhi0, jPhi1, jPhi, nVar

    logical :: IsInputLatReverse = .false.
    logical :: IsRemoveMonopole = .true., DoEliminateCmeBr = .false.
    character(len=200) :: NameVarOut

    character(len=*), parameter:: NameSub = 'read_orig_magnetogram'
    !--------------------------------------------------------------------------
    call read_plot_file(NameFileIn, StringHeaderOut=StringMagHeader, &
         n1Out = nPhi0, n2Out = nTheta0, nVarOut = nVar, &
         ParamOut_I=Param_I, iErrorOut=iError, nParamOut=nParam, &
         TimeOut=CRFraction)

    if(iError /= 0) call CON_stop(NameSub// &
         ': could not read header from file '//trim(NameFileIn))

    ! Reading the original shift in Phi and
    ! Central Meridian Longitude from the Map
    if(nParam>0) LongShift = Param_I(1)
    if(nParam>1) MagnetogramTimeCR = Param_I(2) + CRFraction

    write(*,*) NameSub, &
         ': nTheta0, nPhi0, LongitudeShift, MagnetogramTimeCR = ', &
         nTheta0, nPhi0, LongShift, MagnetogramTimeCR
    ! Saves the original Magnetogram grid
    allocate(Phi0_I(nPhi0), Lat0_I(nTheta0))
    allocate(Br0_II(nPhi0,nTheta0), Var_VII(nVar,nPhi0,nTheta0))

    call read_plot_file(NameFileIn, &
         Coord1Out_I=Phi0_I, Coord2Out_I=Lat0_I, VarOut_VII = Var_VII, &
         iErrorOut=iError, NameVarOut=NameVarOut)

    if(iError /= 0) call CON_stop(NameSub// &
         ': could not read data from file '//trim(NameFileIn), iError)
    Br0_II = Var_VII(1,:,:); DoEliminateCmeBr = index(NameVarOut,'CmeBr') > 0
    if(MagnetogramTimeCR > 0.0)then
       if(UseMagnetogramDate)then
          ! Use MagnetogramDate field from the header to assign
          ! magnetogram time
          write(*,*)StringMagHeader
          iPos = index(StringMagHeader, 'MagnetogramDate = ')
          ! Skip 'MagnetogramDate = '
          iPos = iPos + 17
          if(iPos > 0) then
             iTime_I=0
             ! read YYYY-MM-DD"T"HH-MM-SS
             read(StringMagHeader(iPos+1:iPos+4),*) iTime_I(1)
             read(StringMagHeader(iPos+6:iPos+7),*) iTime_I(2)
             read(StringMagHeader(iPos+9:iPos+10),*) iTime_I(3)
             read(StringMagHeader(iPos+12:iPos+13),*) iTime_I(4)
             read(StringMagHeader(iPos+15:iPos+16),*) iTime_I(5)
             read(StringMagHeader(iPos+18:iPos+19),*) iTime_I(6)
             call time_int_to_real(iTime_I, MagnetogramTime)
             MagnetogramTimeCR = (MagnetogramTime - &
                  tStartCarringtonRotation)/CarringtonSynodicPeriod
          end if
       end if
       MagnetogramTime = MagnetogramTimeCR*CarringtonSynodicPeriod &
            + tStartCarringtonRotation
       call time_real_to_int(MagnetogramTime, iTime_I)
       call open_file(FILE='MAGNETOGRAMTIME.in', NameCaller=NameSub)
       write(UnitTmp_,'(i8,a)')iTime_I(1), cTab//cTab//'iYear'
       write(UnitTmp_,'(i8,a)')iTime_I(2), cTab//cTab//'iMonth'
       write(UnitTmp_,'(i8,a)')iTime_I(3), cTab//cTab//'iDay'
       write(UnitTmp_,'(i8,a)')iTime_I(4), cTab//cTab//'iHour'
       write(UnitTmp_,'(i8,a)')iTime_I(5), cTab//cTab//'iMinute'
       write(UnitTmp_,'(i8,a)')iTime_I(6), cTab//cTab//'iSecond'
       write(UnitTmp_,'(a)')'0.0'//cTab//cTab//cTab//'FracSecond'
       write(UnitTmp_,'(a)')'#END'
       write(UnitTmp_,*)
       call close_file(NameCaller=NameSub)
    end if

    ! Check if the latitude coordinate is uniform or not
    ! There is no point using Chebyshev transform if the original grid
    ! is already uniform in theta
    UseCosTheta = abs(Lat0_I(3) - 2*Lat0_I(2) + Lat0_I(1)) > 1e-6
    if(.not.UseCosTheta)then
       if(UseChebyshevNode) write(*,*) &
            'Already uniform in theta, Chebyshev transform is not needed'
       UseChebyshevNode = .false.
    endif

    IsInputLatReverse = Lat0_I(1) > Lat0_I(nTheta0)

    ! For #CHANGEPOLARFIELD
    if(DoChangePolarField)then
       ! Add and then subtract CmeBr field, if needed
       if(DoEliminateCmeBr)Br0_II = Br0_II - Var_VII(nVar,:,:)
       do iTheta = 1, nTheta0
          Br0_II(:,iTheta) = Br0_II(:,iTheta) * (1 + &
               (PolarFactor-1)*abs(sin(cDegToRad*Lat0_I(iTheta)))&
               **PolarExponent)
       end do
       write(StringMagHeader,'(a,f4.1,a,f4.1)')trim(StringMagHeader)//&
            '; PolarFactor = ', PolarFactor,'; PolarExponent = ', PolarExponent
       if(DoEliminateCmeBr)Br0_II = Br0_II + Var_VII(nVar,:,:)
    end if

    ! options not supported with UseWedge
    if(present(UseWedge))then
       if(DoChangePolarField .and. UseWedge) &
            call CON_stop('UseWedge does not support DoChangePolarField.')
       if(UseCosTheta .and. UseWedge) &
            call CON_stop(NameSub//&
               ': UseWedge only works with uniform latitude grid')

       if(present(DoRemoveMonopole)) &
            IsRemoveMonopole = DoRemoveMonopole .and. (.not. UseWedge)
    endif

    ! For #CHANGEWEAKFIELD
    if(DoChangeWeakField)then
       ! Add and then subtract CmeBr field, if needed
       if(DoEliminateCmeBr)Br0_II = Br0_II - Var_VII(nVar,:,:)
       if(BrMin > 0.0 .or. BrFactor > 1.0) &
            Br0_II = sign(min(abs(Br0_II) + BrMin, &
            BrFactor*abs(Br0_II)), Br0_II)
       write(StringMagHeader,'(a,f4.1,a,f4.1)')trim(StringMagHeader)//&
            '; BrFactor =', BrFactor, '; BrMin = ', BrMin
       if(DoEliminateCmeBr)Br0_II = Br0_II + Var_VII(nVar,:,:)
    endif

    ! Fix too large values of Br
    where (abs(Br0_II) > BrMax) Br0_II = sign(BrMax, Br0_II)

    ! Done for both Harmonics & Fdips unless specified otherwise
    if(IsRemoveMonopole) then
       if (UseCosTheta) then
          BrAverage = sum(Br0_II)/(nTheta0*nPhi0)
       else
          BrAverage = 0.0
          do iTheta = 1, nTheta0
             BrAverage = BrAverage + &
                  sum(Br0_II(:,nTheta0+1-iTheta)) * &
                  cos(cDegToRad*Lat0_I(nTheta0+1-iTheta))
          enddo
          BrAverage = BrAverage / (nTheta0*nPhi0)
       endif
       Br0_II = Br0_II - BrAverage
       write(*,*)NameSub,': Removing BrAverage =', BrAverage
    endif

    ! Save 2D Br in the original magnetogram grid
    ! call save_plot_file('field_2d.out', TypeFileIn='ascii',&
    !     StringHeaderIn='Longitude, Latitude [Deg], Br[G]', &
    !     NameVarIn='Longitude Latitude Br',&
    !     VarIn_II=Br0_II, &
    !     Coord1In_I=Phi0_I, &
    !     Coord2In_I=Lat0_I)

    ! to be passed to harmonics before remeshing
    nThetaorig = nTheta0
    nPhiorig   = nPhi0

    dPhi      = cTwoPi/nPhi0
    dTheta    = cPi/nTheta0
    dSinTheta = 2.0/nTheta0

    if(present(nThetaCoarse) .and. present(nPhiCoarse))then
       IsFdips = .true.
    else if(UseChebyshevNode) then
       ! For Harmonics Only
       call uniform_theta_transform
    endif

    ! To be done for FDIPS only
    ! Step 1 : Coarsening for FDIPS based on Input Theta & Phi
    if(IsFdips)then
       if(nThetaorig > nThetaCoarse .and. nThetaCoarse > 1)then
          if(modulo(nThetaorig, nThetaCoarse) /= 0)then
             write(*,*) NameSub,' nTheta in file    =', nThetaorig
             write(*,*) NameSub,' nTheta in FDIPS.in=', nThetaCoarse
             call CON_stop(NameSub//': not an integer coarsening ratio')
          end if
          ! Set integer coarsening ratio
          nThetaRatio = nThetaorig / nThetaCoarse
          nThetaAll   = nThetaorig / nThetaRatio
       else
          nThetaRatio = 1
          nThetaAll   = nThetaorig
       end if

       if(nPhiorig > nPhiCoarse .and. nPhiCoarse > 1)then
          if(modulo(nPhiorig, nPhiCoarse) /= 0)then
             write(*,*) NameSub,' nPhi in file    =', nPhiorig
             write(*,*) NameSub,' nPhi in FDIPS.in=', nPhiCoarse
             call CON_stop(NameSub//': not an integer coarsening ratio')
          end if
          nPhiRatio = nPhiorig / nPhiCoarse
          nPhiAll   = nPhiorig / nPhiRatio
       else
          nPhiRatio = 1
          nPhiAll   = nPhiorig
       end if

       allocate(BrTmp_II(nPhiAll,nThetaAll))
       BrTmp_II=0.0

       do iPhi = 1, nPhiAll
          jPhi0 = nPhiRatio*(iPhi-1) + 1
          jPhi1 = jPhi0 + nPhiRatio -1

          do jPhi = jPhi0, jPhi1
!!! SHOULD USE CELL AREA ???!!!
             do iTheta = 1, nThetaAll
                iTheta0 = nThetaRatio*(iTheta-1) + 1
                iTheta1 = iTheta0 + nThetaRatio - 1
                BrTmp_II(iPhi,iTheta) = BrTmp_II(iPhi,iTheta) &
                     + sum( Br0_II(jPhi,iTheta0:iTheta1))
             end do
          end do
       end do
       BrTmp_II = BrTmp_II / (nThetaRatio*nPhiRatio)
       if(allocated(Br0_II)) deallocate(Br0_II)
       allocate(Br0_II(nPhiAll,nThetaAll))
       Br0_II = BrTmp_II
       deallocate(BrTmp_II)

       ! Save the 2D field after coarsening
       call save_plot_file('fdips_2d.out', TypeFileIn='ascii',&
            StringHeaderIn='Longitude, Latitude [Deg], Br[G]', &
            NameVarIn='Longitude Latitude Br',&
            VarIn_II=Br0_II, &
            CoordMinIn_D = [cPi/nPhiAll, -cHalfPi+cHalfPi/nThetaAll], &
            CoordMaxIn_D = [cTwoPi-cPi/nPhiAll, cHalfPi- cHalfPi/nThetaAll])

       ! Br0_II is phi,theta till here and not reverse
       if(UseChebyshevNode)then
          write(*,*)'Doing Uniform Theta Transform for FDIPS'
          call uniform_theta_transform
          UseCosTheta = .false.
          nThetaAll = nTheta0
          nPhiAll = nPhi0
       endif
       ! For FDIPS ONLY, transpose
       allocate(BrTrans_II(nThetaAll,nPhiAll))
       if(present(IsPhiThetaOrder)) then
          if(.not. IsPhiThetaOrder)then
             if (IsInputLatReverse) then
                BrTrans_II = transpose(Br0_II)
             else
                BrTrans_II = transpose(Br0_II(:,nThetaAll:1:-1))
             endif
          endif
       endif
       if(allocated(Br0_II)) deallocate(Br0_II)
       allocate(Br0_II(nThetaAll,nPhiAll))
       Br0_II = BrTrans_II
       ! Br0_II is now (Theta, Phi) not lon,Lat
       deallocate(BrTrans_II)
    endif

  end subroutine read_orig_magnetogram
  !============================================================================
  subroutine uniform_theta_transform

    ! The magnetogram is remeshed onto a uniform in theta (co-latitude) grid.
    ! This corresponds to the Chebyshev points with respect to cos theta,
    ! which is the argument of the associated Legendre polynomials.
    ! The equal spacing in the theta direction makes the calculation much
    ! more accurate, especially in the polar regions.
    ! The number of points in the theta direction of the remeshed grid
    ! is always an odd number.

    integer :: nThetaIn, nThetaOut, iW, iU, iTheta, iPhi
    integer :: iLower, iUpper
    real    :: dThetaChebyshev, dThetaInterpolate, BrSouthPole, BrNorthPole
    real, allocatable :: ThetaIn_I(:), ThetaOut_I(:), NewBr_II(:,:), &
         ChebyshevWeightEu_I(:), Br_II(:,:)

    character(len=*), parameter:: NameSub = 'uniform_theta_transform'
    !--------------------------------------------------------------------------
    write(*,*) ' Remeshing to Uniform Theta'

    if(IsFdips)then
       nTheta0   = nThetaAll
       nPhi0     = nPhiAll
       dPhi      = cTwoPi/nPhi0
       dTheta    = cPi/nTheta0
       dSinTheta = 2.0/nTheta0
       write(*,*)'IsFdips, nTheta0, nPhi0',IsFdips, nTheta0, nPhi0
    endif

    nThetaIn = nTheta0
    nThetaOut = ceiling(nThetaIn * cHalfPi)

    ! The number of points in theta direction should be odd
    if (modulo(nThetaOut,2) == 0) nThetaOut = nThetaOut+1

    write(*,*) 'Original nThetaIn      =', nThetaIn
    write(*,*) 'nThetaOut=nThetaIn*Pi/2=', nThetaOut

    ! Note that the indices start from 0 for both FDIPS & Harmonics
    ! Lat grid to Theta grid
    allocate(NewBr_II(0:nPhi0-1,0:nThetaOut-1))
    allocate(ThetaIn_I(0:nThetaIn-1))
    allocate(ThetaOut_I(0:nThetaOut-1))

    do iTheta = 0, nThetaIn-1
       if(UseCosTheta)then
          ThetaIn_I(iTheta) = cHalfPi - asin((iTheta+0.5)*dSinTheta - 1)
       else
          call CON_stop(NameSub//': UseCosTheta=F should not happen here')
          ThetaIn_I(iTheta) = cHalfPi - ((iTheta + 0.5)*dTheta - cHalfPi)
       end if
    end do

    dThetaChebyshev = cPi/(nThetaOut-1)

    ! +90 is 0 deg Theta
    do iTheta = 0, nThetaOut-1
       ThetaOut_I(iTheta) = cPi - iTheta*dThetaChebyshev
    end do

    iLower = -1
    iUpper = -1
    NewBr_II = 0

    ! Allocate a new magnetic field variable which has first index as 0
    ! common for both FDIPS and Harmonics
    allocate(Br_II(0:nPhi0-1,0:nTheta0-1))
    Br_II = Br0_II

    ! Still in PhiTheta order,
    ! Assumes that the Lat(0) is -90 (south pole)
    BrNorthPole = sum(Br_II(:,nThetaIn-1))/nPhi0
    BrSouthPole = sum(Br_II(:,0))/nPhi0

    ! Use linear interpolation to do the data remesh
    do iTheta=0, nThetaOut-1
       iUpper = floor((cos(ThetaOut_I(iTheta)) - cos(ThetaIn_I(0)))/dSinTheta)
       iLower = iUpper+1
       do iPhi = 0, nPhi0-1
          if (iUpper /= -1 .and. iUpper /= nThetaIn-1 ) then
             dThetaInterpolate = ThetaIn_I(iUpper) - ThetaIn_I(iLower)
             NewBr_II(iPhi,iTheta) = Br_II(iPhi,iLower)* &
                  (ThetaIn_I(iUpper)-ThetaOut_I(iTheta))/dThetaInterpolate &
                  +Br_II(iPhi,iUpper)* &
                  (ThetaOut_I(iTheta)-ThetaIn_I(iLower))/dThetaInterpolate
          else if (iUpper == nThetaIn-1) then
             dThetaInterpolate = ThetaIn_I(nThetaIn-1)
             NewBr_II(iPhi,iTheta) = BrNorthPole &
                  *(ThetaIn_I(nThetaIn-1)-ThetaOut_I(iTheta)) &
                  /dThetaInterpolate &
                  + Br_II(iPhi,nThetaIn-1) &
                  *(ThetaOut_I(iTheta))/dThetaInterpolate
          else
             dThetaInterpolate = cPi - ThetaIn_I(0)
             NewBr_II(iPhi,iTheta) = Br_II(iPhi,0)* &
                  (cPi - ThetaOut_I(iTheta))/dThetaInterpolate &
                  +BrSouthPole* &
                  (ThetaOut_I(iTheta) - ThetaIn_I(0))/dThetaInterpolate
          end if
       end do
    end do

    ! Copying the remeshed grid after interpolation into the previous theta
    ! Copy the remeshed grid size into nTheta
    nTheta0 = nThetaOut

    ! Copy the remeshed NewBr_II into Br0_II. Br0_II indices begin from 1
    if(allocated(Br0_II)) deallocate(Br0_II)
    allocate(Br0_II(1:nPhi0,1:nTheta0))
    Br0_II = NewBr_II

    ! Calculate the weights for the transformation in theta direction.
    allocate(ChebyshevWeightE_I(0: nThetaOut-1))
    allocate(ChebyshevWeightW_I(0: nThetaOut-1))
    allocate(ChebyshevWeightEu_I(0:(nThetaOut-1)/2))

    ChebyshevWeightW_I    = 0.0
    ChebyshevWeightE_I    = 1.0
    ChebyshevWeightE_I(0) = 0.5
    ChebyshevWeightE_I(nThetaOut-1) = 0.5

    ChebyshevWeightEu_I    = 1.0
    ChebyshevWeightEu_I(0) = 0.5
    ChebyshevWeightEu_I((nThetaOut-1)/2) = 0.5

    do iW = 0, nThetaOut-1
       do iU = 0, (nThetaOut-1)/2
          ChebyshevWeightW_I(iW) = ChebyshevWeightW_I(iW) + &
               ChebyshevWeightEu_I(iU)*(-2.0)/(4*(iU)**2 - 1)* &
               cos(iW*iu*cPi/((nThetaOut-1)/2))
       end do
    end do
    ChebyshevWeightW_I = ChebyshevWeightW_I/(nThetaOut-1)

    deallocate(NewBr_II, ThetaIn_I, ThetaOut_I, ChebyshevWeightEu_I)

  end subroutine uniform_theta_transform
  !============================================================================

end module ModReadMagnetogram
!==============================================================================
