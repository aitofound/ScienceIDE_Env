module fftw
    use, intrinsic :: iso_c_binding 
    use mhdinit, only: nx, ny, nz, nvar, uu, uu_fourier!,uu_prim,uu_prim_fourier
    use parallel

    implicit none 

    include 'fftw3.f03'

    integer :: nmode_max_x, nmode_max_y, nmode_max_z
    integer :: fft_sign_forward = -1, fft_sign_backward = 1

    real, allocatable, dimension(:) :: fx_aux
    complex, allocatable, dimension(:) :: fx_aux_ft, fy_aux, &
        fz_aux, fy_aux_ft, fz_aux_ft

    type(C_PTR) :: plan_fft_x, plan_fft_y, plan_fft_z, &
        plan_ifft_x, plan_ifft_y, plan_ifft_z

    contains
        subroutine fftw_initialize
            implicit none 

            allocate( fx_aux(nx), fy_aux(ny), fz_aux(nz), &
                fx_aux_ft(nx/2+1), fy_aux_ft(ny), fz_aux_ft(nz))
            
            plan_fft_x = fftw_plan_dft_r2c_1d(nx, fx_aux, fx_aux_ft, FFTW_ESTIMATE)
            plan_fft_y = fftw_plan_dft_1d(ny, fy_aux, fy_aux_ft, fft_sign_forward, FFTW_ESTIMATE)
            plan_fft_z = fftw_plan_dft_1d(nz, fz_aux, fz_aux_ft, fft_sign_forward, FFTW_ESTIMATE)

            plan_ifft_x = fftw_plan_dft_c2r_1d(nx, fx_aux_ft, fx_aux, FFTW_ESTIMATE)
            plan_ifft_y = fftw_plan_dft_1d(ny, fy_aux_ft, fy_aux, fft_sign_backward, FFTW_ESTIMATE)
            plan_ifft_z = fftw_plan_dft_1d(nz, fz_aux_ft, fz_aux, fft_sign_backward, FFTW_ESTIMATE)

            nmode_max_x = nx/3
            nmode_max_y = ny/3
            nmode_max_z = nz/3

        end subroutine fftw_initialize


        subroutine transform_uu_real_to_fourier
            !this subroutine transforms uu to uu_fourier
            implicit none 
            
            integer :: ix,iy,iz,ivar
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax


            do ivar = 1, nvar 

                iymin = yi_offset(myid_i+1) + 1
                iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
                izmin = zj_offset(myid_j+1) + 1 
                izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)

                !do fourier transform along x
                do iz = izmin, izmax 
                    do iy = iymin,iymax
                        fx_aux(:) = uu(:,iy,iz,ivar)
                        call fftw_execute_dft_r2c(plan_fft_x, fx_aux, fx_aux_ft)
                        w_xyz(:,iy,iz) = fx_aux_ft(:) / nx !normalization
                    enddo
                enddo

                call from_xyz_to_zxy

                !copy to uu_fourier
                uu_fourier(:,:,:,ivar) = w_zxy(:,:,:)
            enddo
        end subroutine transform_uu_real_to_fourier

        subroutine transform_uu_fourier_to_real
            !this subroutine transforms uu_fourier to uu
            implicit none 
            
            integer :: ix,iy,iz,ivar
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax


            do ivar = 1, nvar 
            
                w_zxy(:,:,:) = uu_fourier(:,:,:,ivar)

                !transpose and inverse fourier transform to get w_xyz
                call from_zxy_to_xyz

                !inverse fourier transform to get the real-space deriv
                iymin = yi_offset(myid_i+1) + 1
                iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
                izmin = zj_offset(myid_j+1) + 1 
                izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)

                do iz = izmin, izmax 
                    do iy = iymin,iymax
                        fx_aux_ft(:) = w_xyz(:,iy,iz)
                        call fftw_execute_dft_c2r(plan_ifft_x, fx_aux_ft, fx_aux)
                        uu(:,iy,iz,ivar) = fx_aux(:) 
                    enddo
                enddo

            enddo
        end subroutine transform_uu_fourier_to_real


        ! subroutine transform_uu_prim_real_to_fourier
        !     !this subroutine transforms uu to uu_fourier
        !     implicit none 
            
        !     integer :: ix,iy,iz,ivar
        !     integer :: ixmin,ixmax,iymin,iymax,izmin,izmax


        !     iymin = yi_offset(myid_i+1) + 1
        !     iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
        !     izmin = zj_offset(myid_j+1) + 1 
        !     izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)

        !     do ivar = 4, 4
        !         !do fourier transform along x
        !         do iz = izmin, izmax 
        !             do iy = iymin,iymax
        !                 fx_aux(:) = uu_prim(:,iy,iz,ivar)
        !                 call fftw_execute_dft_r2c(plan_fft_x, fx_aux, fx_aux_ft)
        !                 w_xyz(:,iy,iz) = fx_aux_ft(:) / nx !normalization
        !             enddo
        !         enddo

        !         call from_xyz_to_zxy

        !         !copy to uu_prim_fourier
        !         uu_prim_fourier(:,:,:,ivar) = w_zxy(:,:,:)
        !     enddo
        ! end subroutine transform_uu_prim_real_to_fourier

        subroutine from_xyz_to_zxy
            !This subroutine do the following:
            !1. transpose w_xyz to get w_yxz
            !2. Fourier transform w_yxz along y
            !3. transpose w_yxz to get w_zxy
            !4. Fourier transform w_zxy along z
            implicit none

            integer :: ix,iy,iz,ixmin,ixmax,&
                iymin,iymax,izmin,izmax

            !transpose x<-->y
            call transpose_xy

            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)
            izmin = zj_offset(myid_j+1) + 1 
            izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)

            !do fourier transform along y
            do iz = izmin, izmax 
                do ix = ixmin, ixmax 
                    fy_aux(:) = w_yxz(ix,:,iz)
                    call fftw_execute_dft(plan_fft_y,fy_aux,fy_aux_ft)
                    w_yxz(ix,:,iz) = fy_aux_ft(:) / ny 
                enddo
            enddo

            !transpose y<-->z
            call transpose_yz

            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)
            iymin = yj_offset(myid_j+1) + 1 
            iymax = yj_offset(myid_j+1) + yj_size(myid_j+1)

            !do fourier transform along z
            do iy = iymin, iymax 
                do ix = ixmin, ixmax 
                    fz_aux(:) = w_zxy(ix,iy,:)
                    call fftw_execute_dft(plan_fft_z,fz_aux,fz_aux_ft)
                    w_zxy(ix,iy,:) = fz_aux_ft(:) / nz 
                enddo
            enddo
        end subroutine from_xyz_to_zxy

        subroutine from_zxy_to_xyz
            !inverse version of from_xyz_to_zxy
            implicit none 

            integer :: ix,iy,iz,ixmin,ixmax,&
                iymin,iymax,izmin,izmax

            !do inverse-Fourier transform along z
            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)
            iymin = yj_offset(myid_j+1) + 1 
            iymax = yj_offset(myid_j+1) + yj_size(myid_j+1)

            do iy = iymin, iymax 
                do ix = ixmin, ixmax 
                    fz_aux_ft(:) = w_zxy(ix,iy,:)
                    call fftw_execute_dft(plan_ifft_z,fz_aux_ft,fz_aux)
                    w_zxy(ix,iy,:) = fz_aux(:)
                enddo
            enddo

            !transpose z<-->y
            call transpose_zy

            !do inverse Fourier transform along y
            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)
            izmin = zj_offset(myid_j+1) + 1 
            izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)

            do iz = izmin, izmax 
                do ix = ixmin, ixmax 
                    fy_aux_ft(:) = w_yxz(ix,:,iz)
                    call fftw_execute_dft(plan_ifft_y,fy_aux_ft,fy_aux)
                    w_yxz(ix,:,iz) = fy_aux(:)
                enddo
            enddo

            !transpose y<-->x
            call transpose_yx
        end subroutine from_zxy_to_xyz


        subroutine fftw_finalize
            implicit none 

            call fftw_destroy_plan(plan_fft_x)
            call fftw_destroy_plan(plan_fft_y)
            call fftw_destroy_plan(plan_fft_z)

            call fftw_destroy_plan(plan_ifft_x)
            call fftw_destroy_plan(plan_ifft_y)
            call fftw_destroy_plan(plan_ifft_z)

        end subroutine fftw_finalize

end module fftw