! save the parameters used in the final step of DSM-SEM coupling.
subroutine save_parameter_coupling()
  use constants, only: MAX_STRING_LEN
  use specfem_par, only: DT, NSTEP, LOCAL_PATH, myrank
  use tele_coupling_par
  implicit none

  integer ::nstep_after_decimating,nsection_each_package
  integer ::npackage_local
  character(len=MAX_STRING_LEN) ::final_LOCAL_PATH,clean_LOCAL_PATH

  if(mod(npoints_tele_coupling,NPOINTS_PER_PACK).eq.0) then
    npackage_local=npoints_tele_coupling/NPOINTS_PER_PACK
  else
    npackage_local=int(npoints_tele_coupling/NPOINTS_PER_PACK)+1
  end if

  call sum_all_i(npackage_local,npackages_total_coupling)

  if(myrank.ne.0) return

  clean_LOCAL_PATH = adjustl(LOCAL_PATH)
  final_LOCAL_PATH = clean_LOCAL_PATH(1:len_trim(clean_LOCAL_PATH)) // '/'

  open(unit=Imain_SEM_Par_Coupling,file=trim(final_LOCAL_PATH)//"SEM_Coupling_Par_file", &
       status='replace',action='write',form='formatted')

  write(Imain_SEM_Par_Coupling,*)  'npackage_SEM = ',npackages_total_coupling
  write(Imain_SEM_Par_Coupling,*)  'npoints_per_pack_SEM = ', NPOINTS_PER_PACK
  write(Imain_SEM_Par_Coupling,*)  'nstep_each_section_SEM = ', NSTEP_BETWEEN_OUTPUTBOUND
  nstep_after_decimating=int((NSTEP-1)/DECIMATE_COUPLING)+1
  write(Imain_SEM_Par_Coupling,*)  'total_nstep_SEM = ', nstep_after_decimating
  nsection_each_package=int(nstep_after_decimating/NSTEP_BETWEEN_OUTPUTBOUND)
  if(mod(nstep_after_decimating,NSTEP_BETWEEN_OUTPUTBOUND).ne.0) &
     nsection_each_package=nsection_each_package+1
  write(Imain_SEM_Par_Coupling,*)  'nsection_SEM = ', nsection_each_package
  write(Imain_SEM_Par_Coupling,*)  'deltat_SEM = ', DT*DECIMATE_COUPLING
  close(Imain_SEM_Par_Coupling)

end subroutine save_parameter_coupling
