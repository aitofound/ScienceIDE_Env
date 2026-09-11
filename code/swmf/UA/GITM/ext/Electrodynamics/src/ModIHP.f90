!  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf

Module ModIHP

  use ModCharSize
  use ModErrors

  integer :: longmx, latmx, ndx
  real    :: steplat

  integer, parameter :: latmdx = 40
  integer, parameter :: lonmdx = 30
  integer, parameter :: mndx = 10
  integer, parameter :: nConductanceSolutions = 4

  real, dimension(mndx) :: halmin, pedmin, avk50, efx50

  real, dimension(0:lonmdx, 0:latmdx, mndx) :: &
    halar = 0.0, pedar = 0.0, avkar = 0.0, efxar = 0.0

  real, dimension(nConductanceSolutions) :: ConductanceBackground

  real, dimension(4) :: bkgc, bkgcer
  real               :: flxmax, dbymax, dbzmax

  integer, parameter :: pedersen_ = 1
  integer, parameter :: hall_ = 2
  integer, parameter :: eflux_ = 3
  integer, parameter :: avee_ = 4

contains

  !-----------------------------------------------------------------------------
  ! Read in either the IHP file or the PEM file into the same array, so that no
  ! code needs to be rewritten
  !-----------------------------------------------------------------------------

  subroutine read_conductance_model(NameOfModel, NameOfModelDir, iDebugLevel)

    implicit none

    character(len=iCharLenIE_), intent(in) :: NameOfModel
    character(len=iCharLenIE_), intent(in) :: NameOfModelDir
    integer, intent(in) :: iDebugLevel
    character(len=iCharLenIE_) :: ihp_file = 'hpke.noaa'
    character(len=iCharLenIE_) :: pem_file = 'hpke2.pem'
    character(len=iCharLenIE_) :: modelDirTotal

    !  NOAA HPI model:  LONGMX=30,LATMX=20,NDX=10,STEPLAT=2.

    real, dimension(0:lonmdx, 0:latmdx, mndx, 4) :: hpaf
    real    :: scale
    integer :: iunit, ierr, n, ilat, ilon, maxflx, ilatsave

    character(len=80) :: char80

    ConductanceBackground(pedersen_) = 0.2
    ConductanceBackground(hall_) = 0.2
    ConductanceBackground(eflux_) = 0.01
    ConductanceBackground(avee_) = 1.0

    scale = 0.001

    iunit = 76

    if (index(NameOfModel, 'ihp') > 0 .or. &
        index(NameOfModel, 'hpi') > 0) then

      if (iDebugLevel > 2) &
        write(*, *) '===> Reading IHP background conductance model'

      modelDirTotal = ihp_file
      call merge_str(NameOfModelDir, modelDirTotal)

      open(iunit, file=modelDirTotal, status='old', iostat=ierr)
      if (ierr /= 0) then
        call set_error('Error opening file in read_conductance_model:')
        call set_error(ihp_file)
        return
      endif

      longmx = 30
      latmx = 20
      ndx = 10
      steplat = 2.

      do n = 1, 4
        read(iunit, *) char80
      enddo

      if (iDebugLevel > 2) write(*, *) "===> Hall"
      read(iunit, *) char80
      do n = 1, ndx
        do ilat = latmx, 0, -1
          read(iunit, "(15f7.0)") (halar(ilon, ilat, n), ilon=0, 14)
          read(iunit, "(15f7.0)") (halar(ilon, ilat, n), ilon=15, 29)
        enddo
      enddo
      halar(longmx, :, :) = halar(0, :, :)

      halar(0:30, 0:latmx, 1:ndx) = scale*halar(0:30, 0:latmx, 1:ndx)

      if (iDebugLevel > 2) write(*, *) "===> Ped"
      read(iunit, *) char80
      do n = 1, ndx
        do ilat = latmx, 0, -1
          read(iunit, "(15f7.0)") (pedar(ilon, ilat, n), ilon=0, 14)
          read(iunit, "(15f7.0)") (pedar(ilon, ilat, n), ilon=15, 29)
        enddo
      enddo
      pedar(longmx, :, :) = pedar(0, :, :)
      pedar(0:30, 0:latmx, 1:ndx) = scale*pedar(0:30, 0:latmx, 1:ndx)

      if (iDebugLevel > 2) write(*, *) "===> AveE"
      read(iunit, *) char80
      do n = 1, ndx
        do ilat = latmx, 0, -1
          read(iunit, "(15f7.0)") (avkar(ilon, ilat, n), ilon=0, 14)
          read(iunit, "(15f7.0)") (avkar(ilon, ilat, n), ilon=15, 29)
          do ilon = 0, 29
            if (avkar(ilon, ilat, n) == 2855) &
              avkar(ilon, ilat, n) = ConductanceBackground(avee_)/scale
          enddo
        enddo
      enddo
      avkar(longmx, :, :) = avkar(0, :, :)
      avkar(0:30, 0:latmx, 1:ndx) = scale*avkar(0:30, 0:latmx, 1:ndx)

      if (iDebugLevel > 2) write(*, *) "===> TotE"
      read(iunit, *) char80
      do n = 1, ndx
        do ilat = latmx, 0, -1
          read(iunit, "(15f7.0)") (efxar(ilon, ilat, n), ilon=0, 14)
          read(iunit, "(15f7.0)") (efxar(ilon, ilat, n), ilon=15, 29)
        enddo
      enddo
      efxar(longmx, :, :) = efxar(0, :, :)
      efxar(0:30, 0:latmx, 1:ndx) = scale*efxar(0:30, 0:latmx, 1:ndx)

      close(iunit)

    else

      if (iDebugLevel > 2) &
        write(*, *) '===> Reading PEM background conductance model'

      longmx = 24
      latmx = 40
      ndx = 9
      steplat = 1.

      modelDirTotal = pem_file
      call merge_str(NameOfModelDir, modelDirTotal)

      open(iunit, file=modelDirTotal, status='old', iostat=ierr)
      if (ierr /= 0) then
        call set_error('Error opening file in read_conductance_model:')
        call set_error(pem_file)
        return
      endif

      do n = 1, 4
        read(iunit, *) char80
      enddo

      do n = 1, ndx
        read(iunit, "(a80)") char80
        do ilat = latmx, 0, -1
          read(iunit, "(24f6.0)") (halar(ilon, ilat, n), ilon=0, 23)
        enddo
      enddo
      halar(longmx, :, :) = halar(0, :, :)
      halar = scale*halar

      do n = 1, ndx
        read(iunit, "(a80)") char80
        do ilat = latmx, 0, -1
          read(iunit, "(24f6.0)") (pedar(ilon, ilat, n), ilon=0, 23)
        enddo
      enddo
      pedar(longmx, :, :) = pedar(0, :, :)
      pedar = scale*pedar

      do n = 1, ndx
        read(iunit, "(a80)") char80
        do ilat = latmx, 0, -1
          read(iunit, "(24f6.0)") (avkar(ilon, ilat, n), ilon=0, 23)
        enddo
      enddo
      avkar(longmx, :, :) = avkar(0, :, :)
      avkar = scale*avkar

      do n = 1, ndx
        read(iunit, "(a80)") char80
        do ilat = latmx, 0, -1
          read(iunit, "(24f6.0)") (efxar(ilon, ilat, n), ilon=0, 23)
        enddo
      enddo
      efxar(longmx, :, :) = efxar(0, :, :)
      efxar = scale*efxar

      close(iunit)

    endif

    ! get minimum or average long. value at lowest lat.

    if (iDebugLevel > 2) write(*, *) "===> Getting Minimum Conductance"

    do n = 1, ndx

      halmin(n) = halar(1, latmx, n)
      pedmin(n) = pedar(1, latmx, n)
      avk50(n) = avkar(1, latmx, n)
      efx50(n) = efxar(1, latmx, n)

      do ilon = 2, longmx
        halmin(n) = amin1(halmin(n), halar(ilon, latmx, n))
        pedmin(n) = amin1(pedmin(n), pedar(ilon, latmx, n))
        avk50(n) = avk50(n) + avkar(1, latmx, n)
        efx50(n) = efx50(n) + efxar(1, latmx, n)
      enddo

      avk50(n) = avk50(n)/float(longmx)
      efx50(n) = efx50(n)/float(longmx)

    enddo

    if (iDebugLevel > 3) write(*, *) "====> Done with read_conductance_model"

  end subroutine read_conductance_model

  !-----------------------------------------------------------------------------
  ! This is to run the either the PEM or the IHP model
  !-----------------------------------------------------------------------------

  subroutine get_auroral_conductance(amlt, alatd, hp, eflx, avkev)

    implicit none

    real, intent(in) :: alatd, amlt, hp
    real, intent(out) :: avkev, eflx

    real    :: dx, tl, x, y, hpi
    integer :: i, j, lon

    ! Convert from hemispheric power to the hemispheric power index:
    hpi = 2.09*ALOG(hp)*1.0475

    dx = 24./float(longmx)
    i = hpi + 0.49999
    if (i < 1) i = 1
    if (i > ndx) i = ndx

    y = (90.-abs(alatd))/steplat

    if (y > float(latmx)) then

      avkev = avk50(i)
      eflx = efx50(i)

    else

      j = y
      if (j < 0) j = 0
      if (j > latmx - 1) j = latmx - 1

      y = y - j

      tl = amlt
      if (tl < 0.) tl = tl + 24.

      lon = tl*longmx/24.

      x = tl - 24.*lon/float(longmx)
      x = x/dx

      if (lon > longmx - 1) lon = lon - longmx
      if (lon < 0.) lon = lon + longmx

      !ped = x*(y*pedar(lon+1,j+1,i) + (1.-y)*pedar(lon+1,j,i)) + &
      !     (1.-x)*(y*pedar(lon,j+1,i) + (1.-y)*pedar(lon,j,i))
      !hal = x*(y*halar(lon+1,j+1,i) + (1.-y)*halar(lon+1,j,i)) + &
      !     (1.-x)*(y*halar(lon,j+1,i) + (1.-y)*halar(lon,j,i))
      avkev = x*(y*avkar(lon + 1, j + 1, i) + (1.-y)*avkar(lon + 1, j, i)) + &
              (1.-x)*(y*avkar(lon, j + 1, i) + (1.-y)*avkar(lon, j, i))
      eflx = x*(y*efxar(lon + 1, j + 1, i) + (1.-y)*efxar(lon + 1, j, i)) + &
             (1.-x)*(y*efxar(lon, j + 1, i) + (1.-y)*efxar(lon, j, i))

      !if (ped < ConductanceBackground(pedersen_)) ped = ConductanceBackground(pedersen_)
      !if (hal < ConductanceBackground(hall_)) hal = ConductanceBackground(hall_)
      if (avkev < ConductanceBackground(avee_)) avkev = ConductanceBackground(avee_)
      if (eflx < ConductanceBackground(eflux_)) eflx = ConductanceBackground(eflux_)

    endif

    return

  end subroutine get_auroral_conductance

end Module ModIHP
