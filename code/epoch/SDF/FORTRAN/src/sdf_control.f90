!
! SDF (Self-Describing Format) Fortran Library
! Copyright (c) 2010-2016, SDF Development Team
!
! Distributed under the terms of the BSD 3-clause License.
! See the LICENSE file for details.
!

MODULE sdf_control

  USE sdf_job_info
  USE sdf_output_util

  IMPLICIT NONE

  PRIVATE

  PUBLIC :: sdf_open
  PUBLIC :: sdf_close
  PUBLIC :: sdf_destroy_block
  PUBLIC :: sdf_destroy_blocklist
  PUBLIC :: sdf_set_string_length
  PUBLIC :: sdf_get_string_length
  PUBLIC :: sdf_get_max_string_length
  PUBLIC :: sdf_set_default_rank
  PUBLIC :: sdf_read_nblocks
  PUBLIC :: sdf_read_jobid
  PUBLIC :: sdf_errorcode

CONTAINS

  SUBROUTINE sdf_open(h, filename, sdf_comm_in, mode, handle_errors)

    TYPE(sdf_file_handle), TARGET :: h
    CHARACTER(LEN=*), INTENT(IN) :: filename
    INTEGER, INTENT(IN) :: sdf_comm_in
    INTEGER, INTENT(IN), OPTIONAL :: mode
    LOGICAL, INTENT(IN), OPTIONAL :: handle_errors
    INTEGER :: errcode, ierr, i, info, file_mode
    LOGICAL :: exists
    LOGICAL :: first_call = .TRUE.

    IF (first_call) THEN
      DO i = 1, max_handles
        sdf_handles(i)%filehandle = 0
        NULLIFY(sdf_handles(i)%handle)
      END DO
      first_call = .FALSE.
    END IF

    CALL initialise_file_handle(h, handle_errors)
    CALL sdf_set_default_rank(h, 0)

    h%filename = TRIM(filename)
    CALL MPI_COMM_DUP(sdf_comm_in, h%comm, errcode)
    CALL MPI_COMM_RANK(h%comm, h%rank, errcode)

    ierr = KIND(errcode)
    IF (ierr == i4) THEN
      ! Should use MPI_SIZEOF() but this breaks on scalimpi
      h%soi = 4
      h%datatype_integer = c_datatype_integer4
      h%mpitype_integer = MPI_INTEGER4
    ELSE IF (ierr == i8) THEN
      h%soi = 8
      h%datatype_integer = c_datatype_integer8
      h%mpitype_integer = MPI_INTEGER8
    ELSE
      h%error_code = c_err_unsupported_datarep + 64 * h%nblocks
      h%handled_error = .TRUE.
      RETURN
    END IF

    IF (PRESENT(mode)) THEN
      file_mode = mode
    ELSE
      file_mode = c_sdf_write
    END IF

    IF (file_mode == c_sdf_write) THEN
      h%writing = .TRUE.
      h%mode = MPI_MODE_CREATE + MPI_MODE_WRONLY

      ! Delete file
      IF (h%rank == h%rank_master) THEN
        INQUIRE(file=TRIM(filename), exist=exists)
        IF (exists) &
            CALL MPI_FILE_DELETE(TRIM(filename), MPI_INFO_NULL, errcode)
      END IF
    ELSE IF (file_mode == c_sdf_append) THEN
      h%writing = .TRUE.
      h%mode = MPI_MODE_CREATE + MPI_MODE_RDWR
    ELSE
      ! We're opening a file which already exists, so don't damage it
      h%writing = .FALSE.
      h%mode = MPI_MODE_RDONLY
    END IF

    CALL MPI_INFO_CREATE(info, errcode)
    CALL MPI_INFO_SET(info, 'romio_cb_write', 'enable', errcode)
    CALL MPI_FILE_OPEN(h%comm, TRIM(filename), h%mode, info, &
        h%filehandle, errcode)
    IF (errcode /= 0) h%error_code = map_error_code(errcode)
    CALL MPI_INFO_FREE(info, errcode)

    IF (h%filehandle /= 0) THEN
      IF (h%errhandler /= MPI_ERRHANDLER_NULL) THEN
        ! Restore default error handler if changed
        CALL MPI_FILE_GET_ERRHANDLER(MPI_FILE_NULL, errcode, ierr)
        IF (errcode /= h%old_errhandler) THEN
          CALL MPI_FILE_SET_ERRHANDLER(MPI_FILE_NULL, h%old_errhandler, errcode)
        END IF
        h%old_errhandler = MPI_ERRHANDLER_NULL

        CALL MPI_FILE_SET_ERRHANDLER(h%filehandle, h%errhandler, errcode)
      END IF

      DO i = 1, max_handles
        IF (sdf_handles(i)%filehandle == 0) THEN
          sdf_handles(i)%filehandle = h%filehandle
          sdf_handles(i)%handle => h
          open_handles = open_handles + 1
          EXIT
        END IF
      END DO
    END IF

  END SUBROUTINE sdf_open



  SUBROUTINE sdf_close(h)

    TYPE(sdf_file_handle) :: h
    INTEGER :: errcode

    ! No open file
    IF (h%filehandle == -1) RETURN

    ! If writing
    IF (h%writing) THEN
      IF (.NOT.h%station_file) CALL sdf_write_summary(h)

      CALL sdf_flush(h)
    END IF

    CALL MPI_FILE_SET_VIEW(h%filehandle, c_off0, MPI_BYTE, MPI_BYTE, 'native', &
        MPI_INFO_NULL, errcode)

    CALL MPI_BARRIER(h%comm, errcode)

    CALL MPI_FILE_CLOSE(h%filehandle, errcode)
    h%errhandler = MPI_ERRHANDLER_NULL

    CALL sdf_destroy_blocklist(h)
    CALL deallocate_file_handle(h)

  END SUBROUTINE sdf_close



  SUBROUTINE sdf_destroy_block(b)

    TYPE(sdf_block_type), POINTER :: b

    IF (.NOT. ASSOCIATED(b)) RETURN

    CALL deallocate_block_type(b)
    DEALLOCATE(b)

  END SUBROUTINE sdf_destroy_block



  SUBROUTINE sdf_destroy_blocklist(h)

    TYPE(sdf_file_handle) :: h
    TYPE(sdf_block_type), POINTER :: b, next
    INTEGER :: i

    IF (.NOT.ASSOCIATED(h%blocklist)) RETURN

    b => h%blocklist
    DO i = 1,h%nblocks
      next => b%next_block
      CALL sdf_destroy_block(b)
      b => next
    END DO

    NULLIFY(h%blocklist)
    NULLIFY(h%current_block)

  END SUBROUTINE sdf_destroy_blocklist



  SUBROUTINE sdf_set_string_length(h, maxlen)

    TYPE(sdf_file_handle) :: h
    INTEGER, INTENT(IN) :: maxlen

    h%string_length = MIN(INT(maxlen,i4), c_max_string_length)

  END SUBROUTINE sdf_set_string_length



  FUNCTION sdf_get_string_length(h) RESULT(length)

    TYPE(sdf_file_handle) :: h
    INTEGER :: length

    length = INT(h%string_length)

  END FUNCTION sdf_get_string_length



  FUNCTION sdf_get_max_string_length() RESULT(length)

    INTEGER :: length

    length = INT(c_max_string_length)

  END FUNCTION sdf_get_max_string_length



  SUBROUTINE sdf_set_default_rank(h, rank_in)

    TYPE(sdf_file_handle) :: h
    INTEGER, INTENT(IN) :: rank_in

    h%default_rank = INT(rank_in,i4)
    h%rank_master = h%default_rank

  END SUBROUTINE sdf_set_default_rank



  FUNCTION sdf_read_nblocks(h)

    TYPE(sdf_file_handle) :: h
    INTEGER :: sdf_read_nblocks

    sdf_read_nblocks = h%nblocks

  END FUNCTION sdf_read_nblocks



  FUNCTION sdf_read_jobid(h)

    TYPE(sdf_file_handle) :: h
    TYPE(jobid_type) :: sdf_read_jobid

    sdf_read_jobid = h%jobid

  END FUNCTION sdf_read_jobid



  FUNCTION sdf_errorcode(h)

    TYPE(sdf_file_handle) :: h
    INTEGER :: sdf_errorcode

    IF (h%handled_error) THEN
      sdf_errorcode = h%error_code
    ELSE
      sdf_errorcode = c_err_success
    END IF

  END FUNCTION sdf_errorcode

END MODULE sdf_control
