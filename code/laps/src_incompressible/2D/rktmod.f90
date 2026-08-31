Module rktmod
    !--- all the routines and variables needed to perform
    !    temporal integration with the Runge-Kutta method (rktmod.f90)


    use mhdinit, only: nx,ny,nz,nvar,uu_fourier,fnl,fnl_rk, &
        if_resis,resistivity, if_resis_exp,&
        if_visc, viscosity, if_visc_exp, k_square

    Implicit none

    real, dimension(3) :: cc1,dd1,time_step

    contains
        Subroutine rkt_init(dt)
            Real :: dt
            Real, parameter :: cc10 = 8.0/15.0, cc20=5.0/12.0, cc30=0.75  
            Real, parameter :: dd20 = -17./60., dd30=-5./12.
            Real, parameter :: time_step1 = 8./15., time_step2 = 2./15., &
                time_step3 = 1./3.

            fnl_rk(:,:,:,:) = cmplx(0.0, 0.0)

            cc1(1) = cc10*dt ; dd1(1) = 0.0
            cc1(2) = cc20*dt ; dd1(2) = dd20*dt
            cc1(3) = cc30*dt ; dd1(3) = dd30*dt

            time_step(1) = time_step1 * dt 
            time_step(2) = time_step2 * dt 
            time_step(3) = time_step3 * dt

        End Subroutine rkt_init

        Subroutine rkt(irk)
            implicit none 

            Integer,intent(in) :: irk
            integer :: ivar

            uu_fourier(:,:,:,:) = cc1(irk)*fnl(:,:,:,:) + dd1(irk) * fnl_rk(:,:,:,:) &
                    + uu_fourier(:,:,:,:)
            fnl_rk(:,:,:,:) = fnl(:,:,:,:)


            !fully implicit method for resistivity-----------------------
            if (if_resis .and. (.not. if_resis_exp)) then
                do ivar = 5,7
                    uu_fourier(:,:,:,ivar) = uu_fourier(:,:,:,ivar) / (time_step(irk) &
                        * k_square(:,:,:) * resistivity + 1.0)
                enddo
            endif

            !fully implicit method for viscosity-----------------------
            if (if_visc .and. (.not. if_visc_exp)) then
                do ivar = 2,4
                    uu_fourier(:,:,:,ivar) = uu_fourier(:,:,:,ivar) / (time_step(irk) &
                        * k_square(:,:,:) * viscosity + 1.0)
                enddo
            endif
        End Subroutine rkt

End Module rktmod

