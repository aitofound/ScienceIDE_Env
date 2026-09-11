!mpi modules --------------  m00module.f90 -------------------------------   
module MPImodule
  !
  use variables_data, only : iError,nameLocal
  use ModMPI
  implicit none
  !include 'mpif.h' !mor??
  !include 'mpif90.h' !??? where is located???
  integer, parameter :: mpiProcMax=16! number of procs intend to use
  integer :: mpiProcN ! number of procs 
  integer :: mpiProc  ! current proc
  integer, parameter :: mpiHost=0  ! proc printing reports
  integer :: mpiError
  !real (kind=8) :: mpiWtime0 
  !  character (len=MPI_MAX_PROCESSOR_NAME) nameProc
  character (len=MPI_MAX_ERROR_STRING) :: mpiMessage
  character (len=2) :: nameProcOut ! proc name for output
  !
contains 
  !
  subroutine MPIinit !---------------------------------------------------
    call MPI_INIT( iError )
    if(iError /= MPI_SUCCESS) &
         call MPIerror_report(1,nameLocal,'INIT')
  end subroutine MPIinit !-----------------------------------------------
  !
  subroutine MPIcomm_rank !----------------------------------------------
    call MPI_COMM_RANK( MPI_COMM_WORLD, mpiProc, iError )
    if(iError /= MPI_SUCCESS) &
         call MPIerror_report(2,nameLocal,'MPI_COMM_RANK')
  end subroutine MPIcomm_rank !------------------------------------------
  !
  subroutine MPIcomm_size !----------------------------------------------
    call MPI_COMM_SIZE( MPI_COMM_WORLD, mpiProcN, iError )
    if(iError /= MPI_SUCCESS) &
         call MPIerror_report(3,nameLocal,'MPI_COMM_SIZE')
  end subroutine MPIcomm_size !------------------------------------------
  !
  subroutine MPIproc_compare !-------------------------------------------
    if(mpiProcN /= mpiProcMax) &
        call MPIerror_report(4,nameLocal,'MPIproc_compare')
  end subroutine MPIproc_compare !---------------------------------------
  !
  subroutine MPIerror_(name,text) !----------------------------------
    !only MPI errors with complicated diagnose in REDUCE,ALLREDUCE
    !
    integer :: nChar
    integer :: errorcode=0
    !character (len=*) :: name,text ???
    character (len=*), intent(in) :: name,text
    !
    call MPI_ERROR_STRING(mpiMessage,nChar,mpiError)
    !
    if ( mpiError .ne. MPI_SUCCESS ) &
         call MPI_ABORT(MPI_COMM_WORLD,errorcode,mpiError) ! atto
    !
    call MPIerror_report(mpiError,name//':'//text, &
         mpiMessage(1:nChar) ) 
    !
  end subroutine MPIerror_ !----------------------------------------------
  !
  subroutine MPIerror_report(nError,name,textMessage) !-------------------
    !MPI errors with simple diagnose
    !
    integer :: nError
    integer :: errorcode=0
    character (len=*) :: name,textMessage
    !
    if(iError==0) then
       write(*,*)'*** PE',mpiProc,'<WARNING',nError,'>',name,'|',textMessage
       return
    else
       write(*,*)'*** PE',mpiProc,'<ERROR',nError,'>',name,'|',textMessage
       call MPI_ABORT(MPI_COMM_WORLD,errorcode,nError)
       stop
    endif
    !
  end subroutine MPIerror_report !-----------------------------------------
!
  subroutine MPIfinalize !-------------------------------------------------
    call MPI_FINALIZE(iError)
    if(iError /= MPI_SUCCESS) call MPIerror_report(99,nameLocal,'FINALIZE')
  end subroutine MPIfinalize !---------------------------------------------
  !
  subroutine MPIallreduce_min(x,xGlob,n) !---------------------------------
    integer, intent(in) :: n
    real, intent(in) :: x(n)
    real, intent(out) :: xGlob(n)
    call MPI_ALLREDUCE(x,xGlob,n,MPI_DOUBLE_PRECISION,&
         MPI_MIN,MPI_COMM_WORLD,iError)
    !iError=MPI_ALLREDUCE(x,xGlob,n,MPI_DOUBLE_PRECISION,&
     !    MPI_MIN,MPI_COMM_WORLD)
    !
    if(iError /= MPI_SUCCESS) &
         call MPIerror_report(0,nameLocal,'ALLREDUCE MIN')
  end subroutine MPIallreduce_min !----------------------------------------
  !
  subroutine MPIallreduce_max(x,xGlob,n) !---------------------------------
    integer, intent(in) :: n
    real, intent(in) :: x(n)
    real, intent(out) :: xGlob(n)
    call MPI_ALLREDUCE(x,xGlob,n,MPI_DOUBLE_PRECISION,&
         MPI_MAX,MPI_COMM_WORLD,iError)
   !iError=MPI_ALLREDUCE(x,xGlob,n,MPI_DOUBLE_PRECISION,&
    !     MPI_MAX,MPI_COMM_WORLD)

    if(iError /= MPI_SUCCESS) &
         call MPIerror_report(0,nameLocal,'ALLREDUCE MAX')
  end subroutine MPIallreduce_max !----------------------------------------
  !
  subroutine MPIreduce_sum_i(iArrayIn,iArrayOut,nLength) !----------------
    integer, intent(in) :: nLength
    integer, intent(in) :: iArrayIn(1:nLength)
    integer, intent(out) :: iArrayOut(1:nLength)
    !
    call MPI_REDUCE(iArrayIn,iArrayOut,nLength, &
         MPI_INTEGER,MPI_SUM,mpiHost,MPI_COMM_WORLD,iError)
    !iError=MPI_REDUCE(iArrayIn,iArrayOut,nLength, &
     !    MPI_INTEGER,MPI_SUM,mpiHost,MPI_COMM_WORLD)
    !
     if(iError /= MPI_SUCCESS) &
          call MPIerror_(nameLocal//'reduce_sum_i','MPI_REDUCE')
  end subroutine MPIreduce_sum_i !------------------------------------
  !
  subroutine MPIreduce_sum_r(rArrayIn,rArrayOut,nLength) !---------------
    integer, intent(in) :: nLength
    real, intent(in) :: rArrayIn(1:nLength)
    real, intent(out) :: rArrayOut(1:nLength)
    !
    call MPI_REDUCE(rArrayIn,rArrayOut,nLength, &
         MPI_DOUBLE_PRECISION,MPI_SUM,mpiHost,MPI_COMM_WORLD,iError)
    !iError= MPI_REDUCE(rArrayIn,rArrayOut,nLength, &
     !    MPI_DOUBLE_PRECISION,MPI_SUM,mpiHost,MPI_COMM_WORLD)
    !
    if(iError /= MPI_SUCCESS) &
         call MPIerror_(nameLocal//'reduce_sum_r','MPI_REDUCE')
  end subroutine MPIreduce_sum_r !---------------------------------------
  !
  subroutine MPIallreduce_sum_r(rArrayIn,rArrayOut,nLength) !---------------
    integer, intent(in) :: nLength
    real, intent(in) :: rArrayIn(1:nLength)
    real, intent(out) :: rArrayOut(1:nLength)
    !
    !!call MPI_ALLREDUCE(C(1,ii1,ii2,1),CC,nMax(3),
    call MPI_ALLREDUCE(rArrayIn,rArrayOut,nLength, &
         MPI_DOUBLE_PRECISION,MPI_SUM,MPI_COMM_WORLD,iError)
    !iError=MPI_ALLREDUCE(rArrayIn,rArrayOut,nLength, &
     !    MPI_DOUBLE_PRECISION,MPI_SUM,MPI_COMM_WORLD)
    !
    if(iError /= MPI_SUCCESS) &
         call MPIerror_(nameLocal//'allreduce_sum_r','MPI_ALLREDUCE')
  end subroutine MPIallreduce_sum_r !------------------------------------
  !
end module MPImodule



