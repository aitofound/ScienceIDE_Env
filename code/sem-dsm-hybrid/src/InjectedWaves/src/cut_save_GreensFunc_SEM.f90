subroutine cut_save_GreensFunc_SEM(idepth,idepth_end,media_type,source_comp_name,&
      icomp_source,ncomp_source,dir_output)
use extract_Injectedwaves_par, only:depths_SEM,ndistances_SEM,distances_SEM,&
      disp_acous_SEM,pressure_SEM,chi_dot_SEM,disp_acous_time_SEM,pressure_time_SEM,&
      chi_dot_time_SEM,disp_acous_DSM,pressure_DSM,potential_DSM,disp_acous_fit_DSM,&
      pressure_fit_DSM,potential_fit_DSM,depths_acous_DSM,velo_elas_SEM,&
      stress_SEM,velo_elas_time_SEM,stress_time_SEM,velo_elas_DSM,stress_DSM,&
      velo_elas_fit_DSM,stress_fit_DSM,depths_elas_DSM,dt_DSM_refined,npt_eachpack,&
      final_tbegin_save,final_tend_save,myrank,output_para_file,tbegin_save,tend_save,&
      npt_save,npackage_save,zone_acous_DSM,zone_elas_DSM
implicit none

integer, intent(in) ::idepth,idepth_end,media_type,icomp_source,ncomp_source
character(len=80), intent(in) ::source_comp_name
character(len=256), intent(in) ::dir_output

!local parameters
integer ::ipt_begin_save,npt_save_estimate

! The index from which to cut the waveforms
ipt_begin_save=int(tbegin_save/dt_DSM_refined)
npt_save_estimate=int((tend_save-tbegin_save)/dt_DSM_refined)
! Save data into 'npackage_save' packages
npackage_save=int(npt_save_estimate/(npt_eachpack-1))+1
! Number of data points to cut
npt_save=npackage_save*npt_eachpack
final_tbegin_save=(ipt_begin_save-1)*dt_DSM_refined
final_tend_save=final_tbegin_save+(npt_save-1)*dt_DSM_refined


! Save the output parameters only once (only solid or fluid media) or twice (both)
if(icomp_source.eq.1.and.myrank.eq.0.and.idepth.eq.1) then
  call save_output_para(dir_output,npt_save,npackage_save,npt_eachpack,output_para_file,&
                        dt_DSM_refined,final_tbegin_save,final_tend_save)
end if

!save the cut waveforms 
call save_GreensFunc_SEM(dir_output,media_type,idepth,ipt_begin_save,npt_save, &
                       npackage_save,npt_eachpack,source_comp_name)



! All the source components are done and release the below arrays' memories.
if(media_type.eq.1.and.idepth.eq.idepth_end.and.icomp_source.eq.ncomp_source) then
  ! Fluid media for this source component is done and release the SEM arrays
!  deallocate(depths_SEM)
!  deallocate(ndistances_SEM)
!  deallocate(distances_SEM)
  deallocate(disp_acous_SEM)
  deallocate(pressure_SEM)
  deallocate(chi_dot_SEM)
  deallocate(disp_acous_time_SEM)
  deallocate(pressure_time_SEM)
  deallocate(chi_dot_time_SEM)
  ! All the source components are done and release the DSM relevant arrays.
  deallocate(disp_acous_DSM)
  deallocate(pressure_DSM)
  deallocate(potential_DSM)
  deallocate(disp_acous_fit_DSM)
  deallocate(pressure_fit_DSM)
  deallocate(potential_fit_DSM)
  deallocate(depths_acous_DSM)
  deallocate(zone_acous_DSM)
end if
! All the source components are done and release the below arrays' memories.
if(media_type.eq.2.and.idepth.eq.idepth_end.and.icomp_source.eq.ncomp_source) then
  !deallocate(depths_SEM)
  !deallocate(ndistances_SEM)
  !deallocate(distances_SEM)
  deallocate(velo_elas_SEM)
  deallocate(stress_SEM)
  deallocate(velo_elas_time_SEM)
  deallocate(stress_time_SEM)
  ! All the source components are done and release the DSM relevant arrays.
  deallocate(velo_elas_DSM)
  deallocate(stress_DSM)
  deallocate(velo_elas_fit_DSM)
  deallocate(stress_fit_DSM)
  deallocate(depths_elas_DSM)
  deallocate(zone_elas_DSM)
end if
end subroutine


subroutine save_output_para(dir_output,npt_save,npackage_save,npt_eachpack,output_para_file,&
                        dt_DSM_refined,final_tbegin_save,final_tend_save)

implicit none
character(len=256)::dir_output,output_para_file
integer ::npt_save,npt_eachpack,npackage_save
double precision  ::dt_DSM_refined,final_tbegin_save,final_tend_save

!local parameters
character(len=256)::output_file
integer ::ios

output_file=trim(dir_output(1:len_trim(dir_output))) // '/' // &
        trim(output_para_file(1:len_trim(output_para_file)))

open(unit=17,file=output_file,status='unknown',form='formatted',action='write',iostat=ios)
if(ios /= 0) then
   print *,'error:could not open output parameter file'
   call exit_mpi('error opening output parameter file')
end if

write(17,"('dt_Green = ',F16.10)") dt_DSM_refined
write(17,"('npt_Green =', I8)") npt_save
write(17,"('npackage_Green =', I8)") npackage_save
write(17,"('npt_eachpack_Green =',I8)") npt_eachpack
write(17,"('t_begin_Green =',F16.10)") final_tbegin_save
write(17,"('t_end_Green =',F16.10)") final_tend_save
close(17)
end subroutine


subroutine save_GreensFunc_SEM(dir_output,media_type,idepth,ipt_begin_save,npt_save, &
                       npackage_save,npt_eachpack,source_comp_name)

use extract_Injectedwaves_par, only:sub_dir_disp_fluid,sub_dir_pressure,sub_dir_chi_dot,&
    sub_dir_velo_solid, sub_dir_stress,ncomp_disp,ncomp_velo,ncomp_stress,&
    ndistances_SEM,disp_acous_time_SEM,pressure_time_SEM,chi_dot_time_SEM,&
    velo_elas_time_SEM,stress_time_SEM,ismooth,nfrequency
implicit none
character(len=256)::dir_output
character(len=80), intent(in) ::source_comp_name
integer, intent(in) ::media_type,idepth,ipt_begin_save,npt_save,npackage_save,npt_eachpack


!local parameters
character(len=256)::output_file,idepth_ipackage_file,full_dir_output
integer ::ipackage,icomp,it_start,it_end,idist,ios
integer ::it

do ipackage=1,npackage_save
   write(idepth_ipackage_file,"('depth'i5.5,'_package',i5.5)")idepth,ipackage
   it_start=(ipackage-1)*(npt_eachpack-1)+1+ipt_begin_save
   it_end=it_start+npt_eachpack-1

   ! Fluid media
   if(media_type.eq.1) then
     ! Save displacement
     full_dir_output=trim(dir_output(1:len_trim(dir_output))) // '/' // &
        trim(source_comp_name(1:len_trim(source_comp_name))) // '/' // &
        trim(sub_dir_disp_fluid(1:len_trim(sub_dir_disp_fluid)))
     output_file=trim(full_dir_output(1:len_trim(full_dir_output))) // '/' // &
        trim(idepth_ipackage_file(1:len_trim(idepth_ipackage_file)))
     open(unit=15,file=output_file,form='unformatted',status='unknown',action='write',iostat=ios)
     if(ios /= 0) then
       call exit_mpi('error opening output parameter file')
     end if
     do icomp=1,ncomp_disp
        do idist=1,ndistances_SEM(idepth)
          write(15) disp_acous_time_SEM(it_start:it_end,idist,icomp)
        end do
     end do
     close(15)
     output_file=trim(full_dir_output(1:len_trim(full_dir_output))) // '/ascii_' // &
        trim(idepth_ipackage_file(1:len_trim(idepth_ipackage_file)))
     open(unit=15,file=output_file,status='unknown',action='write')
     do icomp=1,ncomp_disp
      do it=it_start,it_end
         ! 1D_iasp91_receiver_side_long_period_1layer
         write(15,*) disp_acous_time_SEM(it,1,icomp),icomp
      end do
     end do
     close(15)


     ! Save pressure
     full_dir_output=trim(dir_output(1:len_trim(dir_output))) // '/' // &
        trim(source_comp_name(1:len_trim(source_comp_name))) // '/' // &
        trim(sub_dir_pressure(1:len_trim(sub_dir_pressure)))
     output_file=trim(full_dir_output(1:len_trim(full_dir_output))) // '/' // &
        trim(idepth_ipackage_file(1:len_trim(idepth_ipackage_file)))
     open(unit=15,file=output_file,form='unformatted',status='unknown',action='write',iostat=ios)
     if(ios /= 0) then
       print *,'error:could not open output pressure file'
       call exit_mpi('error opening output parameter file')
     end if
     do idist=1,ndistances_SEM(idepth)
       write(15) pressure_time_SEM(it_start:it_end,idist)
     end do
     close(15)
     output_file=trim(full_dir_output(1:len_trim(full_dir_output))) // '/ascii_'// &
        trim(idepth_ipackage_file(1:len_trim(idepth_ipackage_file)))
     open(unit=15,file=output_file,status='unknown',action='write')
     do it=it_start,it_end
         !1D_iasp91_receiver_side_long_period_1layer
         write(15,*) pressure_time_SEM(it,1),icomp
     end do
     close(15)

     ! Save chi_dot (parameter name in SEM) or pottential (parameter name in DSM)
     full_dir_output=trim(dir_output(1:len_trim(dir_output))) // '/' // &
        trim(source_comp_name(1:len_trim(source_comp_name))) // '/' // &
        trim(sub_dir_chi_dot(1:len_trim(sub_dir_chi_dot)))
     output_file=trim(full_dir_output(1:len_trim(full_dir_output))) // '/' // &
        trim(idepth_ipackage_file(1:len_trim(idepth_ipackage_file)))
     open(unit=15,file=output_file,form='unformatted',status='unknown',action='write',iostat=ios)
     if(ios /= 0) then
       print *,'error:could not open output chi_dot file'
       call exit_mpi('error opening output parameter file')
     end if
     do idist=1,ndistances_SEM(idepth)
       write(15) chi_dot_time_SEM(it_start:it_end,idist)
     end do
     close(15)
     output_file=trim(full_dir_output(1:len_trim(full_dir_output))) // '/ascii_'// &
        trim(idepth_ipackage_file(1:len_trim(idepth_ipackage_file)))
     open(unit=15,file=output_file,status='unknown',action='write')
     do it=it_start,it_end
         !1D_iasp91_receiver_side_long_period_1layer
         write(15,*) chi_dot_time_SEM(it,1),icomp
     end do
     close(15)

   ! Solid media
   else 
     ! Save velocity seismograms
     full_dir_output=trim(dir_output(1:len_trim(dir_output))) // '/' // &
        trim(source_comp_name(1:len_trim(source_comp_name))) // '/' // &
        trim(sub_dir_velo_solid(1:len_trim(sub_dir_velo_solid)))
     output_file=trim(full_dir_output(1:len_trim(full_dir_output))) // '/' // &
        trim(idepth_ipackage_file(1:len_trim(idepth_ipackage_file)))
     open(unit=15,file=output_file,form='unformatted',status='unknown',action='write',iostat=ios)
     if(ios /= 0) then
       print *,'error:could not open output velo_solid file'
       call exit_mpi('error opening output parameter file')
     end if
     do icomp=1,ncomp_velo
        do idist=1,ndistances_SEM(idepth)
          write(15) velo_elas_time_SEM(it_start:it_end,idist,icomp)
        end do
     end do
     close(15)
     output_file=trim(full_dir_output(1:len_trim(full_dir_output))) // '/ascii_' // &
        trim(idepth_ipackage_file(1:len_trim(idepth_ipackage_file)))
     open(unit=15,file=output_file,status='unknown',action='write')
     do icomp=1,ncomp_velo
      do it=it_start,it_end
         !1D_iasp91_receiver_side_long_period_1layer
         write(15,*) velo_elas_time_SEM(it,1,icomp),icomp
         !do idist=1,ndistances_SEM(idepth)
         !   write(15,*) velo_elas_time_SEM(it,idist,icomp),icomp
         !end do
      end do
     end do
     close(15)


     ! Save stress
     full_dir_output=trim(dir_output(1:len_trim(dir_output))) // '/' // &
        trim(source_comp_name(1:len_trim(source_comp_name))) // '/' // &
        trim(sub_dir_stress(1:len_trim(sub_dir_stress)))
     output_file=trim(full_dir_output(1:len_trim(full_dir_output))) // '/' // &
        trim(idepth_ipackage_file(1:len_trim(idepth_ipackage_file)))
     open(unit=15,file=output_file,form='unformatted',status='unknown',action='write',iostat=ios)
     if(ios /= 0) then
       print *,'error:could not open output stress file'
       call exit_mpi('error opening output parameter file')
     end if
     do icomp=1,ncomp_stress
        do idist=1,ndistances_SEM(idepth)
          write(15) stress_time_SEM(it_start:it_end,idist,icomp)
!          write(15,*) stress_time_SEM(it_start:it_end,idist,icomp)
        end do
     end do
     close(15)
     output_file=trim(full_dir_output(1:len_trim(full_dir_output))) // '/ascii_' // &
        trim(idepth_ipackage_file(1:len_trim(idepth_ipackage_file)))
     open(unit=15,file=output_file,status='unknown',action='write')
     do icomp=1,ncomp_stress
      do it=it_start,it_end
         !1D_iasp91_receiver_side_long_period_1layer
         write(15,*) stress_time_SEM(it,1,icomp),icomp
         !do idist=1,ndistances_SEM(idepth)
         !   write(15,*) stress_time_SEM(it,idist,icomp),icomp,idist
         !end do

      end do
     end do
     close(15)


   end if
end do

end subroutine save_GreensFunc_SEM

