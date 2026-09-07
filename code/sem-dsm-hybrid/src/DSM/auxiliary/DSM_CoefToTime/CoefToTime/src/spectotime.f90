program spectotime

   use spectotime_par
   use constants
   implicit none
#ifdef USE_MPI
  include "mpif.h"
#endif

   integer ::ier,idepth,order_l,l_local,itheta,ifreq
!   complex(kind=8) ::omega
   double precision ::distance
#ifdef USE_MPI
   call initialize()
#endif

!Depth from 2 to ndepth-1
   call read_para()
   call partition_job(itheta_start,itheta_end,ntheta_thisset,&
                    distset_id,myrank,nproc,ntheta_total)
   call allocate_array()
   call read_other_info()
#ifdef USE_MPI
      call MPI_BARRIER(MPI_COMM_WORLD,ier)
#endif

   if(myrank.eq.0) then
       call output_par(time_series_length,2*ismooth*nfrequency,&
                      npt_eachpack,ndepth,dt)
   end if
   !do the order l
#ifdef USE_MPI
      call MPI_BARRIER(MPI_COMM_WORLD,ier)
#endif
   TopBot=.true.
     do order_l=0,lmax
     !do each distance
        !read coefficients for idepth and ifreq
        call check_read_spec(order_l,l_local,nl_eachpack,&
                             TopBot,ifreq_start,fluid)
       do itheta=1,ntheta_thisset
         distance=theta(itheta)*PI/180.0
         !compute variables depending on distance and l, but not on depth
         call comp_para_givendist(source_type,distance,order_l)
         !do the ifrequency
         do ifreq=ifreq_start(l_local),nfrequency
           omega=dcmplx( 2.d0*PI*dble(ifreq)/dble(time_series_length),&
                   -omega_imag)
           !do each depth
           do idepth=1,ndepth
             !compute the contributions from the order l
             call comp_disp_strain(idepth,ifreq,itheta,order_l,&
                                   l_local,omega,myrank)
             call sum_disp_strain(order_l,idepth,ifreq,itheta,myrank)


           end do !end idepth
          end do !end ifreq
        end do  !end itheta
      end do  !end order l
   !do fft
   if(.not.fluid) call comp_stress(ndepth,ntheta_thisset,nfrequency,myrank)
   call do_fft(ndepth,ntheta_thisset,myrank)
  
   !save the array
   call save_array(ndepth,ntheta_thisset,npt_eachpack,npack,npt_save,&
                      distset_id,fluid,TopBot)


#ifdef USE_MPI
   call MPI_FINALIZE(ier)
#endif

end program
