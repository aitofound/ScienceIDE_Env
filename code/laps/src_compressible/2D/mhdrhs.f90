module mhdrhs 
    use mhdinit, only: nx,ny,nz,uu, uu_prim, &
        fnl,adiabatic_index,flux, &
        flux_fourier,wave_number_x, wave_number_y,&
        if_AEB,uu_fourier,expand_term,&
        expand_term_fourier,sin_cor_ang,cos_cor_ang,if_corotating,&
        if_hall,ion_inertial_length, &
        current_density,current_density_fourier, &
        if_resis, if_resis_exp, resistivity, &
        if_visc, if_visc_exp, viscosity, &
        k_square, if_conserve_background, if_z_radial, &
        if_external_force, external_force, external_force_fourier,nextern,&
        Lx,Ly,time,xgrid,ygrid
    use AEBmod, only: radius0, radius, tau_exp
        
    use fftw 
    use parallel 

    implicit none 

    contains 
    
        subroutine calc_flux
            !calculate the nonlinear fluxes in real space.
            implicit none 
            integer :: ix,iy,iz
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax 
            real :: ptot, p, Bx, By, Bz, ux,uy,uz, udotb, rho

            if (if_hall) then 
                call calc_current_density_real
            endif

            ixmin = 1
            ixmax = nx 
            iymin = yi_offset(myid_i+1) + 1
            iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
            izmin = 1
            izmax = 1

            ! order of variables:
            ! flux variable order:
            ! 1:3 : rho*ux, rho*uy, rho*uz
            ! 4:6 : Txx, Tyx, Tzx
            ! 7:9 : Txy, Tyy, Tzy
            ! 10:12 : Txz, Tyz, Tzz
            ! 13:15 : Ex, Ey, Ez
            ! 16:18 : energy fluxes

            do iz = izmin,izmax 
                do iy = iymin, iymax 
                    do ix = ixmin, ixmax 
                        p = uu_prim(ix,iy,iz,4)
                        Bx = uu(ix,iy,iz,5) 
                        By = uu(ix,iy,iz,6) 
                        Bz = uu(ix,iy,iz,7) 
                        ux = uu_prim(ix,iy,iz,1)
                        uy = uu_prim(ix,iy,iz,2)
                        uz = uu_prim(ix,iy,iz,3)
                        rho = uu(ix,iy,iz,1)

                        ptot =  p + 0.5 * (Bx**2 + By**2 + Bz**2)
                        udotb = ux*Bx + uy*By + uz*Bz

                        !mass flux
                        flux(ix,iy,iz,1:3) = uu(ix,iy,iz,2:4)

                        !momentum flux along x
                        flux(ix,iy,iz,4) = uu(ix,iy,iz,2) * ux - Bx * Bx + ptot
                        flux(ix,iy,iz,5) = uu(ix,iy,iz,3) * ux - By * Bx
                        flux(ix,iy,iz,6) = uu(ix,iy,iz,4) * ux - Bz * Bx

                        !momentum flux along y
                        flux(ix,iy,iz,7) = uu(ix,iy,iz,2) * uy - Bx * By
                        flux(ix,iy,iz,8) = uu(ix,iy,iz,3) * uy - By * By + ptot
                        flux(ix,iy,iz,9) = uu(ix,iy,iz,4) * uy - Bz * By

                        !momentum flux along z
                        flux(ix,iy,iz,10) = uu(ix,iy,iz,2) * uz - Bx * Bz
                        flux(ix,iy,iz,11) = uu(ix,iy,iz,3) * uz - By * Bz 
                        flux(ix,iy,iz,12) = uu(ix,iy,iz,4) * uz - Bz * Bz + ptot

                        !electric field
                        flux(ix,iy,iz,13) = uz * By - uy * Bz 
                        flux(ix,iy,iz,14) = ux * Bz - uz * Bx
                        flux(ix,iy,iz,15) = uy * Bx - ux * By

                        !energy flux
                        flux(ix,iy,iz,16) = (uu(ix,iy,iz,8) + ptot) * ux - udotb * Bx 
                        flux(ix,iy,iz,17) = (uu(ix,iy,iz,8) + ptot) * uy - udotb * By 
                        flux(ix,iy,iz,18) = (uu(ix,iy,iz,8) + ptot) * uz - udotb * Bz 

                        !expanding term for energy density
                        !note: need to include the rho & velocity & magnetic field expansion
                        if (if_AEB) then 
                            if (if_z_radial) then 
                                expand_term(ix,iy,iz,1) = -2 * adiabatic_index/(adiabatic_index-1) * p / tau_exp &
                                    - ( Bx**2 + By**2 + 2.0 * Bz**2 ) / tau_exp &
                                    - ( 2 * uu(ix,iy,iz,2) * ux + 2 * uu(ix,iy,iz,3) * uy &
                                    +  uu(ix,iy,iz,4) * uz ) / tau_exp
                            else 
                                expand_term(ix,iy,iz,1) = -2 * adiabatic_index/(adiabatic_index-1) * p / tau_exp &
                                    - (2.0 * Bx**2 + By**2 + Bz**2 ) / tau_exp &
                                    - ( uu(ix,iy,iz,2) * ux + 2 * uu(ix,iy,iz,3) * uy &
                                    + 2 * uu(ix,iy,iz,4) * uz ) / tau_exp
                            endif
                        endif

                        if (if_hall) then 
                            !Ex_h = di/rho * (Jy*Bz-Jz*By)
                            flux(ix,iy,iz,13) = flux(ix,iy,iz,13) + ion_inertial_length/rho * &
                                (current_density(ix,iy,iz,2) * uu(ix,iy,iz,7) - &
                                current_density(ix,iy,iz,3) * uu(ix,iy,iz,6))
                            !Ey_h = di/rho * (Jz*Bx-Jx*Bz)
                            flux(ix,iy,iz,14) = flux(ix,iy,iz,14) + ion_inertial_length/rho * &
                                (current_density(ix,iy,iz,3) * uu(ix,iy,iz,5) - &
                                current_density(ix,iy,iz,1) * uu(ix,iy,iz,7))
                            !Ez_h = di/rho * (Jx*By-Jy*Bx)
                            flux(ix,iy,iz,15) = flux(ix,iy,iz,15) + ion_inertial_length/rho * &
                                (current_density(ix,iy,iz,1) * uu(ix,iy,iz,6) - &
                                current_density(ix,iy,iz,2) * uu(ix,iy,iz,5))
                        endif
                    enddo 
                enddo 
            enddo


            ! if external force
            if (if_external_force) then 
                call calc_external_force_real
            endif
        end subroutine calc_flux


    
        subroutine transform_flux_real_to_fourier
            !this subroutine transforms flux to flux_fourier
            implicit none 
            
            integer :: ix,iy,iz,ivar
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax
            integer :: nflux = 18

            do ivar = 1, nflux
                iymin = yi_offset(myid_i+1) + 1
                iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
                izmin = 1 
                izmax = 1

                !do fourier transform along x
                do iz = izmin, izmax 
                    do iy = iymin,iymax
                        fx_aux(:) = flux(:,iy,iz,ivar)
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

                !copy to flux_fourier
                flux_fourier(:,:,:,ivar) = w_yx(:,:,:)
            enddo

            if (if_AEB) then 
                !do fourier transform along x
                iymin = yi_offset(myid_i+1) + 1
                iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
                izmin = 1 
                izmax = 1

                do iz = izmin, izmax 
                    do iy = iymin,iymax
                        fx_aux(:) = expand_term(:,iy,iz,1)
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

                !copy to flux_fourier
                expand_term_fourier(:,:,:,1) = w_yx(:,:,:)
            endif

            if (if_external_force) then 
                do ivar = 1, nextern
                    !do fourier transform along x
                    iymin = yi_offset(myid_i+1) + 1
                    iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
                    izmin = 1 
                    izmax = 1

                    do iz = izmin, izmax 
                        do iy = iymin,iymax
                            fx_aux(:) = external_force(:,iy,iz,ivar)
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

                    !copy to flux_fourier
                    external_force_fourier(:,:,:,ivar) = w_yx(:,:,:)
                enddo
            endif
        end subroutine transform_flux_real_to_fourier

        subroutine calc_rhs
            !calculate rhs of the MHD equation in conservation form
            !call after transform_flux_real_to_fourier
            implicit none 
            integer :: ix,iy,iz
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax 
            complex :: kx, ky,kz

            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = 1
            iymax = ny

            izmin = 1 
            izmax = 1

            do iz = izmin,izmax 
                kz = cmplx(0,0)
                do iy = iymin,iymax 
                    ky = cmplx(0,wave_number_y(iy)) * radius0/radius
                    do ix = ixmin,ixmax
                        kx = cmplx(0,wave_number_x(ix))
                        if (if_AEB .and. if_z_radial) then 
                            kx = kx * radius0/radius
                        endif

                        if (if_AEB .and. if_corotating) then
                            kx = cmplx(0,wave_number_x(ix)) * cos_cor_ang + &
                                cmplx(0,wave_number_y(iy)) * sin_cor_ang
                            ky = (-cmplx(0,wave_number_x(ix)) * sin_cor_ang + &
                                cmplx(0,wave_number_y(iy)) * cos_cor_ang) * &
                                radius0/radius
                        endif

                        !drho/dt
                        fnl(ix,iy,iz,1) = -( kx * flux_fourier(ix,iy,iz,1) + &
                            ky * flux_fourier(ix,iy,iz,2) + kz * &
                            flux_fourier(ix,iy,iz,3) )

                        !d(rho*u)/dt
                        fnl(ix,iy,iz,2) = -( kx * flux_fourier(ix,iy,iz,4) + &
                            ky * flux_fourier(ix,iy,iz,5) + kz * &
                            flux_fourier(ix,iy,iz,6))
                        fnl(ix,iy,iz,3) = -( kx * flux_fourier(ix,iy,iz,7) + &
                            ky * flux_fourier(ix,iy,iz,8) + kz * &
                            flux_fourier(ix,iy,iz,9))
                        fnl(ix,iy,iz,4) = -( kx * flux_fourier(ix,iy,iz,10) + &
                            ky * flux_fourier(ix,iy,iz,11) + kz * &
                            flux_fourier(ix,iy,iz,12))

                        !dB/dt
                        fnl(ix,iy,iz,5) = kz * flux_fourier(ix,iy,iz,14) - &
                            ky * flux_fourier(ix,iy,iz,15)
                        fnl(ix,iy,iz,6) = kx * flux_fourier(ix,iy,iz,15) - &
                            kz * flux_fourier(ix,iy,iz,13)
                        fnl(ix,iy,iz,7) = ky * flux_fourier(ix,iy,iz,13) - &
                            kx * flux_fourier(ix,iy,iz,14)

                        !de/dt
                        fnl(ix,iy,iz,8) = -( kx * flux_fourier(ix,iy,iz,16) + &
                            ky * flux_fourier(ix,iy,iz,17) + kz * &
                            flux_fourier(ix,iy,iz,18) )

                        if (if_AEB) then 
                            !rho
                            fnl(ix,iy,iz,1) = fnl(ix,iy,iz,1) - 2.0 * uu_fourier(ix,iy,iz,1) / tau_exp

                            !momentum -- need to include (drho/dt)_exp as we are using convervation form
                            if (if_z_radial) then 
                                fnl(ix,iy,iz,2) = fnl(ix,iy,iz,2) - 3.0 * uu_fourier(ix,iy,iz,2) / tau_exp
                                fnl(ix,iy,iz,3) = fnl(ix,iy,iz,3) - 3.0 * uu_fourier(ix,iy,iz,3) / tau_exp
                                fnl(ix,iy,iz,4) = fnl(ix,iy,iz,4) - 2.0 * uu_fourier(ix,iy,iz,4) / tau_exp
                            else
                                fnl(ix,iy,iz,2) = fnl(ix,iy,iz,2) - 2.0 * uu_fourier(ix,iy,iz,2) / tau_exp
                                fnl(ix,iy,iz,3) = fnl(ix,iy,iz,3) - 3.0 * uu_fourier(ix,iy,iz,3) / tau_exp
                                fnl(ix,iy,iz,4) = fnl(ix,iy,iz,4) - 3.0 * uu_fourier(ix,iy,iz,4) / tau_exp
                            endif

                            !B-field
                            if (if_z_radial) then
                                fnl(ix,iy,iz,5) = fnl(ix,iy,iz,5) - 1.0 * uu_fourier(ix,iy,iz,5) / tau_exp
                                fnl(ix,iy,iz,6) = fnl(ix,iy,iz,6) - 1.0 * uu_fourier(ix,iy,iz,6) / tau_exp
                                fnl(ix,iy,iz,7) = fnl(ix,iy,iz,7) - 2.0 * uu_fourier(ix,iy,iz,7) / tau_exp  
                            else
                                fnl(ix,iy,iz,5) = fnl(ix,iy,iz,5) - 2.0 * uu_fourier(ix,iy,iz,5) / tau_exp
                                fnl(ix,iy,iz,6) = fnl(ix,iy,iz,6) - 1.0 * uu_fourier(ix,iy,iz,6) / tau_exp
                                fnl(ix,iy,iz,7) = fnl(ix,iy,iz,7) - 1.0 * uu_fourier(ix,iy,iz,7) / tau_exp  
                            endif

                            !energy density
                            fnl(ix,iy,iz,8) = fnl(ix,iy,iz,8) + expand_term_fourier(ix,iy,iz,1)
                        endif

                        if (if_visc .and. if_visc_exp) then 
                            fnl(ix,iy,iz,2) = fnl(ix,iy,iz,2) - viscosity * uu_fourier(ix,iy,iz,2) &
                                * k_square(ix,iy,iz)
                            fnl(ix,iy,iz,3) = fnl(ix,iy,iz,3) - viscosity * uu_fourier(ix,iy,iz,3) &
                                * k_square(ix,iy,iz)
                            fnl(ix,iy,iz,4) = fnl(ix,iy,iz,4) - viscosity * uu_fourier(ix,iy,iz,4) &
                                * k_square(ix,iy,iz)
                        endif

                        if (if_resis .and. if_resis_exp) then 
                            if (if_conserve_background .and. (ix == 1)) then
                                cycle
                            endif
                            fnl(ix,iy,iz,5) = fnl(ix,iy,iz,5) - resistivity * uu_fourier(ix,iy,iz,5) &
                                * k_square(ix,iy,iz)
                            fnl(ix,iy,iz,6) = fnl(ix,iy,iz,6) - resistivity * uu_fourier(ix,iy,iz,6) &
                                * k_square(ix,iy,iz)
                            fnl(ix,iy,iz,7) = fnl(ix,iy,iz,7) - resistivity * uu_fourier(ix,iy,iz,7) &
                                * k_square(ix,iy,iz)
                        endif

                        if (if_external_force) then 
                            fnl(ix,iy,iz,7) = fnl(ix,iy,iz,7) + external_force_fourier(ix,iy,iz,1)
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
            
            uu_prim(:,:,:,4) = (uu(:,:,:,8) - 0.5 * ( uu(:,:,:,2) * &
                uu_prim(:,:,:,1) + uu(:,:,:,3) * uu_prim(:,:,:,2) + &
                uu(:,:,:,4) * uu_prim(:,:,:,3) + (uu(:,:,:,5))**2 + &
                (uu(:,:,:,6))**2 + (uu(:,:,:,7))**2)) &
                * (adiabatic_index-1)
        end subroutine update_uu_prim_from_uu



        subroutine calc_current_density_real
            implicit none 

            integer :: ix,iy,iz,ivar
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax 
            complex :: kx, ky, kz

            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = 1
            iymax = ny

            izmin = 1 
            izmax = 1


            do iz = izmin,izmax 
                kz = cmplx(0,0) 
                do iy = iymin,iymax 
                    ky = cmplx(0,wave_number_y(iy)) * radius0/radius
                    do ix = ixmin,ixmax
                        kx = cmplx(0,wave_number_x(ix))
                        if (if_AEB .and. if_z_radial) then 
                            kx = kx * radius0/radius
                        endif

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
                w_yx(:,:,:) = current_density_fourier(:,:,:,ivar)

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
                        current_density(:,iy,iz,ivar) = fx_aux(:)
                    enddo
                enddo
            enddo
        end subroutine calc_current_density_real

        subroutine calc_external_force_real
            ! user defined function to calculate external force
            ! here is an example to add a forcing term for Bz
            ! and the force is moving along y direction (Alfven wing)
            implicit none 
            integer :: ix,iy,iz
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax 
            real :: xc,yc,x_width,y_width,yc_mirror,func_x, dBdt

            dBdt = 0.2

            ixmin = 1
            ixmax = nx 
            iymin = yi_offset(myid_i+1) + 1
            iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
            izmin = 1
            izmax = 1


            xc = 0.5 * Lx 
            x_width = 0.05 * Ly ! note: we let the shape isotropic

            yc = mod(0.2*Ly + 0.3*time, Ly)
            y_width = 0.05 * Ly 



            do iz = izmin,izmax 
                do iy = iymin, iymax 
                    do ix = ixmin, ixmax 
                        ! option 1: Gaussian in x
                        func_x = exp(-((xgrid(ix)-xc)/(x_width))**2)

                        ! ! option 2: a bi-polar perturbation in x
                        ! func_x = exp(-((xgrid(ix)-xc)/(x_width))**2) * &
                        !     tanh((xgrid(ix)-xc)/x_width) / 0.375261


                        external_force(ix,iy,iz,1) = dBdt * func_x * exp(-((ygrid(iy)-yc)/(y_width))**2)

                        ! in case the force center is near the upper/lower boundary
                        yc_mirror = yc + Ly 
                        external_force(ix,iy,iz,1) = external_force(ix,iy,iz,1) + &
                            dBdt * func_x * exp(- ((ygrid(iy)-yc_mirror)/(y_width))**2)

                        yc_mirror = yc - Ly 
                        external_force(ix,iy,iz,1) = external_force(ix,iy,iz,1) + &
                            dBdt * func_x * exp(- ((ygrid(iy)-yc_mirror)/(y_width))**2)
                    enddo 
                enddo 
            enddo
        end subroutine calc_external_force_real
end module mhdrhs