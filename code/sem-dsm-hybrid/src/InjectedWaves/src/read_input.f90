subroutine read_para(para_file)
   use extract_Injectedwaves_par, only: source_type,nfrequency,tbegin_save, &
        tend_save,npt_eachpack,flow,fhigh,nfreq_DSM,time_length_DSM,dir_DSM_input,&
        dir_SEM_input,apply_filter,ncomp_source,work_spc,work_time,spc_zero_padding,&
        time_series,data_in,data_filtered,ismooth,ENZ_components,sources_selected,&
        single_force_names,moment_comp_names,explosion_name
   implicit none

! Other variables
   character(len=256) ::para_file
   integer ::ios

   ! para_file="DATA/Par_file"
   open(unit=10,file=trim(para_file),action='read',status="old",iostat=ios)
   if(ios /= 0) then
      print *,'error:could not open parameter file'
      call exit_mpi('error opening parameter file')
   end if
   read(10,*) source_type
   read(10,*) ENZ_components(1), ENZ_components(2), ENZ_components(3)
   read(10,'(A256)') dir_DSM_input
   read(10,'(A256)') dir_SEM_input
   read(10,*) nfrequency
   read(10,*) tbegin_save,tend_save
   read(10,*) npt_eachpack
   read(10,*) apply_filter
   read(10,*) flow,fhigh
   close(10)


   ncomp_source=0
   if(source_type.eq.1) then
      ! If only horizontal component(s) to be simulated, 'Fr' and 'Ft' are selected.
      if((ENZ_components(1).eq.1.or.ENZ_components(2).eq.1).and.ENZ_components(3).eq.0) then
          ncomp_source=2
          allocate(sources_selected(ncomp_source))
          sources_selected(1:2)=single_force_names(1:2)
      ! If three components to be simulated, 'Fr', 'Ft' and 'Fz' are selected.
      else if((ENZ_components(1).eq.1.or.ENZ_components(2).eq.1).and.ENZ_components(3).eq.1) then
          ncomp_source=3 
          allocate(sources_selected(ncomp_source))
          sources_selected(1:ncomp_source)=single_force_names(1:ncomp_source)
      ! If only vertical component to be simulated, only 'Fz' is selected
      else if((ENZ_components(1).eq.0.and.ENZ_components(2).eq.0).and.ENZ_components(3).eq.1) then
          ncomp_source=1
          allocate(sources_selected(ncomp_source))
          sources_selected(1)=single_force_names(3)
      else 
          call exit_mpi('Error in selecting sources, check the input parameter ENZ_components!')
      end if
   else if(source_type.eq.2) then
     ncomp_source=6
     allocate(sources_selected(ncomp_source))
     sources_selected(1:ncomp_source)=moment_comp_names(1:ncomp_source)
   else if(source_type.eq.3) then
     ncomp_source=1
     allocate(sources_selected(ncomp_source))
     sources_selected(1:ncomp_source)=explosion_name(1:ncomp_source)
   else
     call exit_mpi('source_type must be 1 (single force), 2 (moment-tensor Green functions) or 3 (explosion)!')
   end if

   ! Allocate the below temporary variables and they will be used to help IFFT
   ! computions.
   allocate(work_spc(nfrequency))
   allocate(work_time(2*ismooth*nfrequency))
   allocate(spc_zero_padding(2*ismooth*nfrequency))
   allocate(time_series(4*ismooth*nfrequency))
   allocate(data_in(4*ismooth*nfrequency))
   allocate(data_filtered(4*ismooth*nfrequency))
end subroutine



!******************************************read DSM****************************************
! Read DSM input parameters
subroutine read_DSM_para(dir_DSM_input)
  use extract_Injectedwaves_par, only:nfreq_DSM,ndep_acous_DSM,ndep_elas_DSM,&
            min_theta_DSM,dtheta_DSM,ntheta_DSM,time_length_DSM,omega_imag,&
            npt_elas_DSM,npt_acous_DSM,velo_elas_DSM,stress_DSM,disp_acous_DSM,&
            pressure_DSM,potential_DSM,disp_acous_fit_DSM,pressure_fit_DSM,&
            potential_fit_DSM,velo_elas_fit_DSM,nfit_dep,max_theta_DSM,&
            stress_fit_DSM,ncomp_disp,ncomp_velo,ncomp_stress,DSM_para_file,&
            depths_acous_DSM,depths_elas_DSM,nfrequency,DSM_acous_depth_file,&
            DSM_elas_depth_file,nfrequency,tend_save,ismooth,dt_DSM_refined,&
            sources_selected,zone_acous_DSM,zone_elas_DSM,depth_tolerance
  implicit none


  character(len=256),intent(in) ::dir_DSM_input
  ! local parameters
  double precision ::ddep_tmp,depth_tole_tmp
  character(len=256) ::first_source_comp_name
  character(len=256) ::dir_first_src_comp
  character(len=256) ::para_file,depth_file
  integer ::ios,idepth,ndep_tmp
  integer ::ndepths_this_zone,ndepths_last_zone,izone_this_dep,izone_last_dep
 
  ! Parameters in DSM_Par_file are the same, so just read it one time from the
  ! first component.
  first_source_comp_name=sources_selected(1)
  dir_first_src_comp=trim(dir_DSM_input(1:len_trim(dir_DSM_input)))// '/' &
            //trim(first_source_comp_name(1:len_trim(first_source_comp_name)))

  ! Read $DSM_input/M11/OUTPUT_FILES/DSM_Par_file
  para_file=trim(dir_first_src_comp(1:len_trim(dir_first_src_comp)))// &
            '/OUTPUT_FILES/'//trim(DSM_para_file(1:len_trim(DSM_para_file)))
  open(unit=11,file=trim(para_file),status='unknown',form='formatted',iostat=ios)
  if(ios /= 0) stop 'Error in opening file DSM_Par_file'
  read(11,*)nfreq_DSM
  read(11,*)ndep_acous_DSM,ndep_elas_DSM
  read(11,*)min_theta_DSM,dtheta_DSM,ntheta_DSM
  read(11,*)time_length_DSM
  read(11,*)omega_imag
  close(11)
  max_theta_DSM=min_theta_DSM+(ntheta_DSM-1)*dtheta_DSM
  npt_elas_DSM=ndep_elas_DSM*ntheta_DSM
  npt_acous_DSM=ndep_acous_DSM*ntheta_DSM

  ! Check validity
  ! nfrequency must be smaller than nfreq_DSM
  if(nfrequency.gt.nfreq_DSM) call exit_mpi('Error, nfrequency is larger than nfreq_DSM!')
  ! Note that only the first nfrequency are used, although nfreq_DSM
  ! frequencies are available.
  dt_DSM_refined=time_length_DSM/nfrequency/(2.0*ismooth)
  if(tend_save.gt.time_length_DSM) call exit_mpi('Error, tend_save is larger than time_length_DSM!')


  ! Allocate arrays for DSM database 
  allocate(velo_elas_DSM(npt_elas_DSM))
  allocate(stress_DSM(npt_elas_DSM))
  allocate(disp_acous_DSM(npt_acous_DSM))
  allocate(pressure_DSM(npt_acous_DSM))
  allocate(potential_DSM(npt_acous_DSM))

  ! Allocate arrays for DSM Greens functions at idepth
  allocate(disp_acous_fit_DSM(nfrequency,nfit_dep,ntheta_DSM,ncomp_disp))
  allocate(pressure_fit_DSM(nfrequency,nfit_dep,ntheta_DSM))
  allocate(potential_fit_DSM(nfrequency,nfit_dep,ntheta_DSM))
  allocate(velo_elas_fit_DSM(nfrequency,nfit_dep,ntheta_DSM,ncomp_velo))
  allocate(stress_fit_DSM(nfrequency,nfit_dep,ntheta_DSM,ncomp_stress))

  ! Fluid media************************
  ! Prefer the DSM tables saved with the component DATA directory. Keep a
  ! fallback to the historical component-root location for old databases.
  depth_file=trim(dir_first_src_comp(1:len_trim(dir_first_src_comp)))// '/DATA/'  //&
             trim(DSM_acous_depth_file(1:len_trim(DSM_acous_depth_file)))
  open( unit=12, file=depth_file, status='old', iostat=ios )
  if(ios /= 0) then
     depth_file=trim(dir_first_src_comp(1:len_trim(dir_first_src_comp)))// '/'  //&
                trim(DSM_acous_depth_file(1:len_trim(DSM_acous_depth_file)))
     open( unit=12, file=depth_file, status='old', iostat=ios )
     if(ios /= 0) stop 'Error in opening DSM acoustic depth file'
  endif
  ! ddep is used in DSM, but not functional here.
  read(12,*) ddep_tmp,depth_tole_tmp
  read(12,*) ndep_tmp
  if(ndep_tmp.ne.ndep_acous_DSM) stop 'Error, two ndep_acous_DSM not consistent'
  allocate(depths_acous_DSM(ndep_acous_DSM))
  allocate(zone_acous_DSM(ndep_acous_DSM))
  do idepth=1,ndep_acous_DSM
     read(12,*)depths_acous_DSM(idepth),zone_acous_DSM(idepth)
  end do
  close(12)


  ! Check validity
  ndepths_last_zone=0
  do idepth=1,ndep_acous_DSM
     izone_this_dep=zone_acous_DSM(idepth)
     if(idepth.eq.1) then
       izone_last_dep=zone_acous_DSM(idepth)
     else
       izone_last_dep=zone_acous_DSM(idepth-1)
     end if
     ! Reach discontinuity and enter a new zone
     if(izone_this_dep.ne.izone_last_dep) then
       if(dabs(depths_acous_DSM(idepth)-depths_acous_DSM(idepth-1)).gt.depth_tolerance) &
         stop 'Error, two (acoustic) depths (the same number but with different zone numbers) &
               should be set at a discontinuity!'
          ndepths_last_zone=ndepths_this_zone
       if(ndepths_last_zone.lt.nfit_dep) &
         stop 'Error, the number of depth in one (acoustic) zone should be larger than nfit_dep!'
       ndepths_this_zone=0
     end if
     ndepths_this_zone=ndepths_this_zone+1
     if(idepth.eq.ndep_acous_DSM.and.ndepths_this_zone.lt.nfit_dep) &
         stop 'Error, the number of depth in the last (acoustic) zone should be larger than nfit_dep!'
  end do
  
  ! Solid media************************
  ! Read the DSM depth table 
  depth_file=trim(dir_first_src_comp(1:len_trim(dir_first_src_comp)))// '/DATA/' //&
             trim(DSM_elas_depth_file(1:len_trim(DSM_elas_depth_file)))
  open( unit=13, file=depth_file, status='old', iostat=ios )
  if(ios /= 0) then
     depth_file=trim(dir_first_src_comp(1:len_trim(dir_first_src_comp)))// '/' //&
                trim(DSM_elas_depth_file(1:len_trim(DSM_elas_depth_file)))
     open( unit=13, file=depth_file, status='old', iostat=ios )
     if(ios /= 0) stop 'Error in opening DSM elastic depth file'
  endif
  ! ddep_tmp and depth_tole_tmp are used in DSM. Not functional here.
  read(13,*) ddep_tmp,depth_tole_tmp
  read(13,*) ndep_tmp
  if(ndep_tmp.ne.ndep_elas_DSM) stop 'Error, two ndep_acous_DSM not consistent'
  allocate(depths_elas_DSM(ndep_elas_DSM))
  allocate(zone_elas_DSM(ndep_elas_DSM))
  do idepth=1,ndep_elas_DSM
     read(13,*)depths_elas_DSM(idepth),zone_elas_DSM(idepth)
  end do
  close(13)

  ! Check validity
  ndepths_last_zone=0
  do idepth=1,ndep_elas_DSM
     izone_this_dep=zone_elas_DSM(idepth)
     if(idepth.eq.1) then
       izone_last_dep=zone_elas_DSM(idepth)
     else
       izone_last_dep=zone_elas_DSM(idepth-1)
     end if
     ! Reach discontinuity and enter a new zone
     if(izone_this_dep.ne.izone_last_dep) then
       if(dabs(depths_elas_DSM(idepth)-depths_elas_DSM(idepth-1)).gt.depth_tolerance) &
          stop 'Error, two depths (the same number but with different zone numbers) should be set &
               at a (elastic) dicontinuity!'
          ndepths_last_zone=ndepths_this_zone
       if(ndepths_last_zone.lt.nfit_dep) &
          stop 'Error, the number of depth in one (elastic) zone should be larger than nfit_dep!'
       ndepths_this_zone=0
     end if
     ndepths_this_zone=ndepths_this_zone+1
     if(idepth.eq.ndep_elas_DSM.and.ndepths_this_zone.lt.nfit_dep) &
          stop 'Error, the number of depth in the last (elastic) zone should be larger than nfit_dep!'
  end do



end subroutine read_DSM_para

! Read the DSM Green's function database at the idepth
! media_type: 1 for fluid media and 2 for solid meda
! Fluid media: read pressure, potential and displacement
! Solid media: read velocity and stress
subroutine read_DSM_database(idepth,media_type,source_comp_name)
  use extract_Injectedwaves_par, only:disp_acous_DSM,pressure_DSM,potential_DSM,&
               velo_elas_DSM,stress_DSM,dir_DSM_input,ntheta_DSM,disp_acous_fit_DSM,&
               pressure_fit_DSM,potential_fit_DSM,velo_elas_fit_DSM,&
               stress_fit_DSM,nfrequency,nfit_dep,first_idep_fit_DSM,depths_acous_DSM,&
               depths_elas_DSM,depths_SEM,depth_tolerance,zones_SEM,ndep_acous_DSM,ndep_elas_DSM,&
               zone_acous_DSM,zone_elas_DSM
  implicit none

  integer, intent(in) ::idepth,media_type
  character(len=80), intent(in) ::source_comp_name

  ! local parameters
  character(len=256) ::ifreq_file
  character(len=256) ::dir_this_source,dir_DSM_disp,dir_DSM_pres,dir_DSM_potential,&
               dir_DSM_velo,dir_DSM_stress
  
  character(len=256) ::DSM_disp_file,DSM_velo_file,DSM_pressure_file,DSM_potential_file,DSM_stress_file
  integer ::ipt_start,icomp_disp,icomp_velo,icomp_stress,ifreq,ios
  integer ::idep_search,idep_fit,idep_in_DSM
  integer ::idep_start_this_zone,idep_end_this_zone,ndepths_this_zone
  integer, parameter::ncomp_velo=3
  integer, parameter::ncomp_disp=3
  integer, parameter::ncomp_stress=6


  dir_this_source=trim(dir_DSM_input(1:len_trim(dir_DSM_input)))// '/' // &
                   trim(source_comp_name(1:len_trim(source_comp_name)))

  ! Fluid media
  if(media_type.eq.1) then
    ! Find the first DSM depth to be used for interpolation
    
    ! The DSM depths used for interpolation must be in the same zone as depths_SEM(idepth). 
    ! Othwise, (1) the same two depths on the discontinuity cause NaN in the interpolation. 
    ! (2) less accurate interpolation. 

    ! Two steps to get the interpolating DSM depths:

    ! Step 1, find out the first and last DSM detphs of the zone, where
    ! depths_SEM(idepth) is located.
    idep_search=1
    idep_start_this_zone=0
    idep_end_this_zone=0
    do while(idep_search.le.ndep_acous_DSM)
       if(zone_acous_DSM(idep_search).eq.zones_SEM(idepth)) then
           if(idep_search.eq.1) then
             idep_start_this_zone=idep_search
           else if(idep_search.eq.ndep_acous_DSM) then
             idep_end_this_zone=idep_search
           else
             if(zone_acous_DSM(idep_search).eq.zones_SEM(idepth) .and. &
                zone_acous_DSM(idep_search-1).ne.zones_SEM(idepth)) &
                  idep_start_this_zone=idep_search
             if(zone_acous_DSM(idep_search).eq.zones_SEM(idepth) .and. &
                zone_acous_DSM(idep_search+1).ne.zones_SEM(idepth)) &
                  idep_end_this_zone=idep_search
           end if
       end if
       idep_search=idep_search+1
    end do !idep_search
    ndepths_this_zone=idep_end_this_zone-idep_start_this_zone+1
    if(ndepths_this_zone.lt.nfit_dep) stop 'Error, the number of DSM depths in the interpolating &
       zone is smaller than nfit_dep!'

    ! Step 2, search in these depths to get the first interpolating DSM depth
    if(abs(depths_SEM(idepth)-depths_acous_DSM(idep_end_this_zone)).lt.depth_tolerance) then
      first_idep_fit_DSM=idep_end_this_zone
    else
      idep_search=idep_start_this_zone
      do while(depths_acous_DSM(idep_search).le.depths_SEM(idepth)+depth_tolerance.and.&
             idep_search.le.idep_end_this_zone-1)
        idep_search=idep_search+1
      end do
      first_idep_fit_DSM = idep_search-1
    end if
    ! Shift the estimated depth by -nfit_dep/2 such that depths_SEM(idepth) is in
    ! the middle among the nfit_dep interpolating depths
    first_idep_fit_DSM = first_idep_fit_DSM - int(nfit_dep/2.0)
    if(first_idep_fit_DSM.lt.idep_start_this_zone) first_idep_fit_DSM=idep_start_this_zone
    if(first_idep_fit_DSM.gt.idep_end_this_zone-nfit_dep+1) &
         first_idep_fit_DSM=idep_end_this_zone-nfit_dep+1

    do ifreq=1,nfrequency
      write(ifreq_file,"(i5.5)")ifreq-1

      ! Read displacement
      dir_DSM_disp=trim(dir_this_source(1:len_trim(dir_this_source)))//'/OUTPUT_FILES/disp_fluid'
      DSM_disp_file=trim(dir_DSM_disp(1:len_trim(dir_DSM_disp)))// '/freq_' // trim(ifreq_file)
      open(unit=15,file=trim(DSM_disp_file),status='unknown',form='unformatted',iostat=ios)
      if(ios /= 0) stop 'error in reading disp_fluid'
      do icomp_disp=1,ncomp_disp
        read(15) disp_acous_DSM(:)
        do idep_fit=1,nfit_dep
          idep_in_DSM=first_idep_fit_DSM+idep_fit-1
          ipt_start=(idep_in_DSM-1)*ntheta_DSM+1
          disp_acous_fit_DSM(ifreq,idep_fit,1:ntheta_DSM,icomp_disp)=disp_acous_DSM(ipt_start:ipt_start+ntheta_DSM-1)
        end do
      end do
      close(15)

      ! Read pressure
      dir_DSM_pres=trim(dir_this_source(1:len_trim(dir_this_source)))//'/OUTPUT_FILES/pressure'
      DSM_pressure_file=trim(dir_DSM_pres(1:len_trim(dir_DSM_pres)))// '/freq_' // trim(ifreq_file)
      open(unit=16,file=trim(DSM_pressure_file),status='unknown',form='unformatted',iostat=ios)
      if(ios /= 0) stop 'error in reading pressure'
      read(16) pressure_DSM(:)
      do idep_fit=1,nfit_dep
        idep_in_DSM=first_idep_fit_DSM+idep_fit-1
        ipt_start=(idep_in_DSM-1)*ntheta_DSM+1
        ! Pressure has only one component
        pressure_fit_DSM(ifreq,idep_fit,1:ntheta_DSM)=pressure_DSM(ipt_start:ipt_start+ntheta_DSM-1)
      end do
      close(16)

      ! Read potential
      dir_DSM_potential=trim(dir_this_source(1:len_trim(dir_this_source)))//'/OUTPUT_FILES/potential'
      DSM_potential_file=trim(dir_DSM_potential(1:len_trim(dir_DSM_potential)))// '/freq_' // trim(ifreq_file)
      open(unit=16,file=trim(DSM_potential_file),status='unknown',form='unformatted',iostat=ios)
      if(ios /= 0) stop 'error in reading potential'
      read(16) potential_DSM(:)
      do idep_fit=1,nfit_dep
        idep_in_DSM=first_idep_fit_DSM+idep_fit-1
        ipt_start=(idep_in_DSM-1)*ntheta_DSM+1
        ! Potential has only one component
        potential_fit_DSM(ifreq,idep_fit,1:ntheta_DSM)=potential_DSM(ipt_start:ipt_start+ntheta_DSM-1)
      end do
      close(16)

    end do  !ifreq

  !Solid media
  else if(media_type.eq.2) then
    ! Do the same, but for solid media

    ! Step 1, find out the first and last DSM detphs of the zone, where
    ! depths_SEM(idepth) is located.
    idep_search=1
    idep_start_this_zone=0
    idep_end_this_zone=0
    do while(idep_search.le.ndep_elas_DSM)
       if(zone_elas_DSM(idep_search).eq.zones_SEM(idepth)) then
           if(idep_search.eq.1) then
             idep_start_this_zone=idep_search
           else if(idep_search.eq.ndep_elas_DSM) then
             idep_end_this_zone=idep_search
           else
             if(zone_elas_DSM(idep_search).eq.zones_SEM(idepth) .and. &
                zone_elas_DSM(idep_search-1).ne.zones_SEM(idepth)) &
                  idep_start_this_zone=idep_search
             if(zone_elas_DSM(idep_search).eq.zones_SEM(idepth) .and. &
                zone_elas_DSM(idep_search+1).ne.zones_SEM(idepth)) &
                  idep_end_this_zone=idep_search
           end if
       end if
       idep_search=idep_search+1
    end do ! idep_search
    ndepths_this_zone=idep_end_this_zone-idep_start_this_zone+1
    if(ndepths_this_zone.lt.nfit_dep) stop 'Error, the number of DSM depths in the interpolating &
       zone is smaller than nfit_dep!'

    ! Step 2, search in these depths to get the first interpolating DSM depth
    if(abs(depths_SEM(idepth)-depths_elas_DSM(idep_end_this_zone)).lt.depth_tolerance) then
      first_idep_fit_DSM=idep_end_this_zone
    else
      idep_search=idep_start_this_zone
      do while(depths_elas_DSM(idep_search).le.depths_SEM(idepth)+depth_tolerance.and.&
             idep_search.le.idep_end_this_zone-1)
        idep_search=idep_search+1
      end do
      first_idep_fit_DSM = idep_search-1
    end if
    ! Shift the estimated depth by -nfit_dep/2 such that depths_SEM(idepth) is
    ! in the middle among the nfit_dep interpolating depths
    first_idep_fit_DSM = first_idep_fit_DSM - int(nfit_dep/2.0)
    if(first_idep_fit_DSM.lt.idep_start_this_zone) first_idep_fit_DSM=idep_start_this_zone
    if(first_idep_fit_DSM.gt.idep_end_this_zone-nfit_dep+1) &
         first_idep_fit_DSM=idep_end_this_zone-nfit_dep+1

    do ifreq=1,nfrequency
      write(ifreq_file,"(i5.5)")ifreq-1

      ! Read velocity seismograms
      dir_DSM_velo=trim(dir_this_source(1:len_trim(dir_this_source)))//'/OUTPUT_FILES/velo_solid'
      DSM_velo_file=trim(dir_DSM_velo(1:len_trim(dir_DSM_velo)))// '/freq_' // trim(ifreq_file)
      open(unit=15,file=trim(DSM_velo_file),status='unknown',form='unformatted',iostat=ios)
      if(ios /= 0) stop 'error in reading velocity seismograms in the_solid'
      do icomp_velo=1,ncomp_velo
        read(15) velo_elas_DSM(:)
        do idep_fit=1,nfit_dep
          idep_in_DSM=first_idep_fit_DSM+idep_fit-1
          ipt_start=(idep_in_DSM-1)*ntheta_DSM+1
          velo_elas_fit_DSM(ifreq,idep_fit,1:ntheta_DSM,icomp_velo)= &
          velo_elas_DSM(ipt_start:ipt_start+ntheta_DSM-1)
        end do
      end do
      close(15)

      ! Read stress
      dir_DSM_stress=trim(dir_this_source(1:len_trim(dir_this_source)))//'/OUTPUT_FILES/stress'
      DSM_stress_file=trim(dir_DSM_stress(1:len_trim(dir_DSM_stress)))// '/freq_' // trim(ifreq_file)
      open(unit=16,file=trim(DSM_stress_file),status='unknown',form='unformatted',iostat=ios)
      if(ios /= 0) stop 'error in reading stress'
      do icomp_stress=1,ncomp_stress
        read(16) stress_DSM(:)
        do idep_fit=1,nfit_dep
          idep_in_DSM=first_idep_fit_DSM+idep_fit-1
          ipt_start=(idep_in_DSM-1)*ntheta_DSM+1
          stress_fit_DSM(ifreq,idep_fit,1:ntheta_DSM,icomp_stress)= &
          stress_DSM(ipt_start:ipt_start+ntheta_DSM-1)
        end do
      end do
      close(16)
    end do  !ifreq

  else
    stop 'Error, media_type must be 1 (fluid) or 2 (solid).'
  end if
end subroutine read_DSM_database


!******************************************read SEM****************************************
! Read the depth and distance tables from SEM
subroutine read_SEM_depth_distance(dir_SEM_input,media_type,ndepth_this_media,icomp_source)

   use extract_Injectedwaves_par, only:ndepths_SEM,depths_SEM,zones_SEM,ndistances_SEM,distances_SEM,&
       disp_acous_SEM,pressure_SEM,chi_dot_SEM,velo_elas_SEM,stress_SEM,nfrequency,&
       ncomp_disp,ncomp_velo,ncomp_stress,ndep_acous_DSM,ndep_elas_DSM,depths_acous_DSM,&
       depths_elas_DSM,depth_tolerance,acous_depth_file_SEM,min_theta_DSM,max_theta_DSM,&
       dtheta_DSM,nfit_dist,&
       acous_dist_file_SEM,elas_depth_file_SEM,elas_dist_file_SEM,ismooth,disp_acous_time_SEM,&
       pressure_time_SEM,chi_dot_time_SEM,velo_elas_time_SEM,stress_time_SEM,myrank
   implicit none

   character(len=256), intent(in) :: dir_SEM_input
   integer, intent(in) ::media_type,icomp_source
   integer, intent(out) ::ndepth_this_media
   ! local parameters
   character(len=256) :: depth_file,dist_file
   double precision  :: ddep_tmp,depth_tole,dist_tmp,lower_dist_tolerance,upper_dist_tolerance
   integer :: idepth_SEM,idepth_DSM,idist,max_ndistances,ndistances_tmp,ios

   ! Read the depth table
   if(media_type.eq.1) then
     depth_file=trim(dir_SEM_input(1:len_trim(dir_SEM_input)))// '/' // trim(acous_depth_file_SEM)
   else
     depth_file=trim(dir_SEM_input(1:len_trim(dir_SEM_input)))// '/' // trim(elas_depth_file_SEM)
   end if
   open(unit=12, file=depth_file, status='unknown',iostat=ios)
   if(ios /= 0) then
      print *,'error:could not open the SEM depth file'
      call exit_mpi('error opening parameter file')
   end if

  ! ddep_tmp and depth_tole_tmp are used in DSM. Not functional here.
   read(12,*) ddep_tmp,depth_tole
   read(12,*) ndepths_SEM

   ! Release these variables' memories first, because they have been allocated in the
   ! previous running
   if(media_type.gt.1.or.icomp_source.gt.1) &
       deallocate(depths_SEM,zones_SEM,ndistances_SEM)

   ! Then re-allocate
   allocate(depths_SEM(ndepths_SEM))
   allocate(zones_SEM(ndepths_SEM))
   allocate(ndistances_SEM(ndepths_SEM))

   ! Read each depth
   do idepth_SEM=1,ndepths_SEM
      read(12,*)depths_SEM(idepth_SEM),zones_SEM(idepth_SEM)
   end do
   close(12)

   ndepth_this_media=ndepths_SEM

   ! Determine the maximum number of distances across all depths to allocate the
   ! 2-D distance array efficiently
   max_ndistances=0
   if(media_type.eq.1) then
     dist_file=trim(dir_SEM_input(1:len_trim(dir_SEM_input)))// '/' // trim(acous_dist_file_SEM)
   else
     dist_file=trim(dir_SEM_input(1:len_trim(dir_SEM_input)))// '/' // trim(elas_dist_file_SEM)
   end if
   open( unit=13, file=dist_file, status='unknown',iostat=ios)
   if(ios /= 0) then
      print *,'error:could not open the SEM distance file'
      call exit_mpi('error opening parameter file')
   end if

   do idepth_SEM=1,ndepths_SEM
     read(13,*) ndistances_tmp
     do idist=1,ndistances_tmp
       read(13,*) dist_tmp
     end do
     if(ndistances_tmp.gt.max_ndistances) max_ndistances=ndistances_tmp
   end do
   close(13)
   
   ! Allocate memories
   ! Release these memories first, because they have been allocated in the
   ! previous fluid media or source component running
   if(media_type.gt.1.or.icomp_source.gt.1) deallocate(distances_SEM)
   allocate(distances_SEM(ndepths_SEM,max_ndistances))

   ! Only the processors, whose ID is smaller than ndepths_this_media, will 
   ! have loaded job. These memories will be released in .
   if(media_type.eq.1.and.icomp_source.eq.1.and.myrank.lt.ndepth_this_media) then
     allocate(disp_acous_SEM(nfrequency,max_ndistances,ncomp_disp))
     allocate(pressure_SEM(nfrequency,max_ndistances))
     allocate(chi_dot_SEM(nfrequency,max_ndistances))
     allocate(disp_acous_time_SEM(2*ismooth*nfrequency,max_ndistances,ncomp_disp))
     allocate(pressure_time_SEM(2*ismooth*nfrequency,max_ndistances))
     allocate(chi_dot_time_SEM(2*ismooth*nfrequency,max_ndistances))
   end if
   if(media_type.eq.2.and.icomp_source.eq.1.and.myrank.lt.ndepth_this_media) then
     allocate(velo_elas_SEM(nfrequency,max_ndistances,ncomp_velo))
     allocate(stress_SEM(nfrequency,max_ndistances,ncomp_stress))
     allocate(velo_elas_time_SEM(2*ismooth*nfrequency,max_ndistances,ncomp_velo))
     allocate(stress_time_SEM(2*ismooth*nfrequency,max_ndistances,ncomp_stress))
   end if

   ! Re-read the distance table
   open( unit=13, file=dist_file, status='unknown',iostat=ios)
   if(ios /= 0) then
      print *,'error:could not open the SEM distance file'
      call exit_mpi('error opening parameter file')
   end if
   do idepth_SEM=1,ndepths_SEM
     read(13,*) ndistances_SEM(idepth_SEM)
     do idist=1,ndistances_SEM(idepth_SEM)
       read(13,*) distances_SEM(idepth_SEM,idist)
     end do
   end do
   close(13)

  ! Check validity
  ! The SEM depths and distances to be interpolated must be within the DSM database range.
  ! If the DSM table starts close to zero distance, allow a SEM distance just below
  ! the first DSM sample. do_interpolation then uses the first nfit_dist DSM samples
  ! on the right side, which avoids failing on the zero-distance endpoint while still
  ! rejecting ordinary out-of-range distances. At the upper endpoint, allow only
  ! limited one-sided interpolation using the final DSM samples; larger extrapolation
  ! is rejected because it is too far outside the DSM distance table.
  lower_dist_tolerance=dble(nfit_dist)*dtheta_DSM
  upper_dist_tolerance=2.d0*dtheta_DSM
  do idepth_SEM=1,ndepths_SEM
     if(media_type.eq.1) then
         if (depths_SEM(idepth_SEM).lt.depths_acous_DSM(1)-depth_tolerance.or. &
                         depths_SEM(idepth_SEM).gt.depths_acous_DSM(ndep_acous_DSM)+depth_tolerance) then 
           stop 'Error, acoustic SEM depth is out of the DSM depth range!'
         end if
     end if

     if(media_type.eq.2) then
       if (depths_SEM(idepth_SEM).lt.depths_elas_DSM(1)-depth_tolerance.or. &
                         depths_SEM(idepth_SEM).gt.depths_elas_DSM(ndep_elas_DSM)+depth_tolerance) then
            stop 'Error, elastic SEM depth is out of the DSM depth range!'
       end if
     end if


     if(distances_SEM(idepth_SEM,ndistances_SEM(idepth_SEM)).gt.max_theta_DSM+upper_dist_tolerance) &
         stop 'Error, SEM distance is out of the DSM distance range!'
     if(distances_SEM(idepth_SEM,1).lt.min_theta_DSM.and. &
        (distances_SEM(idepth_SEM,1).lt.0.d0.or.min_theta_DSM.gt.lower_dist_tolerance)) &
         stop 'Error, SEM distance is out of the DSM distance range!'
  end do


end subroutine read_SEM_depth_distance
