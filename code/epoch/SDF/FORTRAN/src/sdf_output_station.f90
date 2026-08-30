!
! SDF (Self-Describing Format) Fortran Library
! Copyright (c) 2013-2016, SDF Development Team
!
! Distributed under the terms of the BSD 3-clause License.
! See the LICENSE file for details.
!

MODULE sdf_output_station

  USE sdf_output_station_r4
  USE sdf_output_station_r8

  IMPLICIT NONE

  INTERFACE sdf_write_station_header
    MODULE PROCEDURE &
        write_station_header_1d_r4, &
        write_station_header_2d_r4, &
        write_station_header_3d_r4, &
        write_station_header_1d_r8, &
        write_station_header_2d_r8, &
        write_station_header_3d_r8
  END INTERFACE sdf_write_station_header

  INTERFACE sdf_write_station_array
    MODULE PROCEDURE &
        write_station_array_r4_r4, &
        write_station_array_r8_r4, &
        write_station_array_r4_r8, &
        write_station_array_r8_r8, &
        write_station_array2d_r4_r4, &
        write_station_array2d_r8_r4, &
        write_station_array2d_r4_r8, &
        write_station_array2d_r8_r8, &
        write_srl_station_array_r4_r4, &
        write_srl_station_array_r8_r4, &
        write_srl_station_array_r4_r8, &
        write_srl_station_array_r8_r8, &
        write_srl_station_array2d_r4_r4, &
        write_srl_station_array2d_r8_r4, &
        write_srl_station_array2d_r4_r8, &
        write_srl_station_array2d_r8_r8
  END INTERFACE sdf_write_station_array

END MODULE sdf_output_station
