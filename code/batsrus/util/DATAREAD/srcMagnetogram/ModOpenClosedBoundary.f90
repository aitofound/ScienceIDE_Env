module ModOpenClosedBoundary

  use ModLookupTable, ONLY: i_lookup_table,  get_lookup_table, &
       interpolate_lookup_table
  use ModMpi, ONLY:  MPI_COMM_WORLD, MPI_SUM, mpi_reduce_real_array
  implicit none

  save

  public:: read_open_closed_boundary
  public:: get_open_closed_boundary

  character(len=100):: NameB0File = 'harmonics_bxyz.out'
  real    :: rBoundary
  integer :: nLonBoundary = 360, nLatBoundary = 180
  real    :: SizeStep = 0.01

  real:: rMinB0Local ! radial limits of table
  real:: rMaxB0Local
  real:: LonMinB0Local ! longitude limits of table in degrees
  real:: LonMaxB0Local
  real:: LatMinB0Local ! latitude limits of table in degrees
  real:: LatMaxB0Local
  real:: Param_I(4), IndexMin_I(3), IndexMax_I(3)
  integer:: nCR = 0, nParam = 0
  real:: LonShift = 0., CRFraction = 0.

  integer :: iProc, iError, nProc
  integer :: iTableB0

contains
  !============================================================================
  subroutine read_open_closed_boundary(NameCommand)

    use ModReadParam, ONLY: read_var
    use ModUtilities, ONLY: CON_stop

    character(len=*), intent(in) :: NameCommand

    character(len=*), parameter:: NameSub = 'read_open_closed_boundary'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case('#OPENCLOSEDBOUNDARY')
       call read_var('NameB0File',   NameB0File)
       call read_var('rBoundary',    rBoundary)
       call read_var('nLonBoundary', nLonBoundary)
       call read_var('nLatBoundary', nLatBoundary)
       call read_var('SizeStep',     SizeStep)
    case default
       call CON_stop(NameSub//' invalid NameCommand='//NameCommand)
    end select

  end subroutine read_open_closed_boundary
  !============================================================================
  subroutine init_b0(iComm)

    integer, intent(in), optional :: iComm

    character(len=*), parameter:: NameSub = 'init_b0'
    !--------------------------------------------------------------------------
    if(present(iComm))then
       call MPI_comm_size(iComm,nProc,iError)
       call MPI_comm_rank(iComm,iProc,iError)
    else
       iProc = 0
    end if

    iTableB0 = i_lookup_table('B0')

    call get_lookup_table(iTableB0, nParam=nParam, Param_I=Param_I, &
         IndexMin_I=IndexMin_I, IndexMax_I=IndexMax_I)

    rMinB0Local = IndexMin_I(1)
    rMaxB0Local = IndexMax_I(1)
    LonShift    = Param_I(3)
    nCR         = Param_I(4)

    if(iProc == 0)then
       write(*,*)NameSub," read B0Local table"
       write(*,*)NameSub," rMinB0Local,   rMaxB0Local  =", &
            rMinB0Local, rMaxB0Local
       write(*,*) 'nCR LonShift =', nCR, LonShift
    endif
  end subroutine init_b0
  !============================================================================
  subroutine get_open_closed_boundary(iComm)

    use ModNumConst,       ONLY: cTwoPi, cRadToDeg, cPi
    use ModCoordTransform, ONLY: rlonlat_to_xyz, xyz_to_rlonlat
    use ModPlotFile,       ONLY: save_plot_file

    integer, intent(in), optional :: iComm

    real :: OpenArea

    real :: dLonTrace, dLatTrace, BrLocal
    real :: XyzNow_D(3), XyzNext_D(3), B0Now_D(3), rLonLatNow_D(3)
    integer :: iTrace, iLonTrace, iLatTrace, iDirection, iStep, nStepMax
    real, allocatable :: LonTrace_I(:), LatTrace_I(:),     &
         Status_II(:,:), Br_II(:,:), VarsOut_VII(:,:,:)

    real :: rNow, LatSINow, LonSINow
    real :: LonMinPlot, LonMaxPlot, LatMinPlot, LatMaxPlot
    real :: OpenFluxPos, OpenFluxNeg, TmpArea

    character(len=*), parameter:: NameSub = 'get_open_closed_boundary'
    !--------------------------------------------------------------------------
    if (allocated(LonTrace_I))   deallocate(LonTrace_I)
    if (allocated(LatTrace_I))   deallocate(LatTrace_I)
    if (allocated(Status_II))    deallocate(Status_II)
    if (allocated(Br_II))        deallocate(Br_II)
    if (allocated(VarsOut_VII))  deallocate(VarsOut_VII)

    allocate(LonTrace_I(nLonBoundary), LatTrace_I(nLatBoundary), &
         Status_II(nLonBoundary, nLatBoundary),                  &
         Br_II(nLonBoundary, nLatBoundary),                      &
         VarsOut_VII(2, nLonBoundary, nLatBoundary))

    ! set up the lon-lat grid for tracing the fieldlines, in radian???
    dLonTrace = cTwoPi/nLonBoundary
    dLatTrace = cPi/nLatBoundary
    do iLatTrace = 1, nLatBoundary
       LatTrace_I(iLatTrace) = -cPi/2 + dLatTrace*(iLatTrace-0.5)
    end do
    do iLonTrace = 1, nLonBoundary
       LonTrace_I(iLonTrace) = dLonTrace*(iLonTrace-0.5)
    end do

    ! initialize
    Status_II  = 0
    Br_II      = 0

    ! define the maximum number of steps (iterations)
    nStepMax   = (rMaxB0Local-rMinB0Local)/SizeStep*10

    iTrace = -1
    do iLatTrace = 1, nLatBoundary
       do iLonTrace = 1, nLonBoundary
          ! parallel to save time
          iTrace = iTrace + 1
          if(mod(iTrace, nProc) /= iProc) CYCLE

          ! the starting point of the field line tracing
          rLonLatNow_D = [rBoundary, LonTrace_I(iLonTrace), &
               LatTrace_I(iLatTrace)]
          rNow     = rLonLatNow_D(1)
          LonSINow = rLonLatNow_D(2)*cRadToDeg
          LatSINow = rLonLatNow_D(3)*cRadToDeg
          call rlonlat_to_xyz(rLonLatNow_D, XyzNow_D)
          call interpolate_lookup_table(iTableB0, [rNow,LonSINow,LatSINow],  &
               B0Now_D, DoExtrapolate=(rLonLatNow_D(1)<rMinB0Local) )

          BrLocal = sum(B0Now_D*XyzNow_D)/norm2(XyzNow_D)
          Br_II(iLonTrace, iLatTrace) = BrLocal

          if (BrLocal > 0) then
             ! positive Br
             iDirection = 1
          else if (BrLocal < 0) then
             ! negative Br
             iDirection = -1
          else
             ! Br = 0, set the status to -1
             write(*,*) ' warning: Br = 0!!!!!'
             Status_II(iLonTrace,iLatTrace) = -1
             EXIT
          end if

          iStep = 0
          do
             ! field line integral to the next point
             XyzNext_D = XyzNow_D + iDirection*B0Now_D/norm2(B0Now_D)*SizeStep

             ! move the current point and update rLonLatNow_D
             XyzNow_D  = XyzNext_D
             call xyz_to_rlonlat(XyzNow_D, rLonLatNow_D)
             rNow     = rLonLatNow_D(1)
             LonSINow = rLonLatNow_D(2)*cRadToDeg
             LatSINow = rLonLatNow_D(3)*cRadToDeg

             iStep = iStep + 1
             if (iStep > nStepMax) then
                ! exceed the maximum interation
                Status_II(iLonTrace,iLatTrace) = -1
                EXIT
             end if

             if (rNow > rMaxB0Local) then
                ! open field region
                Status_II(iLonTrace,iLatTrace) = 2
                EXIT
             else if (rNow < rMinB0Local) then
                ! close field region
                Status_II(iLonTrace,iLatTrace) = 1
                EXIT
             end if

             ! update the magnetic field if still within the domain
             call interpolate_lookup_table(iTableB0, [rNow,LonSINow,LatSINow],&
               B0Now_D, DoExtrapolate=(rLonLatNow_D(1)<rMinB0Local) )
          end do
       end do
    end do

    if(nProc > 1)then
       call mpi_reduce_real_array(Status_II, size(Status_II), MPI_SUM, &
            0, iComm, iError)
       call mpi_reduce_real_array(Br_II,     size(Br_II),     MPI_SUM, &
            0, iComm, iError)
    end if

    if (iProc == 0) then
       OpenFluxPos = 0
       OpenFluxNeg = 0
       OpenArea    = 0

       Br_II = Br_II(:,:)! *1e4 ! seems in T in the lookup table

       do iLatTrace = 1, nLatBoundary; do iLonTrace = 1, nLonBoundary
          ! status == 2 is open region
          if (Status_II(iLonTrace, iLatTrace) == 2) then
             TmpArea  = cos(LatTrace_I(iLatTrace)) * rBoundary**2    &
                  *dLonTrace*dLatTrace
             OpenArea = OpenArea + TmpArea
             if (Br_II(iLonTrace, iLatTrace) > 0) then
                OpenFluxPos=OpenFluxPos + TmpArea*Br_II(iLonTrace, iLatTrace)
             else
                OpenFluxNeg=OpenFluxNeg - TmpArea*Br_II(iLonTrace, iLatTrace)
             endif
          end if
       end do; end do

       VarsOut_VII(1,:,:) = Status_II(:,:)
       VarsOut_VII(2,:,:) = Br_II(:,:)

       LonMinPlot = minval(LonTrace_I)*cRadToDeg + LonShift
       LonMaxPlot = maxval(LonTrace_I)*cRadToDeg + LonShift
       LatMinPlot = minval(LatTrace_I)*cRadToDeg
       LatMaxPlot = maxval(LatTrace_I)*cRadToDeg

       write(*,*) 'OpenFluxPos, OpenFluxNeg, OpenArea =', &
            OpenFluxPos, OpenFluxNeg, OpenArea

       call save_plot_file(NameFile = 'boundary_open_closed.out',         &
            nDimIn  = 2,                                                  &
            TimeIn = CRFraction,                                          &
            ParamIn_I=                                                    &
            [real(int(LonShift)), real(nCR), OpenFluxPos, OpenFluxNeg,    &
            OpenArea],                                                    &
            VarIn_VII= VarsOut_VII(:,:,:),                                &
            TypeFileIn    = 'ascii',                                      &
            CoordMinIn_D  = [LonMinPlot, LatMinPlot],                     &
            CoordMaxIn_D  = [LonMaxPlot, LatMaxPlot],                     &
            StringHeaderIn  = 'Created from '//trim(NameB0File),          &
            NameUnitsIn  =                                                &
            ' [deg] [deg] [] [Gs] [deg] [] [Gs*R_s^2] [Gs*R_s^2] [R_s^2]',&
            NameVarIn = 'Longitude Latitude Status Br'//                  &
            ' Long0 CRNumber OpenFluxPos OpenFluxNeg OpenArea')
    end if
    deallocate(LonTrace_I, LatTrace_I, Status_II, Br_II, VarsOut_VII)

  end subroutine get_open_closed_boundary
  !============================================================================
end module ModOpenClosedBoundary
!==============================================================================
