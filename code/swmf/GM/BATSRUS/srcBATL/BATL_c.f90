!  Copyright (C) 2024 Regents of the University of Michigan
!  C-compatible wrapper for BATL for Julia integration

module BATL_c
  use iso_c_binding
  use BATL_lib
  ! Additional modules for internal functions not in BATL_lib
  use BATL_tree, ONLY: init_tree, set_tree_root, refine_tree_node, &
                       coarsen_tree_node, distribute_tree, &
                       find_tree_node, find_tree_cell, interpolate_tree, &
                       i_node_new, is_point_inside_node, show_tree
  use BATL_geometry, ONLY: init_geometry, xyz_to_coord, coord_to_xyz

  implicit none

contains
  !============================================================================
  function c_batl_get_ni() bind(c) result(iRes)
    integer(c_int) :: iRes
    !--------------------------------------------------------------------------
    iRes = int(nI, c_int)
  end function c_batl_get_ni
  !============================================================================
  function c_batl_get_nj() bind(c) result(iRes)
    integer(c_int) :: iRes
    !--------------------------------------------------------------------------
    iRes = int(nJ, c_int)
  end function c_batl_get_nj
  !============================================================================
  function c_batl_get_nk() bind(c) result(iRes)
    integer(c_int) :: iRes
    !--------------------------------------------------------------------------
    iRes = int(nK, c_int)
  end function c_batl_get_nk
  !============================================================================
  function c_batl_get_ng() bind(c) result(iRes)
    integer(c_int) :: iRes
    !--------------------------------------------------------------------------
    iRes = int(nG, c_int)
  end function c_batl_get_ng
  !============================================================================
  function c_batl_get_ndim() bind(c) result(iRes)
    integer(c_int) :: iRes
    !--------------------------------------------------------------------------
    iRes = int(nDim, c_int)
  end function c_batl_get_ndim
  !============================================================================
  function c_batl_get_maxblock() bind(c) result(iRes)
    integer(c_int) :: iRes
    !--------------------------------------------------------------------------
    iRes = int(MaxBlock, c_int)
  end function c_batl_get_maxblock
  !============================================================================
  function c_batl_is_cartesian() bind(c) result(iRes)
    integer(c_int) :: iRes
    !--------------------------------------------------------------------------
    if (IsCartesian) then
       iRes = 1
    else
       iRes = 0
    end if
  end function c_batl_is_cartesian
  !============================================================================
  function c_batl_is_rz_geometry() bind(c) result(iRes)
    integer(c_int) :: iRes
    !--------------------------------------------------------------------------
    if (IsRzGeometry) then
       iRes = 1
    else
       iRes = 0
    end if
  end function c_batl_is_rz_geometry
  !============================================================================
  function c_batl_get_nroot() bind(c) result(iRes)
    integer(c_int) :: iRes
    !--------------------------------------------------------------------------
    iRes = int(nRoot, c_int)
  end function c_batl_get_nroot
  !============================================================================
  function c_batl_get_nnode() bind(c) result(iRes)
    integer(c_int) :: iRes
    !--------------------------------------------------------------------------
    iRes = int(nNode, c_int)
  end function c_batl_get_nnode
  !============================================================================
  subroutine c_init_mpi() bind(c)
    !--------------------------------------------------------------------------
    call init_mpi()
  end subroutine c_init_mpi
  !============================================================================
  subroutine c_init_batl( &
      CoordMinIn_D, CoordMaxIn_D, MaxBlockIn) bind(c)
    real(c_double), intent(in) :: CoordMinIn_D(nDim)
    real(c_double), intent(in) :: CoordMaxIn_D(nDim)
    integer(c_int), intent(in), value :: MaxBlockIn
    !--------------------------------------------------------------------------
    call init_batl(real(CoordMinIn_D), real(CoordMaxIn_D), &
         int(MaxBlockIn))
  end subroutine c_init_batl
  !============================================================================
  subroutine c_clean_batl() bind(c)
    !--------------------------------------------------------------------------
    call clean_batl()
  end subroutine c_clean_batl
  !============================================================================
  subroutine c_init_grid_batl() bind(c)
    !--------------------------------------------------------------------------
    call init_grid_batl()
  end subroutine c_init_grid_batl
  !============================================================================
  subroutine c_regrid_batl(nVar, State_VGB) bind(c)
    integer(c_int), intent(in), value :: nVar
    real(c_double), intent(inout) :: &
      State_VGB(nVar, MinI:MaxI, MinJ:MaxJ, MinK:MaxK, MaxBlock)
    !--------------------------------------------------------------------------
    call regrid_batl(int(nVar), State_VGB)
  end subroutine c_regrid_batl
  !============================================================================
  subroutine c_init_tree(MaxBlockIn) bind(c)
    integer(c_int), intent(in), value :: MaxBlockIn
    !--------------------------------------------------------------------------
    call init_tree(int(MaxBlockIn))
  end subroutine c_init_tree
  !============================================================================
  subroutine c_set_tree_root(nRootIn_D) bind(c)
    integer(c_int), intent(in) :: nRootIn_D(nDim)
    integer :: nRoot_A(MaxDim)
    !--------------------------------------------------------------------------
    nRoot_A = 1
    nRoot_A(1:nDim) = int(nRootIn_D)
    call set_tree_root(nRoot_A)
  end subroutine c_set_tree_root
  !============================================================================
  function c_i_node_new() bind(c) result(iRes)
    integer(c_int) :: iRes
    !--------------------------------------------------------------------------
    iRes = int(i_node_new(), c_int)
  end function c_i_node_new
  !============================================================================
  subroutine c_refine_tree_node(iNode) bind(c)
    integer(c_int), intent(in), value :: iNode
    !--------------------------------------------------------------------------
    call refine_tree_node(int(iNode))
  end subroutine c_refine_tree_node
  !============================================================================
  subroutine c_coarsen_tree_node(iNode) bind(c)
    integer(c_int), intent(in), value :: iNode
    !--------------------------------------------------------------------------
    call coarsen_tree_node(int(iNode))
  end subroutine c_coarsen_tree_node
  !============================================================================
  subroutine c_distribute_tree(iDoBalanceOnlyIn) bind(c)
    integer(c_int), intent(in), value :: iDoBalanceOnlyIn
    !--------------------------------------------------------------------------
    call distribute_tree(iDoBalanceOnlyIn /= 0)
  end subroutine c_distribute_tree
  !============================================================================
  subroutine c_find_tree_node(Coord_D, iNode) bind(c)
    real(c_double), intent(in) :: Coord_D(nDim)
    integer(c_int), intent(out) :: iNode
    integer :: iNodeF
    real :: Coord_A(MaxDim)
    !--------------------------------------------------------------------------
    Coord_A = 0.0
    Coord_A(1:nDim) = real(Coord_D)
    call find_tree_node(Coord_A, iNodeF)
    iNode = int(iNodeF, c_int)
  end subroutine c_find_tree_node
  !============================================================================
  function c_is_point_inside_node(Coord_D, iNode) bind(c) result(iRes)
    real(c_double), intent(in) :: Coord_D(nDim)
    integer(c_int), intent(in), value :: iNode
    integer(c_int) :: iRes
    real :: Coord_A(MaxDim)
    !--------------------------------------------------------------------------
    Coord_A = 0.0
    Coord_A(1:nDim) = real(Coord_D)
    if (is_point_inside_node(Coord_A, int(iNode))) then
       iRes = 1
    else
       iRes = 0
    end if
  end function c_is_point_inside_node
  !============================================================================
  subroutine c_show_tree(StringIn_A) bind(c)
    character(kind=c_char), intent(in) :: StringIn_A(*)
    character(len=100) :: StringF
    integer :: i
    !--------------------------------------------------------------------------
    i = 1
    StringF = ''
    do while (StringIn_A(i) /= c_null_char)
       StringF(i:i) = StringIn_A(i)
       i = i + 1
    end do
    if (i > 1) then
       call show_tree(trim(StringF))
    else
       call show_tree('')
    end if
  end subroutine c_show_tree
  !============================================================================
  subroutine c_init_geometry(TypeGeometryIn_A) bind(c)
    character(kind=c_char), intent(in) :: TypeGeometryIn_A(*)
    character(len=100) :: TypeF
    integer :: i
    !--------------------------------------------------------------------------
    i = 1
    TypeF = ''
    do while (TypeGeometryIn_A(i) /= c_null_char)
       TypeF(i:i) = TypeGeometryIn_A(i)
       i = i + 1
    end do
    call init_geometry(TypeGeometryIn=trim(TypeF))
  end subroutine c_init_geometry
  !============================================================================
  subroutine c_xyz_to_coord(Xyz_D, Coord_D) bind(c)
    real(c_double), intent(in)  :: Xyz_D(nDim)
    real(c_double), intent(out) :: Coord_D(nDim)
    real :: Xyz_A(MaxDim), Coord_A(MaxDim)
    !--------------------------------------------------------------------------
    Xyz_A = 0.0
    Xyz_A(1:nDim) = real(Xyz_D)
    call xyz_to_coord(Xyz_A, Coord_A)
    Coord_D = real(Coord_A(1:nDim), c_double)
  end subroutine c_xyz_to_coord
  !============================================================================
  subroutine c_coord_to_xyz(Coord_D, Xyz_D) bind(c)
    real(c_double), intent(in)  :: Coord_D(nDim)
    real(c_double), intent(out) :: Xyz_D(nDim)
    real :: Coord_A(MaxDim), Xyz_A(MaxDim)
    !--------------------------------------------------------------------------
    Coord_A = 0.0
    Coord_A(1:nDim) = real(Coord_D)
    call coord_to_xyz(Coord_A, Xyz_A)
    Xyz_D = real(Xyz_A(1:nDim), c_double)
  end subroutine c_coord_to_xyz
  !============================================================================
  subroutine c_test_tree() bind(c)
    use BATL_unit_test, ONLY: test_tree
    !--------------------------------------------------------------------------
    call test_tree()
  end subroutine c_test_tree
  !============================================================================
  subroutine c_test_geometry() bind(c)
    use BATL_unit_test, ONLY: test_geometry
    !--------------------------------------------------------------------------
    call test_geometry()
  end subroutine c_test_geometry
  !============================================================================
  function c_batl_get_xyz_ptr() bind(c) result(Res)
    type(c_ptr) :: Res
    !--------------------------------------------------------------------------
    Res = c_loc(Xyz_DGB)
  end function c_batl_get_xyz_ptr
  !============================================================================
  function c_batl_get_cellvolume_ptr() bind(c) result(Res)
    type(c_ptr) :: Res
    !--------------------------------------------------------------------------
    Res = c_loc(CellVolume_GB)
  end function c_batl_get_cellvolume_ptr
  !============================================================================
  function c_batl_get_cellsize_ptr() bind(c) result(Res)
    type(c_ptr) :: Res
    !--------------------------------------------------------------------------
    Res = c_loc(CellSize_DB)
  end function c_batl_get_cellsize_ptr
  !============================================================================
  function c_batl_get_xyz_nb_ptr() bind(c) result(Res)
    type(c_ptr) :: Res
    !--------------------------------------------------------------------------
    Res = c_loc(Xyz_DNB)
  end function c_batl_get_xyz_nb_ptr
  !============================================================================
  function c_batl_get_nblock() bind(c) result(iRes)
    integer(c_int) :: iRes
    !--------------------------------------------------------------------------
    iRes = int(nBlock, c_int)
  end function c_batl_get_nblock
  !============================================================================
  function c_batl_is_block_used(iBlock) bind(c) result(iRes)
    integer(c_int), intent(in), value :: iBlock
    integer(c_int) :: iRes
    !--------------------------------------------------------------------------
    if (Unused_B(iBlock)) then
       iRes = 0
    else
       iRes = 1
    end if
  end function c_batl_is_block_used
  !============================================================================
  function c_batl_get_block_node(iBlock) bind(c) result(iRes)
    integer(c_int), intent(in), value :: iBlock
    integer(c_int) :: iRes
    !--------------------------------------------------------------------------
    iRes = int(iNode_B(iBlock), c_int)
  end function c_batl_get_block_node
  !============================================================================
  subroutine c_message_pass_cell(nVar, State_VGB) bind(c)
    integer(c_int), intent(in), value :: nVar
    real(c_double), intent(inout) :: &
      State_VGB(nVar, MinI:MaxI, MinJ:MaxJ, MinK:MaxK, MaxBlock)
    !--------------------------------------------------------------------------
    call message_pass_cell(int(nVar), State_VGB)
  end subroutine c_message_pass_cell
  !============================================================================
  subroutine c_barrier_mpi() bind(c)
    !--------------------------------------------------------------------------
    call barrier_mpi()
  end subroutine c_barrier_mpi
  !============================================================================
  subroutine c_regrid_batl_full( &
      nVar, State_VGB, iDoBalanceEachLevel) bind(c)
    integer(c_int), intent(in), value :: nVar
    real(c_double), intent(inout) :: &
      State_VGB(nVar, MinI:MaxI, MinJ:MaxJ, MinK:MaxK, MaxBlock)
    integer(c_int), intent(in), value :: iDoBalanceEachLevel
    !--------------------------------------------------------------------------
    call regrid_batl(int(nVar), State_VGB, &
         DoBalanceEachLevelIn=(iDoBalanceEachLevel /= 0))
  end subroutine c_regrid_batl_full
  !============================================================================
  subroutine c_adapt_tree() bind(c)
    !--------------------------------------------------------------------------
    call adapt_tree()
  end subroutine c_adapt_tree
  !============================================================================
  subroutine c_distribute_tree_full(iDoMove) bind(c)
    integer(c_int), intent(in), value :: iDoMove
    !--------------------------------------------------------------------------
    call distribute_tree(iDoMove /= 0)
  end subroutine c_distribute_tree_full
  !============================================================================
  subroutine c_create_grid() bind(c)
    !--------------------------------------------------------------------------
    call create_grid()
  end subroutine c_create_grid
  !============================================================================
  subroutine c_find_tree_cell(Coord_D, iBlock, iResCell_D) bind(c)
    use BATL_tree, ONLY: find_tree_cell, iTree_IA, Block_, Unset_
    real(c_double), intent(in) :: Coord_D(nDim)
    integer(c_int), intent(out) :: iBlock
    integer(c_int), intent(out) :: iResCell_D(nDim)

    real :: Coord_A(MaxDim)
    integer :: iNode
    integer :: iCell_D(MaxDim)
    !--------------------------------------------------------------------------
    Coord_A = 0.0
    Coord_A(1:nDim) = real(Coord_D)

    call find_tree_cell(Coord_A, iNode, iCell_D)

    if (iNode /= Unset_) then
       iBlock = int(iTree_IA(Block_, iNode), c_int)
    else
       iBlock = int(Unset_, c_int)
    end if
    iResCell_D(1:nDim) = int(iCell_D(1:nDim), c_int)
  end subroutine c_find_tree_cell
  !============================================================================
end module BATL_c
!==============================================================================
