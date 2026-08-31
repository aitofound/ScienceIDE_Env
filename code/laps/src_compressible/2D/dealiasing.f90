module dealiasing
    use parallel
    use mhdinit, only: uu_fourier,nx,ny,nz,wave_number_x,&
        wave_number_y,Lx,Ly,pi

    implicit none 

    real,parameter :: dealias_circle_radius = 1./3.
    integer :: dealias_option = 2

    real :: afx = 0.495, afy = 0.495
    real,allocatable, dimension(:) :: filtx, filty

    contains

        subroutine dealias_initialize
            implicit none 

            real :: aj, bj, cj, wav_len
            integer i

            integer :: ix,iy,iz,ivar
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax

            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = 1
            iymax = ny

            izmin = 1 
            izmax = 1

            if (dealias_option == 2 ) then 
                allocate(filtx(ixmin:ixmax))  
                allocate(filty(iymin:iymax)) 

                aj = (5.0 + 6.0 * afx ) / 8.0
                bj = (1.0 + 2.0 * afx ) / 2.0 
                cj =-(1.0 - 2*afx) / 8.0

                do ix = ixmin, ixmax 
                    wav_len = wave_number_x(ix) * Lx / nx
                    filtx(ix) = (aj + bj * cos(wav_len) + cj * cos(2*wav_len)) / &
                        (1 + 2 * afx * cos(wav_len))
                enddo


                aj = (5.0 + 6.0 * afy ) / 8.0
                bj = (1.0 + 2.0 * afy ) / 2.0 
                cj =-(1.0 - 2*afy) / 8.0

                do iy = iymin, iymax 
                    wav_len = wave_number_y(iy) * Ly / ny
                    filty(iy) = (aj + bj * cos(wav_len) + cj * cos(2*wav_len)) / &
                        (1 + 2 * afy * cos(wav_len))
                enddo
            endif
            
        end subroutine dealias_initialize

        subroutine dealias
            implicit none 

            integer :: ix,iy,iz,ivar
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax
            real :: radius,rad_x,rad_y
            
            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = 1
            iymax = ny

            izmin = 1 
            izmax = 1

            if (dealias_option == 1) then 
                !circular padding
                do iz = izmin,izmax
                    do iy = iymin,iymax
                        do ix = ixmin,ixmax 
                            radius = sqrt((wave_number_x(ix) * Lx / (2*pi*nx))**2 + &
                                (wave_number_y(iy) * Ly / (2*pi*ny))**2)
                            if (radius > dealias_circle_radius ) then 
                                uu_fourier(ix,iy,iz,:) = cmplx(0,0)
                            endif
                        enddo
                    enddo
                enddo
            else if (dealias_option == 2) then 
                do ivar = 1, 8
                    do iz = izmin,izmax
                        do iy = iymin,iymax
                            do ix = ixmin,ixmax 
                                uu_fourier(ix,iy,iz,ivar) = uu_fourier(ix,iy,iz,ivar) * &
                                    filtx(ix) * filty(iy)
                            enddo
                        enddo
                    enddo
                enddo
            else if (dealias_option == 3) then 
                !square padding
                do iz = izmin,izmax
                    do iy = iymin,iymax
                        rad_y = abs(wave_number_y(iy) * Ly / (2*pi*ny))
                        do ix = ixmin,ixmax 
                            rad_x = abs(wave_number_x(ix) * Lx / (2*pi*nx))
                            
                            if ((rad_x > dealias_circle_radius) .or. &
                                (rad_y > dealias_circle_radius)) then 
                                    uu_fourier(ix,iy,iz,:) = cmplx(0,0)
                            endif

                        enddo 
                    enddo 
                enddo
            endif

        end subroutine dealias 

end module dealiasing