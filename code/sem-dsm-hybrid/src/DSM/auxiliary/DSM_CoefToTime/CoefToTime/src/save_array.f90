subroutine output_par(time_series_length,npt,npt_eachpack,ndepth_total,&
                      dt)
implicit none
integer ::npt,npt_eachpack,ndepth_total
double precision  ::time_series_length,dt

!local parameters
character(len=80)::par_file
 
write(par_file,"('OUTPUT_FILES/Par_file')")
open(unit=17,file=par_file,status='unknown',form='formatted',action='write')
  write(17,*)dt,npt,npt_eachpack
  write(17,*)ndepth_total
close(17)

end subroutine


subroutine save_array(ndepth_save,ntheta_save,npt_eachpack,npack,npt_save,&
                      distset_id,fluid,TopBot)
use spectotime_par,only: array_pressure,array_chi_dot,array_disp_fluid,array_strain,&
                        array_disp_solid,ncomp_disp,ncomp_strain,&
                        itheta_start,itheta_end,dt

!input
implicit none
integer,intent(in) ::ntheta_save,ndepth_save,npt_eachpack,&
                     npack,distset_id,npt_save
logical,intent(in) ::fluid,TopBot


!local parameters
character(len=80)::green_fluid_disp,green_fluid_pres,green_fluid_chi_dot,&
                   green_solid_velo,green_solid_stre
integer ::idepth,itheta,icomp
integer ::it_start_thispack,it_end_thispack,ipack,it,it_taper_end
integer ::itime
!integer ::i,j

!it_taper_end=it_start_thispack+int(5.0/dt)
 do ipack=1,npack
   if(fluid) then
      if(TopBot) then
       write(green_fluid_disp,"('OUTPUT_FILES/fluid_disp/TopBotset',& 
                              i3.3'_package',i3.3)")distset_id,ipack
       write(green_fluid_pres,"('OUTPUT_FILES/pressure/TopBotset',&
                              i3.3'_package',i3.3)")distset_id,ipack
       write(green_fluid_chi_dot,"('OUTPUT_FILES/chi_dot/TopBotset',&
                              i3.3'_package',i3.3)")distset_id,ipack
      else
       write(green_fluid_disp,"('OUTPUT_FILES/fluid_disp/Middleset',&
                              i3.3'_package',i3.3)")distset_id,ipack
       write(green_fluid_pres,"('OUTPUT_FILES/pressure/Middleset',&
                              i3.3'_package',i3.3)")distset_id,ipack
       write(green_fluid_chi_dot,"('OUTPUT_FILES/chi_dot/Middleset',&
                              i3.3'_package',i3.3)")distset_id,ipack
      end if
!add taper
!      if(ipack.eq.1) then
!             call add_taper(array_pressure,npt_save,ntheta_save,1,&
!                            ndepth_save,it_start_thispack,it_taper_end)
!             call add_taper(array_disp_fluid,npt_save,ntheta_save,ncomp_disp,&
!                            ndepth_save,it_start_thispack,it_taper_end)
!      end if
      open(unit=14,file=green_fluid_chi_dot,form='unformatted',status='unknown',action='write')
      open(unit=15,file=green_fluid_pres,form='unformatted',status='unknown',action='write')
      open(unit=16,file=green_fluid_disp,form='unformatted',status='unknown',action='write')


!      open(unit=14,file=green_fluid_chi_dot,status='unknown',action='write')
!      open(unit=15,file=green_fluid_pres,status='unknown',action='write')
!      write(16,*) itheta_start,itheta_end

      write(14) itheta_start,itheta_end
      write(15) itheta_start,itheta_end
      write(16) itheta_start,itheta_end

   else
      if(TopBot) then
       write(green_solid_velo,"('OUTPUT_FILES/solid_velo/TopBotset',&
                              i3.3'_package',i3.3)")distset_id,ipack
       write(green_solid_stre,"('OUTPUT_FILES/stress/TopBotset',&
                              i3.3'_package',i3.3)")distset_id,ipack
      else
       write(green_solid_velo,"('OUTPUT_FILES/solid_velo/Middleset',&
                              i3.3'_package',i3.3)")distset_id,ipack
       write(green_solid_stre,"('OUTPUT_FILES/stress/Middleset',&
                              i3.3'_package',i3.3)")distset_id,ipack
      end if
!WENBO
!      if(ipack.eq.1) then
!             call add_taper(array_disp_solid,npt_save,ntheta_save,ncomp_disp,&
!                            ndepth_save,it_start_thispack,it_taper_end)
!             call add_taper(array_strain,npt_save,ntheta_save,ncomp_strain,&
!                            ndepth_save,it_start_thispack,it_taper_end)
!      end if

      open(unit=17,file=green_solid_stre,form='unformatted',status='unknown',action='write')
!      open(unit=17,file=green_solid_stre,status='unknown',action='write')
      open(unit=18,file=green_solid_velo,form='unformatted',status='unknown',action='write')
      write(17) itheta_start,itheta_end
      write(18) itheta_start,itheta_end
   end if
   it_start_thispack=(ipack-1)*(npt_eachpack-1)+1
   it_end_thispack=it_start_thispack+npt_eachpack-1
  
  do idepth=1,ndepth_save
   do itheta=1,ntheta_save
    if(fluid) then
      write(14) array_chi_dot(it_start_thispack:it_end_thispack,idepth,itheta)
      write(15) array_pressure(it_start_thispack:it_end_thispack,idepth,itheta)

      do it=it_start_thispack,it_end_thispack
!        if(itheta.eq.1.and.itheta_start.eq.1) then
           if(itheta+itheta_start-1.eq.96) then
            write(*,*) 'out_chi_dot',it,array_chi_dot(it,idepth,itheta)
            write(*,*) 'out_pressure',it,array_pressure(it,idepth,itheta)
        end if
      end do

      do icomp=1,ncomp_disp
         do it=it_start_thispack,it_end_thispack
!           if(itheta.eq.1.and.itheta_start.eq.1) then
           if(itheta+itheta_start-1.eq.96) then
            write(*,*) 'out_disp_fluid',it,array_disp_fluid(it,idepth,itheta,icomp),icomp
           end if
         end do

        write(16) array_disp_fluid(it_start_thispack:it_end_thispack,&
                                   idepth,itheta,icomp)
      end do
    else
!      open(unit=15,file=green_file,form='unformatted',status='unknown',action='write')
!      open(unit=17,file=green_solid_stre,status='unknown',action='write')
!      open(unit=18,file=green_solid_velo,status='unknown',action='write')
      do icomp=1,ncomp_strain
       write(17)array_strain(it_start_thispack:it_end_thispack,idepth,itheta,icomp)
       do it=it_start_thispack,it_end_thispack
          if(itheta.eq.3.and.itheta_start.eq.1) &
            write(*,*) 'out_strain',array_strain(it,idepth,itheta,icomp),icomp,it

!        write(17,*) array_strain(it,idepth,itheta,icomp)
       end do

      end do
      do icomp=1,ncomp_disp
       do it=it_start_thispack,it_end_thispack
        if(itheta.eq.1.and.itheta_start.eq.1) &
            write(*,*) 'out_velo',it,array_disp_solid(it,idepth,itheta,icomp),icomp
!        write(18,*) array_disp_solid(it,idepth,itheta,icomp)
       end do
        write(18) array_disp_solid(it_start_thispack:it_end_thispack,idepth,itheta,icomp)
      end do
    end if
   end do  !itheta
  end do  !idepth
  if(fluid) then
     close(14)
     close(15)
     close(16)
  else
     close(17)
     close(18)
  end if
end do

end subroutine save_array


!*****************add taper at the begining of records******************
subroutine  add_taper(array,npt,ntheta,ncomp,ndepth,it_start,it_taper_end)
use constants, only:PI
implicit none
integer ::npt,ntheta,ncomp,ndepth
real(kind=4), dimension(npt,ndepth,ntheta,ncomp) :: array
integer ::it_start,it_taper_end

!other local variables
integer ::it,icomp,itheta,idepth

if(it_taper_end.gt.npt) stop 'Error in add_taper, it_taper_end > npt!'
do idepth=1,ndepth
 do icomp=1,ncomp
   do itheta=1,ntheta
     do it=it_start,it_taper_end-1
       array(it,idepth,itheta,icomp)=array(it_taper_end,idepth,itheta,icomp)* &
                     (1.0-dcos(dble(it-it_start)/dble(it_taper_end-it_start)*PI))/2.0
     end do
   end do
 end do
end do
end subroutine
