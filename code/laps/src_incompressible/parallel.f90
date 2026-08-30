module parallel
    implicit none 
    include 'mpif.h'

    integer :: ndim_parallel = 2
    integer :: mpi_complextype , mpi_realtype
    integer :: ierr, ipe, ipe_cart,npe,myid_i, myid_j
    integer :: comm_cart, comm1d_i, comm1d_j
    integer :: dims(2), icoords(2)
    integer :: iproc, jproc
    logical :: periods(2), reorder=.true.

    !offsets and sizes used for swap
    !For example, yi_ means ygrid partitioned along iproc direction
    !especially, xi_ corresponds to a (nx/2+1) grid points 
    integer,allocatable,dimension(:) :: yi_offset, zj_offset, yj_offset, xi_offset, &
        yi_size, zj_size, yj_size, xi_size

    !arrays used for swapping
    complex, allocatable, dimension(:,:,:) :: w_xyz, w_yxz, w_zxy

    !assisting arrays for transpose
    integer,allocatable,dimension(:) :: subarr_type_xy_send,subarr_type_xy_recv,&
                subarr_type_yz_send,subarr_type_yz_recv

    contains 

        subroutine parallel_start(nx,ny,nz,nvar,ndim_part)
            implicit none 

            integer,intent(in) :: nx,ny,nz,nvar,ndim_part
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

            !must initialize dims before calling mpi_dims_create
            dims(1) = 0
            dims(2) = 0

            call mpi_init(ierr)

            call mpi_comm_rank(mpi_comm_world, ipe, ierr)
            call mpi_comm_size(mpi_comm_world, npe, ierr)


            if (ndim_part==1) then 
                dims(1) = 1
                dims(2) = npe
            else 
                ! create dims
                call mpi_dims_create(npe,2,dims,ierr)

                if(dims(1) .gt. dims(2)) then
                    dims(1) = dims(2)
                    dims(2) = npe / dims(1)
                endif
            endif

            iproc = dims(1)
            jproc = dims(2)

            if (ipe .eq. 0) then
                write(*,*)  'Number of proc. ', npe
                write(*,'(1x,a,i3,a,i3)')  'Using processor grid: ',iproc,' x ',jproc
            endif

            periods(1:2) = .TRUE.

            !create cart
            call mpi_cart_create(mpi_comm_world, 2, dims,periods,reorder,comm_cart,ierr)

            !get the coordinate of this processor in mpi_cart
            call mpi_cart_get(comm_cart, 2, dims, periods,icoords, ierr)

            !you can also use mpi_cart_rank to get rank from coords
            !and use mpi_cart_coords to get coords from rank
            call mpi_cart_rank(comm_cart, icoords, ipe_cart, ierr)
            !call mpi_cart_coords(comm_cart, ipe, 2, icoords, ierr)


            ! Actually (myid_i, myid_j) = icoords(1:2)
            call mpi_comm_split(comm_cart, icoords(2), icoords(1), comm1d_i, ierr)
            call mpi_comm_rank(comm1d_i, myid_i, ierr )
            call mpi_comm_split(comm_cart, icoords(1), icoords(2), comm1d_j, ierr)
            call mpi_comm_rank(comm1d_j, myid_j, ierr)

            ! write(*,*) ipe_cart, myid_i, myid_j

            !determine the index offset and the size in each processor
            call decompose_1d(ny,dims(1),yi_offset,yi_size)
            call decompose_1d(ny,dims(2),yj_offset,yj_size)
            call decompose_1d(nz,dims(2),zj_offset,zj_size)
            call decompose_1d(nx/2+1,dims(1),xi_offset,xi_size)
            !--------------------------------------------------
            !x-fourier transformed array
            ixmin = 1
            ixmax = nx/2+1 

            iymin = yi_offset(myid_i+1) + 1 
            iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)

            izmin = zj_offset(myid_j+1) + 1 
            izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)

            allocate(w_xyz(ixmin:ixmax, iymin:iymax, izmin:izmax))
            !--------------------------------------------------

            !--------------------------------------------------
            !x<-->y transposed array
            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = 1 
            iymax = ny

            izmin = zj_offset(myid_j+1) + 1 
            izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)

            allocate(w_yxz(ixmin:ixmax, iymin:iymax, izmin:izmax))
            !--------------------------------------------------

            !--------------------------------------------------
            !y<-->z transposed array
            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = yj_offset(myid_j+1) + 1
            iymax = yj_offset(myid_j+1) + yj_size(myid_j+1)

            izmin = 1 
            izmax = nz

            allocate(w_zxy(ixmin:ixmax,iymin:iymax,izmin:izmax))
            !--------------------------------------------------

            allocate(subarr_type_xy_send(dims(1)), subarr_type_xy_recv(dims(1)), &
                subarr_type_yz_send(dims(2)), subarr_type_yz_recv(dims(2)) )

            !create subarrays used in swapping x<-->y----------------------
            !send
            !size_full shoud be size of w_xyz
            arr_size_full = [nx/2+1, yi_size(myid_i+1), zj_size(myid_j+1)]
            do idim = 1, dims(1)
                arr_size_sub = [xi_size(idim), yi_size(myid_i+1), zj_size(myid_j+1)]

                !do not consider the self-defined starting indices, so (0,0) for y&z
                arr_starts = [xi_offset(idim), 0, 0]  

                !note that the start indices should be C-like, i.e. minus 1
                call mpi_type_create_subarray(3, arr_size_full, arr_size_sub, arr_starts,&
                    mpi_order_fortran, mpi_complextype, subarr_type_xy_send(idim), ierr )

                call mpi_type_commit(subarr_type_xy_send(idim), ierr)
            enddo

            !receive
            !size_full should be size of w_yxz
            arr_size_full = [xi_size(myid_i+1), ny, zj_size(myid_j+1)]
            do idim = 1, dims(1)
                arr_size_sub = [xi_size(myid_i+1), yi_size(idim), zj_size(myid_j+1)]

                arr_starts = [0, yi_offset(idim), 0]

                call mpi_type_create_subarray(3, arr_size_full, arr_size_sub, arr_starts,&
                    mpi_order_fortran, mpi_complextype, subarr_type_xy_recv(idim), ierr )

                call mpi_type_commit(subarr_type_xy_recv(idim), ierr)
            enddo
            !---------------------------------------------------------------


            !create subarrays used in swapping y<-->z----------------------
            !send
            !size_full shoud be size of w_yxz
            arr_size_full = [xi_size(myid_i+1), ny, zj_size(myid_j+1)]
            do idim = 1, dims(2)
                arr_size_sub = [xi_size(myid_i+1), yj_size(idim), zj_size(myid_j+1)]

                arr_starts = [0, yj_offset(idim), 0]  

                !note that the start indices should be C-like, i.e. minus 1
                call mpi_type_create_subarray(3, arr_size_full, arr_size_sub, arr_starts,&
                    mpi_order_fortran, mpi_complextype, subarr_type_yz_send(idim), ierr )

                call mpi_type_commit(subarr_type_yz_send(idim), ierr)
            enddo

            !receive
            !size_full should be size of w_zxy
            arr_size_full = [xi_size(myid_i+1), yj_size(myid_j+1), nz]
            do idim = 1, dims(2)
                arr_size_sub = [xi_size(myid_i+1), yj_size(myid_j+1), zj_size(idim)]

                arr_starts = [0, 0, zj_offset(idim)]

                call mpi_type_create_subarray(3, arr_size_full, arr_size_sub, arr_starts,&
                    mpi_order_fortran, mpi_complextype, subarr_type_yz_recv(idim), ierr )

                call mpi_type_commit(subarr_type_yz_recv(idim), ierr)
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
            izmin = zj_offset(myid_j+1) + 1
            izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)
            w_yxz(ixmin : ixmax, iymin : iymax, izmin : izmax) = &
                w_xyz(ixmin : ixmax, iymin : iymax, izmin : izmax)


            do idim = 1, dims(1)-1
                ipe_send = modulo(myid_i - idim + dims(1), dims(1))
                ipe_recv = modulo(myid_i + idim + dims(1), dims(1))

                call mpi_sendrecv(w_xyz, 1, subarr_type_xy_send(ipe_send+1), ipe_send, ipe_send, & 
                    w_yxz, 1, subarr_type_xy_recv(ipe_recv+1), ipe_recv, myid_i, &
                    comm1d_i, MPI_STATUS_IGNORE,ierr)
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
            izmin = zj_offset(myid_j+1) + 1
            izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)

            w_xyz(ixmin : ixmax, iymin : iymax, izmin : izmax) = &
                w_yxz(ixmin : ixmax, iymin : iymax, izmin : izmax)

            do idim = 1, dims(1)-1
                ipe_send = modulo(myid_i - idim + dims(1), dims(1))
                ipe_recv = modulo(myid_i + idim + dims(1), dims(1))

                call mpi_sendrecv(w_yxz, 1, subarr_type_xy_recv(ipe_send+1), ipe_send, ipe_send, & 
                    w_xyz, 1, subarr_type_xy_send(ipe_recv+1), ipe_recv, myid_i, &
                    comm1d_i, MPI_STATUS_IGNORE,ierr)
            enddo
            
        end subroutine transpose_yx


        subroutine transpose_yz
            implicit none 

            integer :: idim , ixmin, ixmax, iymin, iymax, izmin, izmax
            integer :: ipe_send,ipe_recv

            ! copy the diagonal blocks
            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)
            iymin = yj_offset(myid_j+1) + 1
            iymax = yj_offset(myid_j+1) + yj_size(myid_j+1)
            izmin = zj_offset(myid_j+1) + 1
            izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)
            w_zxy(ixmin : ixmax, iymin : iymax, izmin : izmax) = &
                w_yxz(ixmin : ixmax, iymin : iymax, izmin : izmax)

            do idim = 1, dims(2)-1
                ipe_send = modulo(myid_j - idim + dims(2), dims(2))
                ipe_recv = modulo(myid_j + idim + dims(2), dims(2))

                call mpi_sendrecv(w_yxz, 1, subarr_type_yz_send(ipe_send+1), ipe_send, ipe_send, & 
                    w_zxy, 1, subarr_type_yz_recv(ipe_recv+1), ipe_recv, myid_j, &
                    comm1d_j, MPI_STATUS_IGNORE,ierr)
            enddo
        end subroutine transpose_yz


        subroutine transpose_zy
            !this is the inverse subroutine of transpose_yz
            implicit none 
            integer :: idim , ixmin, ixmax, iymin, iymax, izmin, izmax
            integer :: ipe_send,ipe_recv

            ! copy the diagonal blocks
            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)
            iymin = yj_offset(myid_j+1) + 1
            iymax = yj_offset(myid_j+1) + yj_size(myid_j+1)
            izmin = zj_offset(myid_j+1) + 1
            izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)
            w_yxz(ixmin : ixmax, iymin : iymax, izmin : izmax) = &
                w_zxy(ixmin : ixmax, iymin : iymax, izmin : izmax)

            do idim = 1, dims(2)-1
                ipe_send = modulo(myid_j - idim + dims(2), dims(2))
                ipe_recv = modulo(myid_j + idim + dims(2), dims(2))

                call mpi_sendrecv(w_zxy, 1, subarr_type_yz_recv(ipe_send+1), ipe_send, ipe_send, & 
                    w_yxz, 1, subarr_type_yz_send(ipe_recv+1), ipe_recv, myid_j, &
                    comm1d_j, MPI_STATUS_IGNORE,ierr)
            enddo
        end subroutine transpose_zy 

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