!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf

module GITM_location

  ! $Id: get_location.f90,v 1.4 2013/10/12 04:01:00 kopmanis Exp $
  !
  ! Author: G. Toth, Aug 2025
  !   Made this collection of subroutines into a module.
  !   Added subroutine find_lonlat that for some Lon, Lat coordinates can
  !   find the processor, block and cell indexes and weight factors
  !   on any processor as long as the lon-lat grid is not stretched.
  !   Generalization to stretched grid is easy if there are two functions
  !   mapping true longitude to stretched longitude index
  !   and true latitude to stretched latitude index (see stretch_grid).
  !
  ! Author: Angeline G. Burrell (AGB), UMichigan, Jan 2013
  !
  ! Modified: AGB, UMichigan, Feb 2013 - added BlockLocationIndex
  !
  ! LocationIndex: A routine to retireve the longitude, latitude, and block
  !   indeces for a specified location.  Shamelessly stolen from
  !   another place in the GITM code and put in a subroutine so that
  !   it can be used in multiple places.  Exit statements were
  !   added to prevent additional cycling through do-loops.
  !
  ! Inputs: LonFind = Desired longitude
  !         LatFind = Desired latitude
  !
  ! Outputs: iiBlock = Block index containing the desired location
  !          iiLon   = Longitude index for LonFind
  !          iiLat   = Latitude index for LatFind
  !          rLon    = Longitude interpolation scaling factor
  !          rLat    = Latitude interpolation scaling factor
  !
  ! BlockLocationIndex: A routine just like LocationIndex, but for a specified
  !                     block
  !
  ! Inputs: LonFind = Desired Longitude
  !         LatFind = Desired Latitude
  !         iBlock  = Block index containing the desired longitude and latitude
  !
  ! Outputs: iiLon   = Longitude index for LonFind
  !          iiLat   = Latitude index for LatFind
  !          rLon    = Longitude interpolation scaling factor
  !          rLat    = Latitude interpolation scaling factor
  !
  ! BlockAltIndex: A routine similar to BlockLocationIndex, but for a specified
  !                altitude
  !
  ! Inputs: AltFind = Desired Altitude
  !         iBlock  = Block index containing the desired longitude and latitude
  !         iLon    = Longitude index
  !         iLat    = Latitude index
  !
  ! Outputs: iiAlt  = Altitude index for AltFind
  !          rAlt   = Altitude interpolation scaling factor

  implicit none

  private ! except

  public:: find_lonlat
  public:: LocationIndex
  public:: LocationProcIndex
  public:: BlockLocationIndex
  public:: BlockAltIndex
  
contains
  !============================================================================
  subroutine find_lonlat(LonIn, Lat, jProc, iBlock, iLon, iLat, rLon, rLat)

    use ModGITM, ONLY: nProcs
    use ModSizeGitm, ONLY: nLons, nLats
    use ModInputs, ONLY: &
         nBlocksLon, nBlocksLat, LonStart, LonEnd, LatStart, LatEnd
    use ModNumConst, ONLY: cTwoPi, cRadToDeg

    real,    intent(in):: LonIn, Lat
    integer, intent(out):: jProc
    integer, intent(out), optional:: iBlock, iLon, iLat
    real,    intent(out), optional:: rLon, rLat

    ! For LonIn,LatIn coordinates (in radians), find the processor, block,
    ! grid cell and interpolation weights. LonIn is periodic in 2*pi

    real:: Lon, LonNorm, LatNorm, LonIndex, LatIndex
    integer:: iBlockLon, iBlockLat, iBlockAll, nBlockPerProc

    character(len=*), parameter:: NameSub = 'find_lonlat'
    !-------------------------------------------------------------------------
    Lon = modulo(LonIn, cTwoPi) ! Make sure 0 <= Lon <= 2*pi
    if(Lon < LonStart .or. Lon > LonEnd)then
       write(*,*) NameSub,': LonIn, Lon, LonStart, LonEnd=', &
            LonIn*cRadToDeg, LonStart*cRadToDeg, LonEnd*cRadToDeg
       call stop_gitm(NameSub//': Longitude outside of range')
    end if 
    if(Lat < LatStart .or. Lat > LatEnd)then
       write(*,*) NameSub,': Lat, LatStart, LatEnd=', &
            Lat*cRadToDeg, LatStart*cRadToDeg, LatEnd*cRadToDeg
       call stop_gitm(NameSub//': Latitude outside of range')
    end if

    LonNorm = (Lon - LonStart)/(LonEnd - LonStart)
    LatNorm = (Lat - LatStart)/(LatEnd - LatStart)
    ! This is where stretch_grid should be called
    
    iBlockLon = LonNorm*nBlocksLon + 1
    iBlockLat = LatNorm*nBlocksLat + 1
    iBlockAll = iBlockLon + nBlocksLon*(iBlockLat - 1)
    nBlockPerProc = (nBlocksLon*nBlocksLat)/nProcs

    ! Calculate processor index from the global block index
    ! The first nProcs-1 processors contain nBlockPerProc blocks.
    ! The last processor contains the rest, which may exceed nBlockPerProc.
    jProc = min(nProcs - 1, (iBlockAll - 1)/nBlockPerProc)

    if(.not.present(iBlock)) RETURN
    ! Calculate the local block index from the global block index
    iBlock = iBlockAll - jProc*nBlockPerProc

    write(*,*)'Lon,Lat,iBlockLon,iBlockLat,iBlockAll,nBlockPerProc,jProc=',&
         Lon*cRadToDeg, Lat*cRadToDeg, &
         iBlockLon, iBlockLat, iBlockAll, nBlockPerProc,jProc
    
    if(.not.present(iLon)) RETURN

    ! Calculate real valued cell index within the block
    ! For Lon=dLon/2 relative to the block boundary LonIndex = 1
    LonIndex = LonNorm*nBlocksLon*nLons - (iBlockLon - 1)*nLons + 0.5
    LatIndex = LatNorm*nBlocksLat*nLats - (iBlockLat - 1)*nLats + 0.5

    ! Convert real valued index to integer
    iLon = LonIndex
    iLat = LatIndex
    
    ! 1 layer of ghost cells should be sufficient
    if(iLon < 0 .or. iLon > nLons)then
       write(*,*) NameSub,': Lon, Lat, jProc, iBlock, iLon=', &
            Lon*cRadToDeg, Lat*cRadToDeg, jProc, iBlock, iLon
       call stop_gitm(NameSub//': iLon is out of range')
    end if
    if(iLat < 0 .or. iLat > nLats)then
       write(*,*) NameSub,': Lon, Lat, jProc, iBlock, iLat=', &
            Lon*cRadToDeg, Lat*cRadToDeg, jProc, iBlock, iLat
       call stop_gitm(NameSub//': iLat is out of range')
    end if

    ! Calculate weight factors for iLon, iLat cell
    if(present(rLon)) rLon = 1 + iLon - LonIndex
    if(present(rLat)) rLat = 1 + iLat - LatIndex
    
  end subroutine find_lonlat
  !============================================================================
  subroutine LocationIndex(LonFind, LatFind, iiBlock, iiLon, iiLat, rLon, rLat)

    use ModGITM

    real, intent(in) :: LonFind, LatFind
    integer, intent(out) :: iiBlock, iiLon, iiLat
    real, intent(out) :: rLon, rLat

    integer:: iBlock, iLon, iLat
    !--------------------------------------------------------------------------
    iiBlock = -1
    iiLon   = -1
    iiLat   = -1

    do iBlock = 1, nBlocks

       if((Longitude(0,iBlock)+Longitude(1,iBlock))/2 <=LonFind .and. &
            (Longitude(nLons,iBlock)+Longitude(nLons+1,iBlock))/2 >LonFind) &
            then

          if((Latitude(0,iBlock)+Latitude(1,iBlock))/2 <=LatFind .and. &
               (Latitude(nLats,iBlock)+Latitude(nLats+1,iBlock))/2 > LatFind) &
               then

             iiBlock = iBlock

             do iLon = 0,nLons
                if(Longitude(iLon,iBlock) <= LonFind .and. &
                     Longitude(iLon+1,iBlock) > LonFind) then
                   iiLon = iLon
                   rLon = 1.0 - (LonFind - Longitude(iLon,iBlock)) / &
                        (Longitude(iLon+1,iBlock) - Longitude(iLon,iBlock))
                   EXIT
                endif
             enddo

             do iLat = 0,nLats
                if(Latitude(iLat,iBlock) <= LatFind .and. &
                     Latitude(iLat+1,iBlock) > LatFind) then
                   iiLat = iLat
                   rLat = 1.0 - (LatFind - Latitude(iLat,iBlock)) / &
                        (Latitude(iLat+1,iBlock) - Latitude(iLat,iBlock))
                   EXIT
                endif
             enddo

             if(iiLon >= 0 .and. iiLat >= 0) then
                EXIT
             end if
          end if
       end if
    end do

  end subroutine LocationIndex
  !============================================================================
  subroutine LocationProcIndex(LonFind, LatFind, AltFind, &
       iiBlock, iiLon, iiLat, iAlt, rLon, rLat, rAlt, iiProc)

    use ModGITM

    real, intent(in) :: LonFind, LatFind, AltFind
    integer, intent(out) :: iiBlock, iiLon, iiLat, iiProc, iAlt
    real, intent(out) :: rLon, rLat, rAlt

    integer:: iBlock, iLon, iLat, jAlt
    !--------------------------------------------------------------------------
    iiProc  = -1
    iiBlock = -1
    iiLon   = -1
    iiLat   = -1
    iAlt    = -1
    rAlt    = -1.0

    do iBlock = 1, nBlocks

       if((Longitude(0,iBlock)+Longitude(1,iBlock))/2 <=LonFind .and. &
            (Longitude(nLons,iBlock)+Longitude(nLons+1,iBlock))/2 >LonFind) then

          if((Latitude(0,iBlock)+Latitude(1,iBlock))/2 <=LatFind .and. &
               (Latitude(nLats,iBlock)+Latitude(nLats+1,iBlock))/2 >LatFind) then

             ! Allow for altitudes being a function of Lon-Lat
             if((minval(Altitude_GB(:,:,0,iBlock)) <=AltFind) .and. &
                  (maxval(Altitude_GB(:,:,nAlts+1,iBlock)) >AltFind)) then

                iiBlock = iBlock
                iiProc = iProc

                do iLon = 0,nLons
                   if(Longitude(iLon,iBlock) <= LonFind .and. &
                        Longitude(iLon+1,iBlock) > LonFind) then
                      iiLon = iLon
                      rLon = 1 - (LonFind - Longitude(iLon,iBlock)) / &
                           (Longitude(iLon+1,iBlock) - Longitude(iLon,iBlock))
                      EXIT
                   endif
                enddo

                do iLat = 0,nLats
                   if(Latitude(iLat,iBlock) <= LatFind .and. &
                        Latitude(iLat+1,iBlock) > LatFind) then
                      iiLat = iLat
                      rLat = 1 - (LatFind - Latitude(iLat,iBlock)) / &
                           (Latitude(iLat+1,iBlock) - Latitude(iLat,iBlock))
                      EXIT
                   endif
                enddo

                do jAlt = 0,nAlts
                   if (Altitude_GB(iLon, iLat, jAlt, iBlock) <= AltFind .and. &
                        Altitude_GB(iLon, iLat, jAlt+1,iBlock) > AltFind) then
                      iAlt = jAlt
                      rAlt  = 1 - &
                           (AltFind - Altitude_GB(iLon,iLat,iAlt,iBlock)) &
                           /(Altitude_GB(iLon,iLat,iAlt+1,iBlock) &
                           - Altitude_GB(iLon,iLat,iAlt,iBlock))
                      EXIT
                   endif
                enddo

                if(iiLon >= 0 .and. iiLat >= 0 .and. iAlt >= 0) EXIT
             end if
          end if
       end if
    end do

  end subroutine LocationProcIndex
  !============================================================================
  subroutine BlockLocationIndex( &
       LonFind, LatFind, iBlock, iiLon, iiLat, rLon, rLat)

    use ModGITM

    real, intent(in) :: LonFind, LatFind
    integer, intent(in) :: iBlock
    integer, intent(out) :: iiLon, iiLat
    real, intent(out) :: rLon, rLat

    integer:: iLon, iLat
    !--------------------------------------------------------------------------
    iiLon = -1
    iiLat = -1
    rLon  = -1.0
    rLat  = -1.0

    if((Longitude(0,iBlock)+Longitude(1,iBlock))/2 <=LonFind .and. &
         (Longitude(nLons,iBlock)+Longitude(nLons+1,iBlock))/2 >LonFind) then

       if((Latitude(0,iBlock)+Latitude(1,iBlock))/2 <=LatFind .and. &
            (Latitude(nLats,iBlock)+Latitude(nLats+1,iBlock))/2 >LatFind) then

          do iLon = 0,nLons
             if(Longitude(iLon,iBlock) <= LonFind .and. &
                  Longitude(iLon+1,iBlock) > LonFind) then
                iiLon = iLon
                rLon  = 1.0 - (LonFind - Longitude(iLon,iBlock)) / &
                     (Longitude(iLon+1,iBlock) - Longitude(iLon,iBlock))
                EXIT
             endif
          enddo

          do iLat = 0,nLats
             if(Latitude(iLat,iBlock) <= LatFind .and. &
                  Latitude(iLat+1,iBlock) > LatFind) then
                iiLat = iLat
                rLat = 1.0 - (LatFind - Latitude(iLat,iBlock)) / &
                     (Latitude(iLat+1,iBlock) - Latitude(iLat,iBlock))
                EXIT
             endif
          enddo
       end if
    end if

  end subroutine BlockLocationIndex
  !============================================================================
  subroutine BlockAltIndex(AltFind, iBlock, iLon, iLat, iAlt, rAlt)

    use ModGITM

    real, intent(in)     :: AltFind
    integer, intent(in)  :: iBlock, iLon, iLat
    integer, intent(out) :: iAlt
    real, intent(out)    :: rAlt

    integer:: jAlt
    !--------------------------------------------------------------------------
    iAlt = -1
    rAlt = -1.0

    do jAlt = 0,nAlts
       if (Altitude_GB(iLon, iLat, jAlt, iBlock) <= AltFind .and. &
            Altitude_GB(iLon, iLat, jAlt+1,iBlock) > AltFind) then
          iAlt = jAlt
          rAlt  = 1.0 - (AltFind - Altitude_GB(iLon, iLat, iAlt, iBlock)) &
               / (Altitude_GB(iLon, iLat, iAlt+1, iBlock) &
               - Altitude_GB(iLon, iLat, iAlt, iBlock))
          EXIT
       endif
    enddo

  end subroutine BlockAltIndex
  !============================================================================
end module GITM_location
!==============================================================================
