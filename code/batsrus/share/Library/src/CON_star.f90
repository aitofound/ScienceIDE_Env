!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
!
!
module CON_star

  ! Physical information about the planet. The planet is described
  ! with its name. Default values can be set with {\bf planet\_init}.
  ! Simplifying assumptions, such as no rotation, aligned magnetic
  ! and rotational axes etc. can be made.
  !
  ! This is a public class. The variables should be modified by CON only.
  ! Components can only access the data through the inquiry methods
  ! via the {\bf CON\_physics} class.

  use ModKind
  use ModConst, ONLY: cTwoPi, rSun, mSun, RotationPeriodSun, &
       cSecondPerDay
  use ModTimeConvert, ONLY: time_int_to_real
  use ModUtilities, ONLY: CON_stop
  implicit none

  save

  ! Initialize star to be the sun
  character(len=20):: NameStar = 'SUN'
  real:: RadiusStar    = rSun
  real:: MassStar      = mSun
  real:: RotPeriodStar = RotationPeriodSun
  real:: OmegaStar     = cTwoPi/RotationPeriodSun
  !$acc declare create(OmegaStar)

  ! For the Sun:
  ! https://nssdc.gsfc.nasa.gov/space/helios/plan_des.html reads:
  ! " The zero longitude of the HGR system is defined as the longitude that
  ! passed through the ascending node of the solar equator on the ecliptic
  ! plane on 1  January, 1854 at 12 UT (Julian day = 2398220.0). In accordance
  ! with the definition of HGI, at this time HGR and HGI were aligned and
  ! after this the HGR rotates with the CarringtonFrequency about z-axis.
  !
  ! Difference between 01/01/1965 00:00:00 and 01/01/1854 12:00:00 in seconds

  real(Real8_):: tAlignmentHgrHgi  = -3.5027856D+9
  !$acc declare create(tAlignmentHgrHgi)

  ! For different star it may make sense to reset this reference time
  ! by reading the alignment time from PARAM.in file, for example, the time
  ! of stellar magnetogram, so that both planet motion (determined in HGI)
  ! and the magnetogram determined in HGR to be oriented consistently

contains
  !============================================================================
  subroutine read_star_var(NameCommand)

    use ModUtilities, ONLY: upper_case
    use ModReadParam, ONLY: read_var, lStringLine
    use ModIoUnit, ONLY: UnitTmp_

    character (len=*), intent(in) :: NameCommand
    integer :: iYear, iMonth, iDay, iHour, iMinute

    character(len=*), parameter:: NameSub = 'read_star_var'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case("#STAR")
       call read_var('NameStar', NameStar, IsUpperCase=.true.)
       call read_var('RadiusStar',         RadiusStar)  ! In rSun
       RadiusStar = RadiusStar*rSun
       call read_var('MassStar',           MassStar)
       MassStar = MassStar*mSun
       call read_var('RotationPeriodStar', RotPeriodStar)
       if(RotPeriodStar == 0.0)then
          OmegaStar = 0.0
       else
          RotPeriodStar = RotPeriodStar*cSecondPerDay
          OmegaStar = cTwoPi/RotPeriodStar
       end if
       !$acc update device(OmegaStar)
    case("#HGRALIGNMENTTIME")
       call read_var('iYear',   iYear)
       call read_var('iMonth',  iMonth)
       call read_var('iDay',    iDay)
       call read_var('iHour',   iHour)
       call read_var('iMinute', iMinute)
       call time_int_to_real([iYear, iMonth, iDay, iHour, iMinute, 0, 0], &
            tAlignmentHgrHgi)
    case default
       call CON_stop('Unknwn NameCommand='//NameCommand//' in '//NameSub)
    end select

  end subroutine read_star_var
  !============================================================================
end module CON_star
!==============================================================================
