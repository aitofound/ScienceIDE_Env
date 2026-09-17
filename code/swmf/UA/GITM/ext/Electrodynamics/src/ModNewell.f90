! Copyright 2021, the GITM Development Team (see srcDoc/dev_team.md for members)
! Full license can be found in LICENSE

module ModNewell
  ! use ModInputs
  use ModConst
  use ModCharSize
  use ModErrors
  implicit none

  integer, parameter        :: iCharLenNewell_ = iCharLenIE_

  character(len=iCharLenNewell_) :: cFileDiffef = "diff.txt"
  character(len=iCharLenNewell_) :: cFileDiffnf = "diff_n.txt"
  character(len=iCharLenNewell_) :: cFileDiffp = "prob_b_diff.txt"
  character(len=iCharLenNewell_) :: cFileMonoef = "mono.txt"
  character(len=iCharLenNewell_) :: cFileMononf = "mono_n.txt"
  character(len=iCharLenNewell_) :: cFileMonop = "prob_b_mono.txt"
  character(len=iCharLenNewell_) :: cFileWaveef = "wave.txt"
  character(len=iCharLenNewell_) :: cFileWavenf = "wave_n.txt"
  character(len=iCharLenNewell_) :: cFileWavep = "prob_b_wave.txt"
  character(len=iCharLenNewell_) :: cFileIonsef = "ions.txt"
  character(len=iCharLenNewell_) :: cFileIonsnf = "ions_n.txt"

  integer, parameter :: nMltsNewell = 96
  integer, parameter :: nMlatsNewell = 160
  integer, parameter :: minLat = 50
  real :: dlat_newell = 80.0/nMlatsNewell ! Only from 50-90, both hemispheres
  real :: dmlt_newell = 24.0/nMltsNewell
  real, dimension(nMlatsNewell) :: MlatsNewell
  real, dimension(nMltsNewell) :: MltsNewell
  integer, parameter :: nProbs = 3
  integer, parameter :: ndF = 12

  real, dimension(nMltsNewell, nMlatsNewell) :: &
    B1aDiff, B2aDiff, B1aDiffn, B2aDiffn, rFaDiff, rFaDiffn, B1pDiff, B2pDiff, &
    B1aMono, B2aMono, B1aMonon, B2aMonon, rFaMono, rFaMonon, B1pMono, B2pMono, &
    B1aWave, B2aWave, B1aWaven, B2aWaven, rFaWave, rFaWaven, B1pWave, B2pWave, &
    B1aIons, B2aIons, B1aIonsn, B2aIonsn, rFaIons, rFaIonsn, &
    ProbDiffTotal, ProbMonoTotal, ProbWaveTotal, ProbIonsTotal, &
    ElectronEFluxDiffuse, ElectronAvgEnergyDiffuse, &
    ElectronEFluxMono, ElectronAvgEnergyMono, &
    ElectronEFluxWave, ElectronAvgEnergyWave, &
    IonNumberFlux = 0.0, IonEnergyFlux = 0.0, IonAvgEnergy = 0.0, &
    Area

  real, dimension(ndF, nMltsNewell, nMlatsNewell) :: &
    ProbDiff, ProbMono, ProbWave

  ! Settings are modified in init_get_potential!

  real    :: dFdt! = 1000.0
  integer :: dFBin

contains

  ! -------------------------------------------------------------------

  subroutine init_newell(dir, iVerbose)
    use ModConst, only: cPi
    character(len=*), intent(in) :: dir
    integer, intent(in) :: iVerbose
    integer :: iLat

    ! call report("Newell Aurora Initializing", 2)
    if (iVerbose > 2) &
      write(*, *) "reading regression files"
    call read_all_regression_files(dir)
    if (iVerbose > 2) &
      write(*, *) "reading probability files"
    call read_all_probability_files(dir)

    ! This assumes about 500 km altitude and 1/2 deg and 1/4 hour resolution
    area = 120.0*120.0*0.5*3.75

    do iLat = 1, nMlatsNewell/2
      MlatsNewell(iLat) = -(90.0 - 0.5*(iLat - 1))*cPi/180.0
      MlatsNewell(iLat + nMlatsNewell/2) = (50.0 + 0.5*(iLat - 1))*cPi/180.0

      area(:, iLat) = area(:, iLat)*cos(MlatsNewell(iLat))
      area(:, iLat + nMlatsNewell/2) = area(:, iLat)*cos(MlatsNewell(iLat + nMlatsNewell/2))
    enddo

  end subroutine init_newell

  ! -------------------------------------------------------------------

  ! -------------------------------------------------------------------

  subroutine calc_flux(ProbTotal, b1a, b2a, Flux)

    real, dimension(nMltsNewell, nMlatsNewell), intent(in)  :: b1a, b2a
    real, dimension(nMltsNewell, nMlatsNewell), intent(in)  :: ProbTotal
    real, dimension(nMltsNewell, nMlatsNewell), intent(inout) :: Flux

    where (ProbTotal > 0) &
      Flux = (dFdt*b2a + b1a)*ProbTotal
    where (Flux < 0) Flux = 0.0

  end subroutine calc_flux

  ! -------------------------------------------------------------------

  subroutine calc_probability(Prob, b1p, b2p, ProbTotal)

    real, dimension(nMltsNewell, nMlatsNewell), intent(in)      :: b1p, b2p
    real, dimension(ndF, nMltsNewell, nMlatsNewell), intent(in) :: Prob
    real, dimension(nMltsNewell, nMlatsNewell), intent(out)     :: ProbTotal

    integer :: idfp, idfm
    integer :: iMlt, iMLat

    ProbTotal = b1p + b2p*dFdt
    where (ProbTotal < 0.0) ProbTotal = 0.0
    where (ProbTotal > 1.0) ProbTotal = 1.0

    do iMlt = 1, nMltsNewell
      do iMlat = 1, nMlatsNewell
        if (b1p(iMlt, iMlat) == 0 .and. b2p(iMlt, iMlat) == 0) then
          ProbTotal(iMlt, iMlat) = Prob(dfBin, iMlt, iMlat)
          if (ProbTotal(iMlt, iMlat) == 0.0) then
            idfp = dfBin + 1
            idfm = dfBin - 1
            if (idfm < 1) idfm = dfBin + 2
            if (idfp > ndF) idfp = dfBin - 2
            ProbTotal(iMlt, iMlat) = &
              (Prob(idfm, iMlt, iMlat) + Prob(idfp, iMlt, iMlat))/2
          endif
        endif
      enddo
    enddo

  end subroutine calc_probability

  ! -------------------------------------------------------------------

  subroutine calc_dfdt(by, bz, vx)

    use ModConst, only: cPi

    real, intent(in) :: by, bz, vx
    real :: dFAve, dFStep
    real :: v, sintc, bt, tc, bzt

    bzt = bz
    bt = sqrt(by**2 + bz**2)
    v = abs(vx)

    if (bzt == 0.0) bzt = 0.001
    tc = atan2(by, bzt)
    if (bt*cos(tc)*bz < 0.0) tc = tc + cPi

    sintc = abs(sin(tc/2.))

    dFdt = (v**1.33333)*(sintc**2.66667)*(BT**0.66667)

    dfave = 4421.0
    dfStep = dfAve/(ndF - 1)
    dfbin = min(max(floor(dFdt/dfStep), 1), ndF)

  end subroutine calc_dfdt

  ! -------------------------------------------------------------------

  subroutine smooth(value)

    implicit none

    real, dimension(nMltsNewell, nMlatsNewell), intent(inout) :: value
    real, dimension(nMltsNewell, nMlatsNewell) :: valueout

    integer :: nPL = 2, nPM = 2, nMin = 2
    integer :: iMlt, iLat, iM, iL, n, iMa, iLa, iHem
    integer :: iLatStart, iLatEnd
    real    :: ave, std

    ! for removing bad points, we want the zone of consideration to be at
    ! least 2 cells on each side (i.e., 5x5).  For averaging, allow only 1 cell.
    nMin = 1
    ! How many points to average over in Lat and MLT.
    ! Newell is at 1/2 deg resolution in lat, so averaging would be over
    ! nPL = max(1, floor(180.0/float(nMlatsNewell) + 0.499))
    ! no *2, because this is for each side

    ! Newell is at 1/4 hour MLT, which is 3.75 deg so averaging would be over
    ! nPM = max(1, floor(360.0/float(nMltsNewell)/3.75/2 + 0.499))

    valueout = value*0.0
    do iMlt = 1, nMltsNewell
      do iHem = 1, 2
        if (iHem == 1) then
          iLatStart = nPl + 1
          iLatEnd = nMlatsNewell/2 - nPl - 1
        endif
        if (iHem == 2) then
          iLatStart = nMlatsNewell/2 + nPl + 1
          iLatEnd = nMlatsNewell - nPl - 1
        endif

        do iLat = iLatStart, iLatEnd
          if (value(iMlt, iLat) > 0.0) then
            n = 0
            ave = 0.0
            do iM = iMlt - nPM, iMlt + nPM
              iMa = iM
              iMa = mod(iMa + nMltsNewell, nMltsNewell)
              if (iMa == 0) iMa = nMltsNewell
              do iLa = iLat - nPL, iLat + nPL
                if (value(iMa, iLa) > 0.0) then
                  ave = ave + value(iMa, iLa)
                  n = n + 1
                endif
              enddo
            enddo
            if (n > (2*nPL + 1)*(2*nPM + 1)/2) then
              ave = ave/n
              std = 0.0
              do iM = iMlt - nPM, iMlt + nPM
                iMa = iM
                iMa = mod(iMa + nMltsNewell, nMltsNewell)
                if (iMa == 0) iMa = nMltsNewell
                do iLa = iLat - nPL, iLat + nPL
                  if (value(iMa, iLa) > 0.0) then
                    std = std + (ave - value(iMa, iLa))**2
                  endif
                enddo
              enddo
              std = sqrt(std/n)
              ! We only want to kill points that are 2 stdev ABOVE the average
              ! value.
              if (abs(value(iMlt, iLat) - ave) > 2*std) then
                valueout(iMlt, iLat) = ave
              else
                valueout(iMlt, iLat) = value(iMlt, iLat)
              endif
            endif
          endif
        enddo
      enddo
    enddo

    value = valueout

  end subroutine smooth

  ! -------------------------------------------------------------------

  subroutine read_single_regression_file(cFile, rFa, b1a, b2a)

    !   use ModInputs, only: iInputUnit_

    character(len=*), intent(in) :: cFile
    real, dimension(nMltsNewell, nMlatsNewell), intent(out) :: rFa, b1a, b2a
    integer :: year0, day0, year1, day1, nFiles, sf0
    integer :: iMlt, iMlat, i, j, iError, iInputUnit_ = 9

    iError = 0

    open(iInputUnit_, file=cFile, status="old", iostat=iError)

    if (iError /= 0) then
      call set_error("Error in read_single_regression_file")
      call set_error(trim(cFile))
    endif

    read(iInputUnit_, *, iostat=iError) year0, day0, year1, day1, nFiles, sf0

    if (iError /= 0) then
      call set_error("Error in read_single_regression_file")
      call set_error(trim(cFile))
    endif

    do iMlt = 1, nMltsNewell
      do iMlat = 1, nMlatsNewell
        read(iInputUnit_, *, iostat=iError) &
          i, j, b1a(iMlt, iMlat), b2a(iMlt, iMlat), rfa(iMlt, iMlat)
        if (iError /= 0) then
          call set_error("Error in read_single_regression_file")
          call set_error(trim(cFile))
        endif
      enddo
    enddo

    close(iInputUnit_)

  end subroutine read_single_regression_file

  ! -------------------------------------------------------------------

  subroutine read_single_probability_file(cFile, b1p, b2p, Prob)

    character(len=*), intent(in) :: cFile
    real, dimension(nMltsNewell, nMlatsNewell), intent(out) :: b1p, b2p
    real, dimension(ndF, nMltsNewell, nMlatsNewell), intent(out) :: Prob
    integer :: year0, day0, year1, day1, nFiles, sf0
    integer :: iMlt, iMlat, idF, iError, iInputUnit_ = 8

    iError = 0

    open(iInputUnit_, file=cFile, status="old", iostat=iError)

    if (iError /= 0) then
      write(*, *) "Error in read_single_probability_file"
      call set_error(trim(cFile)//" cannot be opened")
    endif

    read(iInputUnit_, *, iostat=iError) year0, day0, year1, day1, nFiles, sf0

    if (iError /= 0) then
      write(*, *) "Error in read_single_probability_file"
      call set_error(trim(cFile)//" cannot read first line")
    endif

    do iMlt = 1, nMltsNewell
      do iMlat = 1, nMlatsNewell
        read(iInputUnit_, *, iostat=iError) &
          b1p(iMlt, iMlat), b2p(iMlt, iMlat)
        if (iError /= 0) then
          write(*, *) "Error in read_single_probability_file:", iMlt, iMlat
          call set_error(trim(cFile)//" error reading file")
        endif
      enddo
    enddo

    do iMlt = 1, nMltsNewell
      do iMlat = 1, nMlatsNewell
        do idF = 1, ndF
          read(iInputUnit_, *, iostat=iError) Prob(idF, iMlt, iMlat)
          if (iError /= 0) then
            write(*, *) "Error in read_single_probability_file:", idF, iMlt, iMlat
            call set_error(trim(cFile)//" error reading file")
          endif
        enddo
      enddo
    enddo

    close(iInputUnit_)

  end subroutine read_single_probability_file

  ! -------------------------------------------------------------------

  subroutine read_all_regression_files(dir)

    character(len=*), intent(in) :: dir

    character(len=iCharLenNewell_) :: cFile

    cFile = cFileDiffef
    call merge_str(dir, cFile)
    call read_single_regression_file(cFile, rFaDiff, b1aDiff, b2aDiff)
    cFile = cFileDiffnf
    call merge_str(dir, cFile)
    call read_single_regression_file(cFile, rFaDiffn, b1aDiffn, b2aDiffn)

    cFile = cFileMonoef
    call merge_str(dir, cFile)
    call read_single_regression_file(cFile, rFaMono, b1aMono, b2aMono)
    cFile = cFileMononf
    call merge_str(dir, cFile)
    call read_single_regression_file(cFile, rFaMonon, b1aMonon, b2aMonon)

    cFile = cFileWaveef
    call merge_str(dir, cFile)
    call read_single_regression_file(cFile, rFaWave, b1aWave, b2aWave)
    cFile = cFileWavenf
    call merge_str(dir, cFile)
    call read_single_regression_file(cFile, rFaWaven, b1aWaven, b2aWaven)

    cFile = cFileIonsef
    call merge_str(dir, cFile)
    call read_single_regression_file(cFile, rFaIons, b1aIons, b2aIons)
    cFile = cFileIonsnf
    call merge_str(dir, cFile)
    call read_single_regression_file(cFile, rFaIonsn, b1aIonsn, b2aIonsn)

  end subroutine read_all_regression_files

  ! -------------------------------------------------------------------

  subroutine read_all_probability_files(dir)
    character(len=*), intent(in) :: dir

    character(len=iCharLenNewell_) :: cFile

    cFile = cFileDiffp
    call merge_str(dir, cFile)
    call read_single_probability_file(cFile, b1pDiff, b2pDiff, ProbDiff)

    cFile = cFileMonop
    call merge_str(dir, cFile)
    call read_single_probability_file(cFile, b1pMono, b2pMono, ProbMono)

    cFile = cFileWavep
    call merge_str(dir, cFile)
    call read_single_probability_file(cFile, b1pWave, b2pWave, ProbWave)

  end subroutine read_all_probability_files

  subroutine update_newell_model(by, bz, vx)
    real, intent(in) :: by, bz, vx
    integer :: iLon, iLat, iMlat, iMlt, iMlat2
    real :: numflux

    ! Holds intermediate values
    real, dimension(nMltsNewell, nMlatsNewell) :: &
      eNumFluxDiff, eEFluxDiff, eAvgEnergyDiff, &
      eNumFluxMono, eEFluxMono, eAvgEnergyMono, &
      eNumFluxWave, eEFluxWave, eAvgEnergyWave, &
      iNumFlux, iEFlux, iAvgEnergy

    call calc_dfdt(by, bz, vx)

    call calc_probability(ProbDiff, B1pDiff, B2pDiff, ProbDiffTotal)
    call calc_probability(ProbMono, B1pMono, B2pMono, ProbMonoTotal)
    call calc_probability(ProbWave, B1pWave, B2pWave, ProbWaveTotal)
    ProbIonsTotal = 1.0

    call calc_flux(ProbDiffTotal, B1aDiff, B2aDiff, eEFluxDiff)
    call calc_flux(ProbDiffTotal, B1aDiffn, B2aDiffn, eNumFluxDiff)

    call calc_flux(ProbMonoTotal, B1aMono, B2aMono, eEFluxMono)
    call calc_flux(ProbMonoTotal, B1aMonon, B2aMonon, eNumFluxMono)

    !       if (iDebugLevel > -1) then
    !          do iMlat = 1, nMLats
    !             write(*,*) "from pat: ",iMlat,dfdt,&
    !                  ProbMonoTotal(8,iMlat), B1aMono(8,iMlat), B2aMono(8,iMlat), &
    !                  eEFluxMono(8,iMlat), eNumFluxMono(8,iMlat)
    !          enddo
    !       endif

    call calc_flux(ProbWaveTotal, B1aWave, B2aWave, eEFluxWave)
    call calc_flux(ProbWaveTotal, B1aWaven, B2aWaven, eNumFluxWave)

    call calc_flux(ProbIonsTotal, B1aIons, B2aIons, iEFlux)
    call calc_flux(ProbIonsTotal, B1aIonsn, B2aIonsn, iNumFlux)

    where (eEFluxDiff > 10.0) eEFluxDiff = 0.5
    where (eEFluxDiff > 5.0) eEFluxDiff = 5.0
    where (eEFluxMono > 10.0) eEFluxMono = 0.5
    where (eEFluxMono > 5.0) eEFluxMono = 5.0
    where (eEfluxWave > 10.0) eEfluxWave = 0.5
    where (eEfluxWave > 5.0) eEfluxWave = 5.0

    where (iEflux > 4.0) iEflux = 0.25
    where (iEflux > 2.0) iEflux = 2.0

    where (eNumFluxDiff > 2.0e10) eNumFluxDiff = 0.0
    where (eNumFluxDiff > 2.0e9) eNumFluxDiff = 1.0e9
    where (eNumFluxMono > 2.0e10) eNumFluxMono = 0.0
    where (eNumFluxMono > 2.0e9) eNumFluxMono = 1.0e9
    where (eNumFluxWave > 2.0e10) eNumFluxWave = 0.0
    where (eNumFluxWave > 2.0e9) eNumFluxWave = 1.0e9

    where (iNumFlux > 5.0e8) iNumFlux = 0.0
    where (iNumFlux > 1.0e8) iNumFlux = 1.0e8

    ! GITM only called smooth if (DoNewellRemoveSpikes_ .or. UseNewellAveraged_)
    ! We will always do it.

    call smooth(eEFluxDiff)
    call smooth(eNumFluxDiff)

    call smooth(eEFluxMono)
    call smooth(eNumFluxMono)

    call smooth(eEfluxWave)
    call smooth(eNumFluxWave)

    call smooth(iEflux)
    call smooth(iNumFlux)

    do iLon = 1, nMltsNewell
      do iLat = 1, nMlatsNewell

        iMlt = iLon
        iMlat = iLat

        if (iMlat > nMlatsNewell/2) then
          iMlat2 = iMlat - nMlatsNewell/2
        else
          iMlat2 = iMlat + nMlatsNewell/2
        endif

        iMlat2 = min(max(iMlat2, 1), nMlatsNewell)

        ! ===================== !
        !  Diffuse Energy Flux  !
        ! ===================== !

        ! Add North and South together
        ElectronEFluxDiffuse(iLon, iLat) = &
          eEFluxDiff(iMlt, iMlat) + eEFluxDiff(iMlt, iMlat2)

        ! If there are values in both hemisphere, then divide by 2
        if (eEFluxDiff(iMlt, iMlat)* &
            eEFluxDiff(iMlt, iMlat2) /= 0) then
          ElectronEFluxDiffuse(iLon, iLat) = &
            ElectronEFluxDiffuse(iLon, iLat)/2.0
        else
          ElectronEFluxDiffuse(iLon, iLat) = &
            eEFluxDiff(iMlt, iMlat)
        endif

        ! ===================== !
        !  Diffuse Avg Energy   !
        ! ===================== !

        ! Add North and South together
        numflux = eNumFluxDiff(iMlt, iMlat) + eNumFluxDiff(iMlt, iMlat2)

        ! If there are values in both hemisphere, then divide by 2
        if (eNumFluxDiff(iMlt, iMlat)* &
            eNumFluxDiff(iMlt, iMlat2) /= 0) then
          numflux = numflux/2
        else
          numflux = eNumFluxDiff(iMlt, iMlat)
        endif

        if (numflux /= 0) then
          ElectronAvgEnergyDiffuse(iLon, iLat) = &
            ElectronEFluxDiffuse(iLon, iLat)/numflux* &
            6.242e11/1000.0 ! ergs -> keV
        endif

        ! ===================== !
        !   Mono Energy Flux    !
        ! ===================== !

        ! Add North and South together
        ElectronEFluxMono(iLon, iLat) = &
          eEFluxMono(iMlt, iMlat) + &
          eEFluxMono(iMlt, iMlat2)

        ! If there are values in both hemisphere, then divide by 2
        if (eEFluxMono(iMlt, iMlat)* &
            eEFluxMono(iMlt, iMlat2) /= 0) then
          ElectronEFluxMono(iLon, iLat) = &
            ElectronEFluxMono(iLon, iLat)/2.0
        else
          ElectronEFluxMono(iLon, iLat) = &
            eEFluxMono(iMlt, iMlat)
        endif

        ! ===================== !
        !    Mono Avg Energy    !
        ! ===================== !

        ! Add North and South together
        numflux = eNumFluxMono(iMlt, iMlat) + eNumFluxMono(iMlt, iMlat2)

        ! If there are values in both hemisphere, then divide by 2
        if (eNumFluxMono(iMlt, iMlat)* &
            eNumFluxMono(iMlt, iMlat2) /= 0) then
          numflux = numflux/2.0
        else
          numflux = eNumFluxMono(iMlt, iMlat)
        endif

        if (numflux /= 0) then
          ElectronAvgEnergyMono(iLon, iLat) = &
            ElectronEFluxMono(iLon, iLat)/numflux* &
            6.242e11/1000.0 ! ergs -> keV
        endif

        ! ===================== !
        !   Wave Energy Flux    !
        ! ===================== !

        ! Add North and South together
        ElectronEFluxWave(iLon, iLat) = &
          eEfluxWave(iMlt, iMlat) + &
          eEfluxWave(iMlt, iMlat2)

        ! If there are values in both hemisphere, then divide by 2
        if (eEfluxWave(iMlt, iMlat)* &
            eEfluxWave(iMlt, iMlat2) /= 0) then
          ElectronEFluxWave(iLon, iLat) = &
            ElectronEFluxWave(iLon, iLat)/2.0
        else
          ElectronEFluxWave(iLon, iLat) = &
            eEfluxWave(iMlt, iMlat)
        endif

        ! ===================== !
        !    Wave Avg Energy    !
        ! ===================== !

        ! Add North and South together
        numflux = eNumFluxWave(iMlt, iMlat) + eNumFluxWave(iMlt, iMlat2)

        ! If there are values in both hemisphere, then divide by 2
        if (eNumFluxWave(iMlt, iMlat)* &
            eNumFluxWave(iMlt, iMlat2) /= 0) then
          numflux = numflux/2.0
        else
          numflux = eNumFluxWave(iMlt, iMlat)
        endif

        if (numflux /= 0) then
          ElectronAvgEnergyWave(iLon, iLat) = &
            ElectronEFluxWave(iLon, iLat)/numflux* &
            6.242e11/1000.0 ! ergs -> keV
        endif

        ! ===================== !
        !   Ion Energy Flux     !
        ! ===================== !

        ! Add North and South together
        IonEnergyFlux(iLon, iLat) = &
          iEFlux(iMlt, iMlat) + iEFlux(iMlt, iMlat2)

        ! If there are values in both hemisphere, then divide by 2
        if (iEflux(iMlt, iMlat)* &
            iEflux(iMlt, iMlat2) /= 0) then
          IonEnergyFlux(iLon, iLat) = &
            IonEnergyFlux(iLon, iLat)/2.0
        else
          IonEnergyFlux(iLon, iLat) = &
            iEflux(iMlt, iMlat)
        endif

        ! ===================== !
        !   Ion Avg Energy      !
        ! ===================== !

        ! Add North and South together
        numflux = iNumFlux(iMlt, iMlat) + iNumFlux(iMlt, iMlat2)

        ! If there are values in both hemisphere, then divide by 2
        if (iNumFlux(iMlt, iMlat)* &
            iNumFlux(iMlt, iMlat2) /= 0) then
          numflux = numflux/2
        else
          numflux = iNumFlux(iMlt, iMlat)
        endif

        if (numflux /= 0) then
          IonAvgEnergy(iLon, iLat) = &
            IonEnergyFlux(iLon, iLat)/numflux* &
            6.242e11/1000.0 ! ergs -> keV
        endif
      enddo
    enddo

  end subroutine update_newell_model

  subroutine get_newell_electron_diffuse(mlt, lat, eFluxOut, AveEOut)
    real, intent(in) :: mlt
    real, intent(in) :: lat
    real, intent(out) :: eFluxOut
    real, intent(out) :: AveEOut

    integer :: iMlt
    integer :: iLat

    if (abs(lat) < minLat) then
      eFluxOut = 0.0
      AveEOut = 2.0
      return
    endif
    iMlt = int(mlt/dmlt_newell) + 1
    iLat = int((abs(lat) - minLat)/dlat_newell) + 1

    eFluxOut = ElectronEFluxDiffuse(imlt, ilat)
    aveEout = ElectronAvgEnergyDiffuse(iMlt, iLat)
    return

  end subroutine get_newell_electron_diffuse

  subroutine get_newell_electron_mono(mlt, lat, eFluxOut, AveEOut)
    real, intent(in) :: mlt
    real, intent(in) :: lat
    real, intent(out) :: eFluxOut
    real, intent(out) :: AveEOut

    integer :: iMlt
    integer :: iLat

    if (abs(lat) < minLat) then
      eFluxOut = 0.0
      AveEOut = 2.0
      return
    endif
    iMlt = int(mlt/dmlt_newell) + 1
    iLat = int((abs(lat) - minLat)/dlat_newell) + 1

    eFluxOut = ElectronEFluxMono(imlt, ilat)
    aveEout = ElectronAvgEnergyMono(iMlt, iLat)
    return

  end subroutine get_newell_electron_mono

  subroutine get_newell_electron_wave(mlt, lat, eFluxOut, AveEOut)
    real, intent(in) :: mlt
    real, intent(in) :: lat
    real, intent(out) :: eFluxOut
    real, intent(out) :: AveEOut

    integer :: iMlt
    integer :: iLat

    if (abs(lat) < minLat) then
      eFluxOut = 0.0
      AveEOut = 2.0
      return
    endif
    iMlt = int(mlt/dmlt_newell) + 1
    iLat = int((abs(lat) - minLat)/dlat_newell) + 1

    eFluxOut = ElectronEFluxWave(iMlt, ilat)
    AveEOut = ElectronAvgEnergyWave(iMlt, iLat)
    return

  end subroutine get_newell_electron_wave

  subroutine get_newell_ion_diffuse(mlt, lat, eFluxOut, AveEOut)
    real, intent(in) :: mlt
    real, intent(in) :: lat
    real, intent(out) :: eFluxOut
    real, intent(out) :: AveEOut

    integer :: iMlt
    integer :: iLat

    if (abs(lat) < minLat) then
      eFluxOut = 0.0
      AveEOut = 12.0
      return
    endif
    iMlt = int(mlt/dmlt_newell) + 1
    iLat = int((abs(lat) - minLat)/dlat_newell) + 1

    eFluxOut = IonEnergyFlux(imlt, ilat)
    AveEOut = IonAvgEnergy(iMlt, iLat)

    return

  end subroutine get_newell_ion_diffuse

end module ModNewell
