
! ------------------------------------------------------------
! Check indices to see if the model has what it needs:

subroutine run_check_indices(this)
  class(ieModel) :: this

  if (this%iEfield_ == iWeimer05_) then
    if (this%needImfBz == rBadValue) &
      call set_error("IMF Bz not set!")
    if (this%needImfBy == rBadValue) &
      call set_error("IMF By not set!")
    if (this%needSwV == rBadValue) call &
      set_error("Solar Wind Speed not set!")
    if (this%needSwN == rBadValue) call &
      set_error("Solar Wind Density not set!")
  endif

  if (this%iEfield_ == iHepMay_) then
    if (this%needImfBz == rBadValue) &
      call set_error("IMF Bz not set!")
    if (this%needImfBy == rBadValue) &
      call set_error("IMF By not set!")
    if (this%needKp == rBadValue) &
      call set_error("Kp not set!")
  endif

  if (this%iAurora_ == iFta_) then
    if (this%needAu == rBadValue) &
      call set_error("Aurora - Upper index not set!")
    if (this%needAl == rBadValue) &
      call set_error("Aurora - Lower index not set!")
  endif

  if (this%iAurora_ == iFRE_) then
    if (this%needHpN == rBadValue) &
      call set_error("Aurora - Hemispheric Power (north) index not set!")
    if (this%needHpS == rBadValue) &
      call set_error("Aurora - Hemispheric Power (south) index not set!")
  endif
  if (this%iAurora_ == iPEM_) then
    if (this%needHpN == rBadValue) &
      call set_error("Aurora - Hemispheric Power (north) index not set!")
    if (this%needHpS == rBadValue) &
      call set_error("Aurora - Hemispheric Power (south) index not set!")
  endif

  if (this%iAurora_ == iOvationPrime_) then
    if (this%needImfBy == rBadValue) &
      call set_error("Aurora - IMF By not set!")
    if (this%needImfBz == rBadValue) &
      call set_error("Aurora - IMF Bz not set!")
    if (this%needSwV == rBadValue) &
      call set_error("Aurora - IMF Vx not set!")
  endif

end subroutine run_check_indices

! ------------------------------------------------------------
! set IMF Bz
subroutine set_bz(this, value)
  class(ieModel), intent(inout) :: this
  real, intent(in) :: value
  if (this%iDebugLevel > 2) &
    write(*, *) "=> Setting imf bz : ", value
  this%needImfBz = value
  this%isAuroraUpdated = .false.
  this%isPotentialUpdated = .false.
end subroutine set_bz

! ------------------------------------------------------------
! set IMF By
subroutine set_by(this, value)
  class(ieModel), intent(inout) :: this
  real, intent(in) :: value
  if (this%iDebugLevel > 2) &
    write(*, *) "=> Setting imf by : ", value
  this%needImfBy = value
  this%isAuroraUpdated = .false.
  this%isPotentialUpdated = .false.
end subroutine set_by

! ------------------------------------------------------------
! set solar wind velocity
subroutine set_swv(this, value)
  class(ieModel), intent(inout) :: this
  real, intent(in) :: value
  if (this%iDebugLevel > 2) &
    write(*, *) "=> Setting Solar Wind Velocity: ", value
  ! Make sure that this velocity is a positive value:
  this%needSwV = abs(value)
  this%isAuroraUpdated = .false.
  this%isPotentialUpdated = .false.
end subroutine set_swv

! ------------------------------------------------------------
! set solar wind density
subroutine set_swn(this, value)
  class(ieModel), intent(inout) :: this
  real, intent(in) :: value
  if (this%iDebugLevel > 2) &
    write(*, *) "=> Setting Solar Wind Density: ", value
  this%needSwN = value
  this%isAuroraUpdated = .false.
  this%isPotentialUpdated = .false.
end subroutine set_swn

! ------------------------------------------------------------
! set the hemispheric power (in gigawatts)
subroutine set_hp(this, value)
  class(ieModel), intent(inout) :: this
  real, intent(in) :: value
  if (this%iDebugLevel > 2) &
    write(*, *) "=> Setting Hemispheric Power (in both hems) : ", value
  this%needHp = value
  ! If the user calls this routine, then it sets the north and south HPs
  this%needHpN = value
  this%needHpS = value
  this%isAuroraUpdated = .false.
  this%isPotentialUpdated = .false.
end subroutine set_hp

! ------------------------------------------------------------
! set north hemispheric power (in gigawatts)
subroutine set_hpn(this, value)
  class(ieModel), intent(inout) :: this
  real, intent(in) :: value
  if (this%iDebugLevel > 2) &
    write(*, *) "=> Setting Hemispheric Power (in north) : ", value
  this%needHpN = value
  this%isAuroraUpdated = .false.
  this%isPotentialUpdated = .false.
end subroutine set_hpn

! ------------------------------------------------------------
! set south hemispheric power (in gigawatts)
subroutine set_hps(this, value)
  class(ieModel), intent(inout) :: this
  real, intent(in) :: value
  if (this%iDebugLevel > 2) &
    write(*, *) "=> Setting Hemispheric Power (in south) : ", value
  this%needHpS = value
  this%isAuroraUpdated = .false.
  this%isPotentialUpdated = .false.
end subroutine set_hps

! ------------------------------------------------------------
! set Hemispheric Power from AE
subroutine set_hp_from_ae(this, value)
  class(ieModel), intent(inout) :: this
  real, intent(in) :: value
  if (this%useAeForHp) then
    ! Set the hemispheric power based on the AE index:
    this%needHp = 0.102*value + 8.953
    this%needHpN = this%needHp
    this%needHpS = this%needHp
    if (this%iDebugLevel > 2) then
      write(*, *) "=> Setting HP from AE. AE = ", value
      write(*, *) "                       HP = ", this%needHp
    endif
  endif
  ! also set in the following routines in case this is ever removed:
  this%isAuroraUpdated = .false.
  this%isPotentialUpdated = .false.
end subroutine set_hp_from_ae

! ------------------------------------------------------------
! set AU (and derive AE)
subroutine set_au(this, value)
  class(ieModel), intent(inout) :: this
  real, intent(in) :: value
  if (this%iDebugLevel > 2) &
    write(*, *) "=> Setting AU (and deriving AE): ", value
  this%needAu = value
  ! derive AE, done in both functions so that the last one called works
  this%needAe = this%needAu - this%needAl
  call this%aehp(this%needAe)
  this%isAuroraUpdated = .false.
  this%isPotentialUpdated = .false.
end subroutine set_au

! ------------------------------------------------------------
! set AL (and derive AE)
subroutine set_al(this, value)
  class(ieModel), intent(inout) :: this
  real, intent(in) :: value
  if (this%iDebugLevel > 2) &
    write(*, *) "=> Setting AL (and deriving AE): ", value
  this%needAl = value
  ! derive AE, done in both functions so that the last one called works
  this%needAe = this%needAu - this%needAl
  call this%aehp(this%needAe)
  this%isAuroraUpdated = .false.
  this%isPotentialUpdated = .false.
end subroutine set_al

! ------------------------------------------------------------
! set AE
subroutine set_ae(this, value)
  class(ieModel), intent(inout) :: this
  real, intent(in) :: value
  if (this%iDebugLevel > 2) &
    write(*, *) "=> Setting AE : ", value
  this%needAe = value
  call this%aehp(this%needAe)
  this%isAuroraUpdated = .false.
  this%isPotentialUpdated = .false.
end subroutine set_ae

! ------------------------------------------------------------
! set Use AE to determine HP to true
subroutine set_useAeForHp(this)
  class(ieModel), intent(inout) :: this
  if (this%iDebugLevel > 2) &
    write(*, *) "=> Using AE to specify the HP!"
  this%useAeForHp = .true.
end subroutine set_useAeForHp

! ------------------------------------------------------------
! set Use AE to determine HP to false
subroutine unset_useAeForHp(this)
  class(ieModel), intent(inout) :: this
  if (this%iDebugLevel > 2) &
    write(*, *) "=> NOT Using AE to specify the HP!"
  this%useAeForHp = .false.
end subroutine unset_useAeForHp

! ------------------------------------------------------------
! set Kp
subroutine set_kp(this, value)
  class(ieModel), intent(inout) :: this
  real, intent(in) :: value
  if (this%iDebugLevel > 2) &
    write(*, *) "=> Setting Kp : ", value
  this%needKp = value
  this%isAuroraUpdated = .false.
  this%isPotentialUpdated = .false.
end subroutine set_kp

