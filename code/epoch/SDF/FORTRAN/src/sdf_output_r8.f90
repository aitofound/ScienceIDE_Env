!
! SDF (Self-Describing Format) Fortran Library
! Copyright (c) 2011-2016, SDF Development Team
!
! Distributed under the terms of the BSD 3-clause License.
! See the LICENSE file for details.
!

MODULE sdf_output_r8

  USE mpi
  USE sdf_common
  USE sdf_output_ru

  IMPLICIT NONE

  INTEGER, PARAMETER, PRIVATE :: sof = 8
  INTEGER, PARAMETER, PRIVATE :: datatype_real = c_datatype_real8
  INTEGER, PARAMETER, PRIVATE :: mpitype_real = MPI_REAL8

CONTAINS

  SUBROUTINE write_constant_real_r8(h, id, name, value, rank_write)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name
    REAL(r8), INTENT(IN) :: value
    INTEGER, INTENT(IN), OPTIONAL :: rank_write
    TYPE(sdf_block_type), POINTER :: b

    CALL sdf_get_next_block(h)
    b => h%current_block

    b%type_size = sof
    b%datatype = datatype_real
    b%mpitype = mpitype_real

    IF (PRESENT(rank_write)) h%rank_master = rank_write

    b%const_value(1:sof) = TRANSFER(value, b%const_value(1:sof))

    CALL write_constant_meta(h, id, name)

    h%rank_master = h%default_rank

  END SUBROUTINE write_constant_real_r8



  SUBROUTINE write_1d_array_real_spec_r8(h, id, name, n1, array, rank_write)

    INTEGER, PARAMETER :: ndims = 1
    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name
    INTEGER, INTENT(IN) :: n1
    REAL(r8), DIMENSION(:), INTENT(IN) :: array
    INTEGER, INTENT(IN), OPTIONAL :: rank_write
    INTEGER :: errcode
    TYPE(sdf_block_type), POINTER :: b

    CALL sdf_get_next_block(h)
    b => h%current_block

    b%type_size = sof
    b%datatype = datatype_real
    b%mpitype = mpitype_real
    b%ndims = ndims

    IF (PRESENT(rank_write)) h%rank_master = rank_write

    b%dims(1) = n1

    ! Write header

    CALL write_array_meta(h, id, name)

    h%current_location = b%data_location

    IF (h%rank == h%rank_master) THEN
      CALL MPI_FILE_SEEK(h%filehandle, h%current_location, MPI_SEEK_SET, &
          errcode)

      ! Actual array
      CALL MPI_FILE_WRITE(h%filehandle, array, n1, b%mpitype, &
          MPI_STATUS_IGNORE, errcode)
    END IF

    h%rank_master = h%default_rank
    h%current_location = b%data_location + b%data_length
    b%done_data = .TRUE.

  END SUBROUTINE write_1d_array_real_spec_r8



  SUBROUTINE write_1d_array_real_r8(h, id, name, array, rank_write)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name
    REAL(r8), DIMENSION(:), INTENT(IN) :: array
    INTEGER, INTENT(IN), OPTIONAL :: rank_write
    INTEGER :: n1

    n1 = SIZE(array,1)
    CALL write_1d_array_real_spec_r8(h, id, name, n1, array, rank_write)

  END SUBROUTINE write_1d_array_real_r8



  SUBROUTINE write_2d_array_real_spec_r8(h, id, name, n1, n2, array, rank_write)

    INTEGER, PARAMETER :: ndims = 2
    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name
    INTEGER, INTENT(IN) :: n1, n2
    REAL(r8), DIMENSION(:,:), INTENT(IN) :: array
    INTEGER, INTENT(IN), OPTIONAL :: rank_write
    INTEGER :: errcode, var_len, i
    TYPE(sdf_block_type), POINTER :: b

    CALL sdf_get_next_block(h)
    b => h%current_block

    b%type_size = sof
    b%datatype = datatype_real
    b%mpitype = mpitype_real
    b%ndims = ndims

    IF (PRESENT(rank_write)) h%rank_master = rank_write

    b%dims(1) = n1
    b%dims(2) = n2

    ! Write header

    CALL write_array_meta(h, id, name)

    h%current_location = b%data_location

    IF (h%rank == h%rank_master) THEN
      CALL MPI_FILE_SEEK(h%filehandle, h%current_location, MPI_SEEK_SET, &
          errcode)

      ! Actual array
      IF (n1 == SIZE(array,1)) THEN
        var_len = INT(b%nelements)
        CALL MPI_FILE_WRITE(h%filehandle, array, var_len, b%mpitype, &
            MPI_STATUS_IGNORE, errcode)
      ELSE
        DO i = 1,n2
          CALL MPI_FILE_WRITE(h%filehandle, array(1,i), n1, b%mpitype, &
              MPI_STATUS_IGNORE, errcode)
        END DO
      END IF
    END IF

    h%rank_master = h%default_rank
    h%current_location = b%data_location + b%data_length
    b%done_data = .TRUE.

  END SUBROUTINE write_2d_array_real_spec_r8



  SUBROUTINE write_2d_array_real_r8(h, id, name, array, rank_write)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name
    REAL(r8), DIMENSION(:,:), INTENT(IN) :: array
    INTEGER, INTENT(IN), OPTIONAL :: rank_write
    INTEGER :: n1, n2

    n1 = SIZE(array,1)
    n2 = SIZE(array,2)
    CALL write_2d_array_real_spec_r8(h, id, name, n1, n2, array, rank_write)

  END SUBROUTINE write_2d_array_real_r8



  SUBROUTINE write_1d_array_real_spec_r8_par(h, id, name, sz, array, &
      distribution, subarray)

    INTEGER, PARAMETER :: ndims = 1
    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name
    INTEGER, DIMENSION(ndims), INTENT(IN) :: sz
    REAL(r8), DIMENSION(:), INTENT(IN) :: array
    INTEGER, INTENT(IN) :: distribution, subarray
    INTEGER :: errcode
    TYPE(sdf_block_type), POINTER :: b

    CALL sdf_get_next_block(h)
    b => h%current_block

    b%type_size = sof
    b%datatype = datatype_real
    b%mpitype = mpitype_real
    b%ndims = ndims

    b%dims(1:ndims) = sz

    ! Write header

    CALL write_array_meta(h, id, name)

    ! Write data
    h%current_location = b%data_location
    CALL MPI_FILE_SET_VIEW(h%filehandle, h%current_location, MPI_BYTE, &
        distribution, 'native', MPI_INFO_NULL, errcode)
    CALL MPI_FILE_WRITE_ALL(h%filehandle, array, 1, subarray, &
        MPI_STATUS_IGNORE, errcode)
    CALL MPI_FILE_SET_VIEW(h%filehandle, c_off0, MPI_BYTE, MPI_BYTE, 'native', &
        MPI_INFO_NULL, errcode)

    h%current_location = b%data_location + b%data_length
    b%done_data = .TRUE.

  END SUBROUTINE write_1d_array_real_spec_r8_par



  SUBROUTINE write_1d_array_real_r8_par(h, id, name, array, &
      sz, local_starts, local_ghosts, null_proc)

    INTEGER, PARAMETER :: ndims = 1
    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name
    REAL(r8), DIMENSION(:), INTENT(IN) :: array
    INTEGER, DIMENSION(ndims), INTENT(IN) :: sz, local_starts
    INTEGER, DIMENSION(2*ndims), INTENT(IN), OPTIONAL :: local_ghosts
    LOGICAL, INTENT(IN), OPTIONAL :: null_proc
    INTEGER, DIMENSION(2*ndims) :: ghosts
    INTEGER, DIMENSION(ndims) :: starts, sizes, subsizes
    INTEGER :: distribution, subarray, errcode
    LOGICAL :: not_this_processor

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
      subsizes = SHAPE(array) - ghosts(1:ndims) - ghosts(ndims+1:)

      CALL MPI_TYPE_CREATE_SUBARRAY(ndims, sizes, subsizes, starts, &
          MPI_ORDER_FORTRAN, mpitype_real, distribution, errcode)
      CALL MPI_TYPE_COMMIT(distribution, errcode)

      ! Subsizes are unchanged
      starts = ghosts(1:ndims)
      sizes = SHAPE(array)

      CALL MPI_TYPE_CREATE_SUBARRAY(ndims, sizes, subsizes, starts, &
          MPI_ORDER_FORTRAN, mpitype_real, subarray, errcode)
      CALL MPI_TYPE_COMMIT(subarray, errcode)
    ELSE
      CALL MPI_TYPE_CONTIGUOUS(0, mpitype_real, distribution, errcode)
      CALL MPI_TYPE_COMMIT(distribution, errcode)
      subarray = distribution
    END IF

    CALL write_1d_array_real_spec_r8_par(h, id, name, &
        sz, array, distribution, subarray)

    IF (subarray /= distribution) CALL MPI_TYPE_FREE(subarray, errcode)
    CALL MPI_TYPE_FREE(distribution, errcode)

  END SUBROUTINE write_1d_array_real_r8_par



  SUBROUTINE write_2d_array_real_spec_r8_par(h, id, name, sz, array, &
      distribution, subarray)

    INTEGER, PARAMETER :: ndims = 2
    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name
    INTEGER, DIMENSION(ndims), INTENT(IN) :: sz
    REAL(r8), DIMENSION(:,:), INTENT(IN) :: array
    INTEGER, INTENT(IN) :: distribution, subarray
    INTEGER :: errcode
    TYPE(sdf_block_type), POINTER :: b

    CALL sdf_get_next_block(h)
    b => h%current_block

    b%type_size = sof
    b%datatype = datatype_real
    b%mpitype = mpitype_real
    b%ndims = ndims

    b%dims(1:ndims) = sz

    ! Write header

    CALL write_array_meta(h, id, name)

    ! Write data
    h%current_location = b%data_location
    CALL MPI_FILE_SET_VIEW(h%filehandle, h%current_location, MPI_BYTE, &
        distribution, 'native', MPI_INFO_NULL, errcode)
    CALL MPI_FILE_WRITE_ALL(h%filehandle, array, 1, subarray, &
        MPI_STATUS_IGNORE, errcode)
    CALL MPI_FILE_SET_VIEW(h%filehandle, c_off0, MPI_BYTE, MPI_BYTE, 'native', &
        MPI_INFO_NULL, errcode)

    h%current_location = b%data_location + b%data_length
    b%done_data = .TRUE.

  END SUBROUTINE write_2d_array_real_spec_r8_par



  SUBROUTINE write_2d_array_real_r8_par(h, id, name, array, &
      sz, local_starts, local_ghosts, null_proc)

    INTEGER, PARAMETER :: ndims = 2
    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name
    REAL(r8), DIMENSION(:,:), INTENT(IN) :: array
    INTEGER, DIMENSION(ndims), INTENT(IN) :: sz, local_starts
    INTEGER, DIMENSION(2*ndims), INTENT(IN), OPTIONAL :: local_ghosts
    LOGICAL, INTENT(IN), OPTIONAL :: null_proc
    INTEGER, DIMENSION(2*ndims) :: ghosts
    INTEGER, DIMENSION(ndims) :: starts, sizes, subsizes
    INTEGER :: distribution, subarray, errcode
    LOGICAL :: not_this_processor

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
      subsizes = SHAPE(array) - ghosts(1:ndims) - ghosts(ndims+1:)

      CALL MPI_TYPE_CREATE_SUBARRAY(ndims, sizes, subsizes, starts, &
          MPI_ORDER_FORTRAN, mpitype_real, distribution, errcode)
      CALL MPI_TYPE_COMMIT(distribution, errcode)

      ! Subsizes are unchanged
      starts = ghosts(1:ndims)
      sizes = SHAPE(array)

      CALL MPI_TYPE_CREATE_SUBARRAY(ndims, sizes, subsizes, starts, &
          MPI_ORDER_FORTRAN, mpitype_real, subarray, errcode)
      CALL MPI_TYPE_COMMIT(subarray, errcode)
    ELSE
      CALL MPI_TYPE_CONTIGUOUS(0, mpitype_real, distribution, errcode)
      CALL MPI_TYPE_COMMIT(distribution, errcode)
      subarray = distribution
    END IF

    CALL write_2d_array_real_spec_r8_par(h, id, name, &
        sz, array, distribution, subarray)

    IF (subarray /= distribution) CALL MPI_TYPE_FREE(subarray, errcode)
    CALL MPI_TYPE_FREE(distribution, errcode)

  END SUBROUTINE write_2d_array_real_r8_par



  SUBROUTINE write_3d_array_real_spec_r8_par(h, id, name, sz, array, &
      distribution, subarray)

    INTEGER, PARAMETER :: ndims = 3
    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name
    INTEGER, DIMENSION(ndims), INTENT(IN) :: sz
    REAL(r8), DIMENSION(:,:,:), INTENT(IN) :: array
    INTEGER, INTENT(IN) :: distribution, subarray
    INTEGER :: errcode
    TYPE(sdf_block_type), POINTER :: b

    CALL sdf_get_next_block(h)
    b => h%current_block

    b%type_size = sof
    b%datatype = datatype_real
    b%mpitype = mpitype_real
    b%ndims = ndims

    b%dims(1:ndims) = sz

    ! Write header

    CALL write_array_meta(h, id, name)

    ! Write data
    h%current_location = b%data_location
    CALL MPI_FILE_SET_VIEW(h%filehandle, h%current_location, MPI_BYTE, &
        distribution, 'native', MPI_INFO_NULL, errcode)
    CALL MPI_FILE_WRITE_ALL(h%filehandle, array, 1, subarray, &
        MPI_STATUS_IGNORE, errcode)
    CALL MPI_FILE_SET_VIEW(h%filehandle, c_off0, MPI_BYTE, MPI_BYTE, 'native', &
        MPI_INFO_NULL, errcode)

    h%current_location = b%data_location + b%data_length
    b%done_data = .TRUE.

  END SUBROUTINE write_3d_array_real_spec_r8_par



  SUBROUTINE write_3d_array_real_r8_par(h, id, name, array, &
      sz, local_starts, local_ghosts, null_proc)

    INTEGER, PARAMETER :: ndims = 3
    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name
    REAL(r8), DIMENSION(:,:,:), INTENT(IN) :: array
    INTEGER, DIMENSION(ndims), INTENT(IN) :: sz, local_starts
    INTEGER, DIMENSION(2*ndims), INTENT(IN), OPTIONAL :: local_ghosts
    LOGICAL, INTENT(IN), OPTIONAL :: null_proc
    INTEGER, DIMENSION(2*ndims) :: ghosts
    INTEGER, DIMENSION(ndims) :: starts, sizes, subsizes
    INTEGER :: distribution, subarray, errcode
    LOGICAL :: not_this_processor

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
      subsizes = SHAPE(array) - ghosts(1:ndims) - ghosts(ndims+1:)

      CALL MPI_TYPE_CREATE_SUBARRAY(ndims, sizes, subsizes, starts, &
          MPI_ORDER_FORTRAN, mpitype_real, distribution, errcode)
      CALL MPI_TYPE_COMMIT(distribution, errcode)

      ! Subsizes are unchanged
      starts = ghosts(1:ndims)
      sizes = SHAPE(array)

      CALL MPI_TYPE_CREATE_SUBARRAY(ndims, sizes, subsizes, starts, &
          MPI_ORDER_FORTRAN, mpitype_real, subarray, errcode)
      CALL MPI_TYPE_COMMIT(subarray, errcode)
    ELSE
      CALL MPI_TYPE_CONTIGUOUS(0, mpitype_real, distribution, errcode)
      CALL MPI_TYPE_COMMIT(distribution, errcode)
      subarray = distribution
    END IF

    CALL write_3d_array_real_spec_r8_par(h, id, name, &
        sz, array, distribution, subarray)

    IF (subarray /= distribution) CALL MPI_TYPE_FREE(subarray, errcode)
    CALL MPI_TYPE_FREE(distribution, errcode)

  END SUBROUTINE write_3d_array_real_r8_par

END MODULE sdf_output_r8
