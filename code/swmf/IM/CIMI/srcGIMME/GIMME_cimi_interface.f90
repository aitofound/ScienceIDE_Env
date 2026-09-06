module GIMME_cimi_interface

  use GIMME_potential, ONLY: get_gimme_potential
  use GIMME_iono_grid, ONLY: &
       TimeSimulation, init_grid_iono,UseFullSphere, iStartTime_I, &
       nTheta, nPhi, Theta_G, Phi_G, Potential_G
  use GIMME_mag_input, ONLY: Jr_C, iThetaBC_I, PotBC_C, init_mag_input
  use GIMME_conductance, ONLY: &
       init_conductance, set_conductance, calc_coefficients
  use GIMME_plots, ONLY: gimme_plot
  use ModNumConst, ONLY: cPi, cDegToRad, cRadToDeg
  use ModInterpolate, ONLY: bilinear, linear
  use ModIoUnit, ONLY: UnitTmp_
  use ModUtilities, ONLY: open_file, close_file

  implicit none
  private ! except

  ! cimi Latitude (radians) and lon (radians) grid
  real, allocatable :: LatCimi_C(:),LonCimi_C(:)
  integer :: nLatCIMI, nLonCIMI

  ! cimi lat boundary (this is iba in CIMI)
  integer, allocatable :: iLatBcCimi_I(:)

  ! cimi potential on cimi grid [V]
  real, allocatable :: PotCIMI_C(:,:)

  ! FAC from CIMI
  real, allocatable:: JrCIMI_C(:,:)

  ! cimi potential on gimme grid [V]
  real, allocatable:: PotCimiOnGimmeGrid_C(:,:)
  real, allocatable:: JrCimiOnGimmeGrid_C(:,:)
  integer, allocatable:: iThetaBcOnGimme_I(:)

  ! gimme potential on cimi grid
  real, allocatable:: PotGimmeOnCimiGrid_C(:,:)

  ! gimme grid parameters for this module
  real, allocatable:: LatGimme(:,:), LonGimme(:,:)

  ! how often to save output
  real, public:: DtGimmePlot = 60.0

  ! if Gimme is used
  logical, public:: UseGimme=.false.

  ! public methods
  public:: init_gimme_from_cimi
  public:: gimme_potential_to_cimi

contains
  !============================================================================
  subroutine init_gimme_from_cimi(iStartTimeIn_I,&
       nLatCimiIn,nLonCimiIn,LatCimiIn_C,LonCimiIn_C)

    ! recv CIMI time and grid at initialization for later interpolation
    integer, intent(in) :: iStartTimeIn_I(7)
    integer, intent(in) :: nLatCimiIn,nLonCimiIn
    real   , intent(in) :: LatCimiIn_C(nLatCimiIn)
    real   , intent(in) :: LonCimiIn_C(nLonCimiIn)
    ! save start time from CIMI as GIMME start time
    !--------------------------------------------------------------------------
    iStartTime_I = iStartTimeIn_I

    ! allocate CIMI grid in module and save input CIMI grid values
    nLatCIMI = nLatCimiIn
    nLonCIMI = nLonCimiIn

    allocate(LatCimi_C(nLatCIMI))
    allocate(LonCimi_C(nLonCIMI))

    ! allocate array to hold CIMI lat boundary (iba)
    allocate(iLatBcCimi_I(nLonCimi))

    LatCimi_C = LatCimiIn_C
    LonCimi_C = LonCimiIn_C

    ! allocate arrays that hold data from cimi
    allocate(PotCIMI_C(nLatCIMI,nLonCIMI))
    allocate(JrCIMI_C(nLatCIMI,nLonCIMI))

    UseFullSphere=.false.

    ! initialize routines in gimme
    call init_grid_iono
    call init_conductance
    call init_mag_input

    ! allocate arrays for holding CIMI values on GIMME grid
    allocate(PotCimiOnGimmeGrid_C(nTheta,nPhi))
    allocate(JrCimiOnGimmeGrid_C(nTheta,nPhi))
    allocate(iThetaBcOnGimme_I(nPhi))

    ! allocate arrays for holding interpolated feedback
    allocate(PotGimmeOnCimiGrid_C(nLatCIMI,nLonCIMI))

  end subroutine init_gimme_from_cimi
  !============================================================================
  subroutine gimme_potential_to_cimi(TimeSimulationIn,&
       nLatCimiIn, nLonCimiIn, &
       PotCimiIn_C,JrCimiIn_C, iLatBcCimiIn_I,DtCimi)

    ! Takes TimeSimulation and CIMI Jr and potential, calculates potential in
    ! GIMME, interpolates potnetial onto CIMI grid and returns

    real,    intent(in)    :: TimeSimulationIn
    integer, intent(in)    :: nLatCimiIn, nLonCimiIn
    real,    intent(inout) :: PotCimiIn_C(nLatCimi,nLonCimi)
    real,    intent(in)    :: JrCimiIn_C(nLatCimi,nLonCimi)
    integer, intent(in)    :: iLatBcCimiIn_I(nLonCimi)
    real,    intent(in)    :: DtCimi

    ! set GIMME simulation time to match CIMI
    !--------------------------------------------------------------------------
    TimeSimulation=TimeSimulationIn

    ! set PotCIMI_C and JrCIMI_C to match incomming values from CIMI
    ! note cimi Jr is opposite of what GIMME expects
    PotCIMI_C=PotCimiIn_C
    JrCIMI_C=-1.0*JrCimiIn_C

    ! save incomming lat boundary (iba)
    iLatBcCimi_I=iLatBcCimiIn_I

    ! interpolate CIMI potential and currents to GIMME grid
    call interpolate_cimi_to_gimme

    ! set Jr_C to JrCimiOnGimmeGrid_C, and set BCs
    Jr_C=JrCimiOnGimmeGrid_C
    PotBC_C=PotCimiOnGimmeGrid_C
    iThetaBC_I=iThetaBcOnGimme_I

    ! set the conductance values
    call set_conductance

    ! update the conductance coef for the solver
    call calc_coefficients

    ! get the potential
    call get_gimme_potential

    ! interpolate GIMME potential back to CIMI grid
    call interpolate_gimme_to_cimi

    ! set value to pass back
    PotCimiIn_C = PotGimmeOnCimiGrid_C
    ! call test_input

    ! Save GIMME output if time is correct
    if( DtGimmePlot>0.0 .and. &
         ( floor( ( TimeSimulation + 1.0e-5 ) / DtGimmePlot ) ) /= &
         floor( ( TimeSimulation + 1.0e-5 - DtCimi) / DtGimmePlot ) ) then
       call gimme_plot
    endif

  end subroutine gimme_potential_to_cimi
  !============================================================================
  subroutine interpolate_cimi_to_gimme

    integer :: iLat, iLon
    real :: LatLon_D(2)

    real :: LatBC, ThetaBC
    real,allocatable:: LatBcTmp_I(:)
    !--------------------------------------------------------------------------
    do iLon = 1, nPhi!-1
       do iLat = 1, nTheta
          ! write(*,*) 'working on', Theta_G(iLat)*180./cPi,Phi_G(iLon)*180./cPi

          ! Now assume GIMME in SM coords but azimuthal angle 0 defined at
          ! noon. For CIMI 0 is at noon so no rotation needed.
          LatLon_D(1) = 0.5*cPi - Theta_G(iLat)
          LatLon_D(2) = Phi_G(iLon)

          ! deal with coord transformation here

          if (LatLon_D(1) > maxval(LatCimi_C) .or. &
               LatLon_D(1) < minval(LatCimi_C) ) then
             PotCimiOnGimmeGrid_C(iLat,iLon) = 0.0
             JrCimiOnGimmeGrid_C(iLat,iLon) = 0.0
             ! write(*,*)'!',LatLon_D
          else
             PotCimiOnGimmeGrid_C(iLat,iLon) = &
                  bilinear(PotCIMI_C,1,nLatCIMI,1,nLonCIMI,LatLon_D, &
                  LatCimi_C,LonCimi_C,DoExtrapolate=.true.)
             JrCimiOnGimmeGrid_C(iLat,iLon) = &
                  bilinear(JrCIMI_C,1,nLatCIMI,1,nLonCIMI,LatLon_D, &
                  LatCimi_C,LonCimi_C,DoExtrapolate=.true.)!*.1

          endif
       enddo
    enddo

    ! interpolate ibCimi to iThetaBoundary in GIMME
    ! first find find latitude boundary array for CIMI
    if (.not.allocated(LatBcTmp_I))allocate(LatBcTmp_I(nLonCimi))
    do iLon = 1, nLonCimi
       LatBcTmp_I(iLon)=LatCimi_C(iLatBcCimi_I(iLon))
    enddo

    ! for each longitude, interpolate to find boundary lat
    do iLon=1,nPhi
       LatBc=linear(LatBcTmp_I,1,nLonCimi,Phi_G(iLon),LonCimi_C,&
            DoExtrapolate=.true.)
       ThetaBc = 0.5*cPi-LatBc

       ! find the corresponding iThetaBoundary
       lat_loop: do iLat=1,nTheta
          if (Theta_G(iLat)>ThetaBc) then
             iThetaBcOnGimme_I(iLon) = iLat
             EXIT lat_loop
          elseif(iLat==nTheta)then
             iThetaBcOnGimme_I(iLon) = iLat
             EXIT lat_loop
          endif
       enddo lat_loop
    end do

    ! call plot_coupled_values
  end subroutine interpolate_cimi_to_gimme
  !============================================================================

  subroutine interpolate_gimme_to_cimi

    integer :: iLat, iLon
    real :: LatLon_D(2)
    !--------------------------------------------------------------------------
    do iLon = 1, nLonCIMI-1
       do iLat = 1, nLatCIMI
          ! Now assume GIMME in SM coords but azimuthal angle 0 defined at
          ! noon. For CIMI 0 is at noon so no rotation needed.
          LatLon_D(1) = 0.5*cPi-LatCIMI_C(iLat)
          LatLon_D(2) = LonCIMI_C(iLon)

          ! write(*,*) 'working on', LatLon_D(1)*180./cPi,LatLon_D(2)*180./cPi
          ! deal with coord transformation here

          if (LatLon_D(1) > maxval(Theta_G) .or. &
               LatLon_D(1) < minval(Theta_G) ) then
             PotGimmeOnCimiGrid_C(iLat,iLon) = 0.0
          else
             PotGimmeOnCimiGrid_C(iLat,iLon) = &
                  bilinear(Potential_G,0,nTheta+1,0,nPhi+1,LatLon_D, &
                  Theta_G,Phi_G,DoExtrapolate=.true.)
          endif
       enddo
    enddo

    ! call plot_coupled_values_2
  end subroutine interpolate_gimme_to_cimi
  !============================================================================

  subroutine plot_coupled_values

    integer :: iLat, iLon

    !--------------------------------------------------------------------------
    call open_file(FILE='GimmeGridPot.dat')
    write(UnitTmp_,'(a)') &
         'VARIABLES = "Lat", "Lon", "Phi", "Jr"'
    write(UnitTmp_,'(a,i3,a,i3,a)') 'Zone I=', nTheta, &
         ', J=', nPhi-1,', DATAPACKING=POINT'

    do iLon = 1, nPhi-1
       do iLat = 1, nTheta
          write(UnitTmp_,"(100es18.10)") 0.5*cPi-Theta_G(iLat),&
               Phi_G(iLon),PotCimiOnGimmeGrid_C(iLat,iLon),&
               JrCimiOnGimmeGrid_C(iLat,iLon)
       enddo
    enddo
    call close_file

    call open_file(FILE='CimiGridPot.dat')
    write(UnitTmp_,'(a)') &
         'VARIABLES = "Lat", "Lon", "Phi", "Jr"'
    write(UnitTmp_,'(a,i3,a,i3,a)') 'Zone I=', nLatCimi, &
         ', J=', nLonCimi,', DATAPACKING=POINT'

    do iLon = 1, nLonCimi
       do iLat = 1, nLatCimi
          write(UnitTmp_,"(100es18.10)") LatCimi_C(iLat),&
               LonCimi_C(iLon),PotCimi_C(iLat,iLon),JrCimi_C(iLat,iLon)
       enddo
    enddo
    call close_file

  end subroutine plot_coupled_values
  !============================================================================
  subroutine plot_coupled_values_2

    integer :: iLat, iLon
    !--------------------------------------------------------------------------
    call open_file(FILE='GimmeGridPot_2.dat')
    write(UnitTmp_,'(a)') &
         'VARIABLES = "Lat", "Lon", "Phi"'
    write(UnitTmp_,'(a,i3,a,i3,a)') 'Zone I=', nTheta, &
         ', J=', nPhi-1,', DATAPACKING=POINT'

    do iLon = 1, nPhi-1
       do iLat = 1, nTheta
          write(UnitTmp_,"(100es18.10)") 0.5*cPi-Theta_G(iLat),&
               Phi_G(iLon),Potential_G(iLat,iLon)
       enddo
    enddo
    call close_file

    call open_file(FILE='CimiGridPot_2.dat')
    write(UnitTmp_,'(a)') &
         'VARIABLES = "Lat", "Lon", "Phi"'
    write(UnitTmp_,'(a,i3,a,i3,a)') 'Zone I=', nLatCimi, &
         ', J=', nLonCimi,', DATAPACKING=POINT'

    do iLon = 1, nLonCimi
       do iLat = 1, nLatCimi
          write(UnitTmp_,"(100es18.10)") LatCimi_C(iLat),&
               LonCimi_C(iLon),PotGimmeOnCimiGrid_C(iLat,iLon)
       enddo
    enddo
    call close_file

  end subroutine plot_coupled_values_2
  !============================================================================
end module GIMME_Cimi_Interface
!==============================================================================

