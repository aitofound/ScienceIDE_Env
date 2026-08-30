!
! SDF (Self-Describing Format) Fortran Library
! Copyright (c) 2010-2016, SDF Development Team
!
! Distributed under the terms of the BSD 3-clause License.
! See the LICENSE file for details.
!

MODULE sdf_input

  USE sdf_input_r4
  USE sdf_input_r8

  IMPLICIT NONE

  INTERFACE sdf_read_header
    MODULE PROCEDURE &
        read_header_ru, &
        read_header_r4, &
        read_header_r8
  END INTERFACE sdf_read_header

  INTERFACE sdf_read_srl
    MODULE PROCEDURE &
        read_constant_real_r4,  &
        read_constant_real_r8,  &
        read_constant_integer,  &
        read_constant_logical,  &
        read_1d_array_real_r4,  &
        read_2d_array_real_r4,  &
        read_3d_array_real_r4,  &
        read_1d_array_real_r8,  &
        read_2d_array_real_r8,  &
        read_3d_array_real_r8,  &
        read_1d_array_integer,  &
        read_2d_array_integer,  &
        read_1d_array_integer8, &
        read_1d_array_logical,  &
        read_2d_array_character
  END INTERFACE sdf_read_srl

  INTERFACE sdf_read_array
    MODULE PROCEDURE &
        read_1d_array_par_real_spec_r8, &
        read_1d_array_par_real_r8,      &
        read_2d_array_par_real_spec_r8, &
        read_2d_array_par_real_r8,      &
        read_3d_array_par_real_spec_r8, &
        read_3d_array_par_real_r8,      &
        read_1d_array_par_real_spec_r4, &
        read_1d_array_par_real_r4,      &
        read_2d_array_par_real_spec_r4, &
        read_2d_array_par_real_r4,      &
        read_3d_array_par_real_spec_r4, &
        read_3d_array_par_real_r4
  END INTERFACE sdf_read_array

  INTERFACE sdf_read_srl_cpu_split
    MODULE PROCEDURE &
        read_srl_cpu_split, &
        read_srl_cpu_split_part
  END INTERFACE sdf_read_srl_cpu_split

  INTERFACE sdf_read_run_info
    MODULE PROCEDURE &
        read_run_info, &
        read_run_info_old, &
        read_run_info_minor
  END INTERFACE sdf_read_run_info

  INTERFACE sdf_read_namevalue
    MODULE PROCEDURE &
        read_namevalue, &
        read_namevalue_i4, &
        read_namevalue_i8, &
        read_namevalue_r4, &
        read_namevalue_r8, &
        read_namevalue_logical, &
        read_namevalue_string
  END INTERFACE sdf_read_namevalue

END MODULE sdf_input
