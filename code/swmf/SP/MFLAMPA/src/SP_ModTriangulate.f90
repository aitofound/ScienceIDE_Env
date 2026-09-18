!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModTriangulate

  use ModUtilities, ONLY: CON_stop
#ifdef _OPENACC
  use ModUtilities, ONLY: norm2
#endif
  use SP_ModGrid, ONLY: nLine, nLineAll, Used_B, iLineAll0, &
       search_line, MHData_VIB, X_, Y_, Z_, nP, nMu
  use SP_ModProc, ONLY: iProc, nProc, iComm, iError
  use SP_ModSize, ONLY: nDim
  use SP_ModTurbulence, ONLY: DPerp_CB, DPerp_IIBI

  implicit none

  SAVE

  PRIVATE ! Except

  public:: read_param           ! Read module parameters
  public:: reset_intersect_surf ! Reset arrays used in intersect_surf
  public:: intersect_surf       ! Intersect on a sphere with a given radius
  public:: build_trmesh         ! Triangulation on a sphere
  public:: interpolate_trmesh   ! Interpolate at the specified location

  ! In all names below "Tri" means "Triangulation"
  ! If we use poles in triangulation
  logical, public :: UsePoleTri   = .true.
  logical, public :: UsePlanarTri = .true.
  real,    public, parameter :: iSouthPoleTri_ = -1, iNorthPoleTri_ = -2
  ! # of intersection points of the MFLAMPA grid lines with
  ! the spherical surface, nReachR
  ! # of points involed into triangilation (equals to nReachR or nReach+2)
  integer, public, allocatable :: nTriMesh_I(:)
  ! Locations of the lines intersection with the spherical surface. Regardless
  ! of the sphere radius, the coordinates are divided by this radius, hence,
  ! are the points at the unit sphere. The point number may be equal to
  ! nLineAll, the total number of lines, or two extra points at the poles
  ! may be added. If some lines are unused or do not reach the surface, the
  ! last elements of the array are not used
  real, allocatable :: Xyz_DII(:,:,:)
  real, public, allocatable :: XyzOrig_DII(:,:,:)
  ! Index of the line correspoinding to a given point at the radial surface
  ! It may differ from just the array index because of the presence of polar
  ! points or absence of bad lines not reaching the boundary.
  integer, allocatable :: iLineReach_II(:,:)
  ! Distribution function interpolated to the points at the spherical surface
  real, allocatable    :: DistrR_IIBI(:,:,:,:)
  ! The result of triangulation
  integer, allocatable :: iList_I(:), iPointer_I(:), iEnd_I(:)
contains
  !============================================================================
  subroutine read_param(NameCommand)

    use ModReadParam, ONLY: read_var
    character(len=*), intent(in) :: NameCommand ! From PARAM.in
    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case('#TRIANGULATION')
       ! get pole triangulartion flag
       call read_var('UsePoleTriangulation', UsePoleTri)
       ! get the triangulation approach flag
       call read_var('UsePlanarTriangles', UsePlanarTri)
    case default
       call CON_stop(NameSub//': Unknown command '//NameCommand)
    end select

  end subroutine read_param
  !============================================================================
  subroutine reset_intersect_surf(nR)
    ! Since there can be multiple calls (`ModDiffusion` or `ModPlot` or
    ! `ModSatellite`) of `intersect_surf`, another subroutine should be
    ! called from ModDiffusion to reset array bounds:
    integer, intent(in) :: nR ! total # of spherical coordinate surfaces
    !  integer, intent(in) :: iRfirst, iRlast ! First and last # of surfaces
    ! assigned to a given Proc
    character(len=*), parameter:: NameSub = 'reset_intersect_surf'
    !--------------------------------------------------------------------------
    if(allocated(Xyz_DII)) deallocate(Xyz_DII, iLineReach_II, &
         DistrR_IIBI, nTriMesh_I, iList_I, iPointer_I, iEnd_I)
    if(allocated(XyzOrig_DII)) deallocate(XyzOrig_DII, DPerp_IIBI)
    allocate(Xyz_DII(X_:Z_, 1:nLineAll+2, nR))
    allocate(XyzOrig_DII(X_:Z_, 1:nLineAll+2, nR))
    allocate(DistrR_IIBI(0:nP+1, 1:nMu, 1:nLineAll+2, nR))
    allocate(DPerp_IIBI(1:nP, 1:nMu, 1:nLineAll+2, nR))
    allocate(iLineReach_II(1:nLineAll+2, nR))
    allocate(nTriMesh_I(nR)); nTriMesh_I = 0
    allocate(iList_I(6*nLineAll))
    allocate(iPointer_I(6*nLineAll))
    allocate(iEnd_I(nLineAll+2))
  end subroutine reset_intersect_surf
  !============================================================================
  subroutine intersect_surf(rSurf, iRootIn, iRIn, IsIncludeDPerpIn)

    use ModMpi
    use SP_ModDistribution, ONLY: Distribution_CB
    ! Input: radius of the spherical surface
    real,    intent(in) :: rSurf
    integer, optional, intent(in) :: iRootIn, iRIn
    logical, optional, intent(in) :: IsIncludeDPerpIn

    ! called from Dperp (=> true) or ModPlot (=> false, default)
    logical :: IsIncludeDPerp
    ! loop variables
    integer :: iLine, iReachR
    integer :: iRoot, iR, nReachR
    ! indexes of corresponding node, latitude and longitude
    integer :: iLineAll
    ! index of particle just above the radius
    integer :: iAboveR
    ! interpolation weight (in radial direction)
    real    :: WeightR
    ! skip a field line not reaching radius of output sphere
    logical :: DoReachR
    character(len=*), parameter:: NameSub = 'intersect_surf'
    !--------------------------------------------------------------------------
    ! Check whether we need to include Dperp
    if(present(IsIncludeDPerpIn)) then
       IsIncludeDPerp = IsIncludeDPerpIn
    else
       IsIncludeDPerp = .false.
    end if

    ! For full implementation of uniform spherical grid for solving
    ! perpendicular diffusion and drift, the lines below should be
    ! shaped as subroutine init
    if(.not.allocated(Xyz_DII)) then
       allocate(Xyz_DII(X_:Z_, 1:nLineAll+2, 1))
       allocate(DistrR_IIBI(0:nP+1, 1:nMu, 1:nLineAll+2, 1))
       if(IsIncludeDPerp) then
          ! In DPerp
          allocate(XyzOrig_DII(X_:Z_, 1:nLineAll+2, 1))
          allocate(DPerp_IIBI(1:nP, 1:nMu, 1:nLineAll+2, 1))
       end if
       allocate(iLineReach_II(1:nLineAll+2, 1))
       allocate(nTriMesh_I(1)); nTriMesh_I = 0
       allocate(iList_I(6*nLineAll))
       allocate(iPointer_I(6*nLineAll))
       allocate(iEnd_I(nLineAll+2))
    end if
    if(present(iRootIn)) then
       iRoot = iRootIn
    else
       iRoot = 0
    end if
    if(present(iRin)) then
       iR = iRin
    else
       iR = 1
    end if
    Xyz_DII(:,:,iR) = 0.0; iLineReach_II(:,iR) = 0
    DistrR_IIBI(:,:,:,iR) = 0.0
    if(IsIncludeDPerp) DPerp_IIBI(:,:,:,iR) = 0.0

    ! go over all lines on the processor and find the point of
    ! intersection with output sphere if present
    LINE:do iLine = 1, nLine
       if(.not.Used_B(iLine)) CYCLE LINE
       iLineAll = iLineAll0 + iLine

       ! Find the particle just above the given radius
       call search_line(iLine, rSurf, iAboveR, DoReachR, WeightR)
       DoReachR = DoReachR .and. iAboveR /= 1
       ! if no intersection found -> proceed to the next line
       if(.not.DoReachR) CYCLE LINE

       ! Found intersection => get POS & log10(f[IO_Unit]) at that location
       ! Find coordinates and log(Distribution) at intersection
       Xyz_DII(:, iLineAll, iR) = ( &
            MHData_VIB(X_:Z_, iAboveR-1, iLine)*(1-WeightR) +  &
            MHData_VIB(X_:Z_, iAboveR,   iLine)*   WeightR )
       DistrR_IIBI(:, :, iLineAll, iR) = ( &
            Distribution_CB(:, :, iAboveR,   iLine)* WeightR + &
            Distribution_CB(:, :, iAboveR-1, iLine)*(1-WeightR))
       if(IsIncludeDPerp) DPerp_IIBI(:, :, iLineAll, iR) = ( &
            DPerp_CB(:, :, iAboveR,   iLine)* WeightR + &
            DPerp_CB(:, :, iAboveR-1, iLine)*(1-WeightR))
    end do LINE !  iLine

    ! Gather interpolated coordinates on the source processor and Broadcast
    if(nProc > 1) then
       ! The entire coordinate array is sent to iRoot, however, the unused
       ! line intersections are marked with all coordinates set to zero.
       call mpi_reduce_real_array(Xyz_DII(:,:,iR), 3*nLineAll, MPI_SUM, &
            iRoot, iComm, iError)
       call mpi_reduce_real_array(DistrR_IIBI(:,:,:,iR), &
            (nP+2)*nMu*nLineAll, MPI_SUM, iRoot, iComm, iError)
       if(IsIncludeDPerp) call mpi_reduce_real_array(DPerp_IIBI(:,:,:,iR), &
            nP*nMu*nLineAll, MPI_SUM, iRoot, iComm, iError)
    end if
    ! Save original intersection points
    if(IsIncludeDPerp) XyzOrig_DII(:,:,iR) = Xyz_DII(:,:,iR)
    if(iProc /= iRoot) RETURN

    ! Sort out unused points
    iReachR = 0
    do iLineAll = 1, nLineAll
       if(all(Xyz_DII(:,iLineAll,iR)==0.0)) CYCLE
       iReachR = iReachR + 1
       ! Store and normalize coordinates to get points on the unit square
       Xyz_DII(:,iReachR,iR) = Xyz_DII(:,iLineAll,iR)/rSurf
       iLineReach_II(iReachR,iR) = iLineAll
       DistrR_IIBI(:,:,iReachR,iR) = DistrR_IIBI(:,:,iLineAll,iR)
       if(IsIncludeDPerp) &
            DPerp_IIBI(:,:,iReachR,iR) = DPerp_IIBI(:,:,iLineAll,iR)
    end do
    if(iReachR == 0) call CON_stop(NameSub//': No valid intersection points!')
    nReachR = iReachR

    ! For poles
    if(UsePoleTri) then
       ! Add two grid nodes at the poles:
       nTriMesh_I(iR) = nReachR + 2
       Xyz_DII(:, 2:nTriMesh_I(iR)-1,iR)    = Xyz_DII(:, 1:nReachR,iR)
       Xyz_DII(:, 1, iR)                    = [0.0, 0.0, -1.0]
       Xyz_DII(:, nTriMesh_I(iR), iR)       = [0.0, 0.0, +1.0]
       iLineReach_II(2:nTriMesh_I(iR)-1,iR) = iLineReach_II(1:nReachR,iR)
       iLineReach_II(1,iR)                  = iSouthPoleTri_
       iLineReach_II(nTriMesh_I(iR),iR)     = iNorthPoleTri_
       DistrR_IIBI(:,:,2:nTriMesh_I(iR)-1,iR) = DistrR_IIBI(:,:,1:nReachR,iR)
    else
       nTriMesh_I(iR)= nReachR
    end if
  end subroutine intersect_surf
  !============================================================================
  subroutine build_trmesh(iRIn, IsIncludeDPerpIn)

    use ModTriangulateSpherical, ONLY: trmesh, fix_state
    integer, optional, intent(in) :: iRIn
    logical, optional, intent(in) :: IsIncludeDPerpIn

    ! called from Dperp (=> true) or ModPlot (=> false, default)
    logical :: IsIncludeDPerp
    integer :: iR, iMu
    character(len=*), parameter:: NameSub = 'build_trmesh'
    !--------------------------------------------------------------------------
    ! Check whether we need to include Dperp
    if(present(IsIncludeDPerpIn)) then
       IsIncludeDPerp = IsIncludeDPerpIn
    else
       IsIncludeDPerp = .false.
    end if
    if(present(iRIn)) then
       iR = iRIn
    else
       iR = 1
    end if
    iList_I = 0; iPointer_I = 0; iEnd_I = 0

    ! Construct the Triangular mesh used for interpolation
    call trmesh(nTriMesh_I(iR),             &
         Xyz_DII(X_, 1:nTriMesh_I(iR), iR), &
         Xyz_DII(Y_, 1:nTriMesh_I(iR), iR), &
         Xyz_DII(Z_, 1:nTriMesh_I(iR), iR), &
         iList_I(:6*(nTriMesh_I(iR)-2)),    &
         iPointer_I(:6*(nTriMesh_I(iR)-2)), &
         iEnd_I(:nTriMesh_I(iR)), iError)
    ! Fix states (for the given function) at the polar nodes
    if(UsePoleTri) then
       do iMu = 1, nMu
          ! North:
          call fix_state( &
               iNodeToFix = nTriMesh_I(iR),                    &
               nNode      = nTriMesh_I(iR),                    &
               iList_I    = iList_I(:6*(nTriMesh_I(iR)-2)),    &
               iPointer_I = iPointer_I(:6*(nTriMesh_I(iR)-2)), &
               iEnd_I     = iEnd_I(:nTriMesh_I(iR)),           &
               Xyz_DI     = Xyz_DII(:,1:nTriMesh_I(iR),iR),    &
               nVar       = nP+2,                              &
               State_VI   = DistrR_IIBI(:,iMu,1:nTriMesh_I(iR),iR))
          ! South:
          call fix_state( &
               iNodeToFix = 1,                                 &
               nNode      = nTriMesh_I(iR),                    &
               iList_I    = iList_I(:6*(nTriMesh_I(iR)-2)),    &
               iPointer_I = iPointer_I(:6*(nTriMesh_I(iR)-2)), &
               iEnd_I     = iEnd_I(:nTriMesh_I(iR)),           &
               Xyz_DI     = Xyz_DII(:,1:nTriMesh_I(iR),iR),    &
               nVar       = nP+2,                              &
               State_VI   = DistrR_IIBI(:, iMu, 1:nTriMesh_I(iR),iR))
       end do
       if(IsIncludeDPerp) then
          do iMu = 1, nMu
             ! North:
             call fix_state( &
                  iNodeToFix = nTriMesh_I(iR),                    &
                  nNode      = nTriMesh_I(iR),                    &
                  iList_I    = iList_I(:6*(nTriMesh_I(iR)-2)),    &
                  iPointer_I = iPointer_I(:6*(nTriMesh_I(iR)-2)), &
                  iEnd_I     = iEnd_I(:nTriMesh_I(iR)),           &
                  Xyz_DI     = Xyz_DII(:,1:nTriMesh_I(iR),iR),    &
                  nVar       = nP,                                &
                  State_VI   = DPerp_IIBI(:,iMu,1:nTriMesh_I(iR),iR))
             ! South:
             call fix_state( &
                  iNodeToFix = 1,                                 &
                  nNode      = nTriMesh_I(iR),                    &
                  iList_I    = iList_I(:6*(nTriMesh_I(iR)-2)),    &
                  iPointer_I = iPointer_I(:6*(nTriMesh_I(iR)-2)), &
                  iEnd_I     = iEnd_I(:nTriMesh_I(iR)),           &
                  Xyz_DI     = Xyz_DII(:,1:nTriMesh_I(iR),iR),    &
                  nVar       = nP,                                &
                  State_VI   = DPerp_IIBI(:,iMu, 1:nTriMesh_I(iR),iR))
          end do
       end if
    end if
  end subroutine build_trmesh
  !============================================================================
  subroutine interpolate_trmesh(XyzInterp_D, iRIn, &
       DistrInterp_II, DPerp_II, iStencilOut_I, WeightOut_I)

    use ModTriangulateSpherical, ONLY: find_triangle_orig, find_triangle_sph
    ! Input: Xyz coordinates of the point for interpolations
    real,    intent(in)   :: XyzInterp_D(nDim)
    integer, optional, intent(in) :: iRIn
    ! Output: Interpolated values at XyzInterp_D
    real,    optional, intent(out):: DistrInterp_II(0:nP+1,1:nMu)
    real,    optional, intent(out):: DPerp_II(1:nP,1:nMu)
    ! variables for interpolation in a triangular mesh
    integer, optional, intent(out):: iStencilOut_I(3)
    real,    optional, intent(out):: WeightOut_I(3)

    ! local variables
    logical :: IsTriangleFound
    integer :: iStencil_I(3)
    real    :: Weight_I(3)
    ! loop variables
    integer :: i, iR
    character(len=*), parameter:: NameSub = 'interpolate_trmesh'
    !--------------------------------------------------------------------------
    if(present(iRIn)) then
       iR = iRIn
    else
       iR = 1
    end if
    IsTriangleFound = .false.
    if(present(iStencilOut_I)) iStencilOut_I = -1
    if(present(WeightOut_I)) WeightOut_I = 0.0
    if(present(DistrInterp_II)) DistrInterp_II = 0.0
    if(present(DPerp_II)) DPerp_II = 0.0

    ! Find the triangle where the satellite locates
    if(UsePlanarTri) then
       call find_triangle_orig(               &
            XyzInterp_D/norm2(XyzInterp_D),   &
            nTriMesh_I(iR),                   &
            Xyz_DII(:,1:nTriMesh_I(iR),iR),   &
            iList_I(:6*(nTriMesh_I(iR)-2)),   &
            iPointer_I(:6*(nTriMesh_I(iR)-2)),&
            iEnd_I(:nTriMesh_I(iR)),          &
            Weight_I, IsTriangleFound, iStencil_I)
    else
       call find_triangle_sph(                &
            XyzInterp_D/norm2(XyzInterp_D),   &
            nTriMesh_I(iR),                   &
            Xyz_DII(:,1:nTriMesh_I(iR),iR),   &
            iList_I(:6*(nTriMesh_I(iR)-2)),   &
            iPointer_I(:6*(nTriMesh_I(iR)-2)),&
            iEnd_I(:nTriMesh_I(iR)),          &
            Weight_I(1), Weight_I(2),         &
            Weight_I(3), IsTriangleFound,     &
            iStencil_I(1), iStencil_I(2), iStencil_I(3))
    end if
    if(.not.IsTriangleFound) RETURN
    Weight_I = max(Weight_I, 0.0)

    ! For outputs as requested in the interface
    if(present(DistrInterp_II)) then
       ! Interpolate the distribution function at satellite as outputs
       do i = 1, 3
          DistrInterp_II = DistrInterp_II + &
               DistrR_IIBI(:,:,iStencil_I(i),iR)*Weight_I(i)
       end do
    end if
    if(present(DPerp_II)) then
       ! Interpolate the Dperp coefficient as outputs
       do i = 1, 3
          DPerp_II = DPerp_II + &
               DPerp_IIBI(:,:,iStencil_I(i),iR)*Weight_I(i)
       end do
    end if
    if(present(iStencilOut_I)) then
       do i = 1, 3
          ! Recover iLineAll corresponding to iStencil
          iStencilOut_I(i) = iLineReach_II(iStencil_I(i),iR)
       end do
    end if
    if(present(WeightOut_I)) WeightOut_I = Weight_I
  end subroutine interpolate_trmesh
  !============================================================================
end module SP_ModTriangulate
!==============================================================================
