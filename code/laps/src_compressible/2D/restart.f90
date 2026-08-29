module restart
    !Chen Shi, May-22-2019
    !module to read a specified output and restart
    !Note: assuming the output is in primitive variables
    !To do: include the option of reading output
    !       in conserved variables
    use mhdoutput
    use parallel
    implicit none 

    real :: t_restart = 0
    logical :: if_restart = .false.
    integer :: n_start = 0

    contains

    subroutine read_restart
        implicit none 

        integer :: i, file_handle, data_size
        character(6) :: fname='      '
        real(kind=4) :: t_restart_temp

        if (n_start ==0  ) write(fname,'(a3,i3)')  'out000'
        if (n_start <1000) write(fname,'(a3,i3)')  'out' ,n_start  
        if (n_start <100 ) write(fname,'(a4,i2)')  'out0' ,n_start
        if (n_start < 10 ) write(fname,'(a5,i1)')  'out00',n_start

        !read time
        if (ipe==0) then 
            open(unit=10,file= fname // '.dat',form='unformatted',&
                status='old',action='read')
                read(10) t_restart_temp
            close(10)

            t_restart = real(t_restart_temp,kind(1.0))

            write(*,'(1x,a,f10.4)') 'Restart from time = ', t_restart
        endif


        if (ipe==0) then
            do i=1,npe-1
                call mpi_send(t_restart,1,mpi_realtype,i,0,mpi_comm_world,ierr)
            enddo
        else 
            call mpi_recv(t_restart,1,mpi_realtype,0,0,mpi_comm_world,&
                mpi_status_ignore,ierr)
        endif
    
        

        !read main array
        call mpi_file_open(mpi_comm_world, fname // '.dat', &
            mpi_mode_rdonly, mpi_info_null, file_handle, ierr)

        data_size = size(uu)

        call mpi_file_set_view(file_handle, displacement, &
            mpi_realtype, out_subarr_type, 'native', mpi_info_null, ierr)

        call mpi_file_read_all(file_handle, uu, data_size, &
                mpi_realtype, mpi_status_ignore, ierr )

        call mpi_file_close(file_handle, ierr)
    end subroutine read_restart

end module restart 
