!
! SDF (Self-Describing Format) Fortran Library
! Copyright (c) 2014-2016, SDF Development Team
!
! Distributed under the terms of the BSD 3-clause License.
! See the LICENSE file for details.
!

MODULE sdf_output_source

  USE sdf_common
  USE sdf_source_info
  USE sdf_output

  IMPLICIT NONE

CONTAINS

  SUBROUTINE sdf_get_source_output(is_written, only_diff)

    LOGICAL, INTENT(OUT) :: is_written
    LOGICAL, INTENT(IN), OPTIONAL :: only_diff
    LOGICAL :: write_diff = .FALSE.

    is_written = .FALSE.

    IF (PRESENT(only_diff)) write_diff = only_diff

    IF (.NOT.write_diff) THEN
      IF (SIZE(sdf_bytes) > 1 .OR. &
            (TRIM(sdf_bytes_checksum_type) /= '' .AND. &
            ICHAR(sdf_bytes_checksum_type(1:1)) /= 0)) THEN
        is_written = .TRUE.
      END IF
    END IF

    IF (write_diff .AND. SIZE(sdf_diff_bytes) > 1) THEN
      is_written = .TRUE.
    END IF

  END SUBROUTINE sdf_get_source_output



  SUBROUTINE sdf_write_source_info(h, only_diff)

    TYPE(sdf_file_handle) :: h
    LOGICAL, INTENT(IN), OPTIONAL :: only_diff
    CHARACTER(LEN=c_id_length) :: stitched_ids(3)
    CHARACTER(LEN=c_id_length) :: time_string
    CHARACTER(LEN=512) :: string_array(6)
    LOGICAL :: write_diff = .FALSE.
    INTEGER :: n, i

    n = 0

    IF (PRESENT(only_diff)) write_diff = only_diff

    IF (.NOT.write_diff) THEN
      IF (SIZE(sdf_bytes) > 1 .OR. &
            (TRIM(sdf_bytes_checksum_type) /= '' .AND. &
            ICHAR(sdf_bytes_checksum_type(1:1)) /= 0)) THEN
        n = n + 1
        CALL sdf_safe_copy_id(h, 'sdf_source/source', stitched_ids(n))
        CALL sdf_write_datablock(h, stitched_ids(n), &
            'SDF source code', sdf_bytes, &
            sdf_bytes_padding, sdf_bytes_mimetype, &
            sdf_bytes_checksum_type, sdf_bytes_checksum)
      ELSE
        write_diff = .TRUE.
      END IF
    END IF

    IF (write_diff .AND. SIZE(sdf_diff_bytes) > 1) THEN
      n = n + 1
      CALL sdf_safe_copy_id(h, 'sdf_source/diff', stitched_ids(n))
      CALL sdf_write_datablock(h, stitched_ids(n), &
          'SDF repository differences', sdf_diff_bytes, &
          sdf_diff_bytes_padding, sdf_diff_bytes_mimetype, &
          sdf_diff_bytes_checksum_type, sdf_diff_bytes_checksum)
    END IF

    n = n + 1
    CALL sdf_safe_copy_id(h, 'sdf_source/info', stitched_ids(n))
    WRITE(time_string, '(I20)') sdf_bytes_compile_date

    string_array(1) = TRIM(sdf_bytes_git_version)
    string_array(2) = TRIM(sdf_bytes_compile_date_string)
    string_array(3) = TRIM(ADJUSTL(time_string))
    string_array(4) = TRIM(sdf_bytes_compile_machine_info)
    string_array(5) = TRIM(sdf_bytes_compiler_info)
    string_array(6) = TRIM(sdf_bytes_compiler_flags)

    ! Prevent truncation warning
    DO i = 1, 6
      string_array(i)(h%string_length:512) = ACHAR(0)
    END DO

    CALL sdf_write_namevalue(h, stitched_ids(n), &
        'SDF repository information', &
        (/'git_version         ', &
          'compile_date_string ', &
          'compile_date_seconds', &
          'compile_machine_info', &
          'compiler_info       ', &
          'compiler_flags      '/), string_array)

    CALL sdf_write_stitched(h, 'sdf_source', 'SDF source', &
        stitched_ids(1), c_stagger_cell_centre, stitched_ids, n)

  END SUBROUTINE sdf_write_source_info

END MODULE sdf_output_source
