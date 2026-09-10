!=====================================================================
!
!               S p e c f e m 3 D  V e r s i o n  3 . 0
!               ---------------------------------------
!
!     Main historical authors: Dimitri Komatitsch and Jeroen Tromp
!                        Princeton University, USA
!                and CNRS / University of Marseille, France
!                 (there are currently many more authors!)
! (c) Princeton University and CNRS / University of Marseille, July 2012
!
! This program is free software; you can redistribute it and/or modify
! it under the terms of the GNU General Public License as published by
! the Free Software Foundation; either version 2 of the License, or
! (at your option) any later version.
!
! This program is distributed in the hope that it will be useful,
! but WITHOUT ANY WARRANTY; without even the implied warranty of
! MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
! GNU General Public License for more details.
!
! You should have received a copy of the GNU General Public License along
! with this program; if not, write to the Free Software Foundation, Inc.,
! 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
!
!=====================================================================

! for elastic solver

! apply 1-D tractions on the bounaries

  subroutine compute_imposed_forces_elastic(NSPEC_AB,NGLOB_AB,SINGLE_FORCE_ENZ,accel, &
                        ibool,iphase, &
                        abs_boundary_normal,abs_boundary_jacobian2Dw, &
                        abs_boundary_ijk,abs_boundary_ispec, &
                        num_abs_boundary_faces,veloc,rho_vp,rho_vs, &
                        ispec_is_elastic,SIMULATION_TYPE,SAVE_FORWARD, &
                        it,deltat,myrank,b_num_abs_boundary_faces,b_reclen_field,&
                        b_absorb_field)

  use constants
  !###################

!#ifdef DEBUG_COUPLED
!    include "../../../add_to_compute_stacey_viscoelastic_9.F90"
!#endif

  implicit none

  integer :: NSPEC_AB,NGLOB_AB,myrank,SINGLE_FORCE_ENZ

! acceleration
  real(kind=CUSTOM_REAL), dimension(NDIM,NGLOB_AB) :: accel
  integer, dimension(NGLLX,NGLLY,NGLLZ,NSPEC_AB) :: ibool

! communication overlap
  integer :: iphase

! Stacey conditions
  real(kind=CUSTOM_REAL), dimension(NDIM,NGLOB_AB) :: veloc
  real(kind=CUSTOM_REAL), dimension(NGLLX,NGLLY,NGLLZ,NSPEC_AB) :: rho_vp,rho_vs

  logical, dimension(NSPEC_AB) :: ispec_is_elastic

  real(kind=CUSTOM_REAL) ::deltat

! absorbing boundary surface
  integer :: num_abs_boundary_faces
  real(kind=CUSTOM_REAL) :: abs_boundary_normal(NDIM,NGLLSQUARE,num_abs_boundary_faces)
  real(kind=CUSTOM_REAL) :: abs_boundary_jacobian2Dw(NGLLSQUARE,num_abs_boundary_faces)
  integer :: abs_boundary_ijk(3,NGLLSQUARE,num_abs_boundary_faces)
  integer :: abs_boundary_ispec(num_abs_boundary_faces)

! adjoint simulations
  integer:: SIMULATION_TYPE
  integer:: it
  integer:: b_num_abs_boundary_faces,b_reclen_field
  real(kind=CUSTOM_REAL),dimension(NDIM,NGLLSQUARE,b_num_abs_boundary_faces):: b_absorb_field

  logical:: SAVE_FORWARD

! local parameters
  real(kind=CUSTOM_REAL)::vx_scattering,vy_scattering,vz_scattering,vn_scattering
  real(kind=CUSTOM_REAL) ::nx,ny,nz,tx,ty,tz,jacobianw


  real(kind=CUSTOM_REAL) ,dimension(3)::velo1D

  integer :: ispec,iglob,i,j,k,iface,igll
  real(kind=CUSTOM_REAL),dimension(3)::traction_imposed
  real(kind=CUSTOM_REAL) :: time
  
  real(kind=CUSTOM_REAL) ::real_tmp
  integer :: int_tmp
  logical :: logical_tmp

  !The below lines are used to avoid possible error reports during compling the code.  
  !These parameters are not used now, that occationally causes error 
  !reports. But they might be usful in the future, so we keep them here.
  int_tmp=SIMULATION_TYPE
  logical_tmp=SAVE_FORWARD
  real_tmp=b_reclen_field;real_tmp=b_absorb_field(1,1,1)

!#ifdef DEBUG_COUPLED
!    include "../../../add_to_compute_stacey_viscoelastic_1.F90"
!#endif

  ! only add these contributions in first pass
  if (iphase /= 1) return

  ! checks if anything to do
  if (num_abs_boundary_faces == 0) return

  time=(it-1)*deltat

  ! absorbs absorbing-boundary surface using Stacey condition (Clayton and Enquist)
  do iface = 1,num_abs_boundary_faces

    ispec = abs_boundary_ispec(iface)

    if (ispec_is_elastic(ispec)) then

      ! reference GLL points on boundary face
      do igll = 1,NGLLSQUARE
        ! gets local indices for GLL point
        i = abs_boundary_ijk(1,igll,iface)
        j = abs_boundary_ijk(2,igll,iface)
        k = abs_boundary_ijk(3,igll,iface)

        iglob = ibool(i,j,k,ispec)
        ! gets associated normal
        nx = abs_boundary_normal(1,igll,iface)
        ny = abs_boundary_normal(2,igll,iface)
        nz = abs_boundary_normal(3,igll,iface)

        call get_1Dvelo(myrank,SINGLE_FORCE_ENZ,velo1D,igll,iface,time)
        ! gets traction
        call get_1Dtraction(myrank,SINGLE_FORCE_ENZ,traction_imposed,nx,ny,nz,igll,iface,time)


!#ifdef DEBUG_COUPLED
!  include "../../../add_to_compute_stacey_viscoelastic_2.F90"
!#endif

        ! gets sacattering wave velocity
        vx_scattering=veloc(1,iglob)-velo1D(1)
        vy_scattering=veloc(2,iglob)-velo1D(2)
        vz_scattering=veloc(3,iglob)-velo1D(3)

        ! velocity component in normal direction (normal points out of element)
        vn_scattering = vx_scattering*nx + vy_scattering*ny &
                      + vz_scattering*nz


        ! stacey term of scattering waves: velocity vector component * vp * rho in 
        !normal direction + vs * rho component tangential to it
        tx = rho_vp(i,j,k,ispec)*vn_scattering*nx + &
             rho_vs(i,j,k,ispec)*(vx_scattering-vn_scattering*nx)
        ty = rho_vp(i,j,k,ispec)*vn_scattering*ny + &
             rho_vs(i,j,k,ispec)*(vy_scattering-vn_scattering*ny)
        tz = rho_vp(i,j,k,ispec)*vn_scattering*nz + &
             rho_vs(i,j,k,ispec)*(vz_scattering-vn_scattering*nz)


!#ifdef DEBUG_COUPLED
!  include "../../../add_to_compute_stacey_viscoelastic_3.F90"
!#endif

        ! gets associated, weighted jacobian
        jacobianw = abs_boundary_jacobian2Dw(igll,iface)

        ! adds stacey term (weak form)
        accel(1,iglob) = accel(1,iglob) + (traction_imposed(1)-tx)*jacobianw
        accel(2,iglob) = accel(2,iglob) + (traction_imposed(2)-ty)*jacobianw
        accel(3,iglob) = accel(3,iglob) + (traction_imposed(3)-tz)*jacobianw

        ! adjoint simulations
!        if (SIMULATION_TYPE == 1 .and. SAVE_FORWARD) then
!          b_absorb_field(1,igll,iface) = tx*jacobianw
!          b_absorb_field(2,igll,iface) = ty*jacobianw
!          b_absorb_field(3,igll,iface) = tz*jacobianw
!        endif !adjoint

!#ifdef DEBUG_COUPLED
!  include "../../../add_to_compute_stacey_viscoelastic_4.F90"
!#endif

      enddo
    endif ! ispec_is_elastic
  enddo

  ! adjoint simulations: stores absorbed wavefield part
!  if (SIMULATION_TYPE == 1 .and. SAVE_FORWARD) then
    ! writes out absorbing boundary value
!    call write_abs(IOABS,b_absorb_field,b_reclen_field,it)
!  endif

!#ifdef DEBUG_COUPLED
!    include "../../../add_to_compute_stacey_viscoelastic_5.F90"
!#endif

  end subroutine compute_imposed_forces_elastic


!
!=====================================================================
!

! for elastic solver on GPU

! absorbing boundary term for elastic media (Stacey conditions)

  subroutine compute_imposed_forces_elastic_GPU(iphase,num_abs_boundary_faces, &
                        SIMULATION_TYPE,SAVE_FORWARD,NSTEP,it, &
                        b_num_abs_boundary_faces,b_reclen_field,b_absorb_field, &
                        Mesh_pointer)

  use constants

!#ifdef DEBUG_COUPLED
!    include "../../../add_to_compute_stacey_viscoelastic_10.F90"
!#endif

  implicit none

! communication overlap
  integer :: iphase

! absorbing boundary surface
  integer :: num_abs_boundary_faces

! adjoint simulations
  integer:: SIMULATION_TYPE
  integer:: NSTEP,it
  integer:: b_num_abs_boundary_faces,b_reclen_field
  real(kind=CUSTOM_REAL),dimension(NDIM,NGLLSQUARE,b_num_abs_boundary_faces):: b_absorb_field

  logical:: SAVE_FORWARD

  ! GPU_MODE variables
  integer(kind=8) :: Mesh_pointer

  real(kind=CUSTOM_REAL) ::real_tmp
  integer :: int_tmp
  logical :: logical_tmp
  integer(kind=8) ::tmp_Mesh

  !The below lines are used to avoid possible error reports during compling the code.  
  !These parameters are not used now, that occationally causes error 
  !reports. But they might be usful in the future, so we keep them here.
  int_tmp=SIMULATION_TYPE
  logical_tmp=SAVE_FORWARD
  int_tmp=b_reclen_field; real_tmp=b_absorb_field(1,1,1)
  tmp_Mesh=Mesh_pointer
  int_tmp=NSTEP; int_tmp=it

!#ifdef DEBUG_COUPLED
!    include "../../../add_to_compute_stacey_viscoelastic_7.F90"
!#endif

  ! only add these contributions in first pass
  if (iphase /= 1) return

  ! checks if anything to do
  if (num_abs_boundary_faces == 0) return

! adjoint simulations:
!  if (SIMULATION_TYPE == 3) then
    ! reads in absorbing boundary array (when first phase is running)
    ! note: the index NSTEP-it+1 is valid if b_displ is read in after the Newmark scheme
!    call read_abs(IOABS,b_absorb_field,b_reclen_field,NSTEP-it+1)
!  endif !adjoint

!  call compute_imposed_forces_elastic_cuda(Mesh_pointer,iphase,b_absorb_field)

  ! adjoint simulations: stores absorbed wavefield part
!  if (SIMULATION_TYPE == 1 .and. SAVE_FORWARD) then
    ! writes out absorbing boundary value
!    call write_abs(IOABS,b_absorb_field,b_reclen_field,it)
!  endif

!#ifdef DEBUG_COUPLED
!    include "../../../add_to_compute_stacey_viscoelastic_8.F90"
!#endif

  end subroutine compute_imposed_forces_elastic_GPU

!#ifdef DEBUG_COUPLED
!    include "../../../add_to_compute_stacey_viscoelastic_11.F90"
!#endif
