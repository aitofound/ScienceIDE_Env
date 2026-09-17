!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module BATL_pass_cell

  use BATL_pass_cell_gpu_parallel, ONLY: message_pass_real_gpu
  use BATL_test, ONLY: test_start, test_stop, iTest, jTest, kTest, &
       iBlockTest
  use BATL_geometry, ONLY: IsCartesianGrid, IsRotatedCartesian, IsRoundCube, &
  	IsCylindricalAxis, IsSphericalAxis, IsLatitudeAxis, Lat_, Theta_, &
  	coord_to_xyz
  use ModNumConst, ONLY: cPi, cHalfPi
  use BATL_high_order, ONLY: restrict_high_order_reschange, &
  	prolong_high_order_amr, &
  	prolong_high_order_face_ghost, &
  	correct_face_ghost_for_fine_block, &
  	limit_interpolation, restrict_high_order_amr
  use BATL_size, ONLY: MaxDim
  use ModUtilities, ONLY: CON_stop, lower_case
  use ModMpi
  use omp_lib

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

  public:: message_pass_cell, message_pass_ng_int1

  interface message_pass_cell
     module procedure            &
          message_pass_real,     &  ! Real array with arbitrary ghost cells
          message_pass_real1,    &  ! Real scalar with arbitrary ghost cells
          message_pass_ng_real,  &  ! Real array with nG ghost cells
          message_pass_ng_real1, &  ! Real scalar with nG ghost cells
          message_pass_ng_int1      ! Integer scalar with nG ghost cells
  end interface message_pass_cell

  ! Force using simple weights
  logical, public:: UseSimpleProlongation = .false.

  ! local variables corresponding to optional arguments
  logical :: UseMin, UseMax  ! logicals for min and max operators

  ! local variables corresponding to optional arguments
  integer :: nWidth
  integer :: nCoarseLayer
  integer :: nProlongOrder
  logical :: DoSendCorner
  logical :: DoRestrictFace
  logical :: DoResChangeOnly
  logical :: UseHighResChange
  character(len=3) :: NameOperator

  ! Variables for coarsened block.
  real, allocatable:: State_VIIIB(:,:,:,:,:)
  logical, allocatable:: IsAccurate_B(:)

  ! Fast lookup tables for index ranges per dimension
  integer, parameter:: Min_=1, Max_=2
  integer :: iRestrictS_DII(MaxDim,-1:1,Min_:Max_)
  integer :: iRestrictR_DII(MaxDim,0:3,Min_:Max_)
  integer :: iProlongS_DII(MaxDim,0:3,Min_:Max_)
  integer :: iProlongR_DII(MaxDim,0:3,Min_:Max_)
  integer :: iEqualS_DII(MaxDim,-1:1,Min_:Max_)
  integer :: iEqualR_DII(MaxDim,-1:1,Min_:Max_)
  !$omp threadprivate( iEqualS_DII, iEqualR_DII )

  ! Variables related to recv and send buffers
  integer, allocatable:: iBufferS_P(:), nBufferS_P(:), nBufferR_P(:)

  integer :: iBufferS, iBufferR
  integer :: MaxBufferS=-1, MaxBufferR=-1
  real, allocatable:: BufferR_I(:), BufferS_I(:)

  integer :: iRequestR, iRequestS, iError
  integer, allocatable:: iRequestR_I(:), iRequestS_I(:)

  ! Positivity of variables
  logical, allocatable:: IsPositive_V(:)

  ! High order resolution change
  logical, allocatable:: IsAccurateFace_GB(:,:,:,:)

  ! counting vs. sendrecv stages
  logical :: DoCountOnly

  ! Stage indexes
  ! indexes for multiple stages
  integer :: iSendStage, iSubStage

  ! local variables corresponding to optional arguments
  logical :: UseTime        ! true if time interpolation is to be done
  !$omp threadprivate( UseTime )

contains
  !============================================================================
  subroutine message_pass_real(nVar, nG, State_VGB, &
       nWidthIn, nProlongOrderIn, nCoarseLayerIn, DoSendCornerIn, &
       DoRestrictFaceIn, TimeOld_B, Time_B, DoTestIn, NameOperatorIn,&
       DoResChangeOnlyIn, UseHighResChangeIn, DefaultState_V,&
       iLevelMin, iLevelMax, UseOpenACCIn, iDecomposition)

    use BATL_size, ONLY: MaxBlock, nBlock, nI, nJ, nK, nIjk_D, &
         nDim, jDim_, kDim_, iRatio_D, MinI, MinJ, MinK, MaxI, MaxJ, MaxK
    use BATL_mpi, ONLY: iComm, nProc, iProc
    use BATL_tree, ONLY: DiLevelNei_IIIB, Unused_B, iNode_B

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
    integer, optional, intent(in) :: iLevelMin, iLevelMax
    real,    optional, intent(in) :: TimeOld_B(MaxBlock)
    real,    optional, intent(in) :: Time_B(MaxBlock)

    logical, optional, intent(in) :: UseHighResChangeIn
    real,    optional, intent(in) :: DefaultState_V(nVar)
    logical, optional, intent(in) :: DoTestIn
    logical, optional, intent(in) :: UseOpenACCIn

    ! to be compatible with gpu code but does nothing at the moment
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
    ! iLevelMin and iLevelMax restrict the communication for blocks with
    !    grid or time (if Time_B is present) levels in the
    !    iLevelMin..iLevelMax range.
    !    Default is message passing among all levels.
    ! TimeOld_B and Time_B are the simulation times associated with the
    !    ghost cells and physical cells of State_VGB, respectively.
    !    If these arguments are present, the ghost cells are interpolated
    !    in time. Default is a simple update with no temporal interpolation.
    ! UseHighResChangeIn determines if the fifth-order accurate scheme is
    !    used to obtain the ghost cell values at resolution changes.
    !    Default is false.
    ! DefaultState_V determines if the variables in State_VGB should be kept
    !    positive. Values larger than 0 indicates positive variables (like
    !    density or pressure). These variables are kept positive by
    !    the high order scheme. By default no variables are forced to
    !    remain positive.
    ! DoTestIn determines if verbose information should be printed.

    ! Local variables

    ! for high order resolution change.
    integer :: iCountOnly     ! index for 2 stage scheme for count, sendrecv

    integer :: iProcRecv, iBlockSend, iProcSend
    integer :: nSendStage
    integer :: iBlock

    logical :: UseOpenACC

    logical:: DoTest
    character(len=*), parameter:: NameSub = 'message_pass_real'
    !--------------------------------------------------------------------------
    UseOpenACC = .false.
    if(present(UseOpenACCIn)) UseOpenACC = UseOpenACCIn

#ifdef _OPENACC
    if(UseOpenACC)then
       call message_pass_real_gpu(nVar, nG, State_VGB, nWidthIn, &
            nProlongOrderIn, nCoarseLayerIn, DoSendCornerIn, &
            DoRestrictFaceIn, DoTestIn, NameOperatorIn, DoResChangeOnlyIn, &
            iDecomposition)
       RETURN
    end if
#endif

    DoTest = .false.

    call test_start(NameSub,DoTest)
    if(present(DoTestIn)) DoTest = DoTestIn .or. DoTest

    if(DoTest)write(*,*)NameSub,' starting with nVar=', nVar

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

    UseHighResChange = .false.
    if(present(UseHighResChangeIn)) UseHighResChange = UseHighResChangeIn

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

    if(UseHighResChange .and. nProlongOrder /=1) call CON_stop(NameSub// &
         'nProlongOrder should be 1 for high order resolution change')

    if(UseHighResChange .and. .not. DoSendCorner) call CON_stop(NameSub// &
         'DoSendCorner should be true when UseHighResChange = .true.')

    if(UseHighResChange .and. nCoarseLayer == 1) call CON_stop(NameSub// &
         'nCoarseLayer should be 2 when UseHighResChange = .true.')

    if(UseHighResChange) DoRestrictFace = .false.

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

    if(present(Time_B) .and. present(NameOperatorIn)) then
       call CON_stop(NameSub// &
            ': Time_B can not be used with '//trim(NameOperator))
    end if

    if(present(Time_B) .neqv. present(iLevelMin))then
       call CON_stop(NameSub// &
            ': Time_B and iLevelMin can only be used together')
    end if

    if(DoTest)write(*,*) NameSub, &
         ': Width, Prolong, Coarse, Corner, RestrictFace, ResChangeOnly=', &
         nWidth, nProlongOrder, nCoarseLayer, DoSendCorner, &
         DoRestrictFace, DoResChangeOnly

    ! Initialize logical for time interpolation/extrapolation
    UseTime = .false.

    call set_range

    if(nProc > 1)then
       ! Allocate fixed size communication arrays.
       if(.not.allocated(iBufferS_P))then
          allocate(iBufferS_P(0:nProc-1), nBufferS_P(0:nProc-1), &
               nBufferR_P(0:nProc-1))
          allocate(iRequestR_I(nProc-1), iRequestS_I(nProc-1))
       end if
    end if

    call timing_stop('init_pass')

    nSendStage = nProlongOrder

    if(UseHighResChange) then
       ! HighResChange has an issue with boundary conditions so far. One
       ! boundary block's corner/edge neighbors may be not defined (like
       ! reflect boundary) and the ghost cell values are not accurate (the
       ! values may be 777 after AMR). This problem only occurs with
       ! resolution change along the boundary with reflect/shear/float
       ! (boundary conditions that ghost cells depending on physical
       ! cells) boundary conditions.
       ! To solve the problem, fill in these corner/edge ghost cells with
       ! the nearby face ghost cell value (see ModCellBoundary::set_edge_
       ! corner_ghost)

       ! stage 1: first order prolongation and do_equal.
       ! stage 2:
       !   a) Do high order restriction remotely for all kinds of ghost cells.
       !   b) Pass restricted cells from fine to coarse.
       !   c) Do high order prolongation for face ghost cells locally.
       ! stage 3:
       !      Pass the locally high order prolonged face ghost cells
       !      to the edge/corner ghost cells of the neighboring fine blocks.
       ! stage 4:
       !      Perform remote high order prolongation on coarse blocks for
       !      edges, corners and the face ghost cells that are too complex
       !      to do locally.
       nSendStage = 4

       allocate( &
            State_VIIIB(nVar,max(nI/2,1),max(nJ/2,1),max(nK/2,1),nBlock), &
            IsAccurate_B(nBlock), &
            IsAccurateFace_GB(MinI:MaxI,MinJ:MaxJ,MinK:MaxK,nBlock), &
            IsPositive_V(nVar))
       IsPositive_V = .false.
       if(present(DefaultState_V)) IsPositive_V = DefaultState_V > 0
    end if

    call timing_start('part1_pass')

    if(nProc == 1) then

       do iSendStage = 1, nSendStage
          if(UseHighResChange) then
             State_VIIIB = 0
             IsAccurate_B = .false.
          endif

          iSubStage = 1
          DoCountOnly = .false.

          call timing_start('single_pass')

          ! Loop through all blocks that may send a message
          !$omp parallel do
          do iBlockSend = 1, nBlock
             if(Unused_B(iBlockSend)) CYCLE
             if(DoTest .and. iBlockSend==iBlockTest)then
                write(*,*)'On iProc=', iProc, ' iBlock=', iBlockSend, &
                     'iTest=', iTest, 'jTest=', jTest, 'kTest=', kTest
                write(*,*)''
             end if
             call message_pass_block(iBlockSend, nVar, nG, State_VGB, &
                  .false., TimeOld_B, Time_B, iLevelMin, iLevelMax, &
                  UseOpenACCIn)
          end do ! iBlockSend
          !$omp end parallel do

          call timing_stop('single_pass')

          if(UseHighResChange .and. iSendStage == 2) then
             !$omp parallel do
             do iBlock = 1, nBlock
                if (Unused_B(iBlock)) CYCLE
                call high_prolong_for_face_ghost(iBlock)
             enddo
             !$omp end parallel do
          endif

       end do
    else
       ! nProc > 1 case
       do iSendStage = 1, nSendStage

          if(UseHighResChange) then
             State_VIIIB = 0
             IsAccurate_B = .false.
          endif

          do iCountOnly = 1, 2
             DoCountOnly = iCountOnly == 1

             call timing_start('remote_pass')

             if(DoCountOnly)then
                ! Initialize buffer size
                nBufferR_P = 0
                nBufferS_P = 0
             else
                ! Make buffers large enough
                if(sum(nBufferR_P) > MaxBufferR) then
                   if(allocated(BufferR_I)) deallocate(BufferR_I)
                   MaxBufferR = sum(nBufferR_P)
                   allocate(BufferR_I(MaxBufferR))
                end if

                if(sum(nBufferS_P) > MaxBufferS) then
                   if(allocated(BufferS_I)) deallocate(BufferS_I)
                   MaxBufferS = sum(nBufferS_P)
                   allocate(BufferS_I(MaxBufferS))
                end if

                ! Initialize buffer indexes for storing data into BufferS_I
                iBufferS = 0
                do iProcRecv = 0, nProc-1
                   iBufferS_P(iProcRecv) = iBufferS
                   iBufferS = iBufferS + nBufferS_P(iProcRecv)
                end do
             end if

             do iBlockSend = 1, nBlock
                if(Unused_B(iBlockSend)) CYCLE
                call message_pass_block(iBlockSend, nVar, nG, State_VGB, &
                     .true., TimeOld_B, Time_B, iLevelMin, iLevelMax)
             end do ! iBlockSend

             call timing_stop('remote_pass')

          end do ! iCountOnly

          ! post sends
          iRequestS = 0
          iBufferS  = 1

          do iProcRecv = 0, nProc-1
             if(nBufferS_P(iProcRecv) == 0) CYCLE
             iRequestS = iRequestS + 1
             call MPI_isend(BufferS_I(iBufferS), nBufferS_P(iProcRecv), &
                  MPI_REAL, iProcRecv, 10, iComm, iRequestS_I(iRequestS), &
                  iError)
             iBufferS = iBufferS + nBufferS_P(iProcRecv)
          end do

          ! post requests
          iRequestR = 0
          iBufferR  = 1
          do iProcSend = 0, nProc-1
             if(nBufferR_P(iProcSend) == 0) CYCLE
             iRequestR = iRequestR + 1
             call MPI_irecv(BufferR_I(iBufferR), nBufferR_P(iProcSend), &
                  MPI_REAL, iProcSend, 10, iComm, iRequestR_I(iRequestR), &
                  iError)
             iBufferR = iBufferR + nBufferR_P(iProcSend)
          end do

          call timing_start('local_mp_pass')

          ! Local message passing
          !$omp parallel do
          do iBlockSend = 1, nBlock
             if(Unused_B(iBlockSend)) CYCLE
             call message_pass_block(iBlockSend, nVar, nG, State_VGB, &
                  .false., TimeOld_B, Time_B, iLevelMin, iLevelMax)
          end do
          !$omp end parallel do

          call timing_stop('local_mp_pass')

          ! wait for all requests to be completed
          call timing_start('wait_pass')
          if(iRequestR > 0) call MPI_waitall(iRequestR, iRequestR_I, &
               MPI_STATUSES_IGNORE, iError)

          ! wait for all sends to be completed
          if(iRequestS > 0) call MPI_waitall(iRequestS, iRequestS_I, &
               MPI_STATUSES_IGNORE, iError)
          call timing_stop('wait_pass')

          call timing_start('buffer_to_state')

!!! To call buffer_to_state on a GPU, the following construct doesn't work:
! acc serial
! ...
! acc end serial
! Using the serial construct results in only one processor receiving
! from the buffer.
! Instead, use the following construct for now.

          call buffer_to_state(nBufferR_P, BufferR_I, nVar, nG, State_VGB,&
               UseTime, TimeOld_B, Time_B)

          call timing_stop('buffer_to_state')

          if(UseHighResChange .and. iSendStage == 2) then
             !$omp parallel do
             do iBlock = 1, nBlock
                if (Unused_B(iBlock)) CYCLE
                call high_prolong_for_face_ghost(iBlock)
             enddo
             !$omp end parallel do
          endif
       end do ! iSendStage
    end if

    call timing_stop('part1_pass')

    if(UseHighResChange) &
         deallocate(State_VIIIB, IsAccurate_B, IsAccurateFace_GB, IsPositive_V)
    call test_stop(NameSub, Dotest)
    call timing_stop('batl_pass')
  contains
    !==========================================================================
    subroutine is_face_accurate(iBlock)

      integer, intent(in):: iBlock
      logical:: IsOnlyCornerFine
      integer:: iDirCorner, jDirCorner, kDirCorner
      integer:: iBegin, iEnd, jBegin, jEnd, kBegin, kEnd, Di, Dj, Dk
      integer:: iDir,jDir,kDir

      ! Non-face ghost cells are also set false.
      character(len=*), parameter:: NameSub = 'is_face_accurate'
      !------------------------------------------------------------------------
      IsAccurateFace_GB(:,:,:,iBlock) = .false.

      ! Assume face ghost cells are accurate.
      IsAccurateFace_GB(-2:0,      1:nJ,1:nK,iBlock) = .true.
      IsAccurateFace_GB(nI+1:nI+3, 1:nJ,1:nK,iBlock) = .true.
      if(nJ == 1) RETURN
      IsAccurateFace_GB(1:nI,   -2:0,   1:nK,iBlock) = .true.
      IsAccurateFace_GB(1:nI,nJ+1:nJ+3, 1:nK,iBlock) = .true.
      if(nK == 1) RETURN
      IsAccurateFace_GB(1:nI,1:nJ,   -2:0,   iBlock) = .true.
      IsAccurateFace_GB(1:nI,1:nJ, nK+1:nK+3,iBlock) = .true.

      do iDir = -1, 1; do jDir = -1, 1; do kDir = -1, 1
         if(abs(iDir)+abs(jDir)+abs(kDir) /= 1) CYCLE
         IsOnlyCornerFine = &
              is_only_corner_fine(iNode_B(iBlock), iDir, jDir, kDir, &
              iDirCorner, jDirCorner, kDirCorner)

         if(.not. IsOnlyCornerFine) CYCLE

         if(iDirCorner == 1) then
            iBegin = nI; iEnd = nI -3; Di = -1
         elseif(iDirCorner==-1) then
            iBegin = 1; iEnd = 4; Di = 1
         else
            call CON_stop(NameSub//': This case should not happen! - case1')
         endif

         if(jDirCorner == 1) then
            jBegin = nJ; jEnd = nJ -3; Dj = -1
         elseif(jDirCorner == -1) then
            jBegin = 1; jEnd = 4; Dj = 1
         else
            call CON_stop(NameSub//': This case should not happen! - case2')
         endif

         if(kDirCorner == 1) then
            kBegin = nK; kEnd = nK - 3; Dk = -1
         elseif(kDirCorner == -1) then
            kBegin = 1; kEnd = 4; Dk = 1
         else
            call CON_stop(NameSub//': This case should not happen! - case3')
         endif

         ! This kind of things should be replaced by a matrix finally.
         if(iDir == 1) then
            iBegin = nI+1; iEnd = nI+3; Di = 1
         elseif(iDir == -1) then
            iBegin = 0; iEnd = -2; Di = -1
         elseif(jDir == 1) then
            jBegin = nJ+1; jEnd = nJ+3; Dj = 1
         elseif(jDir == -1) then
            jBegin = 0; jEnd = -2; Dj = -1
         elseif(kDir == 1) then
            kBegin = nK+1; kEnd = nK+3; Dk = 1
         elseif(kDir == -1) then
            kBegin = 0; kEnd = -2; Dk = -1
         endif

         ! Find out the not accurate face cells.
         IsAccurateFace_GB(iBegin:iEnd:Di,jBegin:jEnd:Dj,kBegin:kEnd:Dk, &
              iBlock) = .false.
      enddo; enddo; enddo ! iDir jDir kDir

    end subroutine is_face_accurate
    !==========================================================================
    subroutine high_prolong_for_face_ghost(iBlock)
      ! High order prolongation for face ghost cells. It is done locally.

      integer, intent(in):: iBlock
      real, allocatable:: Field1_VG(:,:,:,:)
      integer:: DiLevelNei_I(6)
      !------------------------------------------------------------------------
      if(.not. allocated(Field1_VG)) &
           allocate(Field1_VG(nVar,MinI:MaxI,MinJ:MaxJ,MinK:MaxK))

      call is_face_accurate(iBlock)

      call prolong_high_order_face_ghost(&
           iBlock, nVar, Field1_VG, State_VGB(:,:,:,:,iBlock), &
           IsAccurateFace_GB(:,:,:,iBlock), IsPositiveIn_V=IsPositive_V)

      DiLevelNei_I(1) = DiLevelNei_IIIB(-1,0,0,iBlock)
      DiLevelNei_I(2) = DiLevelNei_IIIB(+1,0,0,iBlock)
      DiLevelNei_I(3) = DiLevelNei_IIIB(0,-1,0,iBlock)
      DiLevelNei_I(4) = DiLevelNei_IIIB(0,+1,0,iBlock)
      DiLevelNei_I(5) = DiLevelNei_IIIB(0,0,-1,iBlock)
      DiLevelNei_I(6) = DiLevelNei_IIIB(0,0,+1,iBlock)

      ! If the corner/edge block is not a coarse block, the ghost values for
      ! fine block need to be corrected.
      if(any(DiLevelNei_I == 1)) call correct_face_ghost_for_fine_block(&
           iBlock, nVar, State_VGB(:,:,:,:,iBlock), &
           IsPositiveIn_V=IsPositive_V)

    end subroutine high_prolong_for_face_ghost
    !==========================================================================
    subroutine buffer_to_state(nBufferR_P, BufferR_I, &
         nVar, nG, State_VGB, UseTime, TimeOld_B, Time_B)

      ! Copy buffer into recv block of State_VGB
      use BATL_size, ONLY:MaxBlock, nDim, nI, nJ, nK, jDim_, kDim_

      integer, intent(in):: nBufferR_P(0:)
      real, intent(in):: BufferR_I(:)

      integer, intent(in):: nVar
      integer, intent(in):: nG
      real, intent(inout):: State_VGB(nVar, &
         1-nG:nI+nG,1-nG*jDim_:nJ+nG*jDim_,1-nG*kDim_:nK+nG*kDim_,MaxBlock)

      logical, intent(inout):: UseTime
      real, optional, intent(in):: TimeOld_B(MaxBlock)
      real, optional, intent(in):: Time_B(MaxBlock)

      integer:: iBufferR, iVarR , i, j, k
      real :: TimeSend, WeightOld, WeightNew

      integer:: iProcSend
      integer:: iBlockRecv

      ! Index range for recv and send segments of the blocks
      integer :: iRMin, iRMax, jRMin, jRMax, kRMin, kRMax

      ! Message passing across the pole can reverse the recv. index range
      integer :: DiR, DjR, DkR
      !------------------------------------------------------------------------
      jRMin = 1; jRMax = 1
      kRMin = 1; kRMax = 1

      DiR = 1; DjR = 1; DkR = 1

      iBufferR = 0
      do iProcSend = 0, nProc-1
         if(nBufferR_P(iProcSend) == 0) CYCLE

         do
            iBlockRecv = nint(BufferR_I(iBufferR+1))
            iRMin      = nint(BufferR_I(iBufferR+2))
            iRMax      = nint(BufferR_I(iBufferR+3))
            if(nDim > 1) DiR = sign(1,iRMax - iRMin)
            if(nDim > 1) jRMin = nint(BufferR_I(iBufferR+4))
            if(nDim > 1) jRMax = nint(BufferR_I(iBufferR+5))
            if(nDim > 2) DjR   = sign(1, jRmax - jRMin)
            if(nDim > 2) kRMin = nint(BufferR_I(iBufferR+6))
            if(nDim > 2) kRMax = nint(BufferR_I(iBufferR+7))
            if(nDim > 2) DkR   = sign(1, kRmax - kRMin)

            iBufferR = iBufferR + 1 + 2*nDim
            if(present(Time_B))then
               ! Get time of neighbor and interpolate/extrapolate ghost cells
               iBufferR = iBufferR + 1
               TimeSend = BufferR_I(iBufferR)
               UseTime  = (TimeSend /= Time_B(iBlockRecv))
            end if
            if(UseTime) then
               ! Time interpolation
               WeightOld = (TimeSend - Time_B(iBlockRecv)) &
                    /      (TimeSend - TimeOld_B(iBlockRecv))
               WeightNew = 1 - WeightOld
               do k=kRMin,kRmax,DkR; do j=jRMin,jRMax,DjR; do i=iRMin,iRmax,DiR
                  State_VGB(:,i,j,k,iBlockRecv) = &
                       WeightOld*State_VGB(:,i,j,k,iBlockRecv) + &
                       WeightNew*BufferR_I(iBufferR+1:iBufferR+nVar)

                  iBufferR = iBufferR + nVar
               end do; end do; end do
            elseif(UseHighResChange)then
               do k=kRMin,kRmax,DkR; do j=jRMin,jRMax,DjR; do i=iRMin,iRmax,DiR
                  if(.not. (iSendStage == 4 &
                       .and. IsAccurateFace_GB(i,j,k,iBlockRecv)))then
                     State_VGB(:,i,j,k,iBlockRecv) = &
                          BufferR_I(iBufferR+1:iBufferR+nVar)
                  endif
                  iBufferR = iBufferR + nVar
               end do; end do; end do
            else
               do k=kRMin,kRmax,DkR; do j=jRMin,jRMax,DjR; do i=iRMin,iRmax,DiR
                  do iVarR = 1, nVar
                     State_VGB(iVarR,i,j,k,iBlockRecv) = &
                          BufferR_I(iBufferR+iVarR)
                  enddo
                  iBufferR = iBufferR + nVar
               end do; end do; end do

            end if
            if(iBufferR >= sum(nBufferR_P(0:iProcSend))) EXIT
         end do
      end do

    end subroutine buffer_to_state
    !==========================================================================
    subroutine set_range

      ! Set ranges of send and receive regions

      integer:: nWidthProlongS_D(MaxDim), iDim
      !------------------------------------------------------------------------
      !$omp parallel
      ! Indexed by iDir/jDir/kDir for sender = -1,0,1
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
      !$omp end parallel

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
  end subroutine message_pass_real
  !============================================================================
  subroutine message_pass_ng_real(nVar, State_VGB, &
       nWidthIn, nProlongOrderIn, nCoarseLayerIn, DoSendCornerIn, &
       DoRestrictFaceIn, TimeOld_B, Time_B, DoTestIn, NameOperatorIn,&
       DoResChangeOnlyIn, UseHighResChangeIn, DefaultState_V,&
       iLevelMin, iLevelMax, UseOpenACCIn, iDecomposition)

    ! Message pass real array with nVar variables and BATL_size::nG ghost cells
    use BATL_size, ONLY: MaxBlock, MinI, MaxI, MinJ, MaxJ, MinK, MaxK, nG

    ! Arguments
    integer, intent(in)   :: nVar
    real,    intent(inout):: &
         State_VGB(nVar,MinI:MaxI,MinJ:MaxJ,MinK:MaxK,MaxBlock)

    ! Optional arguments
    integer, optional, intent(in) :: nWidthIn
    integer, optional, intent(in) :: nProlongOrderIn
    integer, optional, intent(in) :: nCoarseLayerIn
    integer, optional, intent(in) :: iLevelMin, iLevelMax
    logical, optional, intent(in) :: DoSendCornerIn
    logical, optional, intent(in) :: DoRestrictFaceIn
    logical, optional, intent(in) :: DoTestIn
    logical, optional, intent(in) :: DoResChangeOnlyIn
    real,    optional, intent(in) :: DefaultState_V(nVar)
    logical, optional, intent(in) :: UseHighResChangeIn
    real,    optional, intent(in) :: TimeOld_B(MaxBlock)
    real,    optional, intent(in) :: Time_B(MaxBlock)
    logical, optional, intent(in) :: UseOpenACCIn
    integer, optional, intent(in) :: iDecomposition
    character(len=*), optional,intent(in) :: NameOperatorIn

    character(len=*), parameter:: NameSub = 'message_pass_ng_real'
    !--------------------------------------------------------------------------
    call message_pass_real(nVar, nG, State_VGB, nWidthIn=nWidthIn, &
         nProlongOrderIn=nProlongOrderIn, nCoarseLayerIn=nCoarseLayerIn, &
         DoSendCornerIn=DoSendCornerIn, DoRestrictFaceIn=DoRestrictFaceIn, &
         TimeOld_B=TimeOld_B, Time_B=Time_B, DoTestIn=DoTestIn, &
         NameOperatorIn=NameOperatorIn, DoResChangeOnlyIn=DoResChangeOnlyIn, &
         UseHighResChangeIn=UseHighResChangeIn, &
         DefaultState_V=DefaultState_V,&
         iLevelMin=iLevelMin, iLevelMax=iLevelMax, &
         UseOpenACCIn=UseOpenACCIn, iDecomposition=iDecomposition)

  end subroutine message_pass_ng_real
  !============================================================================
  subroutine message_pass_ng_real1(State_GB, &
       nWidthIn, nProlongOrderIn, nCoarseLayerIn, DoSendCornerIn, &
       DoRestrictFaceIn, TimeOld_B, Time_B, DoTestIn, NameOperatorIn,&
       DoResChangeOnlyIn, iLevelMin, iLevelMax, iDecomposition)

    ! Message pass real scalar with BATL_size::nG ghost cells
    use BATL_size, ONLY: MaxBlock, MinI, MaxI, MinJ, MaxJ, MinK, MaxK, nG

    ! Arguments
    real, intent(inout):: &
         State_GB(MinI:MaxI,MinJ:MaxJ,MinK:MaxK,MaxBlock)

    ! Optional arguments
    integer, optional, intent(in) :: nWidthIn
    integer, optional, intent(in) :: nProlongOrderIn
    integer, optional, intent(in) :: nCoarseLayerIn
    integer, optional, intent(in) :: iLevelMin, iLevelMax
    logical, optional, intent(in) :: DoSendCornerIn
    logical, optional, intent(in) :: DoRestrictFaceIn
    logical, optional, intent(in) :: DoTestIn
    logical, optional, intent(in) :: DoResChangeOnlyIn
    real,    optional, intent(in) :: TimeOld_B(MaxBlock)
    real,    optional, intent(in) :: Time_B(MaxBlock)
    integer, optional, intent(in) :: iDecomposition
    character(len=*), optional,intent(in) :: NameOperatorIn

    character(len=*), parameter:: NameSub = 'message_pass_ng_real1'
    !--------------------------------------------------------------------------
    call message_pass_real(1, nG, State_GB, nWidthIn=nWidthIn, &
         nProlongOrderIn=nProlongOrderIn, nCoarseLayerIn=nCoarseLayerIn, &
         DoSendCornerIn=DoSendCornerIn, DoRestrictFaceIn=DoRestrictFaceIn, &
         TimeOld_B=TimeOld_B, Time_B=Time_B, DoTestIn=DoTestIn, &
         NameOperatorIn=NameOperatorIn, DoResChangeOnlyIn=DoResChangeOnlyIn,&
         iLevelMin=iLevelMin, iLevelMax=iLevelMax, &
         iDecomposition=iDecomposition)

  end subroutine message_pass_ng_real1
  !============================================================================
  subroutine message_pass_real1(nG, State_GB, &
       nWidthIn, nProlongOrderIn, nCoarseLayerIn, DoSendCornerIn, &
       DoRestrictFaceIn, TimeOld_B, Time_B, DoTestIn, NameOperatorIn,&
       DoResChangeOnlyIn, iLevelMin, iLevelMax, iDecomposition)

    ! Message pass real scalar with BATL_size::nG ghost cells
    use BATL_size, ONLY: MaxBlock, nI, nJ, nK, jDim_, kDim_

    ! Arguments
    integer, intent(in):: nG
    real, intent(inout):: State_GB(1-nG:nI+nG,&
         1-nG*jDim_:nJ+nG*jDim_,1-nG*kDim_:nK+nG*kDim_,MaxBlock)

    ! Optional arguments
    integer, optional, intent(in) :: nWidthIn
    integer, optional, intent(in) :: nProlongOrderIn
    integer, optional, intent(in) :: nCoarseLayerIn
    integer, optional, intent(in) :: iLevelMin, iLevelMax
    logical, optional, intent(in) :: DoSendCornerIn
    logical, optional, intent(in) :: DoRestrictFaceIn
    logical, optional, intent(in) :: DoTestIn
    logical, optional, intent(in) :: DoResChangeOnlyIn
    real,    optional, intent(in) :: TimeOld_B(MaxBlock)
    real,    optional, intent(in) :: Time_B(MaxBlock)
    integer, optional, intent(in) :: iDecomposition
    character(len=*), optional, intent(in) :: NameOperatorIn

    character(len=*), parameter:: NameSub = 'message_pass_real1'
    !--------------------------------------------------------------------------
    call message_pass_real(1, nG, State_GB, nWidthIn=nWidthIn, &
         nProlongOrderIn=nProlongOrderIn, nCoarseLayerIn=nCoarseLayerIn, &
         DoSendCornerIn=DoSendCornerIn, DoRestrictFaceIn=DoRestrictFaceIn, &
         TimeOld_B=TimeOld_B, Time_B=Time_B, DoTestIn=DoTestIn, &
         NameOperatorIn=NameOperatorIn, DoResChangeOnlyIn=DoResChangeOnlyIn,&
         iLevelMin=iLevelMin, iLevelMax=iLevelMax, &
         iDecomposition=iDecomposition)

  end subroutine message_pass_real1
  !============================================================================
  subroutine message_pass_ng_int1(Int_GB, &
       nWidthIn, nProlongOrderIn, nCoarseLayerIn, DoSendCornerIn, &
       DoRestrictFaceIn, TimeOld_B, Time_B, DoTestIn, NameOperatorIn,&
       DoResChangeOnlyIn, UseOpenACCIn)

    ! Message pass scalar integer data with BATL_size::nG ghost cells
    use BATL_size, ONLY: MaxBlock, MinI, MaxI, MinJ, MaxJ, MinK, MaxK, nG, &
         nBlock
    ! Arguments
    integer, intent(inout) :: Int_GB(MinI:MaxI,MinJ:MaxJ,MinK:MaxK,MaxBlock)

    ! Optional arguments
    integer, optional, intent(in) :: nWidthIn
    integer, optional, intent(in) :: nProlongOrderIn
    integer, optional, intent(in) :: nCoarseLayerIn
    logical, optional, intent(in) :: DoSendCornerIn
    logical, optional, intent(in) :: DoRestrictFaceIn
    logical, optional, intent(in) :: DoTestIn
    logical, optional, intent(in) :: DoResChangeOnlyIn
    logical, optional, intent(in) :: UseOpenACCIn
    real,    optional, intent(in) :: TimeOld_B(MaxBlock)
    real,    optional, intent(in) :: Time_B(MaxBlock)
    character(len=*), optional,intent(in) :: NameOperatorIn

    ! help array for converting between Scalar_GB and State_VGB
    ! used by message_pass_cell
    real, allocatable, save:: Scalar_VGB(:,:,:,:,:)
    !$acc declare create(Scalar_VGB)

    character(len=*), parameter:: NameSub = 'message_pass_ng_int1'
    !--------------------------------------------------------------------------
    if(.not.allocated(Scalar_VGB)) &
         allocate(Scalar_VGB(1,MinI:MaxI,MinJ:MaxJ,MinK:MaxK,MaxBlock))

    Scalar_VGB(1,MinI:MaxI,MinJ:MaxJ,MinK:MaxK,1:nBlock) = &
         Int_GB(MinI:MaxI,MinJ:MaxJ,MinK:MaxK,1:nBlock)

    !$acc update device(Scalar_VGB)

    call message_pass_cell(1, nG, Scalar_VGB, nWidthIn=nWidthIn, &
         nProlongOrderIn=nProlongOrderIn, nCoarseLayerIn=nCoarseLayerIn, &
         DoSendCornerIn=DoSendCornerIn, DoRestrictFaceIn=DoRestrictFaceIn, &
         TimeOld_B=TimeOld_B, Time_B=Time_B, DoTestIn=DoTestIn, &
         NameOperatorIn=NameOperatorIn, DoResChangeOnlyIn=DoResChangeOnlyIn,&
         UseOpenACCIn=UseOpenACCIn)

    !$acc update host(Scalar_VGB)

    Int_GB(MinI:MaxI,MinJ:MaxJ,MinK:MaxK,1:nBlock) = &
         nint(Scalar_VGB(1,MinI:MaxI,MinJ:MaxJ,MinK:MaxK,1:nBlock))

  end subroutine message_pass_ng_int1
  !============================================================================
  subroutine message_pass_block(iBlockSend, nVar, nG, State_VGB, &
       DoRemote, TimeOld_B, Time_B, iLevelMin, iLevelMax, UseOpenACCIn)

    use BATL_size, ONLY: MaxBlock, nI, nJ, nK, nIjk_D, &
         MaxDim, nDim, jDim_, kDim_, &
         iRatio, jRatio, kRatio, iRatio_D, InvIjkRatio, &
         MinI, MinJ, MinK, MaxI, MaxJ, MaxK
    use BATL_grid, ONLY: CoordMin_DB, CoordMax_DB, Xyz_DGB, DomainSize_D, &
         CoordMin_D
    use BATL_tree, ONLY: &
         iNodeNei_IIIB, DiLevelNei_IIIB, Unused_BP, iNode_B, &
         iTree_IA, Proc_, Block_, Coord1_, Coord2_, Coord3_, Level_, &
         UseTimeLevel, iTimeLevel_A

    ! Arguments
    integer, intent(in):: iBlockSend

    integer, intent(in):: nVar  ! number of variables
    integer, intent(in):: nG    ! number of ghost cells for 1..nDim
    real, intent(inout):: State_VGB(nVar,&
         1-nG:nI+nG,1-nG*jDim_:nJ+nG*jDim_,1-nG*kDim_:nK+nG*kDim_,MaxBlock)

    ! Send information from block iBlockSend to other blocks
    ! If DoRemote is true, send info to blocks on other cores
    logical, intent(in):: DoRemote

    real,    intent(in), optional:: TimeOld_B(MaxBlock)
    real,    intent(in), optional:: Time_B(MaxBlock)
    integer, intent(in), optional:: iLevelMin, iLevelMax
    logical, intent(in), optional:: UseOpenACCIn

    ! Local variables

    logical :: UseOpenACC

    integer :: iNodeSend
    integer :: iDir, jDir, kDir

    ! Is the sending node next to the symmetry axis?
    logical :: IsAxisNode

    integer :: iLevelSend, DiLevel

    ! For high order resolution change, a few face ghost cells need to be
    ! calculated remotely after the coarse block have got accurate
    ! ghost cells.
    logical:: DoSendFace, DoRecvFace

    ! For 6th order correction, which may be better because of symmetry,
    ! 8 cells are needed in each direction. If it is not satisfied,
    ! use 5th order correction.
    logical, parameter:: DoSixthCorrect = nI>7 .and. nJ>7 .and. &
         (nK==1 .or. nK>7)
    !--------------------------------------------------------------------------
    iNodeSend = iNode_B(iBlockSend)

    ! Skip if the sending block level is not in the level range
    if(present(iLevelMin) .and. .not.UseTimeLevel)then
       iLevelSend = iTree_IA(Level_,iNodeSend)
       if(iLevelSend < iLevelMin) RETURN
    end if
    if(present(iLevelMax) .and. .not.UseTimeLevel)then
       iLevelSend = iTree_IA(Level_,iNodeSend)
       if(iLevelSend > iLevelMax) RETURN
    end if

    UseOpenACC = .false.
    if(present(UseOpenACCIn)) UseOpenACC = UseOpenACCIn

    IsAxisNode = .false.
    UseTime = .false.

    do kDir = -1, 1
       ! Do not message pass in ignored dimensions
       if(nDim < 3 .and. kDir /= 0) CYCLE

       if(nDim > 2 .and. IsLatitudeAxis) IsAxisNode = &
            kDir == -1 .and. &
            CoordMin_DB(Lat_,iBlockSend) < -cHalfPi + 1e-8 .or. &
            kDir == +1 .and. &
            CoordMax_DB(Lat_,iBlockSend) > +cHalfPi - 1e-8

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

          do iDir = -1,1
             ! Ignore inner parts of the sending block
             if(iDir == 0 .and. jDir == 0 .and. kDir == 0) CYCLE

             ! Exclude corners where i and j or k is at the edge
             if(.not.DoSendCorner .and. iDir /= 0 .and. &
                  (jDir /= 0 .or.  kDir /= 0)) CYCLE

             if(nDim > 1 .and. IsCylindricalAxis) IsAxisNode = &
                  iDir == -1 .and. iTree_IA(Coord1_,iNodeSend) == 1

             ! Level difference = own_level - neighbor_level
             DiLevel = DiLevelNei_IIIB(iDir,jDir,kDir,iBlockSend)

             ! Skip if the receiving block grid level is not
             ! in range. Time levels of the receiving block(s)
             ! will be checked later if UseTimeLevel is true.
             if(present(iLevelMin) .and. .not.UseTimeLevel)then
                if(iLevelSend - DiLevel < iLevelMin) CYCLE
             end if
             if(present(iLevelMax) .and. .not.UseTimeLevel)then
                if(iLevelSend - DiLevel > iLevelMax) CYCLE
             end if

             ! Do prolongation in the second stage if
             ! nProlongOrder=2. We still need to call restriction
             ! and prolongation in both stages to calculate the
             ! amount of received data
             if(iSendStage == 2 .and. DiLevel == 0) CYCLE

             ! Fill in the edge/corner ghost cells with values from
             ! face ghost cells.
             if(iSendStage == 3 .and. DiLevel /= 0) CYCLE

             ! Remote high order prolongation
             if(iSendStage == 4 .and. DiLevel == 0) CYCLE

             if(DiLevel == 0)then
                ! Send data to same-level neighbor
                if(iSendStage == 3) then
                   call corrected_do_equal
                else
                   if(.not.DoResChangeOnly) call do_equal(iDir, jDir, kDir,&
                        iNodeSend, iBlockSend, nVar, nG, State_VGB, &
                        DoRemote, IsAxisNode, iLevelMIn, Time_B, TimeOld_B)
                endif
             elseif(DiLevel == 1)then
                ! Send restricted data to coarser neighbor
                call do_restrict(iDir, jDir, kDir, iNodeSend, iBlockSend, &
                     nVar, nG, State_VGB, DoRemote, IsAxisNode, iLevelMIn, &
                     Time_B, TimeOld_B)
             elseif(DiLevel == -1)then
                ! Send prolonged data to finer neighbor
                call do_prolong(iDir, jDir, kDir, iNodeSend, iBlockSend, &
                     nVar, nG, State_VGB, DoRemote, IsAxisNode, iLevelMIn, &
                     Time_B, TimeOld_B)
             end if
          end do ! iDir
       end do ! jDir
    end do ! kDir

  contains
    !==========================================================================
    subroutine corrected_do_equal

      integer:: iEqualSOrig_DII(MaxDim,-1:1,Min_:Max_)
      integer:: iEqualROrig_DII(MaxDim,-1:1,Min_:Max_)
      integer:: iDir1,jDir1,kDir1, nDir
      integer:: iDir2,jDir2,kDir2, iDir3,jDir3,kDir3
      integer:: iSend,jSend,kSend
      integer:: iNodeRecv
      !------------------------------------------------------------------------
      nDir = abs(iDir) + abs(jDir) + abs(kDir)

      if(nDir > nDim-1) RETURN

      if(nDim == 2) then
         kDir1 = 0
         if(iDir /=0) then
            iDir1 = 0
            do jDir1 = -1, 1, 2
               ! Some information passed here is useless.
               ! Advantage: do not need to change do_equal.
               if(DiLevelNei_IIIB(iDir1,jDir1,kDir1,iBlockSend) == 1 .or.&
                    DiLevelNei_IIIB(iDir, jDir1,kDir1,iBlockSend) == 1 ) then
                  iEqualSOrig_DII = iEqualS_DII
                  iEqualROrig_DII = iEqualR_DII

                  if(jDir1 == -1) then
                     iEqualS_DII(2,0,Min_) = 1 - nWidth
                     iEqualS_DII(2,0,Max_) = 0

                     iEqualR_DII(2,0,Min_) = 1 - nWidth
                     iEqualR_DII(2,0,Max_) = 0
                  elseif(jDir1 == 1) then
                     iEqualS_DII(2,0,Min_) = nJ + 1
                     iEqualS_DII(2,0,Max_) = nJ + nWidth

                     iEqualR_DII(2,0,Min_) = nJ + 1
                     iEqualR_DII(2,0,Max_) = nJ + nWidth
                  endif
                  call do_equal(iDir, jDir, kDir, iNodeSend, iBlockSend, &
                       nVar, nG, State_VGB, DoRemote, IsAxisNode, &
                       iLevelMIn, Time_B, TimeOld_B)
                  iEqualS_DII = iEqualSOrig_DII
                  iEqualR_DII = iEqualROrig_DII
               endif
            enddo ! jDir1

         elseif(jDir /= 0) then
            jDir1 = 0
            do iDir1 = -1, 1, 2
               if(DiLevelNei_IIIB(iDir1,jDir1,kDir1,iBlockSend) == 1 .or. &
                    DiLevelNei_IIIB(iDir1,jDir, kDir1,iBlockSend) == 1) then

                  iEqualSOrig_DII = iEqualS_DII
                  iEqualROrig_DII = iEqualR_DII
                  if(iDir1 == -1) then
                     iEqualS_DII(1,0,Min_) = 1 - nWidth
                     iEqualS_DII(1,0,Max_) = 0

                     iEqualR_DII(1,0,Min_) = 1 - nWidth
                     iEqualR_DII(1,0,Max_) = 0
                  elseif(iDir1 == 1) then
                     iEqualS_DII(1,0,Min_) = nI + 1
                     iEqualS_DII(1,0,Max_) = nI + nWidth

                     iEqualR_DII(1,0,Min_) = nI + 1
                     iEqualR_DII(1,0,Max_) = nI + nWidth
                  endif
                  call do_equal(iDir, jDir, kDir, iNodeSend, iBlockSend, &
                       nVar, nG, State_VGB, DoRemote, IsAxisNode, &
                       iLevelMIn, Time_B, TimeOld_B)
                  iEqualR_DII = iEqualROrig_DII
               endif
            enddo ! iDir1
         endif

      elseif(nDim == 3) then
         if(nDir == 2) then
            if(iDir == 0) then
               jDir1 = 0; kDir1 = 0
               do iDir1 = -1, 1, 2
                  if(DiLevelNei_IIIB(iDir1,jDir1,kDir1,iBlockSend) == 1 .or.&
                       DiLevelNei_IIIB(iDir1,jDir,kdir,iBlockSend) == 1) then

                     !-------------------
                     ! The face values of iBlockSend is not accurate. Do not
                     ! send to iBlockRecv. The edge/corner cells of iBlockRecv
                     ! will be filled by do_prolongation.
                     iSend = (3*iDir + 3)/2
                     jSend = (3*jDir + 3)/2
                     kSend = (3*kDir + 3)/2
                     iNodeRecv  = iNodeNei_IIIB(iSend,jSend,kSend,iBlockSend)

                     iDir2 = -100; jDir2 = -100; kDir2 = -100
                     iDir3 = -100; jDir3 = -100; kDir3 = -100
                     if(is_only_corner_fine(&
                          iNodeRecv,iDir1,0,0,iDir2,jDir2,kDir2) .or. &
                          is_only_corner_fine(&
                          iNode_B(iBlockSend),iDir1,0,0,iDir3,jDir3,kDir3))then

                        if((jDir == -jDir2 .and. kDir == -kDir2) .or. &
                             (jDir == jDir3 .and. kDir == kDir3))&
                             CYCLE
                     endif
                     !-------------------

                     iEqualSOrig_DII = iEqualS_DII
                     iEqualROrig_DII = iEqualR_DII

                     if(iDir1 == -1) then
                        iEqualS_DII(1,0,Min_) = 1-nWidth
                        iEqualS_DII(1,0,Max_) = 0

                        iEqualR_DII(1,0,Min_) = 1-nWidth
                        iEqualR_DII(1,0,Max_) = 0
                     elseif(iDir1 == 1) then
                        iEqualS_DII(1,0,Min_) = nI + 1
                        iEqualS_DII(1,0,Max_) = nI + nWidth

                        iEqualR_DII(1,0,Min_) = nI + 1
                        iEqualR_DII(1,0,Max_) = nI + nWidth
                     endif
                     call do_equal(iDir, jDir, kDir, iNodeSend, iBlockSend,&
                          nVar, nG, State_VGB, DoRemote, IsAxisNode, &
                          iLevelMIn, Time_B, TimeOld_B)
                     iEqualS_DII = iEqualSOrig_DII
                     iEqualR_DII = iEqualROrig_DII
                  endif

               enddo

            elseif(jDir == 0) then
               iDir1 = 0; kDir1 = 0
               do jDir1 = -1, 1, 2
                  if(DiLevelNei_IIIB(iDir1,jDir1,kDir1,iBlockSend) == 1 .or.&
                       DiLevelNei_IIIB(iDir,jDir1,kDir,iBlockSend) == 1) then

                     iSend = (3*iDir + 3)/2
                     jSend = (3*jDir + 3)/2
                     kSend = (3*kDir + 3)/2
                     iNodeRecv  = iNodeNei_IIIB(iSend,jSend,kSend,iBlockSend)

                     iDir2 = -100; jDir2 = -100; kDir2 = -100
                     iDir3 = -100; jDir3 = -100; kDir3 = -100
                     if(is_only_corner_fine(&
                          iNodeRecv,0,jDir1,0,iDir2,jDir2,kDir2) .or. &
                          is_only_corner_fine(&
                          iNode_B(iBlockSend),0,jDir1,0,iDir3,jDir3,kDir3))then

                        if((iDir == -iDir2 .and. kDir == -kDir2) .or. &
                             (iDir == iDir3 .and. kDir == kDir3))&
                             CYCLE
                     endif

                     iEqualSOrig_DII = iEqualS_DII
                     iEqualROrig_DII = iEqualR_DII

                     if(jDir1 == -1) then
                        iEqualS_DII(2,0,Min_) = 1-nWidth
                        iEqualS_DII(2,0,Max_) = 0

                        iEqualR_DII(2,0,Min_) = 1-nWidth
                        iEqualR_DII(2,0,Max_) = 0
                     elseif(jDir1 == 1) then
                        iEqualS_DII(2,0,Min_) = nJ + 1
                        iEqualS_DII(2,0,Max_) = nJ + nWidth

                        iEqualR_DII(2,0,Min_) = nJ + 1
                        iEqualR_DII(2,0,Max_) = nJ + nWidth
                     endif

                     call do_equal(iDir, jDir, kDir, iNodeSend, iBlockSend, &
                          nVar, nG, State_VGB, DoRemote, IsAxisNode, &
                          iLevelMin, Time_B, TimeOld_B)
                     iEqualS_DII = iEqualSOrig_DII
                     iEqualR_DII = iEqualROrig_DII
                  endif
               enddo ! jDir1
            elseif(kDir == 0) then
               iDir1 = 0; jDir1 = 0

               do kDir1 = -1, 1, 2
                  if(DiLevelNei_IIIB(iDir1,jDir1,kDir1,iBlockSend) == 1 .or.&
                       DiLevelNei_IIIB(iDir,jDir,kDir1,iBlockSend) == 1) then

                     iSend = (3*iDir + 3)/2
                     jSend = (3*jDir + 3)/2
                     kSend = (3*kDir + 3)/2
                     iNodeRecv  = iNodeNei_IIIB(iSend,jSend,kSend,iBlockSend)

                     iDir2 = -100; jDir2 = -100; kDir2 = -100
                     iDir3 = -100; jDir3 = -100; kDir3 = -100
                     if(is_only_corner_fine(&
                          iNodeRecv,0,0,kDir1,iDir2,jDir2,kDir2) .or. &
                          is_only_corner_fine(&
                          iNode_B(iBlockSend),0,0,kDir1,iDir3,jDir3,kDir3))then

                        if((jDir == -jDir2 .and. iDir == -iDir2) .or. &
                             (jDir == jDir3 .and. iDir == iDir3))&
                             CYCLE
                     endif

                     iEqualSOrig_DII = iEqualS_DII
                     iEqualROrig_DII = iEqualR_DII

                     if(kDir1 == -1) then
                        iEqualS_DII(3,0,Min_) = 1-nWidth
                        iEqualS_DII(3,0,Max_) = 0

                        iEqualR_DII(3,0,Min_) = 1-nWidth
                        iEqualR_DII(3,0,Max_) = 0
                     elseif(kDir1 == 1) then
                        iEqualS_DII(3,0,Min_) = nK + 1
                        iEqualS_DII(3,0,Max_) = nK + nWidth

                        iEqualR_DII(3,0,Min_) = nK + 1
                        iEqualR_DII(3,0,Max_) = nK + nWidth
                     endif

                     call do_equal(iDir, jDir, kDir, iNodeSend, iBlockSend, &
                          nVar, nG, State_VGB, DoRemote, IsAxisNode, &
                          iLevelMin, Time_B, TimeOld_B)

                     iEqualS_DII = iEqualSOrig_DII
                     iEqualR_DII = iEqualROrig_DII
                  endif
               enddo ! kDir1
            endif
         elseif(nDir == 1) then
            if(iDir /= 0) then
               iDir1 = 0
               do kDir1 = -1, 1; do jDir1 = -1, 1
                  if(kDir1 == 0 .and. jDir1 == 0) CYCLE
                  if(kDir1 /= 0 .and. jDir1 /= 0) CYCLE
                  if(DiLevelNei_IIIB(iDir1,jDir1,kDir1,iBlockSend) == 1 .or. &
                       DiLevelNei_IIIB(iDir,jDir1,kDir1,iBlockSend) == 1) then

                     iSend = (3*iDir + 3)/2
                     jSend = (3*jDir + 3)/2
                     kSend = (3*kDir + 3)/2
                     iNodeRecv  = iNodeNei_IIIB(iSend,jSend,kSend,iBlockSend)

                     iDir2 = -100; jDir2 = -100; kDir2 = -100
                     iDir3 = -100; jDir3 = -100; kDir3 = -100

                     if(is_only_corner_fine(&
                          iNodeRecv,iDir1,jDir1,kDir1,iDir2,jDir2,kDir2) .or. &
                          is_only_corner_fine(&
                          iNode_B(iBlockSend),iDir1,jDir1,kDir1,&
                          iDir3,jDir3,kDir3))then

                        if((iDir == iDir3 .and. &
                             (jDir1 == jDir3 .or. kDir1 == kDir3))&
                             .or. &
                             (iDir == -iDir2 .and. &
                             (jDir1 == jDir2 .or. kDir1 == kDir2))) then
                           CYCLE
                        endif

                     endif

                     iEqualSOrig_DII = iEqualS_DII
                     iEqualROrig_DII = iEqualR_DII

                     if(kDir1 == -1) then
                        iEqualS_DII(3,0,Min_) = 1-nWidth
                        iEqualS_DII(3,0,Max_) = 0

                        iEqualR_DII(3,0,Min_) = 1-nWidth
                        iEqualR_DII(3,0,Max_) = 0
                     elseif(kDir1 == 1) then
                        iEqualS_DII(3,0,Min_) = nK + 1
                        iEqualS_DII(3,0,Max_) = nK + nWidth

                        iEqualR_DII(3,0,Min_) = nK + 1
                        iEqualR_DII(3,0,Max_) = nK + nWidth
                     endif

                     if(jDir1 == -1) then
                        iEqualS_DII(2,0,Min_) = 1-nWidth
                        iEqualS_DII(2,0,Max_) = 0

                        iEqualR_DII(2,0,Min_) = 1-nWidth
                        iEqualR_DII(2,0,Max_) = 0
                     elseif(jDir1 == 1) then
                        iEqualS_DII(2,0,Min_) = nJ + 1
                        iEqualS_DII(2,0,Max_) = nJ + nWidth

                        iEqualR_DII(2,0,Min_) = nJ + 1
                        iEqualR_DII(2,0,Max_) = nJ + nWidth
                     endif
                     call do_equal(iDir, jDir, kDir, iNodeSend, iBlockSend, &
                          nVar, nG, State_VGB, DoRemote, IsAxisNode, &
                          iLevelMin, Time_B, TimeOld_B)
                     iEqualS_DII = iEqualSOrig_DII
                     iEqualR_DII = iEqualROrig_DII
                  endif
               enddo; enddo
            elseif(jDir /= 0) then
               jDir1 = 0
               do kDir1 = -1, 1; do iDir1 = -1, 1
                  if(kDir1 == 0 .and. iDir1 == 0) CYCLE
                  if(kDir1 /= 0 .and. iDir1 /= 0) CYCLE
                  if(DiLevelNei_IIIB(iDir1,jDir1,kDir1,iBlockSend) == 1 .or. &
                       DiLevelNei_IIIB(iDir1,jDir,kDir1,iBlockSend) == 1) then

                     iSend = (3*iDir + 3)/2
                     jSend = (3*jDir + 3)/2
                     kSend = (3*kDir + 3)/2
                     iNodeRecv  = iNodeNei_IIIB(iSend,jSend,kSend,iBlockSend)

                     iDir2 = -100; jDir2 = -100; kDir2 = -100
                     iDir3 = -100; jDir3 = -100; kDir3 = -100

                     if(is_only_corner_fine(&
                          iNodeRecv,iDir1,jDir1,kDir1,iDir2,jDir2,kDir2) .or. &
                          is_only_corner_fine(&
                          iNode_B(iBlockSend),iDir1,jDir1,kDir1,&
                          iDir3,jDir3,kDir3))then

                        if((jDir == jDir3 .and. &
                             (iDir1 == iDir3 .or. kDir1 == kDir3))&
                             .or. &
                             (jDir == -jDir2 .and. &
                             (iDir1 == iDir2 .or. kDir1 == kDir2))) then
                           CYCLE
                        endif
                     endif

                     iEqualSOrig_DII = iEqualS_DII
                     iEqualROrig_DII = iEqualR_DII

                     if(kDir1 == -1) then
                        iEqualS_DII(3,0,Min_) = 1-nWidth
                        iEqualS_DII(3,0,Max_) = 0

                        iEqualR_DII(3,0,Min_) = 1-nWidth
                        iEqualR_DII(3,0,Max_) = 0
                     elseif(kDir1 == 1) then
                        iEqualS_DII(3,0,Min_) = nK + 1
                        iEqualS_DII(3,0,Max_) = nK + nWidth

                        iEqualR_DII(3,0,Min_) = nK + 1
                        iEqualR_DII(3,0,Max_) = nK + nWidth
                     endif

                     if(iDir1 == -1) then
                        iEqualS_DII(1,0,Min_) = 1-nWidth
                        iEqualS_DII(1,0,Max_) = 0

                        iEqualR_DII(1,0,Min_) = 1-nWidth
                        iEqualR_DII(1,0,Max_) = 0
                     elseif(iDir1 == 1) then
                        iEqualS_DII(1,0,Min_) = nI + 1
                        iEqualS_DII(1,0,Max_) = nI + nWidth

                        iEqualR_DII(1,0,Min_) = nI + 1
                        iEqualR_DII(1,0,Max_) = nI + nWidth
                     endif

                     call do_equal(iDir, jDir, kDir, iNodeSend, iBlockSend, &
                          nVar, nG, State_VGB, DoRemote, IsAxisNode, &
                          iLevelMin, Time_B, TimeOld_B)
                     iEqualS_DII = iEqualSOrig_DII
                     iEqualR_DII = iEqualROrig_DII
                  endif
               enddo; enddo

            elseif(kDir /= 0) then
               kDir1 = 0
               do jDir1 = -1, 1; do iDir1 = -1, 1
                  if(jDir1 == 0 .and. iDir1 == 0) CYCLE
                  if(jDir1 /= 0 .and. iDir1 /= 0) CYCLE
                  if(DiLevelNei_IIIB(iDir1,jDir1,kDir1,iBlockSend) == 1 .or. &
                       DiLevelNei_IIIB(iDir1,jDir1,kDir,iBlockSend) == 1) then

                     iSend = (3*iDir + 3)/2
                     jSend = (3*jDir + 3)/2
                     kSend = (3*kDir + 3)/2
                     iNodeRecv  = iNodeNei_IIIB(iSend,jSend,kSend,iBlockSend)

                     iDir2 = -100; jDir2 = -100; kDir2 = -100
                     iDir3 = -100; jDir3 = -100; kDir3 = -100

                     if(is_only_corner_fine(&
                          iNodeRecv,iDir1,jDir1,kDir1,iDir2,jDir2,kDir2) .or. &
                          is_only_corner_fine(&
                          iNode_B(iBlockSend),iDir1,jDir1,kDir1,&
                          iDir3,jDir3,kDir3))then

                        if((kDir == kDir3 .and. &
                             (iDir1 == iDir3 .or. jDir1 == jDir3))&
                             .or. &
                             (kDir == -kDir2 .and. &
                             (iDir1 == iDir2 .or. jDir1 == jDir2))) then
                           CYCLE
                        endif
                     endif

                     iEqualSOrig_DII = iEqualS_DII
                     iEqualROrig_DII = iEqualR_DII

                     if(jDir1 == -1) then
                        iEqualS_DII(2,0,Min_) = 1-nWidth
                        iEqualS_DII(2,0,Max_) = 0

                        iEqualR_DII(2,0,Min_) = 1-nWidth
                        iEqualR_DII(2,0,Max_) = 0
                     elseif(jDir1 == 1) then
                        iEqualS_DII(2,0,Min_) = nJ + 1
                        iEqualS_DII(2,0,Max_) = nJ + nWidth

                        iEqualR_DII(2,0,Min_) = nJ + 1
                        iEqualR_DII(2,0,Max_) = nJ + nWidth
                     endif

                     if(iDir1 == -1) then
                        iEqualS_DII(1,0,Min_) = 1-nWidth
                        iEqualS_DII(1,0,Max_) = 0

                        iEqualR_DII(1,0,Min_) = 1-nWidth
                        iEqualR_DII(1,0,Max_) = 0
                     elseif(iDir1 == 1) then
                        iEqualS_DII(1,0,Min_) = nI + 1
                        iEqualS_DII(1,0,Max_) = nI + nWidth

                        iEqualR_DII(1,0,Min_) = nI + 1
                        iEqualR_DII(1,0,Max_) = nI + nWidth
                     endif

                     call do_equal(iDir, jDir, kDir, iNodeSend, iBlockSend, &
                          nVar, nG, State_VGB, DoRemote, IsAxisNode, &
                          iLevelMin, Time_B, TimeOld_B)
                     iEqualS_DII = iEqualSOrig_DII
                     iEqualR_DII = iEqualROrig_DII
                  endif
               enddo; enddo
            endif
         endif
      endif ! nDim

    end subroutine corrected_do_equal
    !==========================================================================
    subroutine do_equal(iDir, jDir, kDir, iNodeSend, iBlockSend, nVar, nG, &
         State_VGB, DoRemote, IsAxisNode, iLevelMIn, Time_B, TimeOld_B)

      use BATL_size, ONLY: MaxBlock, nI, nJ, nK, jDim_, kDim_
      use BATL_mpi, ONLY: iProc

      integer, intent(in):: iDir, jDir, kDir, iNodeSend, iBlockSend, nVar, nG
      real, intent(inout):: State_VGB(nVar,&
           1-nG:nI+nG,1-nG*jDim_:nJ+nG*jDim_,1-nG*kDim_:nK+nG*kDim_,MaxBlock)

      logical, intent(in):: DoRemote, IsAxisNode
      integer, optional, intent(in):: iLevelMin
      real,    optional, intent(in):: Time_B(MaxBlock)
      real,    optional, intent(in):: TimeOld_B(MaxBlock)

      integer :: iBufferS, iVarS, i, j, k, nSize, nWithin
      real    :: WeightOld, WeightNew

      integer :: iSend, jSend, kSend
      integer :: iBlockRecv, iProcRecv, iNodeRecv
      ! Index range for recv and send segments of the blocks
      integer :: iRMin, iRMax, jRMin, jRMax, kRMin, kRMax
      integer :: iSMin, iSMax, jSMin, jSMax, kSMin, kSMax
      ! Message passing across the pole can reverse the recv. index range
      integer :: DiR, DjR, DkR

      character(len=*), parameter:: NameSub = 'do_equal'
      !------------------------------------------------------------------------
      DiR = 1; DjR = 1; DkR = 1

      iSend = (3*iDir + 3)/2
      jSend = (3*jDir + 3)/2
      kSend = (3*kDir + 3)/2

      iNodeRecv  = iNodeNei_IIIB(iSend,jSend,kSend,iBlockSend)

      iProcRecv  = iTree_IA(Proc_,iNodeRecv)

      if(iProc == iProcRecv .eqv. DoRemote) RETURN

      iBlockRecv = iTree_IA(Block_,iNodeRecv)

      ! Skip blocks with a time level outside the range
      if(UseTimeLevel .and. present(iLevelMin))then
         if(  iTimeLevel_A(iNodeRecv) < iLevelMin .and. &
              iTimeLevel_A(iNodeSend) < iLevelMin) RETURN
      end if

      ! For part implicit and part steady schemes
      if(Unused_BP(iBlockRecv,iProcRecv)) RETURN

      ! No need to count data for local copy
      if(DoCountOnly .and. iProc == iProcRecv) RETURN

      iRMin = iEqualR_DII(1,iDir,Min_)
      iRMax = iEqualR_DII(1,iDir,Max_)
      jRMin = iEqualR_DII(2,jDir,Min_)
      jRMax = iEqualR_DII(2,jDir,Max_)
      kRMin = iEqualR_DII(3,kDir,Min_)
      kRMax = iEqualR_DII(3,kDir,Max_)

      ! OpenACC: For 2nd and 1st order scheme, iSendStage can not be 3.
      if(iSendStage == 3) then
         ! Only edge/corner cells need to be overwritten.
         nWithin = 0
         if(.not.(iRMin >= 0 .and. iRMin <= nI)) nWithin = nWithin + 1
         if(.not.(jRMin >= 0 .and. jRMin <= nJ)) nWithin = nWithin + 1
         if(.not.(kRMin >= 0 .and. kRMin <= nK)) nWithin = nWithin + 1
         if(nWithin < 1) RETURN
      endif

      ! OpenAcc: For local copy, DoCountOnly is always false.
      if(DoCountOnly)then
         ! Number of reals to send to and received from the other processor
         nSize = nVar*(iRMax-iRMin+1)*(jRMax-jRMin+1)*(kRMax-kRMin+1) &
              + 1 + 2*nDim
         if(present(Time_B)) nSize = nSize + 1
         nBufferR_P(iProcRecv) = nBufferR_P(iProcRecv) + nSize
         nBufferS_P(iProcRecv) = nBufferS_P(iProcRecv) + nSize
         RETURN
      end if

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

      if(iProc == iProcRecv)then
         ! Local copy
         if(nDim > 1) DiR = sign(1, iRMax - iRMin)
         if(nDim > 2) DjR = sign(1, jRMax - jRMin)
         if(nDim > 2) DkR = sign(1, kRMax - kRMin)

         if(present(Time_B)) &
              UseTime = (Time_B(iBlockSend) /= Time_B(iBlockRecv))

         if(UseTime)then
            ! Time interpolation
            WeightOld = (Time_B(iBlockSend) - Time_B(iBlockRecv)) &
                 /      (Time_B(iBlockSend) - TimeOld_B(iBlockRecv))
            WeightNew = 1 - WeightOld
            State_VGB(:,iRMin:iRMax:DiR,jRMin:jRMax:DjR,kRMin:kRMax:DkR,&
                 iBlockRecv) = WeightOld * &
                 State_VGB(:,iRMin:iRMax:DiR,jRMin:jRMax:DjR,kRMin:kRMax:DkR, &
                 iBlockRecv) + WeightNew * &
                 State_VGB(:,iSMin:iSMax,jSMin:jSMax,kSMin:kSMax,iBlockSend)
         else
            State_VGB(:,iRMin:iRMax:DiR,jRMin:jRMax:DjR,kRMin:kRMax:DkR,&
                 iBlockRecv)= &
                 State_VGB(:,iSMin:iSMax,jSMin:jSMax,kSMin:kSMax,iBlockSend)
         end if
      else
         ! Put data into the send buffer
         iBufferS = iBufferS_P(iProcRecv)

         BufferS_I(            iBufferS+1) = iBlockRecv
         BufferS_I(            iBufferS+2) = iRMin
         BufferS_I(            iBufferS+3) = iRMax
         if(nDim > 1)BufferS_I(iBufferS+4) = jRMin
         if(nDim > 1)BufferS_I(iBufferS+5) = jRMax
         if(nDim > 2)BufferS_I(iBufferS+6) = kRMin
         if(nDim > 2)BufferS_I(iBufferS+7) = kRMax

         iBufferS = iBufferS + 1 + 2*nDim

         if(present(Time_B))then
            iBufferS = iBufferS + 1
            BufferS_I(iBufferS) = Time_B(iBlockSend)
         end if

         do k = kSMin,kSmax; do j = jSMin,jSMax; do i = iSMin,iSmax
            do iVarS = 1, nVar
               BufferS_I(iBufferS+iVarS) = State_VGB(iVarS,i,j,k,iBlockSend)
            end do
            iBufferS = iBufferS + nVar
         end do; end do; end do

         iBufferS_P(iProcRecv) = iBufferS
      end if

    end subroutine do_equal
    !==========================================================================
    subroutine do_restrict(iDir, jDir, kDir, iNodeSend, iBlockSend, nVar, nG, &
         State_VGB, DoRemote, IsAxisNode, iLevelMIn, Time_B, TimeOld_B)

      use BATL_mpi, ONLY: iProc
      use BATL_size, ONLY: MaxBlock, nI, nJ, nK, jDim_, kDim_

      integer, intent(in):: iDir, jDir, kDir, iNodeSend, iBlockSend, nVar, nG
      real, intent(inout):: State_VGB(nVar,&
           1-nG:nI+nG,1-nG*jDim_:nJ+nG*jDim_,1-nG*kDim_:nK+nG*kDim_,MaxBlock)

      logical, intent(in):: DoRemote, IsAxisNode
      integer, optional, intent(in):: iLevelMin
      real,    optional, intent(in):: Time_B(MaxBlock)
      real,    optional, intent(in):: TimeOld_B(MaxBlock)

      integer :: iR, jR, kR, iS1, jS1, kS1, iS2, jS2, kS2, iVar
      integer :: iRatioRestr, jRatioRestr, kRatioRestr
      real    :: InvIjkRatioRestr
      integer :: iBufferS, nSize
      real    :: WeightOld, WeightNew

      real, allocatable:: State_VG(:,:,:,:)

      integer :: iSend,jSend,kSend,iRecv,jRecv,kRecv,iSide,jSide,kSide
      integer :: iBlockRecv,iProcRecv,iNodeRecv
      ! Index range for recv and send segments of the blocks
      integer :: iRMin, iRMax, jRMin, jRMax, kRMin, kRMax
      integer :: iSMin, iSMax, jSMin, jSMax, kSMin, kSMax

      ! Message passing across the pole can reverse the recv. index range
      integer :: DiR, DjR, DkR
      !------------------------------------------------------------------------
      DiR = 1; DjR = 1; DkR = 1

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

      ! Skip blocks with a time level outside the range
      if(UseTimeLevel .and. present(iLevelMin))then
         if(  iTimeLevel_A(iNodeRecv) < iLevelMin .and. &
              iTimeLevel_A(iNodeSend) < iLevelMin) RETURN
      end if

      iProcRecv  = iTree_IA(Proc_,iNodeRecv)

      if(iProc == iProcRecv .eqv. DoRemote) RETURN

      iBlockRecv = iTree_IA(Block_,iNodeRecv)

      if(iSendStage == 4 .and. nK > 1 .and. &
           abs(iDir)+abs(jDir)+abs(kDir) == 1) then
         DoRecvFace = is_only_corner_fine(iNode_B(iBlockSend),iDir,jDir,kDir)
         if(.not.DoRecvFace) RETURN
      endif

      ! For part implicit and part steady schemes
      if(Unused_BP(iBlockRecv,iProcRecv)) RETURN

      ! No need to count data for local copy
      if(DoCountOnly .and. iProc == iProcRecv) RETURN

      if(DoCountOnly .and. (&
           (.not. UseHighResChange .and. iSendStage == nProlongOrder) .or. &
           (UseHighResChange .and. (iSendStage == 1 .or. iSendStage == 4)))) &
           then
         ! This part is unused when nProc == 1

         ! For high resolution change, finer block only receives data
         ! when iSendStage = 1.

         ! This processor will receive a prolonged buffer from
         ! the other processor and the "recv" direction of the prolongations
         ! will be the same as the "send" direction for this restriction:
         ! iSend,kSend,jSend = 0..3
         iRMin = iProlongR_DII(1,iSend,Min_)
         iRMax = iProlongR_DII(1,iSend,Max_)
         jRMin = iProlongR_DII(2,jSend,Min_)
         jRMax = iProlongR_DII(2,jSend,Max_)
         kRMin = iProlongR_DII(3,kSend,Min_)
         kRMax = iProlongR_DII(3,kSend,Max_)

         nSize = nVar*(iRMax-iRMin+1)*(jRMax-jRMin+1)*(kRMax-kRMin+1) &
              + 1 + 2*nDim
         if(present(Time_B)) nSize = nSize + 1
         nBufferR_P(iProcRecv) = nBufferR_P(iProcRecv) + nSize

      end if

      ! If this is the pure prolongation stage, all we did was counting
      if(iSendStage == 2 .and. .not. UseHighResChange) RETURN

      ! For high resolution change, the finer block receives data from
      ! coarser or equal blocks when iSendStage = 1. Restriction will
      ! be done when iSendStage = 2.
      if(UseHighResChange .and. iSendStage == 1) RETURN

      ! Do prolongation for edge/corner ghost cells remotely.
      if(UseHighResChange .and. iSendStage == 4) RETURN

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

      if(DoCountOnly)then
         ! This part is unused when nProc == 1

         ! Number of reals to send to the other processor
         nSize = nVar*(iRMax-iRMin+1)*(jRMax-jRMin+1)*(kRMax-kRMin+1) &
              + 1 + 2*nDim
         if(present(Time_B)) nSize = nSize + 1
         nBufferS_P(iProcRecv) = nBufferS_P(iProcRecv) + nSize
         RETURN
      end if

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

      if(iProc == iProcRecv)then

         if(present(Time_B)) &
              UseTime = (Time_B(iBlockSend) /= Time_B(iBlockRecv))
         if(UseTime)then
            ! Get time of neighbor and interpolate/extrapolate ghost cells
            WeightOld = (Time_B(iBlockSend) - Time_B(iBlockRecv)) &
                 /      (Time_B(iBlockSend) - TimeOld_B(iBlockRecv))
            WeightNew = 1 - WeightOld

            do kR = kRMin, kRMax, DkR
               do jR = jRMin, jRMax, DjR
                  do iR = iRMin, iRMax, DiR
                     kS1 = kSMin + kRatioRestr*abs(kR-kRMin)
                     kS2 = kS1 + kRatioRestr - 1

                     jS1 = jSMin + jRatioRestr*abs(jR-jRMin)
                     jS2 = jS1 + jRatioRestr - 1

                     iS1 = iSMin + iRatioRestr*abs(iR-iRMin)
                     iS2 = iS1 + iRatioRestr - 1
                     if(UseMin) then
                        do iVar = 1, nVar
                           State_VGB(iVar,iR,jR,kR,iBlockRecv) = &
                                minval(State_VGB(iVar,iS1:iS2,jS1:jS2,kS1:kS2,&
                                iBlockSend))
                        end do
                     else if(UseMax) then
                        do iVar = 1, nVar
                           State_VGB(iVar,iR,jR,kR,iBlockRecv) = &
                                maxval(State_VGB(iVar,iS1:iS2,jS1:jS2,kS1:kS2,&
                                iBlockSend))
                        end do
                     else
                        do iVar = 1, nVar
                           State_VGB(iVar,iR,jR,kR,iBlockRecv) = &
                                WeightOld*State_VGB(iVar,iR,jR,kR,iBlockRecv)+&
                                WeightNew*InvIjkRatioRestr * &
                                sum(State_VGB(iVar,iS1:iS2,jS1:jS2,kS1:kS2,&
                                iBlockSend))
                        end do
                     end if
                  end do
               end do
            end do
         else
            ! No time interpolation/extrapolation is needed
            if(UseHighResChange) then
               if(.not.IsAccurate_B(iBlockSend)) &
                    call calc_accurate_coarsened_block(iBlockSend)
               do kR = kRMin, kRMax, DkR
                  kS1 = kSMin + kRatioRestr*abs(kR-kRMin)
                  do jR = jRMin, jRMax, DjR
                     jS1 = jSMin + jRatioRestr*abs(jR-jRMin)
                     do iR = iRMin, iRMax, DiR
                        iS1 = iSMin + iRatioRestr*abs(iR-iRMin)
                        do iVar = 1, nVar
                           State_VGB(iVar,iR,jR,kR,iBlockRecv) = &
                                State_VIIIB(iVar,(iS1+1)/2,(jS1+1)/2,&
                                (kS1+1)/2,iBlockSend)
                        end do
                     enddo
                  enddo
               enddo
            else
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
                                   State_VGB(iVar,iS1:iS2,jS1:jS2,kS1:kS2,&
                                   iBlockSend))
                           end do
                        else if(UseMax) then
                           do iVar = 1, nVar
                              State_VGB(iVar,iR,jR,kR,iBlockRecv) = maxval( &
                                   State_VGB(iVar,iS1:iS2,jS1:jS2,kS1:kS2,&
                                iBlockSend))
                           end do
                        else
                           do iVar = 1, nVar
                              State_VGB(iVar,iR,jR,kR,iBlockRecv) = &
                                   InvIjkRatioRestr * sum( &
                                   State_VGB(iVar,iS1:iS2,jS1:jS2,kS1:kS2, &
                                   iBlockSend))
                           end do
                        end if
                     end do ! iR
                  end do ! jR
               end do ! kR
            end if ! UseHighResChange
         end if ! UseTime
      else ! iProc /= iProcRecv
         if(UseHighResChange) then
            if(.not.IsAccurate_B(iBlockSend)) &
                 call calc_accurate_coarsened_block(iBlockSend)
            if(.not. allocated(State_VG)) then
               allocate(State_VG(nVar,MinI:MaxI,MinJ:MaxJ,MinK:MaxK))
               State_VG = 1
            endif

            do kR = kRMin, kRMax, DkR
               kS1 = kSMin + kRatioRestr*abs(kR-kRMin)
               do jR = jRMin, jRMax, DjR
                  jS1 = jSMin + jRatioRestr*abs(jR-jRMin)
                  do iR = iRMin, iRMax, DiR
                     iS1 = iSMin + iRatioRestr*abs(iR-iRMin)
                     do iVar = 1, nVar
                        State_VG(iVar,iR,jR,kR) = &
                             State_VIIIB(iVar,(iS1+1)/2,(jS1+1)/2,(kS1+1)/2,&
                             iBlockSend)
                     end do
                  enddo
               enddo
            enddo
         endif

         iBufferS = iBufferS_P(iProcRecv)

         BufferS_I(            iBufferS+1) = iBlockRecv
         BufferS_I(            iBufferS+2) = iRMin
         BufferS_I(            iBufferS+3) = iRMax
         if(nDim > 1)BufferS_I(iBufferS+4) = jRMin
         if(nDim > 1)BufferS_I(iBufferS+5) = jRMax
         if(nDim > 2)BufferS_I(iBufferS+6) = kRMin
         if(nDim > 2)BufferS_I(iBufferS+7) = kRMax

         iBufferS = iBufferS + 1 + 2*nDim

         if(present(Time_B))then
            iBufferS = iBufferS + 1
            BufferS_I(iBufferS) = Time_B(iBlockSend)
         end if

         do kR = kRMin, kRMax, DkR
            kS1 = kSMin + kRatioRestr*abs(kR-kRMin)
            kS2 = kS1 + kRatioRestr - 1
            do jR = jRMin, jRMax, DjR
               jS1 = jSMin + jRatioRestr*abs(jR-jRMin)
               jS2 = jS1 + jRatioRestr - 1
               do iR = iRMin, iRMax, DiR
                  iS1 = iSMin + iRatioRestr*abs(iR-iRMin)
                  iS2 = iS1 + iRatioRestr - 1
                  if(UseHighResChange) then
                     do iVar = 1, nVar
                        BufferS_I(iBufferS+iVar) = State_VG(iVar,iR,jR,kR)
                     end do
                  else if(UseMin) then
                     do iVar = 1, nVar
                        BufferS_I(iBufferS+iVar) = &
                             minval(State_VGB(iVar,iS1:iS2,jS1:jS2,kS1:kS2,&
                             iBlockSend))
                     end do
                  else if(UseMax) then
                     do iVar = 1, nVar
                        BufferS_I(iBufferS+iVar) = &
                             maxval(State_VGB(iVar,iS1:iS2,jS1:jS2,kS1:kS2,&
                             iBlockSend))
                     end do
                  else
                     do iVar = 1, nVar
                        BufferS_I(iBufferS+iVar) = &
                             InvIjkRatioRestr * &
                             sum(State_VGB(iVar,iS1:iS2,jS1:jS2,kS1:kS2,&
                             iBlockSend))
                     end do
                  end if
                  iBufferS = iBufferS + nVar
               end do
            end do
         end do
         iBufferS_P(iProcRecv) = iBufferS
      end if

    end subroutine do_restrict
    !==========================================================================
    subroutine do_prolong(iDir, jDir, kDir, iNodeSend, iBlockSend, nVar, nG, &
         State_VGB, DoRemote, IsAxisNode, iLevelMIn, Time_B, TimeOld_B)

      use BATL_size, ONLY: nDimAmr
      use ModCoordTransform, ONLY: cross_product
      use BATL_tree, ONLY: get_tree_position
      use BATL_mpi, ONLY: iProc
      integer, intent(in):: iDir, jDir, kDir, iNodeSend, iBlockSend, nVar, nG
      real, intent(inout):: State_VGB(nVar,&
           1-nG:nI+nG,1-nG*jDim_:nJ+nG*jDim_,1-nG*kDim_:nK+nG*kDim_,MaxBlock)

      logical, intent(in):: DoRemote, IsAxisNode
      integer, optional, intent(in):: iLevelMin
      real,    optional, intent(in):: Time_B(MaxBlock)
      real,    optional, intent(in):: TimeOld_B(MaxBlock)

      integer :: iR, jR, kR, iS, jS, kS, iS1, jS1, kS1
      integer :: iRatioRestr, jRatioRestr, kRatioRestr
      integer :: iBufferS, nSize
      integer, parameter:: Di=iRatio-1, Dj=jRatio-1, Dk=kRatio-1
      real    :: WeightOld, WeightNew, Weight, WeightI, WeightJ, WeightK, InvV
      real, dimension(MaxDim):: Xyz_D, dI_D, dJ_D, dK_D, dR_D, &
           PositionMinR_D, PositionMaxR_D, CoordMinR_D, CoordMaxR_D, &
           CellSizeR_D, CoordR_D

      ! Slopes for 2nd order prolongation.
      real :: Slope_VG(nVar,1-nWidth:nI+nWidth,&
           1-nWidth*jDim_:nJ+nWidth*jDim_,1-nWidth*kDim_:nK+nWidth*kDim_)

      logical :: UseSimpleWeights

      integer :: iVar
      integer:: nWidthProlongS_D(MaxDim), iDim
      real:: CoarseCell_III(5,5,5)
      integer:: i5,j5,k5, iDir1, jDir1, kDir1

      integer :: iSend,jSend,kSend,iRecv,jRecv,kRecv,iSide,jSide,kSide
      integer :: iBlockRecv, iProcRecv, iNodeRecv

      ! Index range for recv and send segments of the blocks
      integer :: iRMin, iRMax, jRMin, jRMax, kRMin, kRMax
      integer :: iSMin, iSMax, jSMin, jSMax, kSMin, kSMax

      ! Message passing across the pole can reverse the recv. index range
      integer :: DiR, DjR, DkR
      !------------------------------------------------------------------------
      DiR = 1; DjR = 1; DkR = 1

      UseSimpleWeights = UseSimpleProlongation .or. &
           nDim == 1 .or. nDimAmr < nDim &
           .or. IsCartesianGrid .or. IsRotatedCartesian .or. IsRoundCube

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

               ! Skip blocks with a time level outside the range
               if(UseTimeLevel .and. present(iLevelMin))then
                  if(  iTimeLevel_A(iNodeRecv) < iLevelMin .and. &
                       iTimeLevel_A(iNodeSend) < iLevelMin) CYCLE
               end if

               iProcRecv  = iTree_IA(Proc_,iNodeRecv)

               if(iProc == iProcRecv .eqv. DoRemote) CYCLE

               iBlockRecv = iTree_IA(Block_,iNodeRecv)

               if(iSendStage == 4 .and. nK > 1 .and. &
                    abs(iDir)+abs(jDir)+abs(kDir) == 1 ) then
                  ! Do_prolongation for edge/corner ghost cells and for
                  ! some special face cells.
                  DoSendFace = is_only_corner_fine(iNodeRecv,-iDir,-jDir,-kDir)
                  if(.not. DoSendFace) CYCLE
               endif

               ! For part implicit and part steady schemes
               if(Unused_BP(iBlockRecv,iProcRecv)) CYCLE

               ! No need to count data for local copy
               if(DoCountOnly .and. iProc == iProcRecv) CYCLE

               if(DoCountOnly .and. (.not. UseHighResChange .and. &
                    iSendStage == 1 .or. &
                    (UseHighResChange .and. iSendStage == 2)))then
                  ! This processor will receive a restricted buffer from
                  ! the other processor and the "recv" direction of the
                  ! restriction will be the same as the "send" direction for
                  ! this prolongation: iSend,kSend,jSend = 0..3
                  iRMin = iRestrictR_DII(1,iSend,Min_)
                  iRMax = iRestrictR_DII(1,iSend,Max_)
                  jRMin = iRestrictR_DII(2,jSend,Min_)
                  jRMax = iRestrictR_DII(2,jSend,Max_)
                  kRMin = iRestrictR_DII(3,kSend,Min_)
                  kRMax = iRestrictR_DII(3,kSend,Max_)

                  nSize = nVar*(iRMax-iRMin+1)*(jRMax-jRMin+1)*(kRMax-kRMin+1)&
                       + 1 + 2*nDim
                  if(present(Time_B)) nSize = nSize + 1
                  nBufferR_P(iProcRecv) = nBufferR_P(iProcRecv) + nSize
               end if

               ! For 2nd order prolongation no prolongation is done in stage 1
               if(.not. UseHighResChange .and. iSendStage < nProlongOrder) &
                    CYCLE

               ! For HighResChange, only do restriction in stage 2.
               if(UseHighResChange .and. iSendStage == 2) CYCLE

               ! Receiving range depends on iRecv,kRecv,jRecv = 0..3
               iRMin = iProlongR_DII(1,iRecv,Min_)
               iRMax = iProlongR_DII(1,iRecv,Max_)
               jRMin = iProlongR_DII(2,jRecv,Min_)
               jRMax = iProlongR_DII(2,jRecv,Max_)
               kRMin = iProlongR_DII(3,kRecv,Min_)
               kRMax = iProlongR_DII(3,kRecv,Max_)

               if(DoCountOnly)then
                  ! Number of reals to send to the other processor
                  nSize = nVar*(iRMax-iRMin+1)*(jRMax-jRMin+1)*(kRMax-kRMin+1)&
                       + 1 + 2*nDim
                  if(present(Time_B)) nSize = nSize + 1
                  nBufferS_P(iProcRecv) = nBufferS_P(iProcRecv) + nSize
                  CYCLE
               end if

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

               if(nDim > 1) DiR = sign(1, iRMax - iRMin)
               if(nDim > 2) DjR = sign(1, jRMax - jRMin)
               if(nDim > 2) DkR = sign(1, kRMax - kRMin)

               if(UseHighResChange .and. iSendStage == 4) then
                  ! The values set in set_range are used for iSendStage == 1,
                  ! Which is first order prolongation. Now, for high order
                  ! prolongation, some values need to be corrected.
                  nWidthProlongS_D(1:nDim) = 1 + (nWidth-1)/iRatio_D(1:nDim)

                  iProlongS_DII(:,0,Min_) = 1
                  iProlongS_DII(:,0,Max_) = nWidthProlongS_D
                  iProlongS_DII(:,1,Min_) = 1
                  iProlongS_DII(:,1,Max_) = nIjk_D/iRatio_D
                  iProlongS_DII(:,2,Min_) = nIjk_D/iRatio_D + 1
                  iProlongS_DII(:,2,Max_) = nIjk_D
                  iProlongS_DII(:,3,Min_) = nIjk_D + 1 - nWidthProlongS_D
                  iProlongS_DII(:,3,Max_) = nIjk_D

                  if(DoSendCorner)then
                     ! Face + two edges + corner or edge + one corner
                     ! are sent/recv together from fine to coarse block
                     do iDim = 1, nDim
                        if(iRatio_D(iDim) == 1)CYCLE
                        ! The extension is by nWidth/2 rounded upwards
                        ! independent of
                        ! the value of nCoarseLayers. There is no need to send
                        ! two coarse layers into corner/edge ghost cells.
                        iProlongS_DII(iDim,1,Max_) = &
                             iProlongS_DII(iDim,1,Max_) + (nWidth+1)/2
                        iProlongS_DII(iDim,2,Min_) = &
                             iProlongS_DII(iDim,2,Min_) - (nWidth+1)/2
                     end do
                  end if
               endif

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

               ! CPU version
               Slope_VG = 0.0
               if(nProlongOrder == 2)then
                  ! Add up 2nd order corrections for all AMR dimensions
                  ! Use simple interpolation, should be OK for ghost cells

                  if(.not.UseSimpleWeights .and. iProcRecv /= iProc)then
                     call get_tree_position(iNodeRecv, &
                          PositionMinR_D, PositionMaxR_D)
                     CoordMinR_D = CoordMin_D + DomainSize_D*PositionMinR_D
                     CoordMaxR_D = CoordMin_D + DomainSize_D*PositionMaxR_D
                     CellSizeR_D = (CoordMaxR_D - CoordMinR_D)/nIjk_D
                  end if

                  do kR = kRMin, kRMax, DkR
                     do jR = jRMin, jRMax, DjR
                        do iR = iRMin, iRMax, DiR
                           ! For kRatio = 1 simple shift:
                           ! kS = kSMin + |kR - kRMin|
                           ! For kRatio = 2 coarsen both
                           ! kR and kRMin before shift
                           ! We add 9 both to kR and kRMin
                           ! before dividing by kRatio
                           ! so that all values remain positive
                           ! and get rounded down.
                           ! This works up to nG=10 ghost cells:
                           ! likely to be enough.
                           kS = kSMin + abs((kR+9)/kRatioRestr &
                                -           (kRMin+9)/kRatioRestr)
                           ! DkR=+1:
                           ! interpolate left for odd kR, right for even kR
                           ! DkR=-1:
                           ! interpolate left for even kR, right for odd kR
                           if(kRatio == 1) kS1 = kS
                           if(kRatio == 2) kS1 = kS + DkR*(1 - 2*modulo(kR,2))

                           jS = jSMin + abs((jR+9)/jRatioRestr &
                                -           (jRMin+9)/jRatioRestr)
                           if(jRatio == 1) jS1 = jS
                           if(jRatio == 2) jS1 = jS + DjR*(1 - 2*modulo(jR,2))

                           iS = iSMin + abs((iR+9)/iRatioRestr &
                                -           (iRMin+9)/iRatioRestr)
                           if(iRatio == 1) iS1 = iS
                           if(iRatio == 2) iS1 = iS + DiR*(1 - 2*modulo(iR,2))

                           if(UseMin)then
                              ! Store min value of the stencil into Slope_VG
                              Slope_VG(:,iR,jR,kR) = min( &
                                   State_VGB(:,iS,jS,kS,iBlockSend), &
                                   State_VGB(:,iS1,jS,kS,iBlockSend), &
                                   State_VGB(:,iS,jS1,kS,iBlockSend), &
                                   State_VGB(:,iS,jS,kS1,iBlockSend)  )
                              CYCLE
                           elseif(UseMax)then
                              ! Store max value of the stencil into Slope_VG
                              Slope_VG(:,iR,jR,kR) = max( &
                                   State_VGB(:,iS,jS,kS,iBlockSend), &
                                   State_VGB(:,iS1,jS,kS,iBlockSend), &
                                   State_VGB(:,iS,jS1,kS,iBlockSend), &
                                   State_VGB(:,iS,jS,kS1,iBlockSend)  )
                              CYCLE
                           end if

                           if(UseSimpleWeights)then
                              ! For Cartesian-like grids the weights are 0.25
                              if(iRatio == 2) WeightI = 0.25
                              if(jRatio == 2) WeightJ = 0.25
                              if(kRatio == 2) WeightK = 0.25
                           else
                              ! The weights are area/volume fractions
                              Xyz_D= Xyz_DGB(:,iS,jS,kS,iBlockSend)
                              dI_D = Xyz_DGB(:,iS1,jS,kS,iBlockSend) - Xyz_D
                              dJ_D = Xyz_DGB(:,iS,jS1,kS,iBlockSend) - Xyz_D
                              dK_D = Xyz_DGB(:,iS,jS,kS1,iBlockSend) - Xyz_D

                              if(iProcRecv == iProc)then
                                 dR_D = Xyz_DGB(:,iR,jR,kR,iBlockRecv) - Xyz_D
                              else
                                 CoordR_D = CoordMinR_D + &
                                      ([iR,jR,kR] - 0.5)*CellSizeR_D
                                 call coord_to_xyz(CoordR_D, dR_D)
                                 dR_D = dR_D - Xyz_D
                              end if

                              ! The max(0.0, and the normalization to 1
                              ! avoids extrapolation when the
                              ! receiving point is outside the sending
                              ! polyhedron. Remove these for exact
                              ! second order test.
                              if(nDim == 2)then
                                 InvV = 1/ &
                                      (dI_D(1)*dJ_D(2)-dI_D(2)*dJ_D(1))
                                 WeightI = max(0.0, InvV* &
                                      (dR_D(1)*dJ_D(2)-dR_D(2)*dJ_D(1)))
                                 WeightJ = max(0.0, InvV* &
                                      (dI_D(1)*dR_D(2)-dI_D(2)*dR_D(1)))
                                 Weight = WeightI + WeightJ
                                 if(Weight > 1)then
                                    WeightI = WeightI / Weight
                                    WeightJ = WeightJ / Weight
                                 end if
                              else
                                 InvV = 1/ &
                                      sum(dI_D*cross_product(dJ_D,dK_D))
                                 WeightI = max(0.0, InvV* &
                                      sum(dR_D*cross_product(dJ_D,dK_D)))
                                 WeightJ = max(0.0, InvV* &
                                      sum(dI_D*cross_product(dR_D,dK_D)))
                                 WeightK = max(0.0, InvV* &
                                      sum(dI_D*cross_product(dJ_D,dR_D)))

                                 Weight  = WeightI + WeightJ + WeightK
                                 if(Weight > 1.0)then
                                    WeightI = WeightI / Weight
                                    WeightJ = WeightJ / Weight
                                    WeightK = WeightK / Weight
                                 end if

                              end if
                           end if

                           if(iRatio == 2) Slope_VG(:,iR,jR,kR) = &
                                Slope_VG(:,iR,jR,kR) + WeightI* &
                                ( State_VGB(:,iS1,jS,kS,iBlockSend) &
                                - State_VGB(:,iS ,jS,kS,iBlockSend) )

                           if(jRatio == 2) Slope_VG(:,iR,jR,kR) = &
                                Slope_VG(:,iR,jR,kR) + WeightJ* &
                                ( State_VGB(:,iS,jS1,kS,iBlockSend) &
                                - State_VGB(:,iS,jS ,kS,iBlockSend) )

                           if(kRatio == 2) Slope_VG(:,iR,jR,kR) = &
                                Slope_VG(:,iR,jR,kR) + WeightK* &
                                ( State_VGB(:,iS,jS,kS1,iBlockSend) &
                                - State_VGB(:,iS,jS,kS ,iBlockSend) )
                        end do
                     end do
                  end do
               end if ! nProlongOrder = 2

               if(iProc == iProcRecv)then

                  if(present(Time_B)) &
                       UseTime = (Time_B(iBlockSend) /= Time_B(iBlockRecv))
                  if(UseTime)then
                     ! Interpolate/extrapolate ghost cells in time
                     WeightOld = (Time_B(iBlockSend) - Time_B(iBlockRecv)) &
                          /      (Time_B(iBlockSend) - TimeOld_B(iBlockRecv))
                     WeightNew = 1 - WeightOld

                     do kR = kRMin, kRMax, DkR
                        do jR = jRMin, jRMax, DjR
                           do iR = iRMin, iRMax, DiR
                              ! For kRatio = 1 simple shift:
                              ! kS = kSMin+kR-kRMin
                              ! For kRatio = 2 coarsen both
                              ! kR and kRMin before shift
                              kS = kSMin + abs((kR+9)/kRatioRestr &
                                   -           (kRMin+9)/kRatioRestr)

                              jS = jSMin + abs((jR+9)/jRatioRestr &
                                   -           (jRMin+9)/jRatioRestr)

                              iS = iSMin + abs((iR+9)/iRatioRestr &
                                   -           (iRMin+9)/iRatioRestr)
                              State_VGB(:,iR,jR,kR,iBlockRecv) = &
                                   WeightOld*State_VGB(:,iR,jR,kR,iBlockRecv)+&
                                   WeightNew*(State_VGB(:,iS,jS,kS,iBlockSend)&
                                   + Slope_VG(:,iR,jR,kR))
                           end do
                        end do
                     end do
                  else
                     if(UseHighResChange .and. iSendStage == 4) then
                        iDir1 = 0; jDir1 = 0; kDir1 = 0
                        i5 = max(5*Di,1); j5 = max(5*Dj,1); k5 = max(5*Dk,1)
                        do kR = kRMin, kRMax, DkR
                           kS = kSMin + abs((kR+9)/kRatioRestr &
                                -           (kRMin+9)/kRatioRestr)
                           ! kDir = -1 if kR is even; kDir = 1 if kR is odd.
                           ! kR may be negative (1-nK),
                           ! so kR+2*nK will be always positive.
                           if(kRatioRestr == 2) kDir1 = 2*mod(kR+2*nK,2) - 1
                           do jR = jRMin, jRMax, DjR
                              jS = jSMin + abs((jR+9)/jRatioRestr &
                                   -           (jRMin+9)/jRatioRestr)
                              if(jRatioRestr == 2) jDir1 = 2*mod(jR+2*nJ,2) - 1
                              do iR = iRMin, iRMax, DiR
                                 iS = iSMin + abs((iR+9)/iRatioRestr &
                                      -           (iRMin+9)/iRatioRestr)
                                 if(iRatioRestr == 2) &
                                      iDir1 = 2*mod(iR+2*nI,2) - 1

                                 if(IsAccurateFace_GB(iR,jR,kR,iBlockRecv))&
                                      CYCLE

                                 do iVar = 1, nVar
                                    CoarseCell_III(1:i5,1:j5,1:k5) = &
                                         State_VGB(iVar,&
                                         iS-2*iDir1:iS+2*iDir1:sign(1,iDir1),&
                                         jS-2*jDir1:jS+2*jDir1:sign(1,jDir1),&
                                         kS-2*kDir1:kS+2*kDir1:sign(1,kDir1),&
                                         iBlockSend)

                                    State_VGB(iVar,iR,jR,kR,iBlockRecv) = &
                                         prolong_high_order_amr&
                                         (CoarseCell_III,&
                                         IsPositiveIn=IsPositive_V(iVar))
                                 enddo

                              end do ! iR
                           end do ! jR
                        end do ! kR
                     else
                        do kR = kRMin, kRMax, DkR
                           do jR = jRMin, jRMax, DjR
                              do iR = iRMin, iRMax, DiR
                                 kS = kSMin + abs((kR+9)/kRatioRestr &
                                      -           (kRMin+9)/kRatioRestr)

                                 jS = jSMin + abs((jR+9)/jRatioRestr &
                                      -           (jRMin+9)/jRatioRestr)

                                 iS = iSMin + abs((iR+9)/iRatioRestr &
                                      -           (iRMin+9)/iRatioRestr)

                                 if(nProlongOrder==2 .and. &
                                      (UseMin .or. UseMax))then
                                    ! Assign min/max value stored in Slope_VG
                                    State_VGB(:,iR,jR,kR,iBlockRecv) = &
                                         Slope_VG(:,iR,jR,kR)
                                 else
                                    State_VGB(:,iR,jR,kR,iBlockRecv) = &
                                         State_VGB(:,iS,jS,kS,iBlockSend) &
                                         + Slope_VG(:,iR,jR,kR)
                                 end if
                              end do
                           end do
                        end do

                     end if ! HighRes
                  end if ! UseTime
               else ! iProc /= iProcRecv
                  iBufferS = iBufferS_P(iProcRecv)

                  BufferS_I(            iBufferS+1) = iBlockRecv
                  BufferS_I(            iBufferS+2) = iRMin
                  BufferS_I(            iBufferS+3) = iRMax
                  if(nDim > 1)BufferS_I(iBufferS+4) = jRMin
                  if(nDim > 1)BufferS_I(iBufferS+5) = jRMax
                  if(nDim > 2)BufferS_I(iBufferS+6) = kRMin
                  if(nDim > 2)BufferS_I(iBufferS+7) = kRMax

                  iBufferS = iBufferS + 1 + 2*nDim
                  if(present(Time_B))then
                     iBufferS = iBufferS + 1
                     BufferS_I(iBufferS) = Time_B(iBlockSend)
                  end if

                  if(UseHighResChange .and. iSendStage == 4) then
                     iDir1 = 0; jDir1 = 0; kDir1 = 0
                     i5 = max(5*Di,1); j5 = max(5*Dj,1); k5 =  max(5*Dk,1)
                     do kR = kRMin, kRMax, DkR
                        kS = kSMin + abs((kR+9)/kRatioRestr &
                             -           (kRMin+9)/kRatioRestr)
                        ! kDir = -1 if kR is even; kDir = 1 if kR is odd.
                        ! kR may be negative (1-nK), kR+2*nK will be positive.
                        if(kRatioRestr == 2) kDir1 = 2*mod(kR+2*nK,2) - 1
                        do jR = jRMin, jRMax, DjR
                           jS = jSMin + abs((jR+9)/jRatioRestr &
                                -           (jRMin+9)/jRatioRestr)
                           if(jRatioRestr == 2) jDir1 = 2*mod(jR+2*nJ,2) - 1
                           do iR = iRMin, iRMax, DiR
                              iS = iSMin + abs((iR+9)/iRatioRestr &
                                   -           (iRMin+9)/iRatioRestr)
                              if(iRatioRestr == 2) iDir1 = 2*mod(iR+2*nI,2) - 1

                              do iVar = 1, nVar
                                 CoarseCell_III(1:i5,1:j5,1:k5) = &
                                      State_VGB(iVar,&
                                      iS-2*iDir1:iS+2*iDir1:sign(1,iDir1),&
                                      jS-2*jDir1:jS+2*jDir1:sign(1,jDir1),&
                                      kS-2*kDir1:kS+2*kDir1:sign(1,kDir1),&
                                      iBlockSend)

                                 BufferS_I(iBufferS+iVar)=&
                                      prolong_high_order_amr&
                                      (CoarseCell_III,&
                                      IsPositiveIn=IsPositive_V(iVar))

                              enddo
                              iBufferS = iBufferS + nVar
                           end do ! iR
                        end do ! jR
                     end do ! kR

                  else
                     do kR = kRMin, kRMax, DkR
                        kS = kSMin + abs((kR+9)/kRatioRestr &
                             -           (kRMin+9)/kRatioRestr)
                        do jR=jRMin, jRMax, DjR
                           jS = jSMin + abs((jR+9)/jRatioRestr &
                                -           (jRMin+9)/jRatioRestr)
                           do iR = iRMin, iRMax, DiR
                              iS = iSMin + abs((iR+9)/iRatioRestr &
                                   -           (iRMin+9)/iRatioRestr)

                              if(nProlongOrder==2 .and. (UseMin.or.UseMax))then
                                 ! Assign min/max value stored in Slope_VG
                                 BufferS_I(iBufferS+1:iBufferS+nVar)= &
                                      Slope_VG(:,iR,jR,kR)
                              else
                                 BufferS_I(iBufferS+1:iBufferS+nVar)= &
                                      State_VGB(:,iS,jS,kS,iBlockSend) &
                                      + Slope_VG(:,iR,jR,kR)
                              end if
                              iBufferS = iBufferS + nVar
                           end do
                        end do
                     end do
                  endif ! UseHighResChange

                  iBufferS_P(iProcRecv) = iBufferS
               end if
            end do
         end do
      end do ! subedge subface triple loop

    end subroutine do_prolong
    !==========================================================================
    subroutine calc_accurate_coarsened_block(iBlock)

      ! For nI*nJ*nK fine block, calculate its coarsened nI/2 * nJ/2 * nK/2
      ! overlaped block.

      integer, intent(in):: iBlock
      integer:: iPerp
      integer:: iDir1, jDir1, kDir1, iVar
      integer:: iDir2, jDir2, kDir2
      integer:: iDirMin, iDirMax, jDirMin, jDirMax, kDirMin, kDirMax
      integer:: DiDir, DjDir, DkDir
      integer:: i, j, k, Di, Dj, Dk
      integer:: i0, j0, k0, iC, jC, kC
      integer:: iBegin, iEnd, jBegin, jEnd, kBegin, kEnd

      real, allocatable:: Fine_VIII(:,:,:,:)
      real:: CoarseCell, Coarse_I(3),Cell_I(5)
      real:: Cell1_I(6), Cell2_I(6), Cell3_I(6)
      real:: Distance_I(4) = [-2,-1,1,2]
      real:: Cell_III(6,6,6)

      ! Some points can be calculated in 2 or 3 symmetric ways, use their
      ! average.
      integer, allocatable:: nCalc_III(:,:,:)
      integer, allocatable:: nCorrected_III(:,:,:)
      logical:: DoResChange_D(3), IsAccurateGhost, DoSymInterp
      real, parameter:: c1over10=1./10, c1over2=1./2
      real:: Coef_I(6)
      real:: Orig, Orig1, Orig2, Orig3, Res1, Res2, Res3
      integer:: nResChange, nEdge, nCorrect, iGhost, iCalTime
      real:: Weight1, Weight2

      character(len=*), parameter:: NameSub = 'calc_accurate_coarsened_block'
      !------------------------------------------------------------------------
      Cell1_I = 0; Cell2_I = 0; Cell3_I = 0

      if(DoSixthCorrect) then
         Coef_I = [0.05, -0.3, 0.75, 0.75, -0.3, 0.05]
      else
         Coef_I = [0.1, -0.5, 1.0, 0.5, -0.1, 0.0]
      endif

      if(.not. allocated(Fine_VIII))&
           allocate(Fine_VIII(nVar,8,6,min(6,nK)))

      if(.not. allocated(nCalc_III)) &
           allocate(nCalc_III(max(nI/2,1), max(nJ/2,1),max(nK/2,1)))
      nCalc_III = 0

      DoResChange_D = .false.
      DoSymInterp = .true.

      ! Resolution change in x-dir
      jDir1 = 0; kDir1 = 0
      do iDir1 = -1, 1, 2
         if(DiLevelNei_IIIB(iDir1,jDir1,kDir1,iBlock) /=1) CYCLE
         ! Resolution change can happen in at most three directions.
         DoResChange_D(1) = .true.

         ! Are the ghost cells close to the opposite face accurate?
         ! For the more than 2 levels refinement case.
         IsAccurateGhost = all(DiLevelNei_IIIB(-iDir1,:,:,iBlock) /= -1)
         DoSymInterp = nI >= 8 .or. IsAccurateGhost

         if(iDir1 == -1) then
            ! i0 is the index of the origin block.
            ! iC is the index of the coarsened block.
            i0 = 1; iC = 1
         elseif(iDir1 == 1) then
            i0 = nI; iC = nI/2
         endif

         jBegin = 1; jEnd = max(nJ/2,1)
         if(DiLevelNei_IIIB(0,1,0,iBlock) == 1) then
            ! For this situation, the calculation of cells j = nJ/2
            ! involves values of coarsened neighbour block. These
            ! cells need to be treated in a special way.
            jBegin = 1; jEnd = max(nJ/2-1,1)
         elseif(DiLevelNei_IIIB(0,-1,0,iBlock) == 1) then
            jBegin = max(nJ/2,1); jEnd = min(2,nJ)
         endif
         Dj = sign(1,jEnd - jBegin)

         ! For 3D.
         kBegin = 1; kEnd = max(nK/2,1)
         if(DiLevelNei_IIIB(0,0,1,iBlock) == 1) then
            kBegin = 1; kEnd = max(nK/2-1,1)
         elseif(DiLevelNei_IIIB(0,0,-1,iBlock) == 1) then
            kBegin = max(nK/2,1); kEnd = min(2,nK)
         endif
         Dk = sign(1,kEnd - kBegin)

         do k = kBegin, kEnd, Dk; do j = jBegin, jEnd, Dj
            if(nK == 1) then ! 2D
               Fine_VIII(:,:,:,k) = &
                    State_VGB(:,&
                    i0:i0-iDir1*7:-iDir1,&
                    2*j-3:2*j+2,&
                    k,iBlock)
            else  ! 3D
               Fine_VIII = &
                    State_VGB(:,&
                    i0:i0-iDir1*7:-iDir1,&
                    2*j-3:2*j+2,&
                    2*k-3:2*k+2,iBlock)
            endif

            do iVar = 1, nVar
               CoarseCell = State_VGB(iVar,i0+iDir1,2*j,min(2*k,nK),iBlock)
               call restrict_high_order_reschange(CoarseCell, &
                    Fine_VIII(iVar,:,:,:), Coarse_I, DoSymInterp,&
                    IsPositiveIn=IsPositive_V(iVar))
               State_VIIIB(iVar,iC:iC-2*iDir1:-iDir1,j,k,iBlock) = Coarse_I
            enddo ! iVar

            nCalc_III(iC:iC-2*iDir1:-iDir1,j,k) = &
                 nCalc_III(iC:iC-2*iDir1:-iDir1,j,k) + 1
         enddo; enddo
      enddo

      ! Resolution change in y-dir
      iDir1 = 0; kDir1 = 0
      do jDir1 = -1, 1, 2
         if(DiLevelNei_IIIB(iDir1,jDir1,kDir1,iBlock) /=1) CYCLE
         DoResChange_D(2) = .true.

         IsAccurateGhost = all(DiLevelNei_IIIB(:,-jDir1,:,iBlock) /= -1)
         DoSymInterp = nJ >= 8 .or. IsAccurateGhost

         if(jDir1 == -1) then
            j0 = 1; jC = 1
         elseif(jDir1 == 1) then
            j0 = nJ; jC = nJ/2
         endif

         iBegin = 1; iEnd = max(nI/2,1)
         if(DiLevelNei_IIIB(1,0,0,iBlock) == 1) then
            iBegin = 1; iEnd = max(nI/2-1,1)
         elseif(DiLevelNei_IIIB(-1,0,0,iBlock) == 1) then
            iBegin = max(nI/2,1); iEnd = min(2,nI)
         endif
         Di = sign(1,iEnd - iBegin)

         ! For 3D.
         kBegin = 1; kEnd = max(nK/2,1)
         if(DiLevelNei_IIIB(0,0,1,iBlock) == 1) then
            kBegin = 1; kEnd = max(nK/2-1,1)
         elseif(DiLevelNei_IIIB(0,0,-1,iBlock) == 1) then
            kBegin = max(nK/2,1); kEnd = min(2,nK)
         endif
         Dk = sign(1,kEnd - kBegin)

         do k = kBegin, kEnd, Dk; do i = iBegin, iEnd, Di
            if(nK == 1) then
               do iPerp = 1, 8
                  Fine_VIII(:,iPerp,:,k) = &
                       State_VGB(:,&
                       2*i-3:2*i+2, &
                       j0-jDir1*(iPerp-1),&
                       k,iBlock)
               enddo
            else ! 3D
               do iPerp = 1, 8
                  Fine_VIII(:,iPerp,:,:) = &
                       State_VGB(:,&
                       2*i-3:2*i+2, &
                       j0-jDir1*(iPerp-1),&
                       2*k-3:2*k+2,iBlock)
               enddo
            endif

            do iVar = 1, nVar
               CoarseCell = State_VGB(iVar,2*i,j0+jDir1,min(2*k,nK),iBlock)

               call restrict_high_order_reschange(CoarseCell, &
                    Fine_VIII(iVar,:,:,:), Coarse_I, DoSymInterp,&
                    IsPositiveIn=IsPositive_V(iVar))

               do iGhost = 1, 3 ! 3 layer ghost cells.
                  ! For ghost cells can be calculated in several symmetric
                  ! ways, use their average.
                  iCalTime = nCalc_III(i,jC-(iGhost-1)*jDir1,k)
                  Weight1 = iCalTime/(iCalTime + 1.0)
                  Weight2 = 1.0 - Weight1
                  State_VIIIB(iVar,i,jC-(iGhost-1)*jDir1,k,iBlock) &
                       = Weight1*&
                       State_VIIIB(iVar,i,jC-(iGhost-1)*jDir1,k,iBlock)&
                       + Weight2*Coarse_I(iGhost)
               enddo
            enddo ! iVar
            nCalc_III(i,jC:jC-2*jDir1:-jDir1,k) = &
                 nCalc_III(i,jC:jC-2*jDir1:-jDir1,k) + 1
         enddo; enddo
      enddo

      ! Resolution change in z-dir.
      iDir1 = 0; jDir1 = 0
      do kDir1 = -1, 1, 2
         if(DiLevelNei_IIIB(iDir1,jDir1,kDir1,iBlock) /=1) CYCLE
         DoResChange_D(3) = .true.

         IsAccurateGhost = all(DiLevelNei_IIIB(:,:,-kDir1,iBlock) /= -1)
         DoSymInterp = nK >= 8 .or. IsAccurateGhost

         if(kDir1 == -1) then
            k0 = 1; kC = 1
         else ! kDir1 == 1
            k0 = nK; kC = nK/2
         endif

         iBegin = 1; iEnd = max(nI/2,1)
         if(DiLevelNei_IIIB(1,0,0,iBlock) == 1) then
            iBegin = 1; iEnd = max(nI/2-1,1)
         elseif(DiLevelNei_IIIB(-1,0,0,iBlock) == 1) then
            iBegin = max(nI/2,1); iEnd = min(2,nI)
         endif
         Di = sign(1,iEnd - iBegin)

         jBegin = 1; jEnd = max(nJ/2,1)
         if(DiLevelNei_IIIB(0,1,0,iBlock) == 1) then
            jBegin = 1; jEnd = max(nJ/2-1,1)
         elseif(DiLevelNei_IIIB(0,-1,0,iBlock) == 1) then
            jBegin = max(nJ/2,1); jEnd = min(2,nJ)
         endif
         Dj = sign(1,jEnd - jBegin)

         do j = jBegin, jEnd, Dj; do i = iBegin, iEnd, Di

            do iPerp = 1, 8
               Fine_VIII(:,iPerp,:,:) = &
                    State_VGB(:,&
                    2*i-3:2*i+2,&
                    2*j-3:2*j+2,&
                    k0-kDir1*(iPerp-1),&
                    iBlock)
            enddo

            do iVar = 1, nVar
               CoarseCell = State_VGB(iVar,2*i,2*j,k0+kDir1,iBlock)
               call restrict_high_order_reschange(CoarseCell, &
                    Fine_VIII(iVar,:,:,:), Coarse_I, DoSymInterp,&
                    IsPositiveIn=IsPositive_V(iVar))

               do iGhost = 1, 3 ! 3 layer ghost cells.
                  iCalTime = nCalc_III(i,j,kC-(iGhost-1)*kDir1)
                  Weight1 = iCalTime/(iCalTime + 1.0)
                  Weight2 = 1.0 - Weight1
                  State_VIIIB(iVar,i,j,kC-(iGhost-1)*kDir1,iBlock) = &
                       Weight1*&
                       State_VIIIB(iVar,i,j,kC-(iGhost-1)*kDir1,iBlock) &
                       + Weight2*Coarse_I(iGhost)
               enddo
            enddo
            nCalc_III(i,j,kC:kC-2*kDir1:-kDir1) = &
                 nCalc_III(i,j,kC:kC-2*kDir1:-kDir1) + 1
         enddo; enddo
      enddo ! kDir1

      ! At least one neighbour block is coarse when this subroutine called.
      ! If it is not a face block, it will be a edge/corner block.
      if(nK == 1) then ! 2D. Combine 2D and 3D part??
         ! Resolution change in the edge direction.
         if(.not. DoResChange_D(1) .and. .not.DoResChange_D(2)) then

            ! ___________________________________________
            ! |                    |         |          |
            ! |                    |         |          |
            ! |                    |         |          |
            ! |        3           |_________|__________|
            ! |                    |         |          |
            ! |                    |   14    |          |
            ! |                    |         |          |        y
            ! |____________________|_________|__________|        |
            ! |          |         |*        |          |        |
            ! |          |   13    |   11    |   12     |        -----> x
            ! |          |         |         |          |
            ! |__________|_________|_________|__________|
            ! |          |         |         |          |
            ! |          |         |   9     |   10     |
            ! |          |         |         |          |
            ! |__________|_________|_________|__________|
            !
            ! 9-14 are the top layer of their parent block.

            ! Coarsened cell * is interpolated diagonally.

            ! Edge ghost cells.
            kDir1 = 0
            do iDir1 = -1, 1, 2; do jDir1 = -1, 1, 2
               if(DiLevelNei_IIIB(iDir1,jDir1,kDir1,iBlock) /=1) CYCLE

               if(iDir1 == 1) then
                  iC = nI/2; i0 = nI; Di  = -1
               else ! iDir1 = -1
                  iC = 1; i0 = 1; Di = 1
               endif

               if(jDir1 == 1) then
                  jC = nJ/2; j0 = nJ; Dj = -1
               else ! jDir1 = -1
                  jC = 1; j0 = 1; Dj = 1
               endif

               do iVar = 1, nVar
                  k = 1
                  do j = jC, jC+2*Dj, Dj; do i = iC, iC+2*Di, Di
                     if(j == jC .and. i == iC) CYCLE
                     Cell_III(:,:,k) = &
                          State_VGB(iVar,2*i-3:2*i+2,2*j-3:2*j+2,k,iBlock)
                     State_VIIIB(iVar,i,j,k,iBlock) = &
                          restrict_high_order_amr(Cell_III,&
                          IsPositiveIn=IsPositive_V(iVar))
                  enddo; enddo

                  ! Interpolate in diagonal direction.
                  j = jC; i = iC; k = 1

                  Cell_I(1) = State_VIIIB(iVar,i+2*Di,j+2*Dj,k,iBlock)
                  Cell_I(2) = State_VIIIB(iVar,i+  Di,j+  Dj,k,iBlock)
                  Cell_I(3) = State_VGB  (iVar,i0-  Di,j0-  Dj,k,iBlock)
                  Cell_I(4) = State_VGB  (iVar,i0-2*Di,j0-2*Dj,k,iBlock)
                  Cell_I(5) = State_VGB  (iVar,i0-3*Di,j0-3*Dj,k,iBlock)

                  Orig = -c1over10*Cell_I(1) + c1over2*Cell_I(2) + Cell_I(3) &
                       -c1over2*Cell_I(4) + c1over10*Cell_I(5)
                  State_VIIIB(iVar,i,j,k,iBlock) = &
                       limit_interpolation(Orig,Cell_I(1:4),Distance_I,&
                       IsPositiveIn=IsPositive_V(iVar))
               enddo ! iVar
            enddo; enddo
         elseif(DoResChange_D(1) .and. DoResChange_D(2)) then

            ! ___________________________________________
            ! |                    |                    |
            ! |                    |                    |
            ! |                    |                    |
            ! |        3           |                    |
            ! |                    |                    |
            ! |                    |   4                |
            ! |                    |                    |        y
            ! |____________________|_________ __________|        |
            ! |                    |*        |          |        |
            ! |                    |   8     |   7      |        -----> x
            ! |                    |         |          |
            ! |        1           |_________|__________|
            ! |                    |         |          |
            ! |                    |   5     |   6      |
            ! |                    |         |          |
            ! |____________________|_________|__________|
            !
            ! 5-8 are the top layer of their parent block.

            ! Coarsened cell * can be corrected in x or y direction. Use
            ! the average.

            if(DiLevelNei_IIIB(-1,0,0,iBlock) == 1) then
               iC = 1; i0 = 1; Di = 1
            elseif(DiLevelNei_IIIB(1,0,0,iBlock) == 1) then
               iC = nI/2; i0 = nI; Di = -1
            else
               call CON_stop(NameSub//': This case should not happen! - case1')
            endif

            if(DiLevelNei_IIIB(0,-1,0,iBlock) == 1) then
               jC = 1; j0 = 1; Dj = 1
            elseif(DiLevelNei_IIIB(0,1,0,iBlock) == 1) then
               jC = nJ/2; j0 = nJ; Dj = -1
            else
               call CON_stop(NameSub//': This case should not happen! - case2')
            endif

            k = 1
            do iVar = 1,nVar
               ! Use the neighbour coarsened cells to correct the corner cell.
               Cell1_I(1:3) = State_VGB(iVar,i0-3*Di:i0-Di:Di, j0, k,iBlock)
               Cell2_I(1:3) = State_VGB(iVar,i0,j0-3*Dj:j0-Dj:Dj,k,iBlock)
               if(DoSixthCorrect) then
                  Cell1_I(4:6) = &
                       State_VIIIB(iVar,iC+Di:iC+3*Di:Di,jC,k,iBlock)
                  Cell2_I(4:6) = &
                       State_VIIIB(iVar,iC, jC+Dj:jC+3*Dj:Dj,k,iBlock)
               else
                  Cell1_I(4:5) = &
                       State_VIIIB(iVar,iC+Di:iC+2*Di:Di,jC,k,iBlock)
                  Cell2_I(4:5) = &
                       State_VIIIB(iVar,iC, jC+Dj:jC+2*Dj:Dj,k,iBlock)
               endif

               Orig1 = sum(Coef_I*Cell1_I)
               Orig2 = sum(Coef_I*Cell2_I)

               Res1 = limit_interpolation(Orig1, Cell1_I(2:5), Distance_I,&
                    IsPositiveIn=IsPositive_V(iVar))
               Res2 = limit_interpolation(Orig2, Cell2_I(2:5), Distance_I,&
                    IsPositiveIn=IsPositive_V(iVar))

               State_VIIIB(iVar,iC,jC,k,iBlock) = 0.5*(Res1 + Res2)
            enddo
         endif

      else ! 3D
         nResChange = 0
         do i = 1, 3
            if(DoResChange_D(i)) nResChange = nResChange + 1
         enddo

         if(nResChange > 1) then ! nResChange is 2 or 3.

            ! Example: coarsen block 11, nResChange == 2
            ! ___________________________________________
            ! |                    |                    |
            ! |                    |                    |
            ! |                    |                    |
            ! |         7          |         8          |
            ! |                    |                    |
            ! |                    |                    |
            ! |                    |                    |
            ! |____________________|____________________|
            ! |                    |        |           |
            ! |                    |        |           |
            ! |                    |        |           |
            ! |          5         |________|___________|
            ! |                    |        |           |
            ! |                    |        |           |
            ! |                    |        |           |
            ! |____________________|________|___________|
            !               TOP LAYER

            ! ___________________________________________
            ! |                    |                    |
            ! |                    |                    |
            ! |                    |                    |
            ! |        3           |                    |
            ! |                    |                    |
            ! |                    |   4                |
            ! |                    |                    |        y
            ! |____________________|_________ __________|        |
            ! |                    |*        |          |        |
            ! |                    |   11    |   12     |        -----> x
            ! |                    |         |          |
            ! |        1           |_________|__________|
            ! |                    |         |          |
            ! |                    |   9     |   10     |
            ! |                    |         |          |
            ! |____________________|_________|__________|
            !                 BOTTOM LAYER
            ! 9-12 are the top layer of their parent block.

            ! For block 11, resolution change happens in x and y direcitons.
            ! Only cells ic == 1 .and. jc == nJ/2 .and. 1 =< kc =< nK/2 need
            ! to be corrected with coarse cell values. Interpolations can be
            ! done in x direction or y direction. Use the average of both.

            ! Example: coarsen block 11, nResChange == 3
            ! ___________________________________________
            ! |                    |                    |
            ! |                    |                    |
            ! |                    |                    |
            ! |         7          |         8          |
            ! |                    |                    |
            ! |                    |                    |
            ! |                    |                    |
            ! |____________________|____________________|
            ! |                    |                    |
            ! |                    |                    |
            ! |                    |                    |
            ! |          5         |         6          |
            ! |                    |                    |
            ! |                    |                    |
            ! |                    |                    |
            ! |____________________|____________________|
            !               TOP LAYER

            ! ___________________________________________
            ! |                    |                    |
            ! |                    |                    |
            ! |                    |                    |
            ! |        3           |                    |
            ! |                    |                    |
            ! |                    |   4                |
            ! |                    |                    |        y
            ! |____________________|_________ __________|        |
            ! |                    |         |          |        |
            ! |                    |   11    |   12     |        -----> x
            ! |                    |         |          |
            ! |        1           |_________|__________|
            ! |                    |         |          |
            ! |                    |   9     |   10     |
            ! |                    |         |          |
            ! |____________________|_________|__________|
            !                 BOTTOM LAYER
            ! 9-12 are the top layer of their parent block.

            ! For block 11, resolution change happens in x, y and z directions.
            ! Correction similar with the nResChange==2 case, but one cell :
            ! ic == 1 .and. jc == nJ/2 .and. kc == nK/2 can be corrected in
            ! three directions. Also use the average.

            if(DiLevelNei_IIIB(-1,0,0,iBlock) == 1) then
               ! i0: index of origin block.
               ! iC & iBegin & iEnd: index of coarsened block.
               iC = 1; i0 = 1; Di = 1
               iBegin = nI/2; iEnd = 2
            elseif(DiLevelNei_IIIB(1,0,0,iBlock) == 1) then
               iC = nI/2; i0 = nI; Di = -1
               iBegin = 1; iEnd = nI/2 - 1
            endif

            if(DiLevelNei_IIIB(0,-1,0,iBlock) == 1) then
               jC = 1; j0 = 1; Dj = 1
               jBegin = nJ/2; jEnd = 2
            elseif(DiLevelNei_IIIB(0,1,0,iBlock) == 1) then
               jC = nJ/2; j0 = nJ; Dj = -1
               jBegin = 1; jEnd = nJ/2 - 1
            endif

            if(DiLevelNei_IIIB(0,0,-1,iBlock) == 1) then
               kC = 1; k0 = 1; Dk = 1
               kBegin = nK/2; kEnd = 2
            elseif(DiLevelNei_IIIB(0,0,1,iBlock) == 1) then
               kC = nK/2; k0 = nK; Dk = -1
               kBegin = 1; kEnd = nK/2 - 1
            endif

            if(.not. DoResChange_D(3) .or. nResChange == 3) then
               if(.not. DoResChange_D(3)) then
                  kBegin = 1; kEnd = nK/2
               endif
               do k = kBegin, kEnd, sign(1,kEnd-kBegin)
                  do iVar = 1,nVar
                     Cell1_I(1:3) = &
                          State_VGB(iVar,i0-3*Di:i0-Di:Di, j0, 2*k,iBlock)
                     Cell2_I(1:3) = &
                          State_VGB(iVar,i0,j0-3*Dj:j0-Dj:Dj,2*k,iBlock)

                     if(DoSixthCorrect) then
                        Cell1_I(4:6) = &
                             State_VIIIB(iVar,iC+Di:iC+3*Di:Di,jC,k,iBlock)
                        Cell2_I(4:6) = &
                             State_VIIIB(iVar,iC, jC+Dj:jC+3*Dj:Dj,k,iBlock)
                     else
                        Cell1_I(4:5) = &
                             State_VIIIB(iVar,iC+Di:iC+2*Di:Di,jC,k,iBlock)
                        Cell2_I(4:5) = &
                             State_VIIIB(iVar,iC, jC+Dj:jC+2*Dj:Dj,k,iBlock)
                     endif

                     Orig1 = sum(Coef_I*Cell1_I)
                     Orig2 = sum(Coef_I*Cell2_I)

                     Res1 = &
                          limit_interpolation(Orig1, Cell1_I(2:5), Distance_I,&
                          IsPositiveIn=IsPositive_V(iVar))
                     Res2 = &
                          limit_interpolation(Orig2, Cell2_I(2:5), Distance_I,&
                          IsPositiveIn=IsPositive_V(iVar))

                     State_VIIIB(iVar,iC,jC,k,iBlock) = 0.5*(Res1 + Res2)
                  enddo
               enddo
            endif

            if(.not. DoResChange_D(2) .or. nResChange == 3) then
               if(.not. DoResChange_D(2)) then
                  jBegin = 1; jEnd = nJ/2
               endif
               do j = jBegin, jEnd, sign(1,jEnd - jBegin)
                  do iVar = 1, nVar
                     Cell1_I(1:3) = &
                          State_VGB(iVar,i0-3*Di:i0-Di:Di,2*j,k0,iBlock)
                     Cell2_I(1:3) = &
                          State_VGB(iVar,i0,2*j,k0-3*Dk:k0-Dk:Dk,iBlock)

                     if(DoSixthCorrect) then
                        Cell1_I(4:6) = &
                             State_VIIIB(iVar,iC+Di:iC+3*Di:Di,j,kC,iBlock)
                        Cell2_I(4:6) = &
                             State_VIIIB(iVar,iC,j,kC+Dk:kC+3*Dk:Dk,iBlock)
                     else
                        Cell1_I(4:5) = &
                             State_VIIIB(iVar,iC+Di:iC+2*Di:Di,j,kC,iBlock)
                        Cell2_I(4:5) = &
                             State_VIIIB(iVar,iC,j,kC+Dk:kC+2*Dk:Dk,iBlock)
                     endif

                     Orig1 = sum(Coef_I*Cell1_I)
                     Orig2 = sum(Coef_I*Cell2_I)

                     Res1 = &
                          limit_interpolation(Orig1, Cell1_I(2:5), Distance_I,&
                          IsPositiveIn=IsPositive_V(iVar))
                     Res2 = &
                          limit_interpolation(Orig2, Cell2_I(2:5), Distance_I,&
                          IsPositiveIn=IsPositive_V(iVar))

                     State_VIIIB(iVar,iC,j,kC,iBlock) = 0.5*(Res1 + Res2)
                  enddo
               enddo
            endif

            if(.not. DoResChange_D(1) .or. nResChange == 3) then
               if(.not. DoResChange_D(1)) then
                  iBegin = 1; iEnd = nI/2
               endif
               do i = iBegin, iEnd, sign(1,iEnd - iBegin)
                  do iVar = 1, nVar
                     Cell1_I(1:3) = &
                          State_VGB(iVar,2*i,j0-3*Dj:j0-Dj:Dj,k0,iBlock)
                     Cell2_I(1:3) = &
                          State_VGB(iVar,2*i,j0,k0-3*Dk:k0-Dk:Dk,iBlock)

                     if(DoSixthCorrect) then
                        Cell1_I(4:6) = &
                             State_VIIIB(iVar,i,jC+Dj:jC+3*Dj:Dj,kC,iBlock)
                        Cell2_I(4:6) = &
                             State_VIIIB(iVar,i,jC,kC+Dk:kC+3*Dk:Dk,iBlock)
                     else
                        Cell1_I(4:5) = &
                             State_VIIIB(iVar,i,jC+Dj:jC+2*Dj:Dj,kC,iBlock)
                        Cell2_I(4:5) = &
                             State_VIIIB(iVar,i,jC,kC+Dk:kC+2*Dk:Dk,iBlock)
                     endif

                     Orig1 = sum(Coef_I*Cell1_I)
                     Orig2 = sum(Coef_I*Cell2_I)

                     Res1 = &
                          limit_interpolation(Orig1, Cell1_I(2:5), Distance_I,&
                          IsPositiveIn=IsPositive_V(iVar))
                     Res2 = &
                          limit_interpolation(Orig2, Cell2_I(2:5), Distance_I,&
                          IsPositiveIn=IsPositive_V(iVar))

                     State_VIIIB(iVar,i,jC,kC,iBlock) = 0.5*(Res1 + Res2)
                  enddo
               enddo
            endif

            if(nResChange == 3) then
               ! One corner cell.
               do iVar = 1, nVar
                  Cell1_I(1:3) = &
                       State_VGB(iVar,i0-3*Di:i0-Di:Di, j0, k0,iBlock)
                  Cell2_I(1:3) = &
                       State_VGB(iVar,i0,j0-3*Dj:j0-Dj:Dj,k0,iBlock)
                  Cell3_I(1:3) = &
                       State_VGB(iVar,i0,j0,k0-3*Dk:k0-Dk:Dk,iBlock)
                  if(DoSixthCorrect) then
                     Cell1_I(4:6) = &
                          State_VIIIB(iVar,iC+Di:iC+3*Di:Di,jC,kC,iBlock)
                     Cell2_I(4:6) = &
                          State_VIIIB(iVar,iC, jC+Dj:jC+3*Dj:Dj,kC,iBlock)
                     Cell3_I(4:6) = &
                          State_VIIIB(iVar,iC,jC,kC+Dk:kC+3*Dk:Dk,iBlock)
                  else
                     Cell1_I(4:5) = &
                          State_VIIIB(iVar,iC+Di:iC+2*Di:Di,jC,kC,iBlock)
                     Cell2_I(4:5) = &
                          State_VIIIB(iVar,iC, jC+Dj:jC+2*Dj:Dj,kC,iBlock)
                     Cell3_I(4:5) = &
                          State_VIIIB(iVar,iC,jC,kC+Dk:kC+2*Dk:Dk,iBlock)
                  endif

                  Orig1 = sum(Coef_I*Cell1_I)
                  Orig2 = sum(Coef_I*Cell2_I)
                  Orig3 = sum(Coef_I*Cell3_I)

                  Res1 = limit_interpolation(Orig1, Cell1_I(2:5), Distance_I,&
                       IsPositiveIn=IsPositive_V(iVar))
                  Res2 = limit_interpolation(Orig2, Cell2_I(2:5), Distance_I,&
                       IsPositiveIn=IsPositive_V(iVar))
                  Res3 = limit_interpolation(Orig3, Cell3_I(2:5), Distance_I,&
                       IsPositiveIn=IsPositive_V(iVar))

                  State_VIIIB(iVar,iC,jC,kC,iBlock) = &
                       (Res1 + Res2 + Res3)/3.0
               enddo
            endif

         elseif(nResChange == 1) then

            ! Example: coarsen block 11
            ! ___________________________________________
            ! |                    |                    |
            ! |                    |                    |
            ! |                    |                    |
            ! |         7          |         8          |
            ! |                    |                    |
            ! |                    |                    |
            ! |                    |                    |
            ! |____________________|____________________|
            ! |                    |                    |
            ! |                    |                    |
            ! |                    |                    |
            ! |          5         |         6          |
            ! |                    |                    |
            ! |                    |                    |
            ! |                    |                    |
            ! |____________________|____________________|
            !               TOP LAYER

            ! ___________________________________________
            ! |                    |         |          |
            ! |                    |         |          |
            ! |                    |         |          |
            ! |        3           |_________|__________|
            ! |                    |         |          |
            ! |                    |   14    |          |
            ! |                    |         |          |        y
            ! |____________________|_________|__________|        |
            ! |          |         |*        |          |        |
            ! |          |   13    |   11    |   12     |        -----> x
            ! |          |         |         |          |
            ! |__________|_________|_________|__________|
            ! |          |         |         |          |
            ! |          |         |   9     |   10     |
            ! |          |         |         |          |
            ! |__________|_________|_________|__________|
            !                 BOTTOM LAYER
            ! 9-14 are the top layer of their parent block.

            ! Assume resolution change happens in z direction for block 11.
            ! If block 3 is also refined, only need to coarsen block 11 to
            ! fill in the face ghost cells of block 6, which is trivial.
            ! If block 3 is coarse, there will be four kinds cells need
            ! to be coarsened for block 11:
            ! ic, jc, kc are the index of coarsened block, not the original
            ! block 11.
            ! Type 1: 1 =< ic =< nI/2; 1 =< jc =< nJ/2; nk/2 -2 =< kc =< nk/2
            !         except for ic == 1 .and. jc==nJ/2.
            ! Type 2: ic == 1 .and. jc==nJ/2 .and. nk/2 -2 =< kc =< nk/2
            ! Type 3: ic == 1 .and. jc==nJ/2 .and. 1 =< kc =< nk/2 - 3
            ! Type 4: 1 =< ic =< nI/2; 1 =< jc =< nJ/2; 1 =< kc =< nk/2 -3
            !         except those belong to Type 3.

            ! Type 1 and Type 2 are face ghost cells of block 6. Some of them
            ! are also edge ghost cells for block 3.
            ! Type 3 and Type 4 are only used for edge ghost cells of block 3.
            ! If nK is 6, there are no type 3 and type 4 cells.

            ! Calculation:
            ! Type 1: the same as simple resolution change.
            ! Type 4: simple restriction use 6*6*6 fine cells.
            ! Type 2&3: interpolation diagionally in x-y plane.

            if(DoResChange_D(1)) then
               iDirMin =  0; iDirMax = 0; DiDir = 1
               jDirMin = -1; jDirMax = 1; DjDir = 2
               kDirMin = -1; kDirMax = 1; DkDir = 2
               iBegin = 1; iEnd = nI/2; Di = 1
            elseif(DoResChange_D(2)) then
               iDirMin = -1; iDirMax = 1; DiDir = 2
               jDirMin =  0; jDirMax = 0; DjDir = 1
               kDirMin = -1; kDirMax = 1; DkDir = 2
               jBegin = 1; jEnd = nJ/2; Dj = 1
            elseif(DoResChange_D(3)) then
               iDirMin = -1; iDirMax = 1; DiDir = 2
               jDirMin = -1; jDirMax = 1; DjDir = 2
               kDirMin =  0; kDirMax = 0; DkDir = 1
               kBegin = 1; kEnd = nK/2; Dk = 1
            endif

            do kDir2 = kDirMin, kDirMax, DkDir
               do jDir2 = jDirMin, jDirMax, DjDir
                  do iDir2 = iDirMin, iDirMax, DiDir
                     if(DiLevelNei_IIIB(iDir2,jDir2,kDir2,iBlock) /=1) CYCLE

                     if(iDir2 == 1) then
                        iBegin = nI/2; iEnd = nI/2 - 2; Di = -1; i0 = nI
                     elseif(iDir2 == -1) then
                        iBegin = 1; iEnd = 3; Di = 1; i0 = 1
                     endif

                     if(jDir2 == 1) then
                        jBegin = nJ/2; jEnd = nJ/2 - 2; Dj = -1; j0 = nJ
                     elseif(jDir2 == -1) then
                        jBegin = 1; jEnd = 3; Dj = 1; j0 = 1
                     endif

                     if(kDir2 == 1) then
                        kBegin = nK/2; kEnd = nK/2 - 2; Dk = -1; k0 = nK
                     elseif(kDir2 == -1) then
                        kBegin = 1; kEnd = 3; Dk = 1; k0 = 1
                     endif

                     ! Calc Type 4 cells.
                     do k = kBegin, kEnd, Dk
                        do j = jBegin, jEnd, Dj
                           do i = iBegin, iEnd, Di
                              if(nCalc_III(i,j,k)>0) CYCLE
                              if(DoResChange_D(1) .and. &
                                   j == jBegin .and. k == kBegin) CYCLE
                              if(DoResChange_D(2) .and. &
                                   i == iBegin .and. k == kBegin) CYCLE
                              if(DoResChange_D(3) .and. &
                                   i == iBegin .and. j == jBegin) CYCLE

                              do iVar = 1, nVar
                                 Cell_III = State_VGB(iVar,&
                                      2*i-3:2*i+2,&
                                      2*j-3:2*j+2,&
                                      2*k-3:2*k+2,&
                                      iBlock)
                                 State_VIIIB(iVar,i,j,k,iBlock) = &
                                      restrict_high_order_amr(Cell_III,&
                                      IsPositiveIn=IsPositive_V(iVar))
                              enddo ! iVar
                              nCalc_III(i,j,k) = nCalc_III(i,j,k)+1

                           enddo ! i
                        enddo ! j
                     enddo ! k

                     ! Calc Type 2 and Type 3.
                     if(DoResChange_D(1)) then
                        j = jBegin; k = kBegin
                        do i = iBegin, iEnd, Di
                           do iVar = 1, nVar
                              Cell_I(1) = State_VIIIB&
                                   (iVar,i,j+2*Dj,k+2*Dk,iBlock)
                              Cell_I(2) = State_VIIIB&
                                   (iVar,i,j+  Dj,k+ Dk,iBlock)
                              Cell_I(3) = State_VGB  &
                                   (iVar,2*i,j0-  Dj,k0-  Dk,iBlock)
                              Cell_I(4) = State_VGB  &
                                   (iVar,2*i,j0-2*Dj,k0-2*Dk,iBlock)
                              Cell_I(5) = State_VGB  &
                                   (iVar,2*i,j0-3*Dj,k0-3*Dk,iBlock)

                              Orig = -c1over10*Cell_I(1) + c1over2*Cell_I(2) +&
                                   Cell_I(3) - c1over2*Cell_I(4) + &
                                   c1over10*Cell_I(5)
                              State_VIIIB(iVar,i,j,k,iBlock) =&
                                   limit_interpolation&
                                   (Orig,Cell_I(1:4),Distance_I,&
                                   IsPositiveIn=IsPositive_V(iVar))
                           enddo ! iVar
                           nCalc_III(i,j,k) = nCalc_III(i,j,k) + 1
                        enddo ! i
                     elseif(DoResChange_D(2)) then
                        i = iBegin; k = kBegin
                        do j = jBegin, jEnd, Dj
                           do iVar = 1, nVar
                              Cell_I(1) = State_VIIIB&
                                   (iVar,i+2*Di,j,k+2*Dk,iBlock)
                              Cell_I(2) = State_VIIIB&
                                   (iVar,i+  Di,j,k+  Dk,iBlock)
                              Cell_I(3) = State_VGB  &
                                   (iVar,i0-  Di,2*j,k0-  Dk,iBlock)
                              Cell_I(4) = State_VGB  &
                                   (iVar,i0-2*Di,2*j,k0-2*Dk,iBlock)
                              Cell_I(5) = State_VGB  &
                                   (iVar,i0-3*Di,2*j,k0-3*Dk,iBlock)

                              Orig = -c1over10*Cell_I(1) + c1over2*Cell_I(2) +&
                                   Cell_I(3) - c1over2*Cell_I(4) + &
                                   c1over10*Cell_I(5)
                              State_VIIIB(iVar,i,j,k,iBlock) =&
                                   limit_interpolation&
                                   (Orig,Cell_I(1:4),Distance_I,&
                                   IsPositiveIn=IsPositive_V(iVar))
                           enddo ! iVar
                           nCalc_III(i,j,k) = nCalc_III(i,j,k) + 1
                        enddo ! j

                     elseif(DoResChange_D(3)) then
                        i = iBegin; j = jBegin
                        do k = kBegin, kEnd, Dk
                           do iVar = 1, nVar
                              Cell_I(1) = State_VIIIB&
                                   (iVar,i+2*Di,j+2*Dj,k,iBlock)
                              Cell_I(2) = State_VIIIB&
                                   (iVar,i+  Di,j+  Dj,k,iBlock)
                              Cell_I(3) = State_VGB  &
                                   (iVar,i0-  Di,j0-  Dj,2*k,iBlock)
                              Cell_I(4) = State_VGB  &
                                   (iVar,i0-2*Di,j0-2*Dj,2*k,iBlock)
                              Cell_I(5) = State_VGB  &
                                   (iVar,i0-3*Di,j0-3*Dj,2*k,iBlock)

                              Orig = -c1over10*Cell_I(1) + c1over2*Cell_I(2) +&
                                   Cell_I(3) - c1over2*Cell_I(4) + &
                                   c1over10*Cell_I(5)
                              State_VIIIB(iVar,i,j,k,iBlock) =&
                                   limit_interpolation&
                                   (Orig,Cell_I(1:4),Distance_I,&
                                   IsPositiveIn=IsPositive_V(iVar))
                           enddo ! iVar
                           nCalc_III(i,j,k) = nCalc_III(i,j,k)+1
                        enddo ! k
                     endif
                  enddo
               enddo
            enddo
         elseif(nResChange == 0) then
            ! Example: coarsen block 11
            ! ___________________________________________
            ! |                    |                    |
            ! |                    |                    |
            ! |                    |                    |
            ! |         7          |         8          |
            ! |                    |                    |
            ! |                    |                    |
            ! |                    |                    |
            ! |____________________|____________________|
            ! |                    |        |           |
            ! |                    |        |           |
            ! |                    |        |           |
            ! |          5         |________|___________|
            ! |                    |        |           |
            ! |                    |        |           |
            ! |                    |        |           |
            ! |____________________|________|___________|
            !               TOP LAYER

            ! ___________________________________________
            ! |                    |         |          |
            ! |                    |         |          |
            ! |                    |         |          |
            ! |        3           |_________|__________|
            ! |                    |         |          |
            ! |                    |   14    |          |
            ! |                    |         |          |        y
            ! |____________________|_________|__________|        |
            ! |          |         |*        |          |        |
            ! |          |   13    |   11    |   12     |        -----> x
            ! |          |         |         |          |
            ! |__________|_________|_________|__________|
            ! |          |         |         |          |
            ! |          |         |   9     |   10     |
            ! |          |         |         |          |
            ! |__________|_________|_________|__________|
            !                 BOTTOM LAYER
            ! 9-14 are the top layer of their parent block.

            ! Six faces neighbours of block 11 are fine blocks.
            ! But at most three edg blocks (block 3, 5, 8) and one
            ! corner (block 7) may not.

            ! Case 1: only one edge block (like block 3) is coarse.
            !         ic = 1 .and. jc = nJ/2 .and. 1 =< kc =< nK/2
            !         are interpolated diagonally in x-y plane. Other
            !         needed values are restricted with 6*6*6 fine cells.
            ! Case 2: n ( 1<n<=3 ) edge blocks are coarse. Similar
            !         with Case 1, but one cell: ic=1,jc=nk/2,kc=nk/2
            !         can be interpolated in n directions. Use the average
            !         of these n interpolations.
            ! Case 3: only the corner block is coarse. Need to coarsen 11
            !         to fill in the corner ghost of block 7.
            !         All needed cells except for ic=1,jc=nk/2,kc=nk/2 can
            !         be rectricted with 6*6*6 fine cells.
            !         The corner one (ic=1,jc=nk/2,kc=nk/2) is interpolated
            !         diagionally in 3D space.

            nEdge = 0
            if(.not. allocated(nCorrected_III)) allocate(&
                 nCorrected_III(max(nI/2,1), max(nJ/2,1),max(nK/2,1)))
            nCorrected_III = 0

            do kDir1 = -1, 1; do jDir1 = -1, 1; do iDir1 = -1, 1
               ! Concentrate on the edge neighbour blocks.
               if(abs(iDir1) + abs(jDir1) + abs(kDir1) /= 2) CYCLE
               if(DiLevelNei_IIIB(iDir1,jDir1,kDir1,iBlock) /=1) CYCLE

               ! Record how many edge neighbour blocks are coarse.
               nEdge = nEdge + 1

               if(iDir1 == 1) then
                  iBegin = nI/2; iEnd = nI/2 - 2; Di = -1; i0 = nI
               elseif(iDir1 == -1) then
                  iBegin = 1; iEnd = 3; Di = 1; i0 = 1
               elseif(iDir1 == 0) then
                  iBegin = 1; iEnd = nI/2; Di = 1
               endif

               if(jDir1 == 1) then
                  jBegin = nJ/2; jEnd = nJ/2 - 2; Dj = -1; j0 = nJ
               elseif(jDir1 == -1) then
                  jBegin = 1; jEnd = 3; Dj = 1; j0 = 1
               elseif(jDir1 == 0) then
                  jBegin = 1; jEnd = nJ/2; Dj = 1
               endif

               if(kDir1 == 1) then
                  kBegin = nK/2; kEnd = nK/2 - 2; Dk = -1; k0 = nK
               elseif(kDir1 == -1) then
                  kBegin = 1; kEnd = 3; Dk = 1; k0 = 1
               elseif(kDir1 == 0) then
                  kBegin = 1; kEnd = nK/2; Dk = 1
               endif

               ! Simple restrictioin for 'inner' cells.
               ! Simple restriction with 6*6*6 fine cells. But some cells
               ! calculated in this way may not accurate and will be
               ! overwritten in the later part.
               do k = kBegin, kEnd, Dk
                  do j = jBegin, jEnd, Dj
                     do i = iBegin, iEnd, Di
                        if(nCalc_III(i,j,k)>0) CYCLE
                        if(kDir1 == 0 .and. &
                             (i==iBegin .and. j==jBegin)) CYCLE
                        if(jDir1 == 0 .and. &
                             i==iBegin .and. k==kBegin) CYCLE
                        if(iDir1 == 0 .and. &
                             j==jBegin .and. k==kBegin) CYCLE
                        do iVar = 1, nVar
                           Cell_III = State_VGB(iVar,&
                                2*i-3:2*i+2,2*j-3:2*j+2,2*k-3:2*k+2,iBlock)
                           ! Some value calculated here is not accurate,
                           ! which will be corrected in the next part.
                           State_VIIIB(iVar,i,j,k,iBlock) = &
                                restrict_high_order_amr(Cell_III, &
                                IsPositiveIn=IsPositive_V(iVar))
                        enddo

                        ! It is somewhat complicated to tell weather it iS
                        ! accurate or not. So, do not set nCalc_III value.

                     enddo ! i
                  enddo ! j
               enddo ! k

               do k = kBegin, kEnd, Dk
                  do j = jBegin, jEnd, Dj
                     do i = iBegin, iEnd, Di
                        if(kDir1 == 0 .and. &
                             .not. (i==iBegin .and. j==jBegin)) CYCLE
                        if(jDir1 == 0 .and. &
                             .not. (i==iBegin .and. k==kBegin)) CYCLE
                        if(iDir1 == 0 .and. &
                             .not. (j==jBegin .and. k==kBegin)) CYCLE

                        do iVar = 1, nVar
                           if(kDir1 == 0) then
                              Cell_I(1) = &
                                   State_VIIIB(iVar,i+2*Di,j+2*Dj,k,iBlock)
                              Cell_I(2) = &
                                   State_VIIIB(iVar,i+  Di,j+  Dj,k,iBlock)
                              Cell_I(3) = &
                                   State_VGB(iVar,i0-  Di,j0-  Dj,2*k,iBlock)
                              Cell_I(4) = &
                                   State_VGB(iVar,i0-2*Di,j0-2*Dj,2*k,iBlock)
                              Cell_I(5) = &
                                   State_VGB(iVar,i0-3*Di,j0-3*Dj,2*k,iBlock)
                           endif

                           if(jDir1 == 0) then
                              Cell_I(1) = &
                                   State_VIIIB(iVar,i+2*Di,j,k+2*Dk,iBlock)
                              Cell_I(2) = &
                                   State_VIIIB(iVar,i+  Di,j,k+  Dk,iBlock)
                              Cell_I(3) = &
                                   State_VGB(iVar,i0-  Di,2*j,k0-  Dk,iBlock)
                              Cell_I(4) = &
                                   State_VGB(iVar,i0-2*Di,2*j,k0-2*Dk,iBlock)
                              Cell_I(5) = &
                                   State_VGB(iVar,i0-3*Di,2*j,k0-3*Dk,iBlock)
                           endif

                           if(iDir1 == 0) then
                              Cell_I(1) = &
                                   State_VIIIB(iVar,i,j+2*Dj,k+2*Dk,iBlock)
                              Cell_I(2) = &
                                   State_VIIIB(iVar,i,j+  Dj,k+  Dk,iBlock)
                              Cell_I(3) = &
                                   State_VGB(iVar,2*i,j0-  Dj,k0-  Dk,iBlock)
                              Cell_I(4) = &
                                   State_VGB(iVar,2*i,j0-2*Dj,k0-2*Dk,iBlock)
                              Cell_I(5) = &
                                   State_VGB(iVar,2*i,j0-3*Dj,k0-3*Dk,iBlock)

                           endif
                           Orig = -c1over10*Cell_I(1) + c1over2*Cell_I(2) + &
                                Cell_I(3) - c1over2*Cell_I(4) + &
                                c1over10*Cell_I(5)
                           nCorrect = nCorrected_III(i,j,k)
                           if(nCorrect == 0) then
                              ! 2D diagonally interpolation.
                              State_VIIIB(iVar,i,j,k,iBlock) = &
                                   limit_interpolation&
                                   (Orig,Cell_I(1:4),Distance_I,&
                                   IsPositiveIn=IsPositive_V(iVar))
                           else
                              ! Corner cell for Case 2.
                              ! Some cells can be corrected in different
                              ! direction. Use the average of these correcitons
                              State_VIIIB(iVar,i,j,k,iBlock) = &
                                   (nCorrect*State_VIIIB(iVar,i,j,k,iBlock) +&
                                   limit_interpolation&
                                   (Orig,Cell_I(1:4),Distance_I,&
                                   IsPositiveIn=IsPositive_V(iVar))) / &
                                   (nCorrect + 1)
                           endif
                        enddo ! iVar

                        ! If it is accurate, it will not be overwritten by
                        ! simple restriction for the inner cell code.
                        nCalc_III(i,j,k) = nCalc_III(i,j,k) + 1

                        nCorrected_III(i,j,k) = nCorrected_III(i,j,k) + 1
                     enddo ! i
                  enddo ! j
               enddo ! k
            enddo; enddo; enddo

            if(nEdge == 0) then
               ! Case 3.
               ! Take care corner neighbour block.
               do kDir1 = -1, 1, 2; do jDir1 = -1, 1, 2; do iDir1 = -1, 1,2
                  if(DiLevelNei_IIIB(iDir1,jDir1,kDir1,iBlock) /=1) CYCLE

                  if(iDir1 == 1) then
                     iBegin = nI/2; iEnd = nI/2 - 2; Di = -1; i0 = nI
                  elseif(iDir1 == -1) then
                     iBegin = 1; iEnd = 3; Di = 1; i0 = 1
                  endif

                  if(jDir1 == 1) then
                     jBegin = nJ/2; jEnd = nJ/2 - 2; Dj = -1; j0 = nJ
                  elseif(jDir1 == -1) then
                     jBegin = 1; jEnd = 3; Dj = 1; j0 = 1
                  endif

                  if(kDir1 == 1) then
                     kBegin = nK/2; kEnd = nK/2 - 2; Dk = -1; k0 = nK
                  elseif(kDir1 == -1) then
                     kBegin = 1; kEnd = 3; Dk = 1; k0 = 1
                  endif

                  ! Simple 6*6*6 restriction.
                  do k = kBegin, kEnd, Dk
                     do j = jBegin, jEnd, Dj
                        do i = iBegin, iEnd, Di
                           if(i == iBegin .and. j == jBegin .and. k == kBegin)&
                                CYCLE
                           do iVar = 1, nVar
                              Cell_III = State_VGB(iVar,&
                                   2*i-3:2*i+2,2*j-3:2*j+2,2*k-3:2*k+2,iBlock)
                              State_VIIIB(iVar,i,j,k,iBlock) = &
                                   restrict_high_order_amr(Cell_III,&
                                   IsPositiveIn=IsPositive_V(iVar))
                           enddo
                        enddo
                     enddo
                  enddo

                  do iVar = 1, nVar
                     ! 3D diagonal interpolation.
                     i = iBegin; j = jBegin; k = kBegin
                     Cell_I(1) = &
                          State_VIIIB(iVar,i+2*Di,j+2*Dj,k+2*Dk,iBlock)
                     Cell_I(2) = &
                          State_VIIIB(iVar,i+  Di,j+  Dj,k+  Dk,iBlock)
                     Cell_I(3) = &
                          State_VGB(iVar,i0-  Di,j0-  Dj,k0-  Dk,iBlock)
                     Cell_I(4) = &
                          State_VGB(iVar,i0-2*Di,j0-2*Dj,k0-2*Dk,iBlock)
                     Cell_I(5) = &
                          State_VGB(iVar,i0-3*Di,j0-3*Dj,k0-3*Dk,iBlock)

                     Orig = -c1over10*Cell_I(1) + c1over2*Cell_I(2) + &
                          Cell_I(3) - c1over2*Cell_I(4) + c1over10*Cell_I(5)
                     State_VIIIB(iVar,i,j,k,iBlock) = &
                          limit_interpolation(Orig,Cell_I(1:4),Distance_I,&
                          IsPositiveIn=IsPositive_V(iVar))

                  enddo ! iVar
               enddo; enddo; enddo
            endif ! nEdge
         endif ! nResChange
      endif ! nK

      IsAccurate_B(iBlock) = .true.

    end subroutine calc_accurate_coarsened_block
    !==========================================================================
  end subroutine message_pass_block
  !============================================================================
  logical function is_only_corner_fine(iNode, iDir0, jDir0, kDir0, &
       iDirCorner, jDirCorner, kDirCorner)

    use BATL_tree, ONLY: find_neighbor_for_anynode

    integer, intent(in):: iNode, iDir0, jDir0, kDir0
    integer, optional, intent(inout):: iDirCorner,jDirCorner,kDirCorner

    integer:: DiLevelNei_III(-1:1,-1:1,-1:1)
    integer:: iDir1, jDir1, kDir1, iDir2, jDir2, kDir2
    integer:: iDirBegin, iDirEnd, jDirBegin, jDirEnd, kDirBegin, kDirEnd
    integer:: DiDir, DjDir, DkDir
    integer:: nRefinedEdge
    logical:: IsOnlyCornerFine
    !--------------------------------------------------------------------------
    call find_neighbor_for_anynode(iNode,DiLevelNei_III)

    IsOnlyCornerFine = .false.
    if(abs(iDir0) + abs(jDir0) + abs(kDir0) == 1) then

       ! Loop through 4 corners block corresponding to this face,
       ! check whether it is finer.
       iDirBegin = -1; iDirEnd = 1; DiDir = 2
       jDirBegin = -1; jDirEnd = 1; DjDir = 2
       kDirBegin = -1; kDirEnd = 1; DkDir = 2

       if(iDir0 /= 0) then
          iDirBegin = iDir0; iDirEnd = iDirBegin; DiDir = 1
       elseif(jDir0 /= 0) then
          jDirBegin = jDir0; jDirEnd = jDirBegin; DjDir = 1
       elseif(kDir0 /= 0) then
          kDirBegin = kDir0; kDirEnd = kDirBegin; DkDir = 1
       endif

       ! 4 corners
       do kDir1=kDirBegin,kDirEnd,DkDir
          do jDir1=jDirBegin,jDirEnd,DjDir
             do iDir1=iDirBegin,iDirEnd,DiDir
                if(DiLevelNei_III(iDir1,jDir1,kDir1) == 0) then
                   nRefinedEdge = 0

                   ! Check first edge block.
                   if(iDir0 /= 0) then
                      iDir2 = iDir1
                      jDir2 = jDir1
                      kDir2 = 0
                   endif
                   if(jDir0 /= 0) then
                      jDir2 = jDir1
                      kDir2 = kDir1
                      iDir2 = 0
                   endif
                   if(kDir0 /= 0) then
                      kDir2 = kDir1
                      iDir2 = iDir1
                      jDir2 = 0
                   endif
                   if(DiLevelNei_III(iDir2,jDir2,kDir2) == 0) &
                        nRefinedEdge = nRefinedEdge + 1

                   ! Check second edge block.
                   if(iDir0 /= 0) then
                      iDir2 = iDir1
                      jDir2 = 0
                      kDir2 = kDir1
                   endif
                   if(jDir0 /= 0) then
                      jDir2 = jDir1
                      kDir2 = 0
                      iDir2 = iDir1
                   endif
                   if(kDir0 /= 0) then
                      kDir2 = kDir1
                      iDir2 = 0
                      jDir2 = jDir1
                   endif
                   if(DiLevelNei_III(iDir2,jDir2,kDir2) == 0) &
                        nRefinedEdge = nRefinedEdge + 1

                   if(nRefinedEdge == 0) then
                      IsOnlyCornerFine = .true.
                      if(present(iDirCorner)) iDirCorner = iDir1
                      if(present(jDirCorner)) jDirCorner = jDir1
                      if(present(kDirCorner)) kDirCorner = kDir1
                   endif
                endif
             enddo
          enddo
       enddo
    endif
    is_only_corner_fine = IsOnlyCornerFine
  end function is_only_corner_fine
  !============================================================================
end module BATL_pass_cell
!==============================================================================
