program unit_test
    use whistler_model
    implicit none
    
    integer :: step, iE
    integer, parameter :: max_steps = 60
    
    ! Dummy parameters for execution
    real :: N_dens = 16e6       ! m^-3
    real :: bo = 149e-9             ! T (149 nT)
    real :: b_length = 5.0 * 6371e3     ! Interaction length (m)
    real :: C1,kT,gamma1,S,E0,gamma2 ! analytical PSD parameters
    real, allocatable :: Ekev_E(:),Ejoules_E(:),sina_A(:),PSD_EA(:,:),p_E(:)
    logical :: doVerbose
    
    ! Timing variables used just for the test
    dt = 1.0
    tSim = 0.0

    ! Set energy and pitch angle grid (will be from CIMI)
    Ekev_E = [1e-4,1e-3,1e-2,1e-1,&
            1e0,1e1,5e1,1e2,2e2,3e2,4e2,5e2,6e2,7e2,8e2,9e2,1e3,&
            2e3,3e3,4e3,5e3,6e3,7e3,8e3,9e3,1e4]
    sina_A = [0.0,0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0]
    Ejoules_E = Ekev_E*ckeV2Joule ! NOTE parameter from module

    ! Analytical PSD function parameters
    C1 = 2.38e6
    kT = 0.001
    gamma1 = 0.978364
    E0 = 1748.57
    gamma2 = 7.03623
    S = 0.3

    ! allocate sample PSD information
    allocate(p_E(SIZE(Ekev_E)))
    allocate(PSD_EA(SIZE(Ekev_E),SIZE(sina_A)))

    ! Set equivalent momentum for each Energy
    do iE=1,SIZE(Ekev_E)
        !p_E(iE) = sqrt(Ekev_E(iE)*me*((Ekev_E(iE)/(me*c**2))+2))
        p_E(iE) = sqrt(Ejoules_E(iE)*cMe*((Ejoules_E(iE)/(cMe*cC**2))+2))
    enddo
    
    ! Get a sample distribution
    call syntheticP_ISD(N_dens,bo,C1,kT,gamma1,S,E0,gamma2,Ekev_E,sina_A,PSD_EA)

    ! Initialize wave grid
    call init_whistlers(p_E,bo)

    ! 2. Main Time Loop
    call write_debug_output(max_steps,0.0,p_E,sina_A,PSD_EA)
    write(*,"(a,f12.2,a,e12.3,a,e12.3)") "Time: ",tSim,&
                                         "   Max W: ",maxval(WPowW_I),&
                                         "   W_pT2: ",WPowTotal
    doVerbose=.true.
    do step = 1, max_steps
        tSim = tSim + dt
        
        call update_wave_power(p_E,sina_A,PSD_EA,N_dens,bo,b_length,dt,&
                               doVerbose)
        call write_debug_output(max_steps,tSim,p_E,sina_A,PSD_EA)
        
        write(*,"(a,f12.2,a,e12.3,a,e12.3)") "Time: ",tSim,&
                                             "   Max W: ",maxval(WPowW_I),&
                                             "   W_pT2: ",WPowTotal
        doVerbose=.false.
    end do
    

contains
    subroutine syntheticP_ISD(N_dens,bo,C1,kT,gamma1,S,E0,gamma2,Ekev_E,&
                             sina_A,PSD_EA)
        real,intent(in) :: N_dens,bo,C1,kT,gamma1,S,E0,gamma2
        real,intent(in) :: Ekev_E(:), sina_A(:)
        real,intent(out) :: PSD_EA(:,:)

        integer :: ialpha, iE

        do ialpha = 1,SIZE(sina_A)
            do iE = 1,SIZE(Ekev_E)
                PSD_EA(iE,ialpha) =  &
                    C1*Ekev_E(iE)*(kT*(gamma1+1)+Ekev_E(iE))**(-gamma1-1)/ &
                       (1+(Ekev_E(iE)/E0)**(gamma2)) * sina_A(ialpha)**(2*S)
            enddo
        enddo

    end subroutine syntheticP_ISD

    subroutine write_debug_output(max_steps,tSim,p_E,sina_A,PSD_EA)
        use ModIoUnit,		ONLY: 	UnitTmp_
        ! Inputs
        integer,intent(in) :: max_steps
        real,intent(in) :: tSim
        real,dimension(:),intent(in) :: p_E
        real,dimension(:),intent(in) :: sina_A
        real,dimension(:,:),intent(in) :: PSD_EA
        ! Local variables
        real,dimension(100) :: pperp_plot
        real,dimension(100) :: ppar_plot
        real,dimension(100,100) :: PSD_plot
        real,dimension(100,100) :: dfdppar_plot
        real,dimension(100,100) :: dfdpperp_plot
        real,dimension(100) :: gammaL
        real,dimension(100) :: pR
        real,dimension(100) :: delR
        real :: dpperp,dppar,wp,wg
        integer :: ip,ialpha,ipperp,ippar,ifreq
        logical,save :: IsFirstCall = .true.
        ! Globals
        !   wResW_I(n_freq), WPowW_I(n_freq)

        wp = wplasma(N_dens)
        wg = wgyro(bo)
        ! Write out PSD to file
        if (IsFirstCall) then
            open(unit=UnitTmp_,&
                file='IM/plots/localwaveP_ISD.psd',&
                status='unknown')
            ! Header info
            write(UnitTmp_,*)"! ntimes, np, nsina"
            write(UnitTmp_,"(3i6)") max_steps+1,SIZE(p_E),SIZE(sina_A)
            write(UnitTmp_,*)"! Energy grid"
            write(UnitTmp_,"(8f9.1)") Ekev_E
            write(UnitTmp_,*)"! momentum grid"
            write(UnitTmp_,"(7e10.3)") p_E
            write(UnitTmp_,*)"! pitch angle grid"
            write(UnitTmp_,"(7f9.3)") sina_A
        else
            open(unit=UnitTmp_,&
                file='IM/plots/localwaveP_ISD.psd',&
                status='old',&
                position='append')
        endif
        ! Entry info
        write(UnitTmp_,*)"! tSim"
        write(UnitTmp_,"(f6.1)") tSim
        do ialpha=1,SIZE(sina_A)
            write(UnitTmp_,"(8e9.2)") PSD_EA(:,ialpha)
        enddo
        close(UnitTmp_)
        if (IsFirstCall) write(*,*)"SAVED: IM/plots/localwaveP_ISD.psd"

        ! Populate regular pperp grid
        pperp_plot(1) = p_E(1)
        pperp_plot(SIZE(pperp_plot)) = p_E(SIZE(p_E))
        dpperp = (p_E(SIZE(p_E))-p_E(1)) / (SIZE(pperp_plot)-1)
        do ipperp = 2,SIZE(pperp_plot)-1
            pperp_plot(ipperp) = pperp_plot(ipperp-1) + dpperp
        enddo

        ! Populate regular ppar grid
        ppar_plot(1) = p_E(1)
        ppar_plot(SIZE(ppar_plot)) = p_E(SIZE(p_E))
        dppar = (p_E(SIZE(p_E))-p_E(1)) / (SIZE(ppar_plot)-1)
        do ippar = 2,SIZE(ppar_plot)-1
            ppar_plot(ippar) = ppar_plot(ippar-1) + dppar
        enddo

        ! Interpolate PSD to output grid
        do ipperp=1,SIZE(pperp_plot)
            do ippar=1,SIZE(ppar_plot)
                call interp_polar_to_cartesian(p_E,sina_A,PSD_EA,&    ! Source
                                pperp_plot(ipperp),ppar_plot(ippar),& ! Target
                                              PSD_plot(ippar,ipperp)) ! Output 
            enddo
        enddo

        ! Write out PSD_plot to file
        if (IsFirstCall) then
            open(unit=UnitTmp_,&
                file='IM/plots/localwaveP_ISDinterp.psd',&
                status='unknown')
            ! Header info
            write(UnitTmp_,*)"! ntimes, nppar, npperp"
            write(UnitTmp_,"(3i6)")max_steps+1,SIZE(ppar_plot),SIZE(pperp_plot)
            write(UnitTmp_,*)"! parallel momentum grid"
            write(UnitTmp_,"(7e10.3)") ppar_plot
            write(UnitTmp_,*)"! perpendicular momentum grid"
            write(UnitTmp_,"(7e10.3)") pperp_plot
        else
            open(unit=UnitTmp_,&
                file='IM/plots/localwaveP_ISDinterp.psd',&
                status='old',&
                position='append')
        endif
        ! Entry info
        write(UnitTmp_,*)"! tSim"
        write(UnitTmp_,"(f6.1)") tSim
        do ipperp=1,SIZE(ppar_plot)
            write(UnitTmp_,"(7e11.3)") PSD_plot(:,ipperp)
        enddo
        if (IsFirstCall) write(*,*)"SAVED: IM/plots/localwaveP_ISDinterp.psd"
        close(UnitTmp_)

        ! Calculate derivatives at each PSD_plot point
        do ipperp=1,SIZE(pperp_plot)
            do ippar=1,SIZE(ppar_plot)
                call calc_derivatives(p_E,sina_A,PSD_EA,&
                            ppar_plot(ippar),pperp_plot(ipperp),dppar,dpperp,&
                                            dfdppar_plot(ippar,ipperp),&
                                            dfdpperp_plot(ippar,ipperp))
            enddo
        enddo

        ! Write out derivatives to file
        if (IsFirstCall) then
            open(unit=UnitTmp_,&
                file='IM/plots/localwave_dfdppar.psd',&
                status='unknown')
            ! Header info
            write(UnitTmp_,*)"! ntimes, nppar, npperp"
            write(UnitTmp_,"(3i6)")max_steps+1,SIZE(ppar_plot),SIZE(pperp_plot)
            write(UnitTmp_,*)"! parallel momentum grid"
            write(UnitTmp_,"(7e10.3)") ppar_plot
            write(UnitTmp_,*)"! perpendicular momentum grid"
            write(UnitTmp_,"(7e10.3)") pperp_plot
        else
            open(unit=UnitTmp_,&
                file='IM/plots/localwave_dfdppar.psd',&
                status='old',&
                position='append')
        endif
        ! Entry info
        write(UnitTmp_,*)"! tSim"
        write(UnitTmp_,"(f6.1)") tSim
        do ipperp=1,SIZE(ppar_plot)
            write(UnitTmp_,"(7e11.3)") dfdppar_plot(:,ipperp)
        enddo
        if (IsFirstCall) write(*,*)"SAVED: IM/plots/localwave_dfdppar.psd"
        close(UnitTmp_)

        if (IsFirstCall) then
            open(unit=UnitTmp_,&
                file='IM/plots/localwave_dfdpperp.psd',&
                status='unknown')
            ! Header info
            write(UnitTmp_,*)"! ntimes, nppar, npperp"
            write(UnitTmp_,"(3i6)")max_steps+1,SIZE(ppar_plot),SIZE(pperp_plot)
            write(UnitTmp_,*)"! parallel momentum grid"
            write(UnitTmp_,"(7e10.3)") ppar_plot
            write(UnitTmp_,*)"! perpendicular momentum grid"
            write(UnitTmp_,"(7e10.3)") pperp_plot
        else
            open(unit=UnitTmp_,&
                file='IM/plots/localwave_dfdpperp.psd',&
                status='old',&
                position='append')
        endif
        ! Entry info
        write(UnitTmp_,*)"! tSim"
        write(UnitTmp_,"(f6.1)") tSim
        do ipperp=1,SIZE(ppar_plot)
            write(UnitTmp_,"(7e11.3)") dfdpperp_plot(:,ipperp)
        enddo
        if (IsFirstCall) write(*,*)"SAVED: IM/plots/localwave_dfdpperp.psd"
        close(UnitTmp_)

        ! Write out pR and delR as function of resonant frequency
        if (IsFirstCall) then
            open(unit=UnitTmp_,&
                file='IM/plots/localwave_resonances.dat',&
                status='unknown')
            ! Header info
            write(UnitTmp_,*)"! ntimes, nfreq, npperp"
            write(UnitTmp_,"(3i6)")max_steps+1,SIZE(wResW_I),SIZE(pperp_plot)
            write(UnitTmp_,*)"! resonant frequencies"
            write(UnitTmp_,"(7e10.3)") wResW_I
            write(UnitTmp_,*)"! perpendicular momentum grid"
            write(UnitTmp_,"(7e10.3)") pperp_plot
        else
            open(unit=UnitTmp_,&
                file='IM/plots/localwave_resonances.dat',&
                status='old',&
                position='append')
        endif
        ! Entry info
        write(UnitTmp_,*)"! tSim"
        write(UnitTmp_,"(f6.1)") tSim
        do ifreq=1,SIZE(wResW_I)
            write(UnitTmp_,"(3a14)") "! wResW_I","kres","WPowW_I"
            write(UnitTmp_,"(3e14.3e3)") wResW_I(ifreq),&
                                         calc_kr(wResW_I(ifreq),wp,wg),&
                                         WPowW_I(ifreq)
            do ipperp=1,SIZE(pperpP_I)
                gammaL = calc_gamma_lorentz(wResW_I(ifreq),wp,wg,&
                                                              pperpP_I(ipperp))
                pR = calc_pR(wResW_I(ifreq),wp,wg,pperpP_I(ipperp))
                delR = calc_delR(wResW_I(ifreq),wp,wg,pperpP_I(ipperp))
            enddo
            write(UnitTmp_,"(1a11)") "! gammaL"
            write(UnitTmp_,"(7e11.3)") gammaL
            write(UnitTmp_,"(1a11)") "! pR"
            write(UnitTmp_,"(7e11.3)") pR
            write(UnitTmp_,"(1a11)") "! delR"
            write(UnitTmp_,"(7e11.3)") delR
        enddo
        if (IsFirstCall) write(*,*)"SAVED: IM/plots/localwave_resonances.dat"
        close(UnitTmp_)

        IsFirstCall = .false.
    end subroutine write_debug_output

end program unit_test
