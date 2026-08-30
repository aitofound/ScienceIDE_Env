program mhd
    !2D version of the code
    use mhdinit
    use fftw
    use AEBmod
    use mhdoutput
    use mhdrhs
    use rktmod
    use dealiasing
    use mhdrms
    use restart

    implicit none 

    integer :: ix,iy,iz, ivar, ixmin,ixmax,iymin,iymax,izmin,izmax
    real :: dt, tmax, dtout, dtrms, dtlog
    real :: tout, toutrms, tlog
    integer :: iout = 0
    integer(kind=8) :: istep = 0
    integer(kind=8) :: dstep_calcdt = 20
    real :: cfl = 0.5
    real :: max_divB = 0.0
    logical :: if_limit_dt_increase = .false.

    integer(kind=8) :: dstep_checknan = 200
    integer :: isnanAll = 0

    integer(kind=8) :: clock_count_start, clock_count_end, clock_count_max, clock_count_rate


    character(LEN=*),parameter :: format_output_log = '(2x,a,2x,f10.4,2x,a,2x,1pe12.4)'
    character(LEN=*),parameter :: format_output_rms = '(6x,a,2x,f10.4,a,1pe10.2,a,1pe12.4)'

    namelist/genr/tmax,dtout,dtrms,output_primitive,if_restart,n_start
    namelist/numerical/cfl,dealias_option,afx,afy,if_limit_dt_increase, &
        if_resis_exp, if_visc_exp, if_conserve_background
    namelist/grid/nx,ny,Lx,Ly
    namelist/phys/adiabatic_index, if_resis,resistivity, if_visc,viscosity
    namelist/field/ifield,B0,a0,current_sheet_width,shear_width,press0,U_fast,U_slow,&
        n_fast,n_slow,Bx0,By0,Bz0,jet_width,Vjet
    namelist/pert/ipert,delta_b,db0,dv0,nmode,in_out_ratio,initial_spectral_slope,&
        B0_SB,dB_SB,R1_SB,H_SB,Rm_SB,Rd_SB,delta_rho,iMode, mode_start, mode_end,&
        if_external_force, correlation_vb
    namelist/AEB/if_AEB,radius0,Ur0,if_corotating,corotating_angle,if_z_radial
    namelist/Hall/if_hall, ion_inertial_length
     ! Get the clock count at the beginning
    call system_clock(clock_count_start, clock_count_rate, clock_count_max )

    open(unit=4, status='old', file='mhd.input')
        read(4, genr)
        read(4, numerical)
        read(4, grid)
        read(4, field)
        read(4, pert)
        read(4, phys)
        read(4, AEB)
        read(4, Hall)
    close(unit=4)

    ! check incompatible parameters
    if (if_AEB) then 
        if (if_z_radial .and. if_corotating) then 
            if (ipe == 0) then 
                write(*,*) 'You CANNOT set both if_z_radial and if_corotating!!!'
            endif
            call mpi_finalize(ierr)
            ! call exit(ierr) 
        endif
    endif


    !initialization modules -----------------------
    call parallel_start(nx,ny,nvar)

    call fftw_initialize

    call grid_initialize

    call arrays_initialize

    call output_initialize 

    call rms_initialize

    if (.not. if_AEB) then 
        Ur0 = 0.0
    endif
    call AEB_initialize

    call dealias_initialize

    if (if_restart) then
        call read_restart
        call evolve_radius(t_restart)

        if (ipe == 0) then
            write(*,'(1x,a,f10.4)') 'Setting radius to be R = ', radius
        endif
    else
        call background_fields_initialize
        call perturbation_initialize
    endif


    call initial_calc_conserve_variable
    call transform_uu_real_to_fourier 
    !----------------------------------------------

    ! determine dt at the beginning
    dt = 0.0
    call vardt  
    call write_initial

    !output grid, parallel_info, uu, rms at the beginning
    call output_grid
    call output_parallel_info

    dtlog = min(dtout, dtrms) / 10.0

    time = t_restart
    iout = n_start
    tout = time + dtout
    toutrms = time + dtrms
    tlog = time + dtlog

    if (ipe==0) then
        write(*,format_output_log) 'OUTPUT at time:', time, ',dt = ', dt
    endif
    call output_uu(iout, time)
    iout = iout + 1

    call calc_max_divB
    if (ipe==0) then
        write(*,format_output_rms) 'OUTPUT RMS at time:', time , &
            ', max(div B) = ', max_divB, ', dt = ', dt
    endif 
    call output_rms(time)
    call output_AEB(time)
    !------------------------------------------

    !main loop ------------------------------
    Principal : Do

        ! output main array at time = tout
        if (time >= tout) then
            if (ipe==0) then
                write(*,format_output_log) 'OUTPUT at time:', time, ',dt = ', dt
            endif
            call output_uu(iout,time)
            iout = iout+1
            tout = tout + dtout
        endif

        ! output rms -------------
        if (time >= toutrms) then
            call calc_max_divB
            if (ipe==0) then
                write(*,format_output_rms) 'OUTPUT RMS at time:', time , &
                    ', max(div B) = ', max_divB, ', dt = ', dt
            endif 
            call output_rms(time)
            call output_AEB(time)
            toutrms = toutrms + dtrms
        end if


        if (dt < 0.00000001) then
            if (ipe==0) then
                write(*,*) '***********************************'
                write(*,*) 
                write(*,*) 'STOP: dt  < 1.e-8    '
                write(*,'(1x,a14,1pe12.4,a14,i6)' ) '  time = ', time,'istep =', istep
                write(*,'(1x,a14,1pe12.4)') 'dt =', dt
                write(*,*)
                write(*,*) '***********************************'
            end if

            if (ipe==0) then
                write(*,format_output_log) 'OUTPUT at time:', time, ',dt = ', dt
            endif

            call output_uu(iout,time)

            call calc_max_divB
            if (ipe==0) then
                write(*,format_output_rms) 'OUTPUT RMS at time:', time , &
                    ', max(div B) = ', max_divB, ', dt = ', dt
            endif 
            call output_rms(time)
            call output_AEB(time)

            exit Principal
        endif


        ! evolve --------------------------
        call evolve
        time = time + dt
        istep = istep + 1
        call evolve_radius(time)
        !---------------------------------


        !--Exit Condition for end time of simulation or for istep==nstep
        if (time >= tmax) then 
            if (ipe==0) then
                write(*,format_output_log) 'OUTPUT at time:', time, ',dt = ', dt
            endif
            call output_uu(iout,time)

            call calc_max_divB
            if (ipe==0) then
                write(*,format_output_rms) 'OUTPUT RMS at time:', time , &
                    ', max(div B) = ', max_divB, ', dt = ', dt
            endif 
            call output_rms(time)
            call output_AEB(time)


            if (ipe==0) then 
                write(*,*)
                write(*,*) '-------------------------------------------------'
                write(*,*) 'Simulation ends because tmax is reached.'
                write(*,*) '-------------------------------------------------'
            endif

            exit Principal
        end if


        !write log file
        if(time >= tlog) then
            call write_log
            tlog = tlog + dtlog
        end if

        !Update dt every (dstep_calcdt) steps
        if ( (istep>0) .and. ( MODULO(istep,dstep_calcdt)==0)) then 
            call vardt
        endif

        ! check nan every (dstep_checknan) steps
        if ((istep>0) .and. ( MODULO(istep,dstep_checknan)==0) ) then 
            call checkNan

            if (isnanAll == 1) then 
                if (ipe==0) then
                    write(*,'(1x,a,f10.4)') "NaN encountered!!! Exit the program at t = ", time 
                endif
                exit Principal
            endif 
        endif

    End do Principal
    !----------------------------------------

    ! !finalize modules------------------
    call parallel_end

    call fftw_finalize
    ! !---------------------------------

    contains 

        subroutine evolve 
            implicit none

            integer :: irk

            do irk=1,3
                !calculate flux function in real space, from uu and uu_prim
                call calc_flux

                !calculate flux_fourier
                call transform_flux_real_to_fourier

                !calculate rhs of the MHD equation
                call calc_rhs
            
                !update uu_fourier
                call rkt(irk) !f(t+dt)

                !dealiasing
                call dealias

                !transform uu_fourier to get uu
                call transform_uu_fourier_to_real

                !update uu_prim
                call update_uu_prim_from_uu
            enddo
        end subroutine evolve 

        subroutine vardt 
            implicit none 

            integer :: ix,iy,iz, ierr
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax 
            real :: csound2, calfven2, cms2
            real :: calfvenx, calfveny, calfvenz
            real :: cfastx, cfasty, cfastz
            real :: cslowx, cslowy, cslowz
            real :: cnsx2, cnsy2, cnsz2
            real :: ux, uy, uz
            real :: cmaxx, cmaxy, cmaxz, cmaxhall
            real :: dtx, dty, dtmin, dtmin_iter

            ixmin = 1
            ixmax = nx 
            iymin = yi_offset(myid_i+1) + 1
            iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
            izmin = 1
            izmax = 1
            
            dtmin = -1

            do iz=izmin,izmax
                do iy=iymin,iymax
                    do ix=ixmin,ixmax
                        ! sound speed squared = kappa * p / rho
                        csound2 = adiabatic_index * uu_prim(ix,iy,iz,4) / uu(ix,iy,iz,1)  

                        ! alfven wave speeds
                        calfvenx = ( uu(ix,iy,iz,5) ) / sqrt(uu(ix,iy,iz,1))
                        calfveny = ( uu(ix,iy,iz,6) ) / sqrt(uu(ix,iy,iz,1))
                        calfvenz = ( uu(ix,iy,iz,7) ) / sqrt(uu(ix,iy,iz,1))

                        calfven2 = calfvenx**2 + calfveny**2 + calfvenz**2
                        cms2 = csound2 + calfven2

                        cnsx2 = sqrt(max(cms2**2 - 4 * csound2 * calfvenx**2 , 0.))
                        cnsy2 = sqrt(max(cms2**2 - 4 * csound2 * calfveny**2 , 0.))
                        cnsz2 = sqrt(max(cms2**2 - 4 * csound2 * calfvenz**2 , 0.))

                        cfastx = sqrt(cms2 + cnsx2) / sqrt(2.)
                        cfasty = sqrt(cms2 + cnsy2) / sqrt(2.)
                        cfastz = sqrt(cms2 + cnsz2) / sqrt(2.)

                        cslowx = sqrt(max(cms2 - cnsx2,0.)) / sqrt(2.)
                        cslowy = sqrt(max(cms2 - cnsy2,0.)) / sqrt(2.)
                        cslowz = sqrt(max(cms2 - cnsz2,0.)) / sqrt(2.)

                        ux = uu_prim(ix,iy,iz,1)
                        uy = uu_prim(ix,iy,iz,2)
                        uz = uu_prim(ix,iy,iz,3)

                        cmaxx = max(abs(ux + cfastx), abs(ux + cslowx), &
                            abs(ux + calfvenx), abs(ux - cfastx), &
                            abs(ux - cslowx), abs(ux - calfvenx), abs(ux))

                        cmaxy = max(abs(uy + cfasty), abs(uy + cslowy), &
                            abs(uy + calfveny), abs(uy - cfasty), &
                            abs(uy - cslowy), abs(uy - calfveny), abs(uy))

                        cmaxz = max(abs(uz + cfastz), abs(uz + cslowz), &
                            abs(uz + calfvenz), abs(uz - cfastz), &
                            abs(uz - cslowz), abs(uz - calfvenz), abs(uz))


                        if (if_resis .and. if_resis_exp) then 
                            cmaxx = max(cmaxx, resistivity / dx)
                            cmaxy = max(cmaxy, resistivity / dy)
                        endif

                        if (if_hall) then 
                            cmaxhall = ion_inertial_length / uu(ix,iy,iz,1) * &
                                max(uu(ix,iy,iz,5),uu(ix,iy,iz,6),uu(ix,iy,iz,7)) / &
                                min(dx,dy)

                            cmaxx = max(cmaxx,cmaxhall)
                            cmaxy = max(cmaxy,cmaxhall)
                            cmaxz = max(cmaxz,cmaxhall)
                        endif

                        dtx = dx / cmaxx 
                        if (if_AEB .and. if_z_radial) then 
                            dtx = dtx * (radius / radius0)
                        endif 
                        dty = dy / cmaxy * (radius / radius0)

                        dtmin_iter = min(dtx,dty)

                        if ( dtmin < 0 .or. dtmin_iter < dtmin ) then 
                            dtmin = dtmin_iter
                        endif
                    enddo
                enddo
            enddo

            dtmin_iter = dtmin
            call mpi_allreduce(dtmin_iter, dtmin,1,mpi_realtype,mpi_min,&
                mpi_comm_world,ierr )

            dtmin = dtmin * cfl 

            if (if_limit_dt_increase) then 
            
                if (dt .eq. 0.0 .or. dt>1.02*dtmin) then
                    dt = dtmin
                endif
            else 
                if (dt<0.98*dtmin .or. dt>1.02*dtmin) then 
                    dt = dtmin
                endif
            endif

            call rkt_init(dt)
        end subroutine vardt 

        subroutine write_log 
            implicit none 

            real :: clock_time
            integer :: hour, minute, second


            ! Get the clock count 
            call system_clock(clock_count_end, clock_count_rate, clock_count_max )
            ! calculate clock time in second
            clock_time = real(clock_count_end - clock_count_start) / real(clock_count_rate)

            if (ipe==0) then
                hour = floor(clock_time / 3600.)
                minute = floor( (clock_time / 3600. - hour) * 60)
                second = floor( ((clock_time / 3600. - hour) * 60 - minute)*60)

                open  (10,file='log')
                    write (10,'(3x,a,f8.4)') 'Simulation time:',time
                    write (10,*) 'dt:',dt
                    write (10,'(3x,a,f15.2)') 'Real time (sec):',clock_time
                    write (10,'(3x,a,i3,a,i3,a,i3,a)') 'Real time (hh,mm,ss):',hour,'h',minute,'m',second,'s'
                    write (10,'(3x,a,i8)') 'Iterations     :',istep
                    write (10,*) 'tasks: ',npe 
                close (10)
            end if
        end subroutine write_log

        subroutine write_initial 
            implicit none 

            if (ipe == 0) then
                write(*,*) '2D MHD simulation..........'
                write(*,*) '--------------------------------------------------'
                write(*,'(5x,a8,f10.2)') 'cfl = ', cfl 
                write(*,'(5x,a8,1pe10.2)') 'dt = ', dt
                write(*,'(3(3x,a6,i6))') 'nx = ', nx, 'ny = ', ny
                write(*,*)
                if (output_primitive) then 
                    write(*,*) 'Output primitive variables...' 
                else 
                    write(*,*) 'Output conserved variables...'
                endif
                if (if_resis) then 
                    if (if_resis_exp) then 
                        write(*,'(1x,a,1pe10.2)') 'Resistivity: ON. Explicit method. eta = ',resistivity
                    else 
                        write(*,'(1x,a,1pe10.2)') 'Resistivity: ON. Implicit method. eta = ',resistivity
                    endif
                else 
                    write(*,'(1x,a)') 'Resistivity: OFF.'
                endif
                if (if_visc) then 
                    if (if_visc_exp) then
                        write(*,'(1x,a,1pe10.2)') 'Viscosity: ON. Explicit method. nu = ',viscosity
                    else 
                        write(*,'(1x,a,1pe10.2)') 'Viscosity: ON. Implicit method. nu = ',viscosity
                    endif
                else 
                    write(*,'(1x,a)') 'Viscosity: OFF.'
                endif
                if (if_AEB) then 
                    write(*,'(1x,a,1pe10.2,a,1pe10.2)') 'Expanding Box: ON. Ur0 = ',Ur0, &
                        ', R0 = ',radius0
                    if (if_z_radial) then 
                        write(*,'(4x,a)') 'Z-axis is the radial direction'
                    else 
                        write(*,'(4x,a)') 'X-axis is the radial direction'
                    endif
                else 
                    write(*,'(1x,a)') 'Expanding Box: OFF.'
                endif

                if (if_Hall) then 
                    write(*,'(1x,a,1pe10.2)') 'Hall term: ON. di = ', ion_inertial_length
                else
                    write(*,'(1x,a)') 'Hall term: OFF.'
                endif

                if (dealias_option==1) then
                    write(*,*)
                    write(*,'(1x,a,f10.4,a,f10.4)') &
                        'Use circular-padding for dealiasing.'
                    write(*,*)
                else if (dealias_option==2) then
                    write(*,*)
                    write(*,'(1x,a,f10.4,a,f10.4)') &
                        'Use smoothing function for dealiasing. afx = ',afx,&
                        ', afy = ',afy
                    write(*,*)
                else if (dealias_option==3) then 
                    write(*,*)
                    write(*,'(1x,a,f10.4,a,f10.4)') &
                        'Use square-padding for dealiasing.'
                    write(*,*)
                endif
                write(*,*) '--------------------------------------------------'
            endif
        end subroutine write_initial

        subroutine calc_max_divB
            ! Get the maximum divB in the fourier space
            implicit none 
            integer :: ix,iy,iz,ierr
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax
            real :: divB_iter
            complex :: kx,ky,kz

            max_divB = 0

            izmin = 1
            izmax = 1

            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = 1
            iymax = ny

            do iz=izmin,izmax
                kz = cmplx(0,0)
                do iy=iymin,iymax
                    ky = cmplx(0,wave_number_y(iy)) * radius0/radius
                    do ix=ixmin,ixmax
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

                        divB_iter = abs( kx * uu_fourier(ix,iy,iz,5) + &
                                ky * uu_fourier(ix,iy,iz,6) + &
                                kz * uu_fourier(ix,iy,iz,7) )
                        if (divB_iter > max_divB) then 
                            max_divB = divB_iter
                        endif
                    enddo
                enddo
            enddo

            divB_iter = max_divB
            call mpi_allreduce(divB_iter, max_divB,1,mpi_realtype,mpi_max,&
                mpi_comm_world,ierr )

        end subroutine calc_max_divB

        subroutine checkNan
            integer :: is_Nan = 0
            integer :: ix,iy,iz,iv
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax

            ixmin = 1
            ixmax = nx 
            iymin = yi_offset(myid_i+1) + 1
            iymax = yi_offset(myid_i+1) + yi_size(myid_i+1)
            izmin = 1
            izmax = 1

            do ix=ixmin,ixmax
                do iy=iymin,iymax 
                    do iz=izmin,izmax 
                        do iv=1,nvar 
                            if (isnan(uu(ix,iy,iz,iv)) == .True.) then 
                                is_Nan = 1
                                exit
                            endif
                        enddo 
                    enddo 
                enddo 
            enddo 

            call MPI_Allreduce(is_Nan, isNanAll, 1, MPI_INTEGER, &
                MPI_MAX, MPI_COMM_WORLD, ierr)
            
        endsubroutine
end program mhd
