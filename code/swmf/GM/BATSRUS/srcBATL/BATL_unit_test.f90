!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf

module BATL_unit_test

  ! Unit tests for BATL

  use BATL_lib

  ! public entities not passed to BATL_lib
  use BATL_tree, ONLY: di_level_nei, Child1_, iProcNew_A
  use BATL_amr, ONLY: do_amr, init_amr

  ! Other things
  use ModNumConst, ONLY: cPi, cTwoPi, cHalfPi
  use ModUtilities, ONLY: CON_stop
  use ModRandomNumber, ONLY: random_real
  use ModMpi
#ifdef _OPENACC
  use ModUtilities, ONLY: norm2
#endif

  implicit none

  private ! except

  public:: test_tree
  public:: test_geometry
  public:: test_grid
  public:: test_pass_cell
  public:: test_pass_face
  public:: test_pass_node
  public:: test_amr
  public:: test_amr_criteria

  ! Private data for test_amr
  integer, parameter:: nExtraData = 2
  real, allocatable:: ExtraData_IB(:, :)

contains
  !============================================================================
  subroutine test_tree ! unit test

    integer, parameter:: MaxBlockTest = 50
    integer, parameter:: nRootTest_D(MaxDim) = [3, 2, 1]
    logical, parameter:: IsPeriodicTest_D(MaxDim) = [.true., .true., .false.]
    real, parameter:: CoordTest_D(MaxDim) = 0.99

    integer :: iNode, iCoord_D(MaxDim), iCell_D(MaxDim)
    real    :: Distance_D(MaxDim), DistanceGood_D(MaxDim), CellSize_D(MaxDim)

    integer :: iNodeCell_II(0:nDim, 2**nDim), iNodeCellGood_II(0:3, 8)
    real    :: Weight_I(2**nDim), WeightGood_I(8)

    integer, allocatable:: iTypeNode_I(:)

    logical:: DoTest
    character(len=*), parameter:: NameSub = 'test_tree'
    !--------------------------------------------------------------------------
    DoTest = iProc == 0

    if (DoTest) write(*,*) 'Starting ', NameSub
    if (DoTest) write(*,*) 'Testing init_tree'
    call init_tree(MaxBlockTest)
    if (MaxBlock /= MaxBlockTest) &
         write(*,*) 'init_tree failed, MaxBlock=', &
         MaxBlock, ' should be ', MaxBlockTest
    if (MaxNode /= 2*ceiling(MaxBlockTest*nProc*(1 + 1.0/(2**nDimAmr - 1)))) &
         write(*,*) 'init_tree failed, MaxNode=', MaxNode, &
         ' should be', 2*ceiling(50*nProc*(1 + 1.0/(2**nDimAmr - 1)))

    if (DoTest) write(*,*) 'Testing init_geometry'
    call init_geometry('cartesian', IsPeriodicTest_D(1:nDim))
    if (any(IsPeriodic_D(1:nDim) .neqv. IsPeriodicTest_D(1:nDim))) &
         write(*,*) 'init_geometry failed, IsPeriodic_D=', &
         IsPeriodic_D(1:nDim), ' should be ', IsPeriodicTest_D(1:nDim)

    if (DoTest) write(*,*) 'Testing i_node_new()'
    iNode = i_node_new()
    if (iNode /= 1) &
         write(*,*) 'i_node_new() failed, iNode=', iNode, ' should be 1'

    if (DoTest) write(*,*) 'Testing set_tree_root'
    call set_tree_root(nRootTest_D(1:nDim))

    if (DoTest) call show_tree('after set_tree_root')

    if (any(nRoot_D(1:nDim) /= nRootTest_D(1:nDim))) &
         write(*,*) 'set_tree_root failed, nRoot_D=', nRoot_D(1:nDim), &
         ' should be ', nRootTest_D(1:nDim)

    iCoord_D = [3, 1, 1]

    if (any(iTree_IA(Coord1_:Coord0_ + nDim, 3) /= iCoord_D(1:nDim))) &
         write(*,*) 'set_tree_root failed, coordinates of node four=', &
         iTree_IA(Coord1_:Coord0_ + nDim, 3), ' should be ', iCoord_D(1:nDim)

    if (DoTest) write(*,*) 'Testing find_tree_cell'
    call find_tree_cell(CoordTest_D, iNode, iCell_D, Distance_D)
    if (iNode /= nRoot) write(*,*) 'ERROR: Test find point failed, iNode=', &
         iNode, ' instead of', nRoot

    if (.not. is_point_inside_node(CoordTest_D, iNode)) &
         write(*,*) 'ERROR: Test find point failed'

    if (any(iCell_D(1:nDim) /= nIJK_D(1:nDim))) &
         write(*,*) 'ERROR: Test find point failed, iCell_D=', &
         iCell_D(1:nDim), ' instead of', nIjk_D(1:nDim)

    ! Cell size in units where the whole domain is 1.0
    CellSize_D = 1.0/(nRoot_D*nIJK_D)
    ! Distance to the last grid cell, normalized to the cell size
    DistanceGood_D = (CoordTest_D - (1.0 - CellSize_D/2))/CellSize_D
    if (any(abs(Distance_D(1:nDim) - DistanceGood_D(1:nDim)) > 1e-6)) &
         write(*,*) 'ERROR: Test find point failed, Distance_D=', &
         Distance_D(1:nDim), ' instead of ', DistanceGood_D(1:nDim)

    if (DoTest) write(*,*) 'Testing interpolate_tree'
    call interpolate_tree(CoordTest_D, iNodeCell_II, Weight_I)
    select case (nDim)
    case (1)
       iNodeCellGood_II(0:1, 1:2) = reshape([nRoot, nI, nRoot, nI + 1], &
            [2, 2])
       WeightGood_I(1:2) = [1 - Distance_D(1), Distance_D(1)]
    case (2)
       iNodeCellGood_II(0:2, 1:4) = reshape( &
            [nRoot, nI, nJ, nRoot, nI + 1, nJ, nRoot, nI, nJ + 1, nRoot, &
            nI + 1, nJ + 1], &
            [3, 4])
       WeightGood_I(1:4) = [ &
            (1 - Distance_D(1))*(1 - Distance_D(2)), &
            Distance_D(1)*(1 - Distance_D(2)), &
            (1 - Distance_D(1))*Distance_D(2), &
            Distance_D(1)*Distance_D(2) &
            ]
    case (3)
       iNodeCellGood_II(0:3, 1:8) = reshape([ &
            nRoot, nI, nJ, nK, &
            nRoot, nI + 1, nJ, nK, nRoot, nI, nJ + 1, nK, nRoot, &
            nI + 1, nJ + 1, nK, &
            nRoot, nI, nJ, nK + 1, &
            nRoot, nI + 1, nJ, nK + 1, nRoot, nI, nJ + 1, nK + 1, nRoot, &
            nI + 1, nJ + 1, nK + 1], &
            [4, 8])
       WeightGood_I(1:8) = [ &
            (1 - Distance_D(1))*(1 - Distance_D(2))*(1 - Distance_D(3)), &
            Distance_D(1)*(1 - Distance_D(2))*(1 - Distance_D(3)), &
            (1 - Distance_D(1))*Distance_D(2)*(1 - Distance_D(3)), &
            Distance_D(1)*Distance_D(2)*(1 - Distance_D(3)), &
            (1 - Distance_D(1))*(1 - Distance_D(2))*Distance_D(3), &
            Distance_D(1)*(1 - Distance_D(2))*Distance_D(3), &
            (1 - Distance_D(1))*Distance_D(2)*Distance_D(3), &
            Distance_D(1)*Distance_D(2)*Distance_D(3) &
            ]
    end select

    if (any(iNodeCell_II /= iNodeCellGood_II(0:nDim, 1:2**nDim))) &
         write(*,*) 'ERROR: Test interpolate_tree failed, iNodeCell_II=', &
         iNodeCell_II, ' instead of ', iNodeCellGood_II

    if (any(abs(Weight_I - WeightGood_I(1:2**nDim)) > 1e-6)) &
         write(*,*) 'ERROR: Test interpolate_tree failed, Weight_I=', &
         Weight_I, ' instead of ', WeightGood_I

    if (DoTest) write(*,*) 'Testing distribute_tree 1st'
    call distribute_tree(.true.)
    if (DoTest) call show_tree('after distribute_tree 1st', .true.)

    if (DoTest) write(*,*) 'Testing refine_tree_node'
    ! Refine the node where the point was found and find it again
    call refine_tree_node(iNode)

    if (DoTest) write(*,*) 'Testing distribute_tree 2nd'

    ! Set node type to level+1
    allocate (iTypeNode_I(MaxNode))
    iTypeNode_I = 1 + iTree_IA(Level_, :)
    call distribute_tree(.true., iTypeNode_I)
    if (DoTest) call show_tree('after distribute_tree with type=level', .true.)

    ! Set node type to the second coordinate index
    iTypeNode_I = iTree_IA(Coord2_, :)
    call distribute_tree(.true., iTypeNode_I)
    if (DoTest) &
         call show_tree('after distribute_tree with type=Coord2', .true.)

    ! Use default (single type)
    call distribute_tree(.true.)
    if (DoTest) call show_tree('after distribute_tree 2nd', .true.)

    call find_tree_node(CoordTest_D, iNode)
    if (.not. is_point_inside_node(CoordTest_D, iNode)) &
         write(*,*) 'ERROR: Test find point failed for iNode=', iNode

    ! Refine another node
    if (DoTest) write(*,*) 'nRoot=', nRoot
    call refine_tree_node(2)

    if (DoTest) call show_tree('after another refine_tree_node')

    if (DoTest) write(*,*) 'Testing coarsen_tree_node'

    ! Coarsen back the last root node and find point again
    call coarsen_tree_node(nRoot)
    if (DoTest) call show_tree('after coarsen_tree_node')

    ! Distribute the new tree
    if (DoTest) write(*,*) 'Testing distribute_tree 3rd'
    call distribute_tree(.true.)
    if (DoTest) call show_tree('after distribute_tree 3rd', .true.)

    call find_tree_node(CoordTest_D, iNode)
    if (iNode /= nRoot) write(*,*) &
         'ERROR: coarsen_tree_node+compact failed, iNode=', &
         iNode, ' instead of', nRoot
    if (.not. is_point_inside_node(CoordTest_D, iNode)) &
         write(*,*) 'ERROR: is_point_inside_node failed'

    if (iTree_IA(Status_, nNode + 1) /= Unset_) &
         write(*,*) 'ERROR: compact_tree failed, nNode=', nNode, &
         ' but status of next node is', iTree_IA(Status_, nNode + 1), &
         ' instead of ', Unset_
    if (any(iTree_IA(Status_, 1:nNode) == Unset_)) &
         write(*,*) 'ERROR: compact_tree failed, nNode=', nNode, &
         ' but iTree_IA(Status_, 1:nNode)=', &
         iTree_IA(Status_, 1:nNode), ' contains unset=', Unset_
    call find_tree_node(CoordTest_D, iNode)
    if (iNode /= nRoot) write(*,*) 'ERROR: compact_tree faild, iNode=', &
         iNode, ' instead of', nRoot
    if (.not. is_point_inside_node(CoordTest_D, iNode)) &
         write(*,*) 'ERROR: is_point_inside_node failed'

    if (DoTest) write(*,*) 'Testing write_tree_file'
    call write_tree_file('tree.rst')

    if (DoTest) write(*,*) 'Testing read_tree_file'
    iTree_IA = Unset_
    nRoot_D = 0
    call read_tree_file('tree.rst')
    if (DoTest) call show_tree('after read_tree')

    call find_tree_node(CoordTest_D, iNode)
    if (iNode /= nRoot) write(*,*) 'ERROR: compact_tree failed, iNode=', &
         iNode, ' instead of', nRoot

    if (DoTest) write(*,*) 'Testing distribute_tree 4th'
    call distribute_tree(.true.)
    if (DoTest) call show_tree('after distribute_tree 4th', .true.)

    if (DoTest) write(*,*) 'Testing clean_tree'
    call clean_tree
    if (DoTest) write(*,*) 'MaxNode=', MaxNode

  end subroutine test_tree
  !============================================================================
  subroutine test_geometry ! unit test

    logical:: IsPeriodicTest_D(MaxDim)
    real:: Xyz_D(MaxDim), Coord_D(MaxDim), Good_D(MaxDim)

    real:: r, GenR
    real:: Rgen_I(5) = [1.0, 1.2, 5.0, 25.0, 100.0]

    logical:: DoTest
    character(len=*), parameter:: NameSub = 'test_geometry'
    !--------------------------------------------------------------------------
    DoTest = iProc == 0

    if (DoTest) write(*,*) 'Starting ', NameSub
    if (DoTest) write(*,*) 'Testing init_geometry for Cartesian'
    call init_geometry

    if (TypeGeometryBatl /= 'cartesian') &
         write(*,*) 'ERROR: init_geometry failed, ', &
         'TypeGeometryBatl=', TypeGeometryBatl, &
         ' should be Cartesian by default'

    if (.not. IsCartesian .or. IsRotatedCartesian .or. &
         IsRzGeometry .or. IsSpherical .or. IsCylindrical) &
         write(*,*) 'ERROR: init_geometry failed for Cartesian grid, ', &
         'IsCartesian, IsRzGeometry, IsSpherical, IsCylindrical=', &
         IsCartesian, IsRzGeometry, IsSpherical, IsCylindrical

    if (IsLogRadius .or. IsGenRadius) &
         write(*,*) 'ERROR: init_geometry failed for Cartesian grid, ', &
         'IsLogRadius, IsGenRadius =', IsLogRadius, IsGenRadius

    if (any(IsPeriodic_D)) &
         write(*,*) 'ERROR: init_geometry failed, ', &
         'IsPeriodic_D =', IsPeriodic_D, ' should be all false'

    if (DoTest) write(*,*) 'Testing xyz_to_coord for Cartesian'
    Xyz_D = [1., 2., 3.]
    call xyz_to_coord(Xyz_D, Coord_D)
    if (any(Coord_D /= Xyz_D)) &
         write(*,*) 'ERROR: xyz_to_coord failed for Cartesian grid, ', &
         'Xyz_D =', Xyz_D, ' Coord_D =', Coord_D

    if (DoTest) write(*,*) 'Testing coord_to_xyz for Cartesian'
    call coord_to_xyz(Coord_D, Xyz_D)
    if (any(Coord_D /= Xyz_D)) &
         write(*,*) 'ERROR: coord_to_xyz failed for Cartesian grid, ', &
         'Xyz_D =', Xyz_D, ' Coord_D =', Coord_D

    if (nDim == 2) then
       if (DoTest) write(*,*) 'Testing init_geometry for RZ geometry'
       IsPeriodicTest_D = [.true., .false., .false.]

       call init_geometry('rz', IsPeriodicIn_D=IsPeriodicTest_D(1:nDim))

       if (TypeGeometryBatl /= 'rz') &
            write(*,*) 'ERROR: init_geometry failed, ', &
            'TypeGeometryBatl=', TypeGeometryBatl, ' should be rz'

       if (.not. IsRzGeometry .or. IsCartesian .or. IsSpherical .or. &
            IsCylindrical) &
            write(*,*) 'ERROR: init_geometry failed for RZ grid, ', &
            'IsCartesian, IsRzGeometry, IsSpherical, IsCylindrical=', &
            IsCartesian, IsRzGeometry, IsSpherical, IsCylindrical

       if (IsLogRadius .or. IsGenRadius) &
            write(*,*) 'ERROR: init_geometry failed for RZ grid, ', &
            'IsLogRadius, IsGenRadius =', IsLogRadius, IsGenRadius

       if (any(IsPeriodic_D(1:nDim) .neqv. IsPeriodicTest_D(1:nDim))) &
            write(*,*) 'ERROR: init_geometry failed, ', &
            'for TypeGeometryBatl=', TypeGeometryBatl, &
            'IsPeriodic_D =', IsPeriodic_D(1:nDim), &
            ' should be ', IsPeriodicTest_D(1:nDim)

       if (DoTest) write(*,*) 'Testing xyz_to_coord for RZ geometry'
       Xyz_D = [1., 2., 3.]
       call xyz_to_coord(Xyz_D, Coord_D)
       if (any(Coord_D /= Xyz_D)) &
            write(*,*) 'ERROR: xyz_to_coord failed for RZ grid, ', &
            'Xyz_D =', Xyz_D, ' Coord_D =', Coord_D

       if (DoTest) write(*,*) 'Testing coord_to_xyz for RZ geometry'
       call coord_to_xyz(Coord_D, Xyz_D)
       if (any(Coord_D /= Xyz_D)) &
            write(*,*) 'ERROR: coord_to_xyz failed for RZ grid, ', &
            'Xyz_D =', Xyz_D, ' Coord_D =', Coord_D

    end if

    if (nDim == 1) RETURN

    if (DoTest) write(*,*) 'Testing init_geometry for cylindrical_lnr'
    IsPeriodicTest_D = [.false., .true., .true.]

    call init_geometry('cylindrical_lnr', &
         IsPeriodicIn_D=IsPeriodicTest_D(1:nDim))

    if (TypeGeometryBatl /= 'cylindrical_lnr') &
         write(*,*) 'ERROR: init_geometry failed, ', &
         'TypeGeometryBatl=', TypeGeometryBatl, ' should be cylindrical_lnr'

    if (.not. IsCylindrical .or. IsCartesian .or. IsRzGeometry .or. &
         IsSpherical) &
         write(*,*) 'ERROR: init_geometry failed for cylindrical_lnr, ', &
         'IsCartesian, IsRzGeometry, IsSpherical, IsCylindrical=', &
         IsCartesian, IsRzGeometry, IsSpherical, IsCylindrical

    if (.not. IsLogRadius .or. IsGenRadius) &
         write(*,*) 'ERROR: init_geometry failed for cylindrical_lnr, ', &
         'IsLogRadius, IsGenRadius =', IsLogRadius, IsGenRadius

    if (any(IsPeriodic_D(1:nDim) .neqv. IsPeriodicTest_D(1:nDim))) &
         write(*,*) 'ERROR: init_geometry failed, ', &
         'for TypeGeometryBatl=', TypeGeometryBatl, &
         'IsPeriodic_D =', IsPeriodic_D(1:nDim), &
         ' should be ', IsPeriodicTest_D(1:nDim)

    if (DoTest) write(*,*) 'Testing xyz_to_coord for cylindrical_lnr'
    Xyz_D = [1., 2., 3.]
    Good_D = [log(sqrt(5.)), atan2(2., 1.), 3.]
    call xyz_to_coord(Xyz_D, Coord_D)
    if (any(abs(Coord_D - Good_D) > 1e-6)) &
         write(*,*) 'ERROR: xyz_to_coord failed for cylindrical_lnr, ', &
         'Xyz_D =', Xyz_D, ' Coord_D =', Coord_D, ' should be ', Good_D

    if (DoTest) write(*,*) 'Testing init_geometry for roundcube'
    IsPeriodicTest_D = [.false., .false., .false.]

    call init_geometry('roundcube', &
         IsPeriodicIn_D=IsPeriodicTest_D(1:nDim))

    if (TypeGeometryBatl /= 'roundcube') &
         write(*,*) 'ERROR: init_geometry failed, ', &
         'TypeGeometryBatl=', TypeGeometryBatl, ' should be roundcube'

    if (.not. IsRoundCube .or. IsCartesian .or. IsRzGeometry &
         .or. IsCylindrical .or. IsSpherical) &
         write(*,*) 'ERROR: init_geometry failed for roundcube, ', &
         'IsRoundCube,IsCartesian,IsRzGeometry,IsSpherical,IsCylindrical=', &
         IsRoundCube, IsCartesian, IsRzGeometry, IsSpherical, IsCylindrical

    if (IsLogRadius .or. IsGenRadius) &
         write(*,*) 'ERROR: init_geometry failed for roundcube, ', &
         'IsLogRadius, IsGenRadius =', IsLogRadius, IsGenRadius

    if (any(IsPeriodic_D(1:nDim) .neqv. IsPeriodicTest_D(1:nDim))) &
         write(*,*) 'ERROR: init_geometry failed, ', &
         'for TypeGeometryBatl=', TypeGeometryBatl, &
         'IsPeriodic_D =', IsPeriodic_D(1:nDim), &
         ' should be ', IsPeriodicTest_D(1:nDim)

    if (DoTest) write(*,*) 'Testing roundcube with rRound0=200 rRound1=320'
    rRound0 = 200.0
    rRound1 = 320.0

    if (DoTest) write(*,*) 'Testing xyz_to_coord for roundcube along X axis'

    ! points along main axes with L1 = rRound1 are most distorted
    Good_D = [320., 0., 0.]
    Xyz_D = sqrt(real(nDim))*Good_D

    call xyz_to_coord(Xyz_D, Coord_D)
    if (any(abs(Coord_D - Good_D) > 1e-6)) &
         write(*,*) 'ERROR: xyz_to_coord failed for roundcube, ', &
         'Xyz_D =', Xyz_D, ' Coord_D =', Coord_D, ' should be ', Good_D

    if (DoTest) write(*,*) 'Testing coord_to_xyz for roundcube along X axis'
    Good_D = Xyz_D
    call coord_to_xyz(Coord_D, Xyz_D)
    if (any(abs(Xyz_D - Good_D) > 1e-6)) &
         write(*,*) 'ERROR: coord_to_xyz failed for roundcube, ', &
         'Coord_D =', Coord_D, ' Xyz_D =', Xyz_D, ' should be ', Good_D

    if (DoTest) &
         write(*,*) 'Testing xyz_to_coord for roundcube along diagonal'
    if (nDim == 3) then
       Xyz_D = [300., 300., 300.]
    elseif (nDim == 2) then
       Xyz_D = [300., 300., 0.]
    end if
    Good_D = Xyz_D

    call xyz_to_coord(Xyz_D, Coord_D)
    if (any(abs(Coord_D - Good_D) > 1e-6)) &
         write(*,*) 'ERROR: xyz_to_coord failed for roundcube, ', &
         'Xyz_D =', Xyz_D, ' Coord_D =', Coord_D, ' should be ', Good_D

    if (DoTest) &
         write(*,*) 'Testing coord_to_xyz for roundcube along diagonal'
    Good_D = Xyz_D
    call coord_to_xyz(Coord_D, Xyz_D)
    if (any(abs(Xyz_D - Good_D) > 1e-6)) &
         write(*,*) 'ERROR: coord_to_xyz failed for roundcube, ', &
         'Coord_D =', Coord_D, ' Xyz_D =', Xyz_D, ' should be ', Good_D

    if (DoTest) write(*,*) 'Testing xyz_to_coord for arbitrary point'
    if (nDim == 3) then
       Xyz_D = [397.1825374147, 264.7883582764, 132.394179138]
       Good_D = [300., 200., 100.]
    elseif (nDim == 2) then
       Xyz_D = [344.1742027, 229.4494684, 0.]
       Good_D = [300., 200., 0.]
    end if

    call xyz_to_coord(Xyz_D, Coord_D)
    if (any(abs(Coord_D - Good_D) > 1e-6)) &
         write(*,*) 'ERROR: xyz_to_coord failed for roundcube, ', &
         'Xyz_D =', Xyz_D, ' Coord_D =', Coord_D, ' should be ', Good_D

    if (DoTest) write(*,*) 'Testing coord_to_xyz for arbitrary point'
    Good_D = Xyz_D
    call coord_to_xyz(Coord_D, Xyz_D)
    if (any(abs(Xyz_D - Good_D) > 1e-6)) &
         write(*,*) 'ERROR: coord_to_xyz failed for roundcube, ', &
         'Coord_D =', Coord_D, ' Xyz_D =', Xyz_D, ' should be ', Good_D

    if (DoTest) &
         write(*,*) 'Testing xyz_to_coord for roundcube inside rRound0'
    Xyz_D = [100., 90., 0.]    ! Inside rRound0, points are not distorted
    Good_D = Xyz_D
    call xyz_to_coord(Xyz_D, Coord_D)
    if (any(abs(Coord_D - Good_D) > 1e-6)) &
         write(*,*) 'ERROR: xyz_to_coord failed for roundcube, ', &
         'Xyz_D =', Xyz_D, ' Coord_D =', Coord_D, ' should be ', Good_D

    if (DoTest) &
         write(*,*) 'Testing coord_to_xyz for roundcube inside rRound0'
    Good_D = Xyz_D
    call coord_to_xyz(Coord_D, Xyz_D)
    if (any(abs(Xyz_D - Good_D) > 1e-6)) &
         write(*,*) 'ERROR: coord_to_xyz failed for roundcube, ', &
         'Coord_D =', Coord_D, ' Xyz_D =', Xyz_D, ' should be ', Good_D

    if (DoTest) &
         write(*,*) 'Testing roundcube with rRound0=1, rRound1=0.6'
    rRound0 = 1.0
    rRound1 = 0.6

    if (DoTest) write(*,*) 'Testing xyz_to_coord for roundcube'
    if (nDim == 2) then
       Xyz_D = [0.0964809, 0.1929618, 0.]
       Good_D = [0.1, 0.2, 0.]
    else if (nDim == 3) then
       Xyz_D = [0.09008918, 0.18017837, 0.2702675]
       Good_D = [0.1, 0.2, 0.3]
    end if

    call xyz_to_coord(Xyz_D, Coord_D)
    if (any(abs(Coord_D - Good_D) > 1e-6)) &
         write(*,*) 'ERROR: xyz_to_coord failed for roundcube, ', &
         'Xyz_D =', Xyz_D, ' Coord_D =', Coord_D, ' should be ', Good_D

    if (DoTest) write(*,*) 'Testing coord_to_xyz for roundcube'
    Good_D = Xyz_D
    call coord_to_xyz(Coord_D, Xyz_D)
    if (any(abs(Xyz_D - Good_D) > 1e-6)) &
         write(*,*) 'ERROR: coord_to_xyz failed for roundcube, ', &
         'Coord_D =', Coord_D, ' Xyz_D =', Xyz_D, ' should be ', Good_D

    if (DoTest) write(*,*) 'Testing xyz_to_coord for roundcube'
    if (nDim == 2) then
       Xyz_D = [0.5736097, 0.4916654, 0.]
       Good_D = [0.7, 0.6, 0.]
    else if (nDim == 3) then
       Xyz_D = [0.52539750154, 0.450340715612, 0.37528392967]
       Good_D = [0.7, 0.6, 0.5]
    endif

    call xyz_to_coord(Xyz_D, Coord_D)
    if (any(abs(Coord_D - Good_D) > 1e-6)) &
         write(*,*) 'ERROR: xyz_to_coord failed for roundcube, ', &
         'Xyz_D =', Xyz_D, ' Coord_D =', Coord_D, ' should be ', Good_D

    if (DoTest) write(*,*) 'Testing coord_to_xyz for roundcube'
    Good_D = Xyz_D
    call coord_to_xyz(Coord_D, Xyz_D)
    if (any(abs(Xyz_D - Good_D) > 1e-6)) &
         write(*,*) 'ERROR: coord_to_xyz failed for roundcube, ', &
         'Coord_D =', Coord_D, ' Xyz_D =', Xyz_D, ' should be ', Good_D

    if (DoTest) write(*,*) 'Testing xyz_to_coord for roundcube along X axis'
    if (nDim == 2) then
       Xyz_D = [0.7, 0., 0.]
    else if (nDim == 3) then
       Xyz_D = [0.3, 0., 0.]
    endif
    Good_D = Xyz_D

    call xyz_to_coord(Xyz_D, Coord_D)
    if (any(abs(Coord_D - Good_D) > 1e-6)) &
         write(*,*) 'ERROR: xyz_to_coord failed for roundcube, ', &
         'Xyz_D =', Xyz_D, ' Coord_D =', Coord_D, ' should be ', Good_D

    if (DoTest) write(*,*) 'Testing coord_to_xyz for roundcube along X axis'
    Good_D = Xyz_D
    call coord_to_xyz(Coord_D, Xyz_D)
    if (any(abs(Xyz_D - Good_D) > 1e-6)) &
         write(*,*) 'ERROR: coord_to_xyz failed for roundcube, ', &
         'Coord_D =', Coord_D, ' Xyz_D =', Xyz_D, ' should be ', Good_D

    if (nDim < 3) RETURN

    if (DoTest) write(*,*) 'Testing init_geometry for spherical_genr'
    IsPeriodicTest_D = [.false., .true., .false.]

    call init_geometry('spherical_genr', IsPeriodicIn_D=IsPeriodicTest_D, &
         RgenIn_I=Rgen_I)

    if (TypeGeometryBatl /= 'spherical_genr') &
         write(*,*) 'ERROR: init_geometry failed, ', &
         'TypeGeometryBatl=', TypeGeometryBatl, ' should be spherical_genr'

    if (.not. IsSpherical .or. IsCartesian .or. IsRzGeometry .or. &
         IsCylindrical) &
         write(*,*) 'ERROR: init_geometry failed for spherical_genr, ', &
         'IsCartesian, IsRzGeometry, IsCylindrical, IsSpherical=', &
         IsCartesian, IsRzGeometry, IsCylindrical, IsSpherical

    if (.not. IsGenRadius .or. IsLogRadius) &
         write(*,*) 'ERROR: init_geometry failed for spherical_genr, ', &
         'IsLogRadius, IsGenRadius =', IsLogRadius, IsGenRadius

    if (any(IsPeriodic_D(1:nDim) .neqv. IsPeriodicTest_D(1:nDim))) &
         write(*,*) 'ERROR: init_geometry failed, ', &
         'for TypeGeometryBatl=', TypeGeometryBatl, &
         'IsPeriodic_D =', IsPeriodic_D(1:nDim), &
         ' should be ', IsPeriodicTest_D(1:nDim)

    if (nRgen /= size(Rgen_I)) &
         write(*,*) 'ERROR: init_geometry failed, ', &
         'for TypeGeometryBatl=', TypeGeometryBatl, &
         'nRgen=', nRgen, ' should be ', size(Rgen_I)

    if (.not. allocated(LogRgen_I)) &
         write(*,*) 'ERROR: init_geometry failed, ', &
         'for TypeGeometryBatl=', TypeGeometryBatl, &
         'LogRgen_I is not allocated'

    if (any(abs(exp(LogRgen_I) - Rgen_I) > 1e-6)) &
         write(*,*) 'ERROR: init_geometry failed, ', &
         'for TypeGeometryBatl=', TypeGeometryBatl, &
         'exp(LogRgen_I) =', exp(LogRgen_I), ' should be ', Rgen_I

    if (DoTest) write(*,*) 'Testing radius_to_gen and gen_to_radius'
    r = sqrt(Rgen_I(2)*Rgen_I(3))
    GenR = r
    call radius_to_gen(GenR)
    if (abs(GenR - 1.5/4) > 1e-6) &
         write(*,*) 'ERROR: radius_to_gen failed for spherical_genr, ', &
         'r=', r, ' GenR =', GenR, ' should be ', 1.5/4

    ! Test conversion back
    call gen_to_radius(GenR)
    if (abs(GenR - r) > 1e-6) &
         write(*,*) 'ERROR: gen_to_radius failed for spherical_genr, ', &
         'Orig r=', r, ' new r =', GenR

    r = 1.0/Rgen_I(2)**2
    GenR = r
    call radius_to_gen(GenR)
    if (abs(GenR + 2.0/4) > 1e-6) &
         write(*,*) 'ERROR: radius_to_gen failed for spherical_genr, ', &
         'r=', r, ' GenR =', GenR, ' should be ', -2.0/4

    ! Test conversion back
    call gen_to_radius(GenR)
    if (abs(GenR - r) > 1e-6) &
         write(*,*) 'ERROR: gen_to_radius failed for spherical_genr, ', &
         'Orig r=', r, ' new r =', GenR

    r = 1600.0
    GenR = r
    call radius_to_gen(GenR)
    if (abs(GenR - (1 + 2./4)) > 1e-6) &
         write(*,*) 'ERROR: radius_to_gen failed for spherical_genr, ', &
         'r=', r, ' GenR =', GenR, ' should be ', 1 + 2./4.

    ! Test conversion back
    call gen_to_radius(GenR)
    if (abs(GenR - r) > 1e-6) &
         write(*,*) 'ERROR: gen_to_radius failed for spherical_genr, ', &
         'Orig r=', r, ' new r =', GenR

    if (DoTest) write(*,*) 'Testing xyz_to_coord for spherical_genr'
    Xyz_D = [9., 12., 20.]
    Good_D = [0.75, atan2(15., 20.), atan2(12., 9.)]
    call xyz_to_coord(Xyz_D, Coord_D)
    if (any(abs(Coord_D - Good_D) > 1e-6)) &
         write(*,*) 'ERROR: xyz_to_coord failed for spherical_genr, ', &
         'Xyz_D =', Xyz_D, ' Coord_D =', Coord_D, ' should be ', Good_D

    if (DoTest) write(*,*) 'Testing coord_to_xyz for spherical_genr'
    Good_D = Xyz_D
    call coord_to_xyz(Coord_D, Xyz_D)
    if (any(abs(Xyz_D - Good_D) > 1e-6)) &
         write(*,*) 'ERROR: coord_to_xyz failed for spherical, ', &
         'Coord_D =', Coord_D, ' Xyz_D =', Xyz_D, ' should be ', Good_D

    if (DoTest) write(*,*) 'Testing cubedsphere'
    IsPeriodicTest_D = [.false., .false., .false.]
    call init_geometry('cubedsphere', IsPeriodicIn_D=IsPeriodicTest_D)
    if(.not. IsCubedSphere .or. IsCartesian .or. IsSpherical) &
         write(*,*) 'ERROR: init_geometry failed for cubedsphere, ', &
         'IsCubedSphere, IsCartesian, IsSpherical=', &
         IsCubedSphere, IsCartesian, IsSpherical

    if (any(IsPeriodic_D .neqv. IsPeriodicTest_D)) &
         write(*,*) 'ERROR: init_geometry failed, ', &
         'for TypeGeometryBatl=', TypeGeometryBatl, &
         'IsPeriodic_D =', IsPeriodic_D, &
         ' should be ', IsPeriodicTest_D

    if (DoTest) write(*,*) 'Testing xyz_to_coord for cubedsphere'
    Xyz_D = [8., 3., 2.]
    Good_D = [norm2(Xyz_D), atan2(Xyz_D(2), Xyz_D(1)), &
         atan2(Xyz_D(3), Xyz_D(1))]
    call xyz_to_coord(Xyz_D, Coord_D)
    if (any(abs(Coord_D - Good_D) > 1e-6)) &
         write(*,*) 'ERROR: xyz_to_coord failed for cubedsphere', &
         'Xyz_D =', Xyz_D, ' Coord_D =', Coord_D, ' should be ', Good_D

    if (DoTest) write(*,*) 'Testing coord_to_xyz for cubedsphere'
    Good_D = Xyz_D
    call coord_to_xyz(Coord_D, Xyz_D)
    if (any(abs(Xyz_D - Good_D) > 1e-6)) &
         write(*,*) 'ERROR: coord_to_xyz failed for cubedsphere, ', &
         'Coord_D =', Coord_D, ' Xyz_D =', Xyz_D, ' should be ', Good_D

  end subroutine test_geometry
  !============================================================================
  subroutine test_grid

    use ModNumConst, ONLY: i_DD, cPi

    integer :: iBlock

    integer, parameter:: MaxBlockTest = 50
    integer, parameter:: nRootTest_D(MaxDim) = [3, 3, 3]
    logical, parameter:: IsPeriodicTest_D(MaxDim) = [.true., .true., .false.]
    real:: DomainMin_D(MaxDim) = [3.0, 2.0, 1.0]
    real:: DomainMax_D(MaxDim) = [9.0, 6.0, 4.0]

    ! number of points in each dimension to test interpolation
    integer, parameter:: &
         nPointI = 91, &
         nPointJ = 1 + 90*min(1, nDim - 1), &
         nPointK = 1 + 90*max(0, nDim - 2)

    integer, parameter:: nPoint_D(MaxDim) = [nPointI, nPointJ, nPointK]
    real, allocatable:: Point_VIII(:,:,:,:)
    integer, parameter:: nVarPoint = nDim
    real:: XyzPoint_D(MaxDim), Point_V(nVarPoint), Weight
    integer:: iPoint, jPoint, kPoint, iPoint_D(MaxDim), iCell, nCell, iError
    integer:: iCell_II(0:nDim, 2**nDim)
    logical:: IsSecondOrder
    integer:: iDiscr_D(MaxDim)
    real   :: Weight_I(2**nDim)

    real:: Tolerance

    integer:: i, j, k, Di, Dj, Dk, iDim, iBlockOut, iProcOut, iCell_D(MaxDim)
    integer:: iNodeCenter
    real:: Radius, Phi, Xyz_D(MaxDim), Coord_D(MaxDim), Distance_D(MaxDim)
    real:: Good, Good_D(MaxDim)
    real, allocatable:: CellVolumeCart_B(:), CellFaceCart_DB(:, :)
    real:: Volume, VolumeExact

    ! testing check_interpolate_amr_gc
    integer, parameter:: nPointCheck = 10000
    integer, parameter:: nRootCheck = 2**nDim
    integer, parameter:: nBlockCheckMax = nRootCheck**2
    integer, parameter:: nCaseCheck = 2**nRootCheck
    integer:: iCase, iNodeCheck, iBlockCheck, iProcCheck, iSeed = 1

    logical:: DoTest
    character(len=*), parameter:: NameSub = 'test_grid'
    !--------------------------------------------------------------------------
    DoTest = iProc == 0

    if (DoTest) then
       write(*,*) 'Starting ', NameSub
       write(*,*) 'Testing init_grid'
       write(*,*) 'nDimAmr, nIJK_D=', nDimAmr, nIJK_D
    end if
    ! Set Cartesian grid geometry before initializing tree and grid
    call init_geometry(IsPeriodicIn_D=IsPeriodicTest_D(1:nDim))
    call init_tree(MaxBlockTest)
    call init_grid(DomainMin_D(1:nDim), DomainMax_D(1:nDim))
    call set_tree_root(nRootTest_D(1:nDim))

    call find_tree_node([0.5, 0.5, 0.5], iNodeCenter)
    call refine_tree_node(iNodeCenter)
    call distribute_tree(.true.)
    if (DoTest) call show_tree('After distribute_tree')

    if (DoTest) write(*,*) 'Testing create_grid'
    call create_grid

    if (iProc == 0) call show_grid_proc

    if (DoTest) write(*,*) 'Testing find_grid_block'
    Xyz_D = 0.0
    Xyz_D(1:nDim) = DomainMin_D(1:nDim)
    call find_grid_block(Xyz_D, iProcOut, iBlockOut, &
         iCell_D, Distance_D, UseGhostCell=.true.)
    if (iProc == iProcOut) then
       Xyz_D = Xyz_DGB(:, iCell_D(1), iCell_D(2), iCell_D(3), iBlockOut) &
            + 0.5*CellSize_DB(:, iBlockOut)
       if (any(abs(DomainMin_D(1:nDim) - Xyz_D(1:nDim)) > 1e-6)) then
          write(*,*) 'Error: DomainMin_D, Xyz_D=', &
               DomainMin_D, Xyz_D
          write(*,*) 'iProcOut, iBlockOut, iCell_D = ', &
               iProcOut, iBlockOut, iCell_D
       end if
    end if

    if (any(iCell_D(1:nDim) /= 0)) then
       write(*,*) 'Error: iCell_D=', iCell_D(1:nDim), ' should be 0'
       write(*,*) 'iProcOut, iBlockOut, Distance_D = ', &
            iProcOut, iBlockOut, Distance_D
    end if

    if (any(abs(Distance_D(1:nDim) - 0.5) > 1e-6)) then
       write(*,*) 'Error: Distance_D=', Distance_D(1:nDim), ' should be -0.5'
       write(*,*) 'iProcOut, iBlockOut, iCell_D = ', &
            iProcOut, iBlockOut, iCell_D
    end if

    Xyz_D = 0.0
    Xyz_D(1:nDim) = DomainMax_D(1:nDim)
    call find_grid_block(Xyz_D, iProcOut, iBlockOut, &
         iCell_D, Distance_D, UseGhostCell=.true.)
    if (iProc == iProcOut) then
       Xyz_D = Xyz_DGB(:, iCell_D(1), iCell_D(2), iCell_D(3), iBlockOut) &
            + 0.5*CellSize_DB(:, iBlockOut)
       if (any(abs(DomainMax_D(1:nDim) - Xyz_D(1:nDim)) > 1e-6)) then
          write(*,*) 'Error: DomainMax_D, Xyz_D=', &
               DomainMax_D, Xyz_D
          write(*,*) 'iProcOut, iBlockOut, iCell_D = ', &
               iProcOut, iBlockOut, iCell_D
       end if
    end if

    if (any(iCell_D(1:nDim) /= nIJK_D(1:nDim))) then
       write(*,*) 'Error: iCell_D=', iCell_D(1:nDim), &
            ' should be ', nIJK_D(1:nDim)
       write(*,*) 'iProcOut, iBlockOut, Distance_D = ', &
            iProcOut, iBlockOut, Distance_D
    end if

    if (any(abs(Distance_D(1:nDim) - 0.5) > 1e-6)) then
       write(*,*) 'Error: Distance_D=', Distance_D(1:nDim), ' should be +0.5'
       write(*,*) 'iProcOut, iBlockOut, iCell_D = ', &
            iProcOut, iBlockOut, iCell_D
    end if

    if (DoTest) write(*,*) 'Testing interpolate_grid'

    Xyz_D = 0.0
    if (.not. allocated(Point_VIII)) &
         allocate (Point_VIII(0:nVarPoint, nPointI, nPointJ, nPointK))
    Point_VIII = 0.0
    do kPoint = 1, nPointK; do jPoint = 1, nPointJ; do iPoint = 1, nPointI
       iPoint_D = [iPoint, jPoint, kPoint]
       XyzPoint_D(1:nDim) = CoordMin_D(1:nDim) + (iPoint_D(1:nDim) - 0.5) &
            *DomainSize_D(1:nDim)/nPoint_D(1:nDim)

       call interpolate_grid(XyzPoint_D, nCell, iCell_II, Weight_I)

       do iCell = 1, nCell
          Point_VIII(0, iPoint, jPoint, kPoint) = &
               Point_VIII(0, iPoint, jPoint, kPoint) + Weight_I(iCell)
          iBlock = iCell_II(0, iCell)
          iCell_D = 1
          iCell_D(1:nDim) = iCell_II(1:nDim, iCell)

          ! Interpolate the coordinates to check order of accuracy
          ! Note: Using array syntax in combination with the indirect
          ! iCell_D index fails with optimization for NAG v5.1
          do iDim = 1, nDim
             Xyz_D(iDim) = &
                  Xyz_DGB(iDim, iCell_D(1), iCell_D(2), iCell_D(3), iBlock)
          end do

          ! Take care of periodic dimensions: shift coordinates as necessary
          do iDim = 1, nDim
             if (.not. IsPeriodicTest_D(iDim)) CYCLE
             if (XyzPoint_D(iDim) < Xyz_D(iDim) - 2*CellSize_DB(iDim,iBlock)) &
                  Xyz_D(iDim) = Xyz_D(iDim) - DomainSize_D(iDim)

             if (XyzPoint_D(iDim) > Xyz_D(iDim) + 2*CellSize_DB(iDim,iBlock)) &
                  Xyz_D(iDim) = Xyz_D(iDim) + DomainSize_D(iDim)
          end do

          Point_VIII(1:nDim, iPoint, jPoint, kPoint) = &
               Point_VIII(1:nDim, iPoint, jPoint, kPoint) &
               + Weight_I(iCell)*Xyz_D(1:nDim)

       end do
    end do; end do; end do

    ! Collect contributions from all processors to proc 0
    if (nProc > 1) call MPI_reduce_real_array(Point_VIII, &
         size(Point_VIII), MPI_SUM, 0, iComm, iError)

    if (iProc == 0) then
       ! Check interpolated coordinate values against point coordinates
       do kPoint = 1, nPointK; do jPoint = 1, nPointJ; do iPoint = 1, nPointI
          iPoint_D = [iPoint, jPoint, kPoint]
          Xyz_D(1:nDim) = CoordMin_D(1:nDim) + (iPoint_D(1:nDim) - 0.5) &
               *DomainSize_D(1:nDim)/nPoint_D(1:nDim)

          Weight = Point_VIII(0, iPoint, jPoint, kPoint)
          Point_V = Point_VIII(1:nDim, iPoint, jPoint, kPoint)/Weight

          if (abs(Weight - 1.0) < 1e-6) then
             Tolerance = 1e-6
          else
             Tolerance = 3e-2
          end if

          if (any(abs(Xyz_D(1:nDim) - Point_V) > Tolerance)) then
             write(*,*) 'ERROR: Point_V=', Point_V(1:nDim), &
                  ' should be ', Xyz_D(1:nDim)
             write(*,*) 'Total weight=', Weight
             write(*,*) 'i,j,kPoint=', iPoint_D(1:nDim)
             write(*,*) 'CoordMin,Max=', CoordMin_D(1:nDim), &
                  CoordMax_D(1:nDim)
          end if
       end do; end do; end do
    end if

    if (DoTest) write(*,*) 'Testing interpolate_grid_amr'
    Xyz_D = 0.0
    if (.not. allocated(Point_VIII)) &
         allocate (Point_VIII(0:nVarPoint, nPointI, nPointJ, nPointK))
    Point_VIII = 0.0
    do kPoint = 1, nPointK; do jPoint = 1, nPointJ; do iPoint = 1, nPointI
       iPoint_D = [iPoint, jPoint, kPoint]
       XyzPoint_D(1:nDim) = CoordMin_D(1:nDim) + (iPoint_D(1:nDim) - 0.5) &
            *DomainSize_D(1:nDim)/nPoint_D(1:nDim)

       call interpolate_grid_amr(XyzPoint_D, nCell, iCell_II, Weight_I, &
            IsSecondOrder)

       do iCell = 1, nCell
          Point_VIII(0, iPoint, jPoint, kPoint) = &
               Point_VIII(0, iPoint, jPoint, kPoint) + Weight_I(iCell)
          iBlock = iCell_II(0, iCell)
          iCell_D = 1
          iCell_D(1:nDim) = iCell_II(1:nDim, iCell)

          ! Interpolate the coordinates to check order of accuracy
          ! Note: Using array syntax in combination with the indirect
          ! iCell_D index fails with optimization for NAG v5.1
          do iDim = 1, nDim
             Xyz_D(iDim) = &
                  Xyz_DGB(iDim, iCell_D(1), iCell_D(2), iCell_D(3), iBlock)
          end do

          ! Take care of periodic dimensions: shift coordinates as necessary
          do iDim = 1, nDim
             if (.not. IsPeriodicTest_D(iDim)) CYCLE
             if (XyzPoint_D(iDim) < Xyz_D(iDim) - 2*CellSize_DB(iDim,iBlock)) &
                  Xyz_D(iDim) = Xyz_D(iDim) - DomainSize_D(iDim)

             if (XyzPoint_D(iDim) > Xyz_D(iDim) + 2*CellSize_DB(iDim,iBlock)) &
                  Xyz_D(iDim) = Xyz_D(iDim) + DomainSize_D(iDim)
          end do

          ! if point is close to boundary then interpolation isn't 2nd order
          where (.not. (IsSecondOrder .or. IsPeriodicTest_D(1:nDim))) &
               Xyz_D(1:nDim) = XyzPoint_D(1:nDim)

          Point_VIII(1:nDim, iPoint, jPoint, kPoint) = &
               Point_VIII(1:nDim, iPoint, jPoint, kPoint) &
               + Weight_I(iCell)*Xyz_D(1:nDim)

       end do

    end do; end do; end do

    ! Collect contributions from all processors to proc 0
    if (nProc > 1) call MPI_reduce_real_array(Point_VIII, &
         size(Point_VIII), MPI_SUM, 0, iComm, iError)

    if (iProc == 0) then
       ! Check interpolated coordinate values against point coordinates
       do kPoint = 1, nPointK; do jPoint = 1, nPointJ; do iPoint = 1, nPointI
          iPoint_D = [iPoint, jPoint, kPoint]
          Xyz_D(1:nDim) = CoordMin_D(1:nDim) + (iPoint_D(1:nDim) - 0.5) &
               *DomainSize_D(1:nDim)/nPoint_D(1:nDim)

          Weight = Point_VIII(0, iPoint, jPoint, kPoint)
          Point_V = Point_VIII(1:nDim, iPoint, jPoint, kPoint)/Weight

          if (abs(Weight - 1.0) < 1e-6) then
             Tolerance = 1e-6
          else
             Tolerance = 3e-2
          end if

          if (any(abs(Xyz_D(1:nDim) - Point_V) > Tolerance)) then
             write(*,*) 'ERROR: Point_V=', Point_V(1:nDim), &
                  ' should be ', Xyz_D(1:nDim)
             write(*,*) 'Total weight=', Weight
             write(*,*) 'i,j,kPoint=', iPoint_D(1:nDim)
             write(*,*) 'CoordMin,Max=', CoordMin_D(1:nDim), &
                  CoordMax_D(1:nDim)
          end if
       end do; end do; end do
    end if

    if (nDim == nDimAmr) then
       if (DoTest) write(*,*) 'Testing interpolate_grid_amr_gc'
       Xyz_D = 0.0
       if (.not. allocated(Point_VIII)) &
            allocate (Point_VIII(0:nVarPoint, nPointI, nPointJ, nPointK))
       Point_VIII = 0.0
       do kPoint = 1, nPointK; do jPoint = 1, nPointJ; do iPoint = 1, nPointI
          iPoint_D = [iPoint, jPoint, kPoint]
          XyzPoint_D(1:nDim) = CoordMin_D(1:nDim) + (iPoint_D(1:nDim) - 0.5) &
               *DomainSize_D(1:nDim)/nPoint_D(1:nDim)
          call interpolate_grid_amr_gc(XyzPoint_D, nCell, iCell_II, Weight_I, &
               IsSecondOrder)

          do iCell = 1, nCell
             Point_VIII(0, iPoint, jPoint, kPoint) = &
                  Point_VIII(0, iPoint, jPoint, kPoint) + Weight_I(iCell)
             iBlock = iCell_II(0, iCell)
             iCell_D = 1
             iCell_D(1:nDim) = iCell_II(1:nDim, iCell)

             ! Interpolate the coordinates to check order of accuracy
             ! Note: Using array syntax in combination with the indirect
             ! iCell_D index fails with optimization for NAG v5.1
             do iDim = 1, nDim
                Xyz_D(iDim) = &
                     Xyz_DGB(iDim, iCell_D(1), iCell_D(2), iCell_D(3), iBlock)
             end do

             ! Take care of periodic dimensions: shift coordinates as necessary
             do iDim = 1, nDim
                if (.not. IsPeriodicTest_D(iDim)) CYCLE
                if (XyzPoint_D(iDim) &
                     < Xyz_D(iDim) - 2*CellSize_DB(iDim,iBlock)) &
                     Xyz_D(iDim) = Xyz_D(iDim) - DomainSize_D(iDim)

                if (XyzPoint_D(iDim) &
                     > Xyz_D(iDim) + 2*CellSize_DB(iDim,iBlock)) &
                     Xyz_D(iDim) = Xyz_D(iDim) + DomainSize_D(iDim)
             end do

             ! Take care of ghost cells: shift coordinates for coarser neighbor
             iDiscr_D = 0
             where (iCell_D(1:nDim) < 1)
                iDiscr_D(1:nDim) = -1
             elsewhere(iCell_D(1:nDim) > nIJK_D(1:nDim))
                iDiscr_D(1:nDim) = 1
             end where
             ! check that neighbor is coarser
             if (DiLevelNei_IIIB(iDiscr_D(1), iDiscr_D(2), iDiscr_D(3), &
                  iBlock) == 1) &
                  Xyz_D = Xyz_D + &
                  0.5*(2*modulo(iCell_D, 2) - 1)*CellSize_DB(:, iBlock)

             ! if point is close to boundary then interpolation isn't 2nd order
             where (.not. (IsSecondOrder .or. IsPeriodicTest_D(1:nDim))) &
                  Xyz_D(1:nDim) = XyzPoint_D(1:nDim)

             Point_VIII(1:nDim, iPoint, jPoint, kPoint) = &
                  Point_VIII(1:nDim, iPoint, jPoint, kPoint) &
                  + Weight_I(iCell)*Xyz_D(1:nDim)
          end do
       end do; end do; end do
       ! Collect contributions from all processors to proc 0
       if (nProc > 1) call MPI_reduce_real_array(Point_VIII, &
            size(Point_VIII), MPI_SUM, 0, iComm, iError)

       if (iProc == 0) then
          ! Check interpolated coordinate values against point coordinates
          do kPoint = 1,nPointK; do jPoint = 1,nPointJ; do iPoint = 1,nPointI
             iPoint_D = [iPoint, jPoint, kPoint]
             Xyz_D(1:nDim) = CoordMin_D(1:nDim) + (iPoint_D(1:nDim) - 0.5) &
                  *DomainSize_D(1:nDim)/nPoint_D(1:nDim)

             Weight = Point_VIII(0, iPoint, jPoint, kPoint)
             if (Weight < 1e-6) CYCLE
             Point_V = Point_VIII(1:nDim, iPoint, jPoint, kPoint)/Weight

             if (abs(Weight - 1.0) < 1e-6) then
                Tolerance = 1e-6
             else
                Tolerance = 3e-2
             end if

             if (any(abs(Xyz_D(1:nDim) - Point_V) > Tolerance)) then
                write(*,*) 'ERROR: Point_V=', Point_V(1:nDim), &
                     ' should be ', Xyz_D(1:nDim)
                write(*,*) 'Total weight=', Weight
                write(*,*) 'i,j,kPoint=', iPoint_D(1:nDim)
                write(*,*) 'CoordMin,Max=', CoordMin_D(1:nDim), &
                     CoordMax_D(1:nDim)
             end if
          end do; end do; end do
       end if
    end if

    if (nDim == 2) then
       if (DoTest) write(*,*) 'Testing create_grid in RZ geometry'

       ! Store Cartesian values for checking
       allocate (CellVolumeCart_B(MaxBlock), CellFaceCart_DB(MaxDim, MaxBlock))
       CellFaceCart_DB = CellFace_DB
       CellVolumeCart_B = CellVolume_B

       ! Clean Cartesian grid
       call clean_grid

       ! Initialize RZ grid
       call init_geometry(TypeGeometryIn='rz')
       call init_grid(DomainMin_D(1:nDim), DomainMax_D(1:nDim))
       call create_grid
       if (iProc == 0) call show_grid_proc

       ! Check relative to Cartesian
       Tolerance = 1e-6
       do iBlock = 1, nBlock
          do k = MinK, MaxK; do j = MinJ, MaxJ; do i = MinI, MaxI
             if (abs(CellVolume_GB(i,j,k,iBlock) &
                  - abs(Xyz_DGB(2,i,j,k,iBlock))*CellVolumeCart_B(iBlock)) &
                  < Tolerance) CYCLE
             write(*,*) NameSub, ' ERROR: incorrect cell volume=', &
                  CellVolume_GB(i, j, k,iBlock), ' should be', &
                  abs(Xyz_DGB(2,i,j,k,iBlock))*CellVolumeCart_B(iBlock), &
                  ' at i,j,k,iBlock,iProc=',i,j,k,iBlock, iProc
          end do; end do; end do
          do iDim = 1, nDim
             Di = i_DD(1, iDim); Dj = i_DD(2, iDim)
             do k = 1, nK; do j = 1, nJ + Dj; do i = 1, nI + Di
                Radius = 0.5*sum(abs(Xyz_DGB(2,i - Di:i,j - Dj:j,k,iBlock)))
                if (abs(CellFace_DFB(iDim,i,j,k,iBlock) - &
                     Radius*CellFaceCart_DB(iDim, iBlock)) &
                     < Tolerance) CYCLE
                write(*,*) NameSub, ' ERROR: incorrect face area=', &
                     CellFace_DFB(iDim,i,j,k,iBlock), ' should be', &
                     Radius*CellFaceCart_DB(iDim, iBlock), &
                     ' at iDim,i,j,k,iBlock,iProc=', &
                     iDim,i,j,k,iBlock, iProc

             end do; end do; end do
          end do
       end do
    end if

    if (nDim >= 2) then
       if (DoTest) write(*,*) 'Testing create_grid in cylindrical geometry'

       ! Clean  grid
       call clean_grid

       ! Initialize cylindrical grid
       call init_geometry(TypeGeometryIn='cylindrical')

       DomainMin_D = [1., 0., -0.5]
       DomainMax_D = [3., 90., 0.5]

       ! This is temporary solution to keep the test working
       IsNodeBasedGrid = .false.
       call init_grid(DomainMin_D(1:nDim), DomainMax_D(1:nDim))
       call create_grid

       if (iProc == 0) call show_grid_proc

       ! Check relative to generalized coordinate volumes and areas
       Tolerance = 1e-6
       do iBlock = 1, nBlock
          do k = MinK, MaxK; do j = MinJ, MaxJ; do i = MinI, MaxI
             Good = sqrt(sum(Xyz_DGB(1:2,i,j,k,iBlock)**2)) &
                  *CellVolume_B(iBlock)
             if (abs(CellVolume_GB(i,j,k,iBlock) - Good) < Tolerance) CYCLE
             write(*,*) NameSub, ' ERROR: incorrect cell volume=', &
                  CellVolume_GB(i,j,k,iBlock), ' should be', Good, &
                  ' at i,j,k,iBlock,iProc=',i,j,k,iBlock, iProc
          end do; end do; end do
          do iDim = 1, nDim
             Di = i_DD(1, iDim); Dj = i_DD(2, iDim); Dk = i_DD(3, iDim)
             do k = 1, nK + Dk; do j = 1, nJ + Dj; do i = 1, nI + Di
                ! Calculate face center in generalized coordinates
                Coord_D = CoordMin_DB(:, iBlock) + CellSize_DB(:, iBlock) &
                     *[i - 0.5*(1 + Di), j - 0.5*(1 + Dj), k - 0.5*(1 + Dk)]

                Good = CellFace_DB(iDim, iBlock)
                if (iDim /= 2) Good = Good*Coord_D(1)
                if (abs(CellFace_DFB(iDim,i,j,k,iBlock) - Good) > Tolerance) &
                     write(*,*) NameSub, ' ERROR: incorrect face area=', &
                     CellFace_DFB(iDim,i,j,k,iBlock), ' should be', Good, &
                     ' at iDim,i,j,k,iBlock,iProc=', &
                     iDim,i,j,k,iBlock, iProc

                Phi = Coord_D(2)
                if (iDim == 1) then
                   Good_D = [cos(Phi), sin(Phi), 0.0]
                elseif (iDim == 2) then
                   Good_D = [-sin(Phi), cos(Phi), 0.0]
                else
                   Good_D = [0.0, 0.0, 1.0]
                end if
                ! Multiply by area (for now)
                Good_D = Good_D*CellFace_DFB(iDim,i,j,k,iBlock)

                if (any(Tolerance < abs(FaceNormal_DDFB(:, iDim,i,j,k,iBlock) &
                     - Good_D(1:nDim)))) &
                     write(*,*) NameSub, ' ERROR: incorrect face area=', &
                     FaceNormal_DDFB(:,iDim,i,j,k,iBlock), &
                     ' should be', Good_D, &
                     ' at iDim,i,j,k,iBlock,iProc=', &
                     iDim, i, j, k, iBlock, iProc

             end do; end do; end do
          end do
       end do
    end if

    if (nDim == 3) then
       if (DoTest) write(*,*) 'Testing create_grid in spherical geometry'

       ! Clean  grid
       call clean_grid

       ! Initialize spherical grid
       call init_geometry(TypeGeometryIn='spherical')

       DomainMin_D = [1., 45.0, 0.0]
       DomainMax_D = [3., 135.0, 90.0]

       ! This is temporary solution to keep the test working
       IsNodeBasedGrid = .false.
       call init_grid(DomainMin_D, DomainMax_D)
       call create_grid

       if (iProc == 0) call show_grid_proc

       if (DoTest) write(*,*) 'Testing create_grid in rlonlat geometry'

       ! Clean grid
       call clean_grid

       ! Initialize r-lon-lat grid
       call init_geometry(TypeGeometryIn='rlonlat')

       DomainMin_D = [1., 0.0, -45.0]
       DomainMax_D = [3., 90.0, 45.0]

       ! This is temporary solution to keep the test working
       IsNodeBasedGrid = .false.
       call init_grid(DomainMin_D, DomainMax_D)
       call create_grid

       if (iProc == 0) call show_grid_proc

       if (DoTest) write(*,*) 'Testing create_grid in roundcube geometry'

       ! Clean  grid
       call clean_grid

       ! Initialize roundcube grid
       call init_geometry(TypeGeometryIn='roundcube')

       rRound0 = 2.0
       rRound1 = 3.2

       DomainMin_D = [-3.2, -3.2, -3.2]
       DomainMax_D = [3.2, 3.2, 3.2]

       IsNodeBasedGrid = .true.
       call init_grid(DomainMin_D, DomainMax_D)
       call create_grid

       if (iProc == 0) call show_grid_proc

       ! Check total volume.
       ! It should be approximately the volume of a sphere of radius 3.2
       Volume = sum(CellVolume_GB(1:nI, 1:nJ, 1:nK, 1:nBlock))
       if (nProc > 1) &
            call MPI_reduce_real_scalar(Volume, MPI_SUM, 0, iComm, iError)

       if (iProc == 0) then
          ! Analytic volume of the sphere
          VolumeExact = 4./3.*cPi*(sqrt(3.)*rRound1)**3

          if (abs(VolumeExact - Volume)/VolumeExact > 0.02) &
               write(*,*) 'ERROR: total volume numerical vs analytic:', &
               Volume, VolumeExact
       end if

       if (DoTest) write(*,*) 'Testing create_grid in cubedsphere geometry'

       ! Clean  grid
       call clean_grid

       ! Initialize cubedsphere grid
       call init_geometry(TypeGeometryIn='cubedsphere')

       ! Angle limits should not exceed 45 degrees
       DomainMin_D = [ 5.0, -45.0, -45.0]
       DomainMax_D = [20.0,  45.0,  45.0]

       IsNodeBasedGrid = .true.
       call init_grid(DomainMin_D, DomainMax_D)
       call create_grid

       if (iProc == 0) call show_grid_proc

       ! Check total volume.
       Volume = sum(CellVolume_GB(1:nI,1:nJ,1:nK,1:nBlock))
       if (nProc > 1) &
            call MPI_reduce_real_scalar(Volume, MPI_SUM, 0, iComm, iError)

       if (iProc == 0) then
          ! Analytic volume of the wedge is 1/6 of spherical shell
          VolumeExact = (4*cPi/18)*(DomainMax_D(1)**3 - DomainMin_D(1)**3)
          if (abs(VolumeExact - Volume)/VolumeExact > 1e-6) &
               write(*,*) 'ERROR: total volume numerical vs analytic:', &
               Volume, VolumeExact
       end if

    end if

    if (DoTest) write(*,*) 'Testing clean_grid'
    call clean_grid
    call clean_tree

    if (nDim == nDimAmr .and. nDim > 1) then
       if (DoTest) write(*,*) 'Testing check_interpolate_amr_gc'

       DomainMin_D = [0.0, 0.0, 0.0]
       DomainMax_D = [1.0, 1.0, 1.0]
       Coord_D = [0.0, 0.0, 0.0]

       ! go over all geometry cases
       do iCase = 1, nCaseCheck
          if (DoTest) &
               write (*, '(a,i3)') '  Testing case ', iCase
          ! Set Cartesian grid geometry before initializing tree and grid
          call init_geometry(IsPeriodicIn_D=spread([.false.], 1, nDim))
          call init_tree(nBlockCheckMax)
          call init_grid(DomainMin_D(1:nDim), DomainMax_D(1:nDim))
          call set_tree_root(spread([2], 1, nDim))

          do iNodeCheck = 1, nRootCheck
             if (BTEST(iCase - 1, iNodeCheck - 1)) &
                  call refine_tree_node(iNodeCheck)
          end do

          call distribute_tree(.true.)

          call create_grid

          ! generate random points and test check_interpolate_amr_gc
          do iPoint = 1, nPointCheck
             do iDim = 1, nDim
                Coord_D(iDim) = &
                     0.75*DomainMin_D(iDim) + 0.25*DomainMin_D(iDim) + &
                     0.5*(DomainMax_D(iDim) - DomainMin_D(iDim))* &
                     random_real(iSeed)
             end do
             call check_interpolate_amr_gc(Coord_D, 1, iProcCheck, iBlockCheck)
             do iBlock = 2, nBlock
                call check_interpolate_amr_gc( &
                     Coord_D, iBlock, iProcOut, iBlockOut)
                if (iProcOut /= iProcCheck .or. iBlockOut /= iBlockCheck) then
                   write (*, '(a,3f11.8,a,i3,a,2i3,a,i3,a,2i3)') &
                        'ERROR: for point ', Coord_D, &
                        ' result from iBlock =', 1, &
                        ' is iProcOut, iBlockOut =', iProcCheck, iBlockCheck, &
                        ' but from iBlock =', iBlock, &
                        ' is iProcOut, iBlockOut =', iProcOut, iBlockOut
                end if
             end do
          end do

          call clean_grid
          call clean_tree
       end do
    end if

  end subroutine test_grid
  !============================================================================
  subroutine test_pass_node ! unit test

    ! To test the message pass for the node we generate a fine uniform grid
    ! for the whole domain and transfer the node values from the
    ! block nodes to the nodes on the fine grid. Then we gather all the
    ! data on the fine grid with the proper operator.
    ! We can then compare the values on the coresponding node after
    ! message_pass_node is called with the fine grid values.

    integer, parameter:: MaxBlockTest = 50
    integer, parameter:: nRootTest_D(MaxDim) = [3, 3, 3]
    logical, parameter:: IsPeriodicTest_D(MaxDim) = .false.
    real, parameter:: DomainMin_D(MaxDim) = [0.0, 0.0, 0.0]
    real, parameter:: DomainMax_D(MaxDim) = [48.0, 48.0, 48.0]

    integer, parameter:: nVar = 3! nDim
    integer :: iVar

    character(len=4) :: NameOperator = "Min"

    integer ::iOp
    integer, parameter :: nOp = 3
    character(len=4) :: NameOperator_I(nOp) = ["mean", "min ", "max "]

    ! Variable on the nodes
    real, allocatable:: State_VNB(:, :, :, :, :)

    integer, allocatable:: i_NB(:, :, :, :)

    real, allocatable, dimension(:, :, :, :, :) :: Xyz_DNB

    real, allocatable, dimension(:, :, :, :) :: FineGridLocal_IIIV
    real, allocatable, dimension(:, :, :, :) :: FineGridGlobal_IIIV
    real :: FineGridStep_D(MaxDim)
    integer :: iFG, jFG, kFG
    integer :: nFineCell
    integer :: iMpiOperator
    integer :: iError

    integer:: iNode, iBlock, i, j, k

    logical:: DoTest
    character(len=*), parameter:: NameSub = 'test_pass_node'
    !--------------------------------------------------------------------------
    DoTest = iProc == 0

    if (DoTest) write(*,*) 'Starting ', NameSub

    call init_tree(MaxBlockTest)
    call init_geometry(IsPeriodicIn_D=IsPeriodicTest_D(1:nDim))
    call init_grid(DomainMin_D(1:nDim), DomainMax_D(1:nDim))
    call set_tree_root(nRootTest_D(1:nDim))

    call find_tree_node([0.5, 0.5, 0.5], iNode)
    call refine_tree_node(iNode)
    call distribute_tree(.true.)
    call create_grid

    allocate (Xyz_DNB(MaxDim, nINode, nJNode, nKNode, MaxBlockTest))
    allocate (State_VNB(nVar, nI + 1, nJ + 1, nK + 1, MaxBlockTest))
    allocate (i_NB(nI + 1, nJ + 1, nK + 1, MaxBlockTest))

    allocate (FineGridLocal_IIIV( &
         nI*iRatio*nRootTest_D(1) + 1, &
         nJ*jRatio*nRootTest_D(2) + 1, &
         nK*kRatio*nRootTest_D(3) + 1, &
         nVar + 1))
    allocate (FineGridGlobal_IIIV( &
         nI*iRatio*nRootTest_D(1) + 1, &
         nJ*jRatio*nRootTest_D(2) + 1, &
         nK*kRatio*nRootTest_D(3) + 1, &
         nVar + 1))

    nFineCell = product(nIJK_D*iRatio_D*nRootTest_D + 1)

    FineGridStep_D = (nIJK_D*iRatio_D*nRootTest_D) &
         /(DomainMax_D - DomainMin_D)

    do iBlock = 1, nBlock
       if (Unused_B(iBlock)) CYCLE
       do k = 1, nKNode; do j = 1, nJNode; do i = 1, nINode
          Xyz_DNB(:,i,j,k,iBlock) = CoordMin_DB(:, iBlock) + &
               ([i, j, k] - 1.0)*CellSize_DB(:, iBlock)
       end do; end do; end do
    end do

    do iBlock = 1, nBlock
       if (Unused_B(iBlock)) CYCLE
       iNode = (iNode_B(iBlock) - 1)*(nI + 1)*(nK + 1)*(nJ + 1)
       do k = 1, nK + 1; do j = 1, nJ + 1; do i = 1, nI + 1
          iNode = iNode + 1
          i_NB(i,j,k,iBlock) = iNode
       end do; end do; end do
    end do

    do iOp = 1, nOp

       NameOperator = NameOperator_I(iOp)

       if (DoTest) write(*,*) 'testing message_pass_node with operator=', &
            NameOperator

       select case (NameOperator)
       case ("mean")
          FineGridLocal_IIIV(:, :, :, :) = 0.0
          FineGridGlobal_IIIV(:, :, :, :) = 0.0
          iMpiOperator = MPI_SUM
       case ("min")
          FineGridLocal_IIIV(:, :, :, :) = 1.0e8
          FineGridGlobal_IIIV(:, :, :, :) = 1.0e8
          iMpiOperator = MPI_MIN
       case ("max")
          FineGridLocal_IIIV(:, :, :, :) = -1.0e8
          FineGridGlobal_IIIV(:, :, :, :) = -1.0e8
          iMpiOperator = MPI_MAX
       case default
          call CON_stop(NameSub//' incorrect operator name='//NameOperator)
       end select

       State_VNB(1, :, :, :, :) = i_NB(:, :, :, :)
       do iBlock = 1, nBlock
          if (Unused_B(iBlock)) CYCLE
          do k = 1, nKNode; do j = 1, nJNode; do i = 1, nINode
             iNode = iNode_B(iBlock) - 1
             State_VNB(2:3,i,j,k,iBlock) = [1.0, real(iNode)]
          end do; end do; end do
       end do

       do iBlock = 1, nBlock
          if (Unused_B(iBlock)) CYCLE
          do k = 1, nKNode; do j = 1, nJNode; do i = 1, nINode

             iFG = int(Xyz_DNB(1,i,j,k,iBlock)*FineGridStep_D(1)) + 1
             jFG = int(Xyz_DNB(2,i,j,k,iBlock)*FineGridStep_D(2)) + 1
             kFG = int(Xyz_DNB(3,i,j,k,iBlock)*FineGridStep_D(3)) + 1

             select case (NameOperator)
             case ("mean")
                FineGridLocal_IIIV(iFG, jFG, kFG, 1:nVar) = &
                     FineGridLocal_IIIV(iFG, jFG, kFG, 1:nVar) + &
                     State_VNB(:,i,j,k,iBlock)
                FineGridLocal_IIIV(iFG, jFG, kFG, nVar + 1) = &
                     FineGridLocal_IIIV(iFG, jFG, kFG, nVar + 1) + 1
             case ("min")
                do iVar = 1, nVar
                   FineGridLocal_IIIV(iFG, jFG, kFG, iVar) = min( &
                        FineGridLocal_IIIV(iFG, jFG, kFG, iVar), &
                        State_VNB(iVar,i,j,k,iBlock))
                end do
             case ("max")
                do iVar = 1, nVar
                   FineGridLocal_IIIV(iFG, jFG, kFG, iVar) = max( &
                        FineGridLocal_IIIV(iFG, jFG, kFG, iVar), &
                        State_VNB(iVar,i,j,k,iBlock))
                end do
             end select
          end do; end do; end do
       end do

       call message_pass_node(nVar, State_VNB, NameOperator_I(iOp))

       call MPI_ALLREDUCE(FineGridLocal_IIIV(1, 1, 1, 1), &
            FineGridGlobal_IIIV(1, 1, 1, 1), &
            nFineCell*(nVar + 1), &
            MPI_REAL, iMpiOperator, iComm, iError)

       do iBlock = 1, nBlock
          if (Unused_B(iBlock)) CYCLE
          do k = 1, nKNode; do j = 1, nJNode; do i = 1, nINode
             iFG = nint(Xyz_DNB(1,i,j,k,iBlock)*FineGridStep_D(1)) + 1
             jFG = nint(Xyz_DNB(2,i,j,k,iBlock)*FineGridStep_D(2)) + 1
             kFG = nint(Xyz_DNB(3,i,j,k,iBlock)*FineGridStep_D(3)) + 1
             do iVar = 1, nVar
                select case (NameOperator)
                case ("mean")
                   if (abs(FineGridGlobal_IIIV(iFG, jFG, kFG, iVar)/ &
                        FineGridGlobal_IIIV(iFG, jFG, kFG, nVar + 1) - &
                        State_VNB(iVar,i,j,k,iBlock)) > 1e-10) then
                      write(*,*) "Error for operator, variable, iBlock= ", &
                           NameOperator, iVar, iBlock, ", value=", &
                           FineGridGlobal_IIIV(iFG, jFG, kFG, iVar)/ &
                           FineGridGlobal_IIIV(iFG, jFG, kFG, nVar + 1), &
                           " should be ", State_VNB(iVar,i,j,k,iBlock)
                   end if
                case ("min", "max")
                   if (FineGridGlobal_IIIV(iFG, jFG, kFG, iVar) /= &
                        State_VNB(iVar,i,j,k,iBlock)) then
                      write(*,*) "Error for operator, variable, iBlock= ", &
                           NameOperator, iVar, iBlock, ", value=", &
                           FineGridGlobal_IIIV(iFG, jFG, kFG, iVar), &
                           " should be ", State_VNB(iVar,i,j,k,iBlock)
                   end if
                end select
             end do
          end do; end do; end do
       end do

    end do

    deallocate (FineGridLocal_IIIV)
    deallocate (FineGridGlobal_IIIV)
    deallocate (Xyz_DNB)
    call clean_grid
    call clean_tree

  end subroutine test_pass_node
  !============================================================================
  subroutine test_amr_criteria ! unit test

    !--------------------------------------------------------------------------
  end subroutine test_amr_criteria
  !============================================================================
  subroutine test_pass_cell

    integer, parameter:: MaxBlockTest = 200
    logical:: IsPeriodicTest_D(MaxDim) = .true.
    integer:: nRootTest_D(MaxDim) = [3, 3, 3]
    real   :: DomainMin_D(MaxDim) = [1.0, 10.0, 100.0]
    real   :: DomainMax_D(MaxDim) = [4.0, 40.0, 400.0]

    real   :: Tolerance = 1e-6

    integer, parameter:: nVar = nDim
    real, allocatable:: State_VGB(:, :, :, :, :)
    real, allocatable:: Scalar_GB(:, :, :, :)
    real, allocatable:: FineGridLocal_III(:, :, :)
    real, allocatable:: FineGridGlobal_III(:, :, :)
    real, allocatable:: XyzCorn_DGB(:, :, :, :, :)
    real :: CourseGridCell_III(iRatio, jRatio, kRatio)

    integer:: nWidth
    integer:: nProlongOrder
    integer:: nCoarseLayer
    integer:: iSendCorner, iRestrictFace
    logical:: DoSendCorner, DoRestrictFace

    real:: Xyz_D(MaxDim)
    integer:: iNode, iBlock, i, j, k, iMin, iMax, jMin, jMax, kMin, kMax, iDim
    integer:: iDir, jDir, kDir, Di, Dj, Dk

    integer:: iOp
    integer, parameter:: nOp = 2
    character(len=4):: NameOperator_I(nOp) = ["min", "max"]
    character(len=4):: NameOperator = "Min"
    real :: FineGridStep_D(MaxDim)
    integer:: iFG, jFG, kFG
    integer:: nFineCell
    integer:: iMpiOperator
    integer:: iError, iTest

    character(len=20):: NameGeometry

    logical:: DoTest
    character(len=*), parameter:: NameSub = 'test_pass_cell'
    !--------------------------------------------------------------------------
    DoTest = iProc == 0

    if (DoTest) write(*,*) 'Starting ', NameSub

    call test_switches
    call test_scalar

    if (nDim == 1) RETURN !------------------------
    call test_non_cartesian

    if (nG < 3) RETURN
    if (nDim > nDimAmr) RETURN
    call test_high_order_cartesian
    call test_high_order_non_cartesian

  contains
    !==========================================================================
    subroutine test_switches
      !------------------------------------------------------------------------
      call init_tree(MaxBlockTest)
      call init_geometry(IsPeriodicIn_D=IsPeriodicTest_D(1:nDim))
      call init_grid(DomainMin_D(1:nDim), DomainMax_D(1:nDim))
      call set_tree_root(nRootTest_D(1:nDim))

      call find_tree_node([0.5, 0.5, 0.5], iNode)
      if (DoTest) write(*,*) NameSub, ' middle node=', iNode
      call refine_tree_node(iNode)
      call distribute_tree(.true.)
      call create_grid

      if (DoTest) call show_tree(NameSub, .true.)

      allocate (State_VGB(nVar, MinI:MaxI, MinJ:MaxJ, MinK:MaxK, MaxBlockTest))

      do nProlongOrder = 1, 2; do nCoarseLayer = 1, 2; do nWidth = 1, nG

         ! Second order prolongation does not work with sending multiple
         ! coarse cell layers into the fine cells with their original values.
         if (nProlongOrder == 2 .and. nCoarseLayer == 2) CYCLE

         ! Cannot send more coarse layers than the number of ghost cell layers
         if (nCoarseLayer > nWidth) CYCLE

         if (DoTest) write(*,*) 'testing message_pass_cell with', &
              ' nProlongOrder=', nProlongOrder, &
              ' nCoarseLayer=', nCoarseLayer, &
              ' nWidth=', nWidth

         ! Set the range of ghost cells that should be set
         iMin = 1 - nWidth
         jMin = 1; if (nDim > 1) jMin = 1 - nWidth
         kMin = 1; if (nDim > 2) kMin = 1 - nWidth
         iMax = nI + nWidth
         jMax = nJ; if (nDim > 1) jMax = nJ + nWidth
         kMax = nK; if (nDim > 2) kMax = nK + nWidth

         do iSendCorner = 1, 2; do iRestrictFace = 1, 2

            DoSendCorner = iSendCorner == 2
            DoRestrictFace = iRestrictFace == 2

            ! Second order prolongation does not work with restricting face:
            ! the first order restricted cell cannot be used in the
            ! prolongation.
            if (DoRestrictFace .and. nProlongOrder == 2) CYCLE

            if (DoTest) write(*,*) 'testing message_pass_cell with', &
                 ' DoSendCorner=', DoSendCorner, &
                 ' DoRestrictFace=', DoRestrictFace

            State_VGB = 0.0

            do iBlock = 1, nBlock
               if (Unused_B(iBlock)) CYCLE
               State_VGB(:, 1:nI, 1:nJ, 1:nK, iBlock) = &
                    Xyz_DGB(1:nDim, 1:nI, 1:nJ, 1:nK, iBlock)
            end do

            call message_pass_cell(nVar, State_VGB, &
                 nProlongOrderIn=nProlongOrder, &
                 nCoarseLayerIn=nCoarseLayer, &
                 nWidthIn=nWidth, &
                 DoSendCornerIn=DoSendCorner, &
                 DoRestrictFaceIn=DoRestrictFace)

            do iBlock = 1, nBlock
               if (Unused_B(iBlock)) CYCLE

               ! Loop through all cells including ghost cells
               do k = MinK, MaxK; do j = MinJ, MaxJ; do i = MinI, MaxI

                  ! The filled in second order accurate ghost cell value
                  ! should be the same as the coordinates of the cell center
                  Xyz_D = Xyz_DGB(:,i,j,k,iBlock)

                  ! Check that no info is sent in the non-used dimensions,
                  ! i.e. for all iDim: nDim+1 < iDim < MaxDim
                  if (i < iMin .or. i > iMax .or. &
                       j < jMin .or. j > jMax .or. &
                       k < kMin .or. k > kMax) then

                     do iDim = 1, nDim
                        if (abs(State_VGB(iDim,i,j,k,iBlock)) > 1e-6) then
                           write(*,*) 'Face should not be set: ', &
                                'iProc,iBlock,i,j,k,iDim,State,Xyz=', &
                                iProc, iBlock, i, j, k, iDim, &
                                State_VGB(iDim,i,j,k,iBlock), &
                                Xyz_D(iDim)
                        end if
                     end do

                     CYCLE
                  end if

                  ! Get the direction vector
                  iDir = 0; if (i < 1) iDir = -1; if (i > nI) iDir = 1
                  jDir = 0; if (j < 1) jDir = -1; if (j > nJ) jDir = 1
                  kDir = 0; if (k < 1) kDir = -1; if (k > nK) kDir = 1

                  ! If nCoarseLayer==2 and DoSendCorner is true
                  ! the second ghost cells in the corner/edges
                  ! are not well defined (they may contain
                  ! the value coming from the first or second coarse cell).

                  if (nCoarseLayer == 2 .and. DoSendCorner .and. ( &
                       (i<0 .or. i>nI+1) .and. (jDir /= 0 .or. kDir /= 0) .or.&
                       (j<0 .or. j>nJ+1) .and. (iDir /= 0 .or. kDir /= 0) .or.&
                       (k<0 .or. k>nK+1) .and. (iDir /= 0 .or. jDir /= 0) &
                       )) CYCLE

                  ! if we do not send corners and edges, check that the
                  ! State_VGB in these cells is still the unset value
                  if (.not. DoSendCorner .and. ( &
                       iDir /= 0 .and. jDir /= 0 .or. &
                       iDir /= 0 .and. kDir /= 0 .or. &
                       jDir /= 0 .and. kDir /= 0)) then

                     do iDim = 1, nDim
                        if (abs(State_VGB(iDim,i,j,k,iBlock)) > 1e-6) then
                           write(*,*) 'corner/edge should not be set: ', &
                                'iProc,iBlock,i,j,k,iDim,State,Xyz=', &
                                iProc, iBlock, i, j, k, iDim, &
                                State_VGB(iDim,i,j,k,iBlock), &
                                Xyz_D
                        end if
                     end do

                     CYCLE
                  end if

                  ! Shift ghost cell coordinate into periodic domain
                  Xyz_D = CoordMin_D + modulo(Xyz_D - CoordMin_D, DomainSize_D)

                  ! Calculate distance of ghost cell layer
                  Di = 0; Dj = 0; Dk = 0
                  if (i < 1 .and. iRatio == 2) Di = 2*i - 1
                  if (i > nI .and. iRatio == 2) Di = 2*(i - nI) - 1
                  if (j < 1 .and. jRatio == 2) Dj = 2*j - 1
                  if (j > nJ .and. jRatio == 2) Dj = 2*(j - nJ) - 1
                  if (k < 1 .and. kRatio == 2) Dk = 2*k - 1
                  if (k > nK .and. kRatio == 2) Dk = 2*(k - nK) - 1

                  if (DoRestrictFace .and. &
                       DiLevelNei_IIIB(iDir, jDir, kDir, iBlock) == -1) then
                     ! Shift coordinates if only 1 layer of fine cells
                     ! is averaged in the orthogonal direction
                     Xyz_D(1) = Xyz_D(1) - 0.25*Di*CellSize_DB(1, iBlock)
                     Xyz_D(2) = Xyz_D(2) - 0.25*Dj*CellSize_DB(2, iBlock)
                     Xyz_D(3) = Xyz_D(3) - 0.25*Dk*CellSize_DB(3, iBlock)
                  end if

                  if (nProlongOrder == 1 .and. &
                       DiLevelNei_IIIB(iDir, jDir, kDir, iBlock) == 1) then
                     ! Shift depends on the parity of the fine ghost cell
                     ! except when there is no AMR or multiple coarse cell
                     ! layers are sent in that direction
                     if (iRatio == 2 .and. (nCoarseLayer == 1 .or. iDir == 0))&
                          Di = 2*modulo(i, 2) - 1
                     if (jRatio == 2 .and. (nCoarseLayer == 1 .or. jDir == 0))&
                          Dj = 2*modulo(j, 2) - 1
                     if (kRatio == 2 .and. (nCoarseLayer == 1 .or. kDir == 0))&
                          Dk = 2*modulo(k, 2) - 1

                     Xyz_D(1) = Xyz_D(1) + 0.5*Di*CellSize_DB(1, iBlock)
                     Xyz_D(2) = Xyz_D(2) + 0.5*Dj*CellSize_DB(2, iBlock)
                     Xyz_D(3) = Xyz_D(3) + 0.5*Dk*CellSize_DB(3, iBlock)
                  end if

                  do iDim = 1, nDim
                     if (abs(State_VGB(iDim,i,j,k,iBlock) - Xyz_D(iDim)) &
                          > Tolerance) then
                        write(*,*) 'iProc,iBlock,i,j,k,iDim,State,Xyz=', &
                             iProc, iBlock, i, j, k, iDim, &
                             State_VGB(iDim,i,j,k,iBlock), &
                             Xyz_D(iDim)
                     end if
                  end do
               end do; end do; end do
            end do

         end do; end do; end do
      end do; end do ! test parameters
      deallocate (State_VGB)

      call clean_grid
      call clean_tree

    end subroutine test_switches
    !==========================================================================
    subroutine test_scalar
      !------------------------ Test Scalar -----------------------------

      ! To test the message pass for the cell with min max operators we
      ! generate a fine uniform grid for the whole domain and transfer the
      ! cell values from the block cells to the cells on the fine grid.
      ! Then we gather all the data
      ! on the fine grid with the proper operator.
      ! We can then compare the values on the coresponding node after
      ! message_pass_cell_scalar is called with the fine grid values.

      ! rescale the domain to make indexing easier
      !------------------------------------------------------------------------
      DomainSize_D = iRatio_D*nRootTest_D*nIJK_D
      DomainMin_D = [0.0, 0.0, 0.0]
      DomainMax_D = DomainSize_D

      call init_tree(MaxBlockTest)
      call init_grid(DomainMin_D(1:nDim), DomainMax_D(1:nDim))
      call init_geometry(IsPeriodicIn_D=IsPeriodicTest_D(1:nDim))
      call set_tree_root(nRootTest_D(1:nDim))

      call find_tree_node([0.5, 0.5, 0.5], iNode)
      call refine_tree_node(iNode)
      call distribute_tree(.true.)
      call create_grid

      ! Position of cell corners, for solving problems with round-off
      ! when getting fine grid positions
      allocate(XyzCorn_DGB(MaxDim,MinI:MaxI,MinJ:MaxJ,MinK:MaxK,MaxBlockTest))
      do iBlock = 1, nBlock
         if (Unused_B(iBlock)) CYCLE
         do k = MinK, MaxK; do j = MinJ, MaxJ; do i = MinI, MaxI
            XyzCorn_DGB(:,i,j,k,iBlock) = &
                 Xyz_DGB(:,i,j,k,iBlock) - &
                 0.5*CellSize_DB(:, iBlock)* &
                 [min(1, nI - 1), min(1, nJ - 1), min(1, nK - 1)]
         end do; end do; end do
      end do

      allocate (Scalar_GB(MinI:MaxI, MinJ:MaxJ, MinK:MaxK, MaxBlockTest))
      Scalar_GB = -7777

      allocate (FineGridLocal_III( &
           nI*iRatio*nRootTest_D(1), &
           nJ*jRatio*nRootTest_D(2), &
           nK*kRatio*nRootTest_D(3)))
      allocate (FineGridGlobal_III( &
           (nI)*iRatio*nRootTest_D(1), &
           (nJ)*jRatio*nRootTest_D(2), &
           (nK)*kRatio*nRootTest_D(3)))

      nFineCell = ((nI)*iRatio*nRootTest_D(1))* &
           ((nJ)*jRatio*nRootTest_D(2))* &
           ((nK)*kRatio*nRootTest_D(3))

      FineGridStep_D = DomainSize_D/(DomainMax_D - DomainMin_D)

      do iOp = 1, nOp

         NameOperator = NameOperator_I(iOp)
         select case (NameOperator)
         case ("min")
            FineGridLocal_III(:, :, :) = 1.0e8
            FineGridGlobal_III(:, :, :) = 1.0e8
            iMpiOperator = MPI_MIN
         case ("max")
            FineGridLocal_III(:, :, :) = -1.0e8
            FineGridGlobal_III(:, :, :) = -1.0e8
            iMpiOperator = MPI_MAX
         case default
            call CON_stop(NameSub//': incorrect operator name')
         end select

         if (DoTest) write(*,*) 'testing message_pass_cell_scalar ', &
              'with operator= ', NameOperator

         do iBlock = 1, nBlock
            if (Unused_B(iBlock)) CYCLE
            do k = 1, nK; do j = 1, nJ; do i = 1, nI
               Scalar_GB(i,j,k,iBlock) = iNode_B(iBlock) + &
                    sum(CoordMin_DB(:, iBlock) + &
                    ([i, j, k])*CellSize_DB(:, iBlock))
            end do; end do; end do
         end do

         do iBlock = 1, nBlock
            if (Unused_B(iBlock)) CYCLE
            do k = 1, nK; do j = 1, nJ; do i = 1, nI

               iFG = nint(XyzCorn_DGB(1,i,j,k,iBlock)*FineGridStep_D(1)) + 1
               jFG = nint(XyzCorn_DGB(2,i,j,k,iBlock)*FineGridStep_D(2)) + 1
               kFG = nint(XyzCorn_DGB(3,i,j,k,iBlock)*FineGridStep_D(3)) + 1

               FineGridLocal_III(iFG, jFG, kFG) = Scalar_GB(i,j,k,iBlock)
            end do; end do; end do
         end do

         call message_pass_cell(Scalar_GB, &
              nProlongOrderIn=1, nCoarseLayerIn=2, &
              DoSendCornerIn=.true., DoRestrictFaceIn=.false., &
              NameOperatorIn=NameOperator_I(iOp))

         call MPI_ALLREDUCE(FineGridLocal_III(1, 1, 1), &
              FineGridGlobal_III(1, 1, 1), &
              nFineCell, MPI_REAL, iMpiOperator, iComm, iError)

         ! making sure that we have the center cell along the x=0 side
         ! so the boundary are not tested.
         call find_tree_node([0.0, 0.5, 0.5], iNode)

         do iBlock = 1, nBlock
            if (Unused_B(iBlock)) CYCLE
            if (iNode_B(iBlock) == iNode) then
               do k = MinK, MaxK; do j = MinJ, MaxJ; do i = 1, MaxI

                  iFG = nint(XyzCorn_DGB(1,i,j,k,iBlock)*FineGridStep_D(1)) + 1
                  jFG = nint(XyzCorn_DGB(2,i,j,k,iBlock)*FineGridStep_D(2)) + 1
                  kFG = nint(XyzCorn_DGB(3,i,j,k,iBlock)*FineGridStep_D(3)) + 1
                  ! copy cells that are inside the course grid cell
                  CourseGridCell_III = FineGridGlobal_III( &
                       iFG:iFG + min(1, iRatio - 1), &
                       jFG:jFG + min(1, jRAtio - 1), &
                       kFG:kFG + min(1, kRatio - 1))
                  select case (NameOperator)
                  case ("min")
                     if (Scalar_GB(i,j,k,iBlock) /= &
                          minval(CourseGridCell_III))then
                        write(*,*) "Error for operator, iNode,  iBlock= ", &
                             NameOperator, iNode_B(iBlock), iBlock, &
                             ", value=", minval(CourseGridCell_III), &
                             " should be ", Scalar_GB(i,j,k,iBlock), &
                             "index : ", i, j, k, " ::", iFG, jFG, kFG
                     end if
                  case ("max")
                     if (Scalar_GB(i,j,k,iBlock) /= &
                          maxval(CourseGridCell_III)) then
                        write(*,*) "Error for operator, iNode,  iBlock= ", &
                             NameOperator, iNode_B(iBlock), iBlock, ", &
                             value=", maxval(CourseGridCell_III), &
                             " should be ", Scalar_GB(i,j,k,iBlock), &
                             "index : ", i, j, k, " ::", iFG, jFG, kFG
                     end if
                  end select

               end do; end do; end do
            end if
         end do

      end do
      deallocate(Scalar_GB, FineGridLocal_III, FineGridGlobal_III, XyzCorn_DGB)
      call clean_grid
      call clean_tree

    end subroutine test_scalar
    !==========================================================================
    subroutine test_non_cartesian
      !------------------------------------------------------------------------
      do iTest = 1, 6
         ! The code is quite inaccurate for partial AMR across the pole
         if (nDimAmr < nDim .and. iTest > 3) EXIT
         call init_tree(MaxBlockTest)
         ! Do not test ghost cells in the radial direction
         iMin = 1; iMax = nI
         jMin = MinJ; jMax = MaxJ
         kMin = MinK; kMax = MaxK
         select case (iTest)
         case (1, 4)
            NameGeometry = 'cylindrical'
            ! 0 < r < 10, 0 < phi < 360deg, -5 < z < 5
            DomainMin_D = [0.0, 0.0, -5.0]
            DomainMax_D = [8.0, cTwoPi, +5.0]
            IsPeriodicTest_D = [.false., .true., .true.]
            ! There must be an even number of root blocks in the phi direction
            ! There are 3 root blocks in z so that we can refine the middle
            ! and avoid issues of periodicity in the testing
            nRootTest_D = [2, 4, 3]
            ! Test ghost cells at rMin
            iMin = MinI
         case (2, 5)
            if (nDim < 3) CYCLE
            NameGeometry = 'spherical'
            ! 1 < r < 9, 0 < theta < 180deg, 0 < phi < 360deg
            DomainMin_D = [1.0, 0.0, 0.0]
            DomainMax_D = [9.0, cPi, cTwoPi]
            IsPeriodicTest_D = [.false., .false., .true.]
            ! There must be an even number of root blocks in the phi direction
            ! There are 3 root blocks in r so that we can refine the middle
            ! and avoid issues at inner and outer radial boundaries
            nRootTest_D = [3, 2, 4]

         case (3, 6)
            if (nDim < 3) CYCLE
            NameGeometry = 'rlonlat'
            ! 1 < r < 9, 0 < phi < 360deg, -90 < lat < 90
            DomainMin_D = [1.0, 0.0, -cHalfPi]
            DomainMax_D = [9.0, cTwoPi, cHalfPi]
            IsPeriodicTest_D = [.false., .true., .false.]
            ! There must be an even number of root blocks in the phi direction
            ! There are 3 root blocks in r so that we can refine the middle
            ! and avoid issues at inner and outer radial boundaries
            nRootTest_D = [3, 4, 2]
         end select
         DomainSize_D = DomainMax_D - DomainMin_D
         if (DoTest) then
            if (iTest <= 3) write(*,*) &
                 'testing message_pass_cell across '//trim(NameGeometry)// &
                 ' pole'
            if (iTest >= 4) write(*,*) &
                 'testing message_pass_cell across '//trim(NameGeometry)// &
                 ' pole with resolution change'
         end if
         call init_geometry(NameGeometry, &
              IsPeriodicIn_D=IsPeriodicTest_D(1:nDim))
         call init_grid(DomainMin_D(1:nDim), DomainMax_D(1:nDim), &
              UseDegreeIn=.false.)
         call set_tree_root(nRootTest_D(1:nDim))
         if (any(IsPeriodic_D(1:nDim) .neqv. IsPeriodicTest_D(1:nDim))) &
              write(*,*) NameSub, ': IsPeriodic_D=', IsPeriodic_D(1:nDim), &
              ' should agree with ', IsPeriodicTest_D(1:nDim)
         if (iTest > 3) then
            ! Test with refined grid
            if (iTest == 4) then
               ! refine node next to r=0 axis but middle in Z direction
               call refine_tree_node(9)
            else
               ! refine root nodes at min and max (theta/lat/phi) coordinates
               ! but middle in the R direction
               call refine_tree_node(2)
               call refine_tree_node(23)
            end if
            ! Restriction is not linear so there is truncation error
            Tolerance = 0.15
         else
            ! For tests with no AMR the error is round-off only
            Tolerance = 1e-6
         end if

         call distribute_tree(.true.)
         call create_grid
         allocate(State_VGB(nVar,MinI:MaxI,MinJ:MaxJ,MinK:MaxK,MaxBlockTest))
         State_VGB = 0.0
         do iBlock = 1, nBlock
            if (Unused_B(iBlock)) CYCLE
            State_VGB(:, 1:nI, 1:nJ, 1:nK, iBlock) = &
                 Xyz_DGB(1:nDim, 1:nI, 1:nJ, 1:nK, iBlock)
         end do
         ! Second order
         call message_pass_cell(nVar, State_VGB)
         do iBlock = 1, nBlock
            if (Unused_B(iBlock)) CYCLE
            ! Loop through all cells including ghost cells
            do k = kMin, kMax; do j = jMin, jMax; do i = iMin, iMax
               ! The filled in second order accurate ghost cell value
               ! should be the same as the coordinates of the cell center
               Xyz_D = Xyz_DGB(:,i,j,k,iBlock)
               ! For 3D cylindrical Z coordinate is periodic
               if ((iTest == 1 .or. iTest == 4) .and. nDim == 3) &
                    Xyz_D(z_) = DomainMin_D(z_) &
                    + modulo(Xyz_D(z_) - DomainMin_D(z_), DomainSize_D(z_))
               do iDim = 1, nDim
                  if (abs(State_VGB(iDim,i,j,k,iBlock) - Xyz_D(iDim)) &
                       /abs(Xyz_D(iDim)) > Tolerance) then
                     write(*,*) 'iProc,iBlock,i,j,k,iDim,State,Xyz=', &
                          iProc, iBlock, i, j, k, iDim, &
                          State_VGB(iDim,i,j,k,iBlock), &
                          Xyz_D(iDim)
                  end if
               end do
            end do; end do; end do
         end do

         deallocate (State_VGB)

         call clean_grid
         call clean_tree
      end do

      ! In previous do loop, it may cycle some loops without cleaning.
      call clean_grid
      call clean_tree

    end subroutine test_non_cartesian
    !==========================================================================
    subroutine test_high_order_cartesian
      real    :: ExactSolution, Error, ErrorTotal
      integer :: iCount, nCount, nRefineNode, iRefinement, nRefinement
      integer :: iNode_I(8)
      integer :: iNode1_I(8)
      logical :: DoTestMeOnly = .false.
      integer :: nPoly = 3
      !------------------------------------------------------------------------
      if (nDim == 2) then
         nCount = 16; nRefineNode = 4
      else
         nCount = 256; nRefineNode = 8
      endif
      if (nDimAmr < nDim) RETURN
      iMin = MinI; iMax = MaxI
      jMin = MinJ; jMax = MaxJ
      kMin = MinK; kMax = MaxK
      NameGeometry = 'cartesian'
      DomainMin_D = [0.0, 0.0, 0.0]
      DomainMax_D = [8.0, 8.0, 8.0]
      DomainSize_D = DomainMax_D - DomainMin_D

      IsPeriodicTest_D = [.true., .true., .true.]
      nRootTest_D = [4, 4, 4]
      nRefinement = 2

      do iRefinement = 1, nRefinement
         ! iRefinement = 1: 1 level refine
         ! iRefinement = 2: 2 level refine
         if (DoTest) then
            write(*,*) &
                 'testing message_pass_cell across '//trim(NameGeometry)// &
                 ' with high resolution change with refinement level =', &
                 iRefinement
         end if
         if (nDim == 2) then
            if (iRefinement == 1) then
               iNode_I = [6, 7, 10, 11, -1, -1, -1, -1]
            else
               iNode1_I = [6, 7, 10, 11, -1, -1, -1, -1]
               iNode_I = [20, 23, 26, 29, -1, -1, -1, -1]
            endif
         else ! 3D
            if (iRefinement == 1) then
               iNode_I = [22, 23, 26, 27, 38, 39, 42, 43]
            else
               iNode1_I = [22, 23, 26, 27, 38, 39, 42, 43]
               iNode_I = [72, 79, 86, 93, 100, 107, 114, 121]
            endif
         endif
         do iCount = 0, nCount - 1
            if (DoTestMeOnly) then
               write(*,*) ''
               write(*,*) 'test_high_order iCount = ', iCount
            endif
            call init_tree(MaxBlockTest)
            call init_geometry(NameGeometry, &
                 IsPeriodicIn_D=IsPeriodicTest_D(1:nDim))

            call init_grid(DomainMin_D(1:nDim), DomainMax_D(1:nDim), &
                 UseDegreeIn=.false.)
            call set_tree_root(nRootTest_D(1:nDim))

            if (any(IsPeriodic_D(1:nDim) .neqv. IsPeriodicTest_D(1:nDim))) &
                 write(*,*) NameSub, ': IsPeriodic_D=', IsPeriodic_D(1:nDim), &
                 ' should agree with ', IsPeriodicTest_D(1:nDim)

            if (iRefinement == 2) then
               do iNode = 1, nRefineNode
                  call refine_tree_node(iNode1_I(iNode))
               enddo
            endif

            do iNode = 1, nRefineNode
               if (btest(iCount, iNode - 1)) then
                  call refine_tree_node(iNode_I(iNode))
                  if (DoTestMeOnly) &
                       write(*,*) 'iNode IsRefined:', iNode_I(iNode), 'TRUE'
               else
                  if (DoTestMeOnly) &
                       write(*,*) 'iNode IsRefined:', iNode_I(iNode), 'FALSE'
               endif
            enddo

            Tolerance = 5e-15

            call distribute_tree(.true.)
            call create_grid

            allocate ( &
                 State_VGB(nVar,MinI:MaxI,MinJ:MaxJ,MinK:MaxK,MaxBlockTest))
            State_VGB = 0

            do iBlock = 1, nBlock
               if (Unused_B(iBlock)) CYCLE
               do i = 1, nI; do j = 1, nJ; do k = 1, nK
                  State_VGB(1,i,j,k,iBlock) = &
                       exact_solution(Xyz_DGB(:,i,j,k,iBlock), nPolyIn=nPoly)
               enddo; enddo; enddo
            end do

            call message_pass_cell(nVar, nG, State_VGB, nProlongOrderIn=1, &
                 nCoarseLayerIn=2, DoResChangeOnlyIn=.false., &
                 UseHighResChangeIn=.true.)
            ErrorTotal = 0
            do iBlock = 1, nBlock
               if (Unused_B(iBlock)) CYCLE

               ! Loop through all cells including ghost cells
               do k = kMin, kMax; do j = jMin, jMax; do i = iMin, iMax
                  Xyz_D = Xyz_DGB(:,i,j,k,iBlock)
                  if (.not. (all(Xyz_D(1:nDim) < DomainMax_D(1:nDim)) &
                       .and. all(Xyz_D(1:nDim) > DomainMin_D(1:nDim)))) then
                     CYCLE
                  endif

                  ExactSolution = exact_solution(Xyz_D, nPolyIn=nPoly)
                  Error = abs(ExactSolution - State_VGB(1,i,j,k,iBlock))
                  ErrorTotal = ErrorTotal + Error
                  if (abs(Error)/abs(ExactSolution) > Tolerance) &
                       then
                     write(*,*) &
                          'iProc,iNode,i,j,k,x,y,z,', &
                          'state,exact-solution,error,relative-error='
                     write (*, '(5I5,7e20.12)') &
                          iProc, iNode_B(iBlock), i, j, k, &
                          Xyz_D, State_VGB(1,i,j,k,iBlock), &
                          ExactSolution, Error, abs(Error)/abs(ExactSolution)

                  end if
               end do; end do; end do
            end do
            if (DoTestMeOnly) then
               write(*,*) 'Refine level = ', iRefinement
               write(*,*) 'Total error  = ', ErrorTotal
            endif
            deallocate (State_VGB)

            call clean_grid
            call clean_tree
         enddo ! iCount
      enddo ! iRefinement

    end subroutine test_high_order_cartesian
    !==========================================================================
    subroutine test_high_order_non_cartesian
      real    :: ErrorTotal, ExactSolution, Error
      real    :: Xyz1_D(3), XyzGeneral_D(3)
      integer :: nPoly
      !------------------------------------------------------------------------
      do iTest = 1, 2

         ! The code is quite inaccurate for partial AMR across the pole
         if (nDimAmr < nDim .and. iTest > 3) EXIT

         call init_tree(MaxBlockTest)

         ! Do not test ghost cells in the radial direction
         iMin = 1; iMax = nI
         jMin = MinJ; jMax = MaxJ
         kMin = MinK; kMax = MaxK

         select case (iTest)
         case (1)

            NameGeometry = 'cylindrical'

            ! 2 < r < 8, 0 < phi < 360deg, -5 < z < 5
            DomainMin_D = [2.0, 0.0, -5.0]
            DomainMax_D = [8.0, cTwoPi, +5.0]
            IsPeriodicTest_D = [.false., .true., .true.]

            ! There must be an even number of root blocks in the phi direction
            ! There are 3 root blocks in z so that we can refine the middle
            ! and avoid issues of periodicity in the testing
            nRootTest_D = [3, 4, 3]

            ! Test ghost cells at rMin
            iMin = MinI

         case (2)
            NameGeometry = 'rotatedcartesian'

            DomainMin_D = [0.0, 0.0, 0.0]
            DomainMax_D = [6.0, 6.0, 6.0]

            IsPeriodicTest_D = [.false., .false., .false.]
            nRootTest_D = [3, 3, 3]
         end select
         DomainSize_D = DomainMax_D - DomainMin_D

         if (DoTest) then
            write(*,*) &
                 'testing message_pass_cell across '//trim(NameGeometry)// &
                 ' with high resolution change'
         end if

         call init_geometry(NameGeometry, &
              IsPeriodicIn_D=IsPeriodicTest_D(1:nDim))

         call init_grid(DomainMin_D(1:nDim), DomainMax_D(1:nDim), &
              UseDegreeIn=.false.)
         call set_tree_root(nRootTest_D(1:nDim))

         if (any(IsPeriodic_D(1:nDim) .neqv. IsPeriodicTest_D(1:nDim))) &
              write(*,*) NameSub, ': IsPeriodic_D=', IsPeriodic_D(1:nDim), &
              ' should agree with ', IsPeriodicTest_D(1:nDim)

         if (iTest == 1) then
            ! refine node next to r=0 axis but middle in Z direction
            if (nDim == 2) then
               call refine_tree_node(2)
            elseif (nDim == 3) then
               call refine_tree_node(17)
            endif
         elseif (iTest == 2) then
            if (nDim == 2) then
               call refine_tree_node(5)
            elseif (nDim == 3) then
               call refine_tree_node(14)
            endif
         end if

         if (iTest == 2) then
            Tolerance = 1e-14
            nPoly = 3
         else
            Tolerance = 7e-3
            nPoly = 1
         endif

         call distribute_tree(.true.)
         call create_grid

         allocate (State_VGB(nVar,MinI:MaxI,MinJ:MaxJ,MinK:MaxK,MaxBlockTest))
         State_VGB = 0

         do iBlock = 1, nBlock
            if (Unused_B(iBlock)) CYCLE
            do i = 1, nI; do j = 1, nJ; do k = 1, nK
               State_VGB(1,i,j,k,iBlock) = &
                    exact_solution(Xyz_DGB(:,i,j,k,iBlock), nPolyIn=nPoly)
            enddo; enddo; enddo
         end do

         call message_pass_cell(nVar, nG, State_VGB, nProlongOrderIn=1, &
              nCoarseLayerIn=2, DoResChangeOnlyIn=.false., &
              UseHighResChangeIn=.true.)

         ! Second order
         ! call message_pass_cell(nVar, State_VGB)

         ErrorTotal = 0
         do iBlock = 1, nBlock
            if (Unused_B(iBlock)) CYCLE
            ! Loop through all cells including ghost cells
            do k = kMin, kMax; do j = jMin, jMax; do i = iMin, iMax

               Xyz_D = Xyz_DGB(:,i,j,k,iBlock)

               if (iTest == 2) then
                  Xyz1_D = rot_to_cart(Xyz_D)
               else
                  call Xyz_to_coord(Xyz_D, Xyz1_D)
               endif

               if (.not. (all(Xyz1_D(1:nDim) < DomainMax_D(1:nDim)) &
                    .and. all(Xyz1_D(1:nDim) > DomainMin_D(1:nDim)))) then
                  CYCLE
               endif

               ExactSolution = exact_solution(Xyz_D, nPolyIn=nPoly)
               Error = abs(ExactSolution - State_VGB(1,i,j,k,iBlock))
               ErrorTotal = ErrorTotal + Error/abs(ExactSolution)

               if (abs(Error)/abs(ExactSolution) > Tolerance) then
                  write(*,*) &
                       'iProc,iNode,i,j,k,x,y,z,', &
                       'state,exact-solution,error,relative-error='
                  write (*, '(5I5,7e20.12)') &
                       iProc, iNode_B(iBlock), i, j, k, &
                       Xyz_D, State_VGB(1,i,j,k,iBlock), &
                       ExactSolution, Error, abs(Error)/abs(ExactSolution)
                  call Xyz_to_coord(Xyz_D, XyzGeneral_D)
                  write(*,*) 'Xyz general = ', XyzGeneral_D
                  write(*,*) ''
               end if

            end do; end do; end do
         end do

         deallocate (State_VGB)

         call clean_grid
         call clean_tree
      end do ! iTest

    end subroutine test_high_order_non_cartesian
    !==========================================================================
    real function exact_solution(Xyz_D, nPolyIn)
      real, intent(in):: Xyz_D(3)
      integer, optional, intent(in) :: nPolyIn
      integer:: nPoly
      real:: x, y, z
      !------------------------------------------------------------------------
      nPoly = 4
      if (present(nPolyIn)) nPoly = nPolyIn

      x = Xyz_D(1)
      y = Xyz_D(2)
      z = Xyz_D(3)

      ! exact_solution = 4.0+sin(y*2*cpi/8.0)+cos(z*2*cpi/8.0)+sin(x*2*cpi/8.0)
      select case (nPoly)
      case (4)
         exact_solution = x**4 + y**4 + z**4 + x**3*y &
              + y**3*x + x**3*z + x*z**3 + y**3*z + y*z**3 &
              + x**2*y**2 + x**2*z**2 + y**2*z**2 &
              + y*z*x**2 + x*z*y**2 + x*y*z**2
      case (3)
         exact_solution = x**3 + y**3 + z**3 + x*y*z + x**2*y + x*y**2 + &
              x**2*z + x*z**2 + y**2*z + y*z**2
      case (1)
         exact_solution = x + y + z
      end select
    end function exact_solution
    !==========================================================================
  end subroutine test_pass_cell
  !============================================================================
  subroutine test_pass_face

    integer, parameter:: MaxBlockTest = 50
    integer, parameter:: nRootTest_D(MaxDim) = [3, 3, 3]
    logical, parameter:: IsPeriodicTest_D(MaxDim) = .true.
    real, parameter:: DomainMin_D(MaxDim) = [1.0, 10.0, 100.0]
    real, parameter:: DomainMax_D(MaxDim) = [4.0, 40.0, 400.0]

    real, parameter:: Tolerance = 1e-6

    integer, parameter:: nVar = nDim
    real, allocatable, dimension(:, :, :, :, :):: &
         Flux_VFD, Flux_VXB, Flux_VYB, Flux_VZB

    integer:: iResChangeOnly
    logical:: DoResChangeOnly

    integer:: iNode, iBlock, i, j, k, iDim, DiLevel
    integer:: iStage, nStage, iTimeLevel
    real:: Flux, FluxGood, FluxUnset = -77.0

    logical:: DoTest
    character(len=*), parameter:: NameSub = 'test_pass_face'
    !--------------------------------------------------------------------------
    DoTest = iProc == 0

    if (DoTest) write(*,*) 'Starting ', NameSub

    call init_tree(MaxBlockTest)
    call init_geometry(IsPeriodicIn_D=IsPeriodicTest_D(1:nDim))
    call init_grid(DomainMin_D(1:nDim), DomainMax_D(1:nDim))
    call set_tree_root(nRootTest_D(1:nDim))

    call find_tree_node([0.5, 0.5, 0.5], iNode)
    if (DoTest) write(*,*) NameSub, ' middle node=', iNode
    call refine_tree_node(iNode)
    call distribute_tree(.true.)
    call create_grid

    allocate ( &
         Flux_VFD(nVar, nI + 1, nJ + 1, nK + 1, nDim), &
         Flux_VXB(nVar, nJ, nK, 2, MaxBlockTest), &
         Flux_VYB(nVar, nI, nK, 2, MaxBlockTest), &
         Flux_VZB(nVar, nI, nJ, 2, MaxBlockTest), &
         iTimeLevel_A(nNode))

    do iNode = 1, nNode
       iTimeLevel_A(iNode) = modulo(iNode, 4) + 5*iTree_IA(Level_, iNode)
    end do

    if (DoTest) &
         call show_tree(NameSub, DoShowNei=.true.) ! DoShowTimeLevel=.true.)

    do iResChangeOnly = 1, 2

       DoResChangeOnly = iResChangeOnly == 1
       UseTimeLevel = .not. DoResChangeOnly

       if (DoTest) write(*,*) 'testing message_pass_face with', &
            ' DoResChangeOnly, UseTimeLevel=', DoResChangeOnly, UseTimeLevel

       Flux_VFD = 0.0
       Flux_VXB = 0.0
       Flux_VYB = 0.0
       Flux_VZB = 0.0

       do iBlock = 1, nBlock
          if (Unused_B(iBlock)) CYCLE

          ! Set Flux_VFD based on the coordinates
          call set_flux

          if (UseTimeLevel) then
             ! Pretend subsycling nStage times
             nStage = iTimeLevel_A(iNode_B(iBlock))
             do iStage = 1, nStage
                call store_face_flux(iBlock, nVar, Flux_VFD, &
                     Flux_VXB, Flux_VYB, Flux_VZB, &
                     DoResChangeOnlyIn=.false., &
                     DoStoreCoarseFluxIn=.true., &
                     DtIn=1.0/nStage)
             end do
          else
             call store_face_flux(iBlock, nVar, Flux_VFD, &
                  Flux_VXB, Flux_VYB, Flux_VZB, &
                  DoResChangeOnlyIn=.true., &
                  DoStoreCoarseFluxIn=.true.)
          end if

          iTimeLevel = iTimeLevel_A(iNode_B(iBlock))

          ! Set the fluxes to the unset value except on the fine side
          if (di_level_nei(-1, 0, 0, iBlock, DoResChangeOnly) < 1) &
               Flux_VXB(:, :, :, 1, iBlock) = FluxUnset

          if (di_level_nei(+1, 0, 0, iBlock, DoResChangeOnly) < 1) &
               Flux_VXB(:, :, :, 2, iBlock) = FluxUnset

          if (nDim > 1) then
             if (di_level_nei(0, -1, 0, iBlock, DoResChangeOnly) < 1) &
                  Flux_VYB(:, :, :, 1, iBlock) = FluxUnset

             if (di_level_nei(0, +1, 0, iBlock, DoResChangeOnly) < 1) &
                  Flux_VYB(:, :, :, 2, iBlock) = FluxUnset

          end if

          if (nDim > 2) then
             if (di_level_nei(0, 0, -1, iBlock, DoResChangeOnly) < 1) &
                  Flux_VZB(:, :, :, 1, iBlock) = FluxUnset

             if (di_level_nei(0, 0, +1, iBlock, DoResChangeOnly) < 1) &
                  Flux_VZB(:, :, :, 2, iBlock) = FluxUnset

          end if

       end do

       ! Message pass from fine to coarse grid/time levels
       ! Zero out the fine side.
       call message_pass_face(nVar, Flux_VXB, Flux_VYB, Flux_VZB, &
            DoSubtractIn=.false., DoResChangeOnlyIn=DoResChangeOnly)

       do iBlock = 1, nBlock
          if (Unused_B(iBlock)) CYCLE

          ! Calculate the coordinate based Flux_VFD again
          call set_flux

          ! Check min X face
          DiLevel = di_level_nei(-1, 0, 0, iBlock, DoResChangeOnly)
          do k = 1, nK; do j = 1, nJ; do iDim = 1, nDim
             Flux = Flux_VXB(iDim, j, k, 1, iBlock)
             FluxGood = flux_good(DiLevel, 1, Flux_VFD(iDim, 1, j, k, 1))

             if (abs(Flux - FluxGood) > Tolerance) then
                write(*,*) 'Error at min X face: ', &
                     'iNode,DiLevel,j,k,iDim,Flux,Good=', &
                     iNode_B(iBlock), DiLevel, j, k, iDim, Flux, FluxGood, &
                     CellFace_DB(1, iBlock)
             end if
          end do; end do; end do

          ! Check max X face
          DiLevel = di_level_nei(+1, 0, 0, iBlock, DoResChangeOnly)
          do k = 1, nK; do j = 1, nJ; do iDim = 1, nDim
             Flux = Flux_VXB(iDim, j, k, 2, iBlock)
             FluxGood = flux_good(DiLevel, 1, Flux_VFD(iDim, nI + 1, j, k, 1))
             if (abs(Flux - FluxGood) > Tolerance) &
                  write(*,*) 'Error at max X face: ', &
                  'iNode,DiLevel,j,k,iDim,Flux,Good=', &
                  iNode_B(iBlock), DiLevel, j, k, iDim, Flux, FluxGood
          end do; end do; end do

          if (nDim > 1) then
             ! Check min Y face
             DiLevel = di_level_nei(0, -1, 0, iBlock, DoResChangeOnly)
             do k = 1, nK; do i = 1, nI; do iDim = 1, nDim
                Flux = Flux_VYB(iDim, i, k, 1, iBlock)
                FluxGood = flux_good(DiLevel, 2, Flux_VFD(iDim,i,1,k,2))
                if (abs(Flux - FluxGood) > Tolerance) &
                     write(*,*) 'Error at min Y face: ', &
                     'iNode,DiLevel,i,k,iDim,Flux,Good=', &
                     iNode_B(iBlock), DiLevel, i, k, iDim, Flux, FluxGood
             end do; end do; end do

             ! Check max Y face
             DiLevel = di_level_nei(0, +1, 0, iBlock, DoResChangeOnly)
             do k = 1, nK; do i = 1, nI; do iDim = 1, nDim
                Flux = Flux_VYB(iDim, i, k, 2, iBlock)
                FluxGood = flux_good(DiLevel, 2, Flux_VFD(iDim,i,nJ+1,k,2))
                if (abs(Flux - FluxGood) > Tolerance) &
                     write(*,*) 'Error at max Y face: ', &
                     'iNode,DiLevel,i,k,iDim,Flux,Good=', &
                     iNode_B(iBlock), DiLevel, i, k, iDim, Flux, FluxGood
             end do; end do; end do
          end if

          if (nDim > 2) then
             ! Check min Z face
             DiLevel = di_level_nei(0, 0, -1, iBlock, DoResChangeOnly)
             do j = 1, nJ; do i = 1, nI; do iDim = 1, nDim
                Flux = Flux_VZB(iDim, i, j, 1, iBlock)
                FluxGood = flux_good(DiLevel, 3, Flux_VFD(iDim,i,j,1,3))
                if (abs(Flux - FluxGood) > Tolerance) &
                     write(*,*) 'Error at min Z face: ', &
                     'iNode,DiLevel,i,j,iDim,Flux,Good=', &
                     iNode_B(iBlock), DiLevel, i, j, iDim, Flux, FluxGood
             end do; end do; end do

             ! Check max Z face
             DiLevel = di_level_nei(0, 0, +1, iBlock, DoResChangeOnly)
             do j = 1, nJ; do i = 1, nI; do iDim = 1, nDim
                Flux = Flux_VZB(iDim, i, j, 2, iBlock)
                FluxGood = flux_good(DiLevel, 3, Flux_VFD(iDim,i,j,nK+1,3))
                if (abs(Flux - FluxGood) > Tolerance) &
                     write(*,*) 'Error at max Z face: ', &
                     'iNode,DiLevel,i,j,iDim,Flux,Good=', &
                     iNode_B(iBlock), DiLevel, i, j, iDim, Flux, FluxGood
             end do; end do; end do
          end if

       end do ! iBlock

    end do ! test parameters
    deallocate (Flux_VFD, Flux_VXB, Flux_VYB, Flux_VZB)

    call clean_grid
    call clean_tree

  contains
    !==========================================================================
    subroutine set_flux

      ! Fill in Flux_VFD with coordinates of the face center
      !------------------------------------------------------------------------
      Flux_VFD(:, :, 1:nJ, 1:nK, 1) = 0.5*CellFace_DB(1, iBlock)* &
           (Xyz_DGB(1:nDim, 0:nI, 1:nJ, 1:nK, iBlock) &
           + Xyz_DGB(1:nDim, 1:nI + 1, 1:nJ, 1:nK, iBlock))

      if (nDim > 1) &
           Flux_VFD(:, 1:nI, :, 1:nK, 2) = 0.5*CellFace_DB(2, iBlock)* &
           (Xyz_DGB(1:nDim, 1:nI, 0:nJ, 1:nK, iBlock) &
           + Xyz_DGB(1:nDim, 1:nI, 1:nJ + 1, 1:nK, iBlock))

      if (nDim > 2) &
           Flux_VFD(:, 1:nI, 1:nJ, :, 3) = 0.5*CellFace_DB(3, iBlock)* &
           (Xyz_DGB(1:nDim, 1:nI, 1:nJ, 0:nK, iBlock) &
           + Xyz_DGB(1:nDim, 1:nI, 1:nJ, 1:nK + 1, iBlock))

    end subroutine set_flux
    !==========================================================================
    real function flux_good(DiLevel, iDir, XyzCellFace)

      integer, intent(in):: DiLevel, iDir
      real, intent(in):: XyzCellFace

      ! Calculate the correct flux solution in direction iDir based on the
      ! grid/time level change DiLevel at the given face.
      ! The coordinate XyzCellFace is the correct answer on the coarse side.
      ! Zero is the correct value on the fine side.
      ! FluxUnset is the correct value everywhere else.

      real:: CellFace
      !------------------------------------------------------------------------
      select case (DiLevel)
      case (0)
         flux_good = FluxUnset
      case (1)
         ! The flux is zeroed out on the fine side
         flux_good = 0.0
      case (-1)
         ! The flux is set to coordinate value on the coarse side
         flux_good = XyzCellFace

         ! Fix periodic coordinates. Flux = Coordinate*CellArea
         if (iDim == iDir) then
            CellFace = CellFace_DB(iDim, iBlock)
            if (abs(XyzCellFace/CellFace - DomainMin_D(iDim)) < Tolerance) &
                 flux_good = DomainMax_D(iDim)*CellFace
            if (abs(XyzCellFace/CellFace - DomainMax_D(iDim)) < Tolerance) &
                 flux_good = DomainMin_D(iDim)*CellFace
         endif
      case default
         call CON_stop(NameSub//' invalid value for DiLevel')
      end select

    end function flux_good
    !==========================================================================
  end subroutine test_pass_face
  !============================================================================
  subroutine test_amr

    integer, parameter:: MaxBlockTest = 100
    integer, parameter:: nRootTest_D(MaxDim) = [3, 3, 3]
    logical, parameter:: IsPeriodicTest_D(MaxDim) = .true.
    real, parameter:: DomainMin_D(MaxDim) = [1.0, 10.0, 100.0]
    real, parameter:: DomainMax_D(MaxDim) = [10.0, 100.0, 1000.0]
    real, parameter:: DomainSize_D(MaxDim) = DomainMax_D - DomainMin_D

    integer, parameter:: nVar = nDim
    real, allocatable:: State_VGB(:, :, :, :, :), Dt_B(:)
    real, allocatable:: TestState_VC(:, :, :, :)
    integer, allocatable:: iEffectedNode_A(:)
    integer:: iBlock, iDim, iNode, iVar, i, j, k
    integer:: iChild

    logical:: DoTest
    character(len=*), parameter:: NameSub = 'test_amr'
    !--------------------------------------------------------------------------
    DoTest = iProc == 0

    if (DoTest) write(*,*) 'Starting ', NameSub

    call init_tree(MaxBlockTest)
    call init_geometry(IsPeriodicIn_D=IsPeriodicTest_D(1:nDim))
    call init_grid(DomainMin_D(1:nDim), DomainMax_D(1:nDim))
    call set_tree_root(nRootTest_D(1:nDim))
    call distribute_tree(.true.)
    call create_grid
    call init_amr

    if (DoTest) call show_tree('after create_grid')

    allocate (State_VGB(nVar,MinI:MaxI,MinJ:MaxJ,MinK:MaxK,MaxBlockTest), &
         Dt_B(MaxBlockTest))

    do iBlock = 1, nBlock
       if (Unused_B(iBlock)) CYCLE
       State_VGB(:, :, :, :, iBlock) = Xyz_DGB(1:nDim, :, :, :, iBlock)
       ! set the time step to the cell size in the first AMR direction
       iDim = iDimAmr_D(1)
       Dt_B(iBlock) = DomainSize_D(iDim)/(nIjk_D(iDim)*nRootTest_D(iDim))
    end do

    if (DoTest) write(*,*) 'test prolong and balance'
    call refine_tree_node(1)
    if (DoTest) call show_tree('after refine_tree_node')
    call distribute_tree(.false.)
    if (DoTest) then
       call show_tree('after distribute_tree(.false.)')
       write(*,*) 'iProc, iProcNew_A=', iProc, iProcNew_A(1:nNode)
    end if

    call do_amr(nVar, State_VGB, Dt_B)
    if (DoTest) call show_tree('after do_amr')
    call move_tree
    if (DoTest) call show_tree('after move_tree')
    if (DoTest) write(*,*) 'iAmrChange_B=', iAmrChange_B(1:nBlock)

    call check_state

    if (DoTest) write(*,*) 'test restrict and balance'
    call coarsen_tree_node(1)
    if (DoTest) call show_tree('after coarsen_tree_node')
    call distribute_tree(.false.)
    if (DoTest) then
       call show_tree('after distribute_tree(.false.)')
       write(*,*) 'iProc, iProcNew_A=', iProc, iProcNew_A(1:nNode)
    end if
    call do_amr(nVar, State_VGB, Dt_B)
    if (DoTest) call show_tree('after do_amr')
    call move_tree
    if (DoTest) call show_tree('after move_tree')
    if (DoTest) write(*,*) 'iAmrChange_B=', iAmrChange_B(1:nBlock)

    call check_state

    ! tests with mask
    if (DoTest) write(*,*) 'test masked cells and extra data'

    allocate( &
         TestState_VC(nVar,nI,nJ,nK), ExtraData_IB(nExtraData,MaxBlockTest))

    do iBlock = 1, nBlock
       if (Unused_B(iBlock)) CYCLE
       State_VGB(:, :, :, :, iBlock) = Xyz_DGB(1:nDim, :, :, :, iBlock)
       ! set the time step to the cells size in the first AMR direction
       iDim = iDimAmr_D(1)
       Dt_B(iBlock) = DomainSize_D(iDim)/(nIjk_D(iDim)*nRootTest_D(iDim))
       ! Set extra data
       ExtraData_IB(:, iBlock) = &
            [real(iNode_B(iBlock)), Xyz_DGB(1, 1, 1, 1, iBlock)]
    end do

    call refine_tree_node(1)
    call distribute_tree(.false.)

    ! Mask out the middle cell of node 1
    if (iTree_IA(Proc_, 1) == iProc) then
       iBlock = iTree_IA(Block_, 1)
       State_VGB(:,(MinI+MaxI)/2,(MinJ+MaxJ)/2,(MinK+MaxK)/2,iBlock) = -7777
       Used_GB((MinI+MaxI)/2,(MinJ+MaxJ)/2,(MinK+MaxK)/2,iBlock) = .false.
    end if

    call do_amr(nVar, State_VGB, Dt_B, Used_GB=Used_GB, &
         nExtraData=nExtraData, &
         pack_extra_data=test_pack, &
         unpack_extra_data=test_unpack)

    call move_tree

    call check_extra_data

    ! Nodes that needs special care in the testing
    allocate (iEffectedNode_A(nNode))
    iEffectedNode_A = unset_

    ! 0: the masked cell, 1: x+ neighbor, 2: y+ neighbor
    ! 3: z+ neighbor
    ! The first child of Node of the refined node
    iChild = Child1_
    iEffectedNode_A(iTree_IA(iChild, 1)) = 0
    iChild = iChild + 1
    ! As masked cell is on the corner of the child nodes
    ! it will also affect the neighboring cell in i-direction
    ! The running "iChild" counter is using the Morton indexing
    ! to give the right neighbor independent of which directions
    ! are refined.
    if (iRatio == 2) then
       iEffectedNode_A(iTree_IA(iChild, 1)) = 1
       iChild = iChild + 1
    end if
    ! Neighbor in j direction
    if (jRatio == 2) then
       iEffectedNode_A(iTree_IA(iChild, 1)) = 2
       iChild = iChild + iRatio
    end if
    ! and k direction
    if (kRatio == 2) iEffectedNode_A(iTree_IA(iChild, 1)) = 3

    call check_state_mask

    !--------------- END refine --------------------
    iEffectedNode_A = unset_
    Used_GB = .true.

    if (iTree_IA(Proc_, iTree_IA(Child1_, 1)) == iProc) then
       iBlock = iTree_IA(Block_, iTree_IA(Child1_, 1))
       if (iRatio == 2) then
          i = nI
       else
          i = (MinI + MaxI)/2
       end if
       if (jRatio == 2) then
          j = nJ
       else
          j = (MinJ + MaxJ)/2
       end if
       if (kRatio == 2) then
          k = nK
       else
          k = (MinK + MaxK)/2
       end if

       State_VGB(:,i,j,k,iBlock) = -7777
       Used_GB(i,j,k,iBlock) = .false.
    end if

    call coarsen_tree_node(1)
    call distribute_tree(.false.)

    call do_amr(nVar, State_VGB, Dt_B, Used_GB=Used_GB, &
         nExtraData=nExtraData, &
         pack_extra_data=test_pack, &
         unpack_extra_data=test_unpack)

    call move_tree

    call check_extra_data

    call check_state_mask

    deallocate (State_VGB, Dt_B, iEffectedNode_A, TestState_VC, ExtraData_IB)

    call clean_grid
    call clean_tree

  contains
    !==========================================================================
    subroutine check_extra_data
      !------------------------------------------------------------------------
      do iBlock = 1, nBlock
         if (iAmrChange_B(iBlock) /= AmrMoved_) CYCLE
         if (abs(ExtraData_IB(1, iBlock) - iNode_B(iBlock)) > 1e-6 .or. &
              abs(ExtraData_IB(2, iBlock) - Xyz_DGB(1,1,1,1,iBlock)) > 1e-6) &
              write(*,*) NameSub, &
              ': error for iProc,iBlock,ExtraData,iNode,x=', &
              iProc, iBlock, ExtraData_IB(:, iBlock), &
              iNode_B(iBlock), Xyz_DGB(1,1,1,1,iBlock)
      end do

    end subroutine check_extra_data
    !==========================================================================
    subroutine check_state
      !------------------------------------------------------------------------
      do iBlock = 1, nBlock
         if (Unused_B(iBlock)) CYCLE

         if (any(abs(State_VGB(:, 1:nI, 1:nJ, 1:nK, iBlock) &
              - Xyz_DGB(1:nDim, 1:nI, 1:nJ, 1:nK, iBlock)) > 1e-6)) then
            write(*,*) NameSub, &
                 ': error for iProc,iBlock,maxloc=', iProc, iBlock, &
                 maxloc(abs(State_VGB(:, 1:nI, 1:nJ, 1:nK, iBlock) &
                 - Xyz_DGB(1:nDim, 1:nI, 1:nJ, 1:nK, iBlock)))
         end if

         iDim = iDimAmr_D(1)
         if (abs(Dt_B(iBlock) - CellSize_DB(iDim, iBlock)) > 1e-6) &
              write(*,*) NameSub, ' error for iProc,iBlock,dt,dx=', &
              iProc, iBlock, Dt_B(iBlock), CellSize_DB(iDim, iBlock)

      end do

    end subroutine check_state
    !==========================================================================
    subroutine check_state_mask

      integer :: iDn, iUp, jDn, jUp, kDn, kUp
      integer :: Di, Dj, Dk
      !------------------------------------------------------------------------
      do iBlock = 1, nBlock
         if (Unused_B(iBlock)) CYCLE
         iNode = iNode_B(iBlock)
         select case (iEffectedNode_A(iNode))
         case (unset_)
            if (any(abs(State_VGB(:, 1:nI, 1:nJ, 1:nK, iBlock) &
                 - Xyz_DGB(1:nDim, 1:nI, 1:nJ, 1:nK, iBlock)) > 1e-6)) then
               write(*,*) NameSub, ' error for iProc,iBlock,maxloc=', &
                    iProc, iBlock, &
                    maxloc(abs(State_VGB(:, 1:nI, 1:nJ, 1:nK, iBlock) &
                    - Xyz_DGB(1:nDim, 1:nI, 1:nJ, 1:nK, iBlock)))
            end if

            iDim = iDimAmr_D(1)
            if (abs(Dt_B(iBlock) - CellSize_DB(iDim, iBlock)) > 1e-6) &
                 write(*,*) NameSub, ' error for iProc,iBlock,dt,dx=', &
                 iProc, iBlock, Dt_B(iBlock), CellSize_DB(iDim, iBlock)
         case (0) ! the masked block

            if (iRatio == 2) then
               iDn = nI - iRatio + 1
               iUp = nI
               Di = iRatio
            else
               iDn = (MinI + MaxI)/2
               iUp = iDn
               Di = 0
            end if

            if (jRatio == 2) then
               jDn = nJ - jRatio + 1
               jUp = nJ
               Dj = jRatio
            else
               jDn = (MinJ + MaxJ)/2
               jUp = jDn
               Dj = 0
            end if

            if (kRatio == 2) then
               kDn = nK - kRatio + 1
               kUp = nK
               Dk = kRatio
            else
               kDn = (MinK + MaxK)/2
               kUp = kDn
               Dk = 0
            end if

            TestState_VC = Xyz_DGB(1:nDim, 1:nI, 1:nJ, 1:nK, iBlock)

            ! the average of the masked Cell
            do iVar = 1, nVar
               TestState_VC(iVar, iDn:iUp, jDn:jUp, kDn:kUp) = &
                    sum(Xyz_DGB(iVar, iDn:iUp, jDn:jUp, kDn:kUp, iBlock))/ &
                    (iRatio*jRatio*kRatio)
            end do

            ! The neighbor cell in i direction will only have
            ! 1st order prolongation
            TestState_VC(1,iDn - Di:iUp - Di,jDn:jUp,kDn:kUp) = &
                 sum(Xyz_DGB(1,iDn - Di:iUp - Di,jDn:jUp,kDn:kUp,iBlock))/ &
                 (iRatio*jRatio*kRatio)

            ! The neighbor cell in j direction will only have
            ! 1st order prolongation
            if (nDim > 1) then
               TestState_VC(2,iDn:iUp,jDn - Dj:jUp - Dj,kDn:kUp) = &
                    sum(Xyz_DGB(2,iDn:iUp,jDn - Dj:jUp - Dj,kDn:kUp,iBlock))/ &
                    (iRatio*jRatio*kRatio)
            end if

            ! The neighbor cell in k direction will only have
            ! 1st order prolongation
            if (nDim > 2) then
               TestState_VC(3,iDn:iUp,jDn:jUp,kDn - Dk:kUp - Dk) = &
                    sum(Xyz_DGB(3,iDn:iUp,jDn:jUp,kDn - Dk:kUp - Dk,iBlock))/ &
                    (iRatio*jRatio*kRatio)
            end if

            if (any(abs(State_VGB(:,1:nI,1:nJ,1:nK,iBlock) &
                 - TestState_VC) > 1e-6)) then
               write(*,*) NameSub, ' case 0 error for iProc,iBlock,maxloc=', &
                    iProc, iBlock, maxloc( &
                    abs(State_VGB(:,1:nI,1:nJ,1:nK,iBlock) - TestState_VC))
            end if

            iDim = iDimAmr_D(1)
            if (abs(Dt_B(iBlock) - CellSize_DB(iDim,iBlock)) > 1e-6) &
                 write(*,*) NameSub, ' error for iProc,iBlock,dt,dx=', &
                 iProc, iBlock, Dt_B(iBlock), CellSize_DB(iDim,iBlock)

         case (1) ! i neigbor of mask block

            if (iRatio == 2) then
               iDn = 1
               iUp = iRatio
               Di = iRatio
            else
               iDn = (MinI + MaxI)/2
               iUp = iDn
               Di = 0
            end if

            if (jRatio == 2) then
               jDn = nJ - jRatio + 1
               jUp = nJ
               Dj = jRatio
            else
               jDn = (MinJ + MaxJ)/2
               jUp = jDn
               Dj = 0
            end if

            if (kRatio == 2) then
               kDn = nK - kRatio + 1
               kUp = nK
               Dk = kRatio
            else
               kDn = (MinK + MaxK)/2
               kUp = kDn
               Dk = 0
            end if

            TestState_VC = Xyz_DGB(1:nDim,1:nI,1:nJ,1:nK,iBlock)

            ! The neighbor cell in i direction will only have
            ! 1st order prolongation
            TestState_VC(1,iDn:iUp,jDn:jUp,kDn:kUp) = &
                 sum(Xyz_DGB(1,iDn:iUp,jDn:jUp,kDn:kUp,iBlock))/ &
                 (iRatio*jRatio*kRatio)

            if (any(abs(State_VGB(:,1:nI,1:nJ,1:nK,iBlock) &
                 - TestState_VC) > 1e-6)) then
               write(*,*) NameSub, ' case 1 error for iProc,iBlock,maxloc=', &
                    iProc, iBlock, maxloc( &
                    abs(State_VGB(:,1:nI,1:nJ,1:nK,iBlock) - TestState_VC))
            end if

            iDim = iDimAmr_D(1)
            if (abs(Dt_B(iBlock) - CellSize_DB(iDim,iBlock)) > 1e-6) &
                 write(*,*) NameSub, ' error for iProc,iBlock,dt,dx=', &
                 iProc, iBlock, Dt_B(iBlock), CellSize_DB(iDim,iBlock)

         case (2) ! j neighbore of masked block

            if (jRatio == 2) then
               jDn = 1
               jUp = jRatio
               Dj = jRatio
            else
               jDn = (MinJ + MaxJ)/2
               jUp = jDn
               Dj = 0
            end if

            if (iRatio == 2) then
               iDn = nI - iRatio + 1
               iUp = nI
               Di = iRatio
            else
               iDn = (MinI + MaxI)/2
               iUp = iDn
               Di = 0
            end if

            if (kRatio == 2) then
               kDn = nK - kRatio + 1
               kUp = nK
               Dk = kRatio
            else
               kDn = (MinK + MaxK)/2
               kUp = kDn
               Dk = 0
            end if

            TestState_VC = Xyz_DGB(1:nDim,1:nI,1:nJ,1:nK,iBlock)

            ! The neighbor cell in j direction will only have
            ! 1st order prolongation
            TestState_VC(2,iDn:iUp,jDn:jUp,kDn:kUp) = &
                 sum(Xyz_DGB(2,iDn:iUp,jDn:jUp,kDn:kUp,iBlock))/ &
                 (iRatio*jRatio*kRatio)

            if (any(abs(State_VGB(:,1:nI,1:nJ,1:nK,iBlock) &
                 - TestState_VC) > 1e-6)) then
               write(*,*) NameSub, ' case 2 error for iProc,iBlock,maxloc=', &
                    iProc, iBlock, maxloc( &
                    abs(State_VGB(:,1:nI,1:nJ,1:nK,iBlock) - TestState_VC))
            end if

            iDim = iDimAmr_D(1)
            if (abs(Dt_B(iBlock) - CellSize_DB(iDim,iBlock)) > 1e-6) &
                 write(*,*) NameSub, ' error for iProc,iBlock,dt,dx=', &
                 iProc, iBlock, Dt_B(iBlock), CellSize_DB(iDim,iBlock)

         case (3) ! k neighbore of masked block

            if (kRatio == 2) then
               kDn = 1
               kUp = kRatio
               Dk = kRatio
            else
               kDn = (MinK + MaxK)/2
               kUp = kDn
               Dk = 0
            end if

            if (iRatio == 2) then
               iDn = nI - iRatio + 1
               iUp = nI
               Di = iRatio
            else
               iDn = (MinI + MaxI)/2
               iUp = iDn
               Di = 0
            end if

            if (jRatio == 2) then
               jDn = nJ - jRatio + 1
               jUp = nJ
               Dj = jRatio
            else
               jDn = (MinJ + MaxJ)/2
               jUp = jDn
               Dj = 0
            end if

            TestState_VC = Xyz_DGB(1:nDim,1:nI,1:nJ,1:nK,iBlock)

            ! The neighbor cell in k direction will only have
            ! 1st order prolongation
            TestState_VC(3,iDn:iUp,jDn:jUp,kDn:kUp) = &
                 sum(Xyz_DGB(3,iDn:iUp,jDn:jUp,kDn:kUp,iBlock))/ &
                 (iRatio*jRatio*kRatio)

            if (any(abs(State_VGB(:,1:nI,1:nJ,1:nK,iBlock) &
                 - TestState_VC) > 1e-6)) then
               write(*,*) NameSub, ' case 3 error for iProc,iBlock,maxloc=', &
                    iProc, iBlock, maxloc( &
                    abs(State_VGB(:,1:nI,1:nJ,1:nK,iBlock) - TestState_VC))
            end if

            iDim = iDimAmr_D(1)
            if (abs(Dt_B(iBlock) - CellSize_DB(iDim,iBlock)) > 1e-6) &
                 write(*,*) NameSub, ' error for iProc,iBlock,dt,dx=', &
                 iProc, iBlock, Dt_B(iBlock), CellSize_DB(iDim,iBlock)

         end select
      end do

    end subroutine check_state_mask
    !==========================================================================
    subroutine show_state

      integer :: iProcShow
      !------------------------------------------------------------------------
      call barrier_mpi

      do iProcShow = 0, nProc - 1
         if (iProc == iProcShow) then
            do iBlock = 1, nBlock
               if (Unused_B(iBlock)) CYCLE
               write (*, '(a,2i4,100f8.4)') &
                    'iProc, iBlock, State(1,1:nI,1,1,iBlock)=', &
                    iProc, iBlock, State_VGB(1, 1:nI, 1, 1, iBlock)

               if (nDim > 1) write (*, '(a,2i4,100f8.3)') &
                    'iProc, iBlock, State(2,1,1:nJ,1,iBlock)=', &
                    iProc, iBlock, State_VGB(2, 1, 1:nJ, 1, iBlock)

               if (nDim > 2) write (*, '(a,2i4,100f8.2)') &
                    'iProc, iBlock, State(3,1,1,1:nK,iBlock)=', &
                    iProc, iBlock, State_VGB(3, 1, 1, 1:nK, iBlock)
            end do
         end if
         call barrier_mpi
      end do

    end subroutine show_state
    !==========================================================================
    subroutine show_dt

      integer:: iProcShow
      !------------------------------------------------------------------------
      call barrier_mpi

      do iProcShow = 0, nProc - 1
         if (iProc == iProcShow) then
            do iBlock = 1, nBlock
               if (Unused_B(iBlock)) CYCLE
               write(*,*) 'iProc, iBlock, Dx, Dt =', &
                    iProc, iBlock, CellSize_DB(1, iBlock), Dt_B(iBlock)
            end do
         end if
         call barrier_mpi
      end do

    end subroutine show_dt
    !==========================================================================
  end subroutine test_amr
  !============================================================================
  subroutine test_pack(iBlock, nBuffer, Buffer_I)

    integer, intent(in) :: iBlock
    integer, intent(in) :: nBuffer
    real, intent(out):: Buffer_I(nBuffer)
    !--------------------------------------------------------------------------
    if (nBuffer /= nExtraData) write(*,*) 'ERROR in test_pack: ', &
         'nBuffer, nExtraData=', nBuffer, nExtraData

    Buffer_I = ExtraData_IB(:, iBlock)

  end subroutine test_pack
  !============================================================================
  subroutine test_unpack(iBlock, nBuffer, Buffer_I)

    integer, intent(in):: iBlock
    integer, intent(in):: nBuffer
    real, intent(in):: Buffer_I(nBuffer)
    !--------------------------------------------------------------------------
    if (nBuffer /= nExtraData) write(*,*) 'ERROR in test_unpack: ', &
         'nBuffer, nExtraData=', nBuffer, nExtraData

    ExtraData_IB(:, iBlock) = Buffer_I

  end subroutine test_unpack
  !============================================================================
end module BATL_unit_test
!==============================================================================
