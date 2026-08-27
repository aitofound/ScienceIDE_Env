module parallel
    implicit none 
    include 'mpif.h'

    integer :: mpi_complextype , mpi_realtype
    integer :: ierr, ipe,npe,myid_i,iproc

    !offsets and sizes used for swap
    !For example, yi_ means ygrid partitioned along iproc direction
    !especially, xi_ corresponds to a (nx/2+1) grid points 
    integer,allocatable,dimension(:) :: yi_offset, xi_offset, &
        yi_size, xi_size

    !arrays used for swapping
    complex, allocatable, dimension(:,:,:) :: w_xy, w_yx

    !assisting arrays for transpose
    integer,allocatable,dimension(:) :: subarr_type_xy_send,subarr_type_xy_recv

    contains 

        subroutine parallel_start(nx,ny,nvar)
            implicit none 

            integer,intent(in) :: nx,ny,nvar
            integer :: arr_size_full(3), arr_size_sub(3), arr_starts(3)
            integer :: idim ,jdim
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax

            if (kind(1.0)==4) then 
                mpi_realtype = mpi_real 
                mpi_complextype = mpi_complex 
            else if(kind(1.0)==8) then 
                mpi_realtype = mpi_double_precision
                mpi_complextype = mpi_double_complex
            else
                write(*,*) 'Unknown real & complex type in parallel_start!!'
            endif

            call mpi_init(ierr)

            call mpi_comm_rank(mpi_comm_world, ipe, ierr)
            call mpi_comm_size(mpi_comm_world, npe, ierr)

            iproc = npe
            myid_i = ipe 

            if (ipe .eq. 0) then
                write(*,*)  'Number of proc. ', npe
            endif


            !determine the index offset and the size in each processor
            call decompose_1d(ny,iproc,yi_offset,yi_size)
            call decompose_1d(nx/2+1,iproc,xi_offset,xi_size)
            !--------------------------------------------------
            !x-fourier transformed array
            ixmin = 1
            ixmax = nx/2+1 

            iymin = yi_offset(myid_i+1) + 1 
            iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)

            izmin = 1
            izmax = 1

            allocate(w_xy(ixmin:ixmax, iymin:iymax, izmin:izmax))
            !--------------------------------------------------

            !--------------------------------------------------
            !x<-->y transposed array
            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = 1 
            iymax = ny

            izmin = 1 
            izmax = 1
            allocate(w_yx(ixmin:ixmax, iymin:iymax, izmin:izmax))
            !--------------------------------------------------


            allocate(subarr_type_xy_send(iproc), subarr_type_xy_recv(iproc))

            !create subarrays used in swapping x<-->y----------------------
            !send
            !size_full shoud be size of w_xy
            arr_size_full = [nx/2+1, yi_size(myid_i+1), 1]
            do idim = 1, iproc
                arr_size_sub = [xi_size(idim), yi_size(myid_i+1), 1]

                !do not consider the self-defined starting indices, so (0,0) for y&z
                arr_starts = [xi_offset(idim), 0, 0]  

                !note that the start indices should be C-like, i.e. minus 1
                call mpi_type_create_subarray(3, arr_size_full, arr_size_sub, arr_starts,&
                    mpi_order_fortran, mpi_complextype, subarr_type_xy_send(idim), ierr )

                call mpi_type_commit(subarr_type_xy_send(idim), ierr)
            enddo

            !receive
            !size_full should be size of w_yx
            arr_size_full = [xi_size(myid_i+1), ny, 1]
            do idim = 1, iproc
                arr_size_sub = [xi_size(myid_i+1), yi_size(idim), 1]

                arr_starts = [0, yi_offset(idim), 0]

                call mpi_type_create_subarray(3, arr_size_full, arr_size_sub, arr_starts,&
                    mpi_order_fortran, mpi_complextype, subarr_type_xy_recv(idim), ierr )

                call mpi_type_commit(subarr_type_xy_recv(idim), ierr)
            enddo
            !---------------------------------------------------------------
        end subroutine parallel_start


        !Chen Shi, March-31-2019
        !new version of transpose functions, using mpi_sendrecv  
        !in order to avoid deadlock
        subroutine transpose_xy
            implicit none 
            integer :: idim , ixmin, ixmax, iymin, iymax, izmin, izmax
            integer :: ipe_send, ipe_recv

            ! copy the diagonal blocks
            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)
            iymin = yi_offset(myid_i+1) + 1
            iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
            izmin = 1
            izmax = 1
            w_yx(ixmin : ixmax, iymin : iymax, izmin : izmax) = &
                w_xy(ixmin : ixmax, iymin : iymax, izmin : izmax)


            do idim = 1, iproc-1
                ipe_send = modulo(myid_i - idim + iproc, iproc)
                ipe_recv = modulo(myid_i + idim + iproc, iproc)

                call mpi_sendrecv(w_xy, 1, subarr_type_xy_send(ipe_send+1), ipe_send, ipe_send, & 
                    w_yx, 1, subarr_type_xy_recv(ipe_recv+1), ipe_recv, myid_i, &
                    mpi_comm_world, MPI_STATUS_IGNORE,ierr)
            enddo
        end subroutine transpose_xy

        subroutine transpose_yx
            !inverse subroutine of transpose_xy
            implicit none 
            integer :: idim , ixmin, ixmax, iymin, iymax, izmin, izmax
            integer :: ipe_send, ipe_recv

            ! copy the diagonal blocks
            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)
            iymin = yi_offset(myid_i+1) + 1
            iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
            izmin = 1
            izmax = 1

            w_xy(ixmin : ixmax, iymin : iymax, izmin : izmax) = &
                w_yx(ixmin : ixmax, iymin : iymax, izmin : izmax)

            do idim = 1, iproc-1
                ipe_send = modulo(myid_i - idim + iproc, iproc)
                ipe_recv = modulo(myid_i + idim + iproc, iproc)

                call mpi_sendrecv(w_yx, 1, subarr_type_xy_recv(ipe_send+1), ipe_send, ipe_send, & 
                    w_xy, 1, subarr_type_xy_send(ipe_recv+1), ipe_recv, myid_i, &
                    mpi_comm_world, MPI_STATUS_IGNORE,ierr)
            enddo
            
        end subroutine transpose_yx

        subroutine decompose_1d(ngrid, nproc, i_offset, i_size)
            implicit none 

            integer,intent(in) :: ngrid, nproc
            integer,intent(out),allocatable,dimension(:) :: i_offset, i_size

            integer :: iproc, normal_size

            allocate(i_offset(nproc), i_size(nproc))

            normal_size = ngrid / nproc 

            i_offset(1) = 0
            i_size(1) = normal_size

            do iproc=2,nproc
                i_offset(iproc) = i_offset(iproc-1) + i_size(iproc-1)
                if (iproc < nproc) then 
                    i_size(iproc) = normal_size
                else 
                    i_size(iproc) = ngrid - i_offset(iproc)
                endif
            enddo
        end subroutine

        subroutine parallel_end 
            implicit none
            
            call mpi_finalize(ierr)
        end subroutine parallel_end










        ! !old version using mpi_isend & mpi_irecv: occasionally deadlock on COMET-----------
        ! Chen Shi, March-31-2019
        ! subroutine transpose_xy
        !     implicit none 
        !     integer,allocatable,dimension(:) :: recv_req, send_req
        !     integer :: idim , ixmin, ixmax, iymin, iymax, izmin, izmax

        !     allocate(recv_req(dims(1)-1), send_req(dims(1)-1))


        !     !send and receive
        !     do idim = 1, myid_i
        !         call mpi_irecv(w_yxz,1, subarr_type_xy_recv(idim), idim-1, myid_i, &
        !             comm1d_i, recv_req(idim), ierr )
        !     enddo

        !     do idim = myid_i + 2, dims(1)
        !         call mpi_irecv(w_yxz,1, subarr_type_xy_recv(idim), idim-1, myid_i, &
        !             comm1d_i, recv_req(idim-1), ierr )
        !     enddo

        !     do idim = 1, myid_i
        !         call mpi_isend(w_xyz,1, subarr_type_xy_send(idim), idim-1, idim-1, &
        !             comm1d_i, send_req(idim), ierr )
        !     enddo

        !     do idim = myid_i+2, dims(1)
        !         call mpi_isend(w_xyz,1, subarr_type_xy_send(idim), idim-1, idim-1, &
        !             comm1d_i, send_req(idim-1), ierr )
        !     enddo

        !     ! copy the diagonal blocks
        !     ixmin = xi_offset(myid_i+1) + 1
        !     ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)
        !     iymin = yi_offset(myid_i+1) + 1
        !     iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
        !     izmin = zj_offset(myid_j+1) + 1
        !     izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)
        !     w_yxz(ixmin : ixmax, iymin : iymax, izmin : izmax) = &
        !         w_xyz(ixmin : ixmax, iymin : iymax, izmin : izmax)

        !     call mpi_waitall(dims(1)-1, recv_req, MPI_STATUSES_IGNORE, ierr ) 
        !     call mpi_waitall(dims(1)-1, send_req, MPI_STATUSES_IGNORE, ierr ) 
        ! end subroutine transpose_xy

        ! subroutine transpose_yx
        !     !inverse subroutine of transpose_xy
        !     implicit none 
        !     integer,allocatable,dimension(:) :: recv_req, send_req
        !     integer :: idim , ixmin, ixmax, iymin, iymax, izmin, izmax

        !     allocate(recv_req(dims(1)-1), send_req(dims(1)-1))


        !     !send and receive
        !     do idim = 1, myid_i
        !         call mpi_irecv(w_xyz,1, subarr_type_xy_send(idim), idim-1, myid_i, &
        !             comm1d_i, recv_req(idim), ierr )
        !     enddo

        !     do idim = myid_i + 2, dims(1)
        !         call mpi_irecv(w_xyz,1, subarr_type_xy_send(idim), idim-1, myid_i, &
        !             comm1d_i, recv_req(idim-1), ierr )
        !     enddo

        !     do idim = 1, myid_i
        !         call mpi_isend(w_yxz,1, subarr_type_xy_recv(idim), idim-1, idim-1, &
        !             comm1d_i, send_req(idim), ierr )
        !     enddo

        !     do idim = myid_i+2, dims(1)
        !         call mpi_isend(w_yxz,1, subarr_type_xy_recv(idim), idim-1, idim-1, &
        !             comm1d_i, send_req(idim-1), ierr )
        !     enddo

        !     ! copy the diagonal blocks
        !     ixmin = xi_offset(myid_i+1) + 1
        !     ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)
        !     iymin = yi_offset(myid_i+1) + 1
        !     iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
        !     izmin = zj_offset(myid_j+1) + 1
        !     izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)

        !     w_xyz(ixmin : ixmax, iymin : iymax, izmin : izmax) = &
        !         w_yxz(ixmin : ixmax, iymin : iymax, izmin : izmax)

        !     call mpi_waitall(dims(1)-1, recv_req, MPI_STATUSES_IGNORE, ierr ) 
        !     call mpi_waitall(dims(1)-1, send_req, MPI_STATUSES_IGNORE, ierr ) 
        ! end subroutine transpose_yx


        ! subroutine transpose_yz
        !     implicit none 
        !     integer,allocatable,dimension(:) :: recv_req, send_req
        !     integer :: idim , ixmin, ixmax, iymin, iymax, izmin, izmax

        !     allocate(recv_req(dims(2)-1), send_req(dims(2)-1))

        !     !send and receive
        !     do idim = 1, myid_j
        !         call mpi_irecv(w_zxy,1, subarr_type_yz_recv(idim), idim-1, myid_j, &
        !             comm1d_j, recv_req(idim), ierr )
        !     enddo

        !     do idim = myid_j + 2, dims(2)
        !         call mpi_irecv(w_zxy,1, subarr_type_yz_recv(idim), idim-1, myid_j, &
        !             comm1d_j, recv_req(idim-1), ierr )
        !     enddo

        !     do idim = 1, myid_j
        !         call mpi_isend(w_yxz,1, subarr_type_yz_send(idim), idim-1, idim-1, &
        !             comm1d_j, send_req(idim), ierr )
        !     enddo

        !     do idim = myid_j+2, dims(2)
        !         call mpi_isend(w_yxz,1, subarr_type_yz_send(idim), idim-1, idim-1, &
        !             comm1d_j, send_req(idim-1), ierr )
        !     enddo


        !     ! copy the diagonal blocks
        !     ixmin = xi_offset(myid_i+1) + 1
        !     ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)
        !     iymin = yj_offset(myid_j+1) + 1
        !     iymax = yj_offset(myid_j+1) + yj_size(myid_j+1)
        !     izmin = zj_offset(myid_j+1) + 1
        !     izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)
        !     w_zxy(ixmin : ixmax, iymin : iymax, izmin : izmax) = &
        !         w_yxz(ixmin : ixmax, iymin : iymax, izmin : izmax)

        !     call mpi_waitall(dims(2)-1, recv_req, MPI_STATUSES_IGNORE, ierr ) 
        !     call mpi_waitall(dims(2)-1, send_req, MPI_STATUSES_IGNORE, ierr ) 
        ! end subroutine transpose_yz


        ! subroutine transpose_zy
        !     !this is the inverse subroutine of transpose_yz
        !     implicit none 
        !     integer,allocatable,dimension(:) :: recv_req, send_req
        !     integer :: idim , ixmin, ixmax, iymin, iymax, izmin, izmax

        !     allocate(recv_req(dims(2)-1), send_req(dims(2)-1))

        !     !send and receive
        !     do idim = 1, myid_j
        !         call mpi_irecv(w_yxz,1, subarr_type_yz_send(idim), idim-1, myid_j, &
        !             comm1d_j, recv_req(idim), ierr )
        !     enddo

        !     do idim = myid_j + 2, dims(2)
        !         call mpi_irecv(w_yxz,1, subarr_type_yz_send(idim), idim-1, myid_j, &
        !             comm1d_j, recv_req(idim-1), ierr )
        !     enddo

        !     do idim = 1, myid_j
        !         call mpi_isend(w_zxy,1, subarr_type_yz_recv(idim), idim-1, idim-1, &
        !             comm1d_j, send_req(idim), ierr )
        !     enddo

        !     do idim = myid_j+2, dims(2)
        !         call mpi_isend(w_zxy,1, subarr_type_yz_recv(idim), idim-1, idim-1, &
        !             comm1d_j, send_req(idim-1), ierr )
        !     enddo

        !     ! copy the diagonal blocks
        !     ixmin = xi_offset(myid_i+1) + 1
        !     ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)
        !     iymin = yj_offset(myid_j+1) + 1
        !     iymax = yj_offset(myid_j+1) + yj_size(myid_j+1)
        !     izmin = zj_offset(myid_j+1) + 1
        !     izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)
        !     w_yxz(ixmin : ixmax, iymin : iymax, izmin : izmax) = &
        !         w_zxy(ixmin : ixmax, iymin : iymax, izmin : izmax)

        !     call mpi_waitall(dims(2)-1, recv_req, MPI_STATUSES_IGNORE, ierr ) 
        !     call mpi_waitall(dims(2)-1, send_req, MPI_STATUSES_IGNORE, ierr ) 
        ! end subroutine transpose_zy 
        ! !end of old version of transpose functions------------------------------------------------------
end module parallel