subroutine save_coupling_disp_traction()
  use specfem_par
  use specfem_par_elastic
  use specfem_par_acoustic
  use specfem_par_poroelastic
  use tele_coupling_par
  use constants, only: DEBUG_COUPLING, MAX_STRING_LEN
  implicit none

  integer ::iface_coupling,iface_coupling_elastic
  integer ::ipoint_elastic,ipoint_elas_acous,ipoint_iface
  integer ::imax,imin,di,jmax,jmin,dj,kmax,kmin,dk
  integer ::i,j,k,it_coupling,it_coupling_tmp,ispec,nstep_iblock
  integer ::ipackage
  integer ::ier1,ier2
  character(len=MAX_STRING_LEN) :: final_LOCAL_PATH,clean_LOCAL_PATH
  character(len=MAX_STRING_LEN) :: dir_this_proc,disp_coupling_file,&
                       traction_coupling_file
  
  it_coupling=(it-1)/DECIMATE_COUPLING+1


  clean_LOCAL_PATH = adjustl(LOCAL_PATH)
  final_LOCAL_PATH = clean_LOCAL_PATH(1:len_trim(clean_LOCAL_PATH))! //'/'


  dir_this_proc=trim(final_LOCAL_PATH)
  write(dir_this_proc,"(a,'iproc',i4.4)")trim(dir_this_proc),myrank

  ipackage=0
  iface_coupling_elastic=0
  do iface_coupling=1,nele_tele_coupling

   ispec=coupling_ele_property(iface_coupling)%ispec_coupling
   imin=coupling_ele_property(iface_coupling)%istart;imax=coupling_ele_property(iface_coupling)%iend
   jmin=coupling_ele_property(iface_coupling)%jstart;jmax=coupling_ele_property(iface_coupling)%jend
   kmin=coupling_ele_property(iface_coupling)%kstart;kmax=coupling_ele_property(iface_coupling)%kend

   if(ispec_is_elastic(ispec)) iface_coupling_elastic=iface_coupling_elastic+1
   di=1;dj=1;dk=1

   ipoint_iface=0
   do k=kmin,kmax,dk
     do j=jmin,jmax,dj
       do i=imin,imax,di
!         if(ispec_is_elastic(ispec)) then
            if(LOW_RESOLUTION) then
                   ipoint_elastic=iface_coupling_elastic
                   ipoint_elas_acous=iface_coupling
            else
                   ipoint_iface=ipoint_iface+1
                   ipoint_elas_acous=(iface_coupling-1)*NGLLX*NGLLY+ipoint_iface
                   ipoint_elastic=(iface_coupling_elastic-1)*NGLLX*NGLLY+ipoint_iface
            end if

            if(mod(ipoint_elas_acous,NPOINTS_PER_PACK).eq.1) then
               ipackage=ipackage+1
               write(disp_coupling_file,"(a,'/disp_pack',i6.6)")trim(dir_this_proc),ipackage
               write(traction_coupling_file,"(a,'/traction_pack',i6.6)")& 
                                                             trim(dir_this_proc),ipackage

               if(it_coupling.le.NSTEP_BETWEEN_OUTPUTBOUND) then
!                 open(unit=11,file=trim(disp_coupling_file),status='unknown',&
!                   action='write',form='formatted',iostat=ier1)
!                 open(unit=21,file=trim(traction_coupling_file),status='unknown',&
!                   action='write',form='formatted',iostat=ier2)
                 open(unit=11,file=trim(disp_coupling_file),status='replace',&
                   action='write',access='stream',form='unformatted',iostat=ier1)
                 open(unit=21,file=trim(traction_coupling_file),status='replace',&
                   action='write',access='stream',form='unformatted',iostat=ier2)
               else 
!                 open(unit=11,file=trim(disp_coupling_file),status='old',&
!                   action='write',form='formatted',position='append',iostat=ier1)
!                 open(unit=21,file=trim(traction_coupling_file),status='old',&
!                   action='write',form='formatted',position='append',iostat=ier2)
                 open(unit=11,file=trim(disp_coupling_file),status='old',&
                   action='write',access='stream',form='unformatted',position='append',iostat=ier1)
                 open(unit=21,file=trim(traction_coupling_file),status='old',&
                   action='write',access='stream',form='unformatted',position='append',iostat=ier2)
               end if
       
               if( ier1 /= 0 ) then
                 print*,'error: could not open  file'
                 print*,'path:',disp_coupling_file(1:len_trim(disp_coupling_file))
                 call exit_mpi(myrank,'error opening disp_coupling file')
               endif

               if( ier2 /= 0 ) then
                 print*,'error: could not open  file'
                 print*,'path:',traction_coupling_file(1:len_trim(traction_coupling_file))
                 call exit_mpi(myrank,'error opening traction_coupling file')
               endif
             end if
            if(it.eq.NSTEP) then
              if(DECIMATE_COUPLING.eq.1) then
                nstep_iblock=mod(it_coupling,NSTEP_BETWEEN_OUTPUTBOUND)
                if(nstep_iblock.eq.0) nstep_iblock=NSTEP_BETWEEN_OUTPUTBOUND
              else 
                if(mod(it_coupling,NSTEP_BETWEEN_OUTPUTBOUND).ne.0) then
                  nstep_iblock=mod(it_coupling,NSTEP_BETWEEN_OUTPUTBOUND)
                else if(it-1.eq.(it_coupling-1)*DECIMATE_COUPLING) then
                  nstep_iblock=NSTEP_BETWEEN_OUTPUTBOUND
                else if(it-((it_coupling-1)*DECIMATE_COUPLING+1).lt.DECIMATE_COUPLING) then
                  nstep_iblock=0
                else
                    call exit_MPI(myrank,'Error of nstep_iblock!')
                end if
              end if
            else
                nstep_iblock=NSTEP_BETWEEN_OUTPUTBOUND
            end if
            if(myrank.eq.0.and.ipoint_elas_acous.eq.1.and.DEBUG_COUPLING) &
                    print *,'disp1_bound',it,it_coupling,nstep_iblock,ipackage
            do it_coupling_tmp=1,nstep_iblock
!               write(11,*) disp_coupling(it_coupling_tmp,:,ipoint_elas_acous)
!               write(21,*) traction_coupling(it_coupling_tmp,:,ipoint_elas_acous)
               write(11) disp_coupling(it_coupling_tmp,:,ipoint_elas_acous)
               write(21) traction_coupling(it_coupling_tmp,:,ipoint_elas_acous)
            end do
            if(mod(ipoint_elas_acous,NPOINTS_PER_PACK).eq.0.or.ipoint_elas_acous.eq.npoints_tele_coupling) then
               close(11)
               close(21)
            end if
!        end if
       end do
     end do
   end do

  end do

end subroutine save_coupling_disp_traction




!---------------------------------------------------------------------
!save the strains for building up Green's functions in the next step of using the
!reciprocity.
subroutine save_strain_selected()
  use specfem_par
  use specfem_par_elastic
  use specfem_par_acoustic
  use specfem_par_poroelastic
  use tele_coupling_par
  implicit none

  integer ::ipoint_saved,iele_strain_saved
  integer ::imax,imin,jmax,jmin,kmax,kmin
  integer ::i,j,k,it_resampled,it_saved_begin,it_saved_end
  integer ::ier1
  character(len=MAX_STRING_LEN) :: final_LOCAL_PATH,clean_LOCAL_PATH
  character(len=MAX_STRING_LEN) :: dir_this_proc,strain_file

  it_resampled=(it-1)/DECIMATE_COUPLING+1
  it_saved_end=it_resampled
  if(mod(it_saved_end,NSTEP_BETWEEN_OUTPUTBOUND).ne.0) then
      it_saved_begin=it_saved_end-mod(it_saved_end,NSTEP_BETWEEN_OUTPUTBOUND)+1
  else
      it_saved_begin=it_saved_end-NSTEP_BETWEEN_OUTPUTBOUND+1
  end if

  clean_LOCAL_PATH = adjustl(LOCAL_PATH)
  final_LOCAL_PATH = clean_LOCAL_PATH(1:len_trim(clean_LOCAL_PATH))! //'/'
  dir_this_proc=trim(final_LOCAL_PATH)
  write(dir_this_proc,"(a,'iproc',i4.4)")trim(dir_this_proc),myrank
  write(strain_file,"(a,'/strain_it',i7.7,'_',i7.7)")trim(dir_this_proc),it_saved_begin,it_saved_end
  open(unit=11,file=trim(strain_file),status='unknown',&
                   action='write',form='unformatted',iostat=ier1)

  if( ier1 /= 0 ) then
      print*,'error: could not open  file'
      print*,'path:',strain_file(1:len_trim(strain_file))
      call exit_mpi(myrank,'error in opening Green_Strain file')
  endif


  ipoint_saved=0
  do iele_strain_saved=1,nele_strain_saved

     if(LOW_RESOLUTION) then
        imin=(NGLLX+1)/2;imax=imin
        jmin=(NGLLY+1)/2;jmax=jmin
        kmin=(NGLLZ+1)/2;kmax=kmin
     else
        imin=1;imax=NGLLX-1
        jmin=1;jmax=NGLLY-1
        kmin=1;kmax=NGLLZ-1
     end if

     do k=kmin,kmax
       do j=jmin,jmax
         do i=imin,imax
            ipoint_saved=ipoint_saved+1
            !strain_saved(NSTEP_BETWEEN_OUTPUTBOUND,6,npoints_strain_saved)
            !write(11,*) strain_saved(:,:,ipoint_saved)
            write(11) strain_saved(1:(it_saved_end-it_saved_begin+1),1:6,ipoint_saved)
       end do
     end do
   end do

  end do
  strain_saved(:,:,:)=0.0
  close(11)

  if(ipoint_saved.ne.npoints_strain_saved) then
      call exit_mpi(myrank,'error in saving strain tensors!')
  endif


end subroutine save_strain_selected
