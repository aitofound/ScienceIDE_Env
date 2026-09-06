module fftw
    use, intrinsic :: iso_c_binding 
    use mhdinit, only: nx, ny, nz, nvar, uu, uu_fourier
    use parallel

    implicit none 

    include 'fftw3.f03'

    integer :: nmode_max_x, nmode_max_y
    integer :: fft_sign_forward = -1, fft_sign_backward = 1

    real, allocatable, dimension(:) :: fx_aux
    complex, allocatable, dimension(:) :: fx_aux_ft, fy_aux, &
        fy_aux_ft

    type(C_PTR) :: plan_fft_x, plan_fft_y, &
        plan_ifft_x, plan_ifft_y

    contains
        subroutine fftw_initialize
            implicit none 

            allocate( fx_aux(nx), fy_aux(ny),  &
                fx_aux_ft(nx/2+1), fy_aux_ft(ny))
            
            plan_fft_x = fftw_plan_dft_r2c_1d(nx, fx_aux, fx_aux_ft, FFTW_ESTIMATE)
            plan_fft_y = fftw_plan_dft_1d(ny, fy_aux, fy_aux_ft, fft_sign_forward, FFTW_ESTIMATE)

            plan_ifft_x = fftw_plan_dft_c2r_1d(nx, fx_aux_ft, fx_aux, FFTW_ESTIMATE)
            plan_ifft_y = fftw_plan_dft_1d(ny, fy_aux_ft, fy_aux, fft_sign_backward, FFTW_ESTIMATE)

            nmode_max_x = nx/3
            nmode_max_y = ny/3

        end subroutine fftw_initialize


        subroutine transform_uu_real_to_fourier
            !this subroutine transforms uu to uu_fourier
            implicit none 
            
            integer :: ix,iy,iz,ivar
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax


            do ivar = 1, nvar 

                iymin = yi_offset(myid_i+1) + 1
                iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
                izmin = 1 
                izmax = 1

                !do fourier transform along x
                do iz = izmin, izmax 
                    do iy = iymin,iymax
                        fx_aux(:) = uu(:,iy,iz,ivar)
                        call fftw_execute_dft_r2c(plan_fft_x, fx_aux, fx_aux_ft)
                        w_xy(:,iy,iz) = fx_aux_ft(:) / nx !normalization
                    enddo
                enddo

                !transpose x<-->y
                call transpose_xy

                ixmin = xi_offset(myid_i+1) + 1
                ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)
                izmin = 1 
                izmax = 1

                !do fourier transform along y
                do iz = izmin, izmax 
                    do ix = ixmin, ixmax 
                        fy_aux(:) = w_yx(ix,:,iz)
                        call fftw_execute_dft(plan_fft_y,fy_aux,fy_aux_ft)
                        w_yx(ix,:,iz) = fy_aux_ft(:) / ny 
                    enddo
                enddo

                !copy to uu_fourier
                uu_fourier(:,:,:,ivar) = w_yx(:,:,:)
            enddo
        end subroutine transform_uu_real_to_fourier

        subroutine transform_uu_fourier_to_real
            !this subroutine transforms uu_fourier to uu
            implicit none 
            
            integer :: ix,iy,iz,ivar
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax


            do ivar = 1, nvar 
            
                w_yx(:,:,:) = uu_fourier(:,:,:,ivar)

                !do inverse Fourier transform along y
                ixmin = xi_offset(myid_i+1) + 1
                ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)
                izmin = 1 
                izmax = 1

                do iz = izmin, izmax 
                    do ix = ixmin, ixmax 
                        fy_aux_ft(:) = w_yx(ix,:,iz)
                        call fftw_execute_dft(plan_ifft_y,fy_aux_ft,fy_aux)
                        w_yx(ix,:,iz) = fy_aux(:)
                    enddo
                enddo

                !transpose y<-->x
                call transpose_yx

                !inverse fourier transform to get the real-space deriv
                iymin = yi_offset(myid_i+1) + 1
                iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
                izmin = 1 
                izmax = 1

                do iz = izmin, izmax 
                    do iy = iymin,iymax
                        fx_aux_ft(:) = w_xy(:,iy,iz)
                        call fftw_execute_dft_c2r(plan_ifft_x, fx_aux_ft, fx_aux)
                        uu(:,iy,iz,ivar) = fx_aux(:)
                    enddo
                enddo
            enddo
        end subroutine transform_uu_fourier_to_real


        subroutine fftw_finalize
            implicit none 

            call fftw_destroy_plan(plan_fft_x)
            call fftw_destroy_plan(plan_fft_y)

            call fftw_destroy_plan(plan_ifft_x)
            call fftw_destroy_plan(plan_ifft_y)

        end subroutine fftw_finalize

end module fftw