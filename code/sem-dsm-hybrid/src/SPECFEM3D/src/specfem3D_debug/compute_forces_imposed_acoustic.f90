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

! for acoustic solver

  subroutine compute_imposed_forces_acoustic(NSPEC_AB,NGLOB_AB,SINGLE_FORCE_ENZ, &
                            potential_dot_dot_acoustic,potential_dot_acoustic, &
                            ibool,iphase,abs_boundary_normal, &
                            abs_boundary_jacobian2Dw,abs_boundary_ijk,abs_boundary_ispec, &
                            num_abs_boundary_faces,rhostore,kappastore,ispec_is_acoustic, &
                            SIMULATION_TYPE,SAVE_FORWARD,it,deltat,myrank,b_reclen_potential, &
                            b_absorb_potential,b_num_abs_boundary_faces)

  use constants

!to be canceled
  use specfem_par, only:xstore,ystore,zstore

  implicit none

  integer :: NSPEC_AB,NGLOB_AB,SINGLE_FORCE_ENZ,myrank

! potentials
  real(kind=CUSTOM_REAL), dimension(NGLOB_AB) :: potential_dot_dot_acoustic, &
                                                 potential_dot_acoustic
  integer, dimension(NGLLX,NGLLY,NGLLZ,NSPEC_AB) :: ibool

! communication overlap
  integer :: iphase

  real(kind=CUSTOM_REAL), dimension(NGLLX,NGLLY,NGLLZ,NSPEC_AB) :: rhostore,kappastore
  logical, dimension(NSPEC_AB) :: ispec_is_acoustic

  real(kind=CUSTOM_REAL) ::deltat
! absorbing boundary surface
  integer :: num_abs_boundary_faces
  real(kind=CUSTOM_REAL) :: abs_boundary_normal(NDIM,NGLLSQUARE,num_abs_boundary_faces)
  real(kind=CUSTOM_REAL) :: abs_boundary_jacobian2Dw(NGLLSQUARE,num_abs_boundary_faces)
  integer :: abs_boundary_ijk(3,NGLLSQUARE,num_abs_boundary_faces)
  integer :: abs_boundary_ispec(num_abs_boundary_faces)

! adjoint simulations
  integer:: SIMULATION_TYPE,it
  integer:: b_num_abs_boundary_faces,b_reclen_potential
  real(kind=CUSTOM_REAL),dimension(NGLLSQUARE,b_num_abs_boundary_faces):: b_absorb_potential
  logical:: SAVE_FORWARD

! local parameters
  real(kind=CUSTOM_REAL) :: rhol,cpl,jacobianw,absorbl
  real(kind=CUSTOM_REAL) :: pressure,chi_dot0
  real(kind=CUSTOM_REAL),dimension(3):: displacement
  real(kind=CUSTOM_REAL) :: displ_n
  real(kind=CUSTOM_REAL) ::nx,ny,nz
  integer :: ispec,iglob,i,j,k,iface,igll
  real(kind=CUSTOM_REAL) :: time

  !integer:: reclen1,reclen2

  ! only add these contributions in first pass
  if (iphase /= 1) return

  ! checks if anything to do
  if (num_abs_boundary_faces == 0) return

  time=(it-1)*deltat

  ! absorbs absorbing-boundary surface using Sommerfeld condition (vanishing field in the outer-space)
  do iface = 1,num_abs_boundary_faces

    ispec = abs_boundary_ispec(iface)

    if (ispec_is_acoustic(ispec)) then

      ! reference GLL points on boundary face
      do igll = 1,NGLLSQUARE
        ! gets local indices for GLL point
        i = abs_boundary_ijk(1,igll,iface)
        j = abs_boundary_ijk(2,igll,iface)
        k = abs_boundary_ijk(3,igll,iface)

        ! gets global index
        iglob=ibool(i,j,k,ispec)

        ! gets associated normal
        nx = abs_boundary_normal(1,igll,iface)
        ny = abs_boundary_normal(2,igll,iface)
        nz = abs_boundary_normal(3,igll,iface)

        ! determines bulk sound speed
        rhol = rhostore(i,j,k,ispec)
        cpl = sqrt( kappastore(i,j,k,ispec) / rhol )

        ! gets associated, weighted jacobian
        jacobianw = abs_boundary_jacobian2Dw(igll,iface)

!!!!pressure = - chi_dot_dot
!problem: -pressure or pressure?? Answer: should be positive
!          call get_1Dpressure(myrank,pressure,igll,iface,time)
        call get_1Dpotential_dot(myrank,SINGLE_FORCE_ENZ,chi_dot0,igll,iface,time)
        call get_1Ddisp_fluid(myrank,SINGLE_FORCE_ENZ,displacement,igll,iface,time)

        displ_n=displacement(1)*nx+displacement(2)*ny+displacement(3)*nz

!        if(myrank.eq.0.and.igll.eq.1.and.iface.eq.36) write(*,*) &
!                        'check_dep21_dispnacous',displacement(:),nx,ny,nz
!        if(myrank.eq.0.and.igll.eq.23.and.iface.eq.36) write(*,*) &
!                        'check_dep17_dispnacous',displacement(:),nx,ny,nz
!        if(myrank.eq.0.and.igll.eq.12.and.iface.eq.48) write(*,*) &
!                        'check_dep11_dispnacous',displacement(:),nx,ny,nz
!        if(myrank.eq.0.and.igll.eq.21.and.iface.eq.60) write(*,*) &
!                        'check_dep1_dispnacous',displacement(:),nx,ny,nz
!        if(myrank.eq.3.and.igll.eq.1.and.iface.eq.157) write(*,*) &
!                        'check_chidot',chi_dot0,potential_dot_acoustic(iglob),&
!                        6371000.0-sqrt(xstore(iglob)**2+ystore(iglob)**2+zstore(iglob)**2)
!          if(dabs(chi_dot0-potential_dot_acoustic(iglob)).gt.8.e-5) write(*,*)
!          &
!                          'Error_chidot',chi_dot0,potential_dot_acoustic(iglob)
!        if(it.eq.1.and.ny<-0.6)  write(*,*) &
!                        'find_left_edges',myrank,igll,iface,nx,ny,nz,&
!                         6371000.0-sqrt(xstore(iglob)**2+ystore(iglob)**2+zstore(iglob)**2)
!          if(it.eq.1.and.nx>0.6.and.sqrt(xstore(iglob)**2+ystore(iglob)**2+zstore(iglob)**2)<1217.09)
!          write(*,*) &
!        if(it.eq.1.and.dabs(nx)>0.6) write(*,*) &
!                        'Error_detected',myrank,igll,iface,nx,ny,nz,&
!                         6371000.0-sqrt(xstore(iglob)**2+ystore(iglob)**2+zstore(iglob)**2)

        potential_dot_dot_acoustic(iglob) = potential_dot_dot_acoustic(iglob) &
                          +jacobianw*displ_n &
                          -(potential_dot_acoustic(iglob)-chi_dot0) * jacobianw / cpl / rhol
!        potential_dot_dot_acoustic(iglob) = potential_dot_dot_acoustic(iglob) &
!                          +jacobianw*displ_n 

!       potential_dot_dot_acoustic(iglob) = potential_dot_dot_acoustic(iglob) &
!                   -(potential_dot_acoustic(iglob)) * jacobianw / cpl / rhol

        ! adjoint simulations
!        if (SIMULATION_TYPE == 1 .and. SAVE_FORWARD) then
!          b_absorb_potential(igll,iface) = absorbl
!        endif !adjoint

       enddo
    endif ! ispec_is_acoustic
  enddo ! num_abs_boundary_faces

  ! adjoint simulations: stores absorbed wavefield part
 ! if (SIMULATION_TYPE == 1 .and. SAVE_FORWARD) then
    ! writes out absorbing boundary value
 !   call write_abs(IOABS_AC,b_absorb_potential,b_reclen_potential,it)
 ! endif

  end subroutine compute_imposed_forces_acoustic
!
!
!=====================================================================
! for acoustic solver on GPU

  subroutine compute_imposed_forces_acoustic_GPU(iphase,num_abs_boundary_faces, &
                            SIMULATION_TYPE,SAVE_FORWARD,it, &
                            b_reclen_potential,b_absorb_potential, &
                            b_num_abs_boundary_faces,Mesh_pointer)

  use constants

  implicit none

! potentials

! communication overlap
  integer :: iphase

! absorbing boundary surface
  integer :: num_abs_boundary_faces

! adjoint simulations
  integer:: SIMULATION_TYPE
  integer:: NSTEP,it
  integer:: b_num_abs_boundary_faces,b_reclen_potential
  real(kind=CUSTOM_REAL),dimension(NGLLSQUARE,b_num_abs_boundary_faces):: b_absorb_potential
  logical:: SAVE_FORWARD

  ! GPU_MODE variables
  integer(kind=8) :: Mesh_pointer

  ! only add these contributions in first pass
  if (iphase /= 1) return

  ! checks if anything to do
  if (num_abs_boundary_faces == 0) return

  ! adjoint simulations:
  if (SIMULATION_TYPE == 3) then
    ! reads in absorbing boundary array (when first phase is running)
    ! note: the index NSTEP-it+1 is valid if b_displ is read in after the Newmark scheme
!    call read_abs(IOABS_AC,b_absorb_potential,b_reclen_potential,NSTEP-it+1)
  endif !adjoint

  ! absorbs absorbing-boundary surface using Sommerfeld condition (vanishing field in the outer-space)
!  call compute_imposed_acoustic_cuda(Mesh_pointer,iphase,b_absorb_potential)

  ! adjoint simulations: stores absorbed wavefield part
  if (SIMULATION_TYPE == 1 .and. SAVE_FORWARD) then
    ! writes out absorbing boundary value
!    call write_abs(IOABS_AC,b_absorb_potential,b_reclen_potential,it)
  endif

  end subroutine compute_imposed_forces_acoustic_GPU
