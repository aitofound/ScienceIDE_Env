!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf

module ModMpiModified

  ! Change definition of MPI_REAL and MPI_COMPLEX
  ! Provide subroutines to simplify in-place reduction

  use ModMpiOrig, &
       MPI_REAL_ORIG => MPI_REAL, MPI_COMPLEX_ORIG => MPI_COMPLEX

  implicit none

  ! iRealPrec = 1 if the code is compiled with 8 byte reals and 0 otherwise
  integer, parameter :: iRealPrec = (1.00000000011 - 1.0)*10000000000.0

  integer, parameter :: MPI_REAL = &
       iRealPrec*MPI_DOUBLE_PRECISION + (1-iRealPrec)*MPI_REAL_ORIG

  integer, parameter :: MPI_COMPLEX = &
       iRealPrec*MPI_DOUBLE_COMPLEX + (1-iRealPrec)*MPI_COMPLEX_ORIG

contains
  !============================================================================
  subroutine mpi_reduce_real_array(Buffer_I, nSize, iOp, iRoot, iComm, iError)

    real, intent(inout):: Buffer_I(*)
    integer, intent(in):: nSize
    integer, intent(in):: iOp
    integer, intent(in):: iRoot
    integer, intent(in):: iComm
    integer, intent(out):: iError

    integer:: iRank
    real:: Recv_I(1)
    !--------------------------------------------------------------------------
    call MPI_comm_rank(iComm, iRank, iError)

    if(iRoot == iRank)then
       call MPI_reduce(MPI_IN_PLACE, Buffer_I, nSize, MPI_REAL, iOp, &
            iRoot, iComm, iError)
    else
       call MPI_reduce(Buffer_I, Recv_I, nSize, MPI_REAL, iOp, &
            iRoot, iComm, iError)
    end if

  end subroutine mpi_reduce_real_array
  !============================================================================
  subroutine mpi_reduce_real_scalar(Value, iOp, iRoot, iComm, iError)

    real, intent(inout):: Value
    integer, intent(in):: iOp
    integer, intent(in):: iRoot
    integer, intent(in):: iComm
    integer, intent(out):: iError

    integer:: iRank
    real :: Recv
    !--------------------------------------------------------------------------
    call MPI_comm_rank(iComm, iRank, iError)

    if(iRoot == iRank)then
       call MPI_reduce(MPI_IN_PLACE, Value, 1, MPI_REAL, iOp, &
            iRoot, iComm, iError)
    else
       call MPI_reduce(Value, Recv, 1, MPI_REAL, iOp, &
            iRoot, iComm, iError)
    end if

  end subroutine mpi_reduce_real_scalar
  !============================================================================
  subroutine mpi_reduce_integer_array(&
       iBuffer_I, nSize, iOp, iRoot, iComm, iError)

    integer, intent(inout):: iBuffer_I(*)
    integer, intent(in):: nSize
    integer, intent(in):: iOp
    integer, intent(in):: iRoot
    integer, intent(in):: iComm
    integer, intent(out):: iError

    integer:: iRank, iRecv_I(1)
    !--------------------------------------------------------------------------
    call MPI_comm_rank(iComm, iRank, iError)

    if(iRoot == iRank)then
       call MPI_reduce(MPI_IN_PLACE, iBuffer_I, nSize, MPI_INTEGER, &
            iOp, iRoot, iComm, iError)
    else
       call MPI_reduce(iBuffer_I, iRecv_I, nSize, MPI_INTEGER, &
            iOp, iRoot, iComm, iError)
    end if

  end subroutine mpi_reduce_integer_array
  !============================================================================
  subroutine mpi_reduce_integer_scalar(iValue, iOp, iRoot, iComm, iError)

    integer, intent(inout):: iValue
    integer, intent(in):: iOp
    integer, intent(in):: iRoot
    integer, intent(in):: iComm
    integer, intent(out):: iError

    integer:: iRank, iRecv
    !--------------------------------------------------------------------------
    call MPI_comm_rank(iComm, iRank, iError)

    if(iRoot == iRank)then
       call MPI_reduce(MPI_IN_PLACE, iValue, 1, MPI_INTEGER, &
            iOp, iRoot, iComm, iError)
    else
       call MPI_reduce(iValue, iRecv, 1, MPI_INTEGER, &
            iOp, iRoot, iComm, iError)
    end if

  end subroutine mpi_reduce_integer_scalar
  !============================================================================
  subroutine mpi_reduce_logical_array(&
       Buffer_I, nSize, iOp, iRoot, iComm, iError)

    logical, intent(inout):: Buffer_I(*)
    integer, intent(in):: nSize
    integer, intent(in):: iOp
    integer, intent(in):: iRoot
    integer, intent(in):: iComm
    integer, intent(out):: iError

    integer:: iRank
    logical:: Recv_I(1)
    !--------------------------------------------------------------------------
    call MPI_comm_rank(iComm, iRank, iError)

    if(iRoot == iRank)then
       call MPI_reduce(MPI_IN_PLACE, Buffer_I, nSize, MPI_LOGICAL, iOp, &
            iRoot, iComm, iError)
    else
       call MPI_reduce(Buffer_I, Recv_I, nSize, MPI_LOGICAL, iOp, &
            iRoot, iComm, iError)
    end if

  end subroutine mpi_reduce_logical_array
  !============================================================================
  subroutine mpi_reduce_logical_scalar(Value, iOp, iRoot, iComm, iError)

    logical, intent(inout):: Value
    integer, intent(in):: iOp
    integer, intent(in):: iRoot
    integer, intent(in):: iComm
    integer, intent(out):: iError

    integer:: iRank
    logical :: Recv
    !--------------------------------------------------------------------------
    call MPI_comm_rank(iComm, iRank, iError)

    if(iRoot == iRank)then
       call MPI_reduce(MPI_IN_PLACE, Value, 1, MPI_LOGICAL, iOp, &
            iRoot, iComm, iError)
    else
       call MPI_reduce(Value, Recv, 1, MPI_LOGICAL, iOp, &
            iRoot, iComm, iError)
    end if

  end subroutine mpi_reduce_logical_scalar
  !============================================================================
end module ModMpiModified
!==============================================================================
