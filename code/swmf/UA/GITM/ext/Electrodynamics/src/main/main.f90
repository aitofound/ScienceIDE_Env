
program test_01

  use ModMain
  use ModCharSize
  use ModErrors
  use ModIE
  use ModTimeAmie
  implicit none

  type(ieModel), pointer :: model

  integer :: iMlt, iLat, iLon, iError

  real :: dLat, dMlt, dLon
  real(kind=Real8_) :: dt, jan1Time
  real :: date
  real :: apexHeight, apexLat, apexLon, Bmag, Bx, By, Bz, V
  real :: sbsllat, sbsllon, magMlt, sec
  real :: coLatPole, LonPole, vPol
  logical :: doSouth = .false.
  real :: nsFac = 1.0

  integer :: iTime_I(7), iDay

  ! This is to figure out the grid.
  ! The dMlt is the spacing in magnetic local time.  If you want geographic,
  ! then dMLT is the spacing in local time.
  isGeographic = .false.
  doSouth = .true.
  minLat = 50.
  dLat = 0.5
  dMlt = 0.25
  dLon = 15.0*dMlt

  ! Define on edges:
  nLats = (90.0 - minLat)/dLat + 1
  nLons = 24.0/dMlt + 1

  ! This is still only for main:
  call allocate_all_variables

  ! This is for the IE library:
  allocate(model)
  model = ieModel()

  call model%verbose(10)

  ! Set the electric potential model:
!   call model % efield_model("hepmay")
  call model%efield_model("weimer05")
!   call model % efield_model("amie")

  ! Set aurora model:
!   call model % aurora_model("hpi")
!   call model%aurora_model("fre")
!   call model % aurora_model("pem")
  call model%aurora_model("fta")
!   call model % aurora_model("amie")

  ! Set where the code can find the model files:
  call model%model_dir("data/ext/")

  ! If we are using AMIE files, set north and south files:
  !! ALB found (and tested) these on Mia, in case anyone else needs to...
  ! call model % filename_north("/backup/Software/Electrodynamics/data/ext/SWMF/swmf20110805N.bin")
  ! call model % filename_south("/backup/Software/Electrodynamics/data/ext/SWMF/swmf20110805S.bin")
  call model%filename_north("/Users/ridley/Data/Gitm/run.20110805/swmf20110805N.bin")
  call model%filename_south("/Users/ridley/Data/Gitm/run.20110805/swmf20110805S.bin")

  ! Initialize the IE library after setting it up:
  call model%init()

  ! If there were errors initializing, stop here
  if (.not. isOk) then
    call set_error("Failed to initialize! Exiting!")
    call report_errors
    stop!
  endif

  ! This is for main, not for the IE library:
  ! Handle Start Time:
  iTime_I(1) = 2011
  iTime_I(2) = 8
  iTime_I(3) = 6
  iTime_I(4) = 0
  iTime_I(5) = 0
  iTime_I(6) = 0
  iTime_I(7) = 0
  call time_int_to_real(iTime_I, startTime)

  ! Handle End Time:
  iTime_I(1) = 2011
  iTime_I(2) = 8
  iTime_I(3) = 6
  iTime_I(4) = 0
  iTime_I(5) = 10
  iTime_I(6) = 0
  iTime_I(7) = 0
  call time_int_to_real(iTime_I, endTime)

  if (doSouth) nsFac = -1.0

  if (isGeographic) then
    iTime_I(2) = 1
    iTime_I(3) = 1
    call time_int_to_real(iTime_I, jan1Time)
    date = iTime_I(1) + (startTime - jan1Time)/(24.0*3600.0)/365.25
    write(*, *) 'Date for Apex Calcs : ', date
    do iLon = 1, nLons
      do iLat = 1, nLats
        geoLats(iLon, iLat) = nsFac*(minLat + (iLat - 1)*dLat)
        geoLons(iLon, iLat) = 0.0 + (iLon - 1)*dLon
        call apex(date, geoLats(iLon, iLat), geoLons(iLon, iLat), 120.0, &
                  apexHeight, apexLat, apexLon, Bmag, Bx, By, Bz, V)
        magLats(iLon, iLat) = apexLat
        magLons(iLon, iLat) = apexLon
      enddo
    enddo
    call dypol(coLatPole, LonPole, vPol)
  else
    do iMlt = 1, nLons
      do iLat = 1, nLats
        magLats(iMlt, iLat) = nsFac*(minLat + (iLat - 1)*dLat)
        magMlts(iMlt, iLat) = 0.0 + (iMlt - 1)*dMlt
      enddo
    enddo
  endif

  ! Now that we have a grid, set the grid that we want data for in the
  ! IE libary:
  call model%nMlts(nLons)
  call model%nLats(nLats)

  ! If the grid is magnetic, it is static, so we don't need to change it
  ! as a function of time, just set it:
  if (.not. isGeographic) &
    call model%grid(magMlts, magLats)

  nTimes = 10
  dt = (endTime - startTime)/nTimes

  do iT = 1, nTimes

    currentTime = startTime + dt*(iT - 1)
    write(*, *) " --> currentTime : ", currentTime
    call time_real_to_int(currentTime, iTime_I)

    ! Set the time in the IE library:
    call model%time_ymdhms( &
      iTime_I(1), iTime_I(2), iTime_I(3), &
      iTime_I(4), iTime_I(5), iTime_I(6))

    if (isGeographic) then
      ! Calculate the subsolar point:
      iDay = floor((startTime - jan1Time)/(24.0*3600.0)) + 1
      sec = iTime_I(6)
      call SUBSOLR(iTime_I(1), iDay, iTime_I(4), iTime_I(5), sec, &
                   sbsllat, sbsllon)
      do iLon = 1, nLons
        do iLat = 1, nLats
          apexLon = magLons(iLon, iLat)
          call magloctm(apexLon, sbsllat, sbsllon, coLatPole, LonPole, &
                        magMlt)
          magMlts(iLon, iLat) = magMlt
        enddo
      enddo

      ! If the grid is geographic, it will rotate with time, so we need
      ! to update it each time step:
      call model%grid(magMlts, magLats)
    endif

    imfBx = 0.0
    imfBy = -2.5
    imfBz = -5.0
    swV = 400.0
    swN = 5.0
    au = 100.0
    al = -500.0
    ae = au - al
    hp = 0.102*ae + 8.953

    ! Set indices in the IE library:
    if (model%doReadMHD) then
      call model%imfBz(imfBz)
      call model%imfBy(imfBy)
      call model%swv(swV)
      call model%swn(swN)
    endif

    if (model%doReadKP) &
      call model%kp(4.0)

    if (model%doReadHPI) &
      ! call model % useAeHp()
      call model%hp(hp)

    if (model%doReadSME) then
      call model%au(au)
      call model%al(al)
    endif

    ! Get potential from the IE library:
    call model%get_potential(potential)
    ! Get electron diffuse aurora from the IE library:
    call model%get_aurora(eDiffuseEflux, eDiffuseAvee)
    !   write(*,*) potential
    call model%get_polarcap(polarcap)

    call output_ie(iError)
    if (iError /= 0) call set_error("Main: Error in output_ie")
    write(*, *) '  --> CPCP : ', maxval(potential) - minval(potential)
    write(*, *) '  --> max eflux : ', maxval(eDiffuseEflux)
    write(*, *) '  --> max ave-e : ', maxval(eDiffuseAveE)

    if (.not. isOk) exit

  enddo

  write(LunMainOutput) 0.1
  close(LunMainOutput)

  call report_errors

end program test_01
