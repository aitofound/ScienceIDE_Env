!
! SDF (Self-Describing Format) Fortran Library
! Copyright (c) 2010-2016, SDF Development Team
!
! Distributed under the terms of the BSD 3-clause License.
! See the LICENSE file for details.
!

MODULE sdf_job_info

  IMPLICIT NONE

  INTEGER, PARAMETER, PRIVATE :: i4 = SELECTED_INT_KIND(9) ! 4-byte 2^31 ~ 10^9

  TYPE :: jobid_type
    INTEGER(i4) :: start_seconds
    INTEGER(i4) :: start_milliseconds
  END TYPE jobid_type

CONTAINS

  FUNCTION unix_seconds(values)

    INTEGER, DIMENSION(8), INTENT(IN) :: values
    INTEGER(i4) :: unix_seconds
    INTEGER :: days, year, month, day, h, m, s
    INTEGER, PARAMETER, DIMENSION(12) :: days_since_new_year = (/ &
        0, &
        31, &
        (31+28), &
        (31+28+31), &
        (31+28+31+30), &
        (31+28+31+30+31), &
        (31+28+31+30+31+30), &
        (31+28+31+30+31+30+31), &
        (31+28+31+30+31+30+31+31), &
        (31+28+31+30+31+30+31+31+30), &
        (31+28+31+30+31+30+31+31+30+31), &
        (31+28+31+30+31+30+31+31+30+31+30) /)

    year = values(1)
    month = values(2)
    day = values(3)
    h = values(5)
    m = values(6) - values(4)
    s = values(7)

    days = (year - 1970)*365 &            ! 365 days per year
          + (year - 1969)/4 &             ! and an extra day for each leap year
          - (year - 1601)/100 + 3 &       ! but century years are not leap years
          + (year - 1601)/400 &           ! unless divisible by 400
          + days_since_new_year(month)

    IF ((MOD(year,400) == 0 &
        .OR.  (MOD(year,4) == 0 .AND. MOD(year,100) /= 0)) &
        .AND. month > 2) days = days + 1

    unix_seconds = INT((((days + day - 1)*24 + h)*60 + m)*60 + s,i4)

  END FUNCTION unix_seconds



  FUNCTION get_unix_time()

    INTEGER(i4) :: get_unix_time
    INTEGER, DIMENSION(8) :: val

    CALL DATE_AND_TIME(values = val)

    get_unix_time = unix_seconds(val)

  END FUNCTION get_unix_time



  SUBROUTINE get_job_id(jobid)

    TYPE(jobid_type), INTENT(OUT) :: jobid
    INTEGER, DIMENSION(8) :: val

    CALL DATE_AND_TIME(values = val)

    jobid%start_seconds = unix_seconds(val)
    jobid%start_milliseconds = INT(val(8),i4)

  END SUBROUTINE get_job_id

END MODULE sdf_job_info
