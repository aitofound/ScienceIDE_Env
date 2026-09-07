subroutine read_para()
   use coupling_SEM_DSM_par
   use convolution_par
   use constants
   implicit none

! other variables
   double precision ::npow,ratio_deltat
   integer ::n_refine
   character(len=MAX_STRING_LEN) ::para_file,para_file_SEM_Par,Green_par_file,DSMsrc_par_file
   character(len=FORCE_NAME_LEN) ::this_force_present
   integer,parameter ::IIN_SEM_Par_Coupling=14
   integer,parameter ::IIN_Green_Par=15
   integer,parameter ::IIN_DSMsrc_Par=16
   integer ::ios
   integer ::ier

   para_file="DATA/Par_file"
   open(unit=IIN,file=trim(para_file),action='read',form="formatted",status="old",iostat=ios)
   if(ios /= 0) then
      print *,'error:not able to open parameter file'
      call exit_mpi('error opening parameter file')
   end if
  
   call read_value_string(IIN,IGNORE_JUNK,dir_SEM_input,'coupling.dir_SEM_input',ier)
   if(ier /= 0) stop 'Error reading coupling parameter dir_SEM_input'

   call read_value_string(IIN,IGNORE_JUNK,dir_DSM_input,'coupling.dir_DSM_input',ier)
   if(ier /= 0) stop 'Error reading coupling parameter dir_DSM_input'

   call read_value_string(IIN,IGNORE_JUNK,dir_DSMsrc_input,'coupling.dir_DSMsrc_input',ier)
   if(ier /= 0) stop 'Error reading coupling parameter dir_DSMsrc_input'

   call read_value_integer(IIN,IGNORE_JUNK,npoints_taper_SEM,'coupling.npoints_taper_SEM',ier)
   if(ier /= 0) stop 'Error reading coupling parameter npoints_taper_SEM'

   call read_value_logical(IIN,IGNORE_JUNK,do_coupling_RTZ(1),'coupling.vertical_component',ier)
   if(ier /= 0) stop 'Error reading coupling parameter vertical_component'

   call read_value_logical(IIN,IGNORE_JUNK,do_coupling_RTZ(2),'coupling.radial_component',ier)
   if(ier /= 0) stop 'Error reading coupling parameter radial_component'

   call read_value_logical(IIN,IGNORE_JUNK,do_coupling_RTZ(3),'coupling.transverse_component',ier)
   if(ier /= 0) stop 'Error reading coupling parameter transverse_component'

   call read_value_integer(IIN,IGNORE_JUNK,nfit_dep,'coupling.nfit_dep',ier)
   if(ier /= 0) stop 'Error reading coupling parameter nfit_dep'

   call read_value_integer(IIN,IGNORE_JUNK,nfit_dist,'coupling.nfit_dist',ier)
   if(ier /= 0) stop 'Error reading coupling parameter nfit_dist'


   close (IIN)

   !check validity
   if((.not.do_coupling_RTZ(1)).and.(.not.do_coupling_RTZ(2)).and.(.not.do_coupling_RTZ(3))) then
      call exit_mpi('To get meaningful coupling, at least one component in Z-R-T should be computed.')
   end if

   if((nfit_dep.lt.2.or.nfit_dep.gt.max_nfit).or.(nfit_dist.lt.2.or.nfit_dist.gt.max_nfit)) then
      call exit_mpi('Error, nfit_dep and/or nfit_dist is below 2 or too large (>max_nfit)!') 
   end if



   para_file_SEM_Par=trim(dir_SEM_input(1:len_trim(dir_SEM_input)))//"/DATABASES_MPI/SEM_Coupling_Par_file"
   open(unit=IIN_SEM_Par_Coupling,file=trim(para_file_SEM_Par),action='read',&
        form="formatted",status="old",iostat=ios)
   if(ios /= 0) then
      print *,'error:not able to open SEM input file',trim(para_file_SEM_Par)
      call exit_mpi('error opening SEM input file SEM_Coupling_Par')
   end if

   call read_value_integer(IIN_SEM_Par_Coupling,IGNORE_JUNK,npackage_SEM,'SEM_Par.npackage_SEM',ier)
   if(ier /= 0) stop 'Error reading SEM parameter npackage_SEM'

   call read_value_integer(IIN_SEM_Par_Coupling,IGNORE_JUNK,npoints_per_pack_SEM,'SEM_Par.npoints_per_pack_SEM',ier)
   if(ier /= 0) stop 'Error reading SEM parameter npoints_per_pack_SEM'

   call read_value_integer(IIN_SEM_Par_Coupling,IGNORE_JUNK,nstep_each_section_SEM,'SEM_Par.nstep_each_section_SEM',ier)
   if(ier /= 0) stop 'Error reading SEM parameter nstep_each_section_SEM'

   call read_value_integer(IIN_SEM_Par_Coupling,IGNORE_JUNK,total_nstep_SEM,'SEM_Par.total_nstep_SEM',ier)
   if(ier /= 0) stop 'Error reading SEM parameter total_nstep_SEM'

   call read_value_integer(IIN_SEM_Par_Coupling,IGNORE_JUNK,nsection_SEM,'SEM_Par.nsection_SEM',ier)
   if(ier /= 0) stop 'Error reading SEM parameter nsection_SEM'

   call read_value_dble_precision(IIN_SEM_Par_Coupling,IGNORE_JUNK,deltat_SEM,'SEM_Par.deltat_SEM',ier)
   if(ier /= 0) stop 'Error reading SEM parameter deltat_SEM'
   
   close(IIN_SEM_Par_Coupling)



   if(do_coupling_RTZ(1)) then
     this_force_present=force_comp(1)
   else if(do_coupling_RTZ(2)) then
     this_force_present=force_comp(2)
   else 
     this_force_present=force_comp(3)
   end if
   Green_par_file=trim(dir_DSM_input(1:len_trim(dir_DSM_input)))// '/' // &
        trim(this_force_present(1:len_trim(this_force_present)))// '/OUTPUT_FILES/' // &
        trim(DSM_para_file(1:len_trim(DSM_para_file)))
   !print *,'Greens_file',Green_par_file
   open(unit=IIN_Green_Par,file=trim(Green_par_file),status='unknown',form='formatted',iostat=ios)
   if(ios /= 0) stop 'Error in openning file Green_Par'
   read(IIN_Green_Par,*)nfreq_DSM
   read(IIN_Green_Par,*)ndep_acous_DSM,ndep_elas_DSM
   read(IIN_Green_Par,*)min_dist_DSM,ddist_DSM,ndist_DSM
   read(IIN_Green_Par,*)time_series_length
   read(IIN_Green_Par,*)omega_imag
   close(IIN_Green_Par)
   nloc_elas_DSM=ndep_elas_DSM*ndist_DSM
   nloc_acous_DSM=ndep_acous_DSM*ndist_DSM
   max_dist_DSM=min_dist_DSM+(ndist_DSM-1)*ddist_DSM

   DSMsrc_par_file=trim(dir_DSMsrc_input(1:len_trim(dir_DSMsrc_input)))//'/OUTPUT_FILES/' // &
        trim(DSM_para_file(1:len_trim(DSM_para_file)))
   !print *,'Greens_file',Green_par_file
   open(unit=IIN_DSMsrc_Par,file=trim(DSMsrc_par_file),status='unknown',form='formatted',iostat=ios)
   if(ios /= 0) stop 'Error in openning file DSMsrc_Par'
   read(IIN_DSMsrc_Par,*)nfreq_DSM_tosrc
   read(IIN_DSMsrc_Par,*)ndep_acous_DSM_toSrc,ndep_elas_DSM_toSrc
   read(IIN_DSMsrc_Par,*)min_dist_DSM_toSrc,ddist_DSM_toSrc,ndist_DSM_toSrc
   read(IIN_DSMsrc_Par,*)time_series_length_tosrc
   read(IIN_DSMsrc_Par,*)omega_imag_tosrc
   close(IIN_DSMsrc_Par)

   if(nfreq_DSM_tosrc-nfreq_DSM.ne.0.or.dabs(time_series_length_tosrc-time_series_length).gt.TINY &
      .and.dabs(omega_imag_tosrc-omega_imag).gt.TINY) then
      call exit_MPI('error,DSM_Recv parameters are different from DSM_Src!')
   end if
   nloc_elas_DSM_toSrc=ndep_elas_DSM_toSrc*ndist_DSM_toSrc
   !print *,'nloc_elas_DSM_toSrc',ndep_elas_DSM_toSrc,ndist_DSM_toSrc
   nloc_acous_DSM_toSrc=ndep_acous_DSM_toSrc*ndist_DSM_toSrc
   max_dist_DSM_toSrc=min_dist_DSM_toSrc+(ndist_DSM_toSrc-1)*ddist_DSM_toSrc

   double_nfreq_DSM=2*nfreq_DSM
   deltat_DSM = time_series_length/double_nfreq_DSM
   npow=double_nfreq_DSM/2.0
   do while(npow.gt.1.d0)
      npow=npow/2.d0
   end do
   if(dabs(npow-1.d0)>TINY) then 
     call exit_MPI('error of double_nfreq_DSM, double_nfreq_DSM should be power of 2')
   end if

   ratio_deltat=deltat_DSM/deltat_SEM
   n_refine=1
   if(ratio_deltat.lt.1.d0+TINY) then
     call exit_MPI('error of deltat, sampling for the boundary disp is too large')
   else
     do while(ratio_deltat.gt.1.d0)
        ratio_deltat=ratio_deltat/2.d0
        n_refine=n_refine*2
     end do
   end if

!to be fixed
!   n_refine=n_refine/4
   n_refine=n_refine/2

!   print *,'n_refine',n_refine,deltat_refine,deltat_DSM
   npts_fine=double_nfreq_DSM*n_refine
   deltat_refine=deltat_DSM/n_refine
   !print *,'n_refine',n_refine,deltat_refine,deltat_DSM

   !if(n_refine.le.2) then
   !    deltat_refine=deltat_DSM
   !    npts_fine=double_nfreq_DSM
   !else
   !    deltat_refine=deltat_DSM/n_refine*2
   !    npts_fine=double_nfreq_DSM*n_refine/2
   !end if
end subroutine read_para

subroutine read_DSM_depths(dir_DSM_input,myrank)
   use constants, only:MAX_STRING_LEN
   use coupling_SEM_DSM_par, only:do_coupling_RTZ,do_coupling_RTZ,FORCE_NAME_LEN,&
        force_comp,DSM_acous_depth_file,DSM_elas_depth_file,ndep_acous_DSM,&
        ndep_elas_DSM,depths_acous_DSM,depths_elas_DSM,zones_acous_DSM,&
        zones_elas_DSM,depth_tolerence,nfit_dep
   implicit none

   character(len=MAX_STRING_LEN),intent(in) ::dir_DSM_input
   integer ::myrank
   !local parameters 
   character(len=FORCE_NAME_LEN) ::this_force_present
   double precision ::ddep_tmp,depth_tole_tmp,nwgll_tmp
   character(len=MAX_STRING_LEN) ::depth_file
   integer ::ios,idepth,ndep_tmp
   integer ::ndepths_this_zone,ndepths_last_zone,izone_this_dep,izone_last_dep


   if(do_coupling_RTZ(1)) then
     this_force_present=force_comp(1)
   else if(do_coupling_RTZ(2)) then
     this_force_present=force_comp(2)
   else
     this_force_present=force_comp(3)
   end if

   !read fluid media depths
   depth_file=trim(dir_DSM_input(1:len_trim(dir_DSM_input)))// '/' // &
        trim(this_force_present(1:len_trim(this_force_present)))// '/DATA/'  //&
        trim(DSM_acous_depth_file(1:len_trim(DSM_acous_depth_file)))
   open( unit=12, file=depth_file, status='old' )
   !ddepth is used in DSM and not functional here.
   read(12,*) ddep_tmp,depth_tole_tmp
   read(12,*) ndep_tmp
   if(ndep_tmp.ne.ndep_acous_DSM) stop 'Error, ndep_acous_DSM from the files DSM_Par_file &
       and depth_fluid_list are not consistent!'

   allocate(depths_acous_DSM(ndep_acous_DSM))
   allocate(zones_acous_DSM(ndep_acous_DSM))
   !read(12,*) nwgll_tmp
   !read each depth
   do idepth=1,ndep_acous_DSM
      read(12,*)depths_acous_DSM(idepth),zones_acous_DSM(idepth)
   end do
   close(12)

  !check validity
  ndepths_last_zone=0
  ndepths_this_zone=0
  do idepth=1,ndep_acous_DSM
     izone_this_dep=zones_acous_DSM(idepth)
     if(idepth.eq.1) then
       izone_last_dep=zones_acous_DSM(idepth)
     else
       izone_last_dep=zones_acous_DSM(idepth-1)
     end if
     !reach discontinuity and enter a new zone
     if(izone_this_dep.ne.izone_last_dep) then
       if(dabs(depths_acous_DSM(idepth)-depths_acous_DSM(idepth-1)).gt.depth_tolerence.and.myrank.eq.0) then
         print *,'Warnning!!!! No DSM depth set on the discontinuity between zones',izone_this_dep,'and',&
               izone_last_dep,',that may produce large errors!'
         print *,'try to set two (acoustic) depths (the same number but with different zone numbers) &
                  at the dicontinuity and re-run DSM!'
       end if
       ndepths_last_zone=ndepths_this_zone
       if(ndepths_last_zone.lt.nfit_dep) &
         stop 'Error, the number of depth in one (acoustic) zone should be larger than nfit_dep!'
       ndepths_this_zone=0
     end if
     ndepths_this_zone=ndepths_this_zone+1
!     if(myrank.eq.0) print *,'rank0-ndep',idepth,ndepths_this_zone
     if(idepth.eq.ndep_acous_DSM.and.ndepths_this_zone.lt.nfit_dep) &
         stop 'Error, the number of depth in the last (acoustic) zone should be larger than nfit_dep!'
  end do


   !read solid media depths
   depth_file=trim(dir_DSM_input(1:len_trim(dir_DSM_input)))// '/' // &
        trim(this_force_present(1:len_trim(this_force_present)))// '/DATA/'  //&
        trim(DSM_elas_depth_file(1:len_trim(DSM_elas_depth_file)))
   open( unit=12, file=depth_file, status='old' )
   !ddepth is used in DSM and not functional here.
   read(12,*) ddep_tmp,depth_tole_tmp 
   read(12,*) ndep_tmp
   if(ndep_tmp.ne.ndep_elas_DSM) stop 'Error, ndep_elas_DSM from the files DSM_Par_file &
       and depth_solid_list are not consistent!'

   allocate(depths_elas_DSM(ndep_elas_DSM))
   allocate(zones_elas_DSM(ndep_elas_DSM))
   !read(12,*) nwgll_tmp
   !read each depth
   do idepth=1,ndep_elas_DSM
      read(12,*)depths_elas_DSM(idepth),zones_elas_DSM(idepth)
!      print *,'DSM_zones',depths_elas_DSM(idepth),zones_elas_DSM(idepth)
   end do
   close(12)

  !check validity
  ndepths_last_zone=0
  ndepths_this_zone=0
  do idepth=1,ndep_elas_DSM
     izone_this_dep=zones_elas_DSM(idepth)
     if(idepth.eq.1) then
       izone_last_dep=zones_elas_DSM(idepth)
     else
       izone_last_dep=zones_elas_DSM(idepth-1)
     end if
     !reach discontinuity and enter a new zone
     if(izone_this_dep.ne.izone_last_dep) then
       if(dabs(depths_elas_DSM(idepth)-depths_elas_DSM(idepth-1)).gt.depth_tolerence.and.myrank.eq.0) then
         print *,'Warnning!!!! No DSM depth set on the discontinuity between zones',izone_this_dep,'and',&
               izone_last_dep,',that may produce large errors!'
         print *,'try to set two (elastic) depths (the same number but with different zone numbers) &
                  at the dicontinuity and re-run DSM!'
         print *
       end if

       ndepths_last_zone=ndepths_this_zone
       if(ndepths_last_zone.lt.nfit_dep) &
         stop 'Error, the number of depth in one (elastic) zone should be larger than nfit_dep!'
       ndepths_this_zone=0
     end if
     ndepths_this_zone=ndepths_this_zone+1
     if(idepth.eq.ndep_elas_DSM.and.ndepths_this_zone.lt.nfit_dep) &
         stop 'Error, the number of depth in the last (elastic) zone should be larger than nfit_dep!'
  end do
end subroutine read_DSM_depths

subroutine read_DSMsrc_depths(dir_DSMsrc_input,myrank)
   use constants, only:MAX_STRING_LEN
   use coupling_SEM_DSM_par, only:&
        DSM_acous_depth_file,DSM_elas_depth_file,ndep_acous_DSM_toSrc,&
        ndep_elas_DSM_toSrc,depths_acous_DSM_toSrc,depths_elas_DSM_toSrc,zones_acous_DSM_toSrc,&
        zones_elas_DSM_toSrc,depth_tolerence,nfit_dep
   implicit none

   character(len=MAX_STRING_LEN),intent(in) ::dir_DSMsrc_input
   integer ::myrank
   !local parameters 
   double precision ::ddep_tmp,depth_tole_tmp,nwgll_tmp
   character(len=MAX_STRING_LEN) ::depth_file
   integer ::ios,idepth,ndep_tmp
   integer ::ndepths_this_zone,ndepths_last_zone,izone_this_dep,izone_last_dep


   !read fluid media depths
   depth_file=trim(dir_DSMsrc_input(1:len_trim(dir_DSMsrc_input)))// '/DATA/' // &
        trim(DSM_acous_depth_file(1:len_trim(DSM_acous_depth_file)))
   open( unit=12, file=depth_file, status='old' )
   !ddepth is used in DSM and not functional here.
   read(12,*) ddep_tmp,depth_tole_tmp
   read(12,*) ndep_tmp
   if(ndep_tmp.ne.ndep_acous_DSM_toSrc) stop 'Error, ndep_acous_DSM from the files DSM_Par_file &
       and depth_fluid_list are not consistent!'

   allocate(depths_acous_DSM_toSrc(ndep_acous_DSM_toSrc))
   allocate(zones_acous_DSM_toSrc(ndep_acous_DSM_toSrc))
   !read(12,*) nwgll_tmp
   !read each depth
   do idepth=1,ndep_acous_DSM_toSrc
      read(12,*)depths_acous_DSM_toSrc(idepth),zones_acous_DSM_toSrc(idepth)
   end do
   close(12)

  !check validity
  ndepths_last_zone=0
  ndepths_this_zone=0
  do idepth=1,ndep_acous_DSM_toSrc
     izone_this_dep=zones_acous_DSM_toSrc(idepth)
     if(idepth.eq.1) then
       izone_last_dep=zones_acous_DSM_toSrc(idepth)
     else
       izone_last_dep=zones_acous_DSM_toSrc(idepth-1)
     end if
     !reach discontinuity and enter a new zone
     if(izone_this_dep.ne.izone_last_dep) then
       if(dabs(depths_acous_DSM_toSrc(idepth)-depths_acous_DSM_toSrc(idepth-1)).gt.depth_tolerence.and.myrank.eq.0) then
         print *,'Warnning!!!! No DSM depth set on the discontinuity between zones',izone_this_dep,'and',&
               izone_last_dep,',that may produce large errors!'
         print *,'try to set two (acoustic) depths (the same number but with different zone numbers) &
                  at the dicontinuity and re-run DSM!'
       end if
       ndepths_last_zone=ndepths_this_zone
       if(ndepths_last_zone.lt.nfit_dep) &
         stop 'Error, the number of depth in one (acoustic) zone should be larger than nfit_dep!'
       ndepths_this_zone=0
     end if
     ndepths_this_zone=ndepths_this_zone+1
!     if(myrank.eq.0) print *,'rank0-ndep',idepth,ndepths_this_zone
     if(idepth.eq.ndep_acous_DSM_toSrc.and.ndepths_this_zone.lt.nfit_dep) &
         stop 'Error, the number of depth in the last (acoustic) zone should be larger than nfit_dep!'
  end do


   !read solid media depths
   depth_file=trim(dir_DSMsrc_input(1:len_trim(dir_DSMsrc_input)))// '/DATA/' // &
        trim(DSM_elas_depth_file(1:len_trim(DSM_elas_depth_file)))
   open( unit=12, file=depth_file, status='old' )
   !ddepth is used in DSM and not functional here.
   read(12,*) ddep_tmp,depth_tole_tmp
   read(12,*) ndep_tmp
   if(ndep_tmp.ne.ndep_elas_DSM_toSrc) stop 'Error, ndep_elas_DSM from the files DSM_Par_file &
       and depth_solid_list are not consistent!'

   allocate(depths_elas_DSM_toSrc(ndep_elas_DSM_toSrc))
   allocate(zones_elas_DSM_toSrc(ndep_elas_DSM_toSrc))

   !read(12,*) nwgll_tmp
   !read each depth
   do idepth=1,ndep_elas_DSM_toSrc
      read(12,*)depths_elas_DSM_toSrc(idepth),zones_elas_DSM_toSrc(idepth)
!      print *,'DSM_zones',depths_elas_DSM(idepth),zones_elas_DSM(idepth)
   end do
   close(12)

  !check validity
  ndepths_last_zone=0
  ndepths_this_zone=0
  do idepth=1,ndep_elas_DSM_toSrc
     izone_this_dep=zones_elas_DSM_toSrc(idepth)
     if(idepth.eq.1) then
       izone_last_dep=zones_elas_DSM_toSrc(idepth)
     else
       izone_last_dep=zones_elas_DSM_toSrc(idepth-1)
     end if
     !reach discontinuity and enter a new zone
     if(izone_this_dep.ne.izone_last_dep) then
       if(dabs(depths_elas_DSM_toSrc(idepth)-depths_elas_DSM_toSrc(idepth-1)).gt.depth_tolerence.and.myrank.eq.0) then
         print *,'Warnning!!!! No DSM depth set on the discontinuity between zones',izone_this_dep,'and',&
               izone_last_dep,',that may produce large errors!'
         print *,'try to set two (elastic) depths (the same number but with different zone numbers) &
                  at the dicontinuity and re-run DSM!'
         print *
       end if

       ndepths_last_zone=ndepths_this_zone
       if(ndepths_last_zone.lt.nfit_dep) &
         stop 'Error, the number of depth in one (elastic) zone should be larger than nfit_dep!'
       ndepths_this_zone=0
     end if
     ndepths_this_zone=ndepths_this_zone+1
     if(idepth.eq.ndep_elas_DSM_toSrc.and.ndepths_this_zone.lt.nfit_dep) &
         stop 'Error, the number of depth in the last (elastic) zone should be larger than nfit_dep!'
  end do
end subroutine read_DSMsrc_depths


subroutine read_SEM_depths(dir_SEM_input)
   use coupling_SEM_DSM_par, only:ndepths_solid_SEM,ndepths_fluid_SEM,depths_solid_SEM,&
       depths_fluid_SEM,zones_solid_SEM,zones_fluid_SEM,acous_depth_file_SEM,&
       elas_depth_file_SEM
   use constants, only: MAX_STRING_LEN
   implicit none

   character(len=MAX_STRING_LEN), intent(in) :: dir_SEM_input
   !local parameters
   character(len=MAX_STRING_LEN) :: depth_file
   double precision  :: ddep_tmp,depth_tole,nwgll
   integer :: idepth_SEM,ios

   !local parameters

   !read the solid media depth table
   depth_file=trim(dir_SEM_input(1:len_trim(dir_SEM_input)))// &
                 '/DATABASES_MPI/' // trim(elas_depth_file_SEM)
   open(unit=12, file=depth_file, status='unknown',iostat=ios)
   if(ios /= 0) then
      print *,'error:could not open the SEM solid depth file'
      call exit_mpi('error opening parameter file')
   end if

   !ddep_tmp is used in DSM and not functional here.
   read(12,*) ddep_tmp,depth_tole
   read(12,*) ndepths_solid_SEM
   !print *,'ndepths_SEM_read',ndepths_SEM,depth_file

   allocate(depths_solid_SEM(ndepths_solid_SEM))
   allocate(zones_solid_SEM(ndepths_solid_SEM))
   !The below two parameters are used in DSM and functional here.
   !read(12,*) nwgll
   !read each depth
   do idepth_SEM=1,ndepths_solid_SEM
      read(12,*)depths_solid_SEM(idepth_SEM),zones_solid_SEM(idepth_SEM)
   end do
   close(12)

   !read the fluid media depth table
   depth_file=trim(dir_SEM_input(1:len_trim(dir_SEM_input)))// &
                 '/DATABASES_MPI/' // trim(acous_depth_file_SEM)
   open(unit=12, file=depth_file, status='unknown',iostat=ios)
   if(ios /= 0) then
      print *,'error:could not open the SEM fluid depth file'
      call exit_mpi('error opening parameter file')
   end if
   read(12,*) ddep_tmp,depth_tole
   read(12,*) ndepths_fluid_SEM
   !print *,'ndepths_SEM_read',ndepths_SEM,depth_file

   allocate(depths_fluid_SEM(ndepths_fluid_SEM))
   allocate(zones_fluid_SEM(ndepths_fluid_SEM))
!   read(12,*) nwgll
   !read each depth
   do idepth_SEM=1,ndepths_fluid_SEM
      read(12,*)depths_fluid_SEM(idepth_SEM),zones_fluid_SEM(idepth_SEM)
   end do
   close(12)

end subroutine read_SEM_depths

subroutine read_package_id(dir_SEM_input)
   use coupling_SEM_DSM_par, only:package_list_file_SEM,npackage_SEM,&
           global_pack_id,in_iproc,local_pack_id,npoint_pack,npoints,npack,ipoint_start,&
           npoint_pack,ipack_start
   use constants, only: MAX_STRING_LEN
   implicit none

   character(len=MAX_STRING_LEN),intent(in) ::dir_SEM_input
   !local variables
   integer ::ipack,ios
   character(len=MAX_STRING_LEN) ::package_file

   package_file=trim(dir_SEM_input(1:len_trim(dir_SEM_input)))// &
                '/DATABASES_MPI/' // trim(package_list_file_SEM)
   !print *,'package',package_file
   open(unit=10,file=trim(package_file),action='read',form="formatted",status="old",iostat=ios)
   if(ios /= 0) then
      print *,'error:could not open SEM package_list file'
      call exit_mpi('error opening SEM package_list file')
   end if
   
   do ipack=1,npackage_SEM
       read(10,*) global_pack_id(ipack),in_iproc(ipack),local_pack_id(ipack),npoint_pack(ipack)
!       read(*) global_pack_id(ipack),in_iproc(ipack),local_pack_id(ipack),npoint_pack(ipack)
   end do
   close(10)


   npoints=0
   do ipack=1,npack
     if(ipack.eq.1) then
      ipoint_start(ipack)=1
     else 
      ipoint_start(ipack)=ipoint_start(ipack-1)+npoint_pack(ipack-1+ipack_start-1)
     end if
   end do
   npoints=ipoint_start(npack)+npoint_pack(npack+ipack_start-1)-1

end subroutine read_package_id


subroutine read_ipack(ipack,ipack_local)
    use coupling_SEM_DSM_par
    implicit none
#ifdef USE_MPI
  ! standard include of the MPI library
  include "mpif.h"
#endif

    integer ::ipack,ipack_local

!other variables
   integer ::iblock_time,ipoint,nstep_this_block,it_this_block,it
!integer   ::icomp
   character(len=MAX_STRING_LEN) ::disp_file,traction_file,file_tmp
   integer ::ios
   integer ::ier

!   double precision ::nstation_temp



!**********************************************************************
!**************************read the displacement file******************
   write(file_tmp,"('DATABASES_MPIiproc',i4.4,'/disp_pack',i6.6)")in_iproc(ipack),local_pack_id(ipack)
   disp_file=trim(dir_SEM_input(1:len_trim(dir_SEM_input)))// '/' //trim(file_tmp(1:len_trim(file_tmp)))
   open(unit=30,file=trim(disp_file),action='read',access='stream',form='unformatted',status="old",iostat=ios)
!   open(unit=30,file=trim(disp_file),action='read',form="formatted",status="old",iostat=ios)
   if(ios /= 0) then
        print *,'displace file',disp_file
        call exit_mpi('error opening disp file')
   end if

!   read(30) nstation_temp
!   if(nstation_temp.ne.npoint(ipack)) then
!        call exit_mpi('error reading nstation')
!   end if
!   call MPI_BARRIER(MPI_COMM_WORLD,ier)
   do iblock_time=1,nsection_SEM
    do ipoint=1,npoint_pack(ipack)
     if(iblock_time.lt.nsection_SEM) then
        nstep_this_block=nstep_each_section_SEM
     else if(iblock_time.eq.nsection_SEM) then
        nstep_this_block=total_nstep_SEM-nstep_each_section_SEM*(nsection_SEM-1)
     end if
     do it_this_block=1,nstep_this_block
        if(ipoint.gt.npoints_per_pack_SEM)then
               stop 'Error reading'
        end if
        it=it_this_block+nstep_each_section_SEM*(iblock_time-1)
        read(30) disp_bound(it,:,ipoint)
     end do
    end do
   end do
   close(30)
!   call MPI_BARRIER(MPI_COMM_WORLD,ier)

!**********************************************************************
!**************************read the stress file******************
   write(file_tmp,"('DATABASES_MPIiproc',i4.4,'/traction_pack',i6.6)")in_iproc(ipack),local_pack_id(ipack)
   traction_file=trim(dir_SEM_input(1:len_trim(dir_SEM_input)))// '/' //trim(file_tmp(1:len_trim(file_tmp)))
   open(unit=40,file=trim(traction_file),action='read',access='stream',form='unformatted',status="old",iostat=ios)
!   open(unit=40,file=trim(traction_file),action='read',form="formatted",status="old",iostat=ios)
   if(ios /= 0) then
           call exit_mpi('error opening traction file')
   end if

!   read(40) nstation_temp
   do iblock_time=1,nsection_SEM
    do ipoint=1,npoint_pack(ipack)
     if(iblock_time.lt.nsection_SEM) then
        nstep_this_block=nstep_each_section_SEM
     else if(iblock_time.eq.nsection_SEM) then
        nstep_this_block=total_nstep_SEM-nstep_each_section_SEM*(nsection_SEM-1)
     end if
     do it_this_block=1,nstep_this_block
       if(ipoint.gt.npoints_per_pack_SEM) stop 'Error reading'
!       read(40) traction_bound(it,:,ipoint-1+ipoint_start(ipack_local))
        it=it_this_block+nstep_each_section_SEM*(iblock_time-1)
!        read(40,*) traction_bound(it,:,ipoint)
        read(40) traction_bound(it,:,ipoint)
     end do
    end do
   end do
   close(40)


end subroutine read_ipack



subroutine read_number_stations()
   use convolution_par
   use constants, only:km,MAX_STRING_LEN
   implicit none

!other variables
   character(len=MAX_STRING_LEN) ::station_file
   integer ::ios

  station_file="./DATA/STATION"
  open(unit=40,file=trim(station_file),action='read',form="formatted",status="unknown",iostat=ios)
  if(ios /= 0) then
    call exit_MPI('Error in openning file DATA/STATION')
  end if
  read(40,*)nstation
  close(40)


end subroutine read_number_stations

subroutine read_station()
   use convolution_par
   use constants, only:MAX_STRING_LEN
   implicit none
!other variables
  integer ::ios,nstation_temp,ista
  character(len=MAX_STRING_LEN) ::station_file

  station_file="./DATA/STATION"
  open(unit=40,file=trim(station_file),action='read',form="formatted",status="unknown",iostat=ios)
  if(ios /= 0) then
    call exit_MPI('Error in openning file DATA/STATION')
  end if
  read(40,*) nstation_temp
  do ista=1,nstation
     read(40,*) station_name(ista),station_lon(ista),station_lat(ista)
  end do
  close(40)
end subroutine read_station


subroutine read_source()
   use convolution_par
   use constants, only:MAX_STRING_LEN
   implicit none
!other variables
  integer ::ios
  character(len=MAX_STRING_LEN) ::source_file

  source_file="./DATA/SOURCE"
  open(unit=40,file=trim(source_file),action='read',form="formatted",status="unknown",iostat=ios)
  if(ios /= 0) then
    call exit_MPI('Error in openning file DATA/SOURCE')
  end if
  read(40,*) source_lon,source_lat
  close(40)
end subroutine read_source
