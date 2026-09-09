subroutine read_par()
   use resave_par
   implicit none
   character(len=80) ::para_file
   integer ::ios,ier
#ifdef USE_MPI
  include "mpif.h"
#endif


!open and read
  para_file="DATA/Par_file"
  open(unit=10,file=trim(para_file),action='read',status="old",iostat=ios)
   if(ios /= 0) then
      print *,'error:could not open parameter file'
      call exit_mpi('error opening parameter file')
   end if
  read(10,*) top_fluid
  read(10,*) bot_fluid
  read(10,*) source_type
  read(10,*) nfrequency
  read(10,*) nl_total
  read(10,*) nl_eachpack
  close(10)

  if(top_fluid) then
    ncomp_foridep(1)=ncomp_fluid
  else
    ncomp_foridep(1)=ncomp_solid
  end if
  if(bot_fluid) then
    ncomp_foridep(2)=ncomp_fluid
  else
    ncomp_foridep(2)=ncomp_solid
  end if

  if(source_type.eq.1) then
       m0=2
  else if(source_type.eq.2) then
       m0=1
  end if

#ifdef USE_MPI
 call MPI_BARRIER(MPI_COMM_WORLD,ier)
#endif


end subroutine 


subroutine read_coef(l_start,l_end)
   use resave_par, only:nfrequency,ncomp_foridep,m0,&
              coef_c_read,coef_dcdr_read,coef_c_top,coef_dcdr_top,&
              coef_c_bot,coef_dcdr_bot,ifreq_start,myrank
   implicit none

   integer, intent(in) ::l_start,l_end
   
!local parameter
   integer ::i_freq,icomp,idep,m,lmax_computed,l
   character(len=80) ::infile_coef_cAnddcdr
   integer ::ios
   integer ::l_tmp


!local parameter



   do i_freq=nfrequency,1,-1
      write(infile_coef_cAnddcdr, "('./DATA/freq_',i5.5)") i_freq
      open(unit=16,file=trim(infile_coef_cAnddcdr),status='old', &
           form='unformatted',iostat=ios)
      if(ios.ne.0) stop 'error reading coefficients'
      read(16) lmax_computed
      do l=l_start,l_end
         if(l.le.lmax_computed) ifreq_start(l)=i_freq
      end do
      do idep=1,2
       do icomp=1,ncomp_foridep(idep)
        do m=-m0,m0
          coef_c_read(:,:,:,:)=dcmplx(0.d0,0.d0)
          coef_dcdr_read(:,:,:,:)=dcmplx(0.d0,0.d0)
          read(16)coef_c_read(0:lmax_computed,m,idep,icomp)
          read(16)coef_dcdr_read(0:lmax_computed,m,idep,icomp)
          if(i_freq.eq.292.and.m.eq.1.and.icomp.eq.3.and.idep.eq.2) then
            !print *,"coef_c_bot_read",maxval(abs(coef_c_read(0:lmax_computed,m,idep,icomp))),&
            !    i_freq,l_start,lmax_computed,icomp,idep,m
            !do l_tmp=0,lmax_computed
            !  print *,"coef_l",coef_c_read(l_tmp,m,idep,icomp),l_tmp
            !end do

            print *,"coef_dcdr_read",maxval(abs(coef_dcdr_read(0:lmax_computed,m,idep,icomp))),&
                i_freq,l_start,lmax_computed,icomp,idep,m
          end if

          if(idep.eq.1) then
!top
            coef_c_top(m,icomp,i_freq,l_start:l_end)= &
              coef_c_read(l_start:l_end,m,idep,icomp)
            coef_dcdr_top(m,icomp,i_freq,l_start:l_end)= &
              coef_dcdr_read(l_start:l_end,m,idep,icomp)
          else
!bot
            coef_c_bot(m,icomp,i_freq,l_start:l_end)= &
              coef_c_read(l_start:l_end,m,idep,icomp)
            coef_dcdr_bot(m,icomp,i_freq,l_start:l_end)= &
              coef_dcdr_read(l_start:l_end,m,idep,icomp)
            !print *,"coef_c_bot_read",maxval(abs(coef_c_bot(m,icomp,i_freq,l_start:l_end))),i_freq,l_start,lmax_computed
            !print *,"coef_dcdr_read",maxval(abs(coef_dcdr_read(l_start:l_end,m,idep,icomp))),i_freq,l_start,lmax_computed

          end if
        end do !m
       end do !icomp
      end do !idep
      close(16)
   end do !i_freq

   do l=l_start,l_end
    if(ifreq_start(l).lt.0) stop 'Error of ifreq_start'
   end do

end subroutine
