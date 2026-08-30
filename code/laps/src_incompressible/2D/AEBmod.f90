module AEBmod
    use mhdinit, only: wave_number_x,wave_number_y,k_square,ny,&
        if_corotating,corotating_angle, cos_cor_ang, sin_cor_ang,&
        rho0,press0,adiabatic_index
    use parallel


    implicit none 

    real :: Ur0 = 0.0, Ur, tau_exp = 0.0
    real :: radius0 = 30.0, radius = 30.0
    

    contains 

        subroutine AEB_initialize
            implicit none 
            logical :: file_exist

            radius = radius0 

            call AEB_calc(radius)

            if (.not. if_corotating) then 
                corotating_angle = 0.0
            endif

            cos_cor_ang = cos(corotating_angle)
            sin_cor_ang = sin(corotating_angle)

            if (ipe==0) then 
                inquire(FILE='EBM_info.dat', exist=file_exist)
                if (file_exist) then 
                    open(FILE='EBM_info.dat',UNIT=30,STATUS='OLD',ACTION='WRITE',POSITION='APPEND')
                else 
                    open(FILE='EBM_info.dat',UNIT=30,STATUS='REPLACE',ACTION='WRITE')
                endif
                close(30)
            endif
        end subroutine AEB_initialize

        subroutine AEB_calc(r)
            implicit none 

            real, intent(in) :: r 

            Ur = Ur0

            tau_exp =  r / Ur  
        end subroutine 

        subroutine evolve_radius(t)
            !note that Ur must be constant 

            implicit none 

            real, intent(in) :: t 

            radius = radius0 + Ur * t
            
            call AEB_calc(radius)

            call update_ksquare
        end subroutine

        subroutine output_AEB(t)
            implicit none 

            real, intent(in)  :: t

            if (ipe==0) then 
                open(FILE='EBM_info.dat',UNIT=30,STATUS='OLD',ACTION='WRITE',POSITION='APPEND')
                    write(30,'(3(1pe16.8))') t, radius, Ur
                close(30)
            endif
        end subroutine output_AEB

        subroutine update_ksquare
            implicit none 

            integer :: ix,iy,iz
            integer :: ixmin, ixmax, iymin, iymax, izmin, izmax
            real :: kx,ky,kz

            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = 1
            iymax = ny

            izmin = 1 
            izmax = 1

            do iz=izmin,izmax
                kz = 0.0
                do iy=iymin,iymax 
                    ky = wave_number_y(iy)
                    do ix=ixmin,ixmax 
                        kx = wave_number_x(ix)

                        if ( if_corotating ) then 
                            k_square(ix,iy,iz) = kx**2 * ( cos_cor_ang**2 + &
                                 (sin_cor_ang * radius0/radius)**2) &
                                + ky**2 * ( sin_cor_ang**2 + &
                                (cos_cor_ang * radius0/radius)**2) + kx * ky * 2 * &
                                cos_cor_ang * sin_cor_ang * (1 - (radius0/radius)**2 )
                        else 
                            k_square(ix,iy,iz) = kx**2 + &
                                (ky * radius0 / radius)**2
                        endif
                    enddo
                enddo
            enddo

        end subroutine update_ksquare

        subroutine update_rho_p
            implicit none 

            ! set pressure
            rho0 = rho0 * (radius0/radius)**2
            press0 = press0 * (radius0/radius)**(2*adiabatic_index)

            ! uu(:,:,:,1) = rho0 * (radius0/radius)**2
            ! !we do not update rho because rho is evolved with expansion term

            ! uu(:,:,:,8) = uu(:,:,:,8) + p0 * (radius0/radius)**(2*adiabatic_index)
        end subroutine update_rho_p
end module AEBmod