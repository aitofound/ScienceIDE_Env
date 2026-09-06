Module ModGitmAtmos
  implicit none

  private

  ! Time Cadence at which ovation data is provided
  integer, public :: DtReadGitm
  real,    public :: StartTime
  logical, public :: UseGITM = .false.
  
  !GITM grid
  real, allocatable :: Lon_G(:),Lat_G(:),Alt_G(:)
  integer :: nLon, nLat, nAlt

  !GITM variables
  real, allocatable :: Tn_G    (:,:,:),&
                  Ti_G    (:,:,:),&
                  Te_G    (:,:,:),&
                  nO_G    (:,:,:),&
                  nN2_G   (:,:,:),&
                  nO2_G   (:,:,:),&
                  nO2P_G  (:,:,:),&
                  nOP_G   (:,:,:),&
                  ne_G    (:,:,:),&
                  Veast_G (:,:,:),&
                  Vnorth_G(:,:,:),&
                  Vup_G   (:,:,:)

  !mass in AMU
  real :: MassO = 16., MassO2=32., MassN=14.,MassN2=28., MassH = 1., MassHe=4.

  !surface gravity in m/s^2
  real :: GravSurface=9.8
  
  public :: read_gitm_file
  public :: unit_test_gitm

contains
  !============================================================================
  subroutine read_gitm_file(tSimulation)

    use ModTimeConvert, ONLY: time_real_to_int
    use ModIoUnit, ONLY: UnitTmp_

    real, intent(in) :: tSimulation
    
    integer, parameter :: nHeader = 9
    integer :: iHeader, iAlt, iLon, iLat
    character(len=200) :: header
    character(len=29)  :: preheader

    !current GITM file
    Character(len=200) :: NameFile

    !time variables
    integer :: iTimeRead_I(7)
    real :: CurrentTime,TimeRead
    integer, parameter :: Year_=1,Month_=2,Day_=3,Hour_=4,Minute_=5,Second_=6
    !--------------------------------------------------------------------------
    
    CurrentTime=StartTime+tSimulation
    ! get read time closest to simulation time without going over
    TimeRead = (floor(CurrentTime/DtReadGitm) * DtReadGitm)
    call time_real_to_int(TimeRead,iTimeRead_I)

    
    write(NameFile,"(a,i4.4,i2.2,i2.2,a,i2.2,i2.2,i2.2,a)") &
         'PW/UAfiles/gitm_',&
         iTimeRead_I(Year_),iTimeRead_I(Month_),iTimeRead_I(Day_),&
         '_',iTimeRead_I(Hour_),iTimeRead_I(Minute_),iTimeRead_I(Second_),'_PWOM.dat'
    !write(*,*) NameFile
    !open file for reading
    open(UnitTmp_,file=NameFile,status="old")
    !discard header
    do iHeader = 1,nHeader
       if (iHeader==4) then
          read(UnitTmp_,"(a28,i3.3)") header,nLon
       else if (iHeader==5) then
          read(UnitTmp_,"(a28,i3.3)") header,nLat
       else if (iHeader==6) then
          read(UnitTmp_,"(a28,i3.3)") header,nAlt
       else
          read(UnitTmp_,*) header
       endif
    enddo

    write(*,*) nLon,nLat,nAlt
    
    !allocate all gitm arrays if not allocatedd yet
    if (.not.allocated(Alt_G)) then
       allocate(Alt_G(0:nAlt+1))
       allocate(Lat_G(0:nLat+1))
       allocate(Lon_G(0:nLon+1))
       allocate(Tn_G(0:nLon+1,0:nLat+1,0:nAlt+1))
       allocate(Ti_G(0:nLon+1,0:nLat+1,0:nAlt+1))
       allocate(Te_G(0:nLon+1,0:nLat+1,0:nAlt+1))
       allocate(nO_G(0:nLon+1,0:nLat+1,0:nAlt+1))
       allocate(nN2_G(0:nLon+1,0:nLat+1,0:nAlt+1))
       allocate(nO2_G(0:nLon+1,0:nLat+1,0:nAlt+1))
       allocate(nO2P_G(0:nLon+1,0:nLat+1,0:nAlt+1))
       allocate(nOP_G(0:nLon+1,0:nLat+1,0:nAlt+1))
       allocate(ne_G(0:nLon+1,0:nLat+1,0:nAlt+1))
       allocate(Veast_G(0:nLon+1,0:nLat+1,0:nAlt+1))
       allocate(Vnorth_G(0:nLon+1,0:nLat+1,0:nAlt+1))
       allocate(Vup_G(0:nLon+1,0:nLat+1,0:nAlt+1))
    endif
    
    do iAlt = 0, nAlt+1 
       do iLat = 0, nLat+1 
          do iLon = 0,nLon+1 
             read(UnitTmp_,*) Lon_G(iLon),Lat_G(iLat),&
                  Alt_G   (ialt),&
                  Tn_G    (iLon,iLat,ialt),&
                  Ti_G    (iLon,iLat,ialt),&
                  Te_G    (iLon,iLat,ialt),&
                  nO_G    (iLon,iLat,iAlt),&
                  nN2_G   (iLon,iLat,iAlt),&
                  nO2_G   (iLon,iLat,iAlt),&
                  nO2P_G  (iLon,iLat,iAlt),&
                  nOP_G   (iLon,iLat,iAlt),&
                  ne_G    (iLon,iLat,iAlt),&
                  Veast_G (iLon,iLat,iAlt),&
                  Vnorth_G(iLon,iLat,iAlt),&
                  Vup_G   (iLon,iLat,iAlt)
          end do
       end do
    end do
    close(UnitTmp_)

    write(*,*) 'Alt_G',Alt_G
    write(*,*) 'Lon_G',Lon_G
    write(*,*) 'Lat_G',Lat_G
    
    
  end subroutine read_gitm_file
  !============================================================================
  subroutine plot_gitm(tSimulation)

    use ModIoUnit, ONLY: UnitTmp_
    use ModNumConst,ONLY: cPi
    use ModTimeConvert, ONLY: time_real_to_int
    real, intent(in) :: tSimulation

    character(len=100) :: NamePlot3D
     !time variables
    integer :: iTimeRead_I(7)
    real :: CurrentTime,TimeRead
    integer, parameter :: Year_=1,Month_=2,Day_=3,Hour_=4,Minute_=5,Second_=6
    real :: rPlanet = 6375.0 !for earth in km
    real :: theta,phi,radius, xcoord,ycoord,zcoord
    integer :: iAlt, iLat, iLon
    !--------------------------------------------------------------------------
    !write the output file names

    CurrentTime=StartTime+tSimulation
    ! get read time closest to simulation time without going over
    TimeRead = (floor(CurrentTime/DtReadGitm) * DtReadGitm)
    call time_real_to_int(TimeRead,iTimeRead_I)
    

    write(NamePlot3D,"(a,i4.4,i2.2,i2.2,i2.2,i2.2,i2.2,a)")&
    'PW/gitm3D_',iTimeRead_I(Year_),iTimeRead_I(Month_),&
         iTimeRead_I(Day_),iTimeRead_I(Hour_),iTimeRead_I(Minute_),&
         iTimeRead_I(Second_),'.dat'

    !write output to file for 3D plotting
    open(UnitTmp_,file=NamePlot3D)
    write(UnitTmp_,'(a)') &
         'VARIABLES = "X [R]", "Y [R]", "Z [R]", "Lon", "Lat", "Alt", "Tn", "Ti", "nN2", "nO"'
    write(UnitTmp_,'(a,i3,a,i3,a,i3,a)') 'Zone I=', nLon, &
         ', J=', nLat,', K=',nAlt,', DATAPACKING=POINT'
    do iAlt = 1,nAlt
       do iLat=1,nLat
          do iLon=1,nLon
             radius = (Alt_G(iAlt)+rPlanet)/rPlanet
             theta = (90.0-Lat_G(iLat))*cPi/180.0
             phi   = Lon_G(iLon)*cPi/180.0
             xcoord=radius*sin(theta)&
                  *cos(phi)
             ycoord=radius*sin(theta)&
                  *sin(phi)
             zcoord=radius*cos(theta)
             write(UnitTmp_,"(100es18.10)") &
                  xcoord,ycoord,zcoord, &
                  Lon_G(iLon), Lat_G(iLat), &
                  Alt_G(iAlt), Tn_G(iLon,iLat,iAlt),  &
                  Ti_G(iLon,iLat,iAlt), nN2_G(iLon,iLat,iAlt),nO_G(iLon,iLat,iAlt)
          end do
       end do
    end do

    close(UnitTmp_)
    
  end subroutine plot_gitm
  !============================================================================
  subroutine get_gitm_point(gLat,gLon,Alt,Tn,Ti,Te,nO,nN2,nO2,nO2P,nOP,ne,&
       Veast,Vnorth,Vup)

    ! get gitm output for  gLat and gLon (in deg)  and alt (km)

    use ModConst,       ONLY: cProtonMass,cBoltzmann
    use ModInterpolate, ONLY: bilinear, trilinear
    real, intent(in) :: gLat,gLon !input in degrees
    real, intent(in) :: Alt !input in km
    real, intent(out):: Tn,Ti,Te,nO,nN2,nO2,nO2P,nOP,ne,Veast,Vnorth,Vup 
    real :: ScaleHeight
    !---------------------------------------------------------------------------
    
    if (Alt>Alt_G(0) .and. Alt<Alt_G(nAlt)) then
       Tn= &
            trilinear(Tn_G,0,nLon+1,0,nLat+1,0,nAlt+1,[gLon,gLat,Alt],&
            Lon_G,Lat_G,Alt_G,DoExtrapolate=.false.)

       Ti= &
            trilinear(Ti_G,0,nLon+1,0,nLat+1,0,nAlt+1,[gLon,gLat,Alt],&
            Lon_G,Lat_G,Alt_G,DoExtrapolate=.false.)

       Te= &
            trilinear(Te_G,0,nLon+1,0,nLat+1,0,nAlt+1,[gLon,gLat,Alt],&
            Lon_G,Lat_G,Alt_G,DoExtrapolate=.false.)

       nO= &
            trilinear(nO_G,0,nLon+1,0,nLat+1,0,nAlt+1,[gLon,gLat,Alt],&
            Lon_G,Lat_G,Alt_G,DoExtrapolate=.false.)

       nN2= &
            trilinear(nN2_G,0,nLon+1,0,nLat+1,0,nAlt+1,[gLon,gLat,Alt],&
            Lon_G,Lat_G,Alt_G,DoExtrapolate=.false.)

       nO2= &
            trilinear(nO2_G,0,nLon+1,0,nLat+1,0,nAlt+1,[gLon,gLat,Alt],&
            Lon_G,Lat_G,Alt_G,DoExtrapolate=.false.)

       nO2P= &
            trilinear(nO2P_G,0,nLon+1,0,nLat+1,0,nAlt+1,[gLon,gLat,Alt],&
            Lon_G,Lat_G,Alt_G,DoExtrapolate=.false.)

       nOP= &
            trilinear(nOP_G,0,nLon+1,0,nLat+1,0,nAlt+1,[gLon,gLat,Alt],&
            Lon_G,Lat_G,Alt_G,DoExtrapolate=.false.)

       ne= &
            trilinear(ne_G,0,nLon+1,0,nLat+1,0,nAlt+1,[gLon,gLat,Alt],&
            Lon_G,Lat_G,Alt_G,DoExtrapolate=.false.)

       Veast= &
            trilinear(Veast_G,0,nLon+1,0,nLat+1,0,nAlt+1,[gLon,gLat,Alt],&
            Lon_G,Lat_G,Alt_G,DoExtrapolate=.false.)

       Vnorth= &
            trilinear(Vnorth_G,0,nLon+1,0,nLat+1,0,nAlt+1,[gLon,gLat,Alt],&
            Lon_G,Lat_G,Alt_G,DoExtrapolate=.false.)

       Vup= &
            trilinear(Vup_G,0,nLon+1,0,nLat+1,0,nAlt+1,[gLon,gLat,Alt],&
            Lon_G,Lat_G,Alt_G,DoExtrapolate=.false.)
       
    elseif(Alt<=Alt_G(0)) then
       ! do not interpolate below min alt return error in this case as
       !PWOM should never go below min alt
    else
       !when above maximum of GITM altitude use hydrostatic assumption based
       !on last cell
       Tn= &
            bilinear(Tn_G(:,:,nAlt), 0, nLon+1, 0, nLat+1, [gLon, gLat],&
            Lon_G,Lat_G,DoExtrapolate=.false.)

       Ti= &
            bilinear(Ti_G(:,:,nAlt),0,nLon+1,0,nLat+1,[gLon,gLat],&
            Lon_G,Lat_G,DoExtrapolate=.false.)

       Te= &
            bilinear(Te_G(:,:,nAlt),0,nLon+1,0,nLat+1,[gLon,gLat],&
            Lon_G,Lat_G,DoExtrapolate=.false.)
              
       
       ScaleHeight = cBoltzmann*Tn/( MassO*cProtonMass*GravSurface )
       nO= &
            bilinear(nO_G(:,:,nAlt),0,nLon+1,0,nLat+1,[gLon,gLat],&
            Lon_G,Lat_G,DoExtrapolate=.false.)*exp(-(Alt-Alt_G(nAlt))&
            /ScaleHeight)

       ScaleHeight = cBoltzmann*Tn/( MassN2*cProtonMass*GravSurface )
       nN2= &
            bilinear(nN2_G(:,:,nAlt),0,nLon+1,0,nLat+1,[gLon,gLat],&
            Lon_G,Lat_G,DoExtrapolate=.false.)*exp(-(Alt-Alt_G(nAlt))&
            /ScaleHeight)

       ScaleHeight = cBoltzmann*Tn/( MassO2*cProtonMass*GravSurface )
       nO2= &
            bilinear(nO2_G(:,:,nAlt),0,nLon+1,0,nLat+1,[gLon,gLat],&
            Lon_G,Lat_G,DoExtrapolate=.false.)*exp(-(Alt-Alt_G(nAlt))&
            /ScaleHeight)

       ScaleHeight = cBoltzmann*Tn/( MassO2*cProtonMass*GravSurface )
       nO2p= &
            bilinear(nO2p_G(:,:,nAlt),0,nLon+1,0,nLat+1,[gLon,gLat],&
            Lon_G,Lat_G,DoExtrapolate=.false.)*exp(-(Alt-Alt_G(nAlt))&
            /ScaleHeight)
       
       ScaleHeight = cBoltzmann*Tn/( MassO*cProtonMass*GravSurface )
       nOP= &
            bilinear(nOP_G(:,:,nAlt),0,nLon+1,0,nLat+1,[gLon,gLat],&
            Lon_G,Lat_G,DoExtrapolate=.false.)*exp(-(Alt-Alt_G(nAlt))&
            /ScaleHeight)

       !dont extend ne above upper boundary, just keep it constant
       !same with neutral winds. just keep constant
       ne= &
            bilinear(ne_G(:,:,nAlt),0,nLon+1,0,nLat+1,[gLon,gLat],&
            Lon_G,Lat_G,DoExtrapolate=.false.)

       
       Veast= &
            bilinear(Veast_G(:,:,nAlt),0,nLon+1,0,nLat+1,[gLon,gLat],&
            Lon_G,Lat_G,DoExtrapolate=.false.)*exp(-(Alt-Alt_G(nAlt))&
            /ScaleHeight)

       Vnorth= &
            bilinear(Vnorth_G(:,:,nAlt),0,nLon+1,0,nLat+1,[gLon,gLat],&
            Lon_G,Lat_G,DoExtrapolate=.false.)*exp(-(Alt-Alt_G(nAlt))&
            /ScaleHeight)

       Vup= &
            bilinear(Vup_G(:,:,nAlt),0,nLon+1,0,nLat+1,[gLon,gLat],&
            Lon_G,Lat_G,DoExtrapolate=.false.)*exp(-(Alt-Alt_G(nAlt))&
            /ScaleHeight)

    endif

  end subroutine get_gitm_point
  !============================================================================
  subroutine unit_test_gitm

    use ModTimeConvert, ONLY: time_int_to_real

    integer :: iStartTime_I(7) = [2011,6,14,4,0,0,0]
    real :: time=0.0
!    real :: SmLat=77.0,SmLon=0.0
    real :: gLat=1.5,gLon=187.5,Alt=238.9

    real :: Tn,Ti,Te,nO,nN2,nO2,nO2P,nOP,ne,Veast,Vnorth,Vup
    !---------------------------------------------------------------------------
    write(*,*) 'testing for time'
    write(*,*) 'year:',iStartTime_I(1)
    write(*,*) 'month:',iStartTime_I(2)
    write(*,*) 'day:',iStartTime_I(3)
    write(*,*) 'hour:',iStartTime_I(4)
    write(*,*) 'min:',iStartTime_I(5)
    write(*,*) 'sec:',iStartTime_I(6)
    write(*,*) 'simulation time:', Time

    DtReadGitm = 10.0
    
    call time_int_to_real(iStartTime_I,StartTime)

    call read_gitm_file(time)

    call get_gitm_point(gLat,gLon,Alt,Tn,Ti,Te,nO,nN2,nO2,nO2P,nOP,ne,&
       Veast,Vnorth,Vup)

    write(*,*) 'GITM Values:'
    write (*,*) 'Tn  =',Tn
    write (*,*) 'Ti  =',Ti
    write (*,*) 'Te  =',Te
    write (*,*) 'nO  =',nO
    write (*,*) 'nN2 =',nN2
    write (*,*) 'nO2 =',nO2
    write (*,*) 'nO2P=',nO2P
    write (*,*) 'nOP =',nOP
    write (*,*) 'ne  =',ne
    write (*,*) 'Veast  =',Veast
    write (*,*) 'Vnorth =',Vnorth
    write (*,*) 'Vup    =',Vup


    call plot_gitm(time)

  end subroutine unit_test_gitm
  !============================================================================
end module ModGitmAtmos
!==============================================================================
