module dealiasing
    use parallel
    use mhdinit, only: uu_fourier,nx,ny,nz,wave_number_x,&
        wave_number_y,wave_number_z,Lx,Ly,Lz,pi

    implicit none 

    real,parameter :: dealias_circle_radius = 1./3.
    integer :: dealias_option = 2
    real :: afx = 0.495, afy = 0.495, afz = 0.495
    real,allocatable, dimension(:) :: filtx, filty,filtz
    contains
        subroutine dealias_initialize
            implicit none 

            real :: aj, bj, cj, wav_len
            integer i

            integer :: ix,iy,iz,ivar
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax

            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = yj_offset(myid_j+1) + 1
            iymax = yj_offset(myid_j+1) + yj_size(myid_j+1)

            izmin = 1 
            izmax = nz

            if (dealias_option == 2) then 
                allocate(filtx(ixmin:ixmax))  
                allocate(filty(iymin:iymax)) 
                allocate(filtz(izmin:izmax))

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

                aj = (5.0 + 6.0 * afz ) / 8.0
                bj = (1.0 + 2.0 * afz ) / 2.0 
                cj =-(1.0 - 2*afz) / 8.0

                do iz = izmin, izmax 
                    wav_len = wave_number_z(iz) * Lz / nz
                    filtz(iz) = (aj + bj * cos(wav_len) + cj * cos(2*wav_len)) / &
                        (1 + 2 * afz * cos(wav_len))
                enddo
            endif
            
        end subroutine dealias_initialize


        subroutine dealias
            implicit none 

            integer :: ix,iy,iz,ivar
            integer :: ixmin,ixmax,iymin,iymax,izmin,izmax
            real :: radius
            
            ixmin = xi_offset(myid_i+1) + 1
            ixmax = xi_offset(myid_i+1) + xi_size(myid_i+1)

            iymin = yj_offset(myid_j+1) + 1
            iymax = yj_offset(myid_j+1) + yj_size(myid_j+1)

            izmin = 1 
            izmax = nz


            if (dealias_option == 1) then 
                do iz = izmin,izmax
                    do iy = iymin,iymax
                        do ix = ixmin,ixmax 
                            radius = sqrt((wave_number_x(ix) * Lx / (2*pi*nx))**2 + &
                                (wave_number_y(iy) * Ly / (2*pi*ny))**2 + &
                                (wave_number_z(iz) * Lz / (2*pi*nz))**2)
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
                                    filtx(ix) * filty(iy) * filtz(iz)
                            enddo
                        enddo
                    enddo
                enddo
            endif

        end subroutine dealias

end module dealiasing