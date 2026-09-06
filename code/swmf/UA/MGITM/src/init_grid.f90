subroutine init_grid

  use ModGITM
  use ModInputs
  use ModConstants
  use GITM_planet
  ! use GITM_location, ONLY: find_lonlat
  use ModSphereInterface
  use ModTime
  use ModEUV,     ONLY: init_mod_euv
  use ModSources, ONLY: init_mod_sources

  implicit none

  type (UAM_ITER) :: r_iter

  integer :: iBlock
  ! ! Testing find_lonlat
  ! integer:: jBlock, jProc, iLon, iLat
  ! real:: rLon, rLat

  logical :: IsOk, IsDone, DoTouchSouth, DoTouchNorth

  call report("init_grid",1)

  if (.not. Is1D) then

     if (IsFullSphere) then
        call UAM_module_setup(iCommGITM, &
             nLons, nLats, nAlts, &
             nBlocksLon, nBlocksLat, &
             -pi/2.0, pi/2.0, &
             .true., .true., &
             0.0, 0.0, 0.0, &
             RBody+AltMin, 5000.0, &
             ok=IsOk)
     else

        DoTouchNorth = .false.
        DoTouchSouth = .false.

        if (LatEnd >= pi/2) then
           LatEnd = pi/2
           DoTouchNorth = .true.
        endif
        if (LatStart <= -pi/2) then
           LatStart = -pi/2
           DoTouchSouth = .true.
        endif

        call UAM_module_setup(iCommGITM, &
             nLons, nLats, nAlts, &
             nBlocksLon, nBlocksLat, &
             LatStart, LatEnd, &
             DoTouchSouth, DoTouchNorth, &
             0.0, 0.0, 0.0, &
             RBody+AltMin, 5000.0, &
             ok=IsOk)
     endif

     if (.not.IsOk) call stop_gitm("Error in trying to create grid.")

     call UAM_XFER_create(ok=IsOk)
     if (.not. IsOk) then
        call UAM_write_error()
        call stop_gitm("Error with UAM_XFER_create")
     endif

     call UAM_ITER_create(r_iter)
     call UAM_ITER_reset(r_iter, iBlock, IsDone)

     nBlocks = 0
     do
        ! ! This is a test of the find_lonlat subroutine
        ! call find_lonlat( &
        !     Longitude(1,iBlock)*0.7+Longitude(2,iBlock)*0.3, &
        !     Latitude(nLats-1,iBlock)*0.2+Latitude(nLats,iBlock)*0.8, &
        !     jProc, jBlock, iLon, iLat, rLon, rLat)
        ! write(*,'(a,6i4,2f6.1)') &
        !     '!!! ijProc,ijBlock,iLon, iLat, rLon, rLat=', &
        !     iProc, jProc, iBlock, jBlock, iLon, iLat, rLon, rLat

        nBlocks = nBlocks + 1
        call UAM_ITER_next(r_iter, iBlock, IsDone)
        if(IsDone) EXIT
     enddo

  else
     nBlocks = 1
     Latitude = LatStart
     Longitude = LonStart
  endif

  call init_mod_gitm
  call init_mod_euv
  call init_mod_sources

end subroutine init_grid
