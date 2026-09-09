!=====================================================================
!
!               S p e c f e m 3 D  V e r s i o n  3 . 0
!               ---------------------------------------
!
!     Main historical authors: Dimitri Komatitsch and Jeroen Tromp
!                        Princeton University, USA
!                and CNRS / University of Marseille, France
!                 (there are currently many more authors!)
! (c) Princeton University and CNRS / University of Marseille, July 2012
!
! This program is free software; you can redistribute it and/or modify
! it under the terms of the GNU General Public License as published by
! the Free Software Foundation; either version 2 of the License, or
! (at your option) any later version.
!
! This program is distributed in the hope that it will be useful,
! but WITHOUT ANY WARRANTY; without even the implied warranty of
! MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
! GNU General Public License for more details.
!
! You should have received a copy of the GNU General Public License along
! with this program; if not, write to the Free Software Foundation, Inc.,
! 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
!
!=====================================================================
!
! United States and French Government Sponsorship Acknowledged.

  subroutine iterate_time()

  use specfem_par
  use specfem_par_acoustic
  use specfem_par_elastic
  use specfem_par_poroelastic
  use specfem_par_movie
  use gravity_perturbation, only: gravity_timeseries, GRAVITY_SIMULATION
!WENBO
  use impose_1D_BCS,only:inject_npt_eachpack,ipack_impose,ipack_old, &
      it_impose,t0_this_pack,inject_dt

  implicit none

#ifdef DEBUG_COUPLED
    include "../../../add_to_iterate_time_5.F90"
#endif

  ! for EXACT_UNDOING_TO_DISK
  integer :: ispec,iglob,i,j,k,counter,record_length
  integer, dimension(:), allocatable :: integer_mask_ibool_exact_undo
  real(kind=CUSTOM_REAL), dimension(:), allocatable :: buffer_for_disk
  character(len=MAX_STRING_LEN) outputname
  ! timing
  double precision, external :: wtime
  double precision :: step8_profile_time

  !----  create a Gnuplot script to display the energy curve in log scale
  if (OUTPUT_ENERGY .and. myrank == 0) then
    open(unit=IOUT_ENERGY,file=trim(OUTPUT_FILES)//'plot_energy.gnu',status='unknown',action='write')
    write(IOUT_ENERGY,*) 'set term wxt'
    write(IOUT_ENERGY,*) '#set term postscript landscape color solid "Helvetica" 22'
    write(IOUT_ENERGY,*) '#set output "energy.ps"'
    write(IOUT_ENERGY,*) 'set logscale y'
    write(IOUT_ENERGY,*) 'set xlabel "Time step number"'
    write(IOUT_ENERGY,*) 'set ylabel "Energy (J)"'
    write(IOUT_ENERGY,'(a152)') '#plot "energy.dat" us 1:2 t ''Kinetic Energy'' w l lc 1, "energy.dat" us 1:3 &
                         &t ''Potential Energy'' w l lc 2, "energy.dat" us 1:4 t ''Total Energy'' w l lc 4'
    write(IOUT_ENERGY,*) '#pause -1 "Hit any key..."'
    write(IOUT_ENERGY,*) '#plot "energy.dat" us 1:2 t ''Kinetic Energy'' w l lc 1'
    write(IOUT_ENERGY,*) '#pause -1 "Hit any key..."'
    write(IOUT_ENERGY,*) '#plot "energy.dat" us 1:3 t ''Potential Energy'' w l lc 2'
    write(IOUT_ENERGY,*) '#pause -1 "Hit any key..."'
    write(IOUT_ENERGY,*) 'plot "energy.dat" us 1:4 t ''Total Energy'' w l lc 4'
    write(IOUT_ENERGY,*) 'pause -1 "Hit any key..."'
    close(IOUT_ENERGY)
  endif

#ifdef DEBUG_COUPLED
    include "../../../add_to_iterate_time_1.F90"
#endif

  ! open the file in which we will store the energy curve
  if (OUTPUT_ENERGY .and. myrank == 0) &
    open(unit=IOUT_ENERGY,file=trim(OUTPUT_FILES)//'energy.dat',status='unknown',action='write')

!
!   s t a r t   t i m e   i t e r a t i o n s
!

  ! synchronize all processes to make sure everybody is ready to start time loop
  call synchronize_all()
  if (myrank == 0) write(IMAIN,*) 'All processes are synchronized before time loop'

  if (myrank == 0) then
    write(IMAIN,*)
    write(IMAIN,*) 'Starting time iteration loop...'
    write(IMAIN,*)
    call flush_IMAIN()
  endif

  ! create an empty file to monitor the start of the simulation
  if (myrank == 0) then
    open(unit=IOUT,file=trim(OUTPUT_FILES)//'/starttimeloop.txt',status='unknown',action='write')
    write(IOUT,*) 'hello, starting time loop'
    close(IOUT)
  endif

  ! initialize variables for writing seismograms
  seismo_offset = it_begin-1
  seismo_current = 0

  ! get MPI starting time
  time_start = wtime()

#ifdef DEBUG_COUPLED
    include "../../../add_to_iterate_time_6.F90"
#endif

  ! *********************************************************
  ! ************* MAIN LOOP OVER THE TIME STEPS *************
  ! *********************************************************

  if (EXACT_UNDOING_TO_DISK) then

    if (GPU_MODE) call exit_MPI(myrank,'EXACT_UNDOING_TO_DISK not supported for GPUs')

    if (UNDO_ATTENUATION) &
      call exit_MPI(myrank,'EXACT_UNDOING_TO_DISK needs UNDO_ATTENUATION to be off because it computes the kernel directly instead')

    if (SIMULATION_TYPE == 1 .and. .not. SAVE_FORWARD) &
      call exit_MPI(myrank,'EXACT_UNDOING_TO_DISK requires SAVE_FORWARD if SIMULATION_TYPE == 1')

    if (ANISOTROPIC_KL) call exit_MPI(myrank,'EXACT_UNDOING_TO_DISK requires ANISOTROPIC_KL to be turned off')

!! DK DK determine the largest value of iglob that we need to save to disk,
!! DK DK since we save the upper part of the mesh only in the case of surface-wave kernels
    ! crust_mantle
    allocate(integer_mask_ibool_exact_undo(NGLOB_AB))
    integer_mask_ibool_exact_undo(:) = -1

    counter = 0
    do ispec = 1, NSPEC_AB
      do k = 1, NGLLZ
        do j = 1, NGLLY
          do i = 1, NGLLX
            iglob = ibool(i,j,k,ispec)
!           height = xstore(iglob)
            ! save that element only if it is in the upper part of the mesh
!           if (height >= 3000.d0) then
            if (.true.) then
              ! if this point has not yet been found before
              if (integer_mask_ibool_exact_undo(iglob) == -1) then
                ! create a new unique point
                counter = counter + 1
                integer_mask_ibool_exact_undo(iglob) = counter
              endif
            endif
          enddo
        enddo
      enddo
    enddo

    ! allocate the buffer used to dump a single time step
    allocate(buffer_for_disk(counter))

    ! open the file in which we will dump all the time steps (in a single file)
    write(outputname,"('huge_dumps/proc',i6.6,'_huge_dump_of_all_time_steps.bin')") myrank
    inquire(iolength=record_length) buffer_for_disk
    ! we write to or read from the file depending on the simulation type
    if (SIMULATION_TYPE == 1) then
      open(file=outputname, unit=IFILE_FOR_EXACT_UNDOING, action='write', status='unknown', &
                      form='unformatted', access='direct', recl=record_length)
    else if (SIMULATION_TYPE == 3) then
      open(file=outputname, unit=IFILE_FOR_EXACT_UNDOING, action='read', status='old', &
                      form='unformatted', access='direct', recl=record_length)
    else
      call exit_MPI(myrank,'EXACT_UNDOING_TO_DISK can only be used with SIMULATION_TYPE == 1 or SIMULATION_TYPE == 3')
    endif

  endif ! of if (EXACT_UNDOING_TO_DISK)

  it_begin = 1
  it_end = NSTEP
  do it = it_begin,it_end
    if(myrank == 0 .and. it <= 5 .and. DEBUG_COUPLING) then
      step8_profile_time = wtime()
      write(IMAIN,*) 'STEP8_PROFILE it start',it,step8_profile_time - time_start
      flush(IMAIN)
    endif
!WENBO_S
    !if(myrank.eq.0.and.DEBUG_COUPLING) print *,"it=",it
    if(CUBED_SPHERE_PROJECTION.and.(COUPLING_TYPE.eq.1.or.COUPLING_TYPE.eq.3.or.&
                                    COUPLING_TYPE.eq.2).and.it.eq.1) then
       call read_parameter_coupling(myrank)
       if(COUPLING_TYPE.eq.1.or.COUPLING_TYPE.eq.3) then
           call read_coupling_element()
           call synchronize_all()
           call set_tele_coupling_variables()
           call save_parameter_coupling()
           call get_tele_coupling_surfaces()
       else if(COUPLING_TYPE.eq.2.and.SINGLE_FORCE_ENZ.ne.0) then
           call elements_strain_saved()
       end if
       !if(DEBUG_COUPLING) print *,'end read tele_coupling_par',myrank,it,SINGLE_FORCE_ENZ
    end if

    if(CUBED_SPHERE_PROJECTION.and.(COUPLING_TYPE.eq.2.or.COUPLING_TYPE.eq.3)) then
      if(NSOURCES.lt.1) then
         call exit_MPI(myrank,'Now, only one source is allowed for imposing boundary conditions mode')
      end if
      if(it.eq.1) then
        call synchronize_all()
        if(DEBUG_COUPLING) print *,'start init'
        call init_impose_BCS()
        if(DEBUG_COUPLING) print *,'end init',inject_dt
        ipack_old=-1
      end if

      if(myrank.eq.1.and.DEBUG_COUPLING) &
         print *,"time",(it-1)*deltat,int( ((it-1)*deltat -t0_this_pack)/inject_dt ) + 1

      ipack_impose=int((it-1)*deltat/(inject_dt*(inject_npt_eachpack-1)))+1
      if(ipack_impose.gt.ipack_old) then
        t0_this_pack=(ipack_impose-1)*inject_dt*(inject_npt_eachpack-1)

        !if(ipack_impose.eq.2.and.DEBUG_COUPLING)  then
        !  print *,'read new package',ipack_impose,t0_this_pack,(it-1)*deltat,int( ((it-1)*deltat - t0_this_pack)/inject_dt ) + 1
        !end if
        if(DEBUG_COUPLING) print *,'INJECTED_WAVEFIELD_PATH_rank',myrank,INJECTED_WAVEFIELD_PATH
        call check_stability()
        call read_1Dboundary_values(ipack_impose,myrank,SINGLE_FORCE_ENZ,INJECTED_WAVEFIELD_PATH)
        if(myrank == 0 .and. it <= 5 .and. DEBUG_COUPLING) then
          step8_profile_time = wtime()
          write(IMAIN,*) 'STEP8_PROFILE after read_1Dboundary_values',it,ipack_impose,step8_profile_time - time_start
          flush(IMAIN)
        endif
        call check_stability()
        ipack_old=ipack_impose
      end if
      it_impose=int( ((it-1)*deltat - t0_this_pack)/inject_dt ) + 1

      if(it_impose.le.0.or.it_impose.ge.inject_npt_eachpack) then
        call exit_MPI(myrank,'Error,it_impose>inject_npt_eachpack-1 or <1')
      end if
    end if
!WENBO_E

    ! simulation status output and stability check
    if (mod(it,NTSTEP_BETWEEN_OUTPUT_INFO) == 0 .or. it == it_begin + 4 .or. it == it_end) then
      call check_stability()
    endif

    ! simulation status output and stability check
    if (OUTPUT_ENERGY) then
      if (mod(it,NTSTEP_BETWEEN_OUTPUT_ENERGY) == 0 .or. it == 5 .or. it == NSTEP) &
        call compute_total_energy()
    endif

    ! updates wavefields using Newmark time scheme
    if (.not. USE_LDDRK) call update_displacement_scheme()
    if(myrank == 0 .and. it <= 5 .and. DEBUG_COUPLING) then
      step8_profile_time = wtime()
      write(IMAIN,*) 'STEP8_PROFILE after update_displacement',it,step8_profile_time - time_start
      flush(IMAIN)
    endif

    ! calculates stiffness term
    if (.not. GPU_MODE) then
      ! wavefields on CPU

      ! note: the order of the computations for acoustic and elastic domains is crucial for coupled simulations
      if (SIMULATION_TYPE == 3) then
        ! kernel/adjoint simulations

        ! adjoint wavefields
        if (ELASTIC_SIMULATION .and. ACOUSTIC_SIMULATION) then
          ! coupled acoustic-elastic simulations
          ! 1. elastic domain w/ adjoint wavefields
          call compute_forces_viscoelastic_calling()
          ! 2. acoustic domain w/ adjoint wavefields
          call compute_forces_acoustic_calling()
        else
          ! non-coupled simulations
          ! (purely acoustic or elastic)
          if (ACOUSTIC_SIMULATION) call compute_forces_acoustic_calling()
          if (ELASTIC_SIMULATION) call compute_forces_viscoelastic_calling()
        endif

        ! backward/reconstructed wavefields
        ! acoustic solver
        ! (needs to be done after elastic one)
        if (ACOUSTIC_SIMULATION) call compute_forces_acoustic_backward_calling()
        ! elastic solver
        ! (needs to be done first, before poroelastic one)
        if (ELASTIC_SIMULATION) call compute_forces_viscoelastic_backward_calling()

      else
        ! forward simulations
        do istage = 1, NSTAGE_TIME_SCHEME
          if (USE_LDDRK) call update_displ_lddrk()
          ! 1. acoustic domain
          if(myrank == 0 .and. it <= 5 .and. DEBUG_COUPLING) then
            step8_profile_time = wtime()
            write(IMAIN,*) 'STEP8_PROFILE before acoustic forces',it,step8_profile_time - time_start
            flush(IMAIN)
          endif
          if (ACOUSTIC_SIMULATION) call compute_forces_acoustic_calling()
          if(myrank == 0 .and. it <= 5 .and. DEBUG_COUPLING) then
            step8_profile_time = wtime()
            write(IMAIN,*) 'STEP8_PROFILE after acoustic forces',it,step8_profile_time - time_start
            flush(IMAIN)
          endif
          ! 2. elastic domain
          if(myrank == 0 .and. it <= 5 .and. DEBUG_COUPLING) then
            step8_profile_time = wtime()
            write(IMAIN,*) 'STEP8_PROFILE before elastic forces',it,step8_profile_time - time_start
            flush(IMAIN)
          endif
          if (ELASTIC_SIMULATION) call compute_forces_viscoelastic_calling()
          if(myrank == 0 .and. it <= 5 .and. DEBUG_COUPLING) then
            step8_profile_time = wtime()
            write(IMAIN,*) 'STEP8_PROFILE after elastic forces',it,step8_profile_time - time_start
            flush(IMAIN)
          endif
        enddo
      endif

      ! poroelastic solver
      if (POROELASTIC_SIMULATION) call compute_forces_poroelastic_calling()

    else
      ! wavefields on GPU
      ! acoustic solver
      if (ACOUSTIC_SIMULATION) call compute_forces_acoustic_GPU_calling()
      ! elastic solver
      ! (needs to be done first, before poroelastic one)
      if (ELASTIC_SIMULATION) call compute_forces_viscoelastic_GPU_calling()
    endif

    ! restores last time snapshot saved for backward/reconstruction of wavefields
    ! note: this must be read in after the Newmark time scheme
    if (SIMULATION_TYPE == 3 .and. it == 1) then
      call it_read_forward_arrays()
    endif

    ! calculating gravity field at current timestep
    if (GRAVITY_SIMULATION) call gravity_timeseries()
!WENBO_S
    if(CUBED_SPHERE_PROJECTION.and.(COUPLING_TYPE.eq.1.or.COUPLING_TYPE.eq.3.or.&
                                    COUPLING_TYPE.eq.2)) then
       if(myrank == 0 .and. it <= 5 .and. DEBUG_COUPLING) then
         step8_profile_time = wtime()
         write(IMAIN,*) 'STEP8_PROFILE before teleseismic_coupling',it,step8_profile_time - time_start
         flush(IMAIN)
       endif
       call teleseismic_coupling()
       if(myrank == 0 .and. it <= 5 .and. DEBUG_COUPLING) then
         step8_profile_time = wtime()
         write(IMAIN,*) 'STEP8_PROFILE after teleseismic_coupling',it,step8_profile_time - time_start
         flush(IMAIN)
       endif
    end if
!WENBO_E
    ! write the seismograms with time shift (GPU_MODE transfer included)
    call write_seismograms()

    ! adjoint simulations: kernels
    if (SIMULATION_TYPE == 3) then
      call compute_kernels()
    endif

    ! outputs movie files
    call write_movie_output()
    if(myrank == 0 .and. it <= 5 .and. DEBUG_COUPLING) then
      step8_profile_time = wtime()
      write(IMAIN,*) 'STEP8_PROFILE it end',it,step8_profile_time - time_start
      flush(IMAIN)
    endif

    ! first step of noise tomography, i.e., save a surface movie at every time step
    ! modified from the subroutine 'write_movie_surface'
    if (NOISE_TOMOGRAPHY == 1) then
      call noise_save_surface_movie(displ,ibool,noise_surface_movie,it,NSPEC_AB,NGLOB_AB, &
                            num_free_surface_faces,free_surface_ispec,free_surface_ijk,Mesh_pointer,GPU_MODE)
    endif

    ! updates VTK window
    if (VTK_MODE) then
      call it_update_vtkwindow()
    endif

#ifdef DEBUG_COUPLED
    include "../../../add_to_iterate_time_2.F90"
#endif

  !
  !---- end of time iteration loop
  !
  enddo   ! end of main time loop


  ! close the huge file that contains a dump of all the time steps to disk
  if (EXACT_UNDOING_TO_DISK) close(IFILE_FOR_EXACT_UNDOING)

  call it_print_elapsed_time()

#ifdef DEBUG_COUPLED
    include "../../../add_to_iterate_time_3.F90"
#endif

  ! Transfer fields from GPU card to host for further analysis
  if (GPU_MODE) call it_transfer_from_GPU()

!----  close energy file
  if (OUTPUT_ENERGY .and. myrank == 0) close(IOUT_ENERGY)

  end subroutine iterate_time

!
!-------------------------------------------------------------------------------------------------
!

#ifdef DEBUG_COUPLED
    include "../../../add_to_iterate_time_4.F90"
#endif

!
!-------------------------------------------------------------------------------------------------
!

  subroutine it_transfer_from_GPU()

! transfers fields on GPU back onto CPU

  use specfem_par
  use specfem_par_elastic
  use specfem_par_acoustic

  implicit none

  ! to store forward wave fields
  if (SIMULATION_TYPE == 1 .and. SAVE_FORWARD) then

    ! acoustic potentials
    if (ACOUSTIC_SIMULATION) &
      call transfer_fields_ac_from_device(NGLOB_AB,potential_acoustic, &
                                          potential_dot_acoustic, potential_dot_dot_acoustic, &
                                          Mesh_pointer)

    ! elastic wavefield
    if (ELASTIC_SIMULATION) then
      call transfer_fields_el_from_device(NDIM*NGLOB_AB,displ,veloc,accel,Mesh_pointer)

      if (ATTENUATION) &
        call transfer_fields_att_from_device(Mesh_pointer, &
                    R_xx,R_yy,R_xy,R_xz,R_yz,size(R_xx), &
                    epsilondev_xx,epsilondev_yy,epsilondev_xy,epsilondev_xz,epsilondev_yz, &
                    size(epsilondev_xx))

    endif

  else if (SIMULATION_TYPE == 3) then

    ! to store kernels
    ! acoustic domains
    if (ACOUSTIC_SIMULATION) then
      ! only in case needed...
      !call transfer_b_fields_ac_from_device(NGLOB_AB,b_potential_acoustic, &
      !                      b_potential_dot_acoustic, b_potential_dot_dot_acoustic, Mesh_pointer)

      ! acoustic kernels
      call transfer_kernels_ac_to_host(Mesh_pointer,rho_ac_kl,kappa_ac_kl,NSPEC_AB)
    endif

    ! elastic domains
    if (ELASTIC_SIMULATION) then
      ! only in case needed...
      !call transfer_b_fields_from_device(NDIM*NGLOB_AB,b_displ,b_veloc,b_accel,Mesh_pointer)

      ! elastic kernels
      call transfer_kernels_el_to_host(Mesh_pointer,rho_kl,mu_kl,kappa_kl,cijkl_kl,NSPEC_AB)
    endif

    ! specific noise strength kernel
    if (NOISE_TOMOGRAPHY == 3) then
      call transfer_kernels_noise_to_host(Mesh_pointer,sigma_kl,NSPEC_AB)
    endif

    ! approximative Hessian for preconditioning kernels
    if (APPROXIMATE_HESS_KL) then
      if (ELASTIC_SIMULATION) call transfer_kernels_hess_el_tohost(Mesh_pointer,hess_kl,NSPEC_AB)
      if (ACOUSTIC_SIMULATION) call transfer_kernels_hess_ac_tohost(Mesh_pointer,hess_ac_kl,NSPEC_AB)
    endif

  endif

  ! from here on, no gpu data is needed anymore
  ! frees allocated memory on GPU
  call prepare_cleanup_device(Mesh_pointer,ACOUSTIC_SIMULATION,ELASTIC_SIMULATION, &
                              STACEY_ABSORBING_CONDITIONS,NOISE_TOMOGRAPHY,COMPUTE_AND_STORE_STRAIN, &
                              ATTENUATION,ANISOTROPY,APPROXIMATE_OCEAN_LOAD, &
                              APPROXIMATE_HESS_KL)

  end subroutine it_transfer_from_GPU

!
!-------------------------------------------------------------------------------------------------
!

  subroutine it_update_vtkwindow()

  implicit none

  stop 'it_update_vtkwindow() not implemented for this code yet'

  end subroutine it_update_vtkwindow

!
!-------------------------------------------------------------------------------------------------
!

  subroutine it_read_forward_arrays()

  use specfem_par
  use specfem_par_acoustic
  use specfem_par_elastic
  use specfem_par_poroelastic

  implicit none

  integer :: ier

! restores last time snapshot saved for backward/reconstruction of wavefields
! note: this is done here after the Newmark time scheme, otherwise the indexing for sources
!          and adjoint sources will become more complicated
!          that is, index it for adjoint sources will match index NSTEP - 1 for backward/reconstructed wavefields
  if (ADIOS_FOR_FORWARD_ARRAYS) then
    call read_forward_arrays_adios()
  else
    ! reads in wavefields
    open(unit=IIN,file=trim(prname)//'save_forward_arrays.bin',status='old', &
          action='read',form='unformatted',iostat=ier)
    if (ier /= 0) then
      print *,'error: opening save_forward_arrays'
      print *,'path: ',trim(prname)//'save_forward_arrays.bin'
      call exit_mpi(myrank,'error open file save_forward_arrays.bin')
    endif

    if (ACOUSTIC_SIMULATION) then
      read(IIN) b_potential_acoustic
      read(IIN) b_potential_dot_acoustic
      read(IIN) b_potential_dot_dot_acoustic
    endif

    ! elastic wavefields
    if (ELASTIC_SIMULATION) then
      read(IIN) b_displ
      read(IIN) b_veloc
      read(IIN) b_accel
      ! memory variables if attenuation
      if (ATTENUATION) then
        read(IIN) b_R_trace
        read(IIN) b_R_xx
        read(IIN) b_R_yy
        read(IIN) b_R_xy
        read(IIN) b_R_xz
        read(IIN) b_R_yz
        read(IIN) b_epsilondev_trace
        read(IIN) b_epsilondev_xx
        read(IIN) b_epsilondev_yy
        read(IIN) b_epsilondev_xy
        read(IIN) b_epsilondev_xz
        read(IIN) b_epsilondev_yz
      endif
    endif

    ! poroelastic wavefields
    if (POROELASTIC_SIMULATION) then
      read(IIN) b_displs_poroelastic
      read(IIN) b_velocs_poroelastic
      read(IIN) b_accels_poroelastic
      read(IIN) b_displw_poroelastic
      read(IIN) b_velocw_poroelastic
      read(IIN) b_accelw_poroelastic
    endif

    close(IIN)
  endif

  if (GPU_MODE) then
    if (ACOUSTIC_SIMULATION) then
    ! transfers fields onto GPU
      call transfer_b_fields_ac_to_device(NGLOB_AB,b_potential_acoustic, &
                                          b_potential_dot_acoustic, &
                                          b_potential_dot_dot_acoustic, &
                                          Mesh_pointer)
    endif
    ! elastic wavefields
    if (ELASTIC_SIMULATION) then
      ! puts elastic wavefield to GPU
      call transfer_b_fields_to_device(NDIM*NGLOB_AB,b_displ,b_veloc,b_accel,Mesh_pointer)
      ! memory variables if attenuation
      if (ATTENUATION) then
        call transfer_b_fields_att_to_device(Mesh_pointer, &
                           b_R_xx,b_R_yy,b_R_xy,b_R_xz,b_R_yz, &
                           size(b_R_xx), &
                           b_epsilondev_xx,b_epsilondev_yy,b_epsilondev_xy, &
                           b_epsilondev_xz,b_epsilondev_yz, &
                           size(b_epsilondev_xx))
      endif
    endif
  endif

  end subroutine it_read_forward_arrays
