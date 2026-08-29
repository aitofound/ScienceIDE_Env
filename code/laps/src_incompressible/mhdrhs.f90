module mhdrhs 
    use mhdinit, only: nx,ny,nz,uu, uu_prim, &
        fnl,adiabatic_index,flux, &
        flux_fourier,wave_number_x, wave_number_y, &
        wave_number_z,if_AEB,uu_fourier,expand_term,&
        expand_term_fourier, &
        sin_cor_ang,cos_cor_ang, if_corotating,&
        if_hall,ion_inertial_length, &
        current_density,current_density_fourier, &
        if_resis, if_resis_exp, resistivity, &
        if_visc, if_visc_exp, viscosity, &
        k_square, if_conserve_background,&
        grad_velocity,grad_velocity_fourier,&
        flux_pressure,flux_pressure_fourier,&
        divB_arr,divB_arr_fourier,divV_arr,divV_arr_fourier,&
        nflux,nfluxPressure,rho0,p0
    use AEBmod, only: radius0, radius, tau_exp
    use fftw 
    use parallel 

    implicit none 

    contains 

        subroutine calc_flux
            !calculate the nonlinear fluxes in real space.
            !for incompressible version, it is essentially electric field.
            implicit none 
            integer :: ix,iy,iz
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax 
            real :: Bx, By, Bz, ux,uy,uz, rho

            ! ! no need: already done before this step
            ! if (if_hall) then 
            !     call calc_current_density_real
            ! endif

            ixmin = 1
            ixmax = nx 
            iymin = yi_offset(myid_i+1) + 1
            iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
            izmin = zj_offset(myid_j+1) + 1
            izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)

            ! order of variables:
            ! flux variable order:
            ! 1:3 : Ex, Ey, Ez

            do iz = izmin,izmax 
                do iy = iymin, iymax 
                    do ix = ixmin, ixmax 
                        ux = uu_prim(ix,iy,iz,1)
                        uy = uu_prim(ix,iy,iz,2)
                        uz = uu_prim(ix,iy,iz,3)
                        Bx = uu(ix,iy,iz,5) 
                        By = uu(ix,iy,iz,6) 
                        Bz = uu(ix,iy,iz,7) 

                        !electric field
                        flux(ix,iy,iz,1) = uz * By - uy * Bz 
                        flux(ix,iy,iz,2) = ux * Bz - uz * Bx
                        flux(ix,iy,iz,3) = uy * Bx - ux * By


                        if (if_hall) then 
                            rho = uu(ix,iy,iz,1)
                            !Ex_h = di/rho * (Jy*Bz-Jz*By)
                            flux(ix,iy,iz,1) = flux(ix,iy,iz,1) + ion_inertial_length/rho * &
                                (current_density(ix,iy,iz,2) * uu(ix,iy,iz,7) - &
                                current_density(ix,iy,iz,3) * uu(ix,iy,iz,6))
                            !Ey_h = di/rho * (Jz*Bx-Jx*Bz)
                            flux(ix,iy,iz,2) = flux(ix,iy,iz,2) + ion_inertial_length/rho * &
                                (current_density(ix,iy,iz,3) * uu(ix,iy,iz,5) - &
                                current_density(ix,iy,iz,1) * uu(ix,iy,iz,7))
                            !Ez_h = di/rho * (Jx*By-Jy*Bx)
                            flux(ix,iy,iz,3) = flux(ix,iy,iz,3) + ion_inertial_length/rho * &
                                (current_density(ix,iy,iz,1) * uu(ix,iy,iz,6) - &
                                current_density(ix,iy,iz,2) * uu(ix,iy,iz,5))
                        endif
                    enddo 
                enddo 
            enddo

        end subroutine calc_flux


    
        subroutine transform_flux_real_to_fourier
            !this subroutine transforms flux to flux_fourier
            implicit none 
            
            integer :: ix,iy,iz,ivar
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax

            iymin = yi_offset(myid_i+1) + 1
            iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
            izmin = zj_offset(myid_j+1) + 1 
            izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)
            
            do ivar = 1, nflux
                !do fourier transform along x
                do iz = izmin, izmax 
                    do iy = iymin,iymax
                        fx_aux(:) = flux(:,iy,iz,ivar)
                        call fftw_execute_dft_r2c(plan_fft_x, fx_aux, fx_aux_ft)
                        w_xyz(:,iy,iz) = fx_aux_ft(:) / nx !normalization
                    enddo
                enddo

                call from_xyz_to_zxy

                !copy to flux_fourier
                flux_fourier(:,:,:,ivar) = w_zxy(:,:,:)
            enddo
        end subroutine transform_flux_real_to_fourier

        subroutine calc_rhs
            !calculate rhs of the MHD equation in conservation form
            !call after transform_flux_real_to_fourier
            implicit none 
            integer :: ix,iy,iz
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax 
            complex :: kx, ky, kz, kdotfp
            real :: k2

            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = yj_offset(myid_j+1) + 1
            iymax = yj_offset(myid_j+1) + yj_size(myid_j+1)

            izmin = 1 
            izmax = nz

            do iz = izmin,izmax 
                kz = cmplx(0,wave_number_z(iz)) * radius0/radius
                do iy = iymin,iymax 
                    ky = cmplx(0,wave_number_y(iy)) * radius0/radius
                    do ix = ixmin,ixmax
                        kx = cmplx(0,wave_number_x(ix))

                        k2 = k_square(ix,iy,iz)
                        if (if_AEB .and. if_corotating) then
                            kx = cmplx(0,wave_number_x(ix)) * cos_cor_ang + &
                                cmplx(0,wave_number_y(iy)) * sin_cor_ang
                            ky = (-cmplx(0,wave_number_x(ix)) * sin_cor_ang + &
                                cmplx(0,wave_number_y(iy)) * cos_cor_ang) * &
                                radius0/radius
                        endif


                        !drho/dt
                        fnl(ix,iy,iz,1) = 0.

                        if (k2<1e-10) then 
                            kdotfp = 0.
                            fnl(ix,iy,iz,2) = 0.
                            fnl(ix,iy,iz,3) = 0.
                            fnl(ix,iy,iz,4) = 0.
                        else 
                            kdotfp = (kx*flux_pressure_fourier(ix,iy,iz,1) + &
                                ky*flux_pressure_fourier(ix,iy,iz,2) + &
                                kz*flux_pressure_fourier(ix,iy,iz,3))/k2 

                            !d(rho*u)/dt
                            !for incompressible 
                            !d(rho u)_k /dt = - (k dot fpk)/k^2 k_vec + fpk
                            !where fpk is the flux_for_pressure
                            !note here: kx,ky,kz are imaginary
                            fnl(ix,iy,iz,2) = flux_pressure_fourier(ix,iy,iz,1) + kdotfp * kx
                            fnl(ix,iy,iz,3) = flux_pressure_fourier(ix,iy,iz,2) + kdotfp * ky
                            fnl(ix,iy,iz,4) = flux_pressure_fourier(ix,iy,iz,3) + kdotfp * kz
                        endif 

                        !dB/dt
                        fnl(ix,iy,iz,5) = kz * flux_fourier(ix,iy,iz,2) - &
                            ky * flux_fourier(ix,iy,iz,3)
                        fnl(ix,iy,iz,6) = kx * flux_fourier(ix,iy,iz,3) - &
                            kz * flux_fourier(ix,iy,iz,1)
                        fnl(ix,iy,iz,7) = ky * flux_fourier(ix,iy,iz,1) - &
                            kx * flux_fourier(ix,iy,iz,2)
                        

                        !dP/dt
                        fnl(ix,iy,iz,8) = 0.

                        if (if_AEB) then 
                            ! !rho 
                            fnl(ix,iy,iz,1) = fnl(ix,iy,iz,1) - 2.0 * uu_fourier(ix,iy,iz,1) / tau_exp

                            !momentum -- need to include (drho/dt)_exp as we are using convervation form
                            fnl(ix,iy,iz,2) = fnl(ix,iy,iz,2) - 2.0 * uu_fourier(ix,iy,iz,2) / tau_exp
                            fnl(ix,iy,iz,3) = fnl(ix,iy,iz,3) - 3.0 * uu_fourier(ix,iy,iz,3) / tau_exp
                            fnl(ix,iy,iz,4) = fnl(ix,iy,iz,4) - 3.0 * uu_fourier(ix,iy,iz,4) / tau_exp

                            !B-field
                            fnl(ix,iy,iz,5) = fnl(ix,iy,iz,5) - 2.0 * uu_fourier(ix,iy,iz,5) / tau_exp
                            fnl(ix,iy,iz,6) = fnl(ix,iy,iz,6) - 1.0 * uu_fourier(ix,iy,iz,6) / tau_exp
                            fnl(ix,iy,iz,7) = fnl(ix,iy,iz,7) - 1.0 * uu_fourier(ix,iy,iz,7) / tau_exp

                            !energy density -- remember to update P after each time step if expansion
                            fnl(ix,iy,iz,8) = fnl(ix,iy,iz,8) - 2.0 * adiabatic_index * & 
                                uu_fourier(ix,iy,iz,8) / tau_exp
                        endif

                        if (if_visc .and. if_visc_exp) then 
                            ! if (if_conserve_background .and. (ix == 1) .and. (iz == 1)) then
                            !     cycle
                            ! endif
                            fnl(ix,iy,iz,2) = fnl(ix,iy,iz,2) - viscosity * uu_fourier(ix,iy,iz,2) &
                                * k_square(ix,iy,iz)
                            fnl(ix,iy,iz,3) = fnl(ix,iy,iz,3) - viscosity * uu_fourier(ix,iy,iz,3) &
                                * k_square(ix,iy,iz)
                            fnl(ix,iy,iz,4) = fnl(ix,iy,iz,4) - viscosity * uu_fourier(ix,iy,iz,4) &
                                * k_square(ix,iy,iz)
                        endif

                        if (if_resis .and. if_resis_exp) then 
                            if (if_conserve_background .and. (ix == 1) .and. (iz == 1)) then
                                cycle
                            endif
                            fnl(ix,iy,iz,5) = fnl(ix,iy,iz,5) - resistivity * uu_fourier(ix,iy,iz,5) &
                                * k_square(ix,iy,iz)
                            fnl(ix,iy,iz,6) = fnl(ix,iy,iz,6) - resistivity * uu_fourier(ix,iy,iz,6) &
                                * k_square(ix,iy,iz)
                            fnl(ix,iy,iz,7) = fnl(ix,iy,iz,7) - resistivity * uu_fourier(ix,iy,iz,7) &
                                * k_square(ix,iy,iz)
                        endif
                    enddo 
                enddo 
            enddo
        end subroutine  calc_rhs


        subroutine update_uu_prim_from_uu
            implicit none 

            uu_prim(:,:,:,1) = uu(:,:,:,2) / uu(:,:,:,1)
            uu_prim(:,:,:,2) = uu(:,:,:,3) / uu(:,:,:,1)
            uu_prim(:,:,:,3) = uu(:,:,:,4) / uu(:,:,:,1)
            
        end subroutine update_uu_prim_from_uu

        subroutine calc_current_density_real
            implicit none 

            integer :: ix,iy,iz,ivar
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax 
            complex :: kx, ky, kz

            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = yj_offset(myid_j+1) + 1
            iymax = yj_offset(myid_j+1) + yj_size(myid_j+1)

            izmin = 1 
            izmax = nz


            do iz = izmin,izmax 
                kz = cmplx(0,wave_number_z(iz)) * radius0/radius
                do iy = iymin,iymax 
                    ky = cmplx(0,wave_number_y(iy)) * radius0/radius
                    do ix = ixmin,ixmax
                        kx = cmplx(0,wave_number_x(ix))

                        if (if_AEB .and. if_corotating) then
                            kx = cmplx(0,wave_number_x(ix)) * cos_cor_ang + &
                                cmplx(0,wave_number_y(iy)) * sin_cor_ang
                            ky = (-cmplx(0,wave_number_x(ix)) * sin_cor_ang + &
                                cmplx(0,wave_number_y(iy)) * cos_cor_ang) * &
                                radius0/radius
                        endif

                        !Jx = dBz/dy - dBy/dz
                        current_density_fourier(ix,iy,iz,1) = ky * uu_fourier(ix,iy,iz,7) - &
                            kz * uu_fourier(ix,iy,iz,6)
                        !Jy = dBx/dz - dBz/dx
                        current_density_fourier(ix,iy,iz,2) = kz * uu_fourier(ix,iy,iz,5) - &
                            kx * uu_fourier(ix,iy,iz,7)
                        !Jz = dBy/dx - dBx/dy
                        current_density_fourier(ix,iy,iz,3) = kx * uu_fourier(ix,iy,iz,6) - &
                            ky * uu_fourier(ix,iy,iz,5)
                    enddo 
                enddo 
            enddo


            do ivar = 1, 3 
                w_zxy(:,:,:) = current_density_fourier(:,:,:,ivar)

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
                        current_density(:,iy,iz,ivar) = fx_aux(:) 
                    enddo
                enddo
            enddo
        end subroutine calc_current_density_real

        subroutine calc_gradient_velocity_real
            implicit none 

            integer :: ix,iy,iz,ivar
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax 
            complex :: kx, ky, kz

            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = yj_offset(myid_j+1) + 1
            iymax = yj_offset(myid_j+1) + yj_size(myid_j+1)

            izmin = 1 
            izmax = nz


            do iz = izmin,izmax 
                kz = cmplx(0,wave_number_z(iz)) * radius0/radius
                do iy = iymin,iymax 
                    ky = cmplx(0,wave_number_y(iy)) * radius0/radius
                    do ix = ixmin,ixmax
                        kx = cmplx(0,wave_number_x(ix))

                        if (if_AEB .and. if_corotating) then
                            kx = cmplx(0,wave_number_x(ix)) * cos_cor_ang + &
                                cmplx(0,wave_number_y(iy)) * sin_cor_ang
                            ky = (-cmplx(0,wave_number_x(ix)) * sin_cor_ang + &
                                cmplx(0,wave_number_y(iy)) * cos_cor_ang) * &
                                radius0/radius
                        endif

                        ! dux/dx
                        grad_velocity_fourier(ix,iy,iz,1) = kx * uu_fourier(ix,iy,iz,2)
                        ! dux/dy
                        grad_velocity_fourier(ix,iy,iz,2) = ky * uu_fourier(ix,iy,iz,2)
                        ! dux/dz
                        grad_velocity_fourier(ix,iy,iz,3) = kz * uu_fourier(ix,iy,iz,2)

                        ! duy/dx
                        grad_velocity_fourier(ix,iy,iz,4) = kx * uu_fourier(ix,iy,iz,3)
                        ! duy/dy
                        grad_velocity_fourier(ix,iy,iz,5) = ky * uu_fourier(ix,iy,iz,3)
                        ! duy/dz
                        grad_velocity_fourier(ix,iy,iz,6) = kz * uu_fourier(ix,iy,iz,3)

                        ! duz/dx
                        grad_velocity_fourier(ix,iy,iz,7) = kx * uu_fourier(ix,iy,iz,4)
                        ! duz/dy
                        grad_velocity_fourier(ix,iy,iz,8) = ky * uu_fourier(ix,iy,iz,4)
                        ! duz/dz
                        grad_velocity_fourier(ix,iy,iz,9) = kz * uu_fourier(ix,iy,iz,4)
                    enddo 
                enddo 
            enddo

            ! note: uu is rho*u, so we need to remove rho
            grad_velocity_fourier(:,:,:,:) = grad_velocity_fourier(:,:,:,:)/rho0

            do ivar = 1, 9 
                w_zxy(:,:,:) = grad_velocity_fourier(:,:,:,ivar)

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
                        grad_velocity(:,iy,iz,ivar) = fx_aux(:) 
                    enddo
                enddo
            enddo
        end subroutine calc_gradient_velocity_real

        subroutine calc_flux_for_pressure
            implicit none 
            integer :: ix,iy,iz
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax 
            real :: ptot, p, Bx, By, Bz, ux,uy,uz, udotb, rho
            ! calculate -rho u dot u + J cross B

    
            ixmin = 1
            ixmax = nx 
            iymin = yi_offset(myid_i+1) + 1
            iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
            izmin = zj_offset(myid_j+1) + 1
            izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)

            ! order of variables:
            ! gradient_velocity order:
            ! 1: duxdx, 2: duxdy, 3: duxdz
            ! 4: duydx, 5: duydy, 6: duydz
            ! 7: duzdx, 8: duzdy, 9: duzdz

            do iz = izmin,izmax 
                do iy = iymin, iymax 
                    do ix = ixmin, ixmax 
                        flux_pressure(ix,iy,iz,1) = -uu(ix,iy,iz,2) * grad_velocity(ix,iy,iz,1) &
                            -uu(ix,iy,iz,3) * grad_velocity(ix,iy,iz,2) &
                            -uu(ix,iy,iz,4) * grad_velocity(ix,iy,iz,3) &
                            + current_density(ix,iy,iz,2) * uu(ix,iy,iz,7) &
                            - current_density(ix,iy,iz,3) * uu(ix,iy,iz,6)

                        flux_pressure(ix,iy,iz,2) = -uu(ix,iy,iz,2) * grad_velocity(ix,iy,iz,4) &
                            -uu(ix,iy,iz,3) * grad_velocity(ix,iy,iz,5) &
                            -uu(ix,iy,iz,4) * grad_velocity(ix,iy,iz,6) &
                            + current_density(ix,iy,iz,3) * uu(ix,iy,iz,5) &
                            - current_density(ix,iy,iz,1) * uu(ix,iy,iz,7)

                        flux_pressure(ix,iy,iz,3) = -uu(ix,iy,iz,2) * grad_velocity(ix,iy,iz,7) &
                            -uu(ix,iy,iz,3) * grad_velocity(ix,iy,iz,8) &
                            -uu(ix,iy,iz,4) * grad_velocity(ix,iy,iz,9) &
                            + current_density(ix,iy,iz,1) * uu(ix,iy,iz,6) &
                            - current_density(ix,iy,iz,2) * uu(ix,iy,iz,5)
                    enddo 
                enddo 
            enddo 
        end subroutine calc_flux_for_pressure

        subroutine transform_flux_for_pressure_real_to_fourier
            !this subroutine transforms flux to flux_fourier
            implicit none 
            
            integer :: ix,iy,iz,ivar
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax

            iymin = yi_offset(myid_i+1) + 1
            iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
            izmin = zj_offset(myid_j+1) + 1 
            izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)
            
            do ivar = 1, nfluxPressure
                !do fourier transform along x
                do iz = izmin, izmax 
                    do iy = iymin,iymax
                        fx_aux(:) = flux_pressure(:,iy,iz,ivar)
                        call fftw_execute_dft_r2c(plan_fft_x, fx_aux, fx_aux_ft)
                        w_xyz(:,iy,iz) = fx_aux_ft(:) / nx !normalization
                    enddo
                enddo

                call from_xyz_to_zxy

                !copy to flux_fourier
                flux_pressure_fourier(:,:,:,ivar) = w_zxy(:,:,:)
            enddo
        end subroutine transform_flux_for_pressure_real_to_fourier

        subroutine calc_pressure_fourier
            implicit none 
            
            integer :: ix,iy,iz,ivar
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax 
            complex :: kx, ky, kz
            real :: k2

            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = yj_offset(myid_j+1) + 1
            iymax = yj_offset(myid_j+1) + yj_size(myid_j+1)

            izmin = 1 
            izmax = nz


            do iz = izmin,izmax 
                kz = cmplx(0,wave_number_z(iz)) * radius0/radius
                do iy = iymin,iymax 
                    ky = cmplx(0,wave_number_y(iy)) * radius0/radius
                    do ix = ixmin,ixmax
                        kx = cmplx(0,wave_number_x(ix))

                        ! k2 = wave_number_x(ix)**2 + wave_number_y(iy)**2 &
                        !     + wave_number_z(iz)**2
                        k2 = k_square(ix,iy,iz)

                        if (if_AEB .and. if_corotating) then
                            kx = cmplx(0,wave_number_x(ix)) * cos_cor_ang + &
                                cmplx(0,wave_number_y(iy)) * sin_cor_ang
                            ky = (-cmplx(0,wave_number_x(ix)) * sin_cor_ang + &
                                cmplx(0,wave_number_y(iy)) * cos_cor_ang) * &
                                radius0/radius
                        endif

                        if (k2 < 1e-10) then  !background field, not important in Fourier space
                            uu_fourier(ix,iy,iz,8) = 0.
                            cycle
                        endif 


                        uu_fourier(ix,iy,iz,8) = -(kx * flux_pressure_fourier(ix,iy,iz,1) &
                            + ky * flux_pressure_fourier(ix,iy,iz,2) &
                            + kz * flux_pressure_fourier(ix,iy,iz,3))/k2
                    enddo 
                enddo 
            enddo
            
        end subroutine calc_pressure_fourier

        ! subroutine add_pressure_fourier_to_flux_fourier
        !     ! no use!!!
        !     implicit none 
        !     ! Txx
        !     flux_fourier(:,:,:,1) = flux_fourier(:,:,:,1) + uu_fourier(:,:,:,8)
        !     ! Tyy
        !     flux_fourier(:,:,:,5) = flux_fourier(:,:,:,5) + uu_fourier(:,:,:,8)
        !     ! Tzz
        !     flux_fourier(:,:,:,9) = flux_fourier(:,:,:,9) + uu_fourier(:,:,:,8) 
        ! end subroutine 


        subroutine calc_divB_real
            ! Get the maximum divB in the real space
            implicit none 
            integer :: ix,iy,iz,ierr
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax
            real :: divB_iter
            complex :: kx,ky,kz

            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = yj_offset(myid_j+1) + 1
            iymax = yj_offset(myid_j+1) + yj_size(myid_j+1)

            izmin = 1
            izmax = nz

            do iz=izmin,izmax
                kz = cmplx(0,wave_number_z(iz)) * radius0/radius
                do iy=iymin,iymax
                    ky = cmplx(0,wave_number_y(iy)) * radius0/radius
                    do ix=ixmin,ixmax
                        kx = cmplx(0,wave_number_x(ix))

                        if (if_AEB .and. if_corotating) then
                            kx = cmplx(0,wave_number_x(ix)) * cos_cor_ang + &
                                cmplx(0,wave_number_y(iy)) * sin_cor_ang
                            ky = (-cmplx(0,wave_number_x(ix)) * sin_cor_ang + &
                                cmplx(0,wave_number_y(iy)) * cos_cor_ang) * &
                                radius0/radius
                        endif

                        divB_arr_fourier(ix,iy,iz,1) =  kx * uu_fourier(ix,iy,iz,5) + &
                            ky * uu_fourier(ix,iy,iz,6) + kz * uu_fourier(ix,iy,iz,7) 
                    enddo
                enddo
            enddo
        
            ! transform back to real space
            w_zxy(:,:,:) = divB_arr_fourier(:,:,:,1)

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
                    divB_arr(:,iy,iz,1) = fx_aux(:) 
                enddo
            enddo
        end subroutine calc_divB_real

        subroutine calc_divV_real
            ! Get the maximum divB in the real space
            implicit none 
            integer :: ix,iy,iz,ierr
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax
            real :: divB_iter
            complex :: kx,ky,kz

            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = yj_offset(myid_j+1) + 1
            iymax = yj_offset(myid_j+1) + yj_size(myid_j+1)

            izmin = 1
            izmax = nz

            do iz=izmin,izmax
                kz = cmplx(0,wave_number_z(iz)) * radius0/radius
                do iy=iymin,iymax
                    ky = cmplx(0,wave_number_y(iy)) * radius0/radius
                    do ix=ixmin,ixmax
                        kx = cmplx(0,wave_number_x(ix))

                        if (if_AEB .and. if_corotating) then
                            kx = cmplx(0,wave_number_x(ix)) * cos_cor_ang + &
                                cmplx(0,wave_number_y(iy)) * sin_cor_ang
                            ky = (-cmplx(0,wave_number_x(ix)) * sin_cor_ang + &
                                cmplx(0,wave_number_y(iy)) * cos_cor_ang) * &
                                radius0/radius
                        endif

                        divV_arr_fourier(ix,iy,iz,1) =  (kx * uu_fourier(ix,iy,iz,2) + &
                            ky * uu_fourier(ix,iy,iz,3) + kz * uu_fourier(ix,iy,iz,4))/rho0
                    enddo
                enddo
            enddo
        
            ! transform back to real space
            w_zxy(:,:,:) = divV_arr_fourier(:,:,:,1)

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
                    divV_arr(:,iy,iz,1) = fx_aux(:) 
                enddo
            enddo
        end subroutine calc_divV_real
end module mhdrhs
