
! Some of these are from the SWMF:

!ROUTINE: upper_case - convert string to all upper case
!INTERFACE:
subroutine upper_case(String)

  !INPUT/OUTPUT ARGUMENTS:
  character(len=*), intent(inout) :: String

  !DESCRIPTION:
  ! Change characters to upper case in String
  !EOP

  integer, parameter :: iA = ichar('a'), iZ = ichar('z'), Di = ichar('A') - iA
  integer :: i, iC
  !--------------------------------------------------------------------------
  do i = 1, len_trim(String)
    iC = ichar(String(i:i))
    if (iC >= iA .and. iC <= iZ) String(i:i) = char(iC + Di)
  enddo

end subroutine upper_case

!ROUTINE: lower_case - convert string to all lower case
!INTERFACE:
subroutine lower_case(String)

  !INPUT/OUTPUT ARGUMENTS:
  character(len=*), intent(inout) :: String

  !DESCRIPTION:
  ! Change characters to lower case in String
  !EOP

  integer, parameter :: iA = ichar('A'), iZ = ichar('Z'), Di = ichar('a') - iA
  integer :: i, iC
  !--------------------------------------------------------------------------
  do i = 1, len_trim(String)
    iC = ichar(String(i:i))
    if (iC >= iA .and. iC <= iZ) String(i:i) = char(iC + Di)
  enddo

end subroutine lower_case

subroutine merge_str(str1, str2)

  use ModCharSize

  character(len=iCharLenIE_) :: str1, str2, temp
  integer :: i, j, k

  i = 1
  do while (iachar(str1(i:i)) /= 32 .and. &
            iachar(str1(i:i)) /= 9 .and. &
            i < 100)
    i = i + 1
  enddo

  j = 1
  do while (iachar(str2(j:j)) /= 32 .and. &
            iachar(str2(j:j)) /= 9 .and. &
            j < 100)
    j = j + 1
  enddo

  temp = str1
  do k = i, 100
    temp(k:k) = ' '
  enddo

  if (i + j - 1 > 100) j = 100 - i + 1

  temp(i:i + j - 1) = str2(1:j)

  str2 = temp

end subroutine merge_str

subroutine strlen(str1, len)

  use ModCharSize

  character(len=iCharLenIE_) :: str1
  integer :: len

  len = 1
  do while (iachar(str1(len:len)) /= 32 .and. &
            iachar(str1(len:len)) /= 9 .and. &
            len < 100)
    len = len + 1
  enddo

end subroutine strlen
