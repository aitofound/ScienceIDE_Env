! Copyright (C) 2009-2019 University of Warwick
!
! This program is free software: you can redistribute it and/or modify
! it under the terms of the GNU General Public License as published by
! the Free Software Foundation, either version 3 of the License, or
! (at your option) any later version.
!
! This program is distributed in the hope that it will be useful,
! but WITHOUT ANY WARRANTY; without even the implied warranty of
! MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
! GNU General Public License for more details.
!
! You should have received a copy of the GNU General Public License
! along with this program.  If not, see <http://www.gnu.org/licenses/>.

MODULE secondary_list

  USE boundary

  IMPLICIT NONE

CONTAINS

  SUBROUTINE reorder_particles_to_grid

    INTEGER :: ispecies, ix
    INTEGER :: cell_x
    TYPE(particle), POINTER :: current, next
    INTEGER(i8) :: local_count
    INTEGER :: i0, i1

    i0 = 1 - ng
    IF (use_field_ionisation) i0 = -ng
    i1 = 1 - i0

    DO ispecies = 1, n_species

      ! Skip species which are not involved in collisions or collisional
      ! ionisation
      IF (.NOT. species_list(ispecies)%make_secondary_list) CYCLE
      species_list(ispecies)%is_shuffled = .FALSE.

      local_count = species_list(ispecies)%attached_list%count
      CALL MPI_ALLREDUCE(local_count, species_list(ispecies)%global_count, &
          1, MPI_INTEGER8, MPI_SUM, comm, errcode)
      ALLOCATE(species_list(ispecies)%secondary_list(i0:nx+i1))
      DO ix = i0, nx + i1
        CALL create_empty_partlist(&
            species_list(ispecies)%secondary_list(ix))
      END DO
      current => species_list(ispecies)%attached_list%head
      DO WHILE(ASSOCIATED(current))
        next => current%next
#ifdef PARTICLE_SHAPE_TOPHAT
        cell_x = FLOOR((current%part_pos - x_grid_min_local) / dx) + 1
#else
        cell_x = FLOOR((current%part_pos - x_grid_min_local) / dx + 1.5_num)
#endif

        CALL remove_particle_from_partlist(&
            species_list(ispecies)%attached_list, current)
        CALL add_particle_to_partlist(&
            species_list(ispecies)%secondary_list(cell_x), current)
        current => next
      END DO
    END DO

  END SUBROUTINE reorder_particles_to_grid



  SUBROUTINE reattach_particles_to_mainlist

    INTEGER :: ispecies, ix
    INTEGER :: i0, i1

    i0 = 1 - ng
    IF (use_field_ionisation) i0 = -ng
    i1 = 1 - i0

    DO ispecies = 1, n_species
      ! Skip species which did not make a secondary list
      IF (.NOT. species_list(ispecies)%make_secondary_list) CYCLE

      DO ix = i0, nx + i1
        CALL append_partlist(species_list(ispecies)%attached_list, &
            species_list(ispecies)%secondary_list(ix))
      END DO
      DEALLOCATE(species_list(ispecies)%secondary_list)
    END DO

    CALL setup_bc_lists
    CALL particle_bcs

  END SUBROUTINE reattach_particles_to_mainlist

END MODULE secondary_list
