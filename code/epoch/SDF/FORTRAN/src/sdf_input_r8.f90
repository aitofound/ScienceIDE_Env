!
! SDF (Self-Describing Format) Fortran Library
! Copyright (c) 2011-2016, SDF Development Team
!
! Distributed under the terms of the BSD 3-clause License.
! See the LICENSE file for details.
!

MODULE sdf_input_r8

  USE sdf_input_ru

  IMPLICIT NONE

CONTAINS

  SUBROUTINE read_header_r8(h, step, time, code_name, code_io_version, &
      string_length, restart_flag, other_domains)

    TYPE(sdf_file_handle) :: h
    INTEGER, INTENT(OUT) :: step
    REAL(r8), INTENT(OUT) :: time
    CHARACTER(LEN=*), INTENT(OUT), OPTIONAL :: code_name
    INTEGER, INTENT(OUT), OPTIONAL :: code_io_version, string_length
    LOGICAL, INTENT(OUT), OPTIONAL :: restart_flag, other_domains

    CALL read_header_ru(h, step)

    time = REAL(h%time,r8)
    IF (PRESENT(code_io_version)) code_io_version = h%code_io_version
    IF (PRESENT(restart_flag)) restart_flag = h%restart_flag
    IF (PRESENT(other_domains)) other_domains = h%other_domains
    IF (PRESENT(code_name)) CALL sdf_safe_copy_string(h%code_name, code_name)
    IF (PRESENT(string_length)) string_length = h%string_length

  END SUBROUTINE read_header_r8



  SUBROUTINE read_constant_real_r8(h, value)

    TYPE(sdf_file_handle) :: h
    REAL(r8), INTENT(OUT) :: value
    REAL(r4) :: real4
    REAL(r8) :: real8
    TYPE(sdf_block_type), POINTER :: b

    CALL sdf_read_constant(h)

    b => h%current_block

    IF (b%datatype == c_datatype_real4) THEN
      real4 = TRANSFER(b%const_value, real4)
      value = REAL(real4,r8)
    ELSE IF (b%datatype == c_datatype_real8) THEN
      real8 = TRANSFER(b%const_value, real8)
      value = REAL(real8,r8)
    END IF

  END SUBROUTINE read_constant_real_r8



  SUBROUTINE read_1d_array_real_r8(h, values)

    TYPE(sdf_file_handle) :: h
    REAL(r8), DIMENSION(:), INTENT(OUT) :: values
    INTEGER :: errcode, n1
    TYPE(sdf_block_type), POINTER :: b

    IF (sdf_check_block_header(h)) RETURN

    b => h%current_block
    IF (.NOT. b%done_info) CALL sdf_read_array_info(h)

    n1 = b%dims(1)

    h%current_location = b%data_location

    IF (h%rank == h%rank_master) THEN
      CALL MPI_FILE_READ_AT(h%filehandle, h%current_location, values, n1, &
          b%mpitype, MPI_STATUS_IGNORE, errcode)
    END IF

    CALL MPI_BCAST(values, n1, b%mpitype, h%rank_master, h%comm, errcode)

    h%current_location = b%next_block_location
    b%done_data = .TRUE.

  END SUBROUTINE read_1d_array_real_r8



  SUBROUTINE read_1d_array_par_real_spec_r8(h, values, subarray, distribution)

    TYPE(sdf_file_handle), INTENT(INOUT) :: h
    REAL(r8), DIMENSION(:), INTENT(OUT) :: values
    INTEGER, INTENT(IN) :: subarray, distribution
    INTEGER :: errcode
    TYPE(sdf_block_type), POINTER :: b

    IF (sdf_check_block_header(h)) RETURN

    b => h%current_block
    IF (.NOT. b%done_info) CALL sdf_read_array_info(h)

    h%current_location = b%data_location
    CALL MPI_FILE_SET_VIEW(h%filehandle, h%current_location, MPI_BYTE, &
        distribution, 'native', MPI_INFO_NULL, errcode)
    CALL MPI_FILE_READ_ALL(h%filehandle, values, 1, subarray, &
        MPI_STATUS_IGNORE, errcode)
    CALL MPI_FILE_SET_VIEW(h%filehandle, c_off0, MPI_BYTE, MPI_BYTE, 'native', &
        MPI_INFO_NULL, errcode)

    h%current_location = b%next_block_location
    b%done_data = .TRUE.

  END SUBROUTINE read_1d_array_par_real_spec_r8



  SUBROUTINE read_1d_array_par_real_r8(h, values, sz, local_starts, &
      local_ghosts, null_proc)

    INTEGER, PARAMETER :: ndims = 1
    TYPE(sdf_file_handle), INTENT(INOUT) :: h
    REAL(r8), DIMENSION(:), INTENT(OUT) :: values
    INTEGER, DIMENSION(ndims), INTENT(IN) :: sz
    INTEGER, DIMENSION(ndims), INTENT(IN) :: local_starts
    INTEGER, DIMENSION(2*ndims), INTENT(IN), OPTIONAL :: local_ghosts
    LOGICAL, INTENT(IN), OPTIONAL :: null_proc
    INTEGER, DIMENSION(2*ndims) :: ghosts
    INTEGER :: distribution, subarray, errcode
    INTEGER, DIMENSION(ndims) :: starts, sizes, subsizes
    LOGICAL :: not_this_processor

    IF (sdf_check_block_header(h)) RETURN

    not_this_processor = .FALSE.
    IF (PRESENT(null_proc)) THEN
      not_this_processor = null_proc
    ELSE IF (ANY(local_starts < 0)) THEN
      not_this_processor = .TRUE.
    END IF

    IF (.NOT. not_this_processor) THEN
      ghosts = 0
      IF (PRESENT(local_ghosts)) ghosts = local_ghosts

      starts = local_starts - 1
      sizes = sz
      subsizes = SHAPE(values) - ghosts(1:ndims) - ghosts(ndims+1:)

      CALL MPI_TYPE_CREATE_SUBARRAY(ndims, sizes, subsizes, starts, &
          MPI_ORDER_FORTRAN, MPI_REAL8, distribution, errcode)
      CALL MPI_TYPE_COMMIT(distribution, errcode)

      ! Subsizes are unchanged
      starts = ghosts(1:ndims)
      sizes = SHAPE(values)

      CALL MPI_TYPE_CREATE_SUBARRAY(ndims, sizes, subsizes, starts, &
          MPI_ORDER_FORTRAN, MPI_REAL8, subarray, errcode)
      CALL MPI_TYPE_COMMIT(subarray, errcode)
    ELSE
      CALL MPI_TYPE_CONTIGUOUS(0, MPI_REAL8, distribution, errcode)
      CALL MPI_TYPE_COMMIT(distribution, errcode)
      subarray = distribution
    END IF

    CALL read_1d_array_par_real_spec_r8(h, values, subarray, distribution)

    IF (subarray /= distribution) CALL MPI_TYPE_FREE(subarray, errcode)
    CALL MPI_TYPE_FREE(distribution, errcode)

  END SUBROUTINE read_1d_array_par_real_r8



  SUBROUTINE read_2d_array_real_r8(h, values)

    TYPE(sdf_file_handle) :: h
    REAL(r8), DIMENSION(:,:), INTENT(OUT) :: values
    INTEGER :: errcode, n1
    TYPE(sdf_block_type), POINTER :: b

    IF (sdf_check_block_header(h)) RETURN

    b => h%current_block
    IF (.NOT. b%done_info) CALL sdf_read_array_info(h)

    n1 = b%dims(1) * b%dims(2)

    h%current_location = b%data_location

    IF (h%rank == h%rank_master) THEN
      CALL MPI_FILE_READ_AT(h%filehandle, h%current_location, values, n1, &
          b%mpitype, MPI_STATUS_IGNORE, errcode)
    END IF

    CALL MPI_BCAST(values, n1, b%mpitype, h%rank_master, h%comm, errcode)

    h%current_location = b%next_block_location
    b%done_data = .TRUE.

  END SUBROUTINE read_2d_array_real_r8



  SUBROUTINE read_2d_array_par_real_spec_r8(h, values, subarray, distribution)

    TYPE(sdf_file_handle), INTENT(INOUT) :: h
    REAL(r8), DIMENSION(:,:), INTENT(OUT) :: values
    INTEGER, INTENT(IN) :: subarray, distribution
    INTEGER :: errcode
    TYPE(sdf_block_type), POINTER :: b

    IF (sdf_check_block_header(h)) RETURN

    b => h%current_block
    IF (.NOT. b%done_info) CALL sdf_read_array_info(h)

    h%current_location = b%data_location
    CALL MPI_FILE_SET_VIEW(h%filehandle, h%current_location, MPI_BYTE, &
        distribution, 'native', MPI_INFO_NULL, errcode)
    CALL MPI_FILE_READ_ALL(h%filehandle, values, 1, subarray, &
        MPI_STATUS_IGNORE, errcode)
    CALL MPI_FILE_SET_VIEW(h%filehandle, c_off0, MPI_BYTE, MPI_BYTE, 'native', &
        MPI_INFO_NULL, errcode)

    h%current_location = b%next_block_location
    b%done_data = .TRUE.

  END SUBROUTINE read_2d_array_par_real_spec_r8



  SUBROUTINE read_2d_array_par_real_r8(h, values, sz, local_starts, &
      local_ghosts, null_proc)

    INTEGER, PARAMETER :: ndims = 2
    TYPE(sdf_file_handle), INTENT(INOUT) :: h
    REAL(r8), DIMENSION(:,:), INTENT(OUT) :: values
    INTEGER, DIMENSION(ndims), INTENT(IN) :: sz
    INTEGER, DIMENSION(ndims), INTENT(IN) :: local_starts
    INTEGER, DIMENSION(2*ndims), INTENT(IN), OPTIONAL :: local_ghosts
    LOGICAL, INTENT(IN), OPTIONAL :: null_proc
    INTEGER, DIMENSION(2*ndims) :: ghosts
    INTEGER :: distribution, subarray, errcode
    INTEGER, DIMENSION(ndims) :: starts, sizes, subsizes
    LOGICAL :: not_this_processor

    IF (sdf_check_block_header(h)) RETURN

    not_this_processor = .FALSE.
    IF (PRESENT(null_proc)) THEN
      not_this_processor = null_proc
    ELSE IF (ANY(local_starts < 0)) THEN
      not_this_processor = .TRUE.
    END IF

    IF (.NOT. not_this_processor) THEN
      ghosts = 0
      IF (PRESENT(local_ghosts)) ghosts = local_ghosts

      starts = local_starts - 1
      sizes = sz
      subsizes = SHAPE(values) - ghosts(1:ndims) - ghosts(ndims+1:)

      CALL MPI_TYPE_CREATE_SUBARRAY(ndims, sizes, subsizes, starts, &
          MPI_ORDER_FORTRAN, MPI_REAL8, distribution, errcode)
      CALL MPI_TYPE_COMMIT(distribution, errcode)

      ! Subsizes are unchanged
      starts = ghosts(1:ndims)
      sizes = SHAPE(values)

      CALL MPI_TYPE_CREATE_SUBARRAY(ndims, sizes, subsizes, starts, &
          MPI_ORDER_FORTRAN, MPI_REAL8, subarray, errcode)
      CALL MPI_TYPE_COMMIT(subarray, errcode)
    ELSE
      CALL MPI_TYPE_CONTIGUOUS(0, MPI_REAL8, distribution, errcode)
      CALL MPI_TYPE_COMMIT(distribution, errcode)
      subarray = distribution
    END IF

    CALL read_2d_array_par_real_spec_r8(h, values, subarray, distribution)

    IF (subarray /= distribution) CALL MPI_TYPE_FREE(subarray, errcode)
    CALL MPI_TYPE_FREE(distribution, errcode)

  END SUBROUTINE read_2d_array_par_real_r8



  SUBROUTINE read_3d_array_real_r8(h, values)

    TYPE(sdf_file_handle) :: h
    REAL(r8), DIMENSION(:,:,:), INTENT(OUT) :: values
    INTEGER :: errcode, n1
    TYPE(sdf_block_type), POINTER :: b

    IF (sdf_check_block_header(h)) RETURN

    b => h%current_block
    IF (.NOT. b%done_info) CALL sdf_read_array_info(h)

    n1 = b%dims(1) * b%dims(2) * b%dims(3)

    h%current_location = b%data_location

    IF (h%rank == h%rank_master) THEN
      CALL MPI_FILE_READ_AT(h%filehandle, h%current_location, values, n1, &
          b%mpitype, MPI_STATUS_IGNORE, errcode)
    END IF

    CALL MPI_BCAST(values, n1, b%mpitype, h%rank_master, h%comm, errcode)

    h%current_location = b%next_block_location
    b%done_data = .TRUE.

  END SUBROUTINE read_3d_array_real_r8



  SUBROUTINE read_3d_array_par_real_spec_r8(h, values, subarray, distribution)

    TYPE(sdf_file_handle), INTENT(INOUT) :: h
    REAL(r8), DIMENSION(:,:,:), INTENT(OUT) :: values
    INTEGER, INTENT(IN) :: subarray, distribution
    INTEGER :: errcode
    TYPE(sdf_block_type), POINTER :: b

    IF (sdf_check_block_header(h)) RETURN

    b => h%current_block
    IF (.NOT. b%done_info) CALL sdf_read_array_info(h)

    h%current_location = b%data_location
    CALL MPI_FILE_SET_VIEW(h%filehandle, h%current_location, MPI_BYTE, &
        distribution, 'native', MPI_INFO_NULL, errcode)
    CALL MPI_FILE_READ_ALL(h%filehandle, values, 1, subarray, &
        MPI_STATUS_IGNORE, errcode)
    CALL MPI_FILE_SET_VIEW(h%filehandle, c_off0, MPI_BYTE, MPI_BYTE, 'native', &
        MPI_INFO_NULL, errcode)

    h%current_location = b%next_block_location
    b%done_data = .TRUE.

  END SUBROUTINE read_3d_array_par_real_spec_r8



  SUBROUTINE read_3d_array_par_real_r8(h, values, sz, local_starts, &
      local_ghosts, null_proc)

    INTEGER, PARAMETER :: ndims = 3
    TYPE(sdf_file_handle), INTENT(INOUT) :: h
    REAL(r8), DIMENSION(:,:,:), INTENT(OUT) :: values
    INTEGER, DIMENSION(ndims), INTENT(IN) :: sz
    INTEGER, DIMENSION(ndims), INTENT(IN) :: local_starts
    INTEGER, DIMENSION(2*ndims), INTENT(IN), OPTIONAL :: local_ghosts
    LOGICAL, INTENT(IN), OPTIONAL :: null_proc
    INTEGER, DIMENSION(2*ndims) :: ghosts
    INTEGER :: distribution, subarray, errcode
    INTEGER, DIMENSION(ndims) :: starts, sizes, subsizes
    LOGICAL :: not_this_processor

    IF (sdf_check_block_header(h)) RETURN

    not_this_processor = .FALSE.
    IF (PRESENT(null_proc)) THEN
      not_this_processor = null_proc
    ELSE IF (ANY(local_starts < 0)) THEN
      not_this_processor = .TRUE.
    END IF

    IF (.NOT. not_this_processor) THEN
      ghosts = 0
      IF (PRESENT(local_ghosts)) ghosts = local_ghosts

      starts = local_starts - 1
      sizes = sz
      subsizes = SHAPE(values) - ghosts(1:ndims) - ghosts(ndims+1:)

      CALL MPI_TYPE_CREATE_SUBARRAY(ndims, sizes, subsizes, starts, &
          MPI_ORDER_FORTRAN, MPI_REAL8, distribution, errcode)
      CALL MPI_TYPE_COMMIT(distribution, errcode)

      ! Subsizes are unchanged
      starts = ghosts(1:ndims)
      sizes = SHAPE(values)

      CALL MPI_TYPE_CREATE_SUBARRAY(ndims, sizes, subsizes, starts, &
          MPI_ORDER_FORTRAN, MPI_REAL8, subarray, errcode)
      CALL MPI_TYPE_COMMIT(subarray, errcode)
    ELSE
      CALL MPI_TYPE_CONTIGUOUS(0, MPI_REAL8, distribution, errcode)
      CALL MPI_TYPE_COMMIT(distribution, errcode)
      subarray = distribution
    END IF

    CALL read_3d_array_par_real_spec_r8(h, values, subarray, distribution)

    IF (subarray /= distribution) CALL MPI_TYPE_FREE(subarray, errcode)
    CALL MPI_TYPE_FREE(distribution, errcode)

  END SUBROUTINE read_3d_array_par_real_r8

END MODULE sdf_input_r8
