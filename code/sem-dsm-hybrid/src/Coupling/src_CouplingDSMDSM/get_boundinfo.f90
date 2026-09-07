subroutine get_boundinfo()
  use coupling_SEM_DSM_par
  implicit none

!#ifdef USE_MPI
!    include "mpif.h"
!    integer ::ier
!#endif

  integer ::ipack_local,global_ipack_old,ios
  integer ::ipoint,ipoint_read
!other variables
  character(len=MAX_STRING_LEN) ::bound_file,file_tmp


!  write(new_dir,"('./BOUND_FFT/iproc',i4.4)") myrank
!  call system('mkdir -p '//adjustl(trim(new_dir)))

  ipoint=0
  do ipack_local=1,npack
     global_ipack_old=ipack_local+ipack_start-1
     write(file_tmp,"('DATABASES_MPIiproc',i4.4,'/boundary_info_pack',i6.6)") &
                   in_iproc(global_ipack_old),local_pack_id(global_ipack_old)
     bound_file=trim(dir_SEM_input(1:len_trim(dir_SEM_input)))// '/' //trim(file_tmp(1:len_trim(file_tmp)))
     open(unit=53,file=trim(bound_file),action='read',form='formatted',status="old",iostat=ios)
     do ipoint_read=1,npoint_pack(global_ipack_old)
       ipoint=ipoint+1
       read(53,*)id_depth_SEM(ipoint),is_elastic(ipoint),is_acoustic(ipoint)
!       if(id_depth_SEM(ipoint).eq.1) then
!          id_depth_SEM(ipoint)=1
!       else if(id_depth_SEM(ipoint).eq.2) then
!          id_depth_SEM(ipoint)=3
!       else if(id_depth_SEM(ipoint).lt.44) then
!          id_depth_SEM(ipoint)=(id_depth(ipoint)-2)*4+3
!       else 
!          id_depth_SEM(ipoint)=169
!       end if

!       read(53,*)c(:,ipoint)
       read(53,*)normal_vector(:,ipoint)
       read(53,*)jacobian(ipoint)
       read(53,*)coord_bound(:,ipoint)
!       if(id_depth_SEM_SEM(ipoint).eq.1) &
!          write(*,'(A10,2I4,5E13.5)'),"idep1_",local_pack_id(global_ipack_old),ipoint,&
!                coord_bound(2,ipoint),coord_bound(3,ipoint),normal_vector(:,ipoint)

!     print *,'ipoint boundinfo',myrank,ipoint
!     call MPI_BARRIER(MPI_COMM_WORLD,ier)

     end do
!     print *,'read boundinfo done',myrank,ipack_local
!     call MPI_BARRIER(MPI_COMM_WORLD,ier)

     close(53)

  end do



end subroutine get_boundinfo
