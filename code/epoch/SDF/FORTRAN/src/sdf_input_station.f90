!
! SDF (Self-Describing Format) Fortran Library
! Copyright (c) 2013-2016, SDF Development Team
!
! Distributed under the terms of the BSD 3-clause License.
! See the LICENSE file for details.
!

MODULE sdf_input_station

  USE sdf_input_station_ru
  USE sdf_input_station_r4
  USE sdf_input_station_r8

  IMPLICIT NONE

  INTERFACE sdf_read_station_array
    MODULE PROCEDURE &
        read_station_array_r4_r4, &
        read_station_array_r8_r4, &
        read_station_array_r4_r8, &
        read_station_array_r8_r8
  END INTERFACE sdf_read_station_array

END MODULE sdf_input_station
