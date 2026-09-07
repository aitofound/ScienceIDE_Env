subroutine teleseismic_coupling() 
  use specfem_par
  use specfem_par_elastic
  use specfem_par_acoustic
  use specfem_par_poroelastic
  use tele_coupling_par
  use constants, only: DEBUG_COUPLING
  implicit none
  integer ::it_coupling

  if(it.eq.1) then
     call compute_pml_rotation_matrix(myrank)
     call get_tele_coupling_surfaces()
  end if
! if(DEBUG_COUPLING) print *,'calculate_strain_ela start',myrank,it
  if(DECIMATE_COUPLING.eq.1.or.mod(it,DECIMATE_COUPLING).eq.1) then
     it_coupling=(it-1)/DECIMATE_COUPLING+1
     call compute_traction_disp_elastic()
     call compute_pres_disp_acoustic()
   else
     it_coupling=-1
   end if
   !if(DEBUG_COUPLING) print *,'velocity',myrank,it
   if( (it.gt.1.and.mod(it_coupling,NSTEP_BETWEEN_OUTPUTBOUND).eq.0) .or. &
       (it.eq.NSTEP .and. mod((it-1)/DECIMATE_COUPLING+1,NSTEP_BETWEEN_OUTPUTBOUND).gt.0) ) then
        if(COUPLING_TYPE.eq.1.or.COUPLING_TYPE.eq.3) then
          call  save_coupling_disp_traction()
        else if(COUPLING_TYPE.eq.2.and.SINGLE_FORCE_ENZ.ne.0) then
          call save_strain_selected()
        end if
   end if

end subroutine teleseismic_coupling
