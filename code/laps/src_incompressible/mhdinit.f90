module mhdinit
    use parallel
    implicit none

    integer :: nx = 128, ny = 128, nz = 64, nvar = 8, nvarPrim = 3, nflux = 3, &
        nfluxPressure = 3

    real :: pi = 3.141592653589793

    integer :: ifield = 0, ipert = 0

    real :: Lx, Ly, Lz, dx, dy, dz


    real :: T0 = 1.0, rho0 = 1.0, P0 = 1.0  ! initial uniform temerature & rho 
    real :: B0 = 1.0, a0 = 0.05 !for current sheet

    real, allocatable, dimension(:) :: xgrid, ygrid, zgrid

    !logical :: if_isothermal = .false.

    real :: adiabatic_index = 5./3. !, T_isothermal = 1.0
    logical :: if_resis = .false., if_AEB = .false.,if_corotating = .false.,&
        if_Hall = .false., if_resis_exp = .false., if_conserve_background = .false., &
        if_visc = .false., if_visc_exp = .false.
    real :: resistivity = 0.0, viscosity = 0.0, ion_inertial_length = 0.0
    real :: corotating_angle = 0.0, cos_cor_ang, sin_cor_ang

    real :: time = 0.0

    !uu is the conservation variables:
    !rho, rho*(ux,uy,uz), bx,by,bz, e=(p/(k-1)+0.5*(rho*u^2+B^2))
    !note that in initiliazation, it is treated as primitive variables:
    !rho,ux,uy,uz,bx,by,bz,p
    real, allocatable, dimension(:,:,:,:) :: uu,uu_prim,flux,flux_pressure,&
        expand_term,current_density,grad_velocity,divB_arr,divV_arr
    complex, allocatable, dimension(:,:,:,:) :: uu_fourier,flux_fourier,&
        flux_pressure_fourier,expand_term_fourier,current_density_fourier,&
        grad_velocity_fourier,divB_arr_fourier,divV_arr_fourier !,uu_prim_fourier
    

    !arrays storing the wave number information
    real,allocatable,dimension(:) :: wave_number_x, wave_number_y, wave_number_z
    real,allocatable,dimension(:,:,:) :: k_square

    complex, allocatable, dimension(:,:,:,:) :: fnl, fnl_rk


    !user-defined-----------------------------------------
    real :: U_fast,U_slow,n_fast,n_slow,press0,shear_width,bx0,by0,&
        bz0,db0,dbx,dby,dbz, in_out_ratio = 1.0, dv0 = 0, drho0 = 0
    integer :: nmode = 16, nmodex = 16, nmodey = 16,nmodez = 16

    real :: B0_SB,dB_SB,R0_SB,R1_SB,H_SB,Rm_SB,Rd_SB

    contains 

        subroutine grid_initialize
            ! must be called after parallel_initialize
            ! initialize grids and wave-numbers
            implicit none

            integer :: ix,iy,iz
            integer :: ixmin, ixmax, iymin, iymax, izmin, izmax 

            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = yj_offset(myid_j+1) + 1
            iymax = yj_offset(myid_j+1) + yj_size(myid_j+1)

            izmin = 1 
            izmax = nz

            allocate(xgrid(nx), ygrid(ny), zgrid(nz), &
                wave_number_x(nx), wave_number_y(ny), wave_number_z(nz), &
                k_square(ixmin:ixmax,iymin:iymax,izmin:izmax))

            dx = Lx / nx 
            dy = Ly / ny 
            dz = Lz / nz 

            do ix=1,nx 
                xgrid(ix) = 0 + (ix-1) * dx 

                if ( ix <= (nx/2+1) ) then
                    wave_number_x(ix) = 2*pi*(ix-1) / Lx
                else
                    wave_number_x(ix) = 2*pi*(ix-1-nx) / Lx
                endif
            enddo

            do iy=1,ny
                ygrid(iy) = 0 + (iy-1) * dy 

                if ( iy <= (ny/2+1) ) then
                    wave_number_y(iy) = 2*pi*(iy-1) / Ly
                else
                    wave_number_y(iy) = 2*pi*(iy-1-ny) / Ly
                endif
            enddo

            do iz=1,nz
                zgrid(iz) = 0 + (iz-1) * dz

                if ( iz <= (nz/2+1) ) then
                    wave_number_z(iz) = 2*pi*(iz-1) / Lz
                else
                    wave_number_z(iz) = 2*pi*(iz-1-nz) / Lz
                endif
            enddo


            !define k_square
            do iz=izmin,izmax
                do iy=iymin,iymax 
                    do ix=ixmin,ixmax 
                        k_square(ix,iy,iz) = (wave_number_x(ix))**2 + &
                            (wave_number_y(iy))**2 + &
                            (wave_number_z(iz))**2
                    enddo
                enddo
            enddo
        end subroutine grid_initialize

        subroutine arrays_initialize
            implicit none 
            integer :: ixmin, ixmax, iymin, iymax, izmin, izmax 
            !--------------------------------------------------
            !arrays in real space
            !index range for real-space arrays
            ixmin = 1
            ixmax = nx 

            iymin = yi_offset(myid_i+1) + 1 
            iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)

            izmin = zj_offset(myid_j+1) + 1 
            izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)

            allocate(uu(ixmin:ixmax, iymin:iymax, izmin:izmax, 1:nvar), &
                uu_prim(ixmin:ixmax, iymin:iymax, izmin:izmax, 1:nvarPrim), &
                flux(ixmin:ixmax, iymin:iymax, izmin:izmax, 1:nflux), &
                flux_pressure(ixmin:ixmax, iymin:iymax, izmin:izmax, 1:nfluxPressure),&
                current_density(ixmin:ixmax, iymin:iymax, izmin:izmax,1:3),&
                grad_velocity(ixmin:ixmax, iymin:iymax, izmin:izmax,1:9),&
                divB_arr(ixmin:ixmax, iymin:iymax, izmin:izmax,1),&
                divV_arr(ixmin:ixmax, iymin:iymax, izmin:izmax,1)) 

            if (if_AEB) then 
                allocate(expand_term(ixmin:ixmax, iymin:iymax, izmin:izmax,1))
            endif

            !if (if_Hall) then 
            !   allocate(current_density(ixmin:ixmax, iymin:iymax, izmin:izmax,3))
            !endif
            !--------------------------------------------------


            !--------------------------------------------------
            !uu in fourier space, note that the order is zxy
            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = yj_offset(myid_j+1) + 1
            iymax = yj_offset(myid_j+1) + yj_size(myid_j+1)

            izmin = 1 
            izmax = nz

            allocate(uu_fourier(ixmin:ixmax, iymin:iymax, izmin:izmax, 1:nvar),&
                flux_fourier(ixmin:ixmax, iymin:iymax, izmin:izmax, 1:nflux), &
                flux_pressure_fourier(ixmin:ixmax, iymin:iymax, izmin:izmax, 1:nfluxPressure), &
                fnl(ixmin:ixmax, iymin:iymax, izmin:izmax, 1:nvar), &
                fnl_rk(ixmin:ixmax, iymin:iymax, izmin:izmax, 1:nvar),&
                current_density_fourier(ixmin:ixmax, iymin:iymax, izmin:izmax,1:3),&
                grad_velocity_fourier(ixmin:ixmax, iymin:iymax, izmin:izmax,1:9),&
                divB_arr_fourier(ixmin:ixmax, iymin:iymax, izmin:izmax,1),&
                divV_arr_fourier(ixmin:ixmax, iymin:iymax, izmin:izmax,1))

            if (if_AEB) then 
                !allocate(uu_prim_fourier(ixmin:ixmax, iymin:iymax, izmin:izmax, 4:4))
                allocate(expand_term_fourier(ixmin:ixmax, iymin:iymax, izmin:izmax, 1))
            endif

            ! if (if_Hall) then 
            !     allocate(current_density_fourier(ixmin:ixmax, iymin:iymax, izmin:izmax, 3))
            ! endif
            !--------------------------------------------------

        end subroutine arrays_initialize

        subroutine background_fields_initialize
            implicit none 
            real :: kx, ky, kz
            integer :: ix, iy, iz, ivar, ik, &
                ixmin,ixmax,iymin,iymax,izmin,izmax
            real :: zlow,zcent,zup,ylow,ycent,yup,ninf = 1.0

            ixmin = 1
            ixmax = nx 

            iymin = yi_offset(myid_i+1) + 1 
            iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)

            izmin = zj_offset(myid_j+1) + 1 
            izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)

            uu(:,:,:,1) = rho0
            uu(:,:,:,8) = rho0*T0 !note this is pressure

            select case(ifield)
            
            Case(1)
                !Add a double current sheet B0 = B0x(z)
                zlow = 0.25 * Lz 
                zup = 0.75 * Lz
                zcent = 0.5 * Lz 

                do iz = izmin,izmax 
                    if (zgrid(iz) < zcent ) then 
                        uu(:,:,iz,5) =   B0 * tanh( (zgrid(iz) - zlow)/a0 ) 
                        uu(:,:,iz,1) = ninf + B0**2 / 2.0 / T0 / &
                            ( cosh((zgrid(iz) - zlow) / a0 ))**2
                    else 
                        uu(:,:,iz,5) = - B0 * tanh( (zgrid(iz) - zup )/a0 )
                        uu(:,:,iz,1) = ninf + B0**2 / 2.0 / T0 / &
                            ( cosh((zgrid(iz) - zup) / a0 ))**2
                    endif
                enddo

                uu(:,:,:,8) = uu(:,:,:,1) * T0

            Case(2) 
                !Add a double Harris Ux(y') and rho(y')
                ylow = 0.25 * Ly
                yup = 0.75 * Ly
                ycent = 0.5 * Ly 

                a0 = Ly * 0.5 * shear_width

                do iy = iymin,iymax 
                    if (ygrid(iy) < ycent ) then 
                        uu(:,iy,:,2) = 0.5*(U_fast-U_slow)*tanh( (ygrid(iy)-ylow) &
                            / a0) + 0.5*(U_fast+U_slow)
                        uu(:,iy,:,1) = 0.5*(n_fast-n_slow)*tanh( (ygrid(iy)-ylow) &
                            / a0) + 0.5*(n_fast+n_slow)
                    else 
                        uu(:,iy,:,2) = -0.5*(U_fast-U_slow)*tanh( (ygrid(iy)-yup) &
                            / a0) + 0.5*(U_fast+U_slow)
                        uu(:,iy,:,1) = -0.5*(n_fast-n_slow)*tanh( (ygrid(iy)-yup) &
                            / a0) + 0.5*(n_fast+n_slow)
                    endif
                enddo

                uu(:,:,:,8) = press0
                uu(:,:,:,5) = bx0 
                uu(:,:,:,6) = by0
                
            Case(3)
                !add uniform background magnetic field
                uu(:,:,:,5) = bx0 
                uu(:,:,:,6) = by0 
                uu(:,:,:,7) = bz0
                uu(:,:,:,8) = press0
                
                p0 = press0

            Case(4)
                !Add a double current sheet B0 = B0x(y)
                !But do not use density to balance pressure, use temperature instead
                ylow = 0.25 * Ly 
                yup = 0.75 * Ly
                ycent = 0.5 * Ly 

                do iy = iymin,iymax 
                    if (ygrid(iy) < ycent ) then 
                        uu(:,iy,:,5) = B0 * tanh( (ygrid(iy) - ylow)/a0 ) 
                    else 
                        uu(:,iy,:,5) = - B0 * tanh( (ygrid(iy) - yup )/a0 )
                    endif
                    uu(:,iy,:,8) = press0 - 0.5 * (uu(:,iy,:,5))**2
                    uu(:,iy,:,1) = ninf
                enddo

            case default
                continue
            end select

        end subroutine background_fields_initialize


        subroutine perturbation_initialize
            implicit none
            real :: kx, ky, kz
            integer :: ix, iy, iz, ivar, ik
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax
            real,allocatable,dimension(:) :: phs,phs1,phs2
            integer :: ir,ikx,iky,ikz
            integer,dimension(12) :: ir_arr
            real :: amp_x, amp_y, amp_z
            real,dimension(3) :: direction_perp
            real :: kmod, B0mod, k_radius
            real :: pph, modulation_y, modulation_y_deriv, ylow,ycent,yup
            real :: ph_z, ph_y, ph_x, ph0, ph1, ph2, correlation_vb = 0
            real :: p_SB, q_SB, dpdr_SB, Bz0_SB,x_SB,y_SB,z_SB,r_SB,&
                    Bx_SB,By_SB,Bz_SB,Br_SB,Bphi_SB,C_SB,phi_SB

            ixmin = 1
            ixmax = nx 
            iymin = yi_offset(myid_i+1) + 1
            iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
            izmin = zj_offset(myid_j+1) + 1
            izmax = zj_offset(myid_j+1) + zj_size(myid_j+1)

            select case(ipert)

            case(1)
                !circularly polarized alfven wave
                kx =  2*pi/Lx
                do ix=ixmin,ixmax 
                    !z component
                    uu(ix,:,:,7) = uu(ix,:,:,7) - db0 * sqrt(uu(ix,:,:,1)) &
                        * sin(kx * xgrid(ix))
                    uu(ix,:,:,4) = uu(ix,:,:,4) + db0 * sin(kx * xgrid(ix))
                    !x&y component
                    uu(ix,:,:,2) = uu(ix,:,:,2) + db0 * cos(kx * xgrid(ix)) * sin_cor_ang
                    uu(ix,:,:,5) = uu(ix,:,:,5) - db0 * sqrt(uu(ix,:,:,1)) &
                        * cos(kx * xgrid(ix)) * sin_cor_ang

                    uu(ix,:,:,3) = uu(ix,:,:,3) + db0 * cos(kx * xgrid(ix)) * cos_cor_ang
                    uu(ix,:,:,6) = uu(ix,:,:,6) - db0 * sqrt(uu(ix,:,:,1)) &
                        * cos(kx * xgrid(ix)) * cos_cor_ang
                enddo

            case(2)
                !add rondom perturbations along x in By and Bz
                allocate(phs(nmode))
                call random_number(phs)

                phs(:) = phs(:) * 2 * pi

                do ik = 1,nmode 
                    kx =  ik*2*pi/Lx
                    do ix=ixmin,ixmax 
                        pph = kx * xgrid(ix) + phs(ik)
                        !z component
                        uu(ix,:,:,7) = uu(ix,:,:,7) + db0 * sin(pph)
                        uu(ix,:,:,4) = uu(ix,:,:,4) - db0 * sin(pph)
                        ! !y component
                        ! uu(ix,:,:,6) = uu(ix,:,:,6) + db0 * cos(pph) 
                    enddo
                enddo


            case(3)
                !add rondom perturbations along y in Bz
                allocate(phs(nmode))
                call random_number(phs)

                phs(:) = phs(:) * 2 * pi

                do ik = 1,nmode 
                    ky =  ik*2*pi/Ly
                    do iy=iymin,iymax 
                        pph = ky * ygrid(iy) + phs(ik)
                        !z component
                        uu(:,iy,:,4) = uu(:,iy,:,4) - db0 * sin(pph)
                        uu(:,iy,:,7) = uu(:,iy,:,7) + db0 * sin(pph)
                    enddo
                enddo

            case(4)
                !add rondom perturbations along z in Bx
                allocate(phs(nmode))
                call random_number(phs)

                phs(:) = phs(:) * 2 * pi

                do ik = 1,nmode 
                    kz =  ik*2*pi/Lz
                    do iz=izmin,izmax 
                        pph = kz * zgrid(iz) + phs(ik)
                        !x component
                        uu(:,:,iz,2) = uu(:,:,iz,2) - db0 * sin(pph)
                        uu(:,:,iz,5) = uu(:,:,iz,5) + db0 * sin(pph)
                    enddo
                enddo

            Case(5)
                !3D perturbations, for tearing
                ylow = 0.25 * Ly 
                yup = 0.75 * Ly
                ycent = 0.5 * Ly 

                nmode = nmodex * (nmodez+1)
                allocate(phs(nmode))


                !lower current sheet
                ir = 1
                call random_seed(ir)
                call random_number(phs)
                phs(:) = phs(:) * 2 * pi 

                do ikx = 1, nmodex 
                    kx = ikx * 2 * pi / Lx 
                    do ikz = 0, nmodez 
                        kz = ikz * 2 * pi / Lz 

                        do iz = izmin,izmax
                            do iy = iymin,iymax 
                                if (ygrid(iy)<=ycent) then
                                    modulation_y = a0 * exp( - ((ygrid(iy)-ylow)/a0)**2  )
                                    modulation_y_deriv = -2 * (ygrid(iy)-ylow)/a0 * &
                                        exp( - ((ygrid(iy)-ylow)/a0)**2  )

                                    do ix= ixmin, ixmax
                                        pph = kx * xgrid(ix) + kz*zgrid(iz) + &
                                            phs( (ikx-1)*nmodez + ikz  )
                                        
                                        !bx
                                        uu(ix,iy,iz,5) = uu(ix,iy,iz,5) + &
                                            dbx * cos(pph) * modulation_y_deriv
                                        !bz
                                        uu(ix,iy,iz,7) = uu(ix,iy,iz,7) + &
                                            dbz * cos(pph) * modulation_y_deriv 
                                        
                                        !by
                                        uu(ix,iy,iz,6) = uu(ix,iy,iz,6) + &
                                            (kx*dbx+kz*dbz) * sin(pph) * &
                                            modulation_y
                                    enddo
                                endif
                            enddo
                        enddo
                    enddo
                enddo


                !upper current sheet
                ir = 16
                call random_seed(ir)
                call random_number(phs)
                phs(:) = phs(:) * 2 * pi 

                do ikx = 1, nmodex 
                    kx = ikx * 2 * pi / Lx 
                    do ikz = 0, nmodez 
                        kz = ikz * 2 * pi / Lz 

                        do iz = izmin,izmax
                            do iy = iymin,iymax 
                                if (ygrid(iy)>ycent) then
                                    modulation_y = a0 * exp( - ((ygrid(iy)-yup)/a0)**2  )
                                    modulation_y_deriv = -2 * (ygrid(iy)-yup)/a0 * &
                                        exp( - ((ygrid(iy)-yup)/a0)**2  )

                                    do ix= ixmin, ixmax
                                        pph = kx * xgrid(ix) + kz*zgrid(iz) + &
                                            phs( (ikx-1)*nmodez + ikz  )
                                        
                                        !bx
                                        uu(ix,iy,iz,5) = uu(ix,iy,iz,5) + &
                                            dbx * cos(pph) * modulation_y_deriv
                                        !bz
                                        uu(ix,iy,iz,7) = uu(ix,iy,iz,7) + &
                                            dbz * cos(pph) * modulation_y_deriv 
                                        
                                        !by
                                        uu(ix,iy,iz,6) = uu(ix,iy,iz,6) + &
                                            (kx*dbx+kz*dbz) * sin(pph) * &
                                            modulation_y
                                    enddo
                                endif
                            enddo
                        enddo
                    enddo
                enddo
        

            Case(6)
                !3D perturbations for turbulence simulation
                !assume B0 along x 
                nmode = nmodex * (2 * nmodez+1)  * (2 * nmodey+1)
                allocate(phs(nmode))


                ! outward wave
                ir = 1
                call random_seed(ir)
                call random_number(phs)
                phs(:) = phs(:) * 2 * pi 

                do ikx=1,nmodex 
                    kx = ikx * 2 * pi / Lx
                    amp_x = sqrt(1/real(abs(ikx)))

                    do iky=-nmodey,nmodey 
                        ky = iky * 2 * pi / Ly 
                        if (iky .eq. 0) then
                            amp_y = 1.0
                        else
                            amp_y = sqrt(1/real(abs(iky)))
                        endif 

                        do ikz=-nmodez,nmodez 
                            kz = ikz * 2 * pi / Lz 
                            if (ikz .eq. 0) then 
                                amp_z = 1.0 
                            else 
                                amp_z = sqrt(1/real(abs(ikz)))
                            endif

                            do iz = izmin, izmax 
                                do iy = iymin, iymax
                                    do ix = ixmin, ixmax
                                        pph = kx*xgrid(ix) + ky*ygrid(iy) + &
                                            kz*zgrid(iz) + phs( 1 + &
                                            ((ikx-1) * (2 * nmodey+1) + iky + nmodey) &
                                            * (2 * nmodez + 1) + ikz + nmodez)

                                        if ((iky .eq. 0) .and. (ikz .eq. 0)) then 
                                            uu(ix,iy,iz,6) = uu(ix,iy,iz,6) + db0 * cos(pph) &
                                            * amp_x * amp_y * amp_z

                                            uu(ix,iy,iz,7) = uu(ix,iy,iz,7) + db0 * sin(pph) &
                                            * amp_x * amp_y * amp_z

                                            uu(ix,iy,iz,3) = uu(ix,iy,iz,3) - sign(1.0,bx0) * db0 * cos(pph) &
                                            * amp_x * amp_y * amp_z

                                            uu(ix,iy,iz,4) = uu(ix,iy,iz,4) - sign(1.0,bx0) * db0 * sin(pph) &
                                            * amp_x * amp_y * amp_z
                                        else 
                                            dby = db0 * kz/sqrt(ky**2 + kz**2) 
                                            dbz = -db0 * ky/sqrt(ky**2 + kz**2)

                                            uu(ix,iy,iz,6) = uu(ix,iy,iz,6) + dby * cos(pph) &
                                            * amp_x * amp_y * amp_z

                                            uu(ix,iy,iz,7) = uu(ix,iy,iz,7) + dbz * cos(pph) &
                                            * amp_x * amp_y * amp_z

                                            uu(ix,iy,iz,3) = uu(ix,iy,iz,3) - sign(1.0,bx0) * dby * cos(pph) &
                                            * amp_x * amp_y * amp_z

                                            uu(ix,iy,iz,4) = uu(ix,iy,iz,4) - sign(1.0,bx0) * dbz * cos(pph) &
                                            * amp_x * amp_y * amp_z
                                        endif
                                    enddo
                                enddo
                            enddo
                        enddo
                    enddo
                enddo



                ! inward wave
                ir = 16
                call random_seed(ir)
                call random_number(phs)
                phs(:) = phs(:) * 2 * pi 

                do ikx=1,nmodex 
                    kx = -ikx * 2 * pi / Lx
                    amp_x = sqrt(1/real(abs(ikx)))

                    do iky=-nmodey,nmodey 
                        ky = iky * 2 * pi / Ly 
                        if (iky .eq. 0) then
                            amp_y = 1.0
                        else
                            amp_y = sqrt(1/real(abs(iky)))
                        endif 

                        do ikz=-nmodez,nmodez 
                            kz = ikz * 2 * pi / Lz 
                            if (ikz .eq. 0) then 
                                amp_z = 1.0 
                            else 
                                amp_z = sqrt(1/real(abs(ikz)))
                            endif

                            do iz = izmin, izmax 
                                do iy = iymin, iymax
                                    do ix = ixmin, ixmax
                                        pph = kx*xgrid(ix) + ky*ygrid(iy) + &
                                            kz*zgrid(iz) + phs( 1 + &
                                            ((ikx-1) * (2 * nmodey+1) + iky + nmodey) &
                                            * (2 * nmodez + 1) + ikz + nmodez)

                                        if ((iky .eq. 0) .and. (ikz .eq. 0)) then 
                                            uu(ix,iy,iz,6) = uu(ix,iy,iz,6) + db0 * cos(pph) &
                                            * amp_x * amp_y * amp_z * in_out_ratio

                                            uu(ix,iy,iz,7) = uu(ix,iy,iz,7) + db0 * sin(pph) &
                                            * amp_x * amp_y * amp_z * in_out_ratio

                                            uu(ix,iy,iz,3) = uu(ix,iy,iz,3) + sign(1.0,bx0) * db0 * cos(pph) &
                                            * amp_x * amp_y * amp_z * in_out_ratio

                                            uu(ix,iy,iz,4) = uu(ix,iy,iz,4) + sign(1.0,bx0) * db0 * sin(pph) &
                                            * amp_x * amp_y * amp_z * in_out_ratio
                                        else 
                                            dby = db0 * kz/sqrt(ky**2 + kz**2) 
                                            dbz = -db0 * ky/sqrt(ky**2 + kz**2)

                                            uu(ix,iy,iz,6) = uu(ix,iy,iz,6) + dby * cos(pph) &
                                            * amp_x * amp_y * amp_z * in_out_ratio

                                            uu(ix,iy,iz,7) = uu(ix,iy,iz,7) + dbz * cos(pph) &
                                            * amp_x * amp_y * amp_z * in_out_ratio

                                            uu(ix,iy,iz,3) = uu(ix,iy,iz,3) + sign(1.0,bx0) * dby * cos(pph) &
                                            * amp_x * amp_y * amp_z * in_out_ratio

                                            uu(ix,iy,iz,4) = uu(ix,iy,iz,4) + sign(1.0,bx0) * dbz * cos(pph) &
                                            * amp_x * amp_y * amp_z * in_out_ratio
                                        endif
                                    enddo
                                enddo
                            enddo

                        enddo
                    enddo
                enddo


                ! !kx =0 modes
                ! deallocate(phs)
                ! allocate(phs((2*nmodey + 1) * (2*nmodez + 1)))

                ! ir = 32
                ! call random_seed(ir)
                ! call random_number(phs)
                ! phs(:) = phs(:) * 2 * pi 

                ! kx = 0.0
                ! amp_x = 1.0

                ! do iky=-nmodey,nmodey 
                !     ky = iky * 2 * pi / Ly 
                !     if (iky .eq. 0) then
                !         amp_y = 1.0
                !     else
                !         amp_y = sqrt(1/real(abs(iky)))
                !     endif 

                !     do ikz=-nmodez,nmodez 
                !         kz = ikz * 2 * pi / Lz 
                !         if (ikz .eq. 0) then 
                !             amp_z = 1.0 
                !         else 
                !             amp_z = sqrt(1/real(abs(ikz)))
                !         endif

                !         do iz = izmin, izmax 
                !             do iy = iymin, iymax
                !                 do ix = ixmin, ixmax
                !                     pph = kx*xgrid(ix) + ky*ygrid(iy) + &
                !                         kz*zgrid(iz) + phs( 1 + &
                !                         (iky + nmodey) * (2 * nmodez + 1) + ikz + nmodez)

                !                     if ((iky .eq. 0) .and. (ikz .eq. 0)) then 
                !                         cycle
                !                     else 
                !                         dby = db0 * kz/sqrt(ky**2 + kz**2) 
                !                         dbz = -db0 * ky/sqrt(ky**2 + kz**2)

                !                         uu(ix,iy,iz,6) = uu(ix,iy,iz,6) + dby * cos(pph) * amp_x * amp_y * amp_z * in_out_ratio

                !                         uu(ix,iy,iz,7) = uu(ix,iy,iz,7) + dbz * cos(pph) * amp_x * amp_y * amp_z * in_out_ratio

                !                         uu(ix,iy,iz,3) = uu(ix,iy,iz,3) + sign(1.0,bx0) * dby * cos(pph) * amp_x * amp_y * amp_z * in_out_ratio

                !                         uu(ix,iy,iz,4) = uu(ix,iy,iz,4) + sign(1.0,bx0) * dbz * cos(pph) * amp_x * amp_y * amp_z * in_out_ratio
                !                     endif
                !                 enddo
                !             enddo
                !         enddo

                !     enddo
                ! enddo
            



            Case(7)
                !3D perturbations for turbulence simulation
                ! random perturbations in U, B, and rho


                correlation_vb = -0.05

                nmode = (2 * nmodex + 1) * (2 * nmodez+1)  * (2 * nmodey+1)
                allocate(phs(nmode),phs1(nmode),phs2(nmode))

                ir = 7 ! 1
                do ix=1,12
                    ir_arr(ix) = ir + 100
                enddo
                call random_seed(PUT=ir_arr)
                call random_number(phs)
                phs(:) = phs(:) * 2 * pi 

                ir = 99 ! 16
                do ix=1,12
                    ir_arr(ix) = ir + 100
                enddo
                call random_seed(PUT=ir_arr)
                call random_number(phs1)
                phs1(:) = phs1(:) * 2 * pi 

                ir = 53 ! 32
                do ix=1,12
                    ir_arr(ix) = ir + 100
                enddo
                call random_seed(PUT=ir_arr)
                call random_number(phs2)
                phs2(:) = phs2(:) * 2 * pi 

                do ikx=0,nmodex 
                    kx = ikx * 2 * pi / Lx

                    do iky=-nmodey,nmodey 
                        ky = iky * 2 * pi / Ly 

                        do ikz=-nmodez,nmodez 
                            kz = ikz * 2 * pi / Lz 

                            if ((ikx==0 .and. iky<0) .or. (ikx==0 .and. iky==0 .and. ikz<=0)) then 
                                cycle 
                            endif

                            k_radius = sqrt(real(ikx**2 + iky**2 + ikz**2))

                            ! istropic condition
                            if (k_radius>max(nmodex,nmodey,nmodez)) then 
                                cycle
                            endif 

                            ! calculate direction perp. to k and B
                            kmod = sqrt(kx**2 + ky**2 + kz**2)
                            B0mod = sqrt(bx0**2 + by0**2 + bz0**2)
                            if ((kmod .eq. 0) .or. (B0mod .eq. 0)) then 
                                write(*,*) 'When intializing perturbation, |k|=0 or |B0|=0!!!'
                                call Exit(0)
                            endif

                            direction_perp(1) = (ky * bz0 - kz * by0)/kmod/B0mod 
                            direction_perp(2) = (kz * bx0 - kx * bz0)/kmod/B0mod
                            direction_perp(3) = (kx * by0 - ky * bx0)/kmod/B0mod

                            ph0 = phs( 1 + ((ikx + nmodex) * (2 * nmodey+1) + iky + nmodey) &
                                * (2 * nmodez + 1) + ikz + nmodez)

                            ph1 = phs1( 1 + ((ikx + nmodex) * (2 * nmodey+1) + iky + nmodey) &
                                * (2 * nmodez + 1) + ikz + nmodez)

                            ph2 = phs2( 1 + ((ikx + nmodex) * (2 * nmodey+1) + iky + nmodey) &
                                * (2 * nmodez + 1) + ikz + nmodez)

                            do iz = izmin, izmax 
                                ph_z = kz*zgrid(iz)
                                do iy = iymin, iymax
                                    ph_y = ky*ygrid(iy) 
                                    do ix = ixmin, ixmax
                                        ph_x = kx*xgrid(ix)
                                        !velocity -- uncorrelated with b
                                        pph = ph_x + ph_y + ph_z + ph0
        
                                        uu(ix,iy,iz,2) = uu(ix,iy,iz,2) + &
                                            sqrt(1-correlation_vb**2) * dv0 * cos(pph) &
                                            / sqrt(k_radius**3) * direction_perp(1)

                                        uu(ix,iy,iz,3) = uu(ix,iy,iz,3) + &
                                            sqrt(1-correlation_vb**2) * dv0 * cos(pph) &
                                            / sqrt(k_radius**3) * direction_perp(2)

                                        uu(ix,iy,iz,4) = uu(ix,iy,iz,4) + &
                                            sqrt(1-correlation_vb**2) * dv0 * cos(pph) &
                                            / sqrt(k_radius**3) * direction_perp(3)

                                        ! magnetic field
                                        pph = ph_x + ph_y + ph_z + ph1

                                        uu(ix,iy,iz,5) = uu(ix,iy,iz,5) + db0 * cos(pph) &
                                            /sqrt(k_radius**3) * direction_perp(1)

                                        uu(ix,iy,iz,6) = uu(ix,iy,iz,6) + db0 * cos(pph) &
                                            /sqrt(k_radius**3) * direction_perp(2)

                                        uu(ix,iy,iz,7) = uu(ix,iy,iz,7) + db0 * cos(pph) &
                                            /sqrt(k_radius**3) * direction_perp(3)

                                        ! velocity correlated

                                        uu(ix,iy,iz,2) = uu(ix,iy,iz,2) + &
                                            correlation_vb * dv0 * cos(pph) &
                                            / sqrt(k_radius**3) * direction_perp(1)

                                        uu(ix,iy,iz,3) = uu(ix,iy,iz,3) + &
                                            correlation_vb * dv0 * cos(pph) &
                                            / sqrt(k_radius**3) * direction_perp(2)

                                        uu(ix,iy,iz,4) = uu(ix,iy,iz,4) + &
                                            correlation_vb * dv0 * cos(pph) &
                                            / sqrt(k_radius**3) * direction_perp(3)


                                        ! density
                                        pph = ph_x + ph_y + ph_z + ph2

                                        uu(ix,iy,iz,1) = uu(ix,iy,iz,1) + drho0 * cos(pph) &
                                            / sqrt(k_radius**3)
                                    enddo
                                enddo
                            enddo
                        enddo
                    enddo
                enddo


                
                ! ! magnetic field
                ! ir = 16
                ! call random_seed(ir)
                ! call random_number(phs)
                ! phs(:) = phs(:) * 2 * pi 

                ! do ikx=-nmodex,nmodex 
                !     kx = ikx * 2 * pi / Lx

                !     do iky=-nmodey,nmodey 
                !         ky = iky * 2 * pi / Ly 

                !         do ikz=-nmodez,nmodez 
                !             kz = ikz * 2 * pi / Lz 

                !             if ( (ikx .eq. 0) .and. (iky .eq. 0) .and. (ikz .eq. 0)) then 
                !                 cycle
                !             endif

                !             k_radius = sqrt(real(ikx**2 + iky**2 + ikz**2))

                !             ! calculate direction perp. to k and B
                !             kmod = sqrt(kx**2 + ky**2 + kz**2)
                !             B0mod = sqrt(bx0**2 + by0**2 + bz0**2)
                !             if ((kmod .eq. 0) .or. (B0mod .eq. 0)) then 
                !                 write(*,*) 'When intializing perturbation, |k|=0 or |B0|=0!!!'
                !                 call Exit(0)
                !             endif

                !             direction_perp(1) = (ky * bz0 - kz * by0)/kmod/B0mod 
                !             direction_perp(2) = (kz * bx0 - kx * bz0)/kmod/B0mod
                !             direction_perp(3) = (kx * by0 - ky * bx0)/kmod/B0mod


                !             do iz = izmin, izmax 
                !                 do iy = iymin, iymax
                !                     do ix = ixmin, ixmax
                !                         pph = kx*xgrid(ix) + ky*ygrid(iy) + &
                !                             kz*zgrid(iz) + phs( 1 + &
                !                             ((ikx + nmodex) * (2 * nmodey+1) + iky + nmodey) &
                !                             * (2 * nmodez + 1) + ikz + nmodez)

        
                !                         uu(ix,iy,iz,5) = uu(ix,iy,iz,5) + db0 * cos(pph) &
                !                             /k_radius * direction_perp(1)

                !                         uu(ix,iy,iz,6) = uu(ix,iy,iz,6) + db0 * cos(pph) &
                !                             /k_radius * direction_perp(2)

                !                         uu(ix,iy,iz,7) = uu(ix,iy,iz,7) + db0 * cos(pph) &
                !                             /k_radius * direction_perp(3)
                !                     enddo
                !                 enddo
                !             enddo
                !         enddo
                !     enddo
                ! enddo


                ! ! density
                ! ir = 32
                ! call random_seed(ir)
                ! call random_number(phs)
                ! phs(:) = phs(:) * 2 * pi 

                ! do ikx=-nmodex,nmodex 
                !     kx = ikx * 2 * pi / Lx

                !     do iky=-nmodey,nmodey 
                !         ky = iky * 2 * pi / Ly 

                !         do ikz=-nmodez,nmodez 
                !             kz = ikz * 2 * pi / Lz 

                !             if ( (ikx .eq. 0) .and. (iky .eq. 0) .and. (ikz .eq. 0)) then 
                !                 cycle
                !             endif

                !             k_radius = sqrt(real(ikx**2 + iky**2 + ikz**2))

                !             do iz = izmin, izmax 
                !                 do iy = iymin, iymax
                !                     do ix = ixmin, ixmax
                !                         pph = kx*xgrid(ix) + ky*ygrid(iy) + &
                !                             kz*zgrid(iz) + phs( 1 + &
                !                             ((ikx + nmodex) * (2 * nmodey+1) + iky + nmodey) &
                !                             * (2 * nmodez + 1) + ikz + nmodez)

                !                         uu(ix,iy,iz,1) = uu(ix,iy,iz,1) + drho0 * cos(pph) &
                !                             / k_radius
                !                     enddo
                !                 enddo
                !             enddo
                !         enddo
                !     enddo
                ! enddo

            case(8)
                ! add a switchback ---------------
                ! DO NOT set background magnetic field!

                ! core part----
                do iz=izmin,izmax 
                    z_SB = zgrid(iz) - Lz/2
                    do iy = iymin,iymax
                        y_SB = ygrid(iy) - Ly/2

                        r_SB = SQRT(y_SB**2 + z_SB**2)

                        do ix = ixmin,ixmax  
                            x_SB = xgrid(ix) - Lx/2
                            
                            ! note that here Bz is actually Bx
                            Bz0_SB = B0_SB + dB_SB * exp(-((r_SB-R0_SB)/R1_SB)**2)
                            
                            Bz_SB = B0_SB + dB_SB * exp(-((r_SB-R0_SB)/R1_SB)**2 - (x_SB/H_SB)**2)
                            C_SB = -(exp(-(R0_SB/R1_SB)**2) + sqrt(PI)*R0_SB/R1_SB*erf(R0_SB/R1_SB))
                            
                            if (r_SB < 1E-8) then
                                phi_SB = 0.
                                Br_SB = 0.
                                Bphi_SB = 0.0
                            else 
                                phi_SB = ATAN2(z_SB,y_SB)
                                Br_SB = -dB_SB * (x_SB * R1_SB**2)/(r_SB * H_SB**2) * &
                                    exp(-(x_SB/H_SB)**2) * (exp(-((r_SB-R0_SB)/R1_SB)**2) + &
                                    sqrt(pi)*R0_SB/R1_SB * erf((R0_SB-r_SB)/R1_SB) + C_SB)
                                

                                ! correction to fix the boundary jump
                                if (r_SB<Rm_SB) then 
                                    p_SB = 1
                                    dpdr_SB = 0
                                    q_SB = 1
                                else
                                    p_SB = exp(-((r_SB-Rm_SB)/Rd_SB)**2)
                                    dpdr_SB = -2*(r_SB-Rm_SB)/Rd_SB**2 * p_SB

                                    q_SB = Bz0_SB/Bz_SB * (1 + 1/Bz0_SB * (p_SB * (Bz_SB-Bz0_SB) - &
                                    (dpdr_SB * dB_SB * R1_SB**2/r_SB * (exp(-((r_SB-R0_SB)/R1_SB)**2) + &
                                        sqrt(pi)*R0_SB/R1_SB*erf((R0_SB-r_SB)/R1_SB) + C_SB) &
                                        * 0.5 * (exp(-(x_SB/H_SB)**2)-1))  ))
                                endif 

                                
                                Br_SB = Br_SB * p_SB
                                Bz_SB = Bz_SB * q_SB 


                                IF (Bz_SB**2 + Br_SB**2 < B0_SB**2) THEN
                                    Bphi_SB = sqrt(B0_SB**2 - Br_SB**2 - Bz_SB**2)
                                else
                                    Bphi_SB = 0.0
                                    ! need to modify pressure to maintain constant Ptot
                                    uu(ix,iy,iz,8) = uu(ix,iy,iz,8) - 0.5*(Bz_SB**2 + Br_SB**2 - B0_SB**2)
                                ENDIF 
                            endif 
                            
                            Bx_SB = Br_SB * cos(phi_SB) - Bphi_SB * sin(phi_SB)
                            By_SB = Br_SB * sin(phi_SB) + Bphi_SB * cos(phi_SB)


                            ! note: we have changed the order of variables!!!
                            ! Bz_SB should be Bx
                            ! Bx_SB should be By
                            ! By_SB should be Bz
                            uu(ix,iy,iz,5) = uu(ix,iy,iz,5) + Bz_SB 
                            uu(ix,iy,iz,6) = uu(ix,iy,iz,6) + Bx_SB 
                            uu(ix,iy,iz,7) = uu(ix,iy,iz,7) + By_SB 
                        enddo
                    enddo
                enddo

                

                ! add anti-correlated velocity fluctuation
                do iz=izmin,izmax 
                    do iy = iymin,iymax
                        do ix = ixmin,ixmax  
                            uu(ix,iy,iz,2) = uu(ix,iy,iz,2) - uu(ix,iy,iz,5)/SQRT(uu(ix,iy,iz,1))
                            uu(ix,iy,iz,3) = uu(ix,iy,iz,3) - uu(ix,iy,iz,6)/SQRT(uu(ix,iy,iz,1))
                            uu(ix,iy,iz,4) = uu(ix,iy,iz,4) - uu(ix,iy,iz,7)/SQRT(uu(ix,iy,iz,1))
                        enddo
                    enddo
                enddo


            case(999)
                !debug
                do iz = izmin, izmax 
                    do iy = iymin, iymax
                        do ix = ixmin, ixmax
                            uu(ix,iy,iz,:) = sqrt((ygrid(iy)-Ly/2.0)**2 &
                                + (zgrid(iz)-Lz/2.0)**2)
                        enddo
                    enddo
                enddo
            case default
                continue 
            end select


        end subroutine perturbation_initialize

        subroutine initial_calc_conserve_variable
            !after initializing background fields and the perturbation
            !transform uu to be conserved quantities
            implicit none 

            uu_prim(:,:,:,1:3) = uu(:,:,:,2:4) !ux,uy,uz

            !rho * u
            uu(:,:,:,2) = uu(:,:,:,1) * uu_prim(:,:,:,1)
            uu(:,:,:,3) = uu(:,:,:,1) * uu_prim(:,:,:,2)
            uu(:,:,:,4) = uu(:,:,:,1) * uu_prim(:,:,:,3)
        end subroutine 

end module mhdinit
