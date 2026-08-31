!
! SDF (Self-Describing Format) Fortran Library
! Copyright (c) 2011-2016, SDF Development Team
!
! Distributed under the terms of the BSD 3-clause License.
! See the LICENSE file for details.
!

MODULE sdf_output_point_ru

  USE mpi
  USE sdf_common
  USE sdf_output_ru

  IMPLICIT NONE

CONTAINS

  SUBROUTINE write_point_mesh_meta_r8(h, id, name, species_id, dim_labels, &
      dim_units, dim_mults)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN), OPTIONAL :: id, name, species_id
    CHARACTER(LEN=*), INTENT(IN), OPTIONAL :: dim_labels(:), dim_units(:)
    REAL(r8), DIMENSION(:), INTENT(IN), OPTIONAL :: dim_mults
    INTEGER :: ndims
    TYPE(sdf_block_type), POINTER :: b
    INTEGER :: i, errcode

    b => h%current_block

    b%blocktype = c_blocktype_point_mesh
    ndims = b%ndims
    b%nelements = b%ndims * b%npoints

    ! Metadata is
    ! - mults     REAL(r8), DIMENSION(ndims)
    ! - labels    CHARACTER(id_length), DIMENSION(ndims)
    ! - units     CHARACTER(id_length), DIMENSION(ndims)
    ! - geometry  INTEGER(i4)
    ! - minval    REAL(r8), DIMENSION(ndims)
    ! - maxval    REAL(r8), DIMENSION(ndims)
    ! - npoints   INTEGER(i8)
    ! - speciesid CHARACTER(id_length)

    b%info_length = h%block_header_length + soi4 + soi8 &
        + (3 * ndims) * sof8 + (2 * ndims + 1) * c_id_length
    b%data_length = b%nelements * b%type_size

    ! Write header

    IF (PRESENT(id)) THEN
      ALLOCATE(b%dim_labels(ndims), b%dim_units(ndims), b%dim_mults(ndims))

      IF (PRESENT(dim_labels)) THEN
        DO i = 1,ndims
          CALL sdf_safe_copy_id(h, dim_labels(i), b%dim_labels(i))
        END DO
      ELSE
        IF (ndims >= 1) CALL sdf_safe_copy_id(h, 'X', b%dim_labels(1))
        IF (ndims >= 2) CALL sdf_safe_copy_id(h, 'Y', b%dim_labels(2))
        IF (ndims >= 3) CALL sdf_safe_copy_id(h, 'Z', b%dim_labels(3))
      END IF

      IF (PRESENT(dim_units)) THEN
        DO i = 1,ndims
          CALL sdf_safe_copy_id(h, dim_units(i), b%dim_units(i))
        END DO
      ELSE
        DO i = 1,ndims
          CALL sdf_safe_copy_id(h, 'm', b%dim_units(i))
        END DO
      END IF

      IF (PRESENT(dim_mults)) THEN
        DO i = 1,ndims
          b%dim_mults(i) = REAL(dim_mults(i),r8)
        END DO
      ELSE
        DO i = 1,ndims
          b%dim_mults(i) = 1.d0
        END DO
      END IF

      IF (PRESENT(species_id)) THEN
        CALL sdf_safe_copy_id(h, species_id, b%species_id)
      ELSE
        CALL sdf_safe_copy_id(h, '__unknown__', b%species_id)
      END IF

      CALL sdf_write_block_header(h, id, name)
    ELSE
      CALL write_block_header(h)
    END IF

    IF (h%rank == h%rank_master) THEN
      CALL MPI_FILE_WRITE(h%filehandle, b%dim_mults, ndims, MPI_REAL8, &
          MPI_STATUS_IGNORE, errcode)

      DO i = 1,ndims
        CALL sdf_safe_write_id(h, b%dim_labels(i))
      END DO

      DO i = 1,ndims
        CALL sdf_safe_write_id(h, b%dim_units(i))
      END DO

      CALL MPI_FILE_WRITE(h%filehandle, b%geometry, 1, MPI_INTEGER4, &
          MPI_STATUS_IGNORE, errcode)

      CALL MPI_FILE_WRITE(h%filehandle, b%extents, 2 * ndims, MPI_REAL8, &
          MPI_STATUS_IGNORE, errcode)

      CALL MPI_FILE_WRITE(h%filehandle, b%npoints, 1, MPI_INTEGER8, &
          MPI_STATUS_IGNORE, errcode)

      CALL sdf_safe_write_id(h, b%species_id)
    END IF

    h%current_location = b%block_start + b%info_length
    b%done_info = .TRUE.

  END SUBROUTINE write_point_mesh_meta_r8



  SUBROUTINE write_point_mesh_meta_r4(h, id, name, species_id, dim_labels, &
      dim_units, dim_mults)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN), OPTIONAL :: id, name, species_id
    CHARACTER(LEN=*), INTENT(IN), OPTIONAL :: dim_labels(:), dim_units(:)
    REAL(r4), DIMENSION(:), INTENT(IN), OPTIONAL :: dim_mults
    REAL(r8), DIMENSION(c_maxdims) :: dim_mults8
    TYPE(sdf_block_type), POINTER :: b
    INTEGER :: i

    IF (PRESENT(dim_mults)) THEN
      b => h%current_block
      DO i = 1,b%ndims
        dim_mults8(i) = REAL(dim_mults(i),r8)
      END DO
      CALL write_point_mesh_meta_r8(h, id, name, species_id, dim_labels, &
          dim_units, dim_mults8)
    ELSE
      CALL write_point_mesh_meta_r8(h, id, name, species_id, dim_labels, &
          dim_units)
    END IF

  END SUBROUTINE write_point_mesh_meta_r4



  SUBROUTINE write_point_variable_meta_r8(h, id, name, species_id, units, &
      mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN), OPTIONAL :: id, name, species_id, units
    CHARACTER(LEN=*), INTENT(IN), OPTIONAL :: mesh_id
    REAL(r8), INTENT(IN), OPTIONAL :: mult
    INTEGER :: ndims
    TYPE(sdf_block_type), POINTER :: b
    INTEGER :: errcode

    b => h%current_block

    b%blocktype = c_blocktype_point_variable
    ndims = b%ndims
    b%nelements = b%ndims * b%npoints

    ! Metadata is
    ! - mult      REAL(r8)
    ! - units     CHARACTER(id_length)
    ! - meshid    CHARACTER(id_length)
    ! - npoints   INTEGER(i8)
    ! - speciesid CHARACTER(id_length)

    b%info_length = h%block_header_length + soi8 + sof8 + 3 * c_id_length
    b%data_length = b%nelements * b%type_size

    ! Write header

    IF (PRESENT(id)) THEN
      CALL sdf_safe_copy_id(h, units, b%units)
      CALL sdf_safe_copy_id(h, mesh_id, b%mesh_id)
      CALL sdf_safe_copy_id(h, species_id, b%species_id)

      IF (PRESENT(mult)) THEN
        b%mult = REAL(mult,r8)
      ELSE
        b%mult = 1.d0
      END IF

      CALL sdf_write_block_header(h, id, name)
    ELSE
      CALL write_block_header(h)
    END IF

    IF (h%rank == h%rank_master) THEN
      CALL MPI_FILE_WRITE(h%filehandle, b%mult, 1, MPI_REAL8, &
          MPI_STATUS_IGNORE, errcode)

      CALL sdf_safe_write_id(h, b%units)

      CALL sdf_safe_write_id(h, b%mesh_id)

      CALL MPI_FILE_WRITE(h%filehandle, b%npoints, 1, MPI_INTEGER8, &
          MPI_STATUS_IGNORE, errcode)

      CALL sdf_safe_write_id(h, b%species_id)
    END IF

    h%current_location = b%block_start + b%info_length
    b%done_info = .TRUE.

  END SUBROUTINE write_point_variable_meta_r8



  SUBROUTINE write_point_variable_meta_r4(h, id, name, species_id, units, &
      mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN), OPTIONAL :: id, name, species_id, units
    CHARACTER(LEN=*), INTENT(IN), OPTIONAL :: mesh_id
    REAL(r4), INTENT(IN), OPTIONAL :: mult

    IF (PRESENT(mult)) THEN
      CALL write_point_variable_meta_r8(h, id, name, species_id, units, &
          mesh_id, REAL(mult,r8))
    ELSE
      CALL write_point_variable_meta_r8(h, id, name, species_id, units, &
          mesh_id)
    END IF

  END SUBROUTINE write_point_variable_meta_r4



  SUBROUTINE write_srl_pt_var_logical_i8_r8(h, id, name, species_id, units, &
      array, npoint_global, mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, species_id, units
    LOGICAL, DIMENSION(:), INTENT(IN) :: array
    INTEGER(i8), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    REAL(r8), INTENT(IN), OPTIONAL :: mult
    INTEGER(i8) :: i, j, n, idx
    INTEGER :: npoint_max, npoint_rem, errcode
    TYPE(sdf_block_type), POINTER :: b
    CHARACTER(LEN=1), ALLOCATABLE :: cvalues(:)

    IF (npoint_global <= 0) RETURN

    CALL sdf_get_next_block(h)
    b => h%current_block

    b%type_size = INT(h%soi,r4)
    b%datatype = c_datatype_logical
    b%mpitype = MPI_CHARACTER
    b%ndims = 1
    b%dims = 0
    b%npoints = npoint_global

    ! Write header

    CALL write_point_variable_meta_r8(h, id, name, species_id, units, &
        mesh_id, mult)

    ! Write the real data

    IF (h%rank == h%rank_master) THEN
      h%current_location = b%data_location
      CALL MPI_FILE_SEEK(h%filehandle, h%current_location, MPI_SEEK_SET, &
          errcode)

      ! This is all a bit messy, but it is necessary because MPI_FILE_WRITE
      ! accepts an INTEGER count of elements to write, which may not be
      ! big enough for npoint_global which is an INTEGER*8

      npoint_max = HUGE(npoint_max)
      npoint_rem = INT(MOD(npoint_global, INT(npoint_max,i8)),i4)

      IF ((npoint_global / npoint_max) > 0) THEN
        ALLOCATE(cvalues(npoint_max))
      ELSE
        ALLOCATE(cvalues(npoint_global))
      END IF

      idx = 1
      DO i = 1, npoint_global / npoint_max
        n = idx
        DO j = 1, npoint_max
          IF (array(n)) THEN
            cvalues(j) = ACHAR(1)
          ELSE
            cvalues(j) = ACHAR(0)
          END IF
          n = n + 1
        END DO
        CALL MPI_FILE_WRITE(h%filehandle, cvalues, npoint_max, b%mpitype, &
            MPI_STATUS_IGNORE, errcode)
        idx = idx + npoint_max
      END DO

      n = idx
      DO j = 1, npoint_rem
        IF (array(n)) THEN
          cvalues(j) = ACHAR(1)
        ELSE
          cvalues(j) = ACHAR(0)
        END IF
        n = n + 1
      END DO
      CALL MPI_FILE_WRITE(h%filehandle, cvalues, npoint_rem, b%mpitype, &
          MPI_STATUS_IGNORE, errcode)

      DEALLOCATE(cvalues)
    END IF

    h%current_location = b%data_location + b%data_length
    b%done_data = .TRUE.

  END SUBROUTINE write_srl_pt_var_logical_i8_r8



  SUBROUTINE write_srl_pt_var_logical_i4_r8(h, id, name, species_id, units, &
      array, npoint_global, mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, species_id, units
    LOGICAL, DIMENSION(:), INTENT(IN) :: array
    INTEGER(i4), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    REAL(r8), INTENT(IN), OPTIONAL :: mult

    CALL write_srl_pt_var_logical_i8_r8(h, id, name, species_id, units, array, &
        INT(npoint_global,i8), mesh_id, mult)

  END SUBROUTINE write_srl_pt_var_logical_i4_r8



  SUBROUTINE write_srl_pt_var_logical_i8_r4(h, id, name, species_id, units, &
      array, npoint_global, mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, species_id, units
    LOGICAL, DIMENSION(:), INTENT(IN) :: array
    INTEGER(i8), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    REAL(r4), INTENT(IN) :: mult

    CALL write_srl_pt_var_logical_i8_r8(h, id, name, species_id, units, array, &
        npoint_global, mesh_id, REAL(mult,r8))

  END SUBROUTINE write_srl_pt_var_logical_i8_r4



  SUBROUTINE write_srl_pt_var_logical_i4_r4(h, id, name, species_id, units, &
      array, npoint_global, mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, species_id, units
    LOGICAL, DIMENSION(:), INTENT(IN) :: array
    INTEGER(i4), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    REAL(r4), INTENT(IN) :: mult

    CALL write_srl_pt_var_logical_i8_r8(h, id, name, species_id, units, array, &
        INT(npoint_global,i8), mesh_id, REAL(mult,r8))

  END SUBROUTINE write_srl_pt_var_logical_i4_r4



  SUBROUTINE write_srl_pt_var_int_i8_r8(h, id, name, species_id, units, array, &
      npoint_global, mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, species_id, units
    INTEGER, DIMENSION(:), INTENT(IN) :: array
    INTEGER(i8), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    REAL(r8), INTENT(IN), OPTIONAL :: mult
    INTEGER(i8) :: i, idx
    INTEGER :: npoint_max, npoint_rem, errcode
    TYPE(sdf_block_type), POINTER :: b

    IF (npoint_global <= 0) RETURN

    CALL sdf_get_next_block(h)
    b => h%current_block

    b%type_size = INT(h%soi,r4)
    b%datatype = h%datatype_integer
    b%mpitype = h%mpitype_integer
    b%ndims = 1
    b%dims = 0
    b%npoints = npoint_global

    ! Write header

    CALL write_point_variable_meta_r8(h, id, name, species_id, units, &
        mesh_id, mult)

    ! Write the real data

    IF (h%rank == h%rank_master) THEN
      h%current_location = b%data_location
      CALL MPI_FILE_SEEK(h%filehandle, h%current_location, MPI_SEEK_SET, &
          errcode)

      ! This is all a bit messy, but it is necessary because MPI_FILE_WRITE
      ! accepts an INTEGER count of elements to write, which may not be
      ! big enough for npoint_global which is an INTEGER*8

      npoint_max = HUGE(npoint_max)
      npoint_rem = INT(MOD(npoint_global, INT(npoint_max,i8)),i4)

      idx = 1
      DO i = 1, npoint_global / npoint_max
        CALL MPI_FILE_WRITE(h%filehandle, array(idx), npoint_max, b%mpitype, &
            MPI_STATUS_IGNORE, errcode)
        idx = idx + npoint_max
      END DO

      CALL MPI_FILE_WRITE(h%filehandle, array(idx), npoint_rem, b%mpitype, &
          MPI_STATUS_IGNORE, errcode)
    END IF

    h%current_location = b%data_location + b%data_length
    b%done_data = .TRUE.

  END SUBROUTINE write_srl_pt_var_int_i8_r8



  SUBROUTINE write_srl_pt_var_int_i4_r8(h, id, name, species_id, units, array, &
      npoint_global, mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, species_id, units
    INTEGER, DIMENSION(:), INTENT(IN) :: array
    INTEGER, INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    REAL(r8), INTENT(IN), OPTIONAL :: mult

    CALL write_srl_pt_var_int_i8_r8(h, id, name, species_id, units, array, &
        INT(npoint_global,i8), mesh_id, mult)

  END SUBROUTINE write_srl_pt_var_int_i4_r8



  SUBROUTINE write_srl_pt_var_int_i8_r4(h, id, name, species_id, units, array, &
      npoint_global, mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, species_id, units
    INTEGER, DIMENSION(:), INTENT(IN) :: array
    INTEGER(i8), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    REAL(r4), INTENT(IN) :: mult

    CALL write_srl_pt_var_int_i8_r8(h, id, name, species_id, units, array, &
        npoint_global, mesh_id, REAL(mult,r8))

  END SUBROUTINE write_srl_pt_var_int_i8_r4



  SUBROUTINE write_srl_pt_var_int_i4_r4(h, id, name, species_id, units, array, &
      npoint_global, mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, species_id, units
    INTEGER, DIMENSION(:), INTENT(IN) :: array
    INTEGER, INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    REAL(r4), INTENT(IN) :: mult

    CALL write_srl_pt_var_int_i8_r8(h, id, name, species_id, units, array, &
        INT(npoint_global,i8), mesh_id, REAL(mult,r8))

  END SUBROUTINE write_srl_pt_var_int_i4_r4



  !----------------------------------------------------------------------------
  ! Code to write a point variable in parallel using an iterator
  !----------------------------------------------------------------------------

  SUBROUTINE write_point_variable_i4(h, id, name, species_id, units, &
      npoint_global, mesh_id, iterator, offset, convert_in, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, species_id, units
    INTEGER(i8), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    INTEGER(i8), INTENT(IN) :: offset
    LOGICAL, INTENT(IN), OPTIONAL :: convert_in
    REAL(r8), INTENT(IN), OPTIONAL :: mult

    INTERFACE
      FUNCTION iterator(array, npoint_it, start, param)
        USE sdf_common
        INTEGER(i4) :: iterator
        INTEGER(i4), DIMENSION(:), INTENT(OUT) :: array
        INTEGER, INTENT(INOUT) :: npoint_it
        LOGICAL, INTENT(IN) :: start
        INTEGER, INTENT(IN), OPTIONAL :: param
      END FUNCTION iterator
    END INTERFACE

    CALL write_point_variable_gen_i4(h, id, name, species_id, units, &
        npoint_global, mesh_id, iterator, 0, offset, convert_in, mult)

  END SUBROUTINE write_point_variable_i4



  !----------------------------------------------------------------------------
  ! Code to write a point variable in parallel using an iterator and parameter
  !----------------------------------------------------------------------------

  SUBROUTINE write_point_variable_gen_i4(h, id, name, species_id, units, &
      npoint_global, mesh_id, iterator, param, offset, convert_in, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, species_id, units
    INTEGER(i8), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    INTEGER, INTENT(IN) :: param
    INTEGER(i8), INTENT(IN) :: offset
    LOGICAL, INTENT(IN), OPTIONAL :: convert_in
    REAL(r8), INTENT(IN), OPTIONAL :: mult

    INTERFACE
      FUNCTION iterator(array, npoint_it, start, param)
        USE sdf_common
        INTEGER(i4) :: iterator
        INTEGER(i4), DIMENSION(:), INTENT(OUT) :: array
        INTEGER, INTENT(INOUT) :: npoint_it
        LOGICAL, INTENT(IN) :: start
        INTEGER, INTENT(IN), OPTIONAL :: param
      END FUNCTION iterator
    END INTERFACE

    INTEGER(MPI_OFFSET_KIND) :: file_offset
    INTEGER :: errcode, npoint_this_cycle, nmax
    INTEGER :: stat
    LOGICAL :: start
    INTEGER(i4), ALLOCATABLE, DIMENSION(:) :: array
    TYPE(sdf_block_type), POINTER :: b
    INTEGER(i4) :: ret

    IF (npoint_global <= 0) RETURN

    ! Allocate buffer arrays

    start = .FALSE.
    DO
      stat = 0
      ALLOCATE(array(npoint_per_iteration), STAT=stat)

      IF (stat == 0) EXIT

      DEALLOCATE(array, STAT=stat)

      start = .TRUE.
      npoint_per_iteration = npoint_per_iteration / 4

      IF (npoint_per_iteration < 2) THEN
        IF (h%print_errors .AND. h%rank == h%rank_master) THEN
          PRINT*, '*** ERROR ***'
          PRINT*, 'SDF library was unable to allocate memory for output buffer'
        END IF
        h%error_code = c_err_sdf
        RETURN
      END IF
    END DO

    IF (start) THEN
      IF (h%print_warnings .AND. h%rank == h%rank_master) THEN
        PRINT*, '*** WARNING ***'
        PRINT*, 'SDF npoint_per_iteration reduced to ', npoint_per_iteration
      END IF
    END IF

    CALL sdf_get_next_block(h)
    b => h%current_block

    b%type_size = 4
    b%datatype = c_datatype_integer4
    b%mpitype = MPI_INTEGER4

    b%blocktype = c_blocktype_point_variable
    b%ndims = 1
    b%dims = 0
    b%npoints = npoint_global

    ! Write header

    CALL write_point_variable_meta_r8(h, id, name, species_id, units, &
        mesh_id, mult)

    ! Write the real data

    npoint_this_cycle = INT(npoint_per_iteration)
    start = .TRUE.
    file_offset = h%current_location + offset * b%type_size

    DO
      ret = iterator(array, npoint_this_cycle, start, param)
      nmax = npoint_this_cycle
      CALL MPI_ALLREDUCE(npoint_this_cycle, nmax, 1, MPI_INTEGER, &
          MPI_MAX, h%comm, errcode)
      IF (nmax <= 0) EXIT

      IF (start) start = .FALSE.

      CALL MPI_FILE_SET_VIEW(h%filehandle, file_offset, MPI_BYTE, &
          b%mpitype, 'native', MPI_INFO_NULL, errcode)
      CALL MPI_FILE_WRITE_ALL(h%filehandle, array, npoint_this_cycle, &
          b%mpitype, MPI_STATUS_IGNORE, errcode)

      file_offset = file_offset + npoint_this_cycle * b%type_size
    END DO

    DEALLOCATE(array)

    CALL MPI_FILE_SET_VIEW(h%filehandle, c_off0, MPI_BYTE, MPI_BYTE, 'native', &
        MPI_INFO_NULL, errcode)

    h%current_location = b%data_location + b%data_length
    b%done_data = .TRUE.

  END SUBROUTINE write_point_variable_gen_i4



  !----------------------------------------------------------------------------
  ! Code to write a point variable in parallel using an iterator
  !----------------------------------------------------------------------------

  SUBROUTINE write_point_variable_i8(h, id, name, species_id, units, &
      npoint_global, mesh_id, iterator, offset, convert_in, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, species_id, units
    INTEGER(i8), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    INTEGER(i8), INTENT(IN) :: offset
    LOGICAL, INTENT(IN), OPTIONAL :: convert_in
    REAL(r8), INTENT(IN), OPTIONAL :: mult

    INTERFACE
      FUNCTION iterator(array, npoint_it, start, param)
        USE sdf_common
        INTEGER(i8) :: iterator
        INTEGER(i8), DIMENSION(:), INTENT(OUT) :: array
        INTEGER, INTENT(INOUT) :: npoint_it
        LOGICAL, INTENT(IN) :: start
        INTEGER, INTENT(IN), OPTIONAL :: param
      END FUNCTION iterator
    END INTERFACE

    CALL write_point_variable_gen_i8(h, id, name, species_id, units, &
        npoint_global, mesh_id, iterator, 0, offset, convert_in, mult)

  END SUBROUTINE write_point_variable_i8



  !----------------------------------------------------------------------------
  ! Code to write a point variable in parallel using an iterator and parameter
  !----------------------------------------------------------------------------

  SUBROUTINE write_point_variable_gen_i8(h, id, name, species_id, units, &
      npoint_global, mesh_id, iterator, param, offset, convert_in, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, species_id, units
    INTEGER(i8), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    INTEGER, INTENT(IN) :: param
    INTEGER(i8), INTENT(IN) :: offset
    LOGICAL, INTENT(IN), OPTIONAL :: convert_in
    REAL(r8), INTENT(IN), OPTIONAL :: mult

    INTERFACE
      FUNCTION iterator(array, npoint_it, start, param)
        USE sdf_common
        INTEGER(i8) :: iterator
        INTEGER(i8), DIMENSION(:), INTENT(OUT) :: array
        INTEGER, INTENT(INOUT) :: npoint_it
        LOGICAL, INTENT(IN) :: start
        INTEGER, INTENT(IN), OPTIONAL :: param
      END FUNCTION iterator
    END INTERFACE

    INTEGER(MPI_OFFSET_KIND) :: file_offset
    INTEGER :: errcode, npoint_this_cycle, nmax
    INTEGER :: stat1, stat2
    LOGICAL :: start, convert
    INTEGER(i8), ALLOCATABLE, DIMENSION(:) :: array
    INTEGER(i4), ALLOCATABLE, DIMENSION(:) :: i4array
    TYPE(sdf_block_type), POINTER :: b
    INTEGER(i8) :: ret

    IF (npoint_global <= 0) RETURN

    IF (PRESENT(convert_in)) THEN
      convert = convert_in
    ELSE
      convert = .FALSE.
    END IF

    ! Allocate buffer arrays

    start = .FALSE.
    DO
      stat1 = 0
      stat2 = 0
      ALLOCATE(array(npoint_per_iteration), STAT=stat1)
      IF (convert) ALLOCATE(i4array(npoint_per_iteration), STAT=stat2)

      IF (stat1 + stat2 == 0) EXIT

      DEALLOCATE(array, STAT=stat1)
      IF (convert) DEALLOCATE(i4array, STAT=stat2)

      start = .TRUE.
      npoint_per_iteration = npoint_per_iteration / 4

      IF (npoint_per_iteration < 2) THEN
        IF (h%print_errors .AND. h%rank == h%rank_master) THEN
          PRINT*, '*** ERROR ***'
          PRINT*, 'SDF library was unable to allocate memory for output buffer'
        END IF
        h%error_code = c_err_sdf
        RETURN
      END IF
    END DO

    IF (start) THEN
      IF (h%print_warnings .AND. h%rank == h%rank_master) THEN
        PRINT*, '*** WARNING ***'
        PRINT*, 'SDF npoint_per_iteration reduced to ', npoint_per_iteration
      END IF
    END IF

    CALL sdf_get_next_block(h)
    b => h%current_block

    IF (convert) THEN
      b%type_size = 4
      b%datatype = c_datatype_integer4
      b%mpitype = MPI_INTEGER4
    ELSE
      b%type_size = 8
      b%datatype = c_datatype_integer8
      b%mpitype = MPI_INTEGER8
    END IF
    b%blocktype = c_blocktype_point_variable
    b%ndims = 1
    b%dims = 0
    b%npoints = npoint_global

    ! Write header

    CALL write_point_variable_meta_r8(h, id, name, species_id, units, &
        mesh_id, mult)

    ! Write the real data

    npoint_this_cycle = INT(npoint_per_iteration)
    start = .TRUE.
    file_offset = h%current_location + offset * b%type_size

    DO
      ret = iterator(array, npoint_this_cycle, start, param)
      nmax = npoint_this_cycle
      CALL MPI_ALLREDUCE(npoint_this_cycle, nmax, 1, MPI_INTEGER, &
          MPI_MAX, h%comm, errcode)
      IF (nmax <= 0) EXIT

      IF (start) start = .FALSE.

      CALL MPI_FILE_SET_VIEW(h%filehandle, file_offset, MPI_BYTE, &
          b%mpitype, 'native', MPI_INFO_NULL, errcode)
      IF (convert) THEN
        i4array(1:npoint_this_cycle) = INT(array(1:npoint_this_cycle),i4)
        CALL MPI_FILE_WRITE_ALL(h%filehandle, i4array, npoint_this_cycle, &
            b%mpitype, MPI_STATUS_IGNORE, errcode)
      ELSE
        CALL MPI_FILE_WRITE_ALL(h%filehandle, array, npoint_this_cycle, &
            b%mpitype, MPI_STATUS_IGNORE, errcode)
      END IF

      file_offset = file_offset + npoint_this_cycle * b%type_size
    END DO

    DEALLOCATE(array)
    IF (convert) DEALLOCATE(i4array)

    CALL MPI_FILE_SET_VIEW(h%filehandle, c_off0, MPI_BYTE, MPI_BYTE, 'native', &
        MPI_INFO_NULL, errcode)

    h%current_location = b%data_location + b%data_length
    b%done_data = .TRUE.

  END SUBROUTINE write_point_variable_gen_i8



  ! Calls without the species_id argument for backwards compatibility
  SUBROUTINE write_nospec_point_mesh_meta_r8(h, id, name, dim_labels, &
      dim_units, dim_mults)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN), OPTIONAL :: id, name
    CHARACTER(LEN=*), INTENT(IN), OPTIONAL :: dim_labels(:), dim_units(:)
    REAL(r8), DIMENSION(:), INTENT(IN), OPTIONAL :: dim_mults

    CALL write_point_mesh_meta_r8(h, id, name, 'nospec', dim_labels, &
        dim_units, dim_mults)

  END SUBROUTINE write_nospec_point_mesh_meta_r8



  SUBROUTINE write_nospec_point_mesh_meta_r4(h, id, name, dim_labels, &
      dim_units, dim_mults)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN), OPTIONAL :: id, name
    CHARACTER(LEN=*), INTENT(IN), OPTIONAL :: dim_labels(:), dim_units(:)
    REAL(r4), DIMENSION(:), INTENT(IN), OPTIONAL :: dim_mults

    CALL write_point_mesh_meta_r4(h, id, name, 'nospec', dim_labels, &
        dim_units, dim_mults)

  END SUBROUTINE write_nospec_point_mesh_meta_r4



  SUBROUTINE write_nospec_point_variable_meta_r8(h, id, name, units, &
      mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN), OPTIONAL :: id, name, units
    CHARACTER(LEN=*), INTENT(IN), OPTIONAL :: mesh_id
    REAL(r8), INTENT(IN), OPTIONAL :: mult

    CALL write_point_variable_meta_r8(h, id, name, 'nospec', units, &
        mesh_id, mult)

  END SUBROUTINE write_nospec_point_variable_meta_r8



  SUBROUTINE write_nospec_point_variable_meta_r4(h, id, name, units, &
      mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN), OPTIONAL :: id, name, units
    CHARACTER(LEN=*), INTENT(IN), OPTIONAL :: mesh_id
    REAL(r4), INTENT(IN), OPTIONAL :: mult

    CALL write_point_variable_meta_r4(h, id, name, 'nospec', units, &
        mesh_id, mult)

  END SUBROUTINE write_nospec_point_variable_meta_r4



  SUBROUTINE write_nospec_srl_pt_var_logical_i8_r8(h, id, name, units, &
      array, npoint_global, mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, units
    LOGICAL, DIMENSION(:), INTENT(IN) :: array
    INTEGER(i8), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    REAL(r8), INTENT(IN), OPTIONAL :: mult

    CALL write_srl_pt_var_logical_i8_r8(h, id, name, 'nospec', units, &
        array, npoint_global, mesh_id, mult)

  END SUBROUTINE write_nospec_srl_pt_var_logical_i8_r8



  SUBROUTINE write_nospec_srl_pt_var_logical_i4_r8(h, id, name, units, &
      array, npoint_global, mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, units
    LOGICAL, DIMENSION(:), INTENT(IN) :: array
    INTEGER(i4), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    REAL(r8), INTENT(IN), OPTIONAL :: mult

    CALL write_srl_pt_var_logical_i4_r8(h, id, name, 'nospec', units, &
        array, npoint_global, mesh_id, mult)

  END SUBROUTINE write_nospec_srl_pt_var_logical_i4_r8



  SUBROUTINE write_nospec_srl_pt_var_logical_i8_r4(h, id, name, units, &
      array, npoint_global, mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, units
    LOGICAL, DIMENSION(:), INTENT(IN) :: array
    INTEGER(i8), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    REAL(r4), INTENT(IN) :: mult

    CALL write_srl_pt_var_logical_i8_r4(h, id, name, 'nospec', units, &
        array, npoint_global, mesh_id, mult)

  END SUBROUTINE write_nospec_srl_pt_var_logical_i8_r4



  SUBROUTINE write_nospec_srl_pt_var_logical_i4_r4(h, id, name, units, &
      array, npoint_global, mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, units
    LOGICAL, DIMENSION(:), INTENT(IN) :: array
    INTEGER(i4), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    REAL(r4), INTENT(IN) :: mult

    CALL write_srl_pt_var_logical_i4_r4(h, id, name, 'nospec', units, &
        array, npoint_global, mesh_id, mult)

  END SUBROUTINE write_nospec_srl_pt_var_logical_i4_r4



  SUBROUTINE write_nospec_srl_pt_var_int_i8_r8(h, id, name, units, array, &
      npoint_global, mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, units
    INTEGER, DIMENSION(:), INTENT(IN) :: array
    INTEGER(i8), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    REAL(r8), INTENT(IN), OPTIONAL :: mult

    CALL write_srl_pt_var_int_i8_r8(h, id, name, 'nospec', units, array, &
        npoint_global, mesh_id, mult)

  END SUBROUTINE write_nospec_srl_pt_var_int_i8_r8



  SUBROUTINE write_nospec_srl_pt_var_int_i4_r8(h, id, name, units, array, &
      npoint_global, mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, units
    INTEGER, DIMENSION(:), INTENT(IN) :: array
    INTEGER, INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    REAL(r8), INTENT(IN), OPTIONAL :: mult

    CALL write_srl_pt_var_int_i4_r8(h, id, name, 'nospec', units, array, &
        npoint_global, mesh_id, mult)

  END SUBROUTINE write_nospec_srl_pt_var_int_i4_r8



  SUBROUTINE write_nospec_srl_pt_var_int_i8_r4(h, id, name, units, array, &
      npoint_global, mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, units
    INTEGER, DIMENSION(:), INTENT(IN) :: array
    INTEGER(i8), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    REAL(r4), INTENT(IN) :: mult

    CALL write_srl_pt_var_int_i8_r4(h, id, name, 'nospec', units, array, &
        npoint_global, mesh_id, mult)

  END SUBROUTINE write_nospec_srl_pt_var_int_i8_r4



  SUBROUTINE write_nospec_srl_pt_var_int_i4_r4(h, id, name, units, array, &
      npoint_global, mesh_id, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, units
    INTEGER, DIMENSION(:), INTENT(IN) :: array
    INTEGER, INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    REAL(r4), INTENT(IN) :: mult

    CALL write_srl_pt_var_int_i4_r4(h, id, name, 'nospec', units, array, &
        npoint_global, mesh_id, mult)

  END SUBROUTINE write_nospec_srl_pt_var_int_i4_r4



  SUBROUTINE write_nospec_point_variable_i4(h, id, name, units, &
      npoint_global, mesh_id, iterator, offset, convert_in, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, units
    INTEGER(i8), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    INTEGER(i8), INTENT(IN) :: offset
    LOGICAL, INTENT(IN), OPTIONAL :: convert_in
    REAL(r8), INTENT(IN), OPTIONAL :: mult

    INTERFACE
      FUNCTION iterator(array, npoint_it, start, param)
        USE sdf_common
        INTEGER(i4) :: iterator
        INTEGER(i4), DIMENSION(:), INTENT(OUT) :: array
        INTEGER, INTENT(INOUT) :: npoint_it
        LOGICAL, INTENT(IN) :: start
        INTEGER, INTENT(IN), OPTIONAL :: param
      END FUNCTION iterator
    END INTERFACE

    CALL write_point_variable_i4(h, id, name, 'nospec', units, &
        npoint_global, mesh_id, iterator, offset, convert_in, mult)

  END SUBROUTINE write_nospec_point_variable_i4



  SUBROUTINE write_nospec_point_variable_gen_i4(h, id, name, units, &
      npoint_global, mesh_id, iterator, param, offset, convert_in, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, units
    INTEGER(i8), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    INTEGER, INTENT(IN) :: param
    INTEGER(i8), INTENT(IN) :: offset
    LOGICAL, INTENT(IN), OPTIONAL :: convert_in
    REAL(r8), INTENT(IN), OPTIONAL :: mult

    INTERFACE
      FUNCTION iterator(array, npoint_it, start, param)
        USE sdf_common
        INTEGER(i4) :: iterator
        INTEGER(i4), DIMENSION(:), INTENT(OUT) :: array
        INTEGER, INTENT(INOUT) :: npoint_it
        LOGICAL, INTENT(IN) :: start
        INTEGER, INTENT(IN), OPTIONAL :: param
      END FUNCTION iterator
    END INTERFACE

    CALL write_point_variable_gen_i4(h, id, name, 'nospec', units, &
        npoint_global, mesh_id, iterator, param, offset, convert_in, mult)

  END SUBROUTINE write_nospec_point_variable_gen_i4



  SUBROUTINE write_nospec_point_variable_i8(h, id, name, units, &
      npoint_global, mesh_id, iterator, offset, convert_in, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, units
    INTEGER(i8), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    INTEGER(i8), INTENT(IN) :: offset
    LOGICAL, INTENT(IN), OPTIONAL :: convert_in
    REAL(r8), INTENT(IN), OPTIONAL :: mult

    INTERFACE
      FUNCTION iterator(array, npoint_it, start, param)
        USE sdf_common
        INTEGER(i8) :: iterator
        INTEGER(i8), DIMENSION(:), INTENT(OUT) :: array
        INTEGER, INTENT(INOUT) :: npoint_it
        LOGICAL, INTENT(IN) :: start
        INTEGER, INTENT(IN), OPTIONAL :: param
      END FUNCTION iterator
    END INTERFACE

    CALL write_point_variable_i8(h, id, name, 'nospec', units, &
        npoint_global, mesh_id, iterator, offset, convert_in, mult)

  END SUBROUTINE write_nospec_point_variable_i8



  SUBROUTINE write_nospec_point_variable_gen_i8(h, id, name, units, &
      npoint_global, mesh_id, iterator, param, offset, convert_in, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, units
    INTEGER(i8), INTENT(IN) :: npoint_global
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    INTEGER, INTENT(IN) :: param
    INTEGER(i8), INTENT(IN) :: offset
    LOGICAL, INTENT(IN), OPTIONAL :: convert_in
    REAL(r8), INTENT(IN), OPTIONAL :: mult

    INTERFACE
      FUNCTION iterator(array, npoint_it, start, param)
        USE sdf_common
        INTEGER(i8) :: iterator
        INTEGER(i8), DIMENSION(:), INTENT(OUT) :: array
        INTEGER, INTENT(INOUT) :: npoint_it
        LOGICAL, INTENT(IN) :: start
        INTEGER, INTENT(IN), OPTIONAL :: param
      END FUNCTION iterator
    END INTERFACE

    CALL write_point_variable_gen_i8(h, id, name, 'nospec', units, &
        npoint_global, mesh_id, iterator, param, offset, convert_in, mult)

  END SUBROUTINE write_nospec_point_variable_gen_i8

END MODULE sdf_output_point_ru
