!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module CON_ray_trace

  ! Provides the infrastructure for parallel tracing of a vector field:
  ! for velocity field stream lines, for magnetic field the field lines,
  ! in general rays.
  !
  ! Each processor is working on the ray segments inside their subdomain.
  ! The processors periodically exchange ray information with the other
  ! processors. The ray information contains the starting position,
  ! the rank of the starting processor, the current position,
  ! the direction of the ray relative to the vector field (parallel or
  ! antiparallel), the status of the ray tracing (done or in progress).

  use ModMpi
  use ModUtilities, ONLY: CON_stop

  implicit none

  save    ! save all variables

  private ! except

  public :: ray_init          ! Initialize module
  public :: ray_clean         ! Clean up storage
  public :: ray_put           ! Put information about a ray to be continued
  public :: ray_get           ! Get information about a ray to be continued
  public :: ray_exchange      ! Exchange ray information with other processors
  public :: ray_test          ! Unit tester

  ! revision history:
  ! 31Jan04 - Gabor Toth <gtoth@umich.edu> - initial prototype/prolog/code
  ! 26Mar04 - Gabor Toth added the passing of nValue real values
  ! 31Mar04 - Gabor Toth removed the passing of nValue real values
  !                      changed XyzStart_D(3) into iStart_D(4)

  ! Private constants
  character(len=*),  parameter :: NameMod='CON_ray_trace'

  ! Named indexes for ray position
  integer, parameter :: &
       RayStartI_=1, RayStartJ_=2, RayStartK_=3, &
       RayStartBlock_=4, RayStartProc_=5, &
       RayEndX_=6, RayEndY_=7, RayEndZ_=8, RayLength_=9, RayDir_=10, &
       RayDone_=11

  ! Minimum dimensionality of ray info
  integer, parameter :: nRayInfo = 11

  ! Private type
  ! Contains nRay rays with full information
  type RayPtr
     integer       :: nRay, MaxRay
     real, pointer :: Ray_VI(:,:)
  end type RayPtr

  ! Private variables

  ! Ray buffers for neighboring communication
  type(RayPtr), pointer :: SendNei_P(:)     ! Buffer for sending rays to start PE-s
  type(RayPtr), target  :: RecvNei          ! Rays received from end PE-s
  integer, pointer      :: nRayRecvNei_P(:) ! Number of rays recv from end PE-s

  ! Ray buffers for end communication
  type(RayPtr), pointer :: SendEnd_P(:)     ! Buffer for sending rays to start PE-s
  type(RayPtr), target  :: RecvEnd          ! Rays received from end PE-s
  integer, pointer      :: nRayRecvEnd_P(:) ! Number of rays recv from end PE-s

  ! MPI variables
  integer               :: iComm=MPI_COMM_NULL, nProc, iProc ! MPI group info
  integer               :: nRequest
  integer, allocatable  :: iRequest_I(:), iStatus_II(:,:)

  integer               :: iError              ! generic MPI error value

contains
  !============================================================================

  subroutine ray_init(iCommIn)

    integer, intent(in) :: iCommIn  ! MPI communicator for the processors

    ! Initialize the ray tracing for the MPI communicator iCommIn.
    ! If called multiple times, it checks if iCommIn is different from the
    ! current communicator. If the same, nothing is done, if different,
    ! the current storage is removed and new storage is allocated.

    integer :: jProc
    character(len=*), parameter:: NameSub = 'ray_init'
    !--------------------------------------------------------------------------

    if(iComm == iCommIn) RETURN               ! nothing to do if the same comm

    if(iComm /= MPI_COMM_NULL) call ray_clean ! clean up previous allocation

    ! Store MPI information
    iComm = iCommIn
    call MPI_COMM_SIZE (iComm, nProc,  iError)
    call MPI_COMM_RANK (iComm, iProc,  iError)

    ! Allocate MPI variables for non-blocking exchange
    ! At most nProc messages are sent
    allocate(iRequest_I(nProc), iStatus_II(MPI_STATUS_SIZE, nProc))

    ! Initialize send buffers
    allocate(SendNei_P(0:nProc-1))
    do jProc=0, nProc-1
       SendNei_P(jProc) % nRay   = 0
       SendNei_P(jProc) % MaxRay = 0
       nullify(SendNei_P(jProc) % Ray_VI)
    end do

    ! Initialize receive buffer
    allocate(nRayRecvNei_P(0:nProc-1))
    RecvNei % nRay   = 0
    RecvNei % MaxRay = 0
    nullify(RecvNei % Ray_VI)

    ! Initialize send buffers to start
    allocate(SendEnd_P(0:nProc-1))
    do jProc=0, nProc-1
       SendEnd_P(jProc) % nRay   = 0
       SendEnd_P(jProc) % MaxRay = 0
       nullify(SendEnd_P(jProc) % Ray_VI)
    end do

    ! Initialize receive buffer from end
    allocate(nRayRecvEnd_P(0:nProc-1))
    RecvEnd % nRay   = 0
    RecvEnd % MaxRay = 0
    nullify(RecvEnd % Ray_VI)

  end subroutine ray_init
  !============================================================================

  subroutine ray_clean

    ! Remove storage allocated in CON\_ray\_trace.

    integer :: jProc

    ! Deallocate MPI variables
    character(len=*), parameter:: NameSub = 'ray_clean'
    !--------------------------------------------------------------------------
    deallocate(iRequest_I, iStatus_II)

    ! Deallocate general send buffers
    if(associated(SendNei_P))then
       do jProc = 0, nProc-1
          if(associated(SendNei_P(jProc) % Ray_VI)) &
               deallocate(SendNei_P(jProc) % Ray_VI)
       end do
    end if
    deallocate(SendNei_P)

    ! Deallocate to start send buffers
    if(associated(SendEnd_P))then
       do jProc = 0, nProc-1
          if(associated(SendEnd_P(jProc) % Ray_VI)) &
               deallocate(SendEnd_P(jProc) % Ray_VI)
       end do
    end if
    deallocate(SendEnd_P)

    ! Deallocate recv buffer
    if(associated(nRayRecvNei_P))deallocate(nRayRecvNei_P)
    if(associated(RecvNei % Ray_VI))deallocate(RecvNei % Ray_VI)
    RecvNei % nRay   = 0
    RecvNei % MaxRay = 0

    ! Deallocate from end recv buffer
    if(associated(nRayRecvEnd_P))deallocate(nRayRecvEnd_P)
    if(associated(RecvEnd % Ray_VI))deallocate(RecvEnd % Ray_VI)
    RecvEnd % nRay   = 0
    RecvEnd % MaxRay = 0

    iComm = MPI_COMM_NULL

  end subroutine ray_clean
  !============================================================================

  subroutine ray_put(&
       iProcStart,iStart_D,iProcEnd,XyzEnd_D,Length,IsParallel,DoneRay)

    integer, intent(in) :: iProcStart,iProcEnd ! PE-s for start and end pos.
    integer, intent(in) :: iStart_D(4)         ! Indexes i,j,k,iBlock for start
    real,    intent(in) :: XyzEnd_D(3)         ! End posistion
    real,    intent(in) :: Length              ! Length of the ray so far
    logical, intent(in) :: IsParallel,DoneRay  ! Direction and status of trace

    ! Put ray information into send buffer. If DoneRay is true, the
    ! information will be sent ot iProcStart, otherwise it will be
    ! sent to iProcEnd.

    integer :: iProcTo

    ! Where should we send the ray
    character(len=*), parameter:: NameSub = 'ray_put'
    !--------------------------------------------------------------------------
    if(DoneRay)then
       iProcTo = iProcStart  ! Send back result to the PE that started tracing
       ! put ray info into the send to start buffer
       call append_ray(SendEnd_P(iProcTo))
    else
       iProcTo = iProcEnd    ! Send to PE which can continue the tracing
       call append_ray(SendNei_P(iProcTo))
    end if

    if(iProcTo<0)&
         call CON_stop(NameSub//' SWMF_error: PE lookup to be implemented')

  contains
    !==========================================================================

    subroutine append_ray(Send)

      ! Append a new element to the Send buffer

      type(RayPtr) :: Send
      integer      :: iRay

      !------------------------------------------------------------------------
      iRay = Send % nRay + 1
      if(iRay > Send % MaxRay) call extend_buffer(Send, iRay+100)

      Send % Ray_VI(RayStartI_:RayStartBlock_,iRay) = real(iStart_D)
      Send % Ray_VI(RayStartProc_            ,iRay) = iProcStart
      Send % Ray_VI(RayEndX_:RayEndZ_        ,iRay) = XyzEnd_D
      Send % Ray_VI(RayLength_               ,iRay) = Length

      if(IsParallel)then
         Send % Ray_VI(RayDir_,iRay)                =  1
      else
         Send % Ray_VI(RayDir_,iRay)                = -1
      end if

      if(DoneRay)then
         Send % Ray_VI(RayDone_,iRay)               =  1
      else
         Send % Ray_VI(RayDone_,iRay)               =  0
      end if

      Send % nRay = iRay

    end subroutine append_ray
    !==========================================================================

  end subroutine ray_put
  !============================================================================

  subroutine ray_get(&
       IsFound,iProcStart,iStart_D,XyzEnd_D,Length,IsParallel,DoneRay,IsEnd)

    logical, optional, intent(in) :: IsEnd ! if get ray from end proc

    logical, intent(out) :: IsFound            ! true if there are still rays
    integer, intent(out) :: iProcStart         ! PE-s for start and end pos.
    integer, intent(out) :: iStart_D(4)        ! Indexes i,j,k,iBlock for start
    real,    intent(out) :: XyzEnd_D(3)        ! End position
    real,    intent(out) :: Length             ! Length of the current ray
    logical, intent(out) :: IsParallel,DoneRay ! Direction and status of trace

    ! Provide the last ray for the component to store or to work on.
    ! If no ray is found in the receive buffer, IsFound=.false. is returned.

    ! local variables
    ! Pointers for choosing the buffer
    type(RayPtr), pointer :: SendPtr_P(:)
    type(RayPtr), pointer :: RecvPtr
    integer, pointer      :: nRayRecvPtr_P(:)

    integer :: iRay
    character(len=*), parameter:: NameSub = 'ray_get'
    !--------------------------------------------------------------------------

    SendPtr_P     => SendNei_P
    RecvPtr       => RecvNei
    nRayRecvPtr_P => nRayRecvNei_P
    if(present(IsEnd)) then
       if(IsEnd) then
          SendPtr_P     => SendEnd_P
          RecvPtr       => RecvEnd
          nRayRecvPtr_P => nRayRecvEnd_P
       end if
    end if

    iRay    = RecvPtr%nRay
    IsFound = iRay > 0

    if(.not.IsFound) RETURN  ! No more rays in the buffer

    ! Copy last ray into output arguments
    iStart_D     = nint(RecvPtr % Ray_VI(RayStartI_:RayStartBlock_,iRay))
    iProcStart   = nint(RecvPtr % Ray_VI(RayStartProc_,iRay))
    XyzEnd_D     = RecvPtr % Ray_VI(RayEndX_:RayEndZ_,iRay)
    Length       = RecvPtr % Ray_VI(RayLength_, iRay)
    IsParallel   = RecvPtr % Ray_VI(RayDir_,iRay)  > 0.0
    DoneRay      = RecvPtr % Ray_VI(RayDone_,iRay) > 0.5

    ! Remove ray from buffer
    RecvPtr % nRay = iRay - 1

  end subroutine ray_get
  !============================================================================

  subroutine ray_exchange(DoneMe, DoneAll, IsNeiProcIn_P)

    logical, intent(in) :: DoneMe

    logical, intent(out):: DoneAll

    !OPTIONAL ARGUMENTS:
    logical, intent(in), optional :: IsNeiProcIn_P(0:nProc-1)

    ! Send the Send\_P buffers to Recv buffers, empty the Send\_P buffers.
    ! Also check if there is more work to do. If the input argument DoneMe
    ! is false on any PE-s (i.e. it has more rays to do),
    ! or if there are any rays passed, the output argument DoneAll is
    ! set to .false. for all PE-s.
    ! If all PE-s have DoneMe true and all send buffers are
    ! empty then DoneAll is set to .true.
    ! The optional argument IsNeiProc\_P array stores the neighboring processor
    ! information to avoid unnecessary communication.

    integer, parameter :: iTag = 1
    integer :: jProc, iRay, nRayRecv

    ! Pointers for choosing the buffer
    type(RayPtr), pointer :: SendPtr_P(:)
    type(RayPtr), pointer :: RecvPtr
    integer, pointer      :: nRayRecvPtr_P(:)

    character(len=*), parameter:: NameSub = 'ray_exchange'
    !--------------------------------------------------------------------------
    ! Exchange number of rays in the send buffer

    if (present(IsNeiProcIn_P)) then
       SendPtr_P     => SendNei_P
       RecvPtr       => RecvNei
       nRayRecvPtr_P => nRayRecvNei_P
    else
       SendPtr_P     => SendEnd_P
       RecvPtr       => RecvEnd
       nRayRecvPtr_P => nRayRecvEnd_P
    end if

    ! Local copy (in case ray remains on the same PE)
    nRayRecvPtr_P = 0
    nRayRecvPtr_P(iProc)=SendPtr_P(iProc) % nRay

    nRequest = 0
    iRequest_I = MPI_REQUEST_NULL
    do jProc = 0, nProc-1
       if(jProc==iProc) CYCLE
       if (present(IsNeiProcIn_P)) then
          if(.not. IsNeiProcIn_P(jProc)) CYCLE
       endif
       nRequest = nRequest + 1
       call MPI_irecv(nRayRecvPtr_P(jProc),1,MPI_INTEGER,jProc,&
            iTag,iComm,iRequest_I(nRequest),iError)
    end do

    ! Wait for all receive commands to be posted for all processors
    call MPI_barrier(iComm,iError)

    ! Use ready-send
    do jProc = 0, nProc-1
       if(jProc==iProc) CYCLE
       if (present(IsNeiProcIn_P)) then
          if(.not. IsNeiProcIn_P(jProc)) CYCLE
       endif
       call MPI_rsend(SendPtr_P(jProc) % nRay,1,MPI_INTEGER,jProc,&
            iTag,iComm,iError)
    end do

    ! Wait for all messages to be received
    if(nRequest > 0)call MPI_waitall(nRequest,iRequest_I,iStatus_II,iError)

    nRayRecv = RecvPtr % nRay + sum(nRayRecvPtr_P)

    ! Extend receive buffer as needed
    if(nRayRecv > RecvPtr % MaxRay) call extend_buffer(RecvPtr,nRayRecv+100)

    ! Exchange ray information
    iRay = RecvPtr % nRay + 1

    ! Local copy if any
    if(nRayRecvPtr_P(iProc) > 0)then
       RecvPtr % Ray_VI(:,iRay:iRay+nRayRecvPtr_P(iProc)-1) = &
            SendPtr_P(iProc) % Ray_VI(:,1:SendPtr_P(iProc) % nRay)
       iRay = iRay + nRayRecvPtr_P(iProc)
    end if

    nRequest   = 0
    iRequest_I = MPI_REQUEST_NULL
    do jProc = 0, nProc-1
       if(jProc==iProc)CYCLE
       if(nRayRecvPtr_P(jProc)==0)CYCLE
       nRequest = nRequest + 1

       call MPI_irecv(RecvPtr % Ray_VI(1,iRay),nRayRecvPtr_P(jProc)*nRayInfo,&
               MPI_REAL,jProc,iTag,iComm,iRequest_I(nRequest),iError)
       iRay = iRay + nRayRecvPtr_P(jProc)
    end do

    call MPI_barrier(iComm, iError)

    ! Wait for all receive commands to be posted for all processors
    call MPI_barrier(iComm, iError)

    do jProc = 0, nProc-1
       if(jProc==iProc)CYCLE
       if(SendPtr_P(jProc) % nRay == 0) CYCLE

       call MPI_rsend(SendPtr_P(jProc) % Ray_VI(1,1),&
            SendPtr_P(jProc) % nRay*nRayInfo,MPI_REAL,jProc,iTag,iComm,iError)
    enddo

    ! Wait for all messages to be received
    if(nRequest > 0)call MPI_waitall(nRequest,iRequest_I,iStatus_II,iError)

    ! Update number of received rays
    RecvPtr % nRay = nRayRecv

    ! Reset send buffers
    do jProc = 0, nProc-1
       SendPtr_P(jProc) % nRay = 0
    end do

    ! Check if all PE-s are done
    DoneAll = DoneMe .and. (RecvPtr % nRay == 0)
    if(nProc > 1)&
         call MPI_allreduce(MPI_IN_PLACE, DoneAll, 1, MPI_LOGICAL, MPI_LAND, &
         iComm, iError)

  end subroutine ray_exchange
  !============================================================================

  subroutine extend_buffer(Buffer, nRayNew)

    ! Extend buffer size to nRayNew (or more)

    type(RayPtr)        :: Buffer
    integer, intent(in) :: nRayNew

    real, pointer :: OldRay_VI(:,:)
    !--------------------------------------------------------------------------
    if(.not.associated(Buffer % Ray_VI))then
       allocate(Buffer % Ray_VI(nRayInfo,nRayNew))    ! allocatenew buffer
       Buffer % nRay   = 0                            ! buffer is empty
       Buffer % MaxRay = nRayNew                      ! set buffer size
    else
       OldRay_VI => Buffer % Ray_VI                   ! store old values
       allocate(Buffer % Ray_VI(nRayInfo,nRayNew))    ! allocate new buffer
       Buffer % Ray_VI(:,1:Buffer % nRay) = &
            OldRay_VI(:,1:Buffer % nRay)              ! copy old values
       deallocate(OldRay_VI)                          ! free old storage
       Buffer % MaxRay = nRayNew                      ! change buffer size
    end if

  end subroutine extend_buffer
  !============================================================================

  subroutine ray_test

    ! Test the CON\_ray\_trace module. This subroutine should be called from
    ! a small stand alone program.

    logical :: IsFound
    integer :: iProcStart
    integer :: iStart_D(4)
    real    :: XyzEnd_D(3), Length
    logical :: IsParallel,DoneRay, DoneAll
    logical, allocatable :: IsNeiProc_P(:)
    integer :: jProc
    !--------------------------------------------------------------------------

    call ray_init(MPI_COMM_WORLD)

    ! initialize the neighboring processor array
    allocate(IsNeiProc_P(0:nProc-1))
    IsNeiProc_P = .false.
    if (iProc > 0) IsNeiProc_P(iProc-1) = .true.
    ! if (iProc < nProc-1) IsNeighbor_P(iProc+1) = .true.

    if(iProc==0) write(*,'(a,i4,i4)')'ray_init done, iProc,nProc=',iProc,nProc

    write(*,"(a,i2,a,4i4,a,i2,a,3f5.0,a,f5.0,a,2l2)") &
         " Sending from iProc=",iProc,&
         " iStart=",[110+iProc,120+iProc,130+iProc,140+iProc],&
         " to jProc=",mod(iProc+1,nProc),&
         " XyzEnd=",[210.+iProc,220.+iProc,230.+iProc], &
         " Length=",10.0*iProc, &
         " IsParallel, DoneRay=",.true.,.false.

    call ray_put(iProc, [110+iProc,120+iProc,130+iProc,140+iProc], &
         mod(iProc+1,nProc), [210.+iProc,220.+iProc,230.+iProc], &
         10.0*iProc, .true.,.false.)

    if(iProc==0) write(*,"(a,i2,a,4i4,a,i2,a,3f5.0,a,f5.0,a,2l2)") &
         " Sending from iProc=",iProc,&
         " iStart=",[110+iProc,120+iProc,130+iProc,140+iProc],&
         " to jProc=",mod(nProc+iProc-1,nProc),&
         " XyzEnd=",[210.+iProc,220.+iProc,230.+iProc], &
         " Length=",10.0*iProc+1.0, &
         " IsParallel, DoneRay=",.false.,.false.

    ! pass rays to the left neighbor iProc-1
    call ray_put(iProc, [110+iProc,120+iProc,130+iProc,140+iProc], &
         mod(nProc+iProc-1,nProc), [210.+iProc,220.+iProc,230.+iProc], &
         10.0*iProc+1.0,.false.,.false.)

    do jProc = 0, nProc-1
       write(*,"(a,i2,i2,i4,i4,100f5.0)")'iProc,jProc,Send_P(jProc)=',&
            iProc,jProc,SendNei_P(jProc) % MaxRay,&
            SendNei_P(jProc) % nRay, &
            SendNei_P(jProc) % Ray_VI(:,1:SendNei_P(jProc) % nRay)
    end do

    if(iProc==0) write(*,'(a)')'ray_put done'

    call ray_exchange(.true., DoneAll, IsNeiProc_P)

    write(*,*)'ray_exchange done, DoneAll=',DoneAll

    do
       call ray_get(IsFound,iProcStart,iStart_D,XyzEnd_D,Length, &
            IsParallel,DoneRay)
       if(.not.IsFound) EXIT
       write(*,"(a,i2,a,4i4,a,i2,a,3f5.0,a,f5.0,a,2l2)")&
            'iProc ',iProc,' received iStart=',iStart_D,&
            ' iProcStart=',iProcStart,' XyzEnd=',XyzEnd_D,' Length=',Length,&
            ' Isparallel,DoneRay=',IsParallel,DoneRay
    end do

    if(iProc==0) write(*,'(a)')'ray_get done'

    call ray_exchange(.true., DoneAll, IsNeiProc_P)

    if(iProc==0) write(*,'(a,l1)')'ray_exchange repeated, DoneAll=',DoneAll

    call ray_clean

    if(iProc==0) write(*,'(a)')'ray_clean done'

  end subroutine ray_test
  !============================================================================

end module CON_ray_trace
!==============================================================================
