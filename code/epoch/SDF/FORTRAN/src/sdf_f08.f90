MODULE sdf_f08

  USE mpi_f08_types
  USE sdf_common
  USE sdf_control, sdf_open_f90 => sdf_open
  USE sdf_output_ru
  USE sdf_output_cartesian_r8

  IMPLICIT NONE

  PRIVATE

  PUBLIC :: sdf_file_handle
  PUBLIC :: c_sdf_write
  PUBLIC :: c_stagger_cell_centre
  PUBLIC :: c_stagger_vertex
  PUBLIC :: c_stagger_face_x
  PUBLIC :: c_stagger_face_y
  PUBLIC :: c_stagger_face_z

  PUBLIC :: sdf_open
  PUBLIC :: sdf_close

  PUBLIC :: sdf_write_header
  PUBLIC :: sdf_write_plain_mesh
  PUBLIC :: sdf_write_plain_variable

  INTERFACE sdf_open
    MODULE PROCEDURE sdf_open_f08
  END INTERFACE sdf_open

  INTERFACE sdf_write_header
    MODULE PROCEDURE write_header_r8
  END INTERFACE sdf_write_header

  INTERFACE sdf_write_plain_mesh
    MODULE PROCEDURE write_2d_mesh_r8_f08
    MODULE PROCEDURE write_3d_mesh_r8_f08
  END INTERFACE sdf_write_plain_mesh

  INTERFACE sdf_write_plain_variable
    MODULE PROCEDURE write_2d_float_r8_f08
    MODULE PROCEDURE write_3d_float_r8_f08
  END INTERFACE sdf_write_plain_variable

CONTAINS

  SUBROUTINE sdf_open_f08(h, filename, sdf_comm_in, mode, handle_errors)

    TYPE(sdf_file_handle), TARGET :: h
    CHARACTER(LEN=*), INTENT(IN) :: filename
    TYPE(MPI_COMM), INTENT(IN) :: sdf_comm_in
    INTEGER, INTENT(IN), OPTIONAL :: mode
    LOGICAL, INTENT(IN), OPTIONAL :: handle_errors

    CALL sdf_open_f90(h, filename, sdf_comm_in%mpi_val, mode, handle_errors)

  END SUBROUTINE sdf_open_f08



  SUBROUTINE write_2d_mesh_r8_f08(h, id, name, x, y, dims, xmin, xmax, &
      ymin, ymax, distribution, subarray, convert_in, dim_labels, &
      dim_units, dim_mults, geometry)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name
    REAL(r8), DIMENSION(:), INTENT(IN) :: x, y
    INTEGER, DIMENSION(:), INTENT(IN) :: dims
    REAL(r8), INTENT(IN) :: xmin, xmax, ymin, ymax
    TYPE(MPI_DATATYPE), DIMENSION(:), INTENT(IN) :: distribution, subarray
    LOGICAL, INTENT(IN), OPTIONAL :: convert_in
    CHARACTER(LEN=*), INTENT(IN), OPTIONAL :: dim_labels(:), dim_units(:)
    REAL(r8), DIMENSION(:), INTENT(IN), OPTIONAL :: dim_mults
    INTEGER, INTENT(IN), OPTIONAL :: geometry

    CALL write_2d_mesh_r8(h, id, name, x, y, dims, xmin, xmax, &
      ymin, ymax, distribution%MPI_VAL, subarray%MPI_VAL, &
      convert_in, dim_labels, dim_units, dim_mults, geometry)

  END SUBROUTINE write_2d_mesh_r8_f08



  SUBROUTINE write_3d_mesh_r8_f08(h, id, name, x, y, z, dims, xmin, xmax, &
      ymin, ymax, zmin, zmax, distribution, subarray, convert_in, dim_labels, &
      dim_units, dim_mults, geometry)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name
    REAL(r8), DIMENSION(:), INTENT(IN) :: x, y, z
    INTEGER, DIMENSION(:), INTENT(IN) :: dims
    REAL(r8), INTENT(IN) :: xmin, xmax, ymin, ymax, zmin, zmax
    TYPE(MPI_DATATYPE), DIMENSION(:), INTENT(IN) :: distribution, subarray
    LOGICAL, INTENT(IN), OPTIONAL :: convert_in
    CHARACTER(LEN=*), INTENT(IN), OPTIONAL :: dim_labels(:), dim_units(:)
    REAL(r8), DIMENSION(:), INTENT(IN), OPTIONAL :: dim_mults
    INTEGER, INTENT(IN), OPTIONAL :: geometry

    CALL write_3d_mesh_r8(h, id, name, x, y, z, dims, xmin, xmax, &
      ymin, ymax, zmin, zmax, distribution%MPI_VAL, subarray%MPI_VAL, &
      convert_in, dim_labels, dim_units, dim_mults, geometry)

  END SUBROUTINE write_3d_mesh_r8_f08



  SUBROUTINE write_2d_float_r8_f08(h, id, name, units, dims, stagger, mesh_id, &
      variable, distribution, subarray, convert, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, units
    INTEGER, DIMENSION(:), INTENT(IN) :: dims
    INTEGER(i4), INTENT(IN) :: stagger
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    REAL(r8), DIMENSION(:,:), INTENT(IN) :: variable
    TYPE(MPI_DATATYPE), INTENT(IN) :: distribution, subarray
    LOGICAL, OPTIONAL, INTENT(IN) :: convert
    REAL(r8), OPTIONAL, INTENT(IN) :: mult

    CALL write_2d_float_r8(h, id, name, units, dims, stagger, mesh_id, &
      variable, distribution%MPI_VAL, subarray%MPI_VAL, convert, mult)

  END SUBROUTINE write_2d_float_r8_f08



  SUBROUTINE write_3d_float_r8_f08(h, id, name, units, dims, stagger, mesh_id, &
      variable, distribution, subarray, convert, mult)

    TYPE(sdf_file_handle) :: h
    CHARACTER(LEN=*), INTENT(IN) :: id, name, units
    INTEGER, DIMENSION(:), INTENT(IN) :: dims
    INTEGER(i4), INTENT(IN) :: stagger
    CHARACTER(LEN=*), INTENT(IN) :: mesh_id
    REAL(r8), DIMENSION(:,:,:), INTENT(IN) :: variable
    TYPE(MPI_DATATYPE), INTENT(IN) :: distribution, subarray
    LOGICAL, OPTIONAL, INTENT(IN) :: convert
    REAL(r8), OPTIONAL, INTENT(IN) :: mult

    CALL write_3d_float_r8(h, id, name, units, dims, stagger, mesh_id, &
      variable, distribution%MPI_VAL, subarray%MPI_VAL, convert, mult)

  END SUBROUTINE write_3d_float_r8_f08

END MODULE sdf_f08
