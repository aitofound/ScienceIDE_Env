subroutine save_coef(l_start,l_end)
   use resave_par, only:npack_thisproc,myrank,ifreq_start,&
                        nfrequency,coef_c_top,coef_dcdr_top,coef_c_bot,&
                        coef_dcdr_bot,nproc,nl_eachpack
   implicit none

   integer,intent(in) ::l_start,l_end
!local parameter
   integer ::ipack,lbegin_thispack,lend_thispack,idep,l,ifreq
   character(len=80) ::outfile_coef_cAnddcdr
   integer ::ios
   

!top
   do ipack=1,npack_thisproc
    lbegin_thispack=l_start+(ipack-1)*nl_eachpack
    lend_thispack=lbegin_thispack+nl_eachpack-1
    if(myrank.eq.nproc-1.and.ipack.eq.npack_thisproc) lend_thispack=l_end
    do idep=1,2
      if(idep.eq.1) then
        write(outfile_coef_cAnddcdr, "('./OUTPUT/dep_top/order_',i5.5,'to',i5.5)")&
                                        lbegin_thispack,lend_thispack
      else 
        write(outfile_coef_cAnddcdr, "('./OUTPUT/dep_bot/order_',i5.5,'to',i5.5)")& 
                                        lbegin_thispack,lend_thispack
      end if
      open(unit=16,file=trim(outfile_coef_cAnddcdr),status='unknown', &
           form="unformatted",iostat=ios)
      if(ios.ne.0) stop 'error reading output files'
      do l=lbegin_thispack,lend_thispack
        write(16) ifreq_start(l)
        do ifreq=ifreq_start(l),nfrequency
         if(idep.eq.1) then
          write(16) coef_c_top(:,:,ifreq,l)
          write(16) coef_dcdr_top(:,:,ifreq,l)
         else
          write(16) coef_c_bot(:,:,ifreq,l)
          write(16) coef_dcdr_bot(:,:,ifreq,l)
          !print *,"coef_c_bot",maxval(abs(coef_c_bot(:,:,ifreq,l)))
          !print *,"coef_dcdr_bot",maxval(abs(coef_dcdr_bot(:,:,ifreq,l)))
         end if
        end do !i_freq
      end do !l

      close(16)
     end do !idep

   end do !ipack


!bot 


end subroutine
