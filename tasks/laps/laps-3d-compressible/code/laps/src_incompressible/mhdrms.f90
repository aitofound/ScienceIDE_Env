module mhdrms 
    use parallel
    use mhdinit, only: uu,uu_prim,nvar,nx,ny,nz
    
    implicit none

    real,allocatable,dimension(:) :: uu_ave, uu_square_ave, uu_ave_sum, &
        uu_square_ave_sum, uu_rms, B0_ave
    real,dimension(3) :: rho_u2, rho_u2_sum ! rho * (u-AVE(u))^2
    integer :: size_grid, nout_perline
    character(len=22) :: format_rms_line

    contains

    subroutine rms_initialize
        implicit none 

        allocate(uu_ave(nvar), uu_square_ave(nvar), uu_rms(nvar), &
            uu_ave_sum(nvar), uu_square_ave_sum(nvar), B0_ave(nvar))

        size_grid = nx*ny*nz

        nout_perline = 2 * nvar + 3 
        write(format_rms_line,'(a,i2,a)') '(f12.6,2x,',nout_perline,'(1pe16.8))'

        !create the rms file
        if (ipe==0) then 
            open(FILE='rms.dat',UNIT=30,STATUS='REPLACE',ACTION='WRITE')
            close(30)
        endif
    end subroutine rms_initialize

    subroutine output_rms(time)
        implicit none 
        real,intent(in) :: time
        

        call calc_rms

        if (ipe==0) then 
            open(FILE='rms.dat',UNIT=30,STATUS='OLD',ACTION='WRITE',POSITION='APPEND')
                write(30,format_rms_line) time, uu_ave, uu_rms, rho_u2
            close(30)
        endif
    end subroutine output_rms

    subroutine calc_rms 
        implicit none 
        integer :: ix,iy,iz,iv,ierr
        integer :: ixmin,ixmax,iymin,iymax,izmin,izmax 

        ixmin = 1
        ixmax = nx 
        iymin = yi_offset(myid_i+1) + 1
        iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
        izmin = zj_offset(myid_j+1) + 1
        izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)

        ! assign B0_ave
        B0_ave(:) = 0

        !calculate average values of uu on each CPU
        uu_ave(:) = 0
        uu_square_ave(:) = 0
        
        ! on each CPU
        do iz=izmin,izmax
            do iy=iymin,iymax
                do ix=ixmin,ixmax
                    uu_ave(1) = uu_ave(1) + uu(ix,iy,iz,1)
                    uu_ave(2:4) = uu_ave(2:4) + uu_prim(ix,iy,iz,1:3)
                    uu_ave(5:7) = uu_ave(5:7) + uu(ix,iy,iz,5:7) + B0_ave(5:7)
                    uu_ave(8) = uu_ave(8) + uu_prim(ix,iy,iz,4)


                    uu_square_ave(1) = uu_square_ave(1) + ( &
                        uu(ix,iy,iz,1))**2
                    uu_square_ave(2:4) = uu_square_ave(2:4) + ( &
                        uu_prim(ix,iy,iz,1:3))**2
                    uu_square_ave(5:7) = uu_square_ave(5:7) + ( &
                        uu(ix,iy,iz,5:7) + B0_ave(5:7))**2
                    uu_square_ave(8) = uu_square_ave(8) + ( &
                        uu_prim(ix,iy,iz,4))**2
                    
                enddo
            enddo
        enddo

        !sum up all CPUs 
        call mpi_allreduce(uu_ave, uu_ave_sum, nvar, mpi_realtype, mpi_sum,&
             mpi_comm_world, ierr)
        call mpi_allreduce(uu_square_ave, uu_square_ave_sum, nvar, mpi_realtype, &
            mpi_sum, mpi_comm_world, ierr)

        !calc AVE(u)
        uu_ave(:) = uu_ave_sum(:) / real(size_grid)
        !calc AVE(u^2)
        uu_square_ave(:) = uu_square_ave_sum(:) / real(size_grid)

        !RMS = AVE(u^2) - (AVE(u))^2
        uu_rms(:) = uu_square_ave(:) - (uu_ave(:))**2


        !kinetic energy
        rho_u2(:) = 0
        do iv=1,3
            do iz=izmin,izmax
                do iy=iymin,iymax
                    do ix=ixmin,ixmax
                        rho_u2(iv) = rho_u2(iv) + uu(ix,iy,iz,1) * ( &
                            uu_prim(ix,iy,iz,iv) - uu_ave(1+iv))**2
                    enddo
                enddo
            enddo
        enddo
        call mpi_allreduce(rho_u2,rho_u2_sum,3,mpi_realtype, mpi_sum,&
             mpi_comm_world, ierr)
        
        rho_u2 = rho_u2_sum / real(size_grid)
    end subroutine
end module mhdrms