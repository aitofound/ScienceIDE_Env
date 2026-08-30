!
! SDF (Self-Describing Format) Fortran Library
! Copyright (c) 2013-2016, SDF Development Team
!
! Distributed under the terms of the BSD 3-clause License.
! See the LICENSE file for details.
!

MODULE sdf_output_station_r4

  USE sdf_output_station_ru

  IMPLICIT NONE

  INTEGER, PARAMETER, PRIVATE :: sof = 4
  INTEGER, PARAMETER, PRIVATE :: datatype_real = c_datatype_real4
  INTEGER, PARAMETER, PRIVATE :: mpitype_real = MPI_REAL4

CONTAINS

  SUBROUTINE station_pre(h, time, step)

    TYPE(sdf_file_handle) :: h
    REAL(r8), INTENT(IN) :: time
    INTEGER, INTENT(IN) :: step
    TYPE(sdf_block_type), POINTER :: b
    REAL(r4) :: real4
    INTEGER :: errcode

    IF (.NOT.ASSOCIATED(h%current_block)) THEN
      IF (h%print_warnings .AND. h%rank == h%rank_master) THEN
        PRINT*,'*** WARNING ***'
        PRINT*,'SDF block cannot be found. Ignoring call.'
      END IF
      RETURN
    END IF

    b => h%current_block
    IF (b%blocktype /= c_blocktype_station) THEN
      IF (h%print_warnings .AND. h%rank == h%rank_master) THEN
        PRINT*,'*** WARNING ***'
        PRINT*,'SDF unable to write station data. Ignoring call.'
      END IF
      RETURN
    END IF

    h%time = time
    h%step = step

    IF (h%rank == h%rank_master) THEN
      h%current_location = b%data_location + b%data_length
      CALL MPI_FILE_SEEK(h%filehandle, h%current_location, MPI_SEEK_SET, &
          errcode)

      real4 = REAL(time,r4)
      CALL MPI_FILE_WRITE(h%filehandle, real4, 1, mpitype_real, &
          MPI_STATUS_IGNORE, errcode)
    END IF
    h%current_location = b%data_location + b%data_length + sof

  END SUBROUTINE station_pre



  SUBROUTINE station_post(h)

    TYPE(sdf_file_handle) :: h
    TYPE(sdf_block_type), POINTER :: b

    b => h%current_block
    b%nelements = b%nelements + 1
    b%data_length = b%data_length + b%type_size

    CALL write_station_update(h)

    h%current_location = b%data_location + b%data_length
    b%done_data = .TRUE.

  END SUBROUTINE station_post



  SUBROUTINE write_station_array_r8_r4(h, time, step, array, &
                                       distribution, subarray)

    TYPE(sdf_file_handle) :: h
    REAL(r8), INTENT(IN) :: time
    INTEGER, INTENT(IN) :: step
    REAL(r4), DIMENSION(:), INTENT(IN) :: array
    INTEGER, INTENT(IN) :: distribution, subarray
    INTEGER :: errcode

    CALL station_pre(h, time, step)

    CALL MPI_FILE_SET_VIEW(h%filehandle, h%current_location, MPI_BYTE, &
        distribution, 'native', MPI_INFO_NULL, errcode)

    CALL MPI_FILE_WRITE_ALL(h%filehandle, array, 1, subarray, &
        MPI_STATUS_IGNORE, errcode)

    CALL MPI_FILE_SET_VIEW(h%filehandle, c_off0, MPI_BYTE, MPI_BYTE, &
        'native', MPI_INFO_NULL, errcode)

    CALL station_post(h)

  END SUBROUTINE write_station_array_r8_r4



  SUBROUTINE write_station_array2d_r8_r4(h, time, step, array, &
                                         distribution, subarray)

    TYPE(sdf_file_handle) :: h
    REAL(r8), INTENT(IN) :: time
    INTEGER, INTENT(IN) :: step
    REAL(r4), DIMENSION(:,:), INTENT(IN) :: array
    INTEGER, INTENT(IN) :: distribution, subarray
    INTEGER :: errcode

    CALL station_pre(h, time, step)

    CALL MPI_FILE_SET_VIEW(h%filehandle, h%current_location, MPI_BYTE, &
        distribution, 'native', MPI_INFO_NULL, errcode)

    CALL MPI_FILE_WRITE_ALL(h%filehandle, array, 1, subarray, &
        MPI_STATUS_IGNORE, errcode)

    CALL MPI_FILE_SET_VIEW(h%filehandle, c_off0, MPI_BYTE, MPI_BYTE, &
        'native', MPI_INFO_NULL, errcode)

    CALL station_post(h)

  END SUBROUTINE write_station_array2d_r8_r4



  SUBROUTINE write_station_array_r4_r4(h, time, step, array, &
                                       distribution, subarray)

    TYPE(sdf_file_handle) :: h
    REAL(r4), INTENT(IN) :: time
    INTEGER, INTENT(IN) :: step
    REAL(r4), DIMENSION(:), INTENT(IN) :: array
    INTEGER, INTENT(IN) :: distribution, subarray

    CALL write_station_array_r8_r4(h, REAL(time,r8), step, array, &
                                   distribution, subarray)

  END SUBROUTINE write_station_array_r4_r4



  SUBROUTINE write_station_array2d_r4_r4(h, time, step, array, &
                                         distribution, subarray)

    TYPE(sdf_file_handle) :: h
    REAL(r4), INTENT(IN) :: time
    INTEGER, INTENT(IN) :: step
    REAL(r4), DIMENSION(:,:), INTENT(IN) :: array
    INTEGER, INTENT(IN) :: distribution, subarray

    CALL write_station_array2d_r8_r4(h, REAL(time,r8), step, array, &
                                     distribution, subarray)

  END SUBROUTINE write_station_array2d_r4_r4



  SUBROUTINE write_srl_station_array_r8_r4(h, time, step, array)

    TYPE(sdf_file_handle) :: h
    REAL(r8), INTENT(IN) :: time
    INTEGER, INTENT(IN) :: step
    REAL(r4), DIMENSION(:), INTENT(IN) :: array
    TYPE(sdf_block_type), POINTER :: b
    INTEGER :: errcode

    CALL station_pre(h, time, step)

    IF (h%rank == h%rank_master) THEN
      b => h%current_block
      CALL MPI_FILE_WRITE(h%filehandle, array, b%nvariables-1, mpitype_real, &
          MPI_STATUS_IGNORE, errcode)
    END IF

    CALL station_post(h)

  END SUBROUTINE write_srl_station_array_r8_r4



  SUBROUTINE write_srl_station_array2d_r8_r4(h, time, step, array)

    TYPE(sdf_file_handle) :: h
    REAL(r8), INTENT(IN) :: time
    INTEGER, INTENT(IN) :: step
    REAL(r4), DIMENSION(:,:), INTENT(IN) :: array
    TYPE(sdf_block_type), POINTER :: b
    INTEGER :: errcode

    CALL station_pre(h, time, step)

    IF (h%rank == h%rank_master) THEN
      b => h%current_block
      CALL MPI_FILE_WRITE(h%filehandle, array, b%nvariables-1, mpitype_real, &
          MPI_STATUS_IGNORE, errcode)
    END IF

    CALL station_post(h)

  END SUBROUTINE write_srl_station_array2d_r8_r4



  SUBROUTINE write_srl_station_array_r4_r4(h, time, step, array)

    TYPE(sdf_file_handle) :: h
    REAL(r4), INTENT(IN) :: time
    INTEGER, INTENT(IN) :: step
    REAL(r4), DIMENSION(:), INTENT(IN) :: array

    CALL write_srl_station_array_r8_r4(h, REAL(time,r8), step, array)

  END SUBROUTINE write_srl_station_array_r4_r4



  SUBROUTINE write_srl_station_array2d_r4_r4(h, time, step, array)

    TYPE(sdf_file_handle) :: h
    REAL(r4), INTENT(IN) :: time
    INTEGER, INTENT(IN) :: step
    REAL(r4), DIMENSION(:,:), INTENT(IN) :: array

    CALL write_srl_station_array2d_r8_r4(h, REAL(time,r8), step, array)

  END SUBROUTINE write_srl_station_array2d_r4_r4

END MODULE sdf_output_station_r4
