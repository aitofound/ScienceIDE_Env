subroutine get_boundary_property(igroup)
  use coupling_SEM_DSM_par
  implicit none

  integer, intent(in) ::igroup

!#ifdef USE_MPI
!    include "mpif.h"
!    integer ::ier
!#endif

  integer ::ipack_local,global_ipack_old,ios
  integer ::ipoint,ipoint_read
!other variables
  character(len=MAX_STRING_LEN) ::boundary_file,file_tmp


  ipoint=0
  do ipack_local=1,npack(igroup)
     global_ipack_old=ipack_local+ipack_start(igroup)-1
     write(file_tmp,"('DATABASES_MPIiproc',i4.4,'/boundary_info_pack',i6.6)") &
                   in_iproc(global_ipack_old),local_pack_id(global_ipack_old)
     boundary_file=trim(dir_SEM_input(1:len_trim(dir_SEM_input)))// '/' //trim(file_tmp(1:len_trim(file_tmp)))
     open(unit=53,file=trim(boundary_file),action='read',form='formatted',status="old",iostat=ios)
     do ipoint_read=1,npoint_pack(global_ipack_old)
       ipoint=ipoint+1
       read(53,*)id_depth_SEM(ipoint),is_elastic(ipoint),is_acoustic(ipoint)
       read(53,*)normal_vector(:,ipoint)
       read(53,*)jacobian(ipoint)
       read(53,*)coord_bound(:,ipoint)
!     call MPI_BARRIER(MPI_COMM_WORLD,ier)
     end do
!     call MPI_BARRIER(MPI_COMM_WORLD,ier)
     close(53)
  end do


end subroutine get_boundary_property
