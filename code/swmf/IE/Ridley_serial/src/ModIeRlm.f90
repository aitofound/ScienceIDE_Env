! Ridley Legacy Model for ionospheric conductance.
! This module is a semi-empirical model for ionospheric conductance.
! It takes FACs as input, fits a simple auroral oval to those currents,
! and employs an empirical fitting to convert FACS to conductance and
! precipitating fluxes.
! Included in this module is the CMEE improvements to the original
! RLM code.

! This model is outlined in Ridley et al., 2004 and detailed in
! Mukhopadhyay et al., 2020.

module ModIeRlm

  implicit none
  save

  ! Parameters to control Ridley Legacy conductance Model and derivatives:
  character(len=100) ::         & ! Location of conductance coeffs
       NameHalFile = 'default', &
       NamePedFile = 'default'
  real :: LatNoConductanceSI = 45.0, FactorHallCMEE = 7.5, FactorPedCMEE = 5.0

  ! Auroral oval options:
  logical :: &
       UseOval=.true.,         &  ! Apply oval?  CAUTION: Doesn't turn off oval!
       UseNewOval = .true.,    &  ! Use new oval calculation?
       DoOvalShift=.true.,     &  ! Shift oval along noon-midnight meridian?
       UseSubOvalCond=.false., &  ! Apply conductance at sub-aurora latitudes?
       DoFitCircle=.false.        ! Set shape of new oval to pure circle?

  ! RLM & CMEE conductance coefficient information:
  ! Coefficients for conductance based on FAC:
  real, allocatable, dimension(:,:) ::  &
       hal_a0_up,ped_a0_up,      &
       hal_a0_do,ped_a0_do,      &
       hal_a1_up,ped_a1_up,      &
       hal_a1_do,ped_a1_do,      &
       hal_a2_up,ped_a2_up,      &
       hal_a2_do,ped_a2_do
  ! Grid for conductance coefficients:
  integer :: i_cond_nmlts=-1, i_cond_nlats=-1
  real, allocatable :: cond_mlts(:)
  real, allocatable :: cond_lats(:)

contains
  !============================================================================
  subroutine load_conductances()

    use ModConductance, ONLY: NameAuroraMod
    use ModIoUnit, ONLY: UnitTmp_
    use ModUtilities, ONLY: open_file, close_file, CON_set_do_test, CON_stop

    ! Local variables:
    character (len=100) :: Line
    integer :: i, j, iError, nMltTemp=-1, nLatTemp=-1

    ! Testing variables:
    logical :: DoTest, DoTestMe

    character(len=*), parameter:: NameSub = 'load_conductances'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)

    ! Check if defaults have changed. If not, select appropriate input file.
    if (NameHalFile == 'default') then
        select case(trim(NameAuroraMod))
        case('RLM3', 'RLM4', 'RLM5')
            NameHalFile = 'cond_hal_coeffs.dat'
        case('CMEE')
            NameHalFile = 'cmee_hal_coeffs.dat'
        case('POWER')
            NameHalFile = 'cond_hal_coeffs_power.dat'
        end select
    end if
    ! Repeat for Pedersen conductance.
    if (NamePedFile == 'default') then
      select case(trim(NameAuroraMod))
      case('RLM3', 'RLM4', 'RLM5')
          NamePedFile = 'cond_ped_coeffs.dat'
      case('CMEE')
          NamePedFile = 'cmee_ped_coeffs.dat'
      case('POWER')
          NamePedFile = 'cond_ped_coeffs_power.dat'
      end select
    end if

    if(DoTest) then
        write(*,*)'IE: Empirical conductance coefficient files used:'
        write(*,*) NameHalFile
        write(*,*) NamePedFile
    end if

    ! Start with Hall Conductance:
    if(DoTest) write(*,*)NameSub//': Opening Hall cond. file '//NameHalFile
    call open_file(file='IE/'//NameHalFile, status="old", NameCaller=NameSub)

    ! Skip until DIMENSIONS are found:
    do
       ! Read line; break at EOF.
       read(UnitTmp_, *, iostat=iError) Line
       if (iError /= 0) EXIT
       ! Parse dimensions of arrays:
       if(index(Line,'#DIMENSIONS')>0) then
          read(UnitTmp_, *, iostat=iError) i_cond_nmlts
          read(UnitTmp_, *, iostat=iError) i_cond_nlats
          EXIT
       end if
    end do

    ! Check if dimensions found.  If not, stop program.
    if( (i_cond_nmlts==-1) .or. (i_cond_nlats==-1) ) call CON_stop(&
         NameSub//' Cannot find #DIMENSION in Hall conductance file.')

    if(DoTest)write(*,*) NameSub//': Size of conductance files (mlt, lat): ', &
         i_cond_nmlts, i_cond_nlats

    ! Allocate conductance arrays.  Include MLT ghost cell.
    ! Hall coefficients:
    allocate( hal_a0_up(i_cond_nmlts+1, i_cond_nlats) )
    allocate( hal_a1_up(i_cond_nmlts+1, i_cond_nlats) )
    allocate( hal_a2_up(i_cond_nmlts+1, i_cond_nlats) )
    allocate( hal_a0_do(i_cond_nmlts+1, i_cond_nlats) )
    allocate( hal_a1_do(i_cond_nmlts+1, i_cond_nlats) )
    allocate( hal_a2_do(i_cond_nmlts+1, i_cond_nlats) )
    ! Pedersen coefficients:
    allocate( ped_a0_up(i_cond_nmlts+1, i_cond_nlats) )
    allocate( ped_a1_up(i_cond_nmlts+1, i_cond_nlats) )
    allocate( ped_a2_up(i_cond_nmlts+1, i_cond_nlats) )
    allocate( ped_a0_do(i_cond_nmlts+1, i_cond_nlats) )
    allocate( ped_a1_do(i_cond_nmlts+1, i_cond_nlats) )
    allocate( ped_a2_do(i_cond_nmlts+1, i_cond_nlats) )
    ! Coefficient grids:
    allocate(cond_mlts(i_cond_nmlts+1), cond_lats(i_cond_nlats) )

    ! Read lines until #START:
    do
       read(UnitTmp_, *, iostat=iError) Line
       if (iError /= 0)           EXIT
       if(index(Line,'#START')>0) EXIT
    end do

    ! Read and load conductance coefficients & grid:
    do i=1, i_cond_nlats
       do j=1, i_cond_nmlts
          ! Parse a single line:
          read(UnitTmp_,*,iostat=iError) cond_lats(i), cond_mlts(j), &
               hal_a0_up(j,i), hal_a0_do(j,i), &
               hal_a1_up(j,i), hal_a1_do(j,i), &
               hal_a2_up(j,i), hal_a2_do(j,i)
          ! Stop code if IO error:
          if(iError/=0) then
             write(*,*)NameSub//': FILE ERROR at i,j = ', i, j
             call CON_stop(NameSub//': FILE ERROR for Hall input')
          end if
       end do
    end do

    ! Close Hall conductance file:
    call close_file(NameCaller=NameSub)

    if(DoTest)then
       ! Write out first and last conductances to screen for visual checking:
       write(*,*)NameSub//': Visual check for conductance coeff reading'
       j = 1
       i = 1
       write(*,*) 'At lat, lon = ', cond_lats(i), cond_mlts(j)
       write(*,'(a, 3(1x,E12.4))') '     HALL_UP A0, A1, A2 = ', &
            hal_a0_up(j,i), hal_a1_up(j,i), hal_a2_up(j,i)
       write(*,'(a, 3(1x,E12.4))') '     HALL_DO A0, A1, A2 = ', &
            hal_a0_do(j,i), hal_a1_do(j,i), hal_a2_do(j,i)

       j = i_cond_nmlts
       i = i_cond_nlats
       write(*,*) 'At lat, lon = ', cond_lats(i), cond_mlts(j)
       write(*,'(a, 3(1x,E12.4))') '     HALL_UP A0, A1, A2 = ', &
            hal_a0_up(j,i), hal_a1_up(j,i), hal_a2_up(j,i)
       write(*,'(a, 3(1x,E12.4))') '     HALL_DO A0, A1, A2 = ', &
            hal_a0_do(j,i), hal_a1_do(j,i), hal_a2_do(j,i)
    end if

    ! Load Pedersen Conductance:
    if(DoTest) write(*,*)NameSub//': Opening Pedersen cond. file '//NamePedFile
    call open_file(file='IE/'//NamePedFile, status="old", NameCaller=NameSub)

    ! Skip until DIMENSIONS are found
    do
       ! Read line; break at EOF.
       read(UnitTmp_, *, iostat=iError) Line
       if (iError /= 0) EXIT
       ! Parse dimensions of arrays:
       if(index(Line,'#DIMENSIONS')>0) then
          read(UnitTmp_, *, iostat=iError) nMltTemp
          read(UnitTmp_, *, iostat=iError) nLatTemp
          EXIT
       end if
    end do
    ! Check if dimensions found.  If not, stop program.
    if( (nLatTemp==-1) .or. (nMltTemp==-1) ) call CON_stop(&
         NameSub//' Cannot find #DIMENSION in Pedersen conductance file.')
    ! Check if match Hall file.
    if( (nLatTemp/=i_cond_nlats) .or. (nMltTemp/=i_cond_nmlts) ) call CON_stop(&
         NameSub//' Hall & Pedersen input file dimensions do not match.')

    ! Read lines until #START:
    do
       read(UnitTmp_, *, iostat=iError) Line
       if(iError /= 0)            EXIT
       if(index(Line,'#START')>0) EXIT
    end do

    ! Read and load conductance coefficients & grid:
    do i=1, i_cond_nlats
       do j=1, i_cond_nmlts
          ! Parse a single line:
          read(UnitTmp_,*,iostat=iError) cond_lats(i), cond_mlts(j), &
               ped_a0_up(j,i), ped_a0_do(j,i), &
               ped_a1_up(j,i), ped_a1_do(j,i), &
               ped_a2_up(j,i), ped_a2_do(j,i)
          ! Stop code if IO error:
          if(iError/=0) then
             write(*,*)NameSub//'FILE ERROR at i,j = ', i, j
             call CON_stop(NameSub//' FILE ERROR for Hall input')
          end if
       end do
    end do

    ! Close Pedersen conductance file:
    call close_file(NameCaller=NameSub)

    ! Wrap values around MLT 00 == 24:
    cond_mlts(i_cond_nmlts+1)   = cond_mlts(1)+24.0
    hal_a0_up(i_cond_nmlts+1,:) = hal_a0_up(1,:)
    ped_a0_up(i_cond_nmlts+1,:) = ped_a0_up(1,:)
    hal_a0_do(i_cond_nmlts+1,:) = hal_a0_do(1,:)
    ped_a0_do(i_cond_nmlts+1,:) = ped_a0_do(1,:)
    hal_a1_up(i_cond_nmlts+1,:) = hal_a1_up(1,:)
    ped_a1_up(i_cond_nmlts+1,:) = ped_a1_up(1,:)
    hal_a1_do(i_cond_nmlts+1,:) = hal_a1_do(1,:)
    ped_a1_do(i_cond_nmlts+1,:) = ped_a1_do(1,:)
    hal_a2_up(i_cond_nmlts+1,:) = hal_a2_up(1,:)
    ped_a2_up(i_cond_nmlts+1,:) = ped_a2_up(1,:)
    hal_a2_do(i_cond_nmlts+1,:) = hal_a2_do(1,:)
    ped_a2_do(i_cond_nmlts+1,:) = ped_a2_do(1,:)

    if(DoTest)then
       ! Write out first and last conductances to screen for visual checking:
       write(*,*)NameSub//': Visual check for conductance coeff reading'
       j = 1
       i = 1
       write(*,*) 'At lat, lon = ', cond_lats(i), cond_mlts(j)
       write(*,'(a, 3(1x,E12.4))') '     PEDER_UP A0, A1, A2 = ', &
            ped_a0_up(j,i), ped_a1_up(j,i), ped_a2_up(j,i)
       write(*,'(a, 3(1x,E12.4))') '     PEDER_DO A0, A1, A2 = ', &
            ped_a0_do(j,i), ped_a1_do(j,i), ped_a2_do(j,i)

       j = i_cond_nmlts
       i = i_cond_nlats
       write(*,*) 'At lat, lon = ', cond_lats(i), cond_mlts(j)
       write(*,'(a, 3(1x,E12.4))') '     PEDER_UP A0, A1, A2 = ', &
            ped_a0_up(j,i), ped_a1_up(j,i), ped_a2_up(j,i)
       write(*,'(a, 3(1x,E12.4))') '     PEDER_DO A0, A1, A2 = ', &
            ped_a0_do(j,i), ped_a1_do(j,i), ped_a2_do(j,i)
    end if

  end subroutine load_conductances
  !============================================================================

  subroutine Determine_Oval_Characteristics(Current_in, Theta_in, Psi_in, &
     Loc_of_Oval, Width_of_Oval, &
     Strength_of_Oval)

    ! This routine calculates everything in radians away from the pole.

    use ModIonosphere
    use IE_ModIo, ONLY: NameIonoDir
    use ModIoUnit, ONLY: UnitTMP_
    use IE_ModMain, ONLY: Time_Array, nSolve

    ! Inputs:
    real, dimension(1:IONO_nTheta, 1:IONO_nPsi), intent(in) :: &
         Current_in, Theta_in, Psi_in

    ! Outputs:
    real, dimension(1:IONO_nPsi), intent(out) :: &
         Loc_of_Oval, Width_of_Oval, Strength_of_Oval

    ! Working Variables:
    real, dimension(1:IONO_nTheta, 1:IONO_nPsi) :: &
         Current, Theta, Psi

    real, dimension(1:8) :: &
         max_fac, max_fac_colat, width, J_Save

    real    :: day_colat, dusk_colat, midnight_colat, dawn_colat
    real    :: day_fac, dusk_fac, midnight_fac, dawn_fac
    real    :: noon_mid, dusk_dawn, day_strength, night_strength
    real    :: mean_colat, dev_colat, sumFAC, Night_Width, Day_Width

    integer :: i, j, n, nloc, dJ, J_Start, J_End

    ! Testing & output variables:
    character(len=100), save    :: NameFile, StringFormat
    logical, save :: IsFirstWrite = .true.
    logical       :: DoTest, DoTestMe

    character(len=*), parameter:: NameSub = 'Determine_Oval_Characteristics'
    !--------------------------------------------------------------------------
    call CON_set_do_test(NameSub, DoTest, DoTestMe)

    ! Reverse the Arrays for Southern Hemisphere:
    if (Theta_in(1,1) < cHalfPi) then
       Current = Current_in
       Theta   = Theta_in
       Psi     = Psi_in
       north   = .true.
    else
       do i = 1, IONO_nTheta
          do j = 1, IONO_nPsi
             Current(IONO_nTheta - (i-1), j) = Current_in(i,j)
             Theta(IONO_nTheta - (i-1), j)   = cPi - Theta_in(i,j)
             Psi(IONO_nTheta - (i-1), j)     = Psi_in(i,j)
          enddo
       enddo
       north   = .false.
    endif

    ! Start oval determination:
     dJ = IONO_nPsi/9

     do n = 1, 8
        ! figure out location of auroral oval:
        ! Let's start a little ways away from the pole...
        nloc = 1
        J_Start = max(1,(n-1)*dJ)
        J_End   = min(n*dJ, IONO_nPsi)
        max_fac(n) = abs(Current(3,J_Start))
        max_fac_colat(n) = Theta(3,J_Start)
        J_Save(n) = J_Start
        do j = J_Start, J_End
           do i = 4, IONO_nTheta
              if (Current(i,j) > max_fac(n)) then
                 max_fac(n) = abs(Current(i,j))
                 max_fac_colat(n) = Theta(i,j)
                 J_Save(n) = j
                 nloc = i
              endif
           enddo
        enddo

        ! figure out width
        width(n) = 0.0
        j = J_Save(n)
        do i = nloc, IONO_nTheta
           if (Current(i,j) > max_fac(n)/4.0) then
              width(n) = abs(max_fac_colat(n) - Theta(i,j))
           endif
        enddo

        if (width(n) <= (theta(2,1)-theta(1,1))) &
             width(n) = max_fac_colat(n)/5.0

     enddo

     day_colat = (max_fac_colat(1) + max_fac_colat(8))/2.0
     dusk_colat = (max_fac_colat(2) + max_fac_colat(3))/2.0
     midnight_colat = (max_fac_colat(4) + max_fac_colat(5))/2.0
     dawn_colat = (max_fac_colat(6) + max_fac_colat(7))/2.0

     day_fac = (max_fac(1) + max_fac(8))/2.0
     dusk_fac = (max_fac(2) + max_fac(3))/2.0
     midnight_fac = (max_fac(4) + max_fac(5))/2.0
     dawn_fac = (max_fac(6) + max_fac(7))/2.0

     night_width = 0.0
     sumFAC = 0.0
     mean_colat = 0.0

     do n=1,8
        night_width = night_width + width(n) * max_fac(n)
        mean_colat = mean_colat + max_fac_colat(n) * max_fac(n)
        sumFAC = sumFAC + max_fac(n)
     enddo

     mean_colat = mean_colat/sumFAC
     Night_Width = Night_Width/sumFAC

     if (Night_Width > 6.0*cDegToRad) Night_Width=6.0*cDegToRad
     Day_Width = max(Night_Width/2.0,1.0*cDegToRad)

     if (mean_colat < 15.0*cDegToRad) then
        mean_colat = 15.0*cDegToRad
        dev_colat = 0.0
     else if(.not.DoOvalShift) then
        dev_colat = 0.0
     else
        dev_colat = ((day_colat - mean_colat) * day_fac - &
             (midnight_colat - mean_colat) * midnight_fac) / &
             (day_fac + midnight_fac)

        ! Restrict auroral location a little bit:
        if (abs(dev_colat) > mean_colat/2.0) then
           dev_colat = dev_colat*mean_colat/2.0/abs(dev_colat)
        endif
     endif

     Day_Strength = dawn_fac
     Night_Strength = sumFAC

     do j=1,IONO_nPsi
        Loc_of_Oval(j)   = mean_colat - dev_colat*cos(Psi(1,j))
        Width_of_Oval(j) = Day_Width + &
             (Night_Width - Day_Width)*sin(Psi(1,j)/2.0)
        Strength_of_Oval(j) = Day_Strength + &
             (Night_Strength - Day_Strength)*sin(Psi(1,j)/2.0)
     enddo

     ! For testing purposes, write oval info to file.
     if(.not.DoTestMe .or. .not.north) RETURN

     if(IsFirstWrite)then
        ! Open file:
        write(NameFile,'(a,i8.8,a)')trim(NameIonoDir)//'aurora_n',nSolve,'.txt'
        open(unit=UnitTmp_, file=NameFile, status='replace')
        ! Write header w/ longitudes:
        write(UnitTmp_, '(a)', advance='NO')'Auroral oval location at Lon='
        do j=1, IONO_nPsi
           write(UnitTmp_,'(1x,f6.2)', advance='NO') Psi(1,j)*cRadToDeg
        end do
        write(UnitTmp_,'(a)')'','YYYY MM DD HH MN SS msc oval_colat'

        ! Set format codes for writing output:
        write(StringFormat,'("(i4,5i3,i4,", i4,"(1x,f6.2))")') IONO_nPsi
        IsFirstWrite=.false.
     else
        ! Open file in append mode:
        open(unit=UnitTmp_, file=NameFile, status='old', position='append')
     end if

     ! Write record and close.
     write(UnitTmp_, StringFormat) Time_Array(1:7), Loc_of_oval(:)*cRadToDeg
     close(UnitTmp_)

  end subroutine Determine_Oval_Characteristics
  !============================================================================

end module ModIeRlm
!==============================================================================
