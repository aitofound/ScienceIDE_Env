!  Copyright (C) 2002 Regents of the University of Michigan, 
!  portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module PIC_ModBatlInterface
  use PIC_ModMain,  ONLY: MaxBlock
   use PC_ModSize, ONLY: nDim
  implicit none
  integer, allocatable, dimension(:,:) :: NeiLevel_SB, NeiPe_SB, NeiBlk_SB
contains
  !===========================================================================
  subroutine set_altor_grid

    use PC_BATL_lib, ONLY: Unused_B, nProc, iComm, nRoot_D
    use PC_ModSize, ONLY: nBlock
    use PIC_ModMain, ONLY: nBlockMax, nTotBlocks
    use ModMpi

    integer:: iBlock, iError
    character(len=*), parameter:: NameSub = 'set_altor_grid'
    !-------------------------------------------------------------------------
    if(.not.allocated(NeiLevel_SB)) allocate( &
         NeiLevel_SB(2*nDim,MaxBlock), &
         NeiPe_SB(2*nDim,MaxBlock), &
         NeiBlk_SB(2*nDim,MaxBlock))

    if(nProc/=1)then
       call MPI_allreduce(nBlock, nBlockMax, 1, MPI_INTEGER, MPI_MAX, &
            iComm, iError)
    else
       nBlockMax = nBlock
    end if
    nTotBlocks = product(nRoot_D(1:nDim))
    do iBlock = 1, nBlock
       if(Unused_B(iBlock)) CYCLE
       call set_altor_block(iBlock)
    end do
  end subroutine set_altor_grid
  !===========================================================================
  subroutine set_altor_block(iBlock)
    use PC_ModSize, ONLY: nDim
    use PC_BATL_lib, ONLY: Block_, Proc_, Unset_, &
         iNodeNei_IIIB, DiLevelNei_IIIB, iTree_IA 

    integer, intent(in):: iBlock

    ! Convert from BATL to BATSRUS ordering of subfaces. 

    integer:: iNodeNei
    !---------------------------------------------------

    NeiLevel_SB( 1,iBlock)  = DiLevelNei_IIIB(-1,0,0,iBlock)
    select case(DiLevelNei_IIIB(-1,0,0,iBlock))
    case(Unset_)
       NeiBlk_SB(1,iBlock)  = Unset_
       NeiPe_SB( 1,iBlock)  = Unset_
    case default
       iNodeNei = iNodeNei_IIIB(0,1,1,iBlock)
       NeiBlk_SB(1,iBlock)  = iTree_IA(Block_,iNodeNei)
       NeiPe_SB( 1,iBlock)  = iTree_IA(Proc_ ,iNodeNei)
    end select

    NeiLevel_SB( 2,iBlock)  = DiLevelNei_IIIB(+1,0,0,iBlock)
    select case(DiLevelNei_IIIB(+1,0,0,iBlock))
    case(Unset_)
       NeiBlk_SB(2,iBlock)  = Unset_
       NeiPe_SB( 2,iBlock)  = Unset_
    case default
       iNodeNei = iNodeNei_IIIB(3,1,1,iBlock)
       NeiBlk_SB(2,iBlock)  = iTree_IA(Block_,iNodeNei)
       NeiPe_SB( 2,iBlock)  = iTree_IA(Proc_ ,iNodeNei)
    end select

    NeiLevel_SB( 3,iBlock)  = DiLevelNei_IIIB(0,-1,0,iBlock)
    select case(DiLevelNei_IIIB(0,-1,0,iBlock))
    case(Unset_)
       NeiBlk_SB(3,iBlock)  = Unset_
       NeiPe_SB( 3,iBlock)  = Unset_
    case default
       iNodeNei = iNodeNei_IIIB(1,0,1,iBlock)
       NeiBlk_SB(3,iBlock)  = iTree_IA(Block_,iNodeNei)
       NeiPe_SB( 3,iBlock)  = iTree_IA(Proc_ ,iNodeNei)
    end select

    NeiLevel_SB( 4,iBlock)  = DiLevelNei_IIIB(0,+1,0,iBlock)
    select case(DiLevelNei_IIIB(0,+1,0,iBlock))
    case(Unset_)
       NeiBlk_SB(4,iBlock)  = Unset_
       NeiPe_SB( 4,iBlock)  = Unset_
    case default
       iNodeNei = iNodeNei_IIIB(1,3,1,iBlock)
       NeiBlk_SB(4,iBlock)  = iTree_IA(Block_,iNodeNei)
       NeiPe_SB( 4,iBlock)  = iTree_IA(Proc_ ,iNodeNei)
    end select
    if(nDim==2)RETURN

    NeiLevel_SB( 2*nDim-1,iBlock)  = DiLevelNei_IIIB(0,0,-1,iBlock)
    select case(DiLevelNei_IIIB(0,0,-1,iBlock))
    case(Unset_ )
       NeiBlk_SB(2*nDim-1,iBlock)  = Unset_
       NeiPe_SB( 2*nDim-1,iBlock)  = Unset_
    case default
       iNodeNei = iNodeNei_IIIB(1,1,0,iBlock)
       NeiBlk_SB(2*nDim-1,iBlock)  = iTree_IA(Block_,iNodeNei)
       NeiPe_SB( 2*nDim-1,iBlock)  = iTree_IA(Proc_ ,iNodeNei)
    end select

    NeiLevel_SB( 2*nDim,iBlock)  = DiLevelNei_IIIB(0,0,+1,iBlock)
    select case(DiLevelNei_IIIB(0,0,+1,iBlock))
    case(Unset_)
       NeiBlk_SB(2*nDim,iBlock)  = Unset_
       NeiPe_SB( 2*nDim,iBlock)  = Unset_
    case default
       iNodeNei = iNodeNei_IIIB(1,1,3,iBlock)
       NeiBlk_SB(2*nDim,iBlock)  = iTree_IA(Block_,iNodeNei)
       NeiPe_SB( 2*nDim,iBlock)  = iTree_IA(Proc_ ,iNodeNei)
    end select
  end subroutine set_altor_block
  !=============================
end module PIC_ModBatlInterface
