module ModFTAModel

  use ModCharSize
  use ModErrors

  implicit none

  integer, parameter :: iCharLenFta_ = iCharLenIE_
  integer, parameter :: iCharLenLong_ = 1500
  character(len=iCharLenFta_) :: dir = "UA/DataIn/Aurora/FTA/"
  logical :: isInitialized = .false.
  logical :: isOkFta = .true.

  integer, parameter :: nMltsFta = 96
  integer, parameter :: nEnergies = 21
  integer, parameter :: nParams = 43

  ! For interpolation grid:

  integer, parameter :: nLatsFta = 120
  real, parameter :: minLat = 30.0
  real :: dLat = (90.0 - minLat)/nLatsFta
  real :: dMlt = 24.0/nMltsFta
  real, dimension(nLatsFta) :: lats_fixed_grid

  integer, parameter :: LunFta_ = 73

  character(len=10) :: emissions(2)

  real, dimension(nMltsFta, nEnergies, 2):: &
    kk_lat, kb_lat, bk_lat, bb_lat, &
    kk_ef, kb_ef, bk_ef, bb_ef, &
    kk_lat2, kb_lat2, kk_ef2, kb_ef2

  real, dimension(nMltsFta, nLatsFta) :: eFluxResult
  real, dimension(nMltsFta, nLatsFta) :: AveEResult
  real, dimension(nMltsFta, nLatsFta) :: LBHLResult
  real, dimension(nMltsFta, nLatsFta) :: LBHSResult
  real, dimension(nMltsFta, nLatsFta) :: PolarCapResult
  logical :: isFtaAualLimit = .false.
  real :: FtaAuVal, FtaAlVal, FtaAeVal

  real, dimension(nMltsFta) :: mlts_fixed_grid

  ! In case we need to adjust the grid:
  integer :: nLatsFta_mod
  real :: minLat_mod
  real :: dLat_mod = 0.5
  real, allocatable :: lbhl(:, :), lbhs(:, :), avee(:, :), eflux(:, :)

  real :: AL_split = 500.0

contains

  ! ------------------------------------------------------------------------
  !
  ! ------------------------------------------------------------------------

  subroutine get_fta_model_result(mlt, lat, eFluxOut, AveEOut, polarCapOut, &
                                 isFtaLimit,FAu,FAl,FAe)
    real, intent(in) :: mlt
    real, intent(in) :: lat
    real, intent(out) :: eFluxOut
    real, intent(out) :: AveEOut
    real, intent(out) :: polarCapOut
    real, intent(out) :: FAu,FAl,FAe
    logical, intent(out) :: isFtaLimit 
    integer :: iMlt
    integer :: iLat
    if (abs(lat) < minLat) then
      eFluxOut = 0.0
      AveEOut = 2.0
      polarCapOut = 0.0
      return
    endif
    iMlt = int(mlt/dMlt) + 1
    iLat = int((abs(lat) - minLat)/dLat) + 1
    ! In the FTA model, we take the closest value, since this is flux-based
    eFluxOut = eFluxResult(iMlt, iLat)
    AveEOut = AveEResult(iMlt, iLat)
    polarCapOut = polarCapResult(iMlt, iLat)
    isFtaLimit = isFtaAualLimit
    FAu = FtaAuVal
    FAl = FtaAlVal
    FAe = FtaAeVal
    return
  end subroutine get_fta_model_result

  ! ------------------------------------------------------------------------
  ! ------------------------------------------------------------------------

  subroutine update_fta_model(IOr_NeedAU, IOr_NeedAL,iError)

    real, intent(in) :: IOr_NeedAU
    real, intent(in) :: IOr_NeedAL
    integer, intent(out) :: iError


    real :: au, al, ae
    character(len=10) :: emis_type

    real, dimension(nMltsFta, nEnergies) :: mlats0_l, efs0_l, mlats0_s, efs0_s
    real, dimension(nMltsFta, nLatsFta) :: polarcap
    real :: al_lp, al_up
    logical :: any_width, any_offset
    logical, dimension(nMltsFta) :: lp
    real :: startMlt, endMlt

    iError = 0

    if (.not. isInitialized) then
      call initialize_fta(dir)
      isInitialized = .true.
      if (.not. isOkFta) then
        iError = 1
        call set_error("Error in initializing FTA model!")
        return
      endif
    endif

    au = IOr_NeedAU
    al = IOr_NeedAL

    ! Limit AL with AU  - Jun, 2025
    if (au > 1050.0) au = 1050
    if (au < 25.0) au = 25.0

    call limiter_al_lp(au, al_lp)
    if (al < al_lp) al = al_lp

    if (au > 750.0) then
      call limiter_al_up(au, al_up)
      if (al > al_up) al = al_up
    endif
    
    isFtaAualLimit = .false.
    if (au /= IOr_NeedAU) isFtaAualLimit = .true.
    if (al /= IOr_NeedAL) isFtaAualLimit = .true.

    ae = au - al

    FtaAuVal = au
    FtaAlVal = al
    FtaAeVal = ae

    emis_type = 'lbhl'
    call calc_emission_pattern(au, al, emis_type, mlats0_l, efs0_l)

    emis_type = 'lbhs'
    call calc_emission_pattern(au, al, emis_type, mlats0_s, efs0_s)

    ! locate minLat
    minLat_mod = min(minval(pack(mlats0_s, efs0_s > 0)), &
                     minval(pack(mlats0_l, efs0_l > 0)))
    minLat_mod = floor(minLat_mod/dLat)*dLat
    nLatsFta_mod = int((90 - minLat_mod)/dLat)

    ! adjust width
    lp = (mlats0_l(:, nEnergies) - mlats0_l(:, 1)) < 0.25
    any_width = any(lp)
    if (any_width) then
      call adjust_width(mlats0_l, lp)
    endif

    lp = (mlats0_s(:, nEnergies) - mlats0_s(:, 1)) < 0.25
    any_width = any(lp)
    if (any_width) then
      call adjust_width(mlats0_s, lp)
    endif

    ! adjust offsets
    lp = (mlats0_s(:, 1) - mlats0_l(:, nEnergies)) >= -0.25/20.0
    any_offset = any(lp)

    if (any_offset) then
      startMlt = minval(pack(mlts_fixed_grid, lp)) - dMlt
      endMlt = maxval(pack(mlts_fixed_grid, lp)) + dMlt
      call adjust_offset(mlats0_l, mlats0_s, startMlt, endMlt)
    endif

    lp = (mlats0_l(:, 1) - mlats0_s(:, nEnergies)) >= -0.25/20.0
    any_offset = any(lp)

    if (any_offset) then
      startMlt = minval(pack(mlts_fixed_grid, lp)) - dMlt
      endMlt = maxval(pack(mlts_fixed_grid, lp)) + dMlt
      call adjust_offset(mlats0_l, mlats0_s, startMlt, endMlt)
    endif

    allocate(lbhl(nMltsFta, nLatsFta))
    allocate(lbhs(nMltsFta, nLatsFta))
    allocate(eflux(nMltsFta, nLatsFta))
    allocate(avee(nMltsFta, nLatsFta))

    eflux = 0.0
    avee = 2.0
    lbhl = 0.0
    lbhs = 0.0

    call calc_full_patterns(mlats0_l, efs0_l, mlats0_s, efs0_s, &
                            lbhl, lbhs, eflux, avee, polarcap)

    LBHLResult = lbhl
    LBHSResult = lbhs
    eFluxResult = eflux
    AveEResult = avee
    polarCapResult = polarcap

    deallocate(lbhl)
    deallocate(lbhs)
    deallocate(eflux)
    deallocate(avee)

  end subroutine update_fta_model

  ! -----------------------------------------------------------------
  ! Generic reading of a file
  ! -----------------------------------------------------------------

  subroutine read_coef_file(NameOfIndexFile, tmp2)

    implicit none

    integer :: iError = 0
    integer :: LunIndices_
    character(len=iCharLenFta_), intent(in) :: NameOfIndexFile
    character(len=iCharLenLong_) :: line

    logical :: done
    integer :: ipt
    DOUBLE PRECISION, dimension(nParams, nMltsFta) :: tmp
    DOUBLE PRECISION, dimension(nMltsFta, nParams), intent(out) :: tmp2

    LunIndices_ = LunFta_

    tmp2 = 0.0
    open(LunIndices_, file=NameOfIndexFile, status="old", iostat=ierror)

    if (ierror .ne. 0) then
      write(*, *) "Could not find file : ", NameOfIndexFile
      isOkFta = .false.
      iError = 1
      call set_error("Error in reading FTA file: ")
      call set_error(NameOfIndexFile)
      return
    endif

    done = .false.

    ipt = 1
    do while (.not. done)

      if (ipt < 9) then
        read(LunIndices_, '(a)', iostat=ierror) line
        if (ierror .ne. 0) done = .true.
      else

        done = .true.
      endif

      ipt = ipt + 1

    enddo

    read(LunIndices_, *) tmp

    tmp2 = transpose(tmp)

    close(LunIndices_)

  end subroutine read_coef_file

  ! -----------------------------------------------------------------
  ! Read all of the files for the FTA model
  ! -----------------------------------------------------------------

  subroutine load_coef(DataDir, emis_type, k_k, k_b, b_k, b_b, k_k2, k_b2)

    implicit none

    double precision, dimension(nMltsFta, nParams) :: k_k, k_b, k_k2, k_b2
    double precision, dimension(nMltsFta, nParams) :: b_k, b_b, tmp2
    character(len=10) :: param, forder
    character(len=10) :: emis_type
    character(len=iCharLenFta_) :: NameOfIndexFile, DataDir

    forder = 'r1'
    param = 'k_k'
    NameOfIndexFile = &
      trim(DataDir)// &
      'fit_coef_21bins_'//trim(emis_type)// &
      '_'//trim(forder)//'_'//trim(param)//'.txt'
    call read_coef_file(NameOfIndexFile, tmp2)
    k_k = tmp2

    forder = 'r1'
    param = 'k_b'
    NameOfIndexFile = trim(DataDir)//'fit_coef_21bins_'//trim(emis_type)// &
                      '_'//trim(forder)//'_'//trim(param)//'.txt'
    call read_coef_file(NameOfIndexFile, tmp2)
    k_b = tmp2

    forder = 'r1'
    param = 'b_k'
    NameOfIndexFile = trim(DataDir)//'fit_coef_21bins_'//trim(emis_type)// &
                      '_'//trim(forder)//'_'//trim(param)//'.txt'
    call read_coef_file(NameOfIndexFile, tmp2)
    b_k = tmp2

    forder = 'r1'
    param = 'b_b'
    NameOfIndexFile = trim(DataDir)//'fit_coef_21bins_'//trim(emis_type)// &
                      '_'//trim(forder)//'_'//trim(param)//'.txt'
    call read_coef_file(NameOfIndexFile, tmp2)
    b_b = tmp2

    forder = 'r2'
    param = 'k_k'
    NameOfIndexFile = trim(DataDir)//'fit_coef_21bins_'//trim(emis_type)// &
                      '_'//trim(forder)//'_'//trim(param)//'_log_4p.txt'
    call read_coef_file(NameOfIndexFile, tmp2)
    k_k2 = tmp2

    forder = 'r2'
    param = 'k_b'
    NameOfIndexFile = trim(DataDir)//'fit_coef_21bins_'//trim(emis_type)// &
                      '_'//trim(forder)//'_'//trim(param)//'_log_4p.txt'
    call read_coef_file(NameOfIndexFile, tmp2)
    k_b2 = tmp2

  end subroutine load_coef

  ! -----------------------------------------------------------------
  ! Feed in the lats & results of one MLTs of the FTA model and
  ! get back the results on a given latitude grid
  ! -----------------------------------------------------------------

  subroutine interp_to_lat_grid(mlat0, efs0, efs)

    implicit none
    real, dimension(nLatsFta):: mlat, efs
    real, dimension(nEnergies):: mlat0, efs0
    real, dimension(:), allocatable :: tmp
    integer, dimension(:), allocatable :: idxt, idxt2

    logical, dimension(nEnergies) :: lp
    logical, dimension(nLatsFta) :: lp1
    integer, dimension(nLatsFta) :: idx
    integer :: i, ii, nn

    mlat = lats_fixed_grid

    do i = 1, nLatsFta

      lp = ((mlat0 > (mlat(i) - dLat/2)) .and. (mlat0 <= (mlat(i) + dLat/2)))

      if (count(lp) == 1) then
        allocate(tmp(count(lp)))
        tmp = pack(efs0, lp)
        efs(i) = tmp(1)
        deallocate(tmp)
      else if (count(lp) > 1) then
        allocate(tmp(count(lp)))
        tmp = pack(efs0, lp)
        efs(i) = sum(tmp)/count(lp)
        deallocate(tmp)
      else
        efs(i) = 0
      endif
    enddo

    idx = (/(i, i=1, nLatsFta, 1)/)
    lp1 = efs > 0
    nn = count(lp1)
    allocate(idxt(nn))

    idxt = pack(idx, lp1)

    allocate(idxt2(size(idxt)))

    do i = idxt(1) + 1, idxt(nn) - 1
      if (efs(i) == 0) then
        idxt2 = pack(idxt, idxt > i)
        ii = idxt2(1)
        efs(i) = (efs(i - 1) - efs(ii))*(i - ii)/(i - 1 - ii) + efs(ii)
      endif
    enddo

    deallocate(idxt)
    deallocate(idxt2)

  end subroutine interp_to_lat_grid

  ! -----------------------------------------------------------------
  ! Read in all of the data files and store the data to be used later
  ! -----------------------------------------------------------------

  subroutine initialize_fta(dataDir)

    character(len=iCharLenFta_) :: dataDir

    double precision, dimension(nMltsFta, nParams) :: &
      k_k, k_b, b_k, b_b, k_k2, k_b2

    integer, dimension(nEnergies) :: idx1, idx2
    integer :: i

    idx1 = (/(i, i=2, nParams - 1, 2)/)
    idx2 = (/(i, i=3, nParams, 2)/)

    emissions(1) = 'lbhl'
    emissions(2) = 'lbhs'

    do i = 1, 2
      call load_coef(dataDir, emissions(i), k_k, k_b, b_k, b_b, k_k2, k_b2)

      kk_lat(:, :, i) = k_k(:, idx1)
      kb_lat(:, :, i) = k_b(:, idx1)
      bk_lat(:, :, i) = b_k(:, idx1)
      bb_lat(:, :, i) = b_b(:, idx1)

      kk_ef(:, :, i) = k_k(:, idx2)
      kb_ef(:, :, i) = k_b(:, idx2)
      bk_ef(:, :, i) = b_k(:, idx2)
      bb_ef(:, :, i) = b_b(:, idx2)

      kk_lat2(:, :, i) = k_k2(:, idx1)
      kb_lat2(:, :, i) = k_b2(:, idx1)

      kk_ef2(:, :, i) = k_k2(:, idx2)
      kb_ef2(:, :, i) = k_b2(:, idx2)
    enddo

    isInitialized = .true.
    lats_fixed_grid = (/(i, i=0, nLatsFta - 1, 1)/)*dLat + minLat + dLat/2.0
    mlts_fixed_grid = (/(i, i=0, nMltsFta - 1, 1)/)*dMlt + dMlt/2.0

  end subroutine initialize_fta

  ! -----------------------------------------------------------------
  ! Create MLT x Energy map for a given emission and AU/AL
  ! -----------------------------------------------------------------

  subroutine calc_emission_pattern(AUs, ALs_n, emis_type, mlat_p, ef_p)

    implicit none
    real, intent(in) :: AUs, ALs_n
    real :: ALs
    real, dimension(nMltsFta, nEnergies):: cf_b_lat, cf_k_lat, cf_b_ef, cf_k_ef, &
                                           mlat_p, ef_p, mlat_b0, ef_b0, &
                                           cf_k_lat2, cf_k_ef2

    integer :: i
    character(len=10) :: emis_type

    integer :: iEmission = 0

    do i = 1, 2
      if (trim(emis_type) == trim(emissions(i))) iEmission = i
    enddo

    if (iEmission == 0) then
      write(*, *) "Cant find emission : ", emis_type, " in emissions"
      write(*, *) "must stop"
      stop
    endif

    ALs = -ALs_n

    if (ALs < AL_Split) then

      cf_b_lat = bb_lat(:, :, iEmission) + bk_lat(:, :, iEmission)*AUs
      cf_k_lat = kb_lat(:, :, iEmission) + kk_lat(:, :, iEmission)*log(AUs)

      cf_b_ef = bb_ef(:, :, iEmission) + bk_ef(:, :, iEmission)*AUs
      cf_k_ef = kb_ef(:, :, iEmission) + kk_ef(:, :, iEmission)*log(AUs)

      mlat_p = cf_b_lat + cf_k_lat*ALs
      ef_p = cf_b_ef + cf_k_ef*ALs

    else

      cf_b_lat = bb_lat(:, :, iEmission) + bk_lat(:, :, iEmission)*AUs
      cf_k_lat = kb_lat(:, :, iEmission) + kk_lat(:, :, iEmission)*log(AUs)

      cf_b_ef = bb_ef(:, :, iEmission) + bk_ef(:, :, iEmission)*AUs
      cf_k_ef = kb_ef(:, :, iEmission) + kk_ef(:, :, iEmission)*log(AUs)

      mlat_b0 = cf_b_lat + cf_k_lat*AL_split
      ef_b0 = cf_b_ef + cf_k_ef*AL_split

      !cf_k_lat2 = kb_lat2(:, :, iEmission) + kk_lat2(:, :, iEmission) * AUs
      !cf_k_ef2  = kb_ef2(:, :, iEmission) + kk_ef2(:, :, iEmission) * AUs

      cf_k_lat2 = kb_lat2(:, :, iEmission) + kk_lat2(:, :, iEmission)*log(AUs)
      cf_k_ef2 = kb_ef2(:, :, iEmission) + kk_ef2(:, :, iEmission)*log(AUs)

      where (cf_k_ef2 < 0)
        cf_k_ef2 = 0
      end where

      mlat_p = mlat_b0 + cf_k_lat2*(ALs - AL_split)
      ef_p = ef_b0 + cf_k_ef2*(ALs - AL_split)

    endif

  end subroutine calc_emission_pattern

  ! -----------------------------------------------------------------
  ! Calculate the average energy from the LBHL and LBHS ratio
  ! -----------------------------------------------------------------

  subroutine calc_avee!(lbhl, lbhs, avee)

    ! real, dimension(nMltsFta, nLatsFta), intent(in) :: lbhl, lbhs
    ! real, dimension(nMltsFta, nLatsFta), intent(out) :: avee
    real :: a, lb, c, ratio
    integer :: iMlt, iLat

    ! # Germay et al.(1994) ratio -> energy flux
    ! avee[loc] = 10**(
    !        np.log((ratio[loc]-c)/a)/np.log(b))

    a = 0.09193196
    lb = log(19.73989114)
    c = 0.5446197

    avee = 2.0

    do iMlt = 1, nMltsFta
      do iLat = 1, nLatsFta
        if ((lbhl(iMlt, iLat) > 1) .and. &
            (lbhs(iMlt, iLat) > 1)) then
          ratio = lbhl(iMlt, iLat)/lbhs(iMlt, iLat)
          if ((ratio - c) > 0) &
            avee(iMlt, iLat) = 10.0**(log((ratio - c)/a)/lb)

        endif
      enddo
    enddo
  end subroutine calc_avee

  ! -----------------------------------------------------------------
  ! Take MLT vs energy grid and put onto MLT vs Lat grid
  ! -----------------------------------------------------------------

  subroutine calc_full_patterns( &
    mlats0_l, efs0_l, &
    mlats0_s, efs0_s, &
    lbhl, lbhs, eflux, avee, polarcap)

    real, dimension(nMltsFta, nEnergies), intent(in) :: mlats0_l, efs0_l
    real, dimension(nMltsFta, nEnergies), intent(in) :: mlats0_s, efs0_s
    real, dimension(nMltsFta, nLatsFta), intent(out) :: lbhl, lbhs, avee, eflux
    real, dimension(nMltsFta, nLatsFta), intent(out) :: polarcap
    real, dimension(nLatsFta) :: polarcap1d
    real, dimension(nLatsFta):: emission_lat
    real, dimension(nEnergies) :: emission_en, lats_en
    integer :: i

    polarcap = 0.0
    do i = 1, nMltsFta
      ! bin and interp lbhl in each MLT sector
      emission_en = efs0_l(i, :)
      lats_en = mlats0_l(i, :)
      call interp_to_lat_grid(lats_en, emission_en, emission_lat)
      lbhl(i, :) = emission_lat

      ! bin and interp lbhs in each MLT sector
      emission_en = efs0_s(i, :)
      lats_en = mlats0_s(i, :)
      call interp_to_lat_grid(lats_en, emission_en, emission_lat)
      lbhs(i, :) = emission_lat

      polarcap1d = 0.0
      where (lats_fixed_grid > lats_en(nEnergies))
        polarcap1d = 1.0
      end where
      polarcap(i, :) = polarcap1d
    enddo

    eflux = lbhl/110.0
    call calc_avee!(lbhl, lbhs, avee)

  end subroutine calc_full_patterns

  ! -----------------------------------------------------------------
  ! Adjust the offset between lbhl and lbhs
  ! -----------------------------------------------------------------

  subroutine adjust_offset(mlats_l, mlats_s, gap1, gap3)

    real, dimension(nMltsFta, nLatsFta_mod) :: mlats_s, mlats_l
    real, intent(in) :: gap1, gap3
    real, dimension(nMltsFta) :: wght, eb, pb, width_s, width_l
    logical, dimension(nMltsFta) :: loc
    integer :: k11, i
    real :: gap2

    wght = 0.0

    ! Calculate middle gap
    gap2 = (gap1 + gap3)*0.5

    loc = (mlts_fixed_grid > gap1) .and. (mlts_fixed_grid <= gap2)
    do i = 1, nMltsFta
      if (loc(i)) then
        wght(i) = (mlts_fixed_grid(i) - gap1)/(gap2 - gap1)*0.5
        !write(*,*) 'mlt,wght: ', mlts_fixed_grid(i),wght(i)
      endif
    enddo

    loc = (mlts_fixed_grid > gap2) .and. (mlts_fixed_grid < gap3)
    do i = 1, nMltsFta
      if (loc(i)) then
        wght(i) = (1 - (mlts_fixed_grid(i) - gap2)/(gap3 - gap2))*0.5
        !write(*,*) 'mlt,wght: ', mlts_fixed_grid(i),wght(i)
      endif
    enddo

    eb = mlats_s(:, 1)*wght + mlats_l(:, 1)*(1.0 - wght)
    pb = mlats_s(:, nEnergies)*wght + mlats_l(:, nEnergies)*(1.0 - wght)
    width_s = mlats_s(:, nEnergies) - mlats_s(:, 1)
    width_l = mlats_l(:, nEnergies) - mlats_l(:, 1)

    loc = (mlts_fixed_grid > gap1) .and. (mlts_fixed_grid < gap3)

    ! Adjust latitudes
    do i = 1, nMltsFta
      do k11 = 1, nEnergies - 2
        if (loc(i)) then
          !write(*,*) 'k11,i',k11,i,eb(i), mlats_s(i, k11), mlats_s(i, k11 + 1)
          mlats_s(i, k11 + 1) = (mlats_s(i, k11 + 1) - mlats_s(i, 1))* &
                                (pb(i) - eb(i))/width_s(i) + eb(i)
          mlats_l(i, k11 + 1) = (mlats_l(i, k11 + 1) - mlats_l(i, 1))* &
                                (pb(i) - eb(i))/width_l(i) + eb(i)
          !write(*,*) 'k11,i',k11,i,mlats_s(i, k11), mlats_s(i, k11 + 1)
        endif
      enddo
    enddo

    ! border adjustments
    do i = 1, nMltsFta
      if (loc(i)) then
        mlats_s(i, 1) = eb(i)
        mlats_s(i, nEnergies) = pb(i)
        mlats_l(i, 1) = eb(i)
        mlats_l(i, nEnergies) = pb(i)
      endif
    enddo

  end subroutine adjust_offset

  subroutine adjust_width(mlat, loc)

    real, dimension(nMltsFta, nEnergies):: mlat
    logical, dimension(nMltsFta) :: loc
    integer :: i, j

    do i = 1, nMltsFta
      do j = 1, nEnergies - 1

        if (loc(i) .and. ((mlat(i, j + 1) - mlat(i, j)) < (0.25/20.0))) then
          mlat(i, j + 1) = mlat(i, j) + 0.25/20.0

        endif
      enddo
    enddo

  end subroutine adjust_width

  ! -----------------------------------------------------------------
  ! Construct limiter of AL based on AU
  ! -----------------------------------------------------------------

  subroutine limiter_al_lp(au, al)

    real, intent(in) :: au
    real, intent(out) :: al

    real :: a, b, c
    real :: coef1_m, coef2_m, x1_m, x2_m, y1_m, y2_m, &
            coef1_p, coef2_p, x1_p, x2_p, y1_p, y2_p
    real :: au_, al_, al1, al2

    if (au < 150.0) then
      a = -7.89e-06
      b = -0.0952
      c = -66.0

      al = (-sqrt(b**2.0 - 4.0*(c - au)*a) - b)/(2.0*a) + 10.0

    else if (au >= 150.0 .and. au <= 175.0) then
      al1 = -3100.0
      al2 = -3025.0
      al = (al2 - al1)/(175.0 - 150.0)*(au - 150.0) &
           + al1 + 10.0

    else if (au > 175.0 .and. au <= 300.0) then
      coef1_m = 0.18503781
      coef2_m = 2.37717756

      x1_m = -3010.0
      x2_m = 1275.0
      y1_m = 175.0
      y2_m = 125.0

      au_ = (au - y1_m)/y2_m
      al_ = asinh(au_/coef1_m)/coef2_m
      al = al_*x2_m + x1_m

    else if (au > 300.0 .and. au <= 575.0) then
      coef1_p = 0.27380323
      coef2_p = 2.0135317

      x1_p = -1580.0
      x2_p = 300.0
      y1_p = 325.0
      y2_p = 250.0

      au_ = (au - y1_p)/y2_p
      al_ = asinh(au_/coef1_p)/coef2_p
      al = al_*x2_p + x1_p

    else if (au > 575.0 .and. au <= 775.0) then
      a = 0.5573
      b = 1300.0 - 10.0

      al = (au - b)/a

    else if (au > 775.0 .and. au <= 1050.0) then
      a = 0.689
      b = 1440.0 - 10.0

      al = (au - b)/a
    else
      call set_error("The FTA limiter does not work for AU > 1050!")
    endif

  end subroutine limiter_al_lp

  subroutine limiter_al_up(au, al)

    real, intent(in) :: au
    real, intent(out) :: al

    real :: a, b

    a = -0.682
    b = 734.0 - 10.0
    al = (au - b)/a

  end subroutine limiter_al_up

end module ModFTAModel
