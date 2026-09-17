
integer function efield_interpret_name(efieldString)

  use ModErrors
  use ModIE
  implicit none

  character(len=*), intent(in) :: efieldString
  character(len=len(efieldString)) :: efieldLower

  efield_interpret_name = -1

  efieldLower = efieldString
  call lower_case(efieldLower)

  if (trim(efieldLower) == "zero") &
    efield_interpret_name = iZero_

  if (trim(efieldLower) == "weimer") &
    efield_interpret_name = iWeimer05_

  if (trim(efieldLower) == "weimer05") &
    efield_interpret_name = iWeimer05_

  if (trim(efieldLower) == "millstone") &
    efield_interpret_name = iMillstone_

  if (trim(efieldLower) == "hepmay") &
    efield_interpret_name = iHepMay_
  if (trim(efieldLower) == "heppnermaynard") &
    efield_interpret_name = iHepMay_

  if (trim(efieldLower) == "amie") &
    efield_interpret_name = iAmiePot_
  if (trim(efieldLower) == "amiefile") &
    efield_interpret_name = iAmiePot_
  if (trim(efieldLower) == "swmffile") &
    efield_interpret_name = iAmiePot_

  if (efield_interpret_name == -1) then
    call set_error("efield model name not understood! Received:")
    call set_error(efieldLower)
  endif

  return

end function efield_interpret_name

integer function aurora_interpret_name(auroraString)

  use ModErrors
  use ModIE
  implicit none

  character(len=*), intent(in) :: auroraString
  character(len=len(auroraString)) :: auroraLower

  aurora_interpret_name = -1

  auroraLower = auroraString
  call lower_case(auroraLower)

  if (trim(auroraLower) == "zero") &
    aurora_interpret_name = iZero_

  if (trim(auroraLower) == "fta") &
    aurora_interpret_name = iFTA_

  if (trim(auroraLower) == "fre") &
    aurora_interpret_name = iFRE_
  if (trim(auroraLower) == "ihp") &
    aurora_interpret_name = iFRE_
  if (trim(auroraLower) == "hpi") &
    aurora_interpret_name = iFRE_

  if (trim(auroraLower) == "pem") &
    aurora_interpret_name = iPEM_

  if ((trim(auroraLower) == "ovation") .or. &
      (trim(auroraLower) == "ovationprime")) &
    aurora_interpret_name = iOvationPrime_

  if (trim(auroraLower) == "amie") &
    aurora_interpret_name = iAmieAur_
  if (trim(auroraLower) == "amiefile") &
    aurora_interpret_name = iAmieAur_
  if (trim(auroraLower) == "swmffile") &
    aurora_interpret_name = iAmieAur_

  if (aurora_interpret_name == -1) then
    call set_error("aurora model name not understood! Received:")
    call set_error(auroraLower)
  endif

  return

end function aurora_interpret_name
