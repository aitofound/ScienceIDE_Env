
  ! ------------------------------------------------------------
  ! set efield model
  subroutine set_efield_model(this, efield_model)
    class(ieModel) :: this
    character(len=*), intent(in) :: efield_model
    if (this%iDebugLevel > 0) &
      write(*, *) "=> Setting efield model to : ", trim(efield_model)
    this%iEfield_ = efield_interpret_name(efield_model)
    if (this%iDebugLevel > 0) &
      write(*, *) "=> That is model : ", this%iEfield_
  end subroutine set_efield_model

  ! ------------------------------------------------------------
  ! set aurora model
  subroutine set_aurora_model(this, aurora_model)
    class(ieModel) :: this
    character(len=*), intent(in) :: aurora_model
    if (this%iDebugLevel > 0) &
      write(*, *) "=> Setting aurora model to : ", trim(aurora_model)
    this%iAurora_ = aurora_interpret_name(aurora_model)
    if (this%iDebugLevel > 0) &
      write(*, *) "=> That is model : ", this%iAurora_
  end subroutine set_aurora_model

  ! ------------------------------------------------------------
  ! filename for north
  subroutine set_filename_north(this, filename)
    class(ieModel) :: this
    character(len=*), intent(in) :: filename
    if (this%iDebugLevel > 0) &
      write(*, *) "=> Setting north file name to : ", trim(filename)
    this%northFile = filename
    if (allocated(this%northFiles)) deallocate(this%northFiles)
    allocate(this%northFiles(1))
    this%northFiles(1) = filename
    this%nFilesNorth = 1
  end subroutine set_filename_north

  ! ------------------------------------------------------------
  ! filename for south
  subroutine set_filename_south(this, filename)
    class(ieModel) :: this
    character(len=*), intent(in) :: filename
    if (this%iDebugLevel > 0) &
      write(*, *) "=> Setting south file name to : ", trim(filename)
    this%southFile = filename
    if (allocated(this%southFiles)) deallocate(this%southFiles)
    allocate(this%southFiles(1))
    this%southFiles(1) = filename
    this%nFilesSouth = 1
  end subroutine set_filename_south

  ! ------------------------------------------------------------
  !  If the user has a list of filenames, you have to do things
  !   in the following order:
  !   - set the number of files
  !   - set the file names one by one
  !   this is because character lengths can be generic

  ! ------------------------------------------------------------
  ! set nfiles for north
  subroutine set_nfiles_north(this, nFiles)
    class(ieModel) :: this
    integer, intent(in) :: nFiles
    if (this%iDebugLevel > 0) &
      write(*, *) "=> Setting nFilesNorth to : ", nFiles
    this%nFilesNorth = nFiles
    if (allocated(this%northFiles)) deallocate(this%northFiles)
    allocate(this%northFiles(nFiles))
  end subroutine set_nfiles_north

  ! ------------------------------------------------------------
  ! filename for north
  subroutine set_filename_list_north(this, iFile, filename)
    class(ieModel) :: this
    integer, intent(in) :: iFile
    character(len=*), intent(in) :: filename
    if (this%iDebugLevel > 0) &
      write(*, *) "=> Setting north file name in list to : ", trim(filename)
    if (.not. allocated(this%northFiles)) then
      call set_error("Need to set number of AMIE files (north) before sending file names.")
      return
    endif
    if (iFile > this%nFilesNorth) then
      call set_error("iFile > nFilesNorth in set_filename_list_north.")
      return
    endif
    this%northFiles(iFile) = filename
  end subroutine set_filename_list_north

  ! ------------------------------------------------------------
  ! set nfiles for south
  subroutine set_nfiles_south(this, nFiles)
    class(ieModel) :: this
    integer, intent(in) :: nFiles
    if (this%iDebugLevel > 0) &
      write(*, *) "=> Setting nFilesSouth to : ", nFiles
    this%nFilesSouth = nFiles
    if (allocated(this%southFiles)) deallocate(this%southFiles)
    allocate(this%southFiles(nFiles))
  end subroutine set_nfiles_south

  ! ------------------------------------------------------------
  ! filename for south
  subroutine set_filename_list_south(this, iFile, filename)
    class(ieModel) :: this
    integer, intent(in) :: iFile
    character(len=*), intent(in) :: filename
    if (this%iDebugLevel > 0) &
      write(*, *) "=> Setting south file name in list to : ", trim(filename)
    if (.not. allocated(this%southFiles)) then
      call set_error("Need to set number of AMIE files (south) before sending file names.")
      return
    endif
    if (iFile > this%nFilesSouth) then
      call set_error("iFile > nFilesSouth in set_filename_list_south.")
      return
    endif
    this%southFiles(iFile) = filename
  end subroutine set_filename_list_south

  ! ------------------------------------------------------------
  ! set the directory for finding all of the coef files
  subroutine set_model_dir(this, dir)
    class(ieModel) :: this
    character(len=*), intent(in) :: dir
    if (this%iDebugLevel > 0) &
      write(*, *) "=> Setting model directory to : ", trim(dir)
    this%modelDir = dir
  end subroutine set_model_dir

  ! ------------------------------------------------------------
  ! run efield model

  subroutine run_potential_model(ie, potential)

    use ModAMIE_Interface, only: get_amie_potential
    class(ieModel) :: ie
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(out) :: potential
    real :: currentTilt = rBadValue, lastTilt = rBadValue
    real :: potVal

    integer :: iMLT, iLat
    potential = 0.0

    call ie%check_time()
    call ie%check_indices()

    if (.not. isOk) then
      call report_errors
      call set_error("Not ok, exiting run_potential_model without doing anything.")
      return
    endif

    if (ie%iEfield_ == iWeimer05_) call ie%weimer05(potential)

    if (ie%iEfield_ == iHepMay_) call ie%hepmay(potential)

    if (ie%iEfield_ == iAmiePot_) call get_amie_potential(potential)

    ie%isPotentialUpdated = .true.

    return
  end subroutine run_potential_model

  ! ------------------------------------------------------------
  ! run Weimer05
  subroutine run_weimer05_model(ie, potential)
    class(ieModel) :: ie
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(inout) :: potential
    real :: currentTilt = rBadValue, lastTilt = rBadValue
    real :: potVal

    integer :: iMLT, iLat

    do iLat = 1, ie%neednLats
      do iMLT = 1, ie%neednMLTs
        ! this is to check if we have changed hemispheres:
        currentTilt = ie%weimerTilt*sign(1.0, ie%needLats(iMlt, iLat))
        if (currentTilt .ne. lastTilt) then
          ! Only need to set up the model once, when everything
          ! stays the same (including the hemisphere!):
          call setmodel( &
            ie%needIMFBy*sign(1.0, ie%needLats(iMlt, iLat)), &
            ie%needIMFBz, &
            currentTilt, &
            ie%needSWV, &
            ie%needSWN, 'epot')
          lastTilt = currentTilt
        endif
        ! Run Weimer for specific lat and mlt:
        call epotval( &
          abs(ie%needLats(iMlt, iLat)), &
          ie%needMlts(iMlt, iLat), &
          0.0, &
          potVal)
        ! Store potential and convert to V:
        potential(iMlt, iLat) = potVal*1000.0
      enddo
    enddo

    return

  end subroutine run_weimer05_model

  ! ------------------------------------------------------------
  ! run Heppner Maynard model
  subroutine run_heppner_maynard_model(ie, potential)
    class(ieModel) :: ie
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(inout) :: potential
    real :: currentTilt = rBadValue, lastTilt = rBadValue
    real :: potVal, eTheta, ePhi

    integer :: iMLT, iLat, iFirst

    iFirst = 1
    do iLat = 1, ie%neednLats
      do iMLT = 1, ie%neednMLTs
          call hmrepot( &
            ie%needLats(iMlt, iLat), &
            ie%needMlts(iMlt, iLat), &
            ie%needIMFBy, &
            ie%needIMFBz, &
            ie%needKp, &
            iFirst, &
            eTheta, &
            ePhi, &
            potVal)
          potential(iMlt, iLat) = potVal*1000.0
          iFirst = iFirst + 1
      enddo
    enddo

    return
  end subroutine run_heppner_maynard_model

  ! ------------------------------------------------------------
  ! run aurora model
  ! Since there can be a lot of different types of aurora, and
  ! some models offer some things, while other models don't,
  ! we break the running & returning into separate parts (when possible).
  ! On the first get_aurora_* call after setting new times or indices,
  ! model outputs are updated for everything possible.

  subroutine run_aurora_model(ie)
    class(ieModel) :: ie
    integer :: iError = 0

    call ie%check_time()
    call ie%check_indices()

    if (ie%iAurora_ == iFTA_) call update_fta_model(ie%needAu, ie%needAl, iError)

    if (ie%iAurora_ == iOvationPrime_) call update_newell_model(ie%needImfBy, ie%needIMFBz, ie%needSWV)

    ie%isAuroraUpdated = .true.

    return

  end subroutine run_aurora_model

  subroutine run_aurora_model_electron_diffuse(ie, eflux, avee)
    use ModAMIE_Interface, only: &
      get_amie_electron_diffuse_eflux, &
      get_amie_electron_diffuse_avee
    class(ieModel) :: ie
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(out) :: eFlux
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(out) :: AveE
    integer :: iError = 0

    if (.not. ie%isAuroraUpdated) then
      call run_aurora_model(ie)
      ie%isAuroraUpdated = .true.
    endif

    eFlux = 0.001 ! ergs/cm2/s
    AveE = 1.0 ! keV

    if (allocated(ie%havePolarCap)) deallocate(ie%havePolarCap)
    allocate(ie%havePolarCap(ie%neednMlts, ie%neednLats), stat=iError)
    if (iError /= 0) then
      isOk = .false.
      call set_error("run_aurora_model - failed to allocate havepolarcap.")
      return
    endif

    ie%havePolarCap = 0.0
    ie%isFtaLimit = .false.
    if (ie%iAurora_ == iFTA_) call ie%fta(eFlux, AveE, ie%havePolarCap)
    ! These two models are the same, because they use the same
    if (ie%iAurora_ == iFRE_) call ie%hpi_pem(eFlux, AveE)
    if (ie%iAurora_ == iPEM_) call ie%hpi_pem(eFlux, AveE)

    if (ie%iAurora_ == iAmieAur_) then
      call get_amie_electron_diffuse_eflux(eFlux)
      call get_amie_electron_diffuse_avee(AveE)
    endif

    if (ie%iAurora_ == iOvationPrime_) call ie%ovation_e_diffuse(eFlux, AveE)

    return
  end subroutine run_aurora_model_electron_diffuse

  subroutine run_aurora_model_electron_mono(ie, eflux, avee)
    ! At the moment, this only works for ovation & AMIE Files...

    use ModAMIE_Interface, only: &
      get_amie_electron_mono_eflux, &
      get_amie_electron_mono_avee

    class(ieModel) :: ie
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(out) :: eFlux
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(out) :: AveE
    integer :: iError = 0

    if (.not. ie%isAuroraUpdated) then
      call run_aurora_model(ie)
      ie%isAuroraUpdated = .true.
    endif

    eFlux = 0.001 ! ergs/cm2/s
    AveE = 1.0 ! keV

    if (ie%iAurora_ == iAmieAur_) then
      call get_amie_electron_mono_eflux(eFlux)
      call get_amie_electron_mono_avee(AveE)
    endif

    if (ie%iAurora_ == iOvationPrime_) call ie%ovation_e_mono(eFlux, AveE)

    return
  end subroutine run_aurora_model_electron_mono

  subroutine run_aurora_model_electron_wave(ie, eflux, avee)
    ! At the moment, this only works for ovation & AMIE files...
    use ModAMIE_Interface, only: &
      get_amie_electron_wave_eflux, &
      get_amie_electron_wave_avee

    class(ieModel) :: ie
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(out) :: eFlux
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(out) :: AveE
    integer :: iError = 0

    if (.not. ie%isAuroraUpdated) then
      call run_aurora_model(ie)
      ie%isAuroraUpdated = .true.
    endif

    eFlux = 0.001 ! ergs/cm2/s
    AveE = 1.0 ! keV

    if (ie%iAurora_ == iAmieAur_) then
      call get_amie_electron_wave_eflux(eFlux)
      call get_amie_electron_wave_avee(AveE)
    endif

    if (ie%iAurora_ == iOvationPrime_) call ie%ovation_e_wave(eFlux, AveE)

    return
  end subroutine run_aurora_model_electron_wave

  subroutine run_aurora_model_ion_diffuse(ie, eflux, avee)
    ! At the moment, this only works for ovation and AMIE files...
    use ModAMIE_Interface, only: &
      get_amie_ion_diffuse_eflux, &
      get_amie_ion_diffuse_avee

    class(ieModel) :: ie
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(out) :: eFlux
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(out) :: AveE
    integer :: iError = 0

    if (.not. ie%isAuroraUpdated) then
      call run_aurora_model(ie)
      ie%isAuroraUpdated = .true.
    endif

    eFlux = 0.001 ! ergs/cm2/s
    AveE = 10.0 ! keV

    if (ie%iAurora_ == iAmieAur_) then
      call get_amie_ion_diffuse_eflux(eFlux)
      call get_amie_ion_diffuse_avee(AveE)
    endif

    if (ie%iAurora_ == iOvationPrime_) call ie%ovation_ion_diffuse(eFlux, AveE)

    return
  end subroutine run_aurora_model_ion_diffuse

  ! ------------------------------------------------------------
  ! These functions are for getting information that was
  ! derived when the auroral model was run
  ! ------------------------------------------------------------

  ! Supply the polar cap to the user:
  ! polarcap = 1 inside the polar cap, and 0 elsewhere.
  subroutine get_polarcap_results(ie, polarcap)
    use ModAMIE_Interface, only: get_amie_polar_cap

    class(ieModel) :: ie
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(out) :: polarcap

    polarcap = 0.0

    if (ie%iAurora_ == iAmieAur_) then
      call get_amie_polar_cap(polarcap)
    else
      ! this should be set in FTA model, then
      if (maxval(ie%havePolarCap) == 0) then
        isOk = .false.
        call set_error("polar cap is not set, be careful!")
      endif
      polarcap = ie%havePolarCap
    endif

    return
  end subroutine get_polarcap_results

  ! ------------------------------------------------------------
  ! run FTA Model, providing aveE and E-Flux
  subroutine run_fta_model(ie, eFlux, AveE, polarCap)
    class(ieModel) :: ie
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(inout) :: eFlux
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(inout) :: AveE
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(inout) :: polarCap
    real :: eFluxVal, AveEVal, polarCapVal
    integer :: iError = 0, iMlt, iLat
    logical :: isFtaLimitVal 

    if (iError /= 0) then
      call set_error('FTA Model update has an error!')
      return
    endif

    do iLat = 1, ie%neednLats - 1
      do iMLT = 1, ie%neednMLTs - 1
        call get_fta_model_result( &
          ie%needMlts(iMlt, iLat), &
          ie%needLats(iMlt, iLat), &
          eFluxVal, AveEVal, polarCapVal, &
          isFtaLimitVal,FtaAuVal,FtaAlVal,FtaAeVal)
        eFlux(iMlt, iLat) = eFluxVal
        AveE(iMlt, iLat) = AveEVal
        polarCap(iMlt, iLat) = polarCapVal
        ie%isFtaLimit = isFtaLimitVal
        ie%FtaAu = FtaAuVal
        ie%FtaAl = FtaAlVal
        ie%FtaAe = FtaAeVal
      enddo
    enddo
    return
  end subroutine run_fta_model

  ! ------------------------------------------------------------
  ! run HPI Model, providing aveE and E-Flux
  subroutine run_hpi_pem_model(ie, eFlux, AveE)
    class(ieModel) :: ie
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(inout) :: eFlux
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(inout) :: AveE
    real :: eFluxVal, AveEVal, hp
    integer :: iError = 0, iMlt, iLat

    eFlux = 0.0
    AveE = 2.0

    do iLat = 1, ie%neednLats
      do iMLT = 1, ie%neednMLTs
        if (ie%needLats(iMlt, iLat) > 0) then
          hp = ie%needHpN
        else
          hp = ie%needHpS
        endif
        call get_auroral_conductance( &
          ie%needMlts(iMlt, iLat), &
          ie%needLats(iMlt, iLat), &
          hp, &
          eFluxVal, AveEVal)
        eFlux(iMlt, iLat) = eFluxVal
        AveE(iMlt, iLat) = AveEVal
      enddo
    enddo
    return
  end subroutine run_hpi_pem_model

! ------------------------------------------------------------
  ! run OVATION Model, providing E-Flux & numflux (Diffuse, Mono, Wave ???)
  subroutine run_ovation_model_electron_diffuse(ie, eflux, AveE)
    class(ieModel) :: ie

    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(inout) :: eFlux
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(inout) :: AveE

    real :: eFluxVal, AveEVal, hp
    integer :: iError = 0, iMlt, iLat

    do iLat = 1, ie%neednLats - 1
      do iMLT = 1, ie%neednMLTs - 1
        call get_newell_electron_diffuse( &
          ie%needMlts(iMlt, iLat), &
          ie%needLats(iMlt, iLat), &
          eFluxVal, AveEVal)
        eFlux(iMlt, iLat) = eFluxVal
        AveE(iMlt, iLat) = AveEVal
      enddo
    enddo

    return

  end subroutine run_ovation_model_electron_diffuse

  subroutine run_ovation_model_electron_mono(ie, eflux, avee)
    class(ieModel) :: ie
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(inout) :: eFlux
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(inout) :: AveE

    real :: eFluxVal, AveEVal, hp
    integer :: iError = 0, iMlt, iLat

    do iLat = 1, ie%neednLats - 1
      do iMLT = 1, ie%neednMLTs - 1
        call get_newell_electron_mono( &
          ie%needMlts(iMlt, iLat), &
          ie%needLats(iMlt, iLat), &
          eFluxVal, AveEVal)
        eFlux(iMlt, iLat) = eFluxVal
        AveE(iMlt, iLat) = AveEVal
      enddo
    enddo

    return

  end subroutine run_ovation_model_electron_mono

  subroutine run_ovation_model_electron_wave(ie, eflux, avee)
    class(ieModel) :: ie
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(inout) :: eFlux
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(inout) :: AveE

    real :: eFluxVal, AveEVal, hp
    integer :: iError = 0, iMlt, iLat

    do iLat = 1, ie%neednLats - 1
      do iMLT = 1, ie%neednMLTs - 1
        call get_newell_electron_wave( &
          ie%needMlts(iMlt, iLat), &
          ie%needLats(iMlt, iLat), &
          eFluxVal, AveEVal)
        eFlux(iMlt, iLat) = eFluxVal
        AveE(iMlt, iLat) = AveEVal
      enddo
    enddo

    return
  end subroutine run_ovation_model_electron_wave

  subroutine run_ovation_model_ion_diffuse(ie, eflux, avee)
    class(ieModel) :: ie
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(inout) :: eFlux
    real, dimension(ie%neednMlts, &
                    ie%neednLats), intent(inout) :: AveE

    real :: eFluxVal, AveEVal, hp
    integer :: iError = 0, iMlt, iLat

    do iLat = 1, ie%neednLats - 1
      do iMLT = 1, ie%neednMLTs - 1
        call get_newell_ion_diffuse( &
          ie%needMlts(iMlt, iLat), &
          ie%needLats(iMlt, iLat), &
          eFluxVal, AveEVal)
        eFlux(iMlt, iLat) = eFluxVal
        AveE(iMlt, iLat) = AveEVal
      enddo
    enddo

    return

  end subroutine run_ovation_model_ion_diffuse
