!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module BATL_pass_cell_gpu_parallel

  use BATL_test, ONLY: test_start, test_stop, iTest, jTest, kTest, &
       iBlockTest
  use BATL_geometry, ONLY: &
       IsCylindricalAxis, IsSphericalAxis, IsLatitudeAxis, Lat_, Theta_
  use ModNumConst, ONLY: cPi, cHalfPi
  use BATL_size, ONLY: MaxDim, nDim
  use ModUtilities, ONLY: CON_stop, lower_case
  use ModMpi

  ! Possible improvements:
  ! (1) Instead of sending the receiving block number
  !     and the 2*nDim range limits, we can send only the tag which
  !     we would use in a block to block communication:
  !        iTag = 100*iBlockRecv + iRecv + 4*(jRecv + 4*kRecv)
  !     There are 2 advantages:
  !     (a) The amount of info reduces: 1+2*nDim numbers --> 1 number (iTag)
  !     (b) This procedure allows to send for 1st order prolongation only
  !         one copy per 2**nDim cells
  ! (2) Instead of waiting for receiving buffers from ALL processors, we
  !     we can wait for ANY receiving and already start unpacking
  ! (3) Instead of determining the receive (and send) buffer size during
  !     the message_pass_cell, we can determine the sizes a priori:
  !     (a) We can then allocate a small known buffer size
  !     (b) we do at least two times message_pass_cell per time iteration,
  !         each time determining the buffer size. This would be reduced to
  !         only once (there is a small complication with operator split
  !         schemes)

  implicit none

  SAVE

  private ! except

  public:: message_pass_real_gpu

  ! local variables corresponding to optional arguments
  logical :: UseMin, UseMax  ! logicals for min and max operators
  !$acc declare create(UseMin, UseMax)

  ! local variables corresponding to optional arguments
  integer :: nWidth
  !$acc declare create(nWidth)
  integer :: nCoarseLayer
  !$acc declare create(nCoarseLayer)
  integer :: nProlongOrder
  !$acc declare create(nProlongOrder)
  logical :: DoSendCorner
  logical :: DoRestrictFace
  !$acc declare create(DoRestrictFace)
  logical :: DoResChangeOnly
  character(len=3) :: NameOperator
  !$acc declare create(DoSendCorner, DoResChangeOnly)

  ! number of indexes sent with each message: iBlock and 2*nDim index limits
  integer, parameter:: nIndex = 1 + 2*nDim

  ! Fast lookup tables for index ranges per dimension
  integer, parameter:: Min_=1, Max_=2
  integer :: iRestrictS_DII(MaxDim,-1:1,Min_:Max_)
  integer :: iRestrictR_DII(MaxDim,0:3,Min_:Max_)
  !$acc declare create(iRestrictS_DII, iRestrictR_DII)
  integer :: iProlongS_DII(MaxDim,0:3,Min_:Max_)
  integer :: iProlongR_DII(MaxDim,0:3,Min_:Max_)
  !$acc declare create(iProlongS_DII, iProlongR_DII)
  integer :: iEqualS_DII(MaxDim,-1:1,Min_:Max_)
  integer :: iEqualR_DII(MaxDim,-1:1,Min_:Max_)

  ! These two arrays do not have to be private for
  ! 2nd and 1st order prolongation schemes.
  !$acc declare create(iEqualS_DII, iEqualR_DII)

  integer :: iRequestR, iRequestS, iError
  integer, allocatable:: iRequestR_I(:), iRequestS_I(:)
  integer, allocatable:: iRequestRMap_I(:), iRequestSMap_I(:)

  ! Stage indexes
  ! Second order prolongation needs two stages:
  ! first stage fills in equal and coarser ghost cells
  ! second stage uses these to prolong and fill in finer ghost cells
  integer, parameter:: MaxStage=2
  ! indexes for multiple stages
  integer :: iSendStage
  !$acc declare create(iSendStage)

  ! number of messages sent to another processor
  integer, allocatable :: nMsgSend_PI(:,:) ! iprocessor, istage
  integer, allocatable :: nMsgRecv_PI(:,:)
  ! number of messages sent to another processor from a given block
  integer, allocatable :: nMsgSend_PBI(:,:,:)
  ! starting index of msgs from block B to processor P
  integer, allocatable :: iMsgInit_PBI(:,:,:) ! block, proc, istage
  !$acc declare create(nMsgSend_PI, nMsgRecv_PI, nMsgSend_PBI, iMsgInit_PBI)

  ! Parallel version of iBuffer
  integer, allocatable :: iBufferS_IPI(:,:,:), iBufferSTemp_IPI(:,:,:)
  integer, allocatable :: iBufferR_IPI(:,:,:)
  integer, allocatable :: nSizeBufferS_PI(:,:)
  integer, allocatable :: nSizeBufferR_PI(:,:)
  integer :: nSizeBufferS, nSizeBufferR, nSizeBuffer
  ! Buffer array capacity
  integer :: nCapBuffer = 0
  integer :: iMsgSend ! loop index for messages
  !$acc declare create(nSizeBufferS_PI, nSizeBufferR_PI, &
  !$acc iBufferS_IPI, iBufferR_IPI)

  ! structured buffer array for do_equal
  real, allocatable :: BufferS_IP(:,:)
  real, allocatable :: BufferR_IP(:,:)
  !$acc declare create(BufferS_IP, BufferR_IP)

  ! the rank of msg in direction I (0-26)
  ! from Block B to Processor P
  integer, allocatable :: iMsgDir_IBPI(:,:,:,:)
  !$acc declare create(iMsgDir_IBPI)

  ! save parameters for last call. If unchanged, skip counting
  integer :: nVarOld, nGOld, nWidthOld, nProlongOrderOld, nCoarseLayerOld

  ! initialize iDecomposition to force counting for the first time
  integer :: iDecompositionOld = -1
  logical :: DoSendCornerOld, DoRestrictFaceOld, DoResChangeOnlyOld

contains
  !============================================================================
  subroutine message_pass_real_gpu(nVar, nG, State_VGB, &
       nWidthIn, nProlongOrderIn, nCoarseLayerIn, DoSendCornerIn, &
       DoRestrictFaceIn, DoTestIn, NameOperatorIn, &
       DoResChangeOnlyIn, iDecomposition)

    use BATL_size, ONLY: MaxBlock, nBlock, nI, nJ, nK, nIjk_D, &
         nDim, jDim_, kDim_, iRatio_D
    use BATL_mpi, ONLY: iComm, nProc, iProc
    use BATL_tree, ONLY: Unused_B

    ! Arguments
    integer, intent(in) :: nVar  ! number of variables
    integer, intent(in) :: nG    ! number of ghost cells for 1..nDim
    real, intent(inout) :: State_VGB(nVar,&
         1-nG:nI+nG,1-nG*jDim_:nJ+nG*jDim_,1-nG*kDim_:nK+nG*kDim_,MaxBlock)

    ! Optional arguments
    integer, optional, intent(in) :: nWidthIn
    integer, optional, intent(in) :: nProlongOrderIn
    integer, optional, intent(in) :: nCoarseLayerIn
    logical, optional, intent(in) :: DoSendCornerIn
    logical, optional, intent(in) :: DoRestrictFaceIn
    character(len=*), optional,intent(in) :: NameOperatorIn
    logical, optional, intent(in) :: DoResChangeOnlyIn
    logical, optional, intent(in) :: DoTestIn

    ! if grid changes or load is rebalanced iDecomposition changes
    integer, optional, intent(in) :: iDecomposition

    ! Fill in the nVar variables in the ghost cells of State_VGB.
    !
    ! nWidthIn is the number of ghost cell layers to be set. Default is all.
    ! nProlongOrderIn is the order of accuracy of prolongation. Default is 2.
    ! nCoarseLayerIn is the number of coarse layers sent during first order
    !    prolongation. Default is 1, so all fine cells are equal.
    !    If it is set to 2, the 2 (or more) coarse layers are copied into
    !    the fine cell layers one by one.
    ! DoSendCornerIn determines if edges/corners are filled. Default is true.
    ! DoRestrictFaceIn determines if restriction is applied to a single layer
    !    of ghost cells instead of two layers. Default is false.
    !    Only works with first order prolongation.
    ! NameOperatorIn is used for taking the minimum or maximum of the fine
    !    cell values for the coarse ghost cell. Default is the average.
    ! DoResChangeOnlyIn determines if only ghost cells next to resolution
    !    changes are filled in. Default is false.
    ! DoTestIn determines if verbose information should be printed.

    ! Local variables

    ! true if input parameters are as in the last call
    logical :: IsCounted

    integer :: iBlockSend, iProcSend

    integer :: nSendStage

    integer :: nMsgSend = 0
    integer :: nMsgRecv = 0
    integer :: nMsg = 0
    integer :: nMsgSendCap = 0 ! dynamic array capacity
    !$acc declare create(nMsgSend, nMsgRecv, nMsg)

    logical :: DoTest
    character(len=*), parameter:: NameSub = 'message_pass_real_gpu'
    !--------------------------------------------------------------------------
    DoTest = .false.

    call test_start(NameSub,DoTest)
    if(present(DoTestIn)) DoTest = DoTestIn .or. DoTest

    call timing_start('batl_pass')

    call timing_start('init_pass')

    ! Set values or defaults for optional arguments
    nWidth = nG
    if(present(nWidthIn)) nWidth = nWidthIn

    nProlongOrder = 2
    if(present(nProlongOrderIn)) nProlongOrder = nProlongOrderIn

    nCoarseLayer = 1
    if(present(nCoarseLayerIn)) nCoarseLayer = nCoarseLayerIn

    DoSendCorner = .true.
    if(present(DoSendCornerIn)) DoSendCorner = DoSendCornerIn

    DoRestrictFace = .false.
    if(present(DoRestrictFaceIn)) DoRestrictFace = DoRestrictFaceIn

    DoResChangeOnly =.false.
    if(present(DoResChangeOnlyIn)) DoResChangeOnly = DoResChangeOnlyIn

    ! Check arguments for consistency
    if(nProlongOrder == 2 .and. DoRestrictFace) call CON_stop(NameSub// &
         ' cannot use 2nd order prolongation with face restriction')

    if(nProlongOrder == 2 .and. nCoarseLayer>1) call CON_stop(NameSub// &
         ' cannot use 2nd order prolongation nCoarseLayer > 1')

    if(nProlongOrder < 1 .or. nProlongOrder > 2) call CON_stop(NameSub// &
         ' only nProlongOrder = 1 or 2 is implemented ')

    if(nWidth < 1 .or. nWidth > nG) call CON_stop(NameSub// &
         ' nWidth do not contain the ghost cells or too many')

    if(nCoarseLayer < 1 .or.  nCoarseLayer > 2 ) call CON_stop(NameSub// &
         ' nCoarseLayer are only defined for value or 1 or 2 ')

    IsCounted = &
         (nVar==nVarOld).and.(nG==nGOld).and.(nWidth==nWidthOld).and.&
         (nProlongOrder==nProlongOrderOld) .and. &
         (nCoarseLayer==nCoarseLayerOld) .and. &
         (DoSendCorner .eqv. DoSendCornerOld) .and. &
         (DoRestrictFace .eqv. DoRestrictFaceOld) .and. &
         (DoResChangeOnly .eqv. DoResChangeOnlyOld)

    if(present(iDecomposition))then
       IsCounted = IsCounted .and. iDecomposition == iDecompositionOld
       iDecompositionOld = iDecomposition
    else
       ! If iDecomposition is not present, grid may have changed
       IsCounted = .false.
    end if

    UseMin = .false.
    UseMax = .false.

    if(present(NameOperatorIn)) then
       NameOperator = adjustl(NameOperatorIn)
       call lower_case(NameOperator)
       select case(NameOperator)
       case("min")
          UseMin=.true.
       case("max")
          UseMax=.true.
       end select
    end if

    if(DoTest)write(*,*) NameSub, &
         ': Width, Prolong, Coarse, Corner, RestrictFace, ResChangeOnly=', &
         nWidth, nProlongOrder, nCoarseLayer, DoSendCorner, &
         DoRestrictFace, DoResChangeOnly

    ! Initialize logical for time interpolation/extrapolation

    if(nProc > 1)then
       if (.not. allocated(nMsgSend_PBI)) then
          allocate(nMsgSend_PBI(0:nProc-1,nBlock,MaxStage))
          nMsgSend_PBI = 0
       else
          if(nBlock > size(nMsgSend_PBI,2))then
             if(DoTest)write(*,*)'Block number changes, allocate new arrays'
             deallocate(nMsgSend_PBI)
             allocate(nMsgSend_PBI(0:nProc-1,nBlock,MaxStage))
             nMsgSend_PBI = 0
          end if
       end if

       if (.not. allocated(iMsgInit_PBI)) then
          allocate(iMsgInit_PBI(0:nProc-1,nBlock,MaxStage))
          iMsgInit_PBI = 0
       else
          if(nBlock > size(iMsgInit_PBI,2))then
             deallocate(iMsgInit_PBI)
             allocate(iMsgInit_PBI(0:nProc-1,nBlock,MaxStage))
             iMsgInit_PBI = 0
          end if
       end if

       if (.not. allocated(nMsgSend_PI)) then
          allocate(nMsgSend_PI(0:nProc-1,MaxStage))
          nMsgSend_PI = 0
       end if

       if (.not. allocated(nMsgRecv_PI)) then
          allocate(nMsgRecv_PI(0:nProc-1,MaxStage))
          nMsgRecv_PI = 0
       end if

       if (.not. allocated(nSizeBufferS_PI))then
          allocate(nSizeBufferS_PI(0:nProc-1,MaxStage))
          nSizeBufferS_PI = 0
       end if

       if (.not. allocated(nSizeBufferR_PI))then
          allocate(nSizeBufferR_PI(0:nProc-1,MaxStage))
          nSizeBufferR_PI = 0
       end if

       if (.not. allocated(iMsgDir_IBPI))then
          ! Convert (kSend,jSend,iSend) to 0-63
          allocate(iMsgDir_IBPI(0:4**nDim-1,nBlock,0:nProc-1,MaxStage))
          iMsgDir_IBPI = -1
       else
          if(nBlock > size(iMsgDir_IBPI,2))then
             deallocate(iMsgDir_IBPI)
             allocate(&
                  iMsgDir_IBPI(0:4**nDim-1,nBlock,0:nProc-1,MaxStage))
             iMsgDir_IBPI = -1
          end if
       end if

       if(nMsgSendCap == 0) then
          nMsgSendCap = 4**nDim
          ! Allocate buffer index arrays with initial sizes
          allocate(iBufferS_IPI(nMsgSendCap,0:nProc-1,MaxStage))
          allocate(iBufferR_IPI(nMsgSendCap,0:nProc-1,MaxStage))
          iBufferR_IPI = 0
       end if
    end if

    !$acc update device( &
    !$acc DoSendCorner, DoResChangeOnly, MaxBlock,  &
    !$acc nWidth, nProlongOrder, nCoarseLayer, DoRestrictFace, &
    !$acc UseMin, UseMax)

    ! The following can run (in series) on GPU as:
    ! acc parallel num_gangs(1) num_workers(1) vector_length(1)

    ! Set index ranges based on arguments
    call set_range

    ! Since set_range runs on CPU, update the following on the device:
    !$acc update device(iEqualS_DII, iEqualR_DII, &
    !$acc iRestrictS_DII, iRestrictR_DII, iProlongS_DII, &
    !$acc iProlongR_DII)

    call timing_stop('init_pass')

    nSendStage = nProlongOrder

    call timing_start('part1_pass')

    if(nProc == 1) then

       do iSendStage = 1, nSendStage
          call timing_start('msg_pass_block')
          !$acc update device(iSendStage)

          ! Loop through all blocks that may send a message
          !$acc parallel loop gang present(State_VGB)
          do iBlockSend = 1, nBlock
             if(Unused_B(iBlockSend)) CYCLE
#ifdef TESTACC
             if(DoTest .and. iBlockSend==iBlockTest)then
                write(*,*)'On iProc=', iProc, ' iBlock=', iBlockSend, &
                     'iTest=', iTest, 'jTest=', jTest, 'kTest=', kTest
             end if
#endif
             call message_pass_block_local(iBlockSend, nVar, nG,State_VGB)
          end do ! iBlockSend

          call timing_stop('msg_pass_block')
       end do
    else
       ! nProc > 1 case requires remote message passing and counting
       do iSendStage = 1, nSendStage
          !$acc update device(iSendStage)

          call timing_start('remote_pass')

          if (.not. allocated(iRequestS_I)) allocate(iRequestS_I(nProc-1))
          if (.not. allocated(iRequestR_I)) allocate(iRequestR_I(nProc-1))
          if (.not. allocated(iRequestSMap_I)) &
               allocate(iRequestSMap_I(nProc-1))
          if (.not. allocated(iRequestRMap_I)) &
               allocate(iRequestRMap_I(nProc-1))

          ! Count only once for
          !   1. nMsgSend_PI, which is the size of memory maps
          !   2. nSizeBufferS_PI, which is the size of the sending buffers
          !   3. nSizeBufferR_PI, which is the size of the recving buffers
          !   Note: need both S and R sizes for asymmetry in buffer size
          !         introduced by AMR
          ! The size of the memory maps can change dynamically when nMsgSend
          ! changes, which mostly happens in the initial pass.
          if(.not. IsCounted) then
             nMsgSend_PI(:,iSendStage) = 0
             nMsgSend_PBI(:,:,iSendStage) = 0
             nMsgRecv_PI(:,iSendStage) = 0
             nSizeBufferS_PI(:,iSendStage) = 0
             nSizeBufferR_PI(:,iSendStage) = 0

             call timing_start('Count_1')
             do iBlockSend = 1, nBlock
                if (Unused_B(iBlockSend))then
                   ! keep the initial index continuous
                   if(iBlockSend < nBlock)&
                        iMsgInit_PBI(:,iBlockSend+1,iSendStage) = &
                        iMsgInit_PBI(:,iBlockSend,iSendStage)
                   CYCLE
                else
                   call message_count_block(iBlockSend, nVar, nG)

                   ! update nMsgSend and nSizeBuffer for each block
                   nMsgSend = maxval(nMsgSend_PI)
                   nMsgRecv = maxval(nMsgRecv_PI)
                   nMsg = max(nMsgSend,nMsgRecv)
                   nSizeBufferS = maxval(nSizeBufferS_PI)
                   nSizeBufferR = maxval(nSizeBufferR_PI)
                   nSizeBuffer = max(nSizeBufferS, nSizeBufferR)

                   ! Keeping track of the initial message index for each
                   ! block is needed for parallel construction of the send &
                   ! buffer.
                   ! minloc() gives the index of the first active block:
                   ! if(iBlockSend == minloc(merge(1,0,Unused_B),1))&
                   !     iMsgInit_PBI(:,iBlockSend,iSendStage) = 1
                   ! but we can change our convention to avoid such ugly syntax
                   ! simply let iMsgInit_PBI start with 0
                   if(iBlockSend < nBlock)&
                        iMsgInit_PBI(:,iBlockSend+1,iSendStage) =&
                        iMsgInit_PBI(:,iBlockSend,iSendStage)+&
                        nMsgSend_PBI(:,iBlockSend,iSendStage)

                   ! dynamically manage array sizes for each block counted
                   if(nMsg + 4**nDim > nMsgSendCap) then
                      call timing_start('resize_arrays')
                      ! allocate temp arrays
                      allocate( &
                           iBufferSTemp_IPI(nMsgSendCap,0:nProc-1,MaxStage))
                      ! copy old to new
                      iBufferSTemp_IPI = iBufferS_IPI
                      ! decallocate old arrays
                      deallocate(iBufferS_IPI)
                      deallocate(iBufferR_IPI)
                      ! enlarge capacity
                      nMsgSendCap = 2*nMsgSendCap
                      ! allocate larger arrays
                      allocate(iBufferS_IPI(nMsgSendCap,0:nProc-1,MaxStage))
                      allocate(iBufferR_IPI(nMsgSendCap,0:nProc-1,MaxStage))
                      ! for this copy, the exact range is 1:nMsgSend+1, as
                      ! we always compute the starting indices for the next
                      ! iBlockSend loop. However this syntax is neat.
                      iBufferS_IPI(1:nMsgSendCap/2,:,:) = iBufferSTemp_IPI
                      iBufferR_IPI = 0
                      ! update on device
                      !$acc update device(iBufferS_IPI, iBufferR_IPI)

                      ! deallocate temp array
                      deallocate(iBufferSTemp_IPI)
                      call timing_stop('resize_arrays')
                   end if
                end if ! Unused_B
             end do

             ! Restriction message size < prolongation, leading to
             ! asymmetric sending/receiving buffer sizes

             ! allocate buffers
             if(nSizeBuffer > nCapBuffer)then
                call timing_start('enlarge_buffer')
                nCapBuffer = nSizeBuffer
                if(allocated(BufferS_IP))deallocate(BufferS_IP)
                if(allocated(BufferR_IP))deallocate(BufferR_IP)
                allocate(BufferS_IP(nCapBuffer,0:nProc-1))
                allocate(BufferR_IP(nCapBuffer,0:nProc-1))
                BufferS_IP = 0.0
                BufferR_IP = 0.0
                !$acc update device(BufferS_IP, BufferR_IP)
                call timing_stop('enlarge_buffer')
             end if

             ! Do not update device iBufferR here - 2nd stage resets 1st!!
             !$acc update device(nMsgSend, nMsgRecv, nMsg, iMsgInit_PBI, &
             !$acc iBufferS_IPI, iMsgDir_IBPI, nMsgRecv_PI)
             call timing_stop('Count_1')
          end if ! IsCounted

          call timing_start('fill_buffer_gpu')
          ! Prepare the buffer for remote message passing

          ! Loop through all blocks that may send a message
          !$acc parallel loop gang present(State_VGB)
          do iBlockSend = 1, nBlock
             if(Unused_B(iBlockSend)) CYCLE
             call message_pass_block_remote(iBlockSend, nVar, nG, State_VGB)
          end do

          call timing_stop('fill_buffer_gpu')

          call timing_stop('remote_pass')

          iRequestS = 0
          !$acc host_data use_device(BufferS_IP, iBufferS_IPI)
          do iProcSend = 0, nProc-1
             if(iProcSend == iProc) CYCLE
             iRequestS = iRequestS + 1
             call MPI_isend(BufferS_IP(1,iProcSend), &
                  nSizeBufferS_PI(iProcSend,iSendStage), MPI_REAL, iProcSend, &
                  10, iComm, iRequestS_I(iRequestS), iError)
             ! if input parameters are new, resend iBuffer
             if(.not. IsCounted) call MPI_isend( &
                  iBufferS_IPI(1,iProcSend,iSendStage), &
                  nMsgSend_PI(iProcSend,iSendStage), MPI_INTEGER, iProcSend, &
                  11, iComm, iRequestSMap_I(iRequestS), iError)
          end do
          !$acc end host_data

          iRequestR = 0
          !$acc host_data use_device(BufferR_IP, iBufferR_IPI)
          do iProcSend = 0, nProc-1
             if(iProc == iProcSend) CYCLE
             iRequestR = iRequestR + 1
             call MPI_irecv(BufferR_IP(1,iProcSend), &
                  nSizeBufferR_PI(iProcSend,iSendStage), MPI_REAL, iProcSend, &
                  10, iComm, iRequestR_I(iRequestR), iError)
             if(.not. IsCounted) call MPI_irecv( &
                  iBufferR_IPI(1,iProcSend,iSendStage), &
                  nMsgRecv_PI(iProcSend,iSendStage), MPI_INTEGER, iProcSend, &
                  11, iComm, iRequestRMap_I(iRequestR), iError)
          end do
          !$acc end host_data

          ! Local message passing
          call timing_start('local_mp_pass')

          !$acc parallel loop gang present(State_VGB)
          do iBlockSend = 1, nBlock
             if(Unused_B(iBlockSend)) CYCLE
             call message_pass_block_local(iBlockSend, nVar, nG, State_VGB)
          end do ! iBlockSend

          call timing_stop('local_mp_pass')

          call timing_start('MPI_wait')

          if(iRequestR > 0) then
             call MPI_waitall(iRequestR, iRequestR_I, &
                  MPI_STATUSES_IGNORE, iError)
             if(.not. IsCounted) call MPI_waitall(iRequestR, iRequestRMap_I, &
                  MPI_STATUSES_IGNORE, iError)
          end if
          ! wait for all sends to be completed
          if(iRequestS > 0) then
             call MPI_waitall(iRequestS, iRequestS_I, &
                  MPI_STATUSES_IGNORE, iError)
             if(.not. IsCounted) call MPI_waitall(iRequestS, iRequestSMap_I, &
                  MPI_STATUSES_IGNORE, iError)
          end if

          call timing_stop('MPI_wait')
          call timing_start('buffer_to_state')

          !$acc parallel loop gang collapse(2) &
          !$acc present(BufferR_IP, iBufferR_IPI)
          do iProcSend = 0, nProc-1
             do iMsgSend = 1, nMsgRecv
                if(iProcSend == iProc) CYCLE
                if(iMsgSend > nMsgRecv_PI(iProcSend,iSendStage)) CYCLE
                call buffer_to_state_parallel(iProcSend, iMsgSend, &
                     nVar, nG, State_VGB)
             end do
          end do

          call timing_stop('buffer_to_state')
       end do ! iSendStage
    end if

    ! save parameters of this call
    nVarOld = nVar
    nGOld = nG
    nWidthOld = nWidth
    nProlongOrderOld = nProlongOrder
    nCoarseLayerOld = nCoarseLayer
    DoSendCornerOld = DoSendCorner
    DoRestrictFaceOld = DoRestrictFace
    DoResChangeOnlyOld = DoResChangeOnly

    call timing_stop('part1_pass')

    call test_stop(NameSub, Dotest)
    call timing_stop('batl_pass')
  contains
    !==========================================================================
    subroutine buffer_to_state_parallel(iProcSend, iMsgSend, &
         nVar, nG, State_VGB)
      !$acc routine vector

      ! Copy buffer into recv block of State_VGB message by message in parallel
      use BATL_size, ONLY:MaxBlock, nDim, nI, nJ, nK, jDim_, kDim_

      integer, intent(in):: iProcSend
      integer, intent(in):: iMsgSend
      integer, intent(in):: nVar
      integer, intent(in):: nG
      real,    intent(inout):: State_VGB(nVar, &
           1-nG:nI+nG,1-nG*jDim_:nJ+nG*jDim_,1-nG*kDim_:nK+nG*kDim_,MaxBlock)

      integer:: iBufferR, iVarR , i, j, k
      integer:: iBlockRecv

      ! Index range for recv and send segments of the blocks
      integer :: iRMin, iRMax, jRMin, jRMax, kRMin, kRMax

      ! Message passing across the pole can reverse the recv. index range
      integer :: DiR, DjR, DkR
      !------------------------------------------------------------------------
      jRMin = 1; jRMax = 1
      kRMin = 1; kRMax = 1

      DiR = 1; DjR = 1; DkR = 1

      ! Initial buffer index
      iBufferR = iBufferR_IPI(iMsgSend,iProcSend,iSendStage)

      ! Extract header information
      iBlockRecv = nint(BufferR_IP(iBufferR,iProcSend))
      iRMin      = nint(BufferR_IP(iBufferR+1,iProcSend))
      iRMax      = nint(BufferR_IP(iBufferR+2,iProcSend))
      if(nDim > 1) DiR = sign(1,iRMax - iRMin)
      if(nDim > 1) jRMin = nint(BufferR_IP(iBufferR+3,iProcSend))
      if(nDim > 1) jRMax = nint(BufferR_IP(iBufferR+4,iProcSend))
      if(nDim > 2) DjR   = sign(1, jRmax - jRMin)
      if(nDim > 2) kRMin = nint(BufferR_IP(iBufferR+5,iProcSend))
      if(nDim > 2) kRMax = nint(BufferR_IP(iBufferR+6,iProcSend))
      if(nDim > 2) DkR   = sign(1, kRmax - kRMin)

      !$acc loop vector collapse(4) private(iBufferR)
      do k = kRMin,kRmax,DkR; do j = jRMin,jRMax,DjR; do i = iRMin,iRmax,DiR
         do iVarR = 1, nVar
            iBufferR = nVar*(&
                 abs(k-kRMin)*(abs(jRMax-jRMin)+1)*(abs(iRMax-iRMin)+1) + &
                 abs(j-jRMin)*(abs(iRMax-iRMin)+1) + &
                 abs(i-iRMin)) + &
                 iVarR + &
                 iBufferR_IPI(iMsgSend,iProcSend,iSendStage) + 2*nDim
            State_VGB(iVarR,i,j,k,iBlockRecv) = BufferR_IP(iBufferR,iProcSend)
         end do
      end do; end do; end do

    end subroutine buffer_to_state_parallel
    !==========================================================================
    subroutine set_range
      ! acc routine seq

      integer:: nWidthProlongS_D(MaxDim), iDim

      ! Indexed by iDir/jDir/kDir for sender = -1,0,1
      !------------------------------------------------------------------------
      iEqualS_DII(:,-1,Min_) = 1
      iEqualS_DII(:,-1,Max_) = nWidth
      iEqualS_DII(:, 0,Min_) = 1
      iEqualS_DII(:, 0,Max_) = nIjk_D
      iEqualS_DII(:, 1,Min_) = nIjk_D + 1 - nWidth
      iEqualS_DII(:, 1,Max_) = nIjk_D

      ! Indexed by iDir/jDir/kDir for sender = -1,0,1
      iEqualR_DII(:,-1,Min_) = nIjk_D + 1
      iEqualR_DII(:,-1,Max_) = nIjk_D + nWidth
      iEqualR_DII(:, 0,Min_) = 1
      iEqualR_DII(:, 0,Max_) = nIjk_D
      iEqualR_DII(:, 1,Min_) = 1 - nWidth
      iEqualR_DII(:, 1,Max_) = 0

      ! Indexed by iDir/jDir/kDir for sender = -1,0,1
      iRestrictS_DII(:,-1,Min_) = 1
      iRestrictS_DII(:, 0,Min_) = 1
      iRestrictS_DII(:, 0,Max_) = nIjk_D
      iRestrictS_DII(:, 1,Max_) = nIjk_D
      if(DoRestrictFace)then
         iRestrictS_DII(:,-1,Max_) = nWidth
         iRestrictS_DII(:, 1,Min_) = nIjk_D + 1 - nWidth
      else
         iRestrictS_DII(:,-1,Max_) = iRatio_D*nWidth
         iRestrictS_DII(:, 1,Min_) = nIjk_D + 1 - iRatio_D*nWidth
      end if

      ! Indexed by iRecv/jRecv/kRecv = 0..3
      iRestrictR_DII(:,0,Min_) = 1 - nWidth
      iRestrictR_DII(:,0,Max_) = 0
      iRestrictR_DII(:,1,Min_) = 1
      do iDim = 1, MaxDim
         ! This loop is used to avoid the NAG 5.1 (282) bug on nyx
         iRestrictR_DII(iDim,1,Max_) = nIjk_D(iDim)/iRatio_D(iDim)
         iRestrictR_DII(iDim,2,Min_) = nIjk_D(iDim)/iRatio_D(iDim) + 1
      end do
      iRestrictR_DII(:,2,Max_) = nIjk_D
      iRestrictR_DII(:,3,Min_) = nIjk_D + 1
      iRestrictR_DII(:,3,Max_) = nIjk_D + nWidth

      ! Number of ghost cells sent from coarse block.
      ! Divided by resolution ratio and rounded up.
      nWidthProlongS_D         = 0
      if(nCoarseLayer == 1)then
         nWidthProlongS_D(1:nDim) = 1 + (nWidth-1)/iRatio_D(1:nDim)
      else
         nWidthProlongS_D(1:nDim) = nWidth
      end if

      ! Indexed by iSend/jSend,kSend = 0..3
      do iDim = 1, MaxDim
         ! This loop is used to avoid the NAG 5.1 (282) bug on nyx
         iProlongS_DII(iDim,0,Min_) = 1
         iProlongS_DII(iDim,0,Max_) = nWidthProlongS_D(iDim)
         iProlongS_DII(iDim,1,Min_) = 1
         iProlongS_DII(iDim,1,Max_) = nIjk_D(iDim)/iRatio_D(iDim)
         iProlongS_DII(iDim,2,Min_) = nIjk_D(iDim)/iRatio_D(iDim) + 1
         iProlongS_DII(iDim,2,Max_) = nIjk_D(iDim)
         iProlongS_DII(iDim,3,Min_) = nIjk_D(iDim) + 1 - nWidthProlongS_D(iDim)
         iProlongS_DII(iDim,3,Max_) = nIjk_D(iDim)
      end do

      ! Indexed by iRecv/jRecv/kRecv = 0,1,2,3
      iProlongR_DII(:, 0,Min_) = 1 - nWidth
      iProlongR_DII(:, 0,Max_) = 0
      iProlongR_DII(:, 1,Min_) = 1
      iProlongR_DII(:, 1,Max_) = nIjk_D
      iProlongR_DII(:, 2,Min_) = 1
      iProlongR_DII(:, 2,Max_) = nIjk_D
      iProlongR_DII(:, 3,Min_) = nIjk_D + 1
      iProlongR_DII(:, 3,Max_) = nIjk_D + nWidth

      if(DoSendCorner)then
         ! Face + two edges + corner or edge + one corner
         ! are sent/recv together from fine to coarse block

         do iDim = 1, nDim
            if(iRatio_D(iDim) == 1)CYCLE

            ! The extension is by nWidth/2 rounded upwards independent of
            ! the value of nCoarseLayers. There is no need to send
            ! two coarse layers into corner/edge ghost cells.
            iProlongS_DII(iDim,1,Max_) = iProlongS_DII(iDim,1,Max_) &
                 + (nWidth+1)/2
            iProlongS_DII(iDim,2,Min_) = iProlongS_DII(iDim,2,Min_) &
                 - (nWidth+1)/2
            iProlongR_DII(iDim,1,Max_) = iProlongR_DII(iDim,1,Max_) + nWidth
            iProlongR_DII(iDim,2,Min_) = iProlongR_DII(iDim,2,Min_) - nWidth
         end do
      end if

    end subroutine set_range
    !==========================================================================
  end subroutine message_pass_real_gpu
  !============================================================================
  subroutine message_count_block(iBlockSend, nVar, nG)
    ! run in serially on cpu
!!! alternative algoritm:
!!! use a scalar to get out after estimating nMsgSend and Recv

    ! set memory maps for parallel algorithm:
    ! nMsgSend_PBI,  iBufferS_IPI, iMsgDir_IBPI

    use BATL_size, ONLY: iRatio, jRatio, kRatio
    use BATL_tree, ONLY: &
         iNodeNei_IIIB, DiLevelNei_IIIB, iNode_B, &
         iTree_IA, Proc_, Block_, Coord1_, Coord2_, Coord3_

    ! Arguments
    integer, intent(in):: iBlockSend

    integer, intent(in):: nVar  ! number of variables
    integer, intent(in):: nG    ! number of ghost cells for 1..nDim

    ! Local variables
    integer :: iNodeSend
    integer :: iDir, jDir, kDir
    integer :: DiLevel

    ! local variables for parallel algorithm
    integer:: iSend, jSend, kSend, iRecv, jRecv, kRecv
    integer:: iNodeRecv, iProcRecv, iBlockRecv
    integer:: iProcSend, iMsg
    integer:: IntDir
    integer:: iSMin, iSMax, jSMin, jSMax, kSMin, kSMax ! for computing msg size
    integer:: iRMin, iRMax, jRMin, jRMax, kRMin, kRMax
    integer:: iSide, jSide, kSide
    integer:: nSizeS
    integer:: nSizeR
    !--------------------------------------------------------------------------
    iNodeSend = iNode_B(iBlockSend)

    do kDir = -1, 1
       ! Do not message pass in ignored dimensions
       if(nDim < 3 .and. kDir /= 0) CYCLE

       do jDir = -1, 1
          if(nDim < 2 .and. jDir /= 0) CYCLE
          ! Skip edges
          if(.not.DoSendCorner .and. jDir /= 0 .and. kDir /= 0) CYCLE

          do iDir = -1, 1
             ! Ignore inner parts of the sending block
             if(iDir == 0 .and. jDir == 0 .and. kDir == 0) CYCLE

             ! Exclude corners where i and j or k is at the edge
             if(.not.DoSendCorner .and. iDir /= 0 .and. &
                  (jDir /= 0 .or.  kDir /= 0)) CYCLE

             ! Level difference = own_level - neighbor_level
             DiLevel = DiLevelNei_IIIB(iDir,jDir,kDir,iBlockSend)

             ! Do prolongation in the second stage if
             ! nProlongOrder=2. We still need to call restriction
             ! and prolongation in both stages to calculate the
             ! amount of received data
             if(iSendStage == 2 .and. DiLevel == 0) CYCLE

             ! find out each block does how many comms
             if(DiLevel == 0) then
                ! Equal resolution

                ! skip equal resolution if message change in only done where
                ! the resolution changes
                if(DoResChangeOnly) CYCLE

                iSend = (3*iDir + 3)/2
                jSend = (3*jDir + 3)/2
                kSend = (3*kDir + 3)/2

                iNodeRecv = iNodeNei_IIIB(iSend,jSend,kSend,iBlockSend)
                iBlockRecv = iTree_IA(Block_,iNodeRecv)
                iProcRecv = iTree_IA(Proc_,iNodeRecv)
                iProcSend = iTree_IA(Proc_,iNodeSend)

                ! convert (iSend,jSend,kSend) to 0-63 using base 4
                IntDir = iSend
                if(nDim > 1) IntDir = IntDir +  4*jSend
                if(nDim > 2) IntDir = IntDir + 16*kSend

                if(iProcRecv /= iProcSend) then
                   ! Starting index of messages for IntDir direction
                   iMsgDir_IBPI(IntDir,iBlockSend,iProcRecv,iSendStage) = &
                        nMsgSend_PBI(iProcRecv,iBlockSend,iSendStage)
                   ! Add this message to the counter nMsgSend_PBI
                   nMsgSend_PBI(iProcRecv,iBlockSend,iSendStage) = &
                        nMsgSend_PBI(iProcRecv,iBlockSend,iSendStage) + 1
                   ! cumulative number of messages sent per processor
                   ! controls the size of dynamic arrays
                   ! in thie serial loop, nMsgSend_PI is also iMsgSend_PI
                   nMsgSend_PI(iProcRecv,iSendStage) = &
                        nMsgSend_PI(iProcRecv,iSendStage) + 1
                   ! Send and Recv maps are symmetric for equal resolution
                   nMsgRecv_PI(iProcRecv,iSendStage) =&
                        nMsgRecv_PI(iProcRecv,iSendStage)+1

                   ! simplify syntax
                   iMsg = nMsgSend_PI(iProcRecv,iSendStage)

                   ! Calculate size of this message
                   iSMin = iEqualS_DII(1,iDir,Min_)
                   iSMax = iEqualS_DII(1,iDir,Max_)
                   jSMin = iEqualS_DII(2,jDir,Min_)
                   jSMax = iEqualS_DII(2,jDir,Max_)
                   kSMin = iEqualS_DII(3,kDir,Min_)
                   kSMax = iEqualS_DII(3,kDir,Max_)
                   ! Block index, 2*nDim index ranges, and nVar*nCell values
                   nSizeS = nIndex &
                        + nVar*(iSMax-iSMin+1)*(jSMax-jSMin+1)*(kSMax-kSMin+1)

                   ! cumulative number of variables sent per processor for
                   ! size of buffers
                   nSizeBufferS_PI(iProcRecv,iSendStage) =&
                        nSizeBufferS_PI(iProcRecv,iSendStage) &
                        + nSizeS
                   ! for equal res, sending and recving sizes are equal
                   nSizeBufferR_PI(iProcRecv,iSendStage) =&
                        nSizeBufferR_PI(iProcRecv,iSendStage) &
                        + nSizeS

                   ! Initialize buffer index for first message
                   if(iMsg == 1) &
                        iBufferS_IPI(iMsg,iProcRecv,iSendStage) = 1

                   ! Initialize buffer index for the next message
                   iBufferS_IPI(iMsg+1,iProcRecv,iSendStage) = &
                        iBufferS_IPI(iMsg,iProcRecv,iSendStage) + nSizeS
                end if ! iProcRecv/=iProcSend
             else if(DiLevel == 1) then
                ! neighbour is coarser, this block does restriction
                ! this block sends 1 message

                iSide = 0; if(iRatio==2) &
                     iSide = modulo(iTree_IA(Coord1_,iNodeSend)-1, 2)
                jSide = 0; if(jRatio==2) &
                     jSide = modulo(iTree_IA(Coord2_,iNodeSend)-1, 2)
                kSide = 0; if(kRatio==2) &
                     kSide = modulo(iTree_IA(Coord3_,iNodeSend)-1, 2)

                ! Do not restrict diagonally in the direction of the sibling
                if(iDir == -1 .and. iSide==1 .and. iRatio == 2) CYCLE
                if(iDir == +1 .and. iSide==0 .and. iRatio == 2) CYCLE
                if(jDir == -1 .and. jSide==1 .and. jRatio == 2) CYCLE
                if(jDir == +1 .and. jSide==0 .and. jRatio == 2) CYCLE
                if(kDir == -1 .and. kSide==1 .and. kRatio == 2) CYCLE
                if(kDir == +1 .and. kSide==0 .and. kRatio == 2) CYCLE

                iSend = (3*iDir + 3 + iSide)/2
                jSend = (3*jDir + 3 + jSide)/2
                kSend = (3*kDir + 3 + kSide)/2

                iNodeRecv  = iNodeNei_IIIB(iSend,jSend,kSend,iBlockSend)
                iProcRecv  = iTree_IA(Proc_,iNodeRecv)
                iBlockRecv = iTree_IA(Block_,iNodeRecv)
                iProcSend  = iTree_IA(Proc_,iNodeSend)

                ! Only do restriction in the first stage, but receive a
                ! prolonged buffer in second stage
                if(nProlongOrder == 2 .and. iSendStage == 2)then
                   if(iProcRecv /= iProcSend)then
                      nMsgRecv_PI(iProcRecv,iSendStage) =&
                           nMsgRecv_PI(iProcRecv,iSendStage) + 1
                      iRMin = iProlongR_DII(1,iSend,Min_)
                      iRMax = iProlongR_DII(1,iSend,Max_)
                      jRMin = iProlongR_DII(2,jSend,Min_)
                      jRMax = iProlongR_DII(2,jSend,Max_)
                      kRMin = iProlongR_DII(3,kSend,Min_)
                      kRMax = iProlongR_DII(3,kSend,Max_)

                      ! Block index, 2*nDim index limits, nVar*nCell variables
                      nSizeR = nIndex + &
                           nVar*(iRMax-iRMin+1)*(jRMax-jRMin+1)*(kRMax-kRMin+1)

                      ! Increase buffer size
                      nSizeBufferR_PI(iProcRecv,iSendStage) =&
                           nSizeBufferR_PI(iProcRecv,iSendStage) &
                           + nSizeR
                   end if
                   CYCLE
                end if

                IntDir = iSend
                if(nDim > 1) IntDir = IntDir +  4*jSend
                if(nDim > 2) IntDir = IntDir + 16*kSend

                if(iProcRecv /= iProcSend) then
                   ! Set message index for this direction
                   iMsgDir_IBPI(IntDir,iBlockSend,iProcRecv,iSendStage) = &
                        nMsgSend_PBI(iProcRecv,iBlockSend,iSendStage)
                   ! Count message
                   nMsgSend_PBI(iProcRecv,iBlockSend,iSendStage) = &
                        nMsgSend_PBI(iProcRecv,iBlockSend,iSendStage) + 1
                   ! cumulative number of messages sent per processor
                   ! controls the size of dynamic arrays
                   ! in this serial loop, nMsgSend_PI is also iMsgSend_PI
                   nMsgSend_PI(iProcRecv,iSendStage) =&
                        nMsgSend_PI(iProcRecv,iSendStage) + 1

                   iMsg = nMsgSend_PI(iProcRecv,iSendStage)

                   ! only receive a message if first order prolongation
                   if(nProlongOrder == 1)then
                      nMsgRecv_PI(iProcRecv,iSendStage) =&
                           nMsgRecv_PI(iProcRecv,iSendStage) + 1
                      ! Size of received buffer is larger:
                      iRMin = iProlongR_DII(1,iSend,Min_)
                      iRMax = iProlongR_DII(1,iSend,Max_)
                      jRMin = iProlongR_DII(2,jSend,Min_)
                      jRMax = iProlongR_DII(2,jSend,Max_)
                      kRMin = iProlongR_DII(3,kSend,Min_)
                      kRMax = iProlongR_DII(3,kSend,Max_)

                      ! Block index, 2*nDim index limits, nVar*nCell variables
                      nSizeR = nIndex + &
                           nVar*(iRMax-iRMin+1)*(jRMax-jRMin+1)*(kRMax-kRMin+1)
                      ! Increase buffer size
                      nSizeBufferR_PI(iProcRecv,iSendStage) =&
                           nSizeBufferR_PI(iProcRecv,iSendStage) &
                           + nSizeR
                   end if

                   ! size of send message: only need to count for the sender
                   iRecv = iSend - 3*iDir
                   jRecv = jSend - 3*jDir
                   kRecv = kSend - 3*kDir

                   ! Receiving range depends on iRecv,kRecv,jRecv = 0..3
                   iSMin = iRestrictR_DII(1,iRecv,Min_)
                   iSMax = iRestrictR_DII(1,iRecv,Max_)
                   jSMin = iRestrictR_DII(2,jRecv,Min_)
                   jSMax = iRestrictR_DII(2,jRecv,Max_)
                   kSMin = iRestrictR_DII(3,kRecv,Min_)
                   kSMax = iRestrictR_DII(3,kRecv,Max_)

                   ! Block index, 2*nDim index limits, nVar*nCell variables
                   nSizeS = nIndex + &
                        nVar*(iSMax-iSMin+1)*(jSMax-jSMin+1)*(kSMax-kSMin+1)

                   ! cumulative number of variables sent per processor
                   ! controls the size of buffers
                   nSizeBufferS_PI(iProcRecv,iSendStage) =&
                        nSizeBufferS_PI(iProcRecv,iSendStage) &
                        + nSizeS

                   ! Buffer index set to 1 for first message
                   if(iMsg == 1) &
                        iBufferS_IPI(iMsg,iProcRecv,iSendStage) = 1

                   ! Buffer index for next message
                   iBufferS_IPI(iMsg+1,iProcRecv,iSendStage)=&
                        iBufferS_IPI(iMsg,iProcRecv,iSendStage) &
                        + nSizeS
                end if ! iProcRecv/=iProcSend
             else if(DiLevel == -1) then
                ! neighbours are finer, this block does prolongation
                ! this block sends 1/2/4 messages, hence the triple loop
                ! Loop through the subfaces or subedges
                do kSide = (1-kDir)/2, 1-(1+kDir)/2, 3-kRatio
                   kSend = (3*kDir + 3 + kSide)/2
                   kRecv = kSend - 3*kDir
                   do jSide = (1-jDir)/2, 1-(1+jDir)/2, 3-jRatio
                      jSend = (3*jDir + 3 + jSide)/2
                      jRecv = jSend - 3*jDir
                      do iSide = (1-iDir)/2, 1-(1+iDir)/2, 3-iRatio
                         iSend = (3*iDir + 3 + iSide)/2
                         iRecv = iSend - 3*iDir

                         iNodeRecv = &
                              iNodeNei_IIIB(iSend,jSend,kSend,iBlockSend)
                         iBlockRecv = iTree_IA(Block_,iNodeRecv)
                         iProcRecv  = iTree_IA(Proc_,iNodeRecv)
                         iProcSend  = iTree_IA(Proc_,iNodeSend)

                         ! Only do prolongation in the second stage, but
                         ! receive a restricted buffer in the first stage
                         if(nProlongOrder == 2 .and. iSendStage == 1) then
                            if(iProcRecv /= iProcSend)then
                               nMsgRecv_PI(iProcRecv,iSendStage) =&
                                    nMsgRecv_PI(iProcRecv,iSendStage)+1

                               iRMin = iRestrictR_DII(1,iSend,Min_)
                               iRMax = iRestrictR_DII(1,iSend,Max_)
                               jRMin = iRestrictR_DII(2,jSend,Min_)
                               jRMax = iRestrictR_DII(2,jSend,Max_)
                               kRMin = iRestrictR_DII(3,kSend,Min_)
                               kRMax = iRestrictR_DII(3,kSend,Max_)

                               ! Block index, 2*nDim index limits, nVar*nCell
                               nSizeR = nIndex + &
                                    nVar*(iRMax-iRMin+1)*(jRMax-jRMin+1)* &
                                    (kRMax-kRMin+1)
                               nSizeBufferR_PI(iProcRecv,iSendStage) = &
                                    nSizeBufferR_PI(iProcRecv,iSendStage) &
                                    + nSizeR
                            end if
                            CYCLE
                         end if

                         ! convert (iSend,jSend,kSend) to 0-63 using base 4
                         IntDir = iSend
                         if(nDim > 1) IntDir = IntDir +  4*jSend
                         if(nDim > 2) IntDir = IntDir + 16*kSend

                         if(iProcRecv /= iProcSend) then
                            iMsgDir_IBPI(IntDir,iBlockSend,iProcRecv, &
                                 iSendStage) = &
                                 nMsgSend_PBI(iProcRecv,iBlockSend,iSendStage)
                            ! ranks in nMsgSend_PBI start from 0
                            nMsgSend_PBI(iProcRecv,iBlockSend,iSendStage) = &
                                 nMsgSend_PBI(iProcRecv,iBlockSend,iSendStage)&
                                 + 1
                            ! cumulative number of messages sent per PE
                            ! controls the size of dynamic arrays
                            ! (serial) nMsgSend_PI is also iMsgSend_PI
                            nMsgSend_PI(iProcRecv,iSendStage) = &
                                 nMsgSend_PI(iProcRecv,iSendStage) + 1

                            iMsg = nMsgSend_PI(iProcRecv,iSendStage)
                            ! only receive a message if first order prolong
                            if(nProlongOrder == 1) then
                               nMsgRecv_PI(iProcRecv,iSendStage) = &
                                    nMsgRecv_PI(iProcRecv,iSendStage) + 1
                               ! size of restricted buffer is smaller:
                               iRMin = iRestrictR_DII(1,iSend,Min_)
                               iRMax = iRestrictR_DII(1,iSend,Max_)
                               jRMin = iRestrictR_DII(2,jSend,Min_)
                               jRMax = iRestrictR_DII(2,jSend,Max_)
                               kRMin = iRestrictR_DII(3,kSend,Min_)
                               kRMax = iRestrictR_DII(3,kSend,Max_)

                               nSizeR = nIndex + &
                                    nVar*(iRMax-iRMin+1)*(jRMax-jRMin+1)*&
                                    (kRMax-kRMin+1)
                               nSizeBufferR_PI(iProcRecv,iSendStage) = &
                                    nSizeBufferR_PI(iProcRecv,iSendStage) &
                                    + nSizeR
                            end if

                            ! size of this message
                            iSMin = iProlongR_DII(1,iRecv,Min_)
                            iSMax = iProlongR_DII(1,iRecv,Max_)
                            jSMin = iProlongR_DII(2,jRecv,Min_)
                            jSMax = iProlongR_DII(2,jRecv,Max_)
                            kSMin = iProlongR_DII(3,kRecv,Min_)
                            kSMax = iProlongR_DII(3,kRecv,Max_)

                            nSizeS = nIndex + nVar*(iSMax-iSMin+1)*&
                                 (jSMax-jSMin+1)*(kSMax-kSMin+1)

                            nSizeBufferS_PI(iProcRecv,iSendStage) =&
                                 nSizeBufferS_PI(iProcRecv,iSendStage) + nSizeS

                            if(iMsg == 1) &
                                 iBufferS_IPI(iMsg,iProcRecv,iSendStage) = 1

                            iBufferS_IPI(iMsg+1,iProcRecv,iSendStage) = &
                                 iBufferS_IPI(iMsg,iProcRecv,iSendStage)+nSizeS
                         end if ! iProcRecv/=iProcSend
                      end do
                   end do
                end do ! loop through subfaces and subedges
             end if ! DiLevel

          end do ! iDir
       end do ! jDir
    end do ! kDir

  end subroutine message_count_block
  !============================================================================
  subroutine message_pass_block_remote(iBlockSend, nVar, nG, State_VGB)
    !$acc routine vector

    use BATL_size, ONLY: MaxBlock, nI, nJ, nK, jDim_, kDim_, &
         iRatio, jRatio, kRatio, InvIjkRatio
    use BATL_grid, ONLY: CoordMin_DB, CoordMax_DB
    use BATL_tree, ONLY: &
         iNodeNei_IIIB, DiLevelNei_IIIB, Unused_BP, iNode_B, &
         iTree_IA, Proc_, Block_, Coord1_, Coord2_, Coord3_

    ! Arguments
    integer, intent(in):: iBlockSend

    integer, intent(in):: nVar  ! number of variables
    integer, intent(in):: nG    ! number of ghost cells for 1..nDim
    real, intent(inout):: State_VGB(nVar, &
         1-nG:nI+nG,1-nG*jDim_:nJ+nG*jDim_,1-nG*kDim_:nK+nG*kDim_,MaxBlock)

    ! Local variables
    integer :: iNodeSend
    integer :: iDir, jDir, kDir

    ! Is the sending node next to the symmetry axis?
    logical :: IsAxisNode

    integer :: DiLevel
    !--------------------------------------------------------------------------
    iNodeSend = iNode_B(iBlockSend)

    IsAxisNode = .false.

    ! acc loop seq
    do kDir = -1, 1
       ! Do not message pass in ignored dimensions
       if(nDim < 3 .and. kDir /= 0) CYCLE

       if(nDim > 2 .and. IsLatitudeAxis) IsAxisNode = &
            kDir == -1 .and. &
            CoordMin_DB(Lat_,iBlockSend) < -cHalfPi + 1e-8 .or. &
            kDir == +1 .and. &
            CoordMax_DB(Lat_,iBlockSend) > +cHalfPi - 1e-8

       ! acc loop seq
       do jDir = -1, 1
          if(nDim < 2 .and. jDir /= 0) CYCLE
          ! Skip edges
          if(.not.DoSendCorner .and. jDir /= 0 .and. kDir /= 0) &
               CYCLE

          if(nDim > 2 .and. IsSphericalAxis) IsAxisNode = &
               jDir == -1 .and. &
               CoordMin_DB(Theta_,iBlockSend) < 1e-8 .or. &
               jDir == +1 .and. &
               CoordMax_DB(Theta_,iBlockSend) > cPi-1e-8

          ! acc loop seq
          do iDir = -1, 1
             ! Ignore inner parts of the sending block
             if(iDir == 0 .and. jDir == 0 .and. kDir == 0) CYCLE

             ! Exclude corners where i and j or k is at the edge
             if(.not.DoSendCorner .and. iDir /= 0 .and. &
                  (jDir /= 0 .or.  kDir /= 0)) CYCLE

             if(nDim > 1 .and. IsCylindricalAxis) IsAxisNode = &
                  iDir == -1 .and. iTree_IA(Coord1_,iNodeSend) == 1

             ! Level difference = own_level - neighbor_level
             DiLevel = DiLevelNei_IIIB(iDir,jDir,kDir,iBlockSend)

             ! Only prolongation in the second stage
             if(iSendStage == 2 .and. DiLevel >= 0) CYCLE

             if(DiLevel == 0)then
                ! Send data to same-level neighbor
                if(.not.DoResChangeOnly)then
                   call do_equal_remote(iDir, jDir, kDir, &
                        iNodeSend, iBlockSend, nVar, nG, State_VGB, IsAxisNode)
                end if
             elseif(DiLevel == 1)then
                ! Send restricted data to coarser neighbor
                call do_restrict_remote(iDir, jDir, kDir, &
                     iNodeSend, iBlockSend, nVar, nG, State_VGB, IsAxisNode)
             elseif(DiLevel == -1 .and. iSendStage == nProlongOrder)then
                ! Send prolonged data to finer neighbor
                call do_prolong_remote(iDir, jDir, kDir, &
                     iNodeSend, iBlockSend, nVar, nG, State_VGB, IsAxisNode)
             endif
          end do ! iDir
       end do ! jDir
    end do ! kDir

  contains
    !==========================================================================
    subroutine do_equal_remote(iDir, jDir, kDir, iNodeSend, iBlockSend, &
         nVar, nG, State_VGB, IsAxisNode)

      !$acc routine vector
      use BATL_size, ONLY: MaxBlock, nI, nJ, nK, jDim_, kDim_, nDim
      use BATL_mpi, ONLY: iProc

      integer, intent(in):: iDir, jDir, kDir, iNodeSend, iBlockSend, nVar, nG
      real, intent(inout):: State_VGB(nVar,&
           1-nG:nI+nG,1-nG*jDim_:nJ+nG*jDim_,1-nG*kDim_:nK+nG*kDim_,MaxBlock)
      logical, intent(in):: IsAxisNode

      integer :: iBufferS, iVarS, i, j, k

      integer :: iSend, jSend, kSend
      integer :: iBlockRecv, iProcRecv, iNodeRecv
      ! Index range for recv and send segments of the blocks
      integer :: iRMin, iRMax, jRMin, jRMax, kRMin, kRMax
      integer :: iSMin, iSMax, jSMin, jSMax, kSMin, kSMax
      ! Message passing across the pole can reverse the recv. index range
      integer :: DiR, DjR, DkR

      logical :: DoTest

      integer :: iMsgGlob
      integer :: IntDir

      character(len=*), parameter:: NameSub = 'do_equal_remote'
      !------------------------------------------------------------------------
      DiR = 1; DjR = 1; DkR = 1

      iSend = (3*iDir + 3)/2
      jSend = (3*jDir + 3)/2
      kSend = (3*kDir + 3)/2

      iNodeRecv  = iNodeNei_IIIB(iSend,jSend,kSend,iBlockSend)
      iProcRecv  = iTree_IA(Proc_,iNodeRecv)

      if(iProc == iProcRecv) RETURN

      iBlockRecv = iTree_IA(Block_,iNodeRecv)

      ! For part implicit and part steady schemes
      if(Unused_BP(iBlockRecv,iProcRecv)) RETURN

      iRMin = iEqualR_DII(1,iDir,Min_)
      iRMax = iEqualR_DII(1,iDir,Max_)
      jRMin = iEqualR_DII(2,jDir,Min_)
      jRMax = iEqualR_DII(2,jDir,Max_)
      kRMin = iEqualR_DII(3,kDir,Min_)
      kRMax = iEqualR_DII(3,kDir,Max_)

      if(IsAxisNode)then
         if(IsLatitudeAxis)then
            kRMin = iEqualR_DII(3,-kDir,Max_)
            kRMax = iEqualR_DII(3,-kDir,Min_)
         elseif(IsSphericalAxis)then
            jRMin = iEqualR_DII(2,-jDir,Max_)
            jRMax = iEqualR_DII(2,-jDir,Min_)
         elseif(IsCylindricalAxis)then
            iRMin = iEqualR_DII(1,1,Max_)
            iRMax = iEqualR_DII(1,1,Min_)
         end if
      end if

      iSMin = iEqualS_DII(1,iDir,Min_)
      iSMax = iEqualS_DII(1,iDir,Max_)
      jSMin = iEqualS_DII(2,jDir,Min_)
      jSMax = iEqualS_DII(2,jDir,Max_)
      kSMin = iEqualS_DII(3,kDir,Min_)
      kSMax = iEqualS_DII(3,kDir,Max_)

      ! convert (iSend,jSend,kSend) to 0-63 using base 4
      IntDir = iSend
      if(nDim > 1) IntDir = IntDir +  4*jSend
      if(nDim > 2) IntDir = IntDir + 16*kSend

      iMsgGlob = 1 + iMsgInit_PBI(iProcRecv,iBlockSend,iSendStage) + &
           iMsgDir_IBPI(IntDir, iBlockSend, iProcRecv, iSendStage)
      iBufferS = iBufferS_IPI(iMsgGlob,iProcRecv,iSendStage)

      BufferS_IP(iBufferS,iProcRecv) = iBlockRecv
      BufferS_IP(iBufferS+1,iProcRecv) = iRMin
      BufferS_IP(iBufferS+2,iProcRecv) = iRMax
      if(nDim > 1) BufferS_IP(iBufferS+3,iProcRecv) = jRMin
      if(nDim > 1) BufferS_IP(iBufferS+4,iProcRecv) = jRMax
      if(nDim > 2) BufferS_IP(iBufferS+5,iProcRecv) = kRMin
      if(nDim > 2) BufferS_IP(iBufferS+6,iProcRecv) = kRMax

      !$acc loop vector collapse(4) private(iBufferS)
      do k = kSMin, kSmax; do j = jSMin, jSMax; do i = iSMin, iSmax
         do iVarS = 1, nVar
            iBufferS = nVar * ( &
                 abs(k-kSMin)*(abs(jSMax-jSMin)+1)*(abs(iSMax-iSMin)+1) + &
                 abs(j-jSMin)*(abs(iSMax-iSMin)+1) + abs(i-iSMin)) + &
                 iVarS + &
                 iBufferS_IPI(iMsgGlob,iProcRecv,iSendStage) + 2*nDim
            ! initial iBuffer
            BufferS_IP(iBufferS,iProcRecv) =&
                 State_VGB(iVarS,i,j,k,iBlockSend)
         end do
      end do; end do; end do

    end subroutine do_equal_remote
    !==========================================================================
    subroutine do_restrict_remote(iDir, jDir, kDir, iNodeSend, iBlockSend, &
         nVar, nG, State_VGB, IsAxisNode)
      !$acc routine vector

      use BATL_mpi, ONLY: iProc
      use BATL_size, ONLY: MaxBlock, nI, nJ, nK, jDim_, kDim_

      integer, intent(in):: iDir, jDir, kDir, iNodeSend, iBlockSend, nVar, nG
      real, intent(inout):: State_VGB(nVar,&
           1-nG:nI+nG,1-nG*jDim_:nJ+nG*jDim_,1-nG*kDim_:nK+nG*kDim_,MaxBlock)
      logical, intent(in):: IsAxisNode

      integer :: iR, jR, kR, iS1, jS1, kS1, iS2, jS2, kS2, iVar
      integer :: iRatioRestr, jRatioRestr, kRatioRestr
      real    :: InvIjkRatioRestr
      integer :: iBufferS

      integer :: iSend,jSend,kSend,iRecv,jRecv,kRecv,iSide,jSide,kSide
      integer :: iBlockRecv,iProcRecv,iNodeRecv
      ! Index range for recv and send segments of the blocks
      integer :: iRMin, iRMax, jRMin, jRMax, kRMin, kRMax
      integer :: iSMin, iSMax, jSMin, jSMax, kSMin, kSMax

      ! Message passing across the pole can reverse the recv. index range
      integer :: DiR, DjR, DkR

      integer :: IntDir, iMsgGlob

      ! For sideways communication from a fine to a coarser block
      ! the coordinate parity of the sender block tells
      ! if the receiver block fills into the
      ! lower (D*Recv = 0) or upper (D*Rev=1) half of the block
      !------------------------------------------------------------------------
      iSide = 0; if(iRatio==2) iSide = modulo(iTree_IA(Coord1_,iNodeSend)-1, 2)
      jSide = 0; if(jRatio==2) jSide = modulo(iTree_IA(Coord2_,iNodeSend)-1, 2)
      kSide = 0; if(kRatio==2) kSide = modulo(iTree_IA(Coord3_,iNodeSend)-1, 2)

      ! Do not restrict diagonally in the direction of the sibling.
      if(iDir == -1 .and. iSide==1 .and. iRatio == 2) RETURN
      if(iDir == +1 .and. iSide==0 .and. iRatio == 2) RETURN
      if(jDir == -1 .and. jSide==1 .and. jRatio == 2) RETURN
      if(jDir == +1 .and. jSide==0 .and. jRatio == 2) RETURN
      if(kDir == -1 .and. kSide==1 .and. kRatio == 2) RETURN
      if(kDir == +1 .and. kSide==0 .and. kRatio == 2) RETURN

      iSend = (3*iDir + 3 + iSide)/2
      jSend = (3*jDir + 3 + jSide)/2
      kSend = (3*kDir + 3 + kSide)/2

      iNodeRecv  = iNodeNei_IIIB(iSend,jSend,kSend,iBlockSend)
      iProcRecv  = iTree_IA(Proc_,iNodeRecv)
      if(iProc == iProcRecv) RETURN

      iBlockRecv = iTree_IA(Block_,iNodeRecv)
      ! For part implicit and part steady schemes
      if(Unused_BP(iBlockRecv,iProcRecv)) RETURN

      iRecv = iSend - 3*iDir
      jRecv = jSend - 3*jDir
      kRecv = kSend - 3*kDir

      ! Receiving range depends on iRecv,kRecv,jRecv = 0..3
      iRMin = iRestrictR_DII(1,iRecv,Min_)
      iRMax = iRestrictR_DII(1,iRecv,Max_)
      jRMin = iRestrictR_DII(2,jRecv,Min_)
      jRMax = iRestrictR_DII(2,jRecv,Max_)
      kRMin = iRestrictR_DII(3,kRecv,Min_)
      kRMax = iRestrictR_DII(3,kRecv,Max_)

      if(IsAxisNode)then
         if(IsLatitudeAxis)then
            kRMin = iRestrictR_DII(3,kSend,Max_)
            kRMax = iRestrictR_DII(3,kSend,Min_)
         elseif(IsSphericalAxis)then
            jRMin = iRestrictR_DII(2,jSend,Max_)
            jRMax = iRestrictR_DII(2,jSend,Min_)
         elseif(IsCylindricalAxis)then
            iRMin = iRestrictR_DII(1,0,Max_)
            iRMax = iRestrictR_DII(1,0,Min_)
         end if
      end if

      DiR = 1; DjR = 1; DkR = 1
      if(nDim > 1) DiR = sign(1, iRMax - iRMin)
      if(nDim > 2) DjR = sign(1, jRMax - jRMin)
      if(nDim > 2) DkR = sign(1, kRMax - kRMin)

      ! Index range that gets restricted depends on iDir,jDir,kDir only
      iSMin = iRestrictS_DII(1,iDir,Min_)
      iSMax = iRestrictS_DII(1,iDir,Max_)
      jSMin = iRestrictS_DII(2,jDir,Min_)
      jSMax = iRestrictS_DII(2,jDir,Max_)
      kSMin = iRestrictS_DII(3,kDir,Min_)
      kSMax = iRestrictS_DII(3,kDir,Max_)

      iRatioRestr = iRatio; jRatioRestr = jRatio; kRatioRestr = kRatio
      InvIjkRatioRestr = InvIjkRatio
      if(DoRestrictFace)then
         if(iDir /= 0) iRatioRestr = 1
         if(jDir /= 0) jRatioRestr = 1
         if(kDir /= 0) kRatioRestr = 1
         InvIjkRatioRestr = 1.0/(iRatioRestr*jRatioRestr*kRatioRestr)
      end if

      ! encode the send direction into an integer
      IntDir = iSend
      if(nDim > 1) IntDir = IntDir +  4*jSend
      if(nDim > 2) IntDir = IntDir + 16*kSend

      iMsgGlob = 1 + iMsgInit_PBI(iProcRecv,iBlockSend,iSendStage) + &
           iMsgDir_IBPI(IntDir, iBlockSend, iProcRecv, iSendStage)
      iBufferS = iBufferS_IPI(iMsgGlob,iProcRecv,iSendStage)
      BufferS_IP(iBufferS, iProcRecv) = iBlockRecv
      BufferS_IP(iBufferS+1, iProcRecv) = iRMin
      BufferS_IP(iBufferS+2, iProcRecv) = iRMax
      if(nDim > 1)BufferS_IP(iBufferS+3, iProcRecv) = jRMin
      if(nDim > 1)BufferS_IP(iBufferS+4, iProcRecv) = jRMax
      if(nDim > 2)BufferS_IP(iBufferS+5, iProcRecv) = kRMin
      if(nDim > 2)BufferS_IP(iBufferS+6, iProcRecv) = kRMax

      !$acc loop vector collapse(4) private(kS1,kS2,jS1,jS2,iS1,iS2,&
      !$acc iBufferS)
      do kR = kRMin, kRMax, DkR
         do jR = jRMin, jRMax, DjR
            do iR = iRMin, iRMax, DiR
               do iVar = 1, nVar

                  kS1 = kSMin + kRatioRestr*abs(kR-kRMin)
                  kS2 = kS1 + kRatioRestr - 1
                  jS1 = jSMin + jRatioRestr*abs(jR-jRMin)
                  jS2 = jS1 + jRatioRestr - 1
                  iS1 = iSMin + iRatioRestr*abs(iR-iRMin)
                  iS2 = iS1 + iRatioRestr - 1

                  iBufferS = nVar*(abs(iR-iRMin) + (abs(iRMax-iRMin)+1)*( &
                       abs(jR-jRMin)+(abs(jRMax-jRMin)+1)*abs(kR-kRMin))) +&
                       iVar + &
                       iBufferS_IPI(iMsgGlob,iProcRecv,iSendStage) + 2*nDim

                  if(UseMin) then
                     BufferS_IP(iBufferS,iProcRecv) = &
                          minval(State_VGB(iVar,iS1:iS2,jS1:jS2,kS1:kS2,&
                          iBlockSend))
                  else if(UseMax) then
                     BufferS_IP(iBufferS,iProcRecv) = &
                          maxval(State_VGB(iVar,iS1:iS2,jS1:jS2,kS1:kS2,&
                          iBlockSend))
                  else
                     BufferS_IP(iBufferS,iProcRecv) = &
                          InvIjkRatioRestr * &
                          sum(State_VGB(iVar,iS1:iS2,jS1:jS2,kS1:kS2,&
                          iBlockSend))
                  end if
               end do ! ivar
            end do
         end do
      end do ! ijkR
    end subroutine do_restrict_remote
    !==========================================================================
    subroutine do_prolong_remote(iDir, jDir, kDir, &
         iNodeSend, iBlockSend, nVar, nG, State_VGB, IsAxisNode)
      !$acc routine vector

      use BATL_mpi, ONLY: iProc
      integer, intent(in):: iDir, jDir, kDir, iNodeSend, iBlockSend, nVar, nG
      real, intent(inout):: State_VGB(nVar,&
           1-nG:nI+nG,1-nG*jDim_:nJ+nG*jDim_,1-nG*kDim_:nK+nG*kDim_,MaxBlock)
      logical, intent(in):: IsAxisNode

      integer:: iR, jR, kR, iS, jS, kS, iS1, jS1, kS1
      integer:: iRatioRestr, jRatioRestr, kRatioRestr
      integer:: iBufferS
      integer, parameter:: Di=iRatio-1, Dj=jRatio-1, Dk=kRatio-1
      real:: WeightI, WeightJ, WeightK

      integer :: iVarS

      integer :: iSend, jSend, kSend, iRecv, jRecv, kRecv, iSide, jSide, kSide
      integer :: iBlockRecv, iProcRecv, iNodeRecv

      ! Index range for recv and send segments of the blocks
      integer :: iRMin, iRMax, jRMin, jRMax, kRMin, kRMax
      integer :: iSMin, iSMax, jSMin, jSMax, kSMin, kSMax

      ! Message passing across the pole can reverse the recv. index range
      integer :: DiR, DjR, DkR

      integer :: IntDir, iMsgGlob
      !------------------------------------------------------------------------

      ! Loop through the subfaces or subedges
      do kSide = (1-kDir)/2, 1-(1+kDir)/2, 3-kRatio
         kSend = (3*kDir + 3 + kSide)/2
         kRecv = kSend - 3*kDir
         do jSide = (1-jDir)/2, 1-(1+jDir)/2, 3-jRatio
            jSend = (3*jDir + 3 + jSide)/2
            jRecv = jSend - 3*jDir
            do iSide = (1-iDir)/2, 1-(1+iDir)/2, 3-iRatio
               iSend = (3*iDir + 3 + iSide)/2
               iRecv = iSend - 3*iDir

               iNodeRecv  = iNodeNei_IIIB(iSend,jSend,kSend,iBlockSend)
               iProcRecv  = iTree_IA(Proc_,iNodeRecv)
               if(iProc == iProcRecv) CYCLE

               iBlockRecv = iTree_IA(Block_,iNodeRecv)
               ! For part implicit and part steady schemes
               if(Unused_BP(iBlockRecv,iProcRecv)) CYCLE

               ! Receiving range depends on iRecv,kRecv,jRecv = 0..3
               iRMin = iProlongR_DII(1,iRecv,Min_)
               iRMax = iProlongR_DII(1,iRecv,Max_)
               jRMin = iProlongR_DII(2,jRecv,Min_)
               jRMax = iProlongR_DII(2,jRecv,Max_)
               kRMin = iProlongR_DII(3,kRecv,Min_)
               kRMax = iProlongR_DII(3,kRecv,Max_)

               if(IsAxisNode)then
                  if(IsLatitudeAxis)then
                     kRMin = iProlongR_DII(3,kSend,Max_)
                     kRMax = iProlongR_DII(3,kSend,Min_)
                  elseif(IsSphericalAxis)then
                     jRMin = iProlongR_DII(2,jSend,Max_)
                     jRMax = iProlongR_DII(2,jSend,Min_)
                  elseif(IsCylindricalAxis)then
                     iRMin = iProlongR_DII(1,0,Max_)
                     iRMax = iProlongR_DII(1,0,Min_)
                  end if
               end if

               DiR = 1; DjR = 1; DkR = 1
               if(nDim > 1) DiR = sign(1, iRMax - iRMin)
               if(nDim > 2) DjR = sign(1, jRMax - jRMin)
               if(nDim > 2) DkR = sign(1, kRMax - kRMin)

               ! Sending range depends on iSend,jSend,kSend = 0..3
               iSMin = iProlongS_DII(1,iSend,Min_)
               iSMax = iProlongS_DII(1,iSend,Max_)
               jSMin = iProlongS_DII(2,jSend,Min_)
               jSMax = iProlongS_DII(2,jSend,Max_)
               kSMin = iProlongS_DII(3,kSend,Min_)
               kSMax = iProlongS_DII(3,kSend,Max_)

               iRatioRestr = iRatio
               jRatioRestr = jRatio
               kRatioRestr = kRatio
               if(iSendStage /= 4 .and. nCoarseLayer > 1)then
                  if(iDir /= 0) iRatioRestr = 1
                  if(jDir /= 0) jRatioRestr = 1
                  if(kDir /= 0) kRatioRestr = 1
               end if

               IntDir = iSend
               if(nDim > 1) IntDir = IntDir +  4*jSend
               if(nDim > 2) IntDir = IntDir + 16*kSend

               iMsgGlob = 1 + iMsgInit_PBI(iProcRecv,iBlockSend,iSendStage) &
                    + iMsgDir_IBPI(IntDir,iBlockSend,iProcRecv,iSendStage)
               iBufferS = iBufferS_IPI(iMsgGlob,iProcRecv,iSendStage)
               BufferS_IP(iBufferS, iProcRecv) = iBlockRecv
               BufferS_IP(iBufferS+1, iProcRecv) = iRMin
               BufferS_IP(iBufferS+2, iProcRecv) = iRMax
               if(nDim > 1)BufferS_IP(iBufferS+3,iProcRecv) = jRMin
               if(nDim > 1)BufferS_IP(iBufferS+4,iProcRecv) = jRMax
               if(nDim > 2)BufferS_IP(iBufferS+5,iProcRecv) = kRMin
               if(nDim > 2)BufferS_IP(iBufferS+6,iProcRecv) = kRMax

               !$acc loop vector private(iBufferS, iS, jS, kS,&
               !$acc iS1, jS1, kS1) collapse(4)
               do kR = kRMin, kRMax, DkR; do jR = jRMin, jRMax, DjR;&
                    do iR = iRMin, iRMax, DiR; do iVarS = 1, nVar
                  iS = iSMin + abs((iR+9)/iRatioRestr &
                       -           (iRMin+9)/iRatioRestr)
                  jS = jSMin + abs((jR+9)/jRatioRestr &
                       -           (jRMin+9)/jRatioRestr)
                  kS = kSMin + abs((kR+9)/kRatioRestr &
                       -           (kRMin+9)/kRatioRestr)

                  iBufferS =nVar*( abs(iR-iRMin) &
                       + (abs(iRMax-iRMin) + 1)*(abs(jR-jRMin)  &
                       + (abs(jRMax-jRMin) + 1)*abs(kR-kRMin))) &
                       + iBufferS_IPI(iMsgGlob,iProcRecv,iSendStage)+2*nDim

                  if(nProlongOrder == 2) then
                     ! For kRatio = 1 simple shift:
                     !    kS = kSMin + |kR - kRMin|
                     ! For kRatio = 2 coarsen both
                     ! kR and kRMin before shift
                     ! We add 9 both to kR and kRMin
                     ! before dividing by kRatio
                     ! so that all values remain
                     ! positive and get rounded down.
                     ! This works up to nG=10 ghost cells:
                     ! likely to be enough.

                     ! DkR=+1:
                     ! interpolate left for odd kR, right for even kR
                     ! DkR=-1:
                     ! interpolate left for even kR, right for odd kR
                     if(kRatio == 1) kS1 = kS
                     if(kRatio == 2) kS1 = kS + DkR*(1-2*modulo(kR,2))

                     if(jRatio == 1) jS1 = jS
                     if(jRatio == 2) jS1 = jS + DjR*(1-2*modulo(jR,2))

                     if(iRatio == 1) iS1 = iS
                     if(iRatio == 2) iS1 = iS + DiR*(1-2*modulo(iR,2))

                     if(UseMin)then
                        BufferS_IP(iBufferS+iVarS, iProcRecv) = min(&
                             State_VGB(iVarS,iS,jS,kS,iBlockSend), &
                             State_VGB(iVarS,iS1,jS,kS,iBlockSend), &
                             State_VGB(iVarS,iS,jS1,kS,iBlockSend), &
                             State_VGB(iVarS,iS,jS,kS1,iBlockSend))

                     elseif(UseMax)then
                        BufferS_IP(iBufferS+iVarS, iProcRecv) = max(&
                             State_VGB(iVarS,iS,jS,kS,iBlockSend), &
                             State_VGB(iVarS,iS1,jS,kS,iBlockSend), &
                             State_VGB(iVarS,iS,jS1,kS,iBlockSend), &
                             State_VGB(iVarS,iS,jS,kS1,iBlockSend))

                     else
                        ! For Cartesian grids the weights are 0.25
                        if(iRatio == 2) WeightI = 0.25
                        if(jRatio == 2) WeightJ = 0.25
                        if(kRatio == 2) WeightK = 0.25

                        BufferS_IP(iBufferS+iVarS, iProcRecv) =&
                             State_VGB(iVarS,iS,jS,kS,iBlockSend)

                        if(iRatio == 2)&
                             BufferS_IP(iBufferS+iVarS,iProcRecv) =&
                             BufferS_IP(iBufferS+iVarS,iProcRecv) +&
                             WeightI* &
                             ( State_VGB(iVarS,iS1,jS,kS,iBlockSend) &
                             - State_VGB(iVarS,iS ,jS,kS,iBlockSend) )

                        if(jRatio == 2)&
                             BufferS_IP(iBufferS+iVarS,iProcRecv) =&
                             BufferS_IP(iBufferS+iVarS,iProcRecv) +&
                             WeightJ* &
                             ( State_VGB(iVarS,iS,jS1,kS,iBlockSend) &
                             - State_VGB(iVarS,iS,jS ,kS,iBlockSend) )

                        if(kRatio == 2)&
                             BufferS_IP(iBufferS+iVarS,iProcRecv) =&
                             BufferS_IP(iBufferS+iVarS,iProcRecv) +&
                             WeightK* &
                             ( State_VGB(iVarS,iS,jS,kS1,iBlockSend) &
                             - State_VGB(iVarS,iS,jS,kS ,iBlockSend) )

                     end if
                  else
                     ! nProlongOrder == 1
                     BufferS_IP(iBufferS+iVarS,iProcRecv) =&
                          State_VGB(iVarS,iS,jS,kS,iBlockSend)
                  endif ! nProlongOrder == 2
               end do; end do; end do; end do
               ! iVarS, iR, jR, kR
            end do
         end do
      end do ! subedge subface triple loop

    end subroutine do_prolong_remote
    !==========================================================================
  end subroutine message_pass_block_remote
  !============================================================================
  subroutine message_pass_block_local(iBlockSend, nVar, nG, State_VGB)
    !$acc routine vector

    use BATL_size, ONLY: MaxBlock, nI, nJ, nK, jDim_, kDim_, &
         iRatio, jRatio, kRatio, InvIjkRatio
    use BATL_grid, ONLY: CoordMin_DB, CoordMax_DB
    use BATL_tree, ONLY: &
         iNodeNei_IIIB, DiLevelNei_IIIB, Unused_BP, iNode_B, &
         iTree_IA, Proc_, Block_, Coord1_, Coord2_, Coord3_

    ! Arguments
    integer, intent(in):: iBlockSend

    integer, intent(in):: nVar  ! number of variables
    integer, intent(in):: nG    ! number of ghost cells for 1..nDim
    real, intent(inout):: State_VGB(nVar, &
         1-nG:nI+nG,1-nG*jDim_:nJ+nG*jDim_,1-nG*kDim_:nK+nG*kDim_,MaxBlock)

    ! Send information from block iBlockSend to other blocks
    ! If DoRemote is true, send info to blocks on other cores

    integer :: iNodeSend
    integer :: iDir, jDir, kDir

    ! Is the sending node next to the symmetry axis?
    logical :: IsAxisNode

    integer :: DiLevel
    !--------------------------------------------------------------------------
    iNodeSend = iNode_B(iBlockSend)

    IsAxisNode = .false.

    ! acc loop seq
    do kDir = -1, 1
       ! Do not message pass in ignored dimensions
       if(nDim < 3 .and. kDir /= 0) CYCLE

       if(nDim > 2 .and. IsLatitudeAxis) IsAxisNode = &
            kDir == -1 .and. &
            CoordMin_DB(Lat_,iBlockSend) < -cHalfPi + 1e-8 .or. &
            kDir == +1 .and. &
            CoordMax_DB(Lat_,iBlockSend) > +cHalfPi - 1e-8

       ! acc loop seq
       do jDir = -1, 1
          if(nDim < 2 .and. jDir /= 0) CYCLE
          ! Skip edges
          if(.not.DoSendCorner .and. jDir /= 0 .and. kDir /= 0) &
               CYCLE

          if(nDim > 2 .and. IsSphericalAxis) IsAxisNode = &
               jDir == -1 .and. &
               CoordMin_DB(Theta_,iBlockSend) < 1e-8 .or. &
               jDir == +1 .and. &
               CoordMax_DB(Theta_,iBlockSend) > cPi-1e-8

          ! acc loop seq
          do iDir = -1, 1
             ! Ignore inner parts of the sending block
             if(iDir == 0 .and. jDir == 0 .and. kDir == 0) CYCLE

             ! Exclude corners where i and j or k is at the edge
             if(.not.DoSendCorner .and. iDir /= 0 .and. &
                  (jDir /= 0 .or.  kDir /= 0)) CYCLE

             if(nDim > 1 .and. IsCylindricalAxis) IsAxisNode = &
                  iDir == -1 .and. iTree_IA(Coord1_,iNodeSend) == 1

             ! Level difference = own_level - neighbor_level
             DiLevel = DiLevelNei_IIIB(iDir,jDir,kDir,iBlockSend)

             ! Do prolongation only in the second stage
             if(iSendStage == 2 .and. DiLevel >= 0) CYCLE

             if(DiLevel == 0)then
                ! Send data to same-level neighbor
                if(.not.DoResChangeOnly)then
                   call do_equal_local(iDir, jDir, kDir, &
                        iNodeSend, iBlockSend, nVar, nG, State_VGB, IsAxisNode)
                end if
             elseif(DiLevel == 1)then
                ! Send restricted data to coarser neighbor
                call do_restrict_local(iDir, jDir, kDir, &
                     iNodeSend, iBlockSend, nVar, nG, State_VGB, IsAxisNode)
             elseif(DiLevel == -1 .and. iSendStage == nProlongOrder)then
                ! Send prolonged data to finer neighbor
                call do_prolong_local(iDir, jDir, kDir, &
                     iNodeSend, iBlockSend, nVar, nG, State_VGB, IsAxisNode)
             endif
          end do ! iDir
       end do ! jDir
    end do ! kDir
  contains
    !==========================================================================
    subroutine do_equal_local(iDir, jDir, kDir, &
         iNodeSend, iBlockSend, nVar, nG, State_VGB, IsAxisNode)

      !$acc routine vector
      use BATL_size, ONLY: MaxBlock, nI, nJ, nK, jDim_, kDim_, nDim
      use BATL_mpi, ONLY: iProc

      integer, intent(in):: iDir, jDir, kDir, iNodeSend, iBlockSend, nVar, nG
      real, intent(inout):: State_VGB(nVar,&
           1-nG:nI+nG,1-nG*jDim_:nJ+nG*jDim_,1-nG*kDim_:nK+nG*kDim_,MaxBlock)
      logical, intent(in):: IsAxisNode

      integer :: iSend, jSend, kSend
      integer :: iBlockRecv, iProcRecv, iNodeRecv
      ! Index range for recv and send segments of the blocks
      integer :: iRMin, iRMax, jRMin, jRMax, kRMin, kRMax
      integer :: iSMin, iSMax, jSMin, jSMax, kSMin, kSMax
      ! Message passing across the pole can reverse the recv. index range
      integer :: DiR, DjR, DkR

      integer:: iS, jS, kS, iR, jR, kR, iVar

      character(len=*), parameter:: NameSub = 'do_equal_local'
      !------------------------------------------------------------------------
      iSend = (3*iDir + 3)/2
      jSend = (3*jDir + 3)/2
      kSend = (3*kDir + 3)/2

      iNodeRecv  = iNodeNei_IIIB(iSend,jSend,kSend,iBlockSend)
      iProcRecv  = iTree_IA(Proc_,iNodeRecv)
      ! Only local message passing is done here
      if(iProcRecv /= iProc) RETURN

      iBlockRecv = iTree_IA(Block_,iNodeRecv)
      ! For part implicit and part steady schemes
      if(Unused_BP(iBlockRecv,iProcRecv)) RETURN

      iRMin = iEqualR_DII(1,iDir,Min_)
      iRMax = iEqualR_DII(1,iDir,Max_)
      jRMin = iEqualR_DII(2,jDir,Min_)
      jRMax = iEqualR_DII(2,jDir,Max_)
      kRMin = iEqualR_DII(3,kDir,Min_)
      kRMax = iEqualR_DII(3,kDir,Max_)

      if(IsAxisNode)then
         if(IsLatitudeAxis)then
            kRMin = iEqualR_DII(3,-kDir,Max_)
            kRMax = iEqualR_DII(3,-kDir,Min_)
         elseif(IsSphericalAxis)then
            jRMin = iEqualR_DII(2,-jDir,Max_)
            jRMax = iEqualR_DII(2,-jDir,Min_)
         elseif(IsCylindricalAxis)then
            iRMin = iEqualR_DII(1,1,Max_)
            iRMax = iEqualR_DII(1,1,Min_)
         end if
      end if

      iSMin = iEqualS_DII(1,iDir,Min_)
      iSMax = iEqualS_DII(1,iDir,Max_)
      jSMin = iEqualS_DII(2,jDir,Min_)
      jSMax = iEqualS_DII(2,jDir,Max_)
      kSMin = iEqualS_DII(3,kDir,Min_)
      kSMax = iEqualS_DII(3,kDir,Max_)

      DiR = 1; DjR = 1; DkR = 1
      if(nDim > 1) DiR = sign(1, iRMax - iRMin)
      if(nDim > 2) DjR = sign(1, jRMax - jRMin)
      if(nDim > 2) DkR = sign(1, kRMax - kRMin)

      !$acc loop vector collapse(4) private(iR, jR, kR)
      do kS = kSMin, kSMax; do jS = jSMin, jSMax; do iS = iSMin, iSMax
         do iVar = 1, nVar
            iR = iRMin + DiR*(iS-iSMin)
            jR = jRMin + DjR*(jS-jSMin)
            kR = kRMin + DkR*(kS-kSMin)
            State_VGB(iVar,iR,jR,kR,iBlockRecv) = &
                 State_VGB(iVar,iS,jS,kS,iBlockSend)
         end do
      end do; end do; end do

    end subroutine do_equal_local
    !==========================================================================
    subroutine do_restrict_local(iDir, jDir, kDir, &
         iNodeSend, iBlockSend, nVar, nG, State_VGB, IsAxisNode)
      !$acc routine vector

      use BATL_mpi, ONLY: iProc
      use BATL_size, ONLY: MaxBlock, nI, nJ, nK, jDim_, kDim_

      integer, intent(in):: iDir, jDir, kDir, iNodeSend, iBlockSend, nVar, nG
      real, intent(inout):: State_VGB(nVar,&
           1-nG:nI+nG,1-nG*jDim_:nJ+nG*jDim_,1-nG*kDim_:nK+nG*kDim_,MaxBlock)

      logical, intent(in):: IsAxisNode

      integer :: iR, jR, kR, iS1, jS1, kS1, iS2, jS2, kS2, iVar
      integer :: iRatioRestr, jRatioRestr, kRatioRestr
      real    :: InvIjkRatioRestr

      integer :: iSend,jSend,kSend,iRecv,jRecv,kRecv,iSide,jSide,kSide
      integer :: iBlockRecv,iProcRecv,iNodeRecv
      ! Index range for recv and send segments of the blocks
      integer :: iRMin, iRMax, jRMin, jRMax, kRMin, kRMax
      integer :: iSMin, iSMax, jSMin, jSMax, kSMin, kSMax

      ! Message passing across the pole can reverse the recv. index range
      integer :: DiR, DjR, DkR
      !------------------------------------------------------------------------

      ! For sideways communication from a fine to a coarser block
      ! the coordinate parity of the sender block tells
      ! if the receiver block fills into the
      ! lower (D*Recv = 0) or upper (D*Rev=1) half of the block
      iSide = 0; if(iRatio==2) iSide = modulo(iTree_IA(Coord1_,iNodeSend)-1, 2)
      jSide = 0; if(jRatio==2) jSide = modulo(iTree_IA(Coord2_,iNodeSend)-1, 2)
      kSide = 0; if(kRatio==2) kSide = modulo(iTree_IA(Coord3_,iNodeSend)-1, 2)

      ! Do not restrict diagonally in the direction of the sibling.
      if(iDir == -1 .and. iSide==1 .and. iRatio == 2) RETURN
      if(iDir == +1 .and. iSide==0 .and. iRatio == 2) RETURN
      if(jDir == -1 .and. jSide==1 .and. jRatio == 2) RETURN
      if(jDir == +1 .and. jSide==0 .and. jRatio == 2) RETURN
      if(kDir == -1 .and. kSide==1 .and. kRatio == 2) RETURN
      if(kDir == +1 .and. kSide==0 .and. kRatio == 2) RETURN

      iSend = (3*iDir + 3 + iSide)/2
      jSend = (3*jDir + 3 + jSide)/2
      kSend = (3*kDir + 3 + kSide)/2

      iNodeRecv  = iNodeNei_IIIB(iSend,jSend,kSend,iBlockSend)
      iProcRecv  = iTree_IA(Proc_,iNodeRecv)
      ! Only local message passing is done here
      if(iProcRecv /= iProc) RETURN

      iBlockRecv = iTree_IA(Block_,iNodeRecv)
      ! For part implicit and part steady schemes
      if(Unused_BP(iBlockRecv,iProcRecv)) RETURN

      iRecv = iSend - 3*iDir
      jRecv = jSend - 3*jDir
      kRecv = kSend - 3*kDir

      ! Receiving range depends on iRecv,kRecv,jRecv = 0..3
      iRMin = iRestrictR_DII(1,iRecv,Min_)
      iRMax = iRestrictR_DII(1,iRecv,Max_)
      jRMin = iRestrictR_DII(2,jRecv,Min_)
      jRMax = iRestrictR_DII(2,jRecv,Max_)
      kRMin = iRestrictR_DII(3,kRecv,Min_)
      kRMax = iRestrictR_DII(3,kRecv,Max_)

      if(IsAxisNode)then
         if(IsLatitudeAxis)then
            kRMin = iRestrictR_DII(3,kSend,Max_)
            kRMax = iRestrictR_DII(3,kSend,Min_)
         elseif(IsSphericalAxis)then
            jRMin = iRestrictR_DII(2,jSend,Max_)
            jRMax = iRestrictR_DII(2,jSend,Min_)
         elseif(IsCylindricalAxis)then
            iRMin = iRestrictR_DII(1,0,Max_)
            iRMax = iRestrictR_DII(1,0,Min_)
         end if
      end if

      DiR = 1; DjR = 1; DkR = 1
      if(nDim > 1) DiR = sign(1, iRMax - iRMin)
      if(nDim > 2) DjR = sign(1, jRMax - jRMin)
      if(nDim > 2) DkR = sign(1, kRMax - kRMin)

      ! Index range that gets restricted depends on iDir,jDir,kDir only
      iSMin = iRestrictS_DII(1,iDir,Min_)
      iSMax = iRestrictS_DII(1,iDir,Max_)
      jSMin = iRestrictS_DII(2,jDir,Min_)
      jSMax = iRestrictS_DII(2,jDir,Max_)
      kSMin = iRestrictS_DII(3,kDir,Min_)
      kSMax = iRestrictS_DII(3,kDir,Max_)

      iRatioRestr = iRatio; jRatioRestr = jRatio; kRatioRestr = kRatio
      InvIjkRatioRestr = InvIjkRatio
      if(DoRestrictFace)then
         if(iDir /= 0) iRatioRestr = 1
         if(jDir /= 0) jRatioRestr = 1
         if(kDir /= 0) kRatioRestr = 1
         InvIjkRatioRestr = 1.0/(iRatioRestr*jRatioRestr*kRatioRestr)
      end if

      !$acc loop vector collapse(3) private(iS1,iS2,jS1,jS2,kS1,kS2)
      do kR = kRMin, kRMax, DkR
         do jR = jRMin, jRMax, DjR
            do iR = iRMin, iRMax, DiR
               kS1 = kSMin + kRatioRestr*abs(kR-kRMin)
               kS2 = kS1 + kRatioRestr - 1

               jS1 = jSMin + jRatioRestr*abs(jR-jRMin)
               jS2 = jS1 + jRatioRestr - 1

               iS1 = iSMin + iRatioRestr*abs(iR-iRMin)
               iS2 = iS1 + iRatioRestr - 1
               if(UseMin)then
                  do iVar = 1, nVar
                     State_VGB(iVar,iR,jR,kR,iBlockRecv) = minval( &
                          State_VGB(iVar,iS1:iS2,jS1:jS2,kS1:kS2,iBlockSend))
                  end do
               else if(UseMax) then
                  do iVar = 1, nVar
                     State_VGB(iVar,iR,jR,kR,iBlockRecv) = maxval( &
                          State_VGB(iVar,iS1:iS2,jS1:jS2,kS1:kS2,iBlockSend))
                  end do
               else
                  do iVar = 1, nVar
                     State_VGB(iVar,iR,jR,kR,iBlockRecv) = &
                          InvIjkRatioRestr * sum( &
                          State_VGB(iVar,iS1:iS2,jS1:jS2,kS1:kS2,iBlockSend))
                  end do
               end if
            end do ! iR
         end do ! jR
      end do ! kR

    end subroutine do_restrict_local
    !==========================================================================
    subroutine do_prolong_local(iDir, jDir, kDir, iNodeSend, iBlockSend, &
         nVar, nG, State_VGB, IsAxisNode)
      !$acc routine vector

      use BATL_mpi, ONLY: iProc
      integer, intent(in):: iDir, jDir, kDir, iNodeSend, iBlockSend, nVar, nG
      real, intent(inout):: State_VGB(nVar,&
           1-nG:nI+nG,1-nG*jDim_:nJ+nG*jDim_,1-nG*kDim_:nK+nG*kDim_,MaxBlock)
      logical, intent(in):: IsAxisNode

      integer:: iR, jR, kR, iS, jS, kS, iS1, jS1, kS1
      integer:: iRatioRestr, jRatioRestr, kRatioRestr
      integer, parameter:: Di=iRatio-1, Dj=jRatio-1, Dk=kRatio-1
      real:: WeightI, WeightJ, WeightK

      integer :: iSend, jSend, kSend, iRecv, jRecv, kRecv, iSide, jSide, kSide
      integer :: iBlockRecv, iProcRecv, iNodeRecv

      ! Index range for recv and send segments of the blocks
      integer :: iRMin, iRMax, jRMin, jRMax, kRMin, kRMax
      integer :: iSMin, iSMax, jSMin, jSMax, kSMin, kSMax

      ! Message passing across the pole can reverse the recv. index range
      integer :: DiR, DjR, DkR
      !------------------------------------------------------------------------

      ! Loop through the subfaces or subedges
      do kSide = (1-kDir)/2, 1-(1+kDir)/2, 3-kRatio
         kSend = (3*kDir + 3 + kSide)/2
         kRecv = kSend - 3*kDir
         do jSide = (1-jDir)/2, 1-(1+jDir)/2, 3-jRatio
            jSend = (3*jDir + 3 + jSide)/2
            jRecv = jSend - 3*jDir
            do iSide = (1-iDir)/2, 1-(1+iDir)/2, 3-iRatio
               iSend = (3*iDir + 3 + iSide)/2
               iRecv = iSend - 3*iDir

               iNodeRecv  = iNodeNei_IIIB(iSend,jSend,kSend,iBlockSend)
               iProcRecv  = iTree_IA(Proc_,iNodeRecv)
               ! Only local message passing is done here
               if(iProcRecv /= iProc) CYCLE

               iBlockRecv = iTree_IA(Block_,iNodeRecv)
               ! For part implicit and part steady schemes
               if(Unused_BP(iBlockRecv,iProcRecv)) CYCLE

               ! Receiving range depends on iRecv,kRecv,jRecv = 0..3
               iRMin = iProlongR_DII(1,iRecv,Min_)
               iRMax = iProlongR_DII(1,iRecv,Max_)
               jRMin = iProlongR_DII(2,jRecv,Min_)
               jRMax = iProlongR_DII(2,jRecv,Max_)
               kRMin = iProlongR_DII(3,kRecv,Min_)
               kRMax = iProlongR_DII(3,kRecv,Max_)

               if(IsAxisNode)then
                  if(IsLatitudeAxis)then
                     kRMin = iProlongR_DII(3,kSend,Max_)
                     kRMax = iProlongR_DII(3,kSend,Min_)
                  elseif(IsSphericalAxis)then
                     jRMin = iProlongR_DII(2,jSend,Max_)
                     jRMax = iProlongR_DII(2,jSend,Min_)
                  elseif(IsCylindricalAxis)then
                     iRMin = iProlongR_DII(1,0,Max_)
                     iRMax = iProlongR_DII(1,0,Min_)
                  end if
               end if

               DiR = 1; DjR = 1; DkR = 1
               if(nDim > 1) DiR = sign(1, iRMax - iRMin)
               if(nDim > 2) DjR = sign(1, jRMax - jRMin)
               if(nDim > 2) DkR = sign(1, kRMax - kRMin)

               ! Sending range depends on iSend,jSend,kSend = 0..3
               iSMin = iProlongS_DII(1,iSend,Min_)
               iSMax = iProlongS_DII(1,iSend,Max_)
               jSMin = iProlongS_DII(2,jSend,Min_)
               jSMax = iProlongS_DII(2,jSend,Max_)
               kSMin = iProlongS_DII(3,kSend,Min_)
               kSMax = iProlongS_DII(3,kSend,Max_)

               iRatioRestr = iRatio
               jRatioRestr = jRatio
               kRatioRestr = kRatio
               if(nCoarseLayer > 1)then
                  if(iDir /= 0) iRatioRestr = 1
                  if(jDir /= 0) jRatioRestr = 1
                  if(kDir /= 0) kRatioRestr = 1
               end if

               !$acc loop vector collapse(3)
               do kR = kRMin, kRMax, DkR
                  do jR = jRMin, jRMax, DjR
                     do iR = iRMin, iRMax, DiR
                        iS = iSMin + abs((iR+9)/iRatioRestr &
                             -           (iRMin+9)/iRatioRestr)
                        jS = jSMin + abs((jR+9)/jRatioRestr &
                             -           (jRMin+9)/jRatioRestr)
                        kS = kSMin + abs((kR+9)/kRatioRestr &
                             -           (kRMin+9)/kRatioRestr)
                        if(nProlongOrder == 2) then
                           ! For kRatio = 1 simple shift:
                           !    kS = kSMin + |kR - kRMin|
                           ! For kRatio = 2 coarsen both
                           ! kR and kRMin before shift
                           ! We add 9 both to kR and kRMin
                           ! before dividing by kRatio
                           ! so that all values remain
                           ! positive and get rounded down.
                           ! This works up to nG=10 ghost cells:
                           ! likely to be enough.

                           ! DkR=+1:
                           ! interpolate left for odd kR, right for even kR
                           ! DkR=-1:
                           ! interpolate left for even kR, right for odd kR
                           if(kRatio == 1) kS1 = kS
                           if(kRatio == 2) kS1 = kS &
                                + DkR*(1 - 2*modulo(kR, 2))

                           if(jRatio == 1) jS1 = jS
                           if(jRatio == 2) jS1 = jS &
                                + DjR*(1 - 2*modulo(jR, 2))

                           if(iRatio == 1) iS1 = iS
                           if(iRatio == 2) iS1 = iS &
                                + DiR*(1 - 2*modulo(iR, 2))

                           if(UseMin)then
                              State_VGB(:,iR,jR,kR,iBlockRecv) =  min( &
                                   State_VGB(:,iS,jS,kS,iBlockSend), &
                                   State_VGB(:,iS1,jS,kS,iBlockSend), &
                                   State_VGB(:,iS,jS1,kS,iBlockSend), &
                                   State_VGB(:,iS,jS,kS1,iBlockSend)  )
                           elseif(UseMax)then
                              State_VGB(:,iR,jR,kR,iBlockRecv) =  max( &
                                   State_VGB(:,iS,jS,kS,iBlockSend), &
                                   State_VGB(:,iS1,jS,kS,iBlockSend), &
                                   State_VGB(:,iS,jS1,kS,iBlockSend), &
                                   State_VGB(:,iS,jS,kS1,iBlockSend)  )
                           else
                              ! For Cartesian grids the weights are 0.25
                              if(iRatio == 2) WeightI = 0.25
                              if(jRatio == 2) WeightJ = 0.25
                              if(kRatio == 2) WeightK = 0.25

                              State_VGB(:,iR,jR,kR,iBlockRecv) = &
                                   State_VGB(:,iS,jS,kS,iBlockSend)

                              if(iRatio == 2) &
                                   State_VGB(:,iR,jR,kR,iBlockRecv) = &
                                   State_VGB(:,iR,jR,kR,iBlockRecv) &
                                   + WeightI* &
                                   ( State_VGB(:,iS1,jS,kS,iBlockSend) &
                                   - State_VGB(:,iS ,jS,kS,iBlockSend) )

                              if(jRatio == 2) &
                                   State_VGB(:,iR,jR,kR,iBlockRecv) = &
                                   State_VGB(:,iR,jR,kR,iBlockRecv) &
                                   + WeightJ* &
                                   ( State_VGB(:,iS,jS1,kS,iBlockSend) &
                                   - State_VGB(:,iS,jS ,kS,iBlockSend) )

                              if(kRatio == 2) &
                                   State_VGB(:,iR,jR,kR,iBlockRecv) = &
                                   State_VGB(:,iR,jR,kR,iBlockRecv) &
                                   + WeightK* &
                                   ( State_VGB(:,iS,jS,kS1,iBlockSend) &
                                   - State_VGB(:,iS,jS,kS ,iBlockSend) )

                           end if
                        else
                           ! nProlongOrder == 1
                           State_VGB(:,iR,jR,kR,iBlockRecv) = &
                                State_VGB(:,iS,jS,kS,iBlockSend)
                        endif
                     end do
                  end do
               end do
            end do
         end do
      end do ! subedge subface triple loop

    end subroutine do_prolong_local
    !==========================================================================
  end subroutine message_pass_block_local
  !============================================================================
end module BATL_pass_cell_gpu_parallel
!==============================================================================
