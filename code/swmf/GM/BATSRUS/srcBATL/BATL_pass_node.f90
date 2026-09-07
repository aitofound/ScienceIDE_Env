!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module BATL_pass_node

  use BATL_geometry, ONLY: &
       IsCylindricalAxis, IsSphericalAxis, IsLatitudeAxis, Lat_, Theta_
  use ModNumConst, ONLY: cPi, cHalfPi

  use ModUtilities, ONLY: CON_stop

  ! Possible improvements:
  ! (1) Instead of sending the receiving block number
  !     and the 3*nDim range limits and strides, we can send only a tag.
  ! (2) Instead of waiting for receiving buffers from ALL processors,
  !     we can wait for ANY receiving and already start unpacking

  implicit none

  SAVE

  private ! except

  public message_pass_node

contains
  !============================================================================
  subroutine message_pass_node(nVar, State_VNB, NameOperatorIn, &
       UsePeriodicCoordIn)

    ! State_VNB contains node centered data. The same node may occur in
    ! up to 8 different blocks.
    !
    ! NameOperatorIn = 'mean', 'min', 'max' (default is 'mean')
    ! controls if we take the average, the minimum, or the maximum of
    ! the co-located values and put it back into State_VNB.
    !
    ! UsePeriodicCoordIn tells that nodes next to truly periodic boundaries
    ! should be combined.

    use BATL_size, ONLY: &
         nBlock, nIJK_D, &
         MaxDim, nDim, iRatio, jRatio, kRatio, iRatio_D,  &
         nI, nJ, nK, nBlock, nINode, nJNode, nKNode, nIJKNode_D

    use BATL_mpi, ONLY: iComm, nProc, barrier_mpi

    use BATL_tree, ONLY: &
         iNodeNei_IIIB, DiLevelNei_IIIB, Unused_B, Unused_BP, iNode_B, &
         iTree_IA, Proc_, Block_, Coord1_, Coord2_, Coord3_, &
         set_tree_periodic

    use BATL_grid, ONLY: CoordMin_DB, CoordMax_DB

    use ModMpi

    use ModUtilities, ONLY: lower_case

    ! Arguments
    integer, intent(in) :: nVar
    real, intent(inout) :: State_VNB(nVar,nI+1,nJ+1,nK+1,nBlock)

    ! Optional arguments
    character(len=*), optional, intent(in) :: NameOperatorIn
    logical,          optional, intent(in) :: UsePeriodicCoordIn

    ! Local variables

    logical, parameter :: UseRSend = .false.

    character(len=4) :: NameOperator
    logical          :: UsePeriodicCoord

    integer :: iCountOnly     ! index for 2 stage scheme for count, sendrecv
    logical :: DoCountOnly    ! logical for count vs. sendrecv stages

    integer :: iSend, jSend, kSend, iRecv, jRecv, kRecv, iSide, jSide, kSide
    integer :: iDir, jDir, kDir
    integer :: iNodeRecv, iNodeSend
    integer :: iBlockRecv, iProcRecv, iBlockSend, iProcSend, DiLevel

    ! Is the sending node next to the symmetry axis?
    logical :: IsAxisNode

    ! Fast lookup tables for index ranges per dimension
    integer, parameter:: Min_=1, Max_=2
    integer:: iEqualS_DII(MaxDim,-1:1,Min_:Max_)
    integer:: iEqualR_DII(MaxDim,-1:1,Min_:Max_)
    integer:: iRestrictS_DII(MaxDim,-1:1,Min_:Max_)
    integer:: iRestrictR_DII(MaxDim,0:3,Min_:Max_)
    integer:: iProlongS_DII(MaxDim,0:3,Min_:Max_)
    integer:: iProlongR_DII(MaxDim,0:3,Min_:Max_)

    ! Index range for recv and send segments of the blocks
    integer :: iRMin, iRMax, jRMin, jRMax, kRMin, kRMax
    integer :: iSMin, iSMax, jSMin, jSMax, kSMin, kSMax

    ! Variables related to recv and send buffers
    integer, allocatable, save:: iBufferS_P(:), nBufferS_P(:), nBufferR_P(:)

    integer :: iBufferS, iBufferR, nBuffer
    integer :: MaxBufferS = -1, MaxBufferR = -1
    real, allocatable, save:: BufferR_I(:), BufferS_I(:)

    integer:: iRequestR, iRequestS, iError

    integer, allocatable, save:: iRequestR_I(:), iRequestS_I(:)

    integer, allocatable :: nCount_NB(:,:,:,:)

    logical:: UseMin=.false., UseMax=.false., UseMean=.false.

    character(len=*), parameter:: NameSub = 'message_pass_node'
    !--------------------------------------------------------------------------
    call timing_start('pass_node')

    ! call timing_start('init_pass_node')

    UseMin =.false.
    UseMax =.false.
    UseMean=.false.

    if(present(NameOperatorIn)) then
       NameOperator = adjustl(NameOperatorIn)
       call lower_case(NameOperator)
       select case(NameOperator)
       case("min")
          UseMin=.true.
       case("max")
          UseMax=.true.
       case("mean")
          UseMean=.true.
       case default
          call CON_stop(NameSub// ' unknown NameOperator='//trim(NameOperator))
       end select
    else
       UseMean = .true.
    end if

    if(UseMean) then
       ! Counter for the number of values to be averaged
       allocate(nCount_NB(nI+1,nJ+1,nK+1,nBlock))
       nCount_NB(:,:,:,:) = 1
    end if

    UsePeriodicCoord = .false.
    if(present(UsePeriodicCoordIn)) UsePeriodicCoord = UsePeriodicCoordIn

    ! If required switch off periodicity before passing the nodes
    if(UsePeriodicCoord) call set_tree_periodic(.false.)

    ! Set index ranges based on arguments
    call set_range

    ! Allocate fixed size communication arrays.
    if(.not.allocated(iBufferS_P))then
       allocate(iBufferS_P(0:nProc-1), nBufferS_P(0:nProc-1), &
            nBufferR_P(0:nProc-1))
       allocate(iRequestR_I(nProc), iRequestS_I(nProc))
    end if

    ! call timing_stop('init_pass_node')

    do iCountOnly = 1, 2
       DoCountOnly = iCountOnly == 1

       ! call timing_start('local_pass_node')

       if(DoCountOnly)then
          ! initialize buffer size
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

       ! Loop through all blocks that may send a message
       do iBlockSend = 1, nBlock

          if(Unused_B(iBlockSend)) CYCLE

          iNodeSend = iNode_B(iBlockSend)

          IsAxisNode = .false.

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

                if(nDim > 2 .and. IsSphericalAxis) IsAxisNode = &
                     jDir == -1 .and. &
                     CoordMin_DB(Theta_,iBlockSend) < 1e-8 .or. &
                     jDir == +1 .and. &
                     CoordMax_DB(Theta_,iBlockSend) > cPi-1e-8

                do iDir = -1,1
                   ! Ignore inner parts of the sending block
                   if(iDir == 0 .and. jDir == 0 .and. kDir == 0) CYCLE

                   if(nDim > 1 .and. IsCylindricalAxis) IsAxisNode = &
                        iDir == -1 .and. iTree_IA(Coord1_,iNodeSend) == 1

                   DiLevel = DiLevelNei_IIIB(iDir,jDir,kDir,iBlockSend)

                   if(DiLevel == 0)then
                      call do_equal
                   elseif(DiLevel == 1)then
                      call do_restrict
                   elseif(DiLevel == -1)then
                      call do_prolong
                   endif
                end do ! iDir
             end do ! jDir
          end do ! kDir
       end do ! iBlockSend

       ! call timing_stop('local_pass_node')

    end do ! iCountOnly

    if(nProc == 1)then
       nBuffer = nBufferS_P(0)
       if(nBuffer > 0) BufferR_I(1:nBuffer) = BufferS_I(1:nBuffer)
    else
       ! post requests
       ! call timing_start('recv_pass_node')
       iRequestR = 0
       iBufferR  = 1
       do iProcSend = 0, nProc - 1
          if(nBufferR_P(iProcSend) == 0) CYCLE
          iRequestR = iRequestR + 1
          call MPI_irecv(BufferR_I(iBufferR), nBufferR_P(iProcSend), &
               MPI_REAL, iProcSend, 40, iComm, iRequestR_I(iRequestR), &
               iError)
          iBufferR  = iBufferR  + nBufferR_P(iProcSend)
       end do
       ! call timing_stop('recv_pass_node')

       if(UseRSend) then
          ! call timing_start('barrier_pass_node')
          call barrier_mpi
          ! call timing_stop('barrier_pass_node')
       end if

       ! post sends
       ! call timing_start('send_pass_node')
       iRequestS = 0
       iBufferS  = 1
       do iProcRecv = 0, nProc-1
          if(nBufferS_P(iProcRecv) == 0) CYCLE
          iRequestS = iRequestS + 1

          if(UseRSend)then
             call MPI_rsend(BufferS_I(iBufferS), nBufferS_P(iProcRecv), &
                  MPI_REAL, iProcRecv, 40, iComm, iError)
          else
             call MPI_isend(BufferS_I(iBufferS), nBufferS_P(iProcRecv), &
                  MPI_REAL, iProcRecv, 40, iComm, iRequestS_I(iRequestS), &
                  iError)
          end if

          iBufferS  = iBufferS  + nBufferS_P(iProcRecv)
       end do
       ! call timing_stop('send_pass_node')

       ! wait for all requests to be completed
       ! call timing_start('wait_pass_node')
       if(iRequestR > 0) call MPI_waitall( &
            iRequestR, iRequestR_I, MPI_STATUSES_IGNORE, iError)

       ! wait for all sends to be completed
       if(.not.UseRSend .and. iRequestS > 0) call MPI_waitall( &
            iRequestS, iRequestS_I, MPI_STATUSES_IGNORE, iError)

       ! call timing_stop('wait_pass_node')
    end if

    ! call timing_start('buffer_pass_node')
    call buffer_to_state
    ! call timing_stop('buffer_pass_node')

    if(UseMean) deallocate(nCount_NB)

    ! If required switch back periodicity after passing the nodes
    if(UsePeriodicCoord) call set_tree_periodic(.true.)

    call timing_stop('pass_node')

  contains
    !==========================================================================
    subroutine buffer_to_state

      ! Copy buffer into recv block of State_VNB

      integer:: iBufferR, i, j, k, iVar, iBlock
      integer:: nDx,nDy,nDz
      !------------------------------------------------------------------------
      jRMin = 1; jRMax = 1
      kRMin = 1; kRMax = 1
      nDy = 1; nDz = 1

      iBufferR = 0
      do iProcSend = 0, nProc-1
         if(nBufferR_P(iProcSend) == 0) CYCLE

         do
            iBlockRecv = nint(BufferR_I(iBufferR+1))
            iRMin      = nint(BufferR_I(iBufferR+2))
            iRMax      = nint(BufferR_I(iBufferR+3))
            nDx        = nint(BufferR_I(iBufferR+4))
            if(nDim > 1) jRMin = nint(BufferR_I(iBufferR+5))
            if(nDim > 1) jRMax = nint(BufferR_I(iBufferR+6))
            if(nDim > 1) nDy   = nint(BufferR_I(iBufferR+7))
            if(nDim > 2) kRMin = nint(BufferR_I(iBufferR+8))
            if(nDim > 2) kRMax = nint(BufferR_I(iBufferR+9))
            if(nDim > 2) nDz   = nint(BufferR_I(iBufferR+10))

            iBufferR = iBufferR + 1 + 3*nDim

            if(UseMean) then
               do k=kRMin,kRMax,nDz; do j=jRMin,jRMax,nDy; do i=iRMin,iRMax,nDx
                  do iVar=1,nVar
                     State_VNB(iVar,i,j,k,iBlockRecv) = &
                          State_VNB(iVar,i,j,k,iBlockRecv) &
                          + BufferR_I(iBufferR+iVar)
                  end do
                  nCount_NB(i,j,k,iBlockRecv) = nCount_NB(i,j,k,iBlockRecv) + 1
                  iBufferR = iBufferR + nVar
               end do; end do; end do
            else if (UseMin) then
               do k=kRMin,kRMax,nDz; do j=jRMin,jRMax,nDy; do i=iRMin,iRMax,nDx
                  do iVar=1,nVar
                     State_VNB(iVar,i,j,k,iBlockRecv) = &
                          min(State_VNB(iVar,i,j,k,iBlockRecv), &
                          BufferR_I(iBufferR+iVar))
                  end do
                  iBufferR = iBufferR + nVar
               end do; end do; end do
            else if (UseMax) then
               do k=kRMin,kRMax,nDz; do j=jRMin,jRMax,nDy; do i=iRMin,iRMax,nDx
                  do iVar=1,nVar
                     State_VNB(iVar,i,j,k,iBlockRecv) = &
                          max(State_VNB(iVar,i,j,k,iBlockRecv), &
                          BufferR_I(iBufferR+iVar))
                  end do
                  iBufferR = iBufferR + nVar
               end do; end do; end do
            end if
            if(iBufferR >= sum(nBufferR_P(0:iProcSend))) EXIT
         end do
      end do

      if(UseMean) then
         do iBlock = 1, nBlock
            if(Unused_B(iBlock)) CYCLE
            do k = 1, nKNode; do j = 1, nJNode; do i = 1, nINode;
               if( nCount_NB(i,j,k,iBlock) > 1 ) then
                  State_VNB(:,i,j,k,iBlock) = &
                       State_VNB(:,i,j,k,iBlock)/nCount_NB(i,j,k,iBlock)
               end if
            end do; end do; end do
         end do
      end if

    end subroutine buffer_to_state
    !==========================================================================
    subroutine do_equal

      integer :: iBufferS, i, j, k, nSize
      !------------------------------------------------------------------------
      iSend = (3*iDir + 3)/2
      jSend = (3*jDir + 3)/2
      kSend = (3*kDir + 3)/2

      iNodeRecv  = iNodeNei_IIIB(iSend,jSend,kSend,iBlockSend)
      iProcRecv  = iTree_IA(Proc_,iNodeRecv)
      iBlockRecv = iTree_IA(Block_,iNodeRecv)

      ! For part steady/implicit schemes
      if(Unused_BP(iBlockRecv,iProcRecv)) RETURN

      iRMin = iEqualR_DII(1,iDir,Min_)
      iRMax = iEqualR_DII(1,iDir,Max_)
      jRMin = iEqualR_DII(2,jDir,Min_)
      jRMax = iEqualR_DII(2,jDir,Max_)
      kRMin = iEqualR_DII(3,kDir,Min_)
      kRMax = iEqualR_DII(3,kDir,Max_)

      if(DoCountOnly)then
         ! Number of reals to send to and received from the other processor
         nSize = nVar*(iRMax-iRMin+1)*(jRMax-jRMin+1)*(kRMax-kRMin+1) &
              + 1 + 3*nDim

         nBufferR_P(iProcRecv) = nBufferR_P(iProcRecv) + nSize
         nBufferS_P(iProcRecv) = nBufferS_P(iProcRecv) + nSize
         RETURN
      end if

      if(IsAxisNode)then
         if(IsLatitudeAxis)then
            kRMin = iEqualR_DII(3,-kDir,Min_)
            kRMax = iEqualR_DII(3,-kDir,Max_)
         elseif(IsSphericalAxis)then
            jRMin = iEqualR_DII(2,-jDir,Min_)
            jRMax = iEqualR_DII(2,-jDir,Max_)
         elseif(IsCylindricalAxis)then
            iRMin = iEqualR_DII(1,1,Min_)
            iRMax = iEqualR_DII(1,1,Max_)
         end if
      end if

      iSMin = iEqualS_DII(1,iDir,Min_)
      iSMax = iEqualS_DII(1,iDir,Max_)
      jSMin = iEqualS_DII(2,jDir,Min_)
      jSMax = iEqualS_DII(2,jDir,Max_)
      kSMin = iEqualS_DII(3,kDir,Min_)
      kSMax = iEqualS_DII(3,kDir,Max_)

      ! Put data into the send buffer
      iBufferS = iBufferS_P(iProcRecv)

      BufferS_I(            iBufferS+1)  = iBlockRecv
      BufferS_I(            iBufferS+2)  = iRMin
      BufferS_I(            iBufferS+3)  = iRMax
      BufferS_I(            iBufferS+4)  = 1
      if(nDim > 1)BufferS_I(iBufferS+5)  = jRMin
      if(nDim > 1)BufferS_I(iBufferS+6)  = jRMax
      if(nDim > 1)BufferS_I(iBufferS+7)  = 1
      if(nDim > 2)BufferS_I(iBufferS+8)  = kRMin
      if(nDim > 2)BufferS_I(iBufferS+9)  = kRMax
      if(nDim > 2)BufferS_I(iBufferS+10) = 1

      iBufferS = iBufferS + 1 + 2*nDim + nDim

      do k = kSMin,kSmax; do j = jSMin,jSMax; do i = iSMin,iSmax
         BufferS_I(iBufferS+1:iBufferS+nVar) = State_VNB(:,i,j,k,iBlockSend)
         iBufferS = iBufferS + nVar
      end do; end do; end do

      iBufferS_P(iProcRecv) = iBufferS

    end subroutine do_equal
    !==========================================================================
    subroutine do_restrict

      integer :: iS, jS, kS, iVar
      integer :: iBufferS, nSize

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
      iBlockRecv = iTree_IA(Block_,iNodeRecv)

      ! For part steady/implicit schemes
      if(Unused_BP(iBlockRecv,iProcRecv)) RETURN

      if(DoCountOnly)then
         ! This processor will receive a prolonged buffer from
         ! the other processor and the "recv" direction of the prolongations
         ! will be the same as the "send" direction for this restriction:
         ! iSend,kSend,jSend = 0..3

         iRmin = 1; iRMax = 1
         jRmin = 1; jRMax = 1
         kRmin = 1; kRMax = 1

         iRMin = iProlongR_DII(1,iSend,Min_)
         iRMax = iProlongR_DII(1,iSend,Max_)
         if(nDim > 1) jRMin = iProlongR_DII(2,jSend,Min_)
         if(nDim > 1) jRMax = iProlongR_DII(2,jSend,Max_)
         if(nDim > 2) kRMin = iProlongR_DII(3,kSend,Min_)
         if(nDim > 2) kRMax = iProlongR_DII(3,kSend,Max_)

         nSize = nVar*((iRMax-iRMin)/iRatio_D(1)+1) &
              *((jRMax-jRMin)/iRatio_D(2)+1) &
              *((kRMax-kRMin)/iRatio_D(3)+1) &
              + 1 + 2*nDim + nDim
         ! if(present(Time_B)) nSize = nSize + 1
         nBufferR_P(iProcRecv) = nBufferR_P(iProcRecv) + nSize
      end if

      iRecv = iSend - 3*iDir
      jRecv = jSend - 3*jDir
      kRecv = kSend - 3*kDir

      ! Set defaults for ignored dimensions
      iRmin = 1; iRMax = 1
      jRmin = 1; jRMax = 1
      kRmin = 1; kRMax = 1

      ! Receiving range depends on iRecv,kRecv,jRecv = 0..3
      iRMin = iRestrictR_DII(1,iRecv,Min_)
      iRMax = iRestrictR_DII(1,iRecv,Max_)
      if(nDim > 1) jRMin = iRestrictR_DII(2,jRecv,Min_)
      if(nDim > 1) jRMax = iRestrictR_DII(2,jRecv,Max_)
      if(nDim > 2) kRMin = iRestrictR_DII(3,kRecv,Min_)
      if(nDim > 2) kRMax = iRestrictR_DII(3,kRecv,Max_)

      if(DoCountOnly)then
         ! Number of reals to send to the other processor
         nSize = nVar*((iRMax-iRMin)+1) &
              *((jRMax-jRMin)+1) &
              *((kRMax-kRMin)+1) &
              + 1 + 3*nDim

         nBufferS_P(iProcRecv) = nBufferS_P(iProcRecv) + nSize
         RETURN
      end if

      if(IsAxisNode)then
         if(IsLatitudeAxis)then
            kRMin = iRestrictR_DII(3,kSend,Min_)
            kRMax = iRestrictR_DII(3,kSend,Max_)
         elseif(IsSphericalAxis)then
            jRMin = iRestrictR_DII(2,jSend,Min_)
            jRMax = iRestrictR_DII(2,jSend,Max_)
         elseif(IsCylindricalAxis)then
            iRMin = iRestrictR_DII(1,0,Min_)
            iRMax = iRestrictR_DII(1,0,Max_)
         end if
      end if

      iBufferS = iBufferS_P(iProcRecv)

      BufferS_I(            iBufferS+1)  = iBlockRecv
      BufferS_I(            iBufferS+2)  = iRMin
      BufferS_I(            iBufferS+3)  = iRMax
      BufferS_I(            iBufferS+4)  = 1
      if(nDim > 1)BufferS_I(iBufferS+5)  = jRMin
      if(nDim > 1)BufferS_I(iBufferS+6)  = jRMax
      if(nDim > 1)BufferS_I(iBufferS+7)  = 1
      if(nDim > 2)BufferS_I(iBufferS+8)  = kRMin
      if(nDim > 2)BufferS_I(iBufferS+9)  = kRMax
      if(nDim > 2)BufferS_I(iBufferS+10) = 1

      iBufferS = iBufferS + 1 + 3*nDim

      ! Index range that gets restricted depends on iDir,jDir,kDir only
      iSMin = iRestrictS_DII(1,iDir,Min_)
      iSMax = iRestrictS_DII(1,iDir,Max_)
      jSMin = iRestrictS_DII(2,jDir,Min_)
      jSMax = iRestrictS_DII(2,jDir,Max_)
      kSMin = iRestrictS_DII(3,kDir,Min_)
      kSMax = iRestrictS_DII(3,kDir,Max_)

      do kS = kSMin,kSMax,iRatio_D(3)
         do jS = jSMin,jSMax,iRatio_D(2)
            do iS = iSMin,iSMax,iRatio_D(1)
               do iVar = 1, nVar
                  BufferS_I(iBufferS+iVar) = &
                       State_VNB(iVar,iS,jS,kS,iBlockSend)
               end do
               iBufferS = iBufferS + nVar
            end do
         end do
      end do
      iBufferS_P(iProcRecv) = iBufferS

    end subroutine do_restrict
    !==========================================================================
    subroutine do_prolong

      integer :: iS, jS, kS
      integer :: iBufferS, nSize

      ! Loop through the subfaces or subedges
      !------------------------------------------------------------------------
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
               iBlockRecv = iTree_IA(Block_,iNodeRecv)

               ! For part steady/implicit schemes
               if(Unused_BP(iBlockRecv,iProcRecv)) CYCLE

               if(DoCountOnly)then
                  ! This processor will receive a restricted buffer from
                  ! the other processor and the "recv" direction of the
                  ! restriction will be the same as the "send" direction for
                  ! this prolongation: iSend,kSend,jSend = 0..3

                  iRmin = 1; iRMax = 1
                  jRmin = 1; jRMax = 1
                  kRmin = 1; kRMax = 1

                  iRMin = iRestrictR_DII(1,iSend,Min_)
                  iRMax = iRestrictR_DII(1,iSend,Max_)
                  if(nDim > 1) jRMin = iRestrictR_DII(2,jSend,Min_)
                  if(nDim > 1) jRMax = iRestrictR_DII(2,jSend,Max_)
                  if(nDim > 2) kRMin = iRestrictR_DII(3,kSend,Min_)
                  if(nDim > 2) kRMax = iRestrictR_DII(3,kSend,Max_)

                  nSize = nVar*((iRMax-iRMin)+1) &
                       *((jRMax-jRMin)+1) &
                       *((kRMax-kRMin)+1) &
                       + 1 + 2*nDim + nDim

                  nBufferR_P(iProcRecv) = nBufferR_P(iProcRecv) + nSize

               end if

               ! Receiving range depends on iRecv,kRecv,jRecv = 0..3
               iRmin = 1; iRMax = 1
               jRmin = 1; jRMax = 1
               kRmin = 1; kRMax = 1

               iRMin = iProlongR_DII(1,iRecv,Min_)
               iRMax = iProlongR_DII(1,iRecv,Max_)
               if(nDim > 1) jRMin = iProlongR_DII(2,jRecv,Min_)
               if(nDim > 1) jRMax = iProlongR_DII(2,jRecv,Max_)
               if(nDim > 2) kRMin = iProlongR_DII(3,kRecv,Min_)
               if(nDim > 2) kRMax = iProlongR_DII(3,kRecv,Max_)

               if(DoCountOnly)then
                  ! Number of reals to send to the other processor
                  nSize = nVar*((iRMax-iRMin)/iRatio_D(1)+1) &
                       *((jRMax-jRMin)/iRatio_D(2)+1) &
                       *((kRMax-kRMin)/iRatio_D(3)+1) &
                       + 1 + 3*nDim

                  nBufferS_P(iProcRecv) = nBufferS_P(iProcRecv) + nSize
                  CYCLE
               end if

               if(IsAxisNode)then
                  if(IsLatitudeAxis)then
                     kRMin = iProlongR_DII(3,kSend,Min_)
                     kRMax = iProlongR_DII(3,kSend,Max_)
                  elseif(IsSphericalAxis)then
                     jRMin = iProlongR_DII(2,jSend,Min_)
                     jRMax = iProlongR_DII(2,jSend,Max_)
                  elseif(IsCylindricalAxis)then
                     iRMin = iProlongR_DII(1,0,Min_)
                     iRMax = iProlongR_DII(1,0,Max_)
                  end if
               end if

               iBufferS = iBufferS_P(iProcRecv)

               BufferS_I(            iBufferS+1)  = iBlockRecv
               BufferS_I(            iBufferS+2)  = iRMin
               BufferS_I(            iBufferS+3)  = iRMax
               BufferS_I(            iBufferS+4)  = iRatio_D(1)
               if(nDim > 1)BufferS_I(iBufferS+5)  = jRMin
               if(nDim > 1)BufferS_I(iBufferS+6)  = jRMax
               if(nDim > 1)BufferS_I(iBufferS+7)  = iRatio_D(2)
               if(nDim > 2)BufferS_I(iBufferS+8)  = kRMin
               if(nDim > 2)BufferS_I(iBufferS+9)  = kRMax
               if(nDim > 2)BufferS_I(iBufferS+10) = iRatio_D(3)

               iBufferS = iBufferS + 1 + 3*nDim

               ! Sending range depends on iSend,jSend,kSend = 0..3

               iSMin = 1; iSMax = 1
               jSMin = 1; jSMax = 1
               kSMin = 1; kSMax = 1

               iSMin = iProlongS_DII(1,iSend,Min_)
               iSMax = iProlongS_DII(1,iSend,Max_)
               if(nDim > 1) jSMin = iProlongS_DII(2,jSend,Min_)
               if(nDim > 1) jSMax = iProlongS_DII(2,jSend,Max_)
               if(nDim > 2) kSMin = iProlongS_DII(3,kSend,Min_)
               if(nDim > 2) kSMax = iProlongS_DII(3,kSend,Max_)

               do kS=kSMin,kSMax
                  do jS=jSMin,jSMax
                     do iS=iSMin,iSMax
                        BufferS_I(iBufferS+1:iBufferS+nVar)= &
                             State_VNB(:,iS,jS,kS,iBlockSend)
                        iBufferS = iBufferS + nVar
                     end do
                  end do
               end do

               iBufferS_P(iProcRecv) = iBufferS

            end do
         end do
      end do

    end subroutine do_prolong
    !==========================================================================
    subroutine set_range

      integer:: iDim
      !------------------------------------------------------------------------
      ! Indexed by iDir/jDir/kDir for sender = -1,0,1
      iEqualS_DII(:,-1,Min_) = 1
      iEqualS_DII(:,-1,Max_) = 1
      iEqualS_DII(:, 0,Min_) = 1
      iEqualS_DII(:, 0,Max_) = nIJKNode_D
      iEqualS_DII(:, 1,Min_) = nIJKNode_D
      iEqualS_DII(:, 1,Max_) = nIJKNode_D

      ! Indexed by iDir/jDir/kDir for sender = -1,0,1
      iEqualR_DII(:,-1,Min_) = nIJKNode_D
      iEqualR_DII(:,-1,Max_) = nIJKNode_D
      iEqualR_DII(:, 0,Min_) = 1
      iEqualR_DII(:, 0,Max_) = nIJKNode_D
      iEqualR_DII(:, 1,Min_) = 1
      iEqualR_DII(:, 1,Max_) = 1

      ! Indexed by iDir/jDir/kDir for sender = -1,0,1
      iRestrictS_DII(:,-1,Min_) = 1
      iRestrictS_DII(:, 0,Min_) = 1
      iRestrictS_DII(:, 0,Max_) = nIJKNode_D
      iRestrictS_DII(:, 1,Max_) = nIJKNode_D
      iRestrictS_DII(:,-1,Max_) = 1
      iRestrictS_DII(:, 1,Min_) = nIJKNode_D

      ! Indexed by iRecv/jRecv/kRecv = 0..3
      iRestrictR_DII = 1
      iRestrictR_DII(:,0,Min_) = 1
      iRestrictR_DII(:,0,Max_) = 1
      iRestrictR_DII(:,1,Min_) = 1
      do iDim = 1, MaxDim
         ! This loop is used to avoid the NAG 5.1 (282) bug on nyx
         iRestrictR_DII(iDim,1,Max_) = nIJK_D(iDim)/iRatio_D(iDim) + 1
         iRestrictR_DII(iDim,2,Min_) = nIJK_D(iDim)/iRatio_D(iDim) + 1
      end do
      iRestrictR_DII(:,2,Max_) = nIJKNode_D
      iRestrictR_DII(:,3,Min_) = nIJKNode_D
      iRestrictR_DII(:,3,Max_) = nIJKNode_D

      ! Indexed by iSend/jSend,kSend = 0..3
      do iDim = 1, MaxDim
         ! This loop is used to avoid the NAG 5.1 (282) bug on nyx
         iProlongS_DII(iDim,0,Min_) = 1
         iProlongS_DII(iDim,0,Max_) = 1
         iProlongS_DII(iDim,1,Min_) = 1
         iProlongS_DII(iDim,1,Max_) = nIJK_D(iDim)/iRatio_D(iDim) + 1
         iProlongS_DII(iDim,2,Min_) = nIJK_D(iDim)/iRatio_D(iDim) + 1
         iProlongS_DII(iDim,2,Max_) = nIJKNode_D(iDim)
         iProlongS_DII(iDim,3,Min_) = nIJKNode_D(iDim)
         iProlongS_DII(iDim,3,Max_) = nIJKNode_D(iDim)
      end do

      ! Indexed by iRecv/jRecv/kRecv = 0,1,2,3
      iProlongR_DII(:, 0,Min_) = 1
      iProlongR_DII(:, 0,Max_) = 1
      iProlongR_DII(:, 1,Min_) = 1
      iProlongR_DII(:, 1,Max_) = nIJKNode_D
      iProlongR_DII(:, 2,Min_) = 1
      iProlongR_DII(:, 2,Max_) = nIJKNode_D
      iProlongR_DII(:, 3,Min_) = nIJKNode_D
      iProlongR_DII(:, 3,Max_) = nIJKNode_D

    end subroutine set_range
    !==========================================================================
  end subroutine message_pass_node
  !============================================================================
end module BATL_pass_node
!==============================================================================
