subroutine calc_physics(iBlock)

! OrbitAngle = Ls  (from Mars.f90 calculation) 
! Prints off: Ls, SolDeclinationDeg

  use ModGITM
  use ModTime
  use ModConstants
  use ModEUV
  use GITM_planet
  use ModInputs
  implicit none

  integer, external :: jday
  real*8, external :: julian_date
  integer, intent(in) :: iBlock

  integer :: itime(7)
  real*8 :: VernalDay
  real :: JulianDayEq, DaysInYear, OrbitAngle
  real :: SunDeclination, SunDeclinationDeg, SinDec, CosDec

  real :: DayNumber,TimeHour,Cy
  real :: MeanAnomaly,RMeanAnomaly,DMeanAnomaly
  real :: EccAnomaly1,EccAnomaly2,REccAnomaly1,REccAnomlay2
  real :: TrueAnomaly,RTrueAnomaly, MTC, EOT_Hours, delta_lon

  real, DIMENSION(-1:nlons+2) :: LMST
  
  integer :: i
  integer :: iLon, iLat, iAlt

  call report("calc_physics",2)

  !\
  ! Solar Things
  !/

!\
! Orbital Elements procedure taken from 
! http://home.att.net/~srschmitt/planetorbits.html
!/

  TimeHour = iTimeArray(4)+iTimeArray(5)/60.0+iTimeArray(6)/3600.0
  
  DayNumber = 367*iTimeArray(1)-7*(iTimeArray(1)+(iTimeArray(2)+9)/12)/4 &
	   +275*iTimeArray(2)/9+iTimeArray(3)-730531.5+TimeHour/24.0
 
  CY = DayNumber/36525.0
  
  MeanAnomaly =(SunOrbit_D+((SunOrbit_E*CY)/3600.0)) - SunOrbit_C
  RMeanAnomaly = pi/180.0*MeanAnomaly
  DMeanAnomaly = RMeanAnomaly/(2.0*pi)
  
  if (DMeanAnomaly > 0.0) then
	 RMeanAnomaly = (2.0*pi)*(DMeanAnomaly-floor(DMeanAnomaly))
  else
	 RMeanAnomaly = (2.0*pi)*(DMeanAnomaly-ceiling(DMeanAnomaly))
  endif
  
  if (RMeanAnomaly < 0.0) then
	 RMeanAnomaly = (2.0*pi)+RMeanAnomaly
  endif
  
  EccAnomaly1 = RMeanAnomaly + SunOrbit_B*sin(RMeanAnomaly)*(1.0+ &
	 SunOrbit_B*cos(RMeanAnomaly))
  
  !  Calculate true anomaly  
  do i=1,10
	 EccAnomaly2 = EccAnomaly1
	 
	 EccAnomaly1 = EccAnomaly2 - ((EccAnomaly2-SunOrbit_B* &
		sin(EccAnomaly2)-RMeanAnomaly)/(1.0-SunOrbit_B* &
		cos(EccAnomaly2)))

  enddo

  TrueAnomaly = 2.0*atan(sqrt((1.0+SunOrbit_B)/ &
	 (1.0-SunOrbit_B))*tan(0.5*EccAnomaly1))
 
  if (TrueAnomaly < 0) then
	 TrueAnomaly = TrueAnomaly + (2.0*pi)
  endif
  
 !!!!!!! General SunOrbitEccentricity 
  SunOrbitEccentricity = SunOrbit_A*(1.0-SunOrbit_B**2.0)/(1.0+ &
	   SunOrbit_B*cos(TrueAnomaly))

 !!!!!!! Old SunOrbitEccentricity
!
!  SunOrbitEccentricity = SunOrbit_A                     + &
!                         SunOrbit_B*cos(OrbitAngle)    + &
!                         SunOrbit_C*sin(OrbitAngle)    + &
!                         SunOrbit_D*cos(2.*OrbitAngle) + &
!                         SunOrbit_E*sin(2.*OrbitAngle)
!
  MarsOrbitalDistance(1:nLons,1:nLats,1:nAlts) = &
       SunOrbitEccentricity

  OrbitAngle = L_s
  SunDeclination = atan(tan(Tilt*pi/180.)*sin(OrbitAngle*pi/180.))
  SubSolarLatitude = SunDeclination ! Radians
  EOT_Hours = EOT*24/360. !convert EOT to hours first

  MTC = mod(24*(((JD_UTC-2451549.5)/1.027491252)+44796.0 - 0.0009626 ),24.0)
  SubsolarLongitude = mod(((MTC+(EOT_Hours))*(360/24.)+180), 360.0) 

  !Use deg E
  SubsolarLongitude = (360-SubsolarLongitude) * pi/180. ! Radians
	  
  SinDec = sin(SunDeclination)
  CosDec = cos(SunDeclination)

  LMST = modulo( MTC + Longitude(:,iBlock) * 24/(2*pi), 24.0 ) 
  LocalTime = modulo( LMST + EOT_hours, 24.0 )  

  do iAlt=-1,nAlts+2

     !
     ! Compute Magnetic Local Time
     !

     if (UseApex .and. IsEarth) then
        do iLat=-1,nLats+2
           do iLon=-1,nLons+2
              call magloctm( &
                   MLongitude(iLon,iLat,iAlt,iBlock), &
                   SubsolarLatitude,   &
                   SubsolarLongitude,  &
                   MagneticPoleColat, &
                   MagneticPoleLon,   &
                   MLT(iLon,iLat,iAlt))
              if (mlt(iLon,iLat,iAlt) < 0) &
                   mlt(iLon,iLat,iAlt) = mlt(iLon,iLat,iAlt) + 24.0
           enddo
        enddo
     else

        do iLat=-1,nLats+2
           MLT(:,iLat,iAlt) = LocalTime
        enddo

        ! Since we go over the pole, 
        !we have to move the points to the proper location:

        if (Latitude(0,iBlock) < -pi/2.0) then
           MLT(:,-1,iAlt)      = MLT(:,2,iAlt) + 12.0
           MLT(:,0,iAlt)       = MLT(:,1,iAlt) + 12.0
        endif

        if (Latitude(0,iBlock) > pi/2.0) then
           MLT(:,nLats+1,iAlt) = MLT(:,nLats,iAlt) + 12.0
           MLT(:,nLats+2,iAlt) = MLT(:,nLats-1,iAlt) + 12.0
        endif

     endif

  enddo

!  write(*,*) mlt(1,1,1)

  where (MLT > 12.0)
     MLT = MLT - 24.0
  end where

  do iLon = 1, nLons
     delta_lon = mod((Longitude(iLon,iBlock) - SubsolarLongitude),360.0) 
     do iLat = 1, nLats
         sza(iLon, iLat,iBlock) =  &
             acos(SinDec*sin(Latitude(iLat,iBlock)) + &
             CosDec*CosLatitude(iLat,iBlock) * &
             cos(delta_lon))

        if (DtLTERadiation < Rotation_Period) then
           call calc_avesza(iLon,iLat,iBlock, SinDec, CosDec)
        endif

     enddo
  enddo

  call calc_scaled_euv

  do iAlt = 1, nAlts
     xSolar(:,:,iAlt) = RadialDistance_GB(1:nLons,1:nLats,iAlt,iBlock) &
          * cos(SZA(:,:,iBlock))
     ySolar(:,:,iAlt) = RadialDistance_GB(1:nLons,1:nLats,iAlt,iBlock) &
          * sin(SZA(:,:,iBlock))
  enddo

end subroutine calc_physics
