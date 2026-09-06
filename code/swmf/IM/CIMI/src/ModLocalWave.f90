module whistler_model
    implicit none
    save
    
    ! Global Constants
    real, parameter :: cC = 2.9979e8       ! speed of light (m/s)
    real, parameter :: cMe = 9.1094e-31    ! electron mass (kg)
    real, parameter :: cPi = 3.14159265359
    real, parameter :: cEta0 = 8.854e-12   ! permittivity of free space
    real, parameter :: cQe = 1.602e-19     ! electron charge (coulombs)
    real, parameter :: ckeV2Joule = cQe*1e3! keV to Joules
    real, parameter :: ccm2m = 1e-4        ! cm^2 to m^2
    
    ! Grid & Simulation parameters (Manually set in this module)
    ! NOTE W_I indicates array with shape of of resonant frequency grid
    !      P_I indicates array with shape of perpendicular momentum grid
    integer :: nFreq=189, nPperp=100, nPpar=100
    real :: FreqGridFactor=0.95
    real, allocatable :: wResW_I(:), pperpP_I(:), pparP_I(:), rIonoW_I(:)
    real :: dpperp, dppar

    ! Grid & Simulation parameters (To be allocated outside the module)
    !integer,public :: n_energy, n_alpha
    real :: tSim, dt
    
    ! Global State Variables
    real, allocatable :: WPowW_I(:)          ! Wave power
    real, allocatable :: WPowGrownW_I(:)     ! Power generated
    real, allocatable :: WPowLostW_I(:)      ! Power lost
    real :: WPowTotal                        ! Wave power in pT^2
    
    
contains
    ! ======================================================================
    ! Initialize Whistlers
    ! ======================================================================
    subroutine init_whistlers(p,bo)
        real,dimension(:),intent(in) :: p
        real, intent(in) :: bo
        integer :: ipperp,ippar,iFreq
        real :: wg

        ! allocate wave grid and momentum grid
        allocate(wResW_I(nFreq))
        allocate(rIonoW_I(nFreq))
        allocate(pperpP_I(nPperp))
        allocate(pparP_I(nPpar))
        allocate(WPowW_I(nFreq))
        allocate(WPowGrownW_I(nFreq))
        allocate(WPowLostW_I(nFreq))

        ! Populate regular pperp grid
        pperpP_I(1) = p(1)
        pperpP_I(nPperp) = p(SIZE(p))
        dpperp = (p(SIZE(p))-p(1)) / (float(nPperp)-1)
        do ipperp = 2,nPperp-1
            pperpP_I(ipperp) = pperpP_I(ipperp-1) + dpperp
        enddo

        ! Populate regular ppar grid
        pparP_I(1) = p(1)
        pparP_I(nPpar) = p(SIZE(p))
        dppar = (p(SIZE(p))-p(1)) / (float(nPpar)-1)
        do ippar = 2,nPpar-1
            pparP_I(ippar) = pparP_I(ippar-1) + dppar
        enddo

        ! Populate the resonance frequency grid
        wg = wgyro(bo)
        do iFreq=1,nFreq
            wResW_I(iFreq) = (0.5*wg) * FreqGridFactor**(iFreq-1)
        enddo

        WPowW_I = 1.0e2 ! Small seed wave power

        ! Initialize rIonoW_I array (e.g., 5% reflection -> 0.05)
        rIonoW_I = 0.005
        
        call freq_to_energy()
    end subroutine init_whistlers
    ! ======================================================================
    ! Update Wave Power (Advance 1 time step)
    ! ======================================================================
    subroutine update_wave_power(p,sina,PSD,&
                                 N_dens, bo, b_length,dt, isVerbose)
        ! Inputs
        real, dimension(:),intent(in) :: p,sina
        real, dimension(:,:),intent(in) :: PSD
        real, intent(in) :: N_dens, bo, b_length
        real, intent(in) :: dt
        logical, intent(inout), optional :: isVerbose

        ! Outputs
        ! NOTE Global variables modified: W_pow
        !                     unmodified: rIonoW_I, wResW_I, nFreq
        
        ! Locals
        integer :: i
        real :: wp, wg, k_res, gamma_L, p_res, del_R, a_crit
        real :: v_group, eta_r, a_r, growth_rate, gain_rate, loss_rate

        ! Default to isVerbose=False
        if (.not.present(isVerbose)) isVerbose=.false.
        
        ! 1. Plasma properties (Functions I & II)
        wp = wplasma(N_dens)
        wg = wgyro(bo)
        
        if (isVerbose) then
            ! Quick tests
            print *, " test wp: ", wp
            print *, " test wg: ", wg
            print *," test kr|wResW_I=0.1wg: ", calc_kr(0.1*wg,wp,wg)
            print *," test wResW_I/(kr c)|wResW_I=0.1wg: ", &
                                           0.1*wg/(calc_kr(0.1*wg,wp,wg)*cC)
            print *," test gammaL|wResW_I=0.1wg, pperpP_I=0: ", &
                                        calc_gamma_lorentz(0.1*wg,wp,wg,0.0)
            print *," test pR|wResW_I=0.1wg, pperpP_I=0: ", &
                                                   calc_pR(0.1*wg,wp,wg,0.0)
            print *," test delR|wResW_I=0.1wg, pperpP_I=0: ", &
                                                 calc_delR(0.1*wg,wp,wg,0.0)
            print *," test norm1|wResW_I=0.1wg, N=16: ", &
                                               calc_norm1(0.1*wg,wp,wg,16.0)
            print *," test vgroup/c|wResW_I=0.1wg: ", &
                                                calc_vgroup(0.1*wg,wp,wg)/cC
        endif

        ! Loop over all frequencies
        if (isVerbose) then
            write(*,"(7a11)") "wr(rad/s)","eta","A","growth",&
                                                       "vgrp","loss","Wpow"
        endif
        do i = 1, nFreq

            ! 1. Get the portion of the distribution in cyclotron resonance
            call calc_eta_rel(p,sina,PSD,N_dens,wResW_I(i),wp,wg,eta_r)

            ! 2. Get the relativistic pitch angle anisotropy factor
            call calc_a_rel(p,sina,PSD,wResW_I(i),wp,wg,a_r)

            ! 3. Get the growth rate
            growth_rate = calc_growth_rate(wResW_I(i),wp,wg,eta_r,a_r)

            ! 4. Get the group velocity and loss rate
            v_group = calc_vgroup(wResW_I(i),wp,wg)
            gain_rate = 2*growth_rate
            loss_rate = 2*v_group/b_length * log(rIonoW_I(i))

            ! 5. Update Wave power state
            WPowW_I(i) = WPowW_I(i) * exp((growth_rate+loss_rate) * dt)
            WPowGrownW_I(i) = WPowW_I(i) * (exp(growth_rate)*dt) - WPowW_I(i)
            WPowLostW_I(i) =  WPowW_I(i) * (exp(loss_rate)*dt) - WPowW_I(i)

            if (isVerbose) then
                write(*,"(7e11.3)") wResW_I(i),eta_r,a_r,&
                                growth_rate,v_group,loss_rate,WPowW_I(i)
            endif
        end do

        call freq_to_energy()
    end subroutine update_wave_power

    ! ======================================================================
    ! Frequency to Energy Convolution
    ! ======================================================================
    subroutine freq_to_energy()
        real, dimension(nFreq) :: frW_I
        integer :: iFreq
        ! Integrate over frequency space and obtain total wave power in pT^2
        frW_I = wResW_I/(2*cPi)
        WPowTotal = 0.0
        !print *, "i      f        df        W"
        do iFreq=2,nFreq
            WPowTotal = WPowTotal + (WPowW_I(iFreq)+WPowW_I(iFreq-1))/2 * &
                                            (frW_I(iFreq-1)-frW_I(iFreq))
            !write(*,"(i4,3e12.3)") iFreq, frW_I(iFreq),&
            !                     (frW_I(iFreq-1)-frW_I(iFreq)), WPowTotal
        enddo
    end subroutine freq_to_energy

    ! ======================================================================
    ! Portion of distribution in cyclotron resonance (Eq A2 integration)
    ! ======================================================================
    subroutine calc_eta_rel(p,sina,PSD,N,&
                            wResW_I,wp,wg,&
                            eta_r)
        ! Inputs
        real, dimension(:), intent(in) :: p,sina    ! PSD grid (from CIMI)
        real, dimension(:,:), intent(in) :: PSD     ! Phase space denisty
        real, intent(in) :: N, wResW_I, wg, wp       ! Dens, Wave frequencies
        ! Outputs
        real, intent(out) :: eta_r
        ! NOTE Global variables modified: None
        !                     unmodified: pperpP_I, pperpP_I
        ! Locals
        real :: kr, p_res, del_R
        real :: integral,dfdpparR,dfdpperpR,dfdpperpRmax,dfdpperpRmin
        integer :: j
        
        integral = 0.0
        ! Numerical integration over perpendicular momentum (pperp)
        !write(*,"(a4,5a12)") "j","pperpP_I(j)","del_R",&
        !                  "dpperp","dfdpperpR","integral"
        dfdpperpRmax = 0.0
        dfdpperpRmin = 0.0
        do j = 2,  SIZE(pperpP_I)-1
            ! Find strictly parallel resonant momentum at this freq & pperp
            kr = calc_kr(wResW_I,wp,wg)
            p_res = calc_pR(wResW_I,wp,wg,pperpP_I(j))
            del_R = calc_delR(wResW_I,wp,wg,pperpP_I(j))
            ! Get derivative interpolated to ppar = p_res
            call calc_derivatives(p,sina,PSD,&                 ! SOURCE
                             abs(p_res),pperpP_I(j),dppar,dpperp,&! IN
                                  dfdpparR,dfdpperpR)          ! OUT
            if (dfdpperpR.gt.dfdpperpRmax) dfdpperpRmax=dfdpperpR
            if (dfdpperpR.lt.dfdpperpRmin) dfdpperpRmin=dfdpperpR
            ! Construct the integrand
            integral = integral + (pperpP_I(j)**2/del_R) * dpperp *dfdpperpR&
                              /(pperpP_I(j)**2+p_res**2) / (ccm2m*ckeV2Joule)
            ! TODO investigate all the zeros in dfdpperp
            !       - my guess is that we're often off the grid in terms of 
            !           ppar = pR, so then the derivative is flat...
            !       - continue with this, plot with the correct units
            !       - also check with different boundary conditions
            !write(*,"(i4,5e12.3)") j, pperpP_I(j), del_R,&
            !                       dpperp, dfdpperpR, integral
        !print *, "dfdpperpRmax: ",dfdpperpRmax, "dfdpperpRmin: ",dfdpperpRmin
        end do
        
        eta_r = (cPi * cMe * (wResW_I - wg)) / (N * kr) * integral
    end subroutine calc_eta_rel

    ! ======================================================================
    ! Anisotropy factor (Eq A3 integration)
    ! ======================================================================
    subroutine calc_a_rel(p,sina,PSD,&
                          wResW_I,wp,wg,&
                          a_r)
        ! Inputs
        real, dimension(:), intent(in) :: p,sina    ! PSD grid (from CIMI)
        real, dimension(:,:), intent(in) :: PSD     ! Phase space denisty
        real, intent(in) :: wResW_I, wg, wp           ! Dens, Wave frequencies
        ! Outputs
        real, intent(out) :: a_r
        ! NOTE Global variables modified: None
        !                     unmodified: pperpP_I, dpperp
        ! Locals
        real :: kr, p_res, del_R, gamma_L
        real :: integral1,integral2,dfdpparR,dfdpperpR
        integer :: j

        integral1 = 0.0
        integral2 = 0.0
        ! Numerical integration over perpendicular momentum (pperp)
        do j = 2,  SIZE(pperpP_I)-1
            ! Find strictly parallel resonant momentum at this freq & pperp
            kr = calc_kr(wResW_I,wp,wg)
            p_res = calc_pR(wResW_I,wp,wg,pperpP_I(j))
            del_R = calc_delR(wResW_I,wp,wg,pperpP_I(j))
            gamma_L = calc_gamma_lorentz(wResW_I,wp,wg,pperpP_I(j))
            ! Get derivative interpolated to ppar = p_res
            call calc_derivatives(p,sina,PSD,&                 ! SOURCE
                             abs(p_res),pperpP_I(j),dppar,dpperp,&! IN
                                  dfdpparR,dfdpperpR)          ! OUT
            ! Construct the integrand(s)
            integral1 = integral1 + &
                            (pperpP_I(j)**2/del_R) * dpperp * dfdpperpR&
                            /(pperpP_I(j)**2+p_res**2) / (ccm2m*ckeV2Joule)
            integral2 = integral2 + &
                            (pperpP_I(j)**2/(del_R*gamma_L)) * dpperp* &
                              (pperpP_I(j)*dfdpparR - p_res*dfdpperpR)&
                            /(pperpP_I(j)**2+p_res**2) / (ccm2m*ckeV2Joule)
        end do
        
        a_r = kr/(cMe*(wResW_I-wg)) * integral2 / integral1
    end subroutine calc_a_rel

    ! ======================================================================
    ! Utility Subroutines and Functions (Eq A1 - A9)
    ! ======================================================================
    
    subroutine calc_derivatives(p,sina,PSD, ppar_0,pperp_0,dppar,dpperp,&! IN
                                dfdppar,dfdpperp)                        !OUT
        ! Inputs
        real, dimension(:), intent(in) :: p,sina ! momentum/PA grid
        real, dimension(:,:), intent(in) :: PSD ! phase-space density
        real, intent(in) :: ppar_0,pperp_0 ! target location for calc
        real, intent(in) :: dppar,dpperp ! differentials for calc

        ! Outputs
        real, intent(out) :: dfdppar,dfdpperp ! PSD derivatives at location

        ! Local variables
        real :: f_ppar_plus, f_ppar_minus, f_pperp_plus, f_pperp_minus

        ! Interpolate PSD at four locations near the target location
        call interp_polar_to_cartesian(p,sina,PSD,&             ! Source
                                       pperp_0, ppar_0+dppar,&  ! Target
                                       f_ppar_plus)             ! Output

        call interp_polar_to_cartesian(p,sina,PSD,&             ! Source
                                       pperp_0, ppar_0-dppar,&  ! Target
                                       f_ppar_minus)            ! Output

        call interp_polar_to_cartesian(p,sina,PSD,&             ! Source
                                       pperp_0+dpperp, ppar_0,& ! Target
                                       f_pperp_plus)             ! Output

        call interp_polar_to_cartesian(p,sina,PSD,&             ! Source
                                       pperp_0-dpperp, ppar_0,& ! Target
                                       f_pperp_minus)            ! Output

        ! Calculate derivatives using central difference
        dfdppar  = (f_ppar_plus - f_ppar_minus) / (2.0*dppar)
        dfdpperp = (f_pperp_plus - f_pperp_minus) / (2.0*dpperp)

    end subroutine calc_derivatives

    subroutine interp_polar_to_cartesian(p, sina, PSD, pperp, ppar,&
                                         PSDinterp)
        ! Inputs
        real, dimension(:), intent(in) :: p ! Total momentum grid
        real, dimension(:), intent(in) :: sina ! Pitch angle grid
        real, dimension(:, :), intent(in) :: PSD ! PSD(p, alpha)
        real, intent(in) :: pperp, ppar        ! Target coordinates

        ! Output
        real, intent(out) :: PSDinterp

        ! Local variables
        real,dimension(SIZE(sina)) :: alpha
        real :: target_p, target_alpha
        integer :: ip, ialpha
        integer :: i, j, p_idx, a_idx
        real :: t, u, f1, f2

        alpha = asin(sina)
        ! 1. Convert Cartesian (pperp, ppar) to Polar (p, alpha)
        target_p = sqrt(pperp**2 + ppar**2)
        target_alpha = atan2(pperp, ppar) ! NOTE Returns value in [0, pi]
                                          !       since p_perp >= 0

        ! 2. Handle Out-of-Bounds (Return 0.0 if outside the grid)
        ! TODO try this boundary condition
        !if (target_p < p(1)) p_idx = 1
        !if (target_p >= p(SIZE(p)))  p_idx = SIZE(p)-1
        !if (target_alpha < alpha(1))  a_idx = 1
        !if (target_alpha >= alpha(SIZE(alpha)))  a_idx = SIZE(alpha)-1
        if (target_p < p(1) .or. &
            target_p >= p(SIZE(p)) .or. &
            target_alpha < alpha(1) .or. &
            target_alpha >= alpha(SIZE(alpha))) then
        
            PSDinterp = 1e-20
            return
        
        end if

        ! 3. Find the lower bounding indices (p_idx, a_idx)
        p_idx = 1
        do i = 1, SIZE(p) - 1
            if (target_p >= p(i) .and. target_p < p(i+1)) then
                p_idx = i
                exit
            end if
        end do

        a_idx = 1
        do j = 1, SIZE(alpha) - 1
            if (target_alpha >= alpha(j) .and. &
                target_alpha < alpha(j+1)) then
                a_idx = j
                exit
            end if
        end do

        ! 4. Calculate fractional distances (weights)
        ! t ranges from 0 to 1 between p(i) and p(i+1)
        t = (target_p - p(p_idx)) / (p(p_idx+1) - p(p_idx))

        ! u ranges from 0 to 1 between alpha(j) and alpha(j+1)
        u = (target_alpha - alpha(a_idx)) / (alpha(a_idx+1) - alpha(a_idx))

        ! 5. Perform Bilinear Interpolation
        ! Interpolate along p at alpha_idx
        f1 = (1.0 - t) * PSD(p_idx, a_idx) + t * PSD(p_idx+1, a_idx)

        ! Interpolate along p at alpha_idx+1
        f2 = (1.0 - t) * PSD(p_idx, a_idx+1) + t * PSD(p_idx+1, a_idx+1)

        ! Interpolate along alpha
        PSDinterp = (1.0 - u) * f1 + u * f2

    end subroutine interp_polar_to_cartesian

    ! Function I
    real function wplasma(N_dens)
        real, intent(in) :: N_dens
        wplasma = sqrt(N_dens * cQe**2 / (cMe*cEta0))
    end function wplasma

    ! Function II
    real function wgyro(bo)
        real, intent(in) :: bo
        wgyro = abs((cQe * bo) / (cMe))
    end function wgyro

    ! Function III (Dispersion - requires solving Eq A8 for k)
    real function calc_kr(w_r, wp, wg)
        real, intent(in) :: w_r, wp, wg
        calc_kr = (w_r / cC) * sqrt(1.0 + (wp**2) / (w_r * (wg - w_r)))
    end function calc_kr

    ! Function IV (Eq A4 equivalent for Lorentz factor)
    real function calc_gamma_lorentz(wResW_I, wp, wg, pperp)
        real, intent(in) :: wResW_I, wp, wg, pperp
        real :: kr, term1, term2, term3
        ! Calculate gamma_R based on momentum components
        kr = calc_kr(wResW_I,wp,wg)
        term1 = (cC * kr / wResW_I)**2 - 1.0
        term2 = 1.0 + (pperp / (cMe * cC))**2
        term3 = (wResW_I / wg)**2
    calc_gamma_lorentz = &
                (-1.0+(cC * kr/wResW_I) * sqrt(term1 * term2 * term3 + 1))&
                                 /(term1 * (wResW_I / wg))
    end function calc_gamma_lorentz

    ! Function V (Eq A5)
    real function calc_pR(wResW_I, wp, wg, pperp)
        real,intent(in) :: wResW_I,wp,wg,pperp
        real :: kr,gamma_L
        kr = calc_kr(wResW_I,wp,wg)
        gamma_L = calc_gamma_lorentz(wResW_I,wp,wg,pperp)
        calc_pR = (cMe / kr) * (gamma_L * wResW_I - wg)
    end function calc_pR

    ! Function IV (Eq A6)
    real function calc_delR(wResW_I,wp,wg,pperp)
        real, intent(in) :: wResW_I,wp,wg,pperp
        real :: kr,gamma_L,pR

        kr = calc_kr(wResW_I,wp,wg)
        gamma_L = calc_gamma_lorentz(wResW_I,wp,wg,pperp)
        pR = calc_pR(wResW_I,wp,wg,pperp)
        calc_delR = 1.0 - (wResW_I * pR)/(cMe * cC**2 * kr * gamma_L)
    end function calc_delR

    ! Function VII (Eq A7)
    real function calc_Acritical(wResW_I, wg)
        real, intent(in) :: wResW_I, wg
        calc_Acritical = wResW_I / (wg - wResW_I)
    end function calc_Acritical

    ! Function VIII (Eq A9 - Group Velocity)
    real function calc_vgroup(wResW_I, wp, wg)
        real, intent(in) :: wResW_I, wp, wg
        real :: kr, num, den
        kr = calc_kr(wResW_I,wp,wg)
        num = 2.0 * cC**2 * kr * wg * (1.0-wResW_I/wg)**2
        den = wp**2 + 2.0 * wg * wResW_I * (1.0 - wResW_I/wg)**2
        calc_vgroup = num / den
    end function calc_vgroup

    ! Function IX (Eq A1 - Growth Rate)
    real function calc_growth_rate(wResW_I,wp,wg,eta_r, a_r)
        real, intent(in) :: wResW_I, wp, wg, eta_r, a_r
        real :: a_crit,num,denom
        a_crit = calc_Acritical(wResW_I,wg)
        num = cPi * wp**2 * eta_r * (a_r - a_crit)
        denom = 2 * wResW_I + (wp**2 * wg) / (wResW_I - wg)**2
        calc_growth_rate = num / denom
    end function calc_growth_rate

    ! Norm 1
    real function calc_norm1(wResW_I,wp,wg,N)
        real, intent(in) :: wResW_I,wp,wg,N
        real :: kr
        kr = calc_kr(wResW_I,wp,wg)
        calc_norm1 = cPi*cMe*(wResW_I-wg)/(N*kr)
    end function calc_norm1

    ! Norm 2
    real function calc_norm2(wResW_I,wp,wg)
        real, intent(in) :: wResW_I,wp,wg
        real ::  kr
        kr = calc_kr(wResW_I,wp,wg)
        calc_norm2 =  kr/(cMe*(wResW_I-wg))
    end function calc_norm2


end module whistler_model

