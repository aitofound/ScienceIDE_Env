module mhdoutput
    use mhdinit, only: xgrid, ygrid, zgrid, nx, ny, nz, nvar, &
        uu, uu_prim
    use parallel

    implicit none 

    integer :: size_uu
    integer :: out_size_full(4), out_size_sub(4), out_start_ind(4),out_subarr_type
    integer(kind=mpi_offset_kind),parameter :: displacement = 12

    logical :: output_primitive = .true. !if false, output coservation quantities

    real,allocatable,dimension(:,:,:,:) :: output_temp_storage

    contains
        subroutine output_initialize
            implicit none 
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax

            out_size_full = [nx,ny,nz,nvar]
            out_size_sub = [nx,yi_size(myid_i+1),zj_size(myid_j+1),nvar]
            out_start_ind = [0,yi_offset(myid_i+1),zj_offset(myid_j+1),0]

            call mpi_type_create_subarray(4,out_size_full,out_size_sub,out_start_ind,&
                mpi_order_fortran,mpi_realtype,out_subarr_type,ierr)
            call mpi_type_commit(out_subarr_type, ierr)


            ! !create the time file
            ! if (ipe==0) then 
            !     open(FILE='time.dat',UNIT=30,STATUS='REPLACE',ACTION='WRITE')
            !     close(30)
            ! endif

            if (output_primitive) then 
                ixmin = 1
                ixmax = nx 

                iymin = yi_offset(myid_i+1) + 1 
                iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)

                izmin = zj_offset(myid_j+1) + 1 
                izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)

                allocate(output_temp_storage(ixmin:ixmax, iymin:iymax, izmin:izmax, 1:4))
            endif
        end subroutine output_initialize


        subroutine output_grid
            implicit none 

            if (ipe ==0) then 
                open(10,file='grid.dat',form='unformatted')
                    write(10) real(nx,4), real(ny,4), real(nz,4)
                    write(10) real(xgrid,4),real(ygrid,4),real(zgrid,4) 
                close(10)
            endif
        end subroutine output_grid

        subroutine output_parallel_info
            implicit none
            if (ipe == 0) then 
                open(10, file='parallel_info.dat', form='unformatted')
                    write(10) real(npe,4), real(iproc,4), real(jproc,4), real(nvar,4)
                close(10)
            endif
        end subroutine output_parallel_info


        subroutine output_uu(iout, time)
            ! OUTPUT main array
            implicit none 
            integer :: i, file_handle, data_size
            integer,intent(in) :: iout
            real,intent(in) :: time
            character(6) :: fname='      '



            if (iout ==0  ) write(fname,'(a3,i3)')  'out000'
            if (iout <1000) write(fname,'(a3,i3)')  'out' ,iout   
            if (iout <100 ) write(fname,'(a4,i2)')  'out0' ,iout
            if (iout < 10 ) write(fname,'(a5,i1)')  'out00',iout

            !write time
            if (ipe==0) then 
                open(unit=10,file= fname // '.dat',form='unformatted',&
                    status='replace',action='write')
                    write(10) real(time,4)
                close(10)
            endif

            if (output_primitive) then
                !temporarily store rho*u and energy density 
                output_temp_storage(:,:,:,1:3) = uu(:,:,:,2:4)
                output_temp_storage(:,:,:,4) = uu(:,:,:,8)

                !replace rho*u by u and e by p
                uu(:,:,:,2:4) = uu_prim(:,:,:,1:3)
                uu(:,:,:,8) = uu_prim(:,:,:,4)
            endif

            call mpi_file_open(mpi_comm_world, fname // '.dat', &
                mpi_mode_wronly + mpi_mode_create, mpi_info_null, &
                file_handle, ierr)

            data_size = size(uu)

            call mpi_file_set_view(file_handle, displacement, &
                mpi_realtype, out_subarr_type, 'native', mpi_info_null, ierr)

            call mpi_file_write_all(file_handle, uu, data_size, &
                mpi_realtype, mpi_status_ignore, ierr )

            call mpi_file_close(file_handle, ierr)

            if (output_primitive) then 
                !recover rho*u and e
                uu(:,:,:,2:4) = output_temp_storage(:,:,:,1:3)
                uu(:,:,:,8) = output_temp_storage(:,:,:,4)
            endif

            ! !write time to time.dat
            ! if (ipe==0) then 
            !     open(FILE='time.dat',UNIT=30,STATUS='OLD',ACTION='WRITE',POSITION='APPEND')
            !         write(30,'(i5,3x,f12.6)') iout,time
            !     close(30)
            ! endif
        end subroutine output_uu
end module mhdoutput