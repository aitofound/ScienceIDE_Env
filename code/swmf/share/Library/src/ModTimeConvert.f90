!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
!
!
module ModTimeConvert

  ! The methods in this module can convert between various representations
  ! of time. The date can be defined as year, month, day, hour, minute, second
  ! and a fractional second, which is double precision real.
  ! Alternatively an integer millisecond can be used.
  ! These representations are easy to read, but difficult to manipulate.
  !
  ! An easy to manipulate representation is the time measured
  ! from some base time, namely the 00:00 GMT on January 1 of iYearBase.
  ! This time is measured in seconds with an 8 byte real number.
  !
  ! Finally, for output and for constructing file names, a string
  ! representation is convenient. The string contains year, month,
  ! day, hour, minute, second in 14 characters. This order of the date and
  ! time makes alphabetic order to be the same as temporal order.
  ! The string is padded with 0-s so it can be used to form file names.
  !
  ! The conversion routines can convert between the different fields
  ! of a variable of TimeType, or they can convert between integer arrays
  ! (year...millisecond) and an 8 byte real variable (seconds since base time).

  use ModUtilities, ONLY: CON_stop
  use ModKind
  use ModConst

  implicit none

  save

  private ! except

  !PUBLIC TYPES:
  public :: TimeType
  type TimeType
     integer           :: iYear
     integer           :: iMonth
     integer           :: iDay
     integer           :: iHour
     integer           :: iMinute
     integer           :: iSecond
     real(Real8_)      :: FracSecond
     real(Real8_)      :: Time         ! time in seconds since base time
     character(len=14) :: String       ! string with year...second.
  end type TimeType

  public :: n_day_of_year    ! day of year

  public :: time_int_to_real ! Convert integer time info to real
  interface time_int_to_real
     module procedure time_int_to_real1, time_int_to_real2
  end interface time_int_to_real

  public :: time_real_to_int ! Convert real time info into integer
  interface time_real_to_int
     module procedure time_real_to_int1, time_real_to_int2
  end interface time_real_to_int

  public :: time_real_to_julian, time_int_to_julian ! Julian day

  public :: test_time        ! unit tester

  !PUBLIC DATA MEMBERS:

  ! iYearBase=1585 is inherited from ModConst. It used to be 1965.
  ! UA/GITM may have to update its routines to correctly calculate the date.

  ! The earliest year which is already correctly handled
  integer, parameter :: iYearMin  = iYearBase

  ! revision history:
  ! 01Aug03 Aaron Ridley and G. Toth - initial implementation
  ! 22Aug03 G. Toth - added TypeFreq and is_time_to function
  ! 25Aug03 G. Toth - added adjust_freq subroutine
  ! 23Mar04 G. Toth - splitting CON_time into a smaller CON_time,
  !                             ModTimeConvert, ModTimeFreq

  ! February will be adjusted.....
  integer, dimension(1:12), private :: nDayInMonth_I = [ &
       31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]

  character(len=*), parameter, private :: NameMod='ModTimeConvert'

contains
  !============================================================================
  logical function is_valid_int_time(Time)

    ! Check if the integer description of the time is valid.
    ! The year may be corrected, e.g. 66 --> 1966, 03 --> 2003.
    ! Return false if time is not valid.

    type(TimeType), intent(inout) :: Time
    !--------------------------------------------------------------------------
    call fix_year(Time % iYear)
    is_valid_int_time = .false.
    ! if(Time % iYear < iYearMin) RETURN
    if(Time % iMonth > 12 .or. Time % iMonth < 1) RETURN
    if(Time % iMonth == 2) call fix_february(Time % iYear)
    ! if(Time%iDay < 1 .or. Time%iDay > nDayInMonth_I(Time % iMonth)) RETURN
    if(Time % iHour < 0   .or. Time % iHour   > 23) RETURN
    if(Time % iMinute < 0 .or. Time % iMinute > 59) RETURN
    if(Time % iSecond < 0 .or. Time % iSecond > 59) RETURN
    if(Time % FracSecond < 0.0_Real8_ .or. Time % FracSecond > 1.0_Real8_) &
         RETURN
    is_valid_int_time = .true.

  end function is_valid_int_time
  !============================================================================
  subroutine time_int_to_string(Time)

    ! Convert integer time info into the string field of the Time variable

    type(TimeType), intent(inout) :: Time

    character(len=*), parameter:: NameSub = 'time_int_to_string'
    !--------------------------------------------------------------------------
    if(.not.is_valid_int_time(Time))then
       write(*,*)NameSub,': invalid Time = ',Time
       call CON_stop(NameSub//' ERROR invalid time')
    end if

    write(Time % String,'(i4.4,5(i2.2))') &
         Time % iYear, Time % iMonth, Time % iDay, &
         Time % iHour, Time % iMinute, Time % iSecond

  end subroutine time_int_to_string
  !============================================================================
  subroutine time_int_to_real2(iTime_I, Time)

    ! Convert an integer array containing
    ! year, month, day, hour, minute, second, millisecond
    ! into the number of seconds since 00:00 January 1 of the base year.

    integer, intent(in) :: iTime_I(1:7)

    real(real8_), intent(out) :: Time
    type(TimeType) :: TimeTmp

    character(len=*), parameter:: NameSub = 'time_int_to_real2'
    !--------------------------------------------------------------------------
    TimeTmp % iYear   = iTime_I(1)
    TimeTmp % iMonth  = iTime_I(2)
    TimeTmp % iDay    = iTime_I(3)
    TimeTmp % iHour   = iTime_I(4)
    TimeTmp % iMinute = iTime_I(5)
    TimeTmp % iSecond = iTime_I(6)
    TimeTmp % FracSecond = iTime_I(7)/1000.0

    call time_int_to_real(TimeTmp)

    Time = TimeTmp % Time

  end subroutine time_int_to_real2
  !============================================================================
  subroutine time_int_to_real1(Time)

    type(TimeType), intent(inout) :: Time

    ! Convert the integer fields containing year ... second and the
    ! fractional second into the double precision seconds counted
    ! from the beginning of the base year. Also fill in the string field
    ! of Time.

    character(len=*), parameter:: NameSub = 'time_int_to_real1'
    !--------------------------------------------------------------------------
    if(.not.is_valid_int_time(Time))then
       write(*,*)NameSub,': invalid Time = ',Time
       call CON_stop(NameSub//' ERROR invalid time')
    end if

    Time % Time = &
         ((Time%iYear-iYearBase) * 365 + n_leap_day(Time%iYear) + &
         n_day_of_year(Time%iYear, Time%iMonth, Time%iDay)-1)*cSecondPerDay + &
         Time%iHour * cSecondPerHour + &
         Time%iMinute * cSecondPerMinute + &
         Time%iSecond + &
         Time%FracSecond

    call time_int_to_string(Time)

  end subroutine time_int_to_real1
  !============================================================================
  subroutine time_real_to_int1(Time)

    type(TimeType), intent(inout) :: Time
    ! Convert the number of seconds counted from the beginning of the base year
    ! to the integer fields and the fractional second field. Also fill in
    ! the string field of Time.

    integer :: iYear, iMonth, iDay, nLeapYear
    real(Real8_) :: TimeRemaining

    character(len=*), parameter:: NameSub = 'time_real_to_int1'
    !--------------------------------------------------------------------------
    iYear = floor(Time%Time/cSecondPerYear) + iYearBase
    ! write(*,*) 'iYear=',iYear
    do
       nLeapYear = n_leap_day(iYear)
       ! write(*,*) 'nLeapYear=',nLeapYear
       iDay = floor((Time%Time - (iYear-iYearBase)*cSecondPerYear)/&
            cSecondPerDay) - nLeapYear
       ! write(*,*)'iDay=', iDay
       if(iDay >= 0) EXIT
       iYear = iYear - 1
    end do
    ! write(*,*) 'iYear, nLeapYear, is_leap_year, iDay=',&
    !     iYear,nLeapYear, is_leap_year(iYear),iDay

    TimeRemaining = Time % Time - (iYear-iYearBase) * cSecondPerYear
    TimeRemaining = TimeRemaining - (iDay+nLeapYear)*cSecondPerDay

    Time % iHour = floor(TimeRemaining/cSecondPerHour)
    TimeRemaining = TimeRemaining - Time % iHour * cSecondPerHour

    Time % iMinute = floor(TimeRemaining/cSecondPerMinute)
    TimeRemaining = TimeRemaining - Time % iMinute*cSecondPerMinute

    Time % iSecond = floor(TimeRemaining)

    Time % FracSecond = TimeRemaining - Time % iSecond

    iMonth = 1;
    call fix_february(iYear)

    do while (iDay >= nDayInMonth_I(iMonth))
       iDay = iDay - nDayInMonth_I(iMonth)
       iMonth = iMonth + 1
    end do

    Time % iYear = iYear
    Time % iMonth = iMonth
    Time % iDay = iDay + 1

    call time_int_to_string(Time)

  end subroutine time_real_to_int1
  !============================================================================
  subroutine time_real_to_int2(Time, iTime_I)

    ! Convert the double precision number of seconds since the beginning
    ! of the base year into an integer array of year, month, day, hour,
    ! minute, second, millisecond.

    real(real8_), intent(in) :: Time
    integer, intent(out) :: iTime_I(1:7)

    type(TimeType) :: TimeTmp

    character(len=*), parameter:: NameSub = 'time_real_to_int2'
    !--------------------------------------------------------------------------
    TimeTmp % Time = Time

    call time_real_to_int(TimeTmp)

    iTime_I(1) = TimeTmp % iYear
    iTime_I(2) = TimeTmp % iMonth
    iTime_I(3) = TimeTmp % iDay
    iTime_I(4) = TimeTmp % iHour
    iTime_I(5) = TimeTmp % iMinute
    iTime_I(6) = TimeTmp % iSecond
    iTime_I(7) = TimeTmp % FracSecond * 1000.0

  end subroutine time_real_to_int2
  !============================================================================
  subroutine fix_february(iYear)

    ! Set number of days in February for year iYear

    integer, intent(in) :: iYear
    !--------------------------------------------------------------------------
    if(is_leap_year(iYear))then
       nDayInMonth_I(2) = 29
    else
       nDayInMonth_I(2) = 28
    end if

  end subroutine fix_february
  !============================================================================
  subroutine fix_year(iYear)

    integer, intent(inout) :: iYear

    ! Attempt to fix 2 digit years. Assumption :
    ! begin{verbatim}
    !  0-49 --> 2000-2049
    ! 50-99 --> 1950-1999
    ! end{verbatim}
    ! Using a 4 digit year is safer. You should convert before using.

    character(len=*), parameter:: NameSub = 'fix_year'
    !--------------------------------------------------------------------------
    select case(iYear)
    case(0:49)
       iYear = iYear + 2000
    case(50:99)
       iYear = iYear + 1900
    end select

  end subroutine fix_year
  !============================================================================
  integer function n_leap_day(iYear)

    ! Return the number of leap days from base year to the year preceeding
    ! iYear. The leap day in iYear itself is not counted!

    integer, intent(in) :: iYear

    ! local variables

    integer, parameter :: iYearBase100 = 100*(iYearBase/100) + 1
    integer, parameter :: iYearBase400 = 400*(iYearBase/400) + 1

    character(len=*), parameter:: NameSub = 'n_leap_day'
    !--------------------------------------------------------------------------
    n_leap_day = &
         (iYear - iYearBase)/4 &
         - (iYear - iYearBase100)/100 &
         + (iYear - iYearBase400)/400

  end function n_leap_day
  !============================================================================
  logical function is_leap_year(iYear)

    integer, intent(in) :: iYear
    !--------------------------------------------------------------------------
    is_leap_year = modulo(iYear, 4) == 0 .and. &
         (modulo(iYear, 100) /= 0 .or. modulo(iYear, 400) == 0)

  end function is_leap_year
  !============================================================================
  integer function n_day_of_year(iYear, iMonth, iDay)

    ! Calculate the number of days since the beginning of iYear.
    ! January 1 returns 1. Leap years are taken into account.

    integer, intent(in) :: iYear, iMonth, iDay

    character(len=*), parameter:: NameSub = 'n_day_of_year'
    !--------------------------------------------------------------------------
    call fix_february(iYear)
    n_day_of_year = sum(nDayInMonth_I(1:iMonth-1)) + iDay

  end function n_day_of_year
  !============================================================================
  subroutine time_real_to_julian(Time, JulianDay)

    ! convert real time info into real Julian day.
    ! Note that accuracy may be single precision.

    real(Real8_), intent(in)  :: Time
    real, intent(out) :: JulianDay

    integer:: iTime_I(7)
    !--------------------------------------------------------------------------
    JulianDay = JulianDayBase + Time/cSecondPerDay

  end subroutine time_real_to_julian
  !============================================================================
  subroutine time_int_to_julian(iTime_I, JulianDay)

    ! convert integer time info into Julian day

    integer, intent(in)  :: iTime_I(1:7)
    real,    intent(out) :: JulianDay

    real(Real8_):: Time

    character(len=*), parameter:: NameSub = 'time_int_to_julian'
    !--------------------------------------------------------------------------
    call time_int_to_real(iTime_I, Time)
    call time_real_to_julian(Time, JulianDay)

  end subroutine time_int_to_julian
  !============================================================================
  subroutine test_time

    integer, parameter :: iYearMax  = 2499
    type(TimeType) :: TimeConvert, TimeStart
    real(Real8_):: Time
    integer:: nDay
    real:: JulianDay
    !--------------------------------------------------------------------------
    write(*,*)'Testing time conversion routines'
    TimeStart % FracSecond = 0.0;

    TimeStart % iHour = 0; TimeStart % iMinute = 0; TimeStart % iSecond = 1
    call check_all_days
    TimeStart % iHour =23; TimeStart % iMinute =59; TimeStart % iSecond =59
    call check_all_days
    write(*,'(a,i5,a,i5)')'Successfully tested all days from Jan 1',&
         iYearMin,' to Dec 31',iYearMax
    call time_int_to_julian([2026, 6, 25, 12, 0, 0, 0], JulianDay)
    ! Test on month index > 2:
    if(abs(JulianDay - 2461217.0) > 0.1) &
         write(*,*)'Error for time_int_to_julian: JulianDay=', JulianDay, &
         ' instead of 2461217.0'
    ! Test on month index <= 2:
    call time_int_to_julian([2026, 1, 12, 0, 0, 0, 0], JulianDay)
    if(abs(JulianDay - 2461052.5) > 0.1) &
         write(*,*)'Error for time_int_to_julian: JulianDay=', JulianDay, &
         ' instead of 2461052.5'
    ! Test n_day_of_year
    nDay = n_day_of_year(2020, 6, 25) ! leap year
    if(nDay /= 177) &
         write(*,*)'Error: n_day_of_year(2020,6,25)=', nDay, ' instead of 177'
    nDay = n_day_of_year(2000, 6, 25) ! leap year
    if(nDay /= 177) &
         write(*,*)'Error: n_day_of_year(2000,6,25)=', nDay, ' instead of 177'
    nDay = n_day_of_year(2100, 6, 25) ! not leap year
    if(nDay /= 176) &
         write(*,*)'Error: n_day_of_year(2100,6,25)=', nDay, ' instead of 176'
    nDay = n_day_of_year(2026, 6, 25) ! not leap year
    if(nDay /= 176) &
         write(*,*)'Error: n_day_of_year(2026,6,25)=', nDay, ' instead of 176'

    ! Test converting to seconds and then to Julian day
    call time_int_to_real([2026, 1, nDay, 12, 0, 0, 0], Time)
    call time_real_to_julian(Time, JulianDay)
    if(abs(JulianDay - 2461217.0) > 0.1) &
         write(*,*)'Error for time_real_to_julian: JulianDay=', JulianDay, &
         ' instead of 2461217.0'

  contains
    !==========================================================================
    subroutine check_all_days

      ! The latest year to be tested
      integer :: iYear, iMonth, iDay
      !------------------------------------------------------------------------
      do iYear = iYearMin,iYearMax
         TimeStart % iYear = iYear
         do iMonth = 1, 12
            TimeStart % iMonth = iMonth
            call fix_february(iYear)
            do iDay = 1, nDayInMonth_I(iMonth)
               TimeStart % iDay = iDay

               ! Convert to real time
               call time_int_to_real(TimeStart)
               TimeConvert % Time = TimeStart % Time
               ! Convert back
               call time_real_to_int(TimeConvert)

               if(TimeConvert % String /= TimeStart % String) then
                  write(*,*)'TimeStart  =',TimeStart % String
                  write(*,*)'TimeConvert=', TimeConvert % String
                  stop
               end if
            end do
         end do
      end do
    end subroutine check_all_days
    !==========================================================================
  end subroutine test_time
  !============================================================================
end module ModTimeConvert
!==============================================================================
