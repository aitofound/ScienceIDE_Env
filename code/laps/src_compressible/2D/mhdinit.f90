module mhdinit
    use parallel
    implicit none

    integer :: nx = 128, ny = 128, nz = 1, nvar = 8, nextern = 1

    real :: pi = 3.141592653589793

    integer :: ifield = 0, ipert = 0

    real :: Lx, Ly, dx, dy

    real :: T0 = 1.0, rho0 = 1.0  ! initial uniform temerature & rho 
    real :: B0 = 1.0, a0 = 0.05 !for current sheet

    real, allocatable, dimension(:) :: xgrid, ygrid

    !logical :: if_isothermal = .false.

    real :: adiabatic_index = 5./3. !, T_isothermal = 1.0
    logical :: if_resis = .false., if_AEB = .false.,if_corotating = .false.,&
        if_Hall = .false., if_resis_exp = .false., if_conserve_background = .false.,&
        if_visc = .false., if_visc_exp = .false., if_z_radial = .false.,&
        if_external_force = .false.
    real :: resistivity = 0.0, viscosity = 0.0, ion_inertial_length
    real :: corotating_angle = 0.0, cos_cor_ang, sin_cor_ang

    real :: time = 0.0

    !uu is the conservation variables:
    !rho, rho*(ux,uy,uz), bx,by,bz, e=(p/(k-1)+0.5*(rho*u^2+B^2))
    !note that in initiliazation, it is treated as primitive variables:
    !rho,ux,uy,uz,bx,by,bz,p
    real, allocatable, dimension(:,:,:,:) :: uu,uu_prim,flux,expand_term,current_density,&
        external_force
    complex, allocatable, dimension(:,:,:,:) :: uu_fourier,flux_fourier,expand_term_fourier,&
        current_density_fourier,external_force_fourier
    

    !arrays storing the wave number information
    real,allocatable,dimension(:) :: wave_number_x, wave_number_y, wave_number_z
    real,allocatable,dimension(:,:,:) :: k_square

    complex, allocatable, dimension(:,:,:,:) :: fnl, fnl_rk




    !user-defined----------------------
    real :: delta_b,db0,dv0,in_out_ratio = 1.0, initial_spectral_slope = 2.0, &
        delta_rho, correlation_vb = 0.0
    real :: U_fast,U_slow,n_fast,n_slow,press0,shear_width,bx0,by0,bz0
    real :: current_sheet_width = 0.075
    integer :: nmode = 16
    integer :: iMode = 128, mode_start = 64, mode_end = 512

    real :: jet_width=1.0, Vjet = 0.3
    real :: B0_SB,dB_SB,R1_SB,H_SB,Rm_SB,Rd_SB

    contains 

        subroutine grid_initialize
            ! must be called after parallel_initialize
            ! initialize grids and wave-numbers
            implicit none

            integer :: ix,iy,iz
            integer :: ixmin, ixmax, iymin, iymax, izmin, izmax 

            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = 1
            iymax = ny

            izmin = 1 
            izmax = 1

            allocate(xgrid(nx), ygrid(ny), &
                wave_number_x(nx), wave_number_y(ny),wave_number_z(nz),&
                k_square(ixmin:ixmax,iymin:iymax,izmin:izmax))

            dx = Lx / nx 
            dy = Ly / ny 

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

            wave_number_z(1) = 0.0

            !define k_square
            do iz=izmin,izmax
                do iy=iymin,iymax 
                    do ix=ixmin,ixmax 
                        k_square(ix,iy,iz) = (wave_number_x(ix))**2 + &
                            (wave_number_y(iy))**2 
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

            izmin = 1 
            izmax = 1

            allocate(uu(ixmin:ixmax, iymin:iymax, izmin:izmax, 1:nvar), &
                uu_prim(ixmin:ixmax, iymin:iymax, izmin:izmax, 1:4), &
                flux(ixmin:ixmax, iymin:iymax, izmin:izmax, 1:18)) 

            if (if_AEB) then 
                allocate(expand_term(ixmin:ixmax, iymin:iymax, izmin:izmax,1))
            endif

            if (if_Hall) then 
                allocate(current_density(ixmin:ixmax, iymin:iymax, izmin:izmax,3))
            endif

            if (if_external_force) then 
                allocate(external_force(ixmin:ixmax, iymin:iymax, izmin:izmax,1:nextern))
            endif 
            !--------------------------------------------------


            !--------------------------------------------------
            !uu in fourier space
            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = 1
            iymax = ny

            izmin = 1 
            izmax = 1

            allocate(uu_fourier(ixmin:ixmax, iymin:iymax, izmin:izmax, 1:nvar),&
                flux_fourier(ixmin:ixmax, iymin:iymax, izmin:izmax, 1:18), &
                fnl(ixmin:ixmax, iymin:iymax, izmin:izmax, 1:nvar), &
                fnl_rk(ixmin:ixmax, iymin:iymax, izmin:izmax, 1:nvar))

            if (if_AEB) then 
                !allocate(uu_prim_fourier(ixmin:ixmax, iymin:iymax, izmin:izmax, 4:4))
                allocate(expand_term_fourier(ixmin:ixmax, iymin:iymax, izmin:izmax, 1))
            endif

            if (if_Hall) then 
                allocate(current_density_fourier(ixmin:ixmax, iymin:iymax, izmin:izmax, 3))
            endif

            if (if_external_force) then 
                allocate(external_force_fourier(ixmin:ixmax, iymin:iymax, izmin:izmax,1:nextern))
            endif 
            !--------------------------------------------------

        end subroutine arrays_initialize

        subroutine background_fields_initialize
            implicit none 
            real :: kx, ky, kz
            integer :: ix, iy, iz, ivar, ik, &
                ixmin,ixmax,iymin,iymax,izmin,izmax
            real :: ylow,ycent,yup,ninf = 1.0, a_cs 
            real :: ylow0, ylow1, yup0, yup1, ycent0, ycent1, y_cent
            real :: r_yz

            ixmin = 1
            ixmax = nx 

            iymin = yi_offset(myid_i+1) + 1 
            iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)

            izmin = 1 
            izmax = 1

            uu(:,:,:,1) = rho0
            uu(:,:,:,8) = rho0*T0 !note this is pressure

            select case(ifield)

            Case(0)
                    !add uniform background magnetic field plus a jet
                    uu(:,:,:,1) = rho0

                    uu(:,:,:,5) = bx0 
                    uu(:,:,:,6) = by0 
                    uu(:,:,:,7) = bz0
                    
                    uu(:,:,:,8) = press0 
            
            Case(1)
                !Add a double current sheet B0 = B0x(y)
                ylow = 0.25 * Ly
                yup = 0.75 * Ly
                ycent = 0.5 * Ly 

                do iy = iymin,iymax 
                    if (ygrid(iy) < ycent ) then 
                        uu(:,iy,:,5) = B0 * tanh( (ygrid(iy) - ylow)/a0 ) 
                        uu(:,iy,:,1) = ninf + B0**2 / 2.0 / T0 / &
                            ( cosh((ygrid(iy) - ylow) / a0 ))**2
                    else 
                        uu(:,iy,:,5) = - B0 * tanh( (ygrid(iy) - yup )/a0 )
                        uu(:,iy,:,1) = ninf + B0**2 / 2.0 / T0 / &
                            ( cosh((ygrid(iy) - yup) / a0 ))**2
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
                !Add a double Harris Ux(y') and rho(y')
                !with a dip in B0 at the center of the slow stream
                ylow = 0.25 * Ly
                yup = 0.75 * Ly
                ycent = 0.5 * Ly 

                a0 = Ly * 0.5 * shear_width
                a_cs = Ly * 0.5 * current_sheet_width

                do iy = iymin,iymax 
                    if (ygrid(iy) < ycent ) then 
                        uu(:,iy,:,2) = 0.5*(U_fast-U_slow)*tanh( (ygrid(iy)-ylow) &
                            / a0) + 0.5*(U_fast+U_slow)
                        uu(:,iy,:,1) = 0.5*(n_fast-n_slow)*tanh( (ygrid(iy)-ylow) &
                            / a0) + 0.5*(n_fast+n_slow)

                        uu(:,iy,:,5) = B0 * (1 - 1/(cosh(ygrid(iy)/a_cs))**2) * cos_cor_ang
                        uu(:,iy,:,6) = -B0 * (1 - 1/(cosh(ygrid(iy)/a_cs))**2) * sin_cor_ang
                        uu(:,iy,:,7) = B0 * sqrt( 1- (1 - 1/(cosh(ygrid(iy)/a_cs))**2)**2)
                    else 
                        uu(:,iy,:,2) = -0.5*(U_fast-U_slow)*tanh( (ygrid(iy)-yup) &
                            / a0) + 0.5*(U_fast+U_slow)
                        uu(:,iy,:,1) = -0.5*(n_fast-n_slow)*tanh( (ygrid(iy)-yup) &
                            / a0) + 0.5*(n_fast+n_slow)

                        uu(:,iy,:,5) = B0 * (1 - 1/(cosh((ygrid(iy)-Ly)/a_cs))**2) * cos_cor_ang
                        uu(:,iy,:,6) = -B0 * (1 - 1/(cosh((ygrid(iy)-Ly)/a_cs))**2) * sin_cor_ang
                        uu(:,iy,:,7) = B0 * sqrt(1 - (1 - 1/(cosh((ygrid(iy)-Ly)/a_cs ))**2 )**2)
                    endif
                enddo

                uu(:,:,:,8) = press0
            
            Case(4)
                !add a double-(double-Harris) Ux(y') and rho(y')
                !plus a double-Harris B0(y') with current sheets 
                !in the center of the slow streams

                ylow0 = 0.25 * Ly * 0.5
                yup0 = 0.75 * Ly * 0.5
                ycent0 = 0.5 * Ly * 0.5

                ylow1 = 1.25 * Ly * 0.5 
                yup1 = 1.75 * Ly * 0.5
                ycent1 = 1.5 * Ly * 0.5

                y_cent = 0.5 * Ly

                a0 = Ly * 0.5 * shear_width * 0.5
                a_cs = Ly * 0.5 * current_sheet_width * 0.5

                do iy = iymin,iymax 
                    if (ygrid(iy) < y_cent) then 
                        if (ygrid(iy) < ycent0 ) then 
                            uu(:,iy,:,2) = 0.5*(U_fast-U_slow)*tanh( (ygrid(iy)-ylow0) &
                                / a0) + 0.5*(U_fast+U_slow)
                            uu(:,iy,:,1) = 0.5*(n_fast-n_slow)*tanh( (ygrid(iy)-ylow0) &
                                / a0) + 0.5*(n_fast+n_slow)

                            uu(:,iy,:,5) = B0 * tanh(ygrid(iy) / a_cs) * cos_cor_ang
                            uu(:,iy,:,6) = -B0 * tanh(ygrid(iy) / a_cs) * sin_cor_ang
                            uu(:,iy,:,7) = B0 * 1/cosh(ygrid(iy)/a_cs)
                        else 
                            uu(:,iy,:,2) = -0.5*(U_fast-U_slow)*tanh( (ygrid(iy)-yup0) &
                                / a0) + 0.5*(U_fast+U_slow)
                            uu(:,iy,:,1) = -0.5*(n_fast-n_slow)*tanh( (ygrid(iy)-yup0) &
                                / a0) + 0.5*(n_fast+n_slow)

                            uu(:,iy,:,5) = -B0 * tanh( (ygrid(iy)-y_cent) / a_cs) * cos_cor_ang
                            uu(:,iy,:,6) = B0 * tanh( (ygrid(iy)-y_cent) / a_cs) * sin_cor_ang
                            uu(:,iy,:,7) = -B0 * 1/cosh((ygrid(iy)-y_cent)/a_cs)
                        endif
                    else
                        if (ygrid(iy) < ycent1) then 
                            uu(:,iy,:,2) = 0.5*(U_fast-U_slow)*tanh( (ygrid(iy)-ylow1) &
                                / a0) + 0.5*(U_fast+U_slow)
                            uu(:,iy,:,1) = 0.5*(n_fast-n_slow)*tanh( (ygrid(iy)-ylow1) &
                                / a0) + 0.5*(n_fast+n_slow)

                            uu(:,iy,:,5) = -B0 * tanh( (ygrid(iy)-y_cent) / a_cs) * cos_cor_ang
                            uu(:,iy,:,6) = B0 * tanh( (ygrid(iy)-y_cent) / a_cs) * sin_cor_ang
                            uu(:,iy,:,7) = -B0 * 1/cosh((ygrid(iy)-y_cent)/a_cs)
                        else 
                            uu(:,iy,:,2) = -0.5*(U_fast-U_slow)*tanh( (ygrid(iy)-yup1) &
                                / a0) + 0.5*(U_fast+U_slow)
                            uu(:,iy,:,1) = -0.5*(n_fast-n_slow)*tanh( (ygrid(iy)-yup1) &
                                / a0) + 0.5*(n_fast+n_slow)

                            uu(:,iy,:,5) = B0 * tanh( (ygrid(iy)-Ly) / a_cs) * cos_cor_ang
                            uu(:,iy,:,6) = -B0 * tanh( (ygrid(iy)-Ly) / a_cs) * sin_cor_ang
                            uu(:,iy,:,7) = B0 * 1/cosh((ygrid(iy)-Ly)/a_cs)
                        endif
                    endif
                enddo

                uu(:,:,:,8) = press0


                Case(5)
                    !add uniform background magnetic field plus a jet
                    uu(:,:,:,5) = bx0 
                    uu(:,:,:,6) = by0 
                    uu(:,:,:,7) = bz0

                    uu(:,:,:,8) = press0 


                    do iy=iymin,iymax
                        do ix=ixmin,ixmax
                            ! uu(:,iy,iz,2) = Vjet * exp(- 0.5 *((zgrid(iz) - 0.5*Lz) &
                            !     /(jet_width))**2 - 0.5 * ((ygrid(iy) - 0.5*Ly) & 
                            !     /(jet_width))**2)

                            r_yz = sqrt((xgrid(ix)-Lx/2)**2 + (ygrid(iy)-Ly/2)**2)
                            uu(ix,iy,:,4) = Vjet/2 * (1 - tanh((r_yz - 1)/0.1)) 
                        enddo
                    enddo

                Case(6)
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

            real :: dbx, dby, dbz
            real,allocatable,dimension(:) :: phs, phs1, Br_sign
            integer,dimension(12) :: ir_arr 
            integer :: n_m,ir,ikx,iky
            real :: pph,pph1,kk,yup,ylow,B_sign
            real :: amplitude_slope_index,ik_slope

            real :: x_SB,y_SB,r_SB,Bx_SB,By_SB,Bz_SB,Br_SB,Bphi_SB,C_SB,phi_SB,&
                p_SB, dpdy_SB

            real :: dir_x, dir_y

            amplitude_slope_index = initial_spectral_slope/2.0

            ixmin = 1
            ixmax = nx 
            iymin = yi_offset(myid_i+1) + 1
            iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
            izmin = 1
            izmax = 1

            select case(ipert)

            case(1)
                !add monochromatic outward alfven wave
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
                !add a gaussian perturbation in Uz&Bz
                do iz = izmin,izmax
                    do iy = iymin,iymax
                        do ix = ixmin,ixmax 
                            uu(ix,iy,iz,4) = uu(ix,iy,iz,4) + db0 * exp( -( (xgrid(ix)-0.5*Lx) &
                                /(0.1*Lx) )**2 - ((ygrid(iy)-0.5*Ly)/(0.1*Ly))**2)
                            uu(ix,iy,iz,7) = uu(Ix,iy,iz,7) - db0/uu(ix,iy,iz,1) * exp( -( (xgrid(ix)-0.5*Lx) &
                                /(0.1*Lx) )**2 - ((ygrid(iy)-0.5*Ly)/(0.1*Ly))**2)
                        enddo
                    enddo
                enddo

            Case(3)
                !add pure outward wave band
                allocate(phs(nmode))
                call random_number(phs)

                phs(:) = phs(:) * 2 * pi

                do ik = 1,nmode 
                    kx =  ik*2*pi/Lx
                    ik_slope = ik**amplitude_slope_index
                    do ix=ixmin,ixmax 
                        pph = kx * xgrid(ix) + phs(ik)
                        !z component
                        uu(ix,:,:,7) = uu(ix,:,:,7) - db0/ik_slope * sqrt(uu(ix,:,:,1)) &
                            * sin(pph)
                        uu(ix,:,:,4) = uu(ix,:,:,4) + db0/ik_slope * sin(pph)
                        !x&y component
                        uu(ix,:,:,2) = uu(ix,:,:,2) + db0/ik_slope * cos(pph) * sin_cor_ang
                        uu(ix,:,:,5) = uu(ix,:,:,5) - db0/ik_slope * sqrt(uu(ix,:,:,1)) &
                            * cos(pph) * sin_cor_ang

                        uu(ix,:,:,3) = uu(ix,:,:,3) + db0/ik_slope * cos(pph) * cos_cor_ang
                        uu(ix,:,:,6) = uu(ix,:,:,6) - db0/ik_slope * sqrt(uu(ix,:,:,1)) &
                            * cos(pph) * cos_cor_ang
                    enddo
                enddo

            Case(4)
                !add outward+inward wave band
                allocate(phs(nmode))

                !outward------------
                ir = 1
                call random_seed(ir)
                call random_number(phs)

                phs(:) = phs(:) * 2 * pi

                do ik = 1,nmode 
                    kx =  ik*2*pi/Lx
                    ik_slope = ik**amplitude_slope_index
                    do ix=ixmin,ixmax 
                        pph = kx * xgrid(ix) + phs(ik)

                        ! ! Belowing is not good: div(B) /= 0.
                        ! ! Need to ensure that B perturbation is constant
                        ! !z component
                        ! uu(ix,:,:,7) = uu(ix,:,:,7) - db0/ik_slope * sqrt(uu(ix,:,:,1)) &
                        !     * sin(pph)
                        ! uu(ix,:,:,4) = uu(ix,:,:,4) + db0/ik_slope * sin(pph)
                        ! !x&y component
                        ! uu(ix,:,:,2) = uu(ix,:,:,2) + db0/ik_slope * cos(pph) * sin_cor_ang
                        ! uu(ix,:,:,5) = uu(ix,:,:,5) - db0/ik_slope * sqrt(uu(ix,:,:,1)) &
                        !     * cos(pph) * sin_cor_ang

                        ! uu(ix,:,:,3) = uu(ix,:,:,3) + db0/ik_slope * cos(pph) * cos_cor_ang
                        ! uu(ix,:,:,6) = uu(ix,:,:,6) - db0/ik_slope * sqrt(uu(ix,:,:,1)) &
                        !     * cos(pph) * cos_cor_ang

                        !z component
                        uu(ix,:,:,7) = uu(ix,:,:,7) - db0/ik_slope * sin(pph)
                        uu(ix,:,:,4) = uu(ix,:,:,4) + db0/sqrt(uu(ix,:,:,1)) &
                            /ik_slope * sin(pph)
                        !x&y component
                        uu(ix,:,:,2) = uu(ix,:,:,2) + db0/sqrt(uu(ix,:,:,1)) &
                            /ik_slope * cos(pph) * sin_cor_ang
                        uu(ix,:,:,5) = uu(ix,:,:,5) - db0/ik_slope &
                            * cos(pph) * sin_cor_ang

                        uu(ix,:,:,3) = uu(ix,:,:,3) + db0/sqrt(uu(ix,:,:,1)) &
                            /ik_slope * cos(pph) * cos_cor_ang
                        uu(ix,:,:,6) = uu(ix,:,:,6) - db0/ik_slope &
                            * cos(pph) * cos_cor_ang
                    enddo
                enddo


                !inward------------
                ir = 24
                call random_seed(ir)
                call random_number(phs)
                
                phs(:) = phs(:) * 2 * pi

                do ik = 1,nmode 
                    kx =  ik*2*pi/Lx
                    ik_slope = ik**amplitude_slope_index
                    do ix=ixmin,ixmax 
                        pph = kx * xgrid(ix) + phs(ik)
                        ! !z component
                        ! uu(ix,:,:,7) = uu(ix,:,:,7) + db0/ik_slope * sqrt(uu(ix,:,:,1)) &
                        !     * sin(pph) * in_out_ratio
                        ! uu(ix,:,:,4) = uu(ix,:,:,4) + db0/ik_slope * sin(pph) * in_out_ratio
                        ! !x&y component
                        ! uu(ix,:,:,2) = uu(ix,:,:,2) + db0/ik_slope * cos(pph) * sin_cor_ang &
                        !     * in_out_ratio
                        ! uu(ix,:,:,5) = uu(ix,:,:,5) + db0/ik_slope * sqrt(uu(ix,:,:,1)) &
                        !     * cos(pph) * sin_cor_ang * in_out_ratio

                        ! uu(ix,:,:,3) = uu(ix,:,:,3) + db0/ik_slope * cos(pph) * cos_cor_ang &
                        !     * in_out_ratio
                        ! uu(ix,:,:,6) = uu(ix,:,:,6) + db0/ik_slope * sqrt(uu(ix,:,:,1)) &
                        !     * cos(pph) * cos_cor_ang * in_out_ratio

                        !z component
                        uu(ix,:,:,7) = uu(ix,:,:,7) + db0/ik_slope * sin(pph) * in_out_ratio
                        uu(ix,:,:,4) = uu(ix,:,:,4) + db0/sqrt(uu(ix,:,:,1)) &
                            /ik_slope * sin(pph) * in_out_ratio
                        !x&y component
                        uu(ix,:,:,2) = uu(ix,:,:,2) + db0/sqrt(uu(ix,:,:,1)) &
                            /ik_slope * cos(pph) * sin_cor_ang * in_out_ratio
                        uu(ix,:,:,5) = uu(ix,:,:,5) + db0/ik_slope &
                            * cos(pph) * sin_cor_ang * in_out_ratio

                        uu(ix,:,:,3) = uu(ix,:,:,3) + db0/sqrt(uu(ix,:,:,1)) &
                            /ik_slope * cos(pph) * cos_cor_ang * in_out_ratio
                        uu(ix,:,:,6) = uu(ix,:,:,6) + db0/ik_slope * cos(pph) &
                            * cos_cor_ang * in_out_ratio
                    enddo
                enddo

            Case(5)
                !add pure inward wave band
                allocate(phs(nmode))
                call random_number(phs)

                phs(:) = phs(:) * 2 * pi

                do ik = 1,nmode 
                    kx =  ik*2*pi/Lx
                    ik_slope = ik**amplitude_slope_index
                    do ix=ixmin,ixmax 
                        pph = kx * xgrid(ix) + phs(ik)
                        !z component
                        uu(ix,:,:,7) = uu(ix,:,:,7) + db0/ik_slope * sqrt(uu(ix,:,:,1)) &
                            * sin(pph)
                        uu(ix,:,:,4) = uu(ix,:,:,4) + db0/ik_slope * sin(pph)
                        !x&y component
                        uu(ix,:,:,2) = uu(ix,:,:,2) + db0/ik_slope * cos(pph) * sin_cor_ang
                        uu(ix,:,:,5) = uu(ix,:,:,5) + db0/ik_slope * sqrt(uu(ix,:,:,1)) &
                            * cos(pph) * sin_cor_ang

                        uu(ix,:,:,3) = uu(ix,:,:,3) + db0/ik_slope * cos(pph) * cos_cor_ang
                        uu(ix,:,:,6) = uu(ix,:,:,6) + db0/ik_slope * sqrt(uu(ix,:,:,1)) &
                            * cos(pph) * cos_cor_ang
                    enddo
                enddo



            Case(6)
                !Similar to Case(4) but considers the polarity of the magnetic field
                !to be combined with Case(4) of background field

                !add outward+inward wave band
                allocate(phs(nmode),Br_sign(iymin:iymax))

                !determine the polarity of the radial magnetic field
                do iy=iymin,iymax 
                    Br_sign(iy) = sign(1.0, uu(1,iy,1,5))
                enddo


                !outward------------
                ir = 1
                call random_seed(ir)
                call random_number(phs)

                phs(:) = phs(:) * 2 * pi

                do ik = 1,nmode 
                    kx =  ik*2*pi/Lx
                    ik_slope = ik**amplitude_slope_index

                    do iy=iymin,iymax
                        B_sign = Br_sign(iy)
                        do ix=ixmin,ixmax 
                            pph = kx * xgrid(ix) + phs(ik)

                            !z component
                            uu(ix,iy,:,7) = uu(ix,iy,:,7) - db0/ik_slope * sin(pph)
                            uu(ix,iy,:,4) = uu(ix,iy,:,4) + db0/sqrt(uu(ix,iy,:,1)) &
                                /ik_slope * sin(pph) * B_sign
                            !x&y component
                            uu(ix,iy,:,2) = uu(ix,iy,:,2) + db0/sqrt(uu(ix,iy,:,1)) &
                                /ik_slope * cos(pph) * sin_cor_ang * B_sign
                            uu(ix,iy,:,5) = uu(ix,iy,:,5) - db0/ik_slope &
                                * cos(pph) * sin_cor_ang

                            uu(ix,iy,:,3) = uu(ix,iy,:,3) + db0/sqrt(uu(ix,iy,:,1)) &
                                /ik_slope * cos(pph) * cos_cor_ang * B_sign
                            uu(ix,iy,:,6) = uu(ix,iy,:,6) - db0/ik_slope &
                                * cos(pph) * cos_cor_ang
                        enddo
                    enddo
                enddo


                !inward------------
                ir = 24
                call random_seed(ir)
                call random_number(phs)
                
                phs(:) = phs(:) * 2 * pi
                do ik = 1,nmode 
                    kx =  ik*2*pi/Lx
                    ik_slope = ik**amplitude_slope_index

                    do iy = iymin,iymax
                        B_sign = Br_sign(iy)
                        do ix=ixmin,ixmax 
                            pph = kx * xgrid(ix) + phs(ik)

                            !z component
                            uu(ix,iy,:,7) = uu(ix,iy,:,7) + db0/ik_slope * sin(pph) * in_out_ratio
                            uu(ix,iy,:,4) = uu(ix,iy,:,4) + db0/sqrt(uu(ix,iy,:,1)) &
                                /ik_slope * sin(pph) * in_out_ratio * B_sign
                            !x&y component
                            uu(ix,iy,:,2) = uu(ix,iy,:,2) + db0/sqrt(uu(ix,iy,:,1)) &
                                /ik_slope * cos(pph) * sin_cor_ang * in_out_ratio * B_sign
                            uu(ix,iy,:,5) = uu(ix,iy,:,5) + db0/ik_slope &
                                * cos(pph) * sin_cor_ang * in_out_ratio

                            uu(ix,iy,:,3) = uu(ix,iy,:,3) + db0/sqrt(uu(ix,iy,:,1)) &
                                /ik_slope * cos(pph) * cos_cor_ang * in_out_ratio * B_sign
                            uu(ix,iy,:,6) = uu(ix,iy,:,6) + db0/ik_slope * cos(pph) &
                                * cos_cor_ang * in_out_ratio
                        enddo
                    enddo
                enddo

            case(7)
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
                        ! !y component
                        ! uu(ix,:,:,6) = uu(ix,:,:,6) + db0 * cos(pph) 
                    enddo
                enddo


            case(8)
                !add rondom perturbations along y in Bz
                allocate(phs(nmode))
                call random_number(phs)

                phs(:) = phs(:) * 2 * pi

                do ik = 1,nmode 
                    ky =  ik*2*pi/Ly
                    do iy=iymin,iymax 
                        pph = ky * ygrid(iy) + phs(ik)
                        !z component
                        uu(:,iy,:,7) = uu(:,iy,:,7) + db0 * sin(pph)
                    enddo
                enddo


            case(9)
                !add 2D version of the 3D switchback model
                do iy = iymin,iymax
                    y_SB = ygrid(iy) - Ly/2

                    r_SB = abs(y_SB)

                    do ix = ixmin,ixmax  
                        x_SB = xgrid(ix) - Lx/2
                        
                        uu(ix,iy,:,5) = B0_SB + dB_SB * exp(-x_SB*x_SB/H_SB/H_SB &
                            - y_SB*y_SB/R1_SB/R1_SB) 
                        uu(ix,iy,:,6) = dB_SB * sqrt(pi) * x_SB * R1_SB / H_SB / H_SB &
                            * exp(-x_SB*x_SB/H_SB/H_SB) * erf(y_SB/R1_SB)


                        ! add modification near the boundary
                        if (r_SB>=Rm_SB) then 
                            p_SB = exp(-((r_SB-Rm_SB)/Rd_SB)**2)
                            dpdy_SB = -2*(r_SB-Rm_SB)/Rd_SB**2 * p_SB * sign(1.0,y_SB)

                            uu(ix,iy,:,6) = p_SB * uu(ix,iy,:,6)
                            uu(ix,iy,:,5) = uu(ix,iy,:,5) + sqrt(pi)/2 * dpdy_SB * &
                                dB_SB * R1_SB * erf(y_SB/R1_SB) * (exp(-x_SB*x_SB/H_SB/H_SB) - 1)
                        endif 

                        
                        IF (uu(ix,iy,1,5)**2 + uu(ix,iy,1,6)**2 < B0_SB**2) THEN
                            uu(ix,iy,:,7) = sqrt(B0_SB**2 - uu(ix,iy,:,5)**2 - uu(ix,iy,:,6)**2)
                        else
                            uu(ix,iy,:,7) = 0.0
                            ! need to modify pressure to maintain constant Ptot
                            uu(ix,iy,:,8) = uu(ix,iy,:,8) - 0.5*(uu(ix,iy,:,5)**2 + &
                                uu(ix,iy,:,6)**2 - B0_SB**2)
                        ENDIF 
                    enddo 
                enddo


                ! add anti-correlated velocity fluctuation
                uu(:,:,:,2) = uu(:,:,:,2) - uu(:,:,:,5)/SQRT(uu(:,:,:,1))
                uu(:,:,:,3) = uu(:,:,:,3) - uu(:,:,:,6)/SQRT(uu(:,:,:,1))
                uu(:,:,:,4) = uu(:,:,:,4) - uu(:,:,:,7)/SQRT(uu(:,:,:,1))


            case(10)
                !add monochromatic outward alfven wave plus random density fluctuations
                ! for studying PDI
                kx =  iMode * 2 * pi/Lx
                do ix=ixmin,ixmax 
                    !z component
                    uu(ix,:,:,7) = uu(ix,:,:,7) - db0 * sqrt(uu(ix,:,:,1)) &
                        * sin(kx * xgrid(ix))
                    uu(ix,:,:,4) = uu(ix,:,:,4) + db0 * sin(kx * xgrid(ix))
                    !x&y component
                    ! uu(ix,:,:,2) = uu(ix,:,:,2) + db0 * cos(kx * xgrid(ix))  * sin_cor_ang
                    ! uu(ix,:,:,5) = uu(ix,:,:,5) - db0 * sqrt(uu(ix,:,:,1)) &
                    !     * cos(kx * xgrid(ix))  * sin_cor_ang

                    uu(ix,:,:,3) = uu(ix,:,:,3) + db0 * cos(kx * xgrid(ix)) ! * cos_cor_ang
                    uu(ix,:,:,6) = uu(ix,:,:,6) - db0 * sqrt(uu(ix,:,:,1)) &
                        * cos(kx * xgrid(ix)) ! * cos_cor_ang
                enddo


                ! random density fluctuations
                nmode = mode_end - mode_start + 1
                allocate(phs(nmode))
                ir = 1
                do ix=1,12
                    ir_arr(ix) = ir + 100
                enddo
                call random_seed(PUT=ir_arr)
                call random_number(phs)

                Do ikx=mode_start, mode_end
                    kk = ikx*2*pi/Lx
                    pph = (phs(ikx-mode_start+1) - 0.5) * 2.0 * pi 

                    do iz = izmin, izmax
                        do iy = iymin,iymax
                            do ix = ixmin,ixmax
                                uu(ix,iy,iz,1) = uu(ix,iy,iz,1) + delta_rho * &
                                    cos(kk*xgrid(ix)+pph)
                            enddo
                        enddo 
                    enddo
                End Do


            case(11)
                ! for 2D EBM simulation with Z-axis being the radial direction
                ! For comparison with (Matteini+2024)

                nmode = 4 * mode_end * mode_end 
                allocate(phs(nmode))
                ir = 1
                do ix=1,12
                    ir_arr(ix) = ir + 100
                enddo
                call random_seed(PUT=ir_arr)
                call random_number(phs)

                allocate(phs1(nmode))
                ir = 33
                do ix=1,12
                    ir_arr(ix) = ir + 100
                enddo
                call random_seed(PUT=ir_arr)
                call random_number(phs1)


                iMode = 1
                do ikx = -mode_end, mode_end
                    kx = ikx *  2 * pi/Lx
                    do iky = -mode_end, mode_end
                        ky = iky *  2 * pi/Ly
                        if (ikx==0 .and. iky==0) then 
                            cycle
                        endif 

                        if (floor(sqrt(real(ikx * ikx + iky * iky))) < mode_start .or. &
                            ceiling(sqrt(real(ikx * ikx + iky * iky))) > mode_end) then 
                            cycle 
                        endif 

                        ! take a random phase
                        pph = phs(iMode) * 2 * Pi
                        pph1 = phs1(iMode) * 2 * Pi
                        iMode = iMode + 1

                        ! determine direction
                        dir_x = ky / sqrt(kx**2 + ky**2)
                        dir_y = -kx / sqrt(kx**2 + ky**2) 

                        ! add perturbations
                        iz = 1
                        do iy=iymin,iymax 
                            do ix=ixmin,ixmax
                                !velocity -- uncorrelated with b
                                uu(ix,iy,iz,2) = uu(ix,iy,iz,2) + &
                                    sqrt(1-correlation_vb**2) * dir_x * dv0 * &
                                    cos(kx * xgrid(ix) + ky * ygrid(iy) + pph1) 

                                uu(ix,iy,iz,3) = uu(ix,iy,iz,3) + &
                                    sqrt(1-correlation_vb**2) * dir_y * dv0 *  &
                                    cos(kx * xgrid(ix) + ky * ygrid(iy) + pph1) 

                                ! magnetic field
                                ! bx
                                uu(ix,iy,iz,5) = uu(ix,iy,iz,5) + dir_x * db0 * &
                                    cos(kx * xgrid(ix) + ky * ygrid(iy) + pph)
                                ! by
                                uu(ix,iy,iz,6) = uu(ix,iy,iz,6) + dir_y * db0 * &
                                    cos(kx * xgrid(ix) + ky * ygrid(iy) + pph)

                                !velociy -- correlated with b
                                ! ux
                                uu(ix,iy,iz,2) = uu(ix,iy,iz,2) - correlation_vb * &
                                    dir_x * dv0 * cos(kx * xgrid(ix) + ky * ygrid(iy) + pph)
                                ! uy
                                uu(ix,iy,iz,3) = uu(ix,iy,iz,3) - correlation_vb * &
                                    dir_y * dv0 * cos(kx * xgrid(ix) + ky * ygrid(iy) + pph)
                            enddo
                        enddo
                    enddo
                enddo 



            case(14)
                !for tearing instability
                n_m = nmode

                ylow = 0.25 * Ly
                yup = 0.75 * Ly

                allocate(phs(nx))

                ! lower interface
                ir = 1

                call random_seed(ir)
                call random_number(phs)


                Do ikx=1, n_m
                    kk = ikx*2*pi/Lx
                    pph = (phs(ir) - 0.5) * 2.0 * pi 

                    ir = ir+1
                    do iz = izmin, izmax
                        do iy = iymin,iymax
                            do ix = ixmin,ixmax
                                uu(ix,iy,iz,5) = uu(ix,iy,iz,5) + delta_b * &
                                    cos(kk*xgrid(ix)+pph) * 0.5 * exp(-( (ygrid(iy) &
                                    -ylow) / a0 )**2) * (-2 * (ygrid(iy)-ylow)/a0**2)
                                uu(ix,iy,iz,6) = uu(ix,iy,iz,6) + delta_b * &
                                    kk * sin(kk*xgrid(ix)+pph) * 0.5 * exp(-( &
                                    (ygrid(iy)-ylow) / a0 )**2)
                            enddo
                        enddo 
                    enddo
                End Do


                ! upper interface
                ir = 16

                call random_seed(ir)
                call random_number(phs)

                Do ikx=1, n_m
                    pph = (phs(ir) - 0.5) * 2.0 * pi
                    ir = ir+1
                    kk = ikx*2*pi/Lx

                    do iz = izmin, izmax
                        do iy = iymin,iymax
                            do ix = ixmin,ixmax
                                uu(ix,iy,iz,5) = uu(ix,iy,iz,5) + delta_b * &
                                    cos(kk*xgrid(ix)+pph) * 0.5 * exp(-( (ygrid(iy) &
                                    -yup) / a0 )**2) * (-2 * (ygrid(iy)-yup)/a0**2)

                                uu(ix,iy,iz,6) = uu(ix,iy,iz,6) + delta_b * &
                                    kk * sin(kk*xgrid(ix)+pph) * 0.5 * exp(-( &
                                    (ygrid(iy)-yup) / a0 )**2)
                            enddo
                        enddo 
                    enddo

                End Do

            case default
                continue 
            end select


        end subroutine perturbation_initialize

        subroutine initial_calc_conserve_variable
            !after initializing background fields and the perturbation
            !transform uu to be conserved quantities
            implicit none 

            uu_prim(:,:,:,1:3) = uu(:,:,:,2:4) !ux,uy,uz
            uu_prim(:,:,:,4) = uu(:,:,:,8)  !pressure

            !rho * u
            uu(:,:,:,2) = uu(:,:,:,1) * uu_prim(:,:,:,1)
            uu(:,:,:,3) = uu(:,:,:,1) * uu_prim(:,:,:,2)
            uu(:,:,:,4) = uu(:,:,:,1) * uu_prim(:,:,:,3)

            !energy density
            uu(:,:,:,8) = uu_prim(:,:,:,4)/(adiabatic_index-1) + &
                0.5*(uu(:,:,:,1)*( (uu_prim(:,:,:,1))**2 + &
                (uu_prim(:,:,:,2))**2 + (uu_prim(:,:,:,3))**2) + &
                (uu(:,:,:,5))**2 + (uu(:,:,:,6))**2 + &
                (uu(:,:,:,7))**2 )
        end subroutine 

end module mhdinit