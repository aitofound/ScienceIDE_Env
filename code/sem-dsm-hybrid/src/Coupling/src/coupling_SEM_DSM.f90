program coupling_SEM_DSM
    use coupling_SEM_DSM_par

    implicit none
#ifdef USE_MPI
    include "mpif.h"
#endif

    integer ::ipackage,ipoint,igroup
#ifdef USE_MPI
    integer ::ier
#endif


    call initialize()
    call read_para()
    !To avoid memory issue, split the packages into N groups. Each group
    !contains nproc*npack_per_proc.
    call read_DSM_depths(dir_DSM_input,myrank)
    call partition_job()

    call allocate_array1()
    call read_SEM_depths(dir_SEM_input)

    do igroup=1,ngroups
       call read_package_id(dir_SEM_input,igroup)
       if(igroup.eq.1) then
         call allocate_array2()
         call read_number_stations()
       end if
   
       if(myrank.eq.0) then
         write(*,*) "Start reading boundary displacement and traction &
             and it takes a while ..."
         call flush(6)
       end if
       do ipackage=1,npack(igroup)
         call read_ipack(ipack_start(igroup)+ipackage-1,ipackage)
         do ipoint=1,npoint_pack(ipack_start(igroup)+ipackage-1)
           call resample(ipoint,ipackage)
           call fft(ipoint-1+ipoint_start(ipackage))
         end do
   !      call MPI_BARRIER(MPI_COMM_WORLD,ier)
       end do
       if(myrank.eq.0) then
         write(*,*) "Reading boundary displacement and traction done."
         call flush(6)
       end if
       call get_boundary_property(igroup)
   !    call MPI_BARRIER(MPI_COMM_WORLD,ier)
   !    call deallocate_array()
   !    call MPI_BARRIER(MPI_COMM_WORLD,ier)
       if(myrank.eq.0) then
         write(*,*) "Convolution starts..."
         call flush(6)
       end if
       call convolution(igroup)
       if(myrank.eq.0) then
         write(*,*) "Done!"
         call flush(6)
       end if
    end do
#ifdef USE_MPI
   call MPI_FINALIZE(ier)
#endif


end program
