!  Copyright (C) 2002 Regents of the University of Michigan, 
!  portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module PIC_ModField
  use PC_ModSize, ONLY: nX, nY, nZ, x_, y_, z_, &
       nDim, nCell_D, MaxBlock, jDim_, kDim_, nPType
  use PC_BATL_size,ONLY: MaxDim, nBlock
  use PIC_ModMain,ONLY:&
       SpeedOfLight_D,&     !This is c\Delta t/\Delta x,...
       UseVectorPotential   !If we use vector-potential, or not
  use PIC_ModMain, ONLY: Dx_D, IsPeriodicField_D, TypeFieldBc_S
  use PIC_ModFormFactor, ONLY: lOrderFF
  implicit none

  SAVE

  !Introduce the number of ghostcells (iGCN)
  integer,parameter:: iGCN=&
       1 &    !because even with the first order formfactor the 
              !particle can escape the domain within a time step
       +lOrderFF/2 
  !because for lOrderFF>1 the particle form-factor
  !is wider than a mesh size

  !For fields the number of ghostcells may be less
  integer, parameter:: nGF = (1 + lOrderFF)/2

  !Structures
  !Array index is the coordinate of the gridpoint with +1/2 being
  !approximated as 1
  !                                                Ez (:,:,nZ+2,1)    
  !       _!_!x!x               _!_!_!_  Not used: Ex (nX+2,:,:)
  !       _!_!x!x               _!_!_!_            Ey (:,nY+2,:,1)
  !       _!_!_!                x!x!_!_  Bz(:,nY+2,:,1),Bz(nX+2,:,:,1)       
  !    -2  ! ! !                x!x!_!_  Bx(:,nY+2,:,1),Bx(:,:,nZ+2,1)
  !       -2                             By(nX+2,:,:,1),By(:,:,nZ+2,1)
  real,allocatable:: Aux_CB(:,:,:,:)
  real,allocatable:: E_GDB(:,:,:,:,:)
  real,allocatable:: B_GDB(:,:,:,:,:)
  real,allocatable:: Current_GDB(:,:,:,:,:)

  real,allocatable:: State_VGBI(:,:,:,:,:,:) 
  real :: B0_D(3) = 0.0

  real,allocatable:: A_GDB(:,:,:,:,:)
  !Methods
  public::update_magnetic!Updates the magnetic field, or vector potential
  public::update_e       !Updates the electric field     
contains
  !=======================
  subroutine allocate_fields
    
    allocate(E_GDB(1-nGF:nX+nGF, 1-nGF*jDim_:nY+nGF*jDim_,&
          1-nGF*kDim_:nZ+nGF*kDim_,MaxDim, MaxBlock))
    E_GDB = 0.0
    allocate(B_GDB(1-nGF:nX+nGF, 1-nGF*jDim_:nY+nGF*jDim_,&
         1-nGF*kDim_:nZ+nGF*kDim_, MaxDim, MaxBlock))
    B_GDB = 0.0 
    allocate(Current_GDB(1-iGCN:nX+iGCN, 1-iGCN*jDim_:nY+iGCN*jDim_,&
         1-iGCN*kDim_:nZ+iGCN*kDim_, MaxDim, MaxBlock)) 
    Current_GDB = 0.0 
    allocate(State_VGBI(10,1-nGF:nX+nGF, 1-nGF*jDim_:nY+nGF*jDim_,&
         1-nGF*kDim_:nZ+nGF*kDim_, MaxBlock,nPType)) 
    State_VGBI = 0.0

    allocate(Aux_CB(1:nX, 1:nY,1:nZ, MaxBlock)); Aux_CB = 0.0 
    if(UseVectorPotential)then
       allocate(A_GDB(1-nGF:nX+nGF, 1-nGF*jDim_:nY+nGF*jDim_,&
         1-nGF*kDim_:nZ+nGF*kDim_, MaxDim, MaxBlock))
       A_GDB = 0.0
    end if
  end subroutine allocate_fields
  !================
  subroutine add_e
    use ModReadParam, ONLY: read_var
    real:: E_D(3)
    integer:: i,j,k,iDim, iBlock
    !------------
    call read_var('Ex',E_D(1))
    call read_var('Ey',E_D(2))
    call read_var('Ez',E_D(3))
    do iBlock = 1, MaxBlock
       do iDim = 1,3
          do k=1-nGF*kDim_,nZ+nGF*kDim_
             do j=1-nGF*jDim_,nY+nGF*jDim_
                do i=1-nGF,nX+nGF
                   E_GDB(i,j,k,iDim,iBlock) = &
                        E_GDB(i,j,k,iDim,iBlock) + E_D(iDim)
                end do
             end do
          end do
       end do
    end do
  end subroutine add_e
  !===================
  subroutine add_b
    use ModReadParam, ONLY: read_var
    integer:: i,j,k,iDim, iBlock
    !------------
    call read_var('Bx',B0_D(1))
    call read_var('By',B0_D(2))
    call read_var('Bz',B0_D(3))
    do iBlock = 1, MaxBlock
       do iDim = 1,3
          do k=1-nGF*kDim_,nZ+nGF*kDim_
             do j=1-nGF*jDim_,nY+nGF*jDim_
                do i=1-nGF,nX+nGF
                   B_GDB(i,j,k,iDim,iBlock) = &
                        B_GDB(i,j,k,iDim,iBlock) + B0_D(iDim)
                end do
             end do
          end do
       end do
    end do
  end subroutine add_b
  !===================
  !Transforms the vector potential to magn. field
  subroutine get_b_from_a(iBlock)
    integer, intent(in)::iBlock
    integer::i,j,k
    !Applied only if UseVectorPotential
    !The vector potential is in A_GDB
    !Calculates the magnetic field 

    !The whole magnetic field is calculated and 
    !the result is saved to B_GDB
    !---------------------------------
    !Array index is the coordinate of the gridpoint with +1/2 being
    !approximated as 1
    !                                                Ez (:,:,nZ+2,1)    
    !       _!_!x!x               _!_!_!_  Not used: Ex (nX+2,:,:,1)
    !       _!_!x!x               _!_!_!_            Ey (:,nY+2,:,1)
    !       _!_!_!                x!x!_!_  Bz(:,nY+2,:,1),Bz(nX+2,:,:,1)       
    !    -2  ! ! !                x!x!_!_  Bx(:,nY+2,:,1),Bx(:,:,nZ+2,1)
    !       -2                             By(nX+2,:,:,1),By(:,:,nZ+2,1)
    do k=1-nGF*kDim_,nZ+(nGF-1)*kDim_ 
       do j=1-nGF*jDim_,nY+(nGF-1)*jDim_
          do i=1-nGF,nX+nGF
             B_GDB(i,j,k,x_,iBlock) = B0_D(x_) + &
                  SpeedOfLight_D(y_)*&
                  (A_GDB(i,j+jDim_,k,z_,iBlock) - A_GDB(i,j,k,z_,iBlock))
             if(nDim==3)B_GDB(i,j,k,x_,iBlock) = B_GDB(i,j,k,x_,iBlock)-&
                  SpeedOfLight_D(z_)*&
                  (A_GDB(i,j,k+kDim_,y_,iBlock) - A_GDB(i,j,k,y_,iBlock)) 
          end do
       end do
    end do

    !Array index is the coordinate of the gridpoint with +1/2 being
    !approximated as 1
    !                                                Ez (:,:,nZ+2,1)    
    !       _!_!x!x               _!_!_!_  Not used: Ex (nX+2,:,:,1)
    !       _!_!x!x               _!_!_!_            Ey (:,nY+2,:,1)
    !       _!_!_!                x!x!_!_  Bz(:,nY+2,:,1),Bz(nX+2,:,:,1)       
    !    -2  ! ! !                x!x!_!_  Bx(:,nY+2,:,1),Bx(:,:,nZ+2,1)
    !       -2                             By(nX+2,:,:,1),By(:,:,nZ+2,1)
    do k=1-nGF*kDim_,nZ+(nGF-1)*kDim_ 
       do j=1-nGF*jDim_,nY+nGF*jDim_ 
          do i=1-nGF,nX+nGF-1
             B_GDB(i,j,k,y_,iBlock) = B0_D(y_) - &
                  SpeedOfLight_D(x_)*&
                  (A_GDB(i+1,j,k,z_,iBlock) - A_GDB(i,j,k,z_,iBlock))
             if(nDim==3)B_GDB(i,j,k,y_,iBlock) = B_GDB(i,j,k,y_,iBlock) +&
                  SpeedOfLight_D(z_)*&
                  (A_GDB(i,j,k+kDim_,x_,iBlock) - A_GDB(i,j,k,x_,iBlock))
          end do
       end do
    end do

    !Array index is the coordinate of the gridpoint with +1/2 being
    !approximated as 1
    !                                                Ez (:,:,nZ+2,1)    
    !       _!_!x!x               _!_!_!_  Not used: Ex (nX+2,:,:,1)
    !       _!_!x!x               _!_!_!_            Ey (:,nY+2,:,1)
    !       _!_!_!                x!x!_!_  Bz(:,nY+2,:,1),Bz(nX+2,:,:,1)       
    !    -2  ! ! !                x!x!_!_  Bx(:,nY+2,:,1),Bx(:,:,nZ+2,1)
    !       -2                             By(nX+2,:,:,1),By(:,:,nZ+2,1)
    do k=1-nGF*kDim_,nZ+nGF*kDim_
       do j=1-nGF*jDim_,nY+(nGF-1)*jDim_
          do i=1-nGF,nX+nGF-1
             B_GDB(i,j,k,z_,iBlock) = &
                  SpeedOfLight_D(x_)*&
                  (A_GDB(i+1,j,k,y_,iBlock) - A_GDB(i,j,k,y_,iBlock))-&
                  SpeedOfLight_D(y_)*& 
                  (A_GDB(i,j+jDim_,k,x_,iBlock) - A_GDB(i,j,k,x_,iBlock)) +&
                  B0_D(z_)
          end do
       end do
    end do
  end subroutine get_b_from_a
  !==========================
  subroutine update_magnetic
    integer::i,j,k,iBlock
    real,dimension(MaxDim):: SpeedOfLightHalf_D
    !--------------------------
    if(UseVectorPotential)then
       !Advance vector potential throught a half timestep
       !Array index is the coordinate of the gridpoint with +1/2 being
       !approximated as 1
       !                                                Ez (:,:,nZ+2,1)    
       !       _!_!x!x               _!_!_!_  Not used: Ex (nX+2,:,:,1)
       !       _!_!x!x               _!_!_!_            Ey (:,nY+2,:,1)
       !       _!_!_!                x!x!_!_  Bz(:,nY+2,:,1),Bz(nX+2,:,:,1)       
       !    -2  ! ! !                x!x!_!_  Bx(:,nY+2,:,1),Bx(:,:,nZ+2,1)
       !       -2                             By(nX+2,:,:,1),By(:,:,nZ+2,1)
       do iBlock = 1, nBlock
          A_GDB(     1-nGF:nX+nGF-1,1-nGF*jDim_:nY+nGF*jDim_,&
               1-nGF*kDim_:nZ+nGF*kDim_,x_,iBlock) = &
               A_GDB(1-nGF:nX+nGF-1,1-nGF*jDim_:nY+nGF*jDim_,&
               1-nGF*kDim_:nZ+nGF*kDim_,x_,iBlock) - 0.50*&
               E_GDB(1-nGF:nX+nGF-1,1-nGF*jDim_:nY+nGF*jDim_,&
               1-nGF*kDim_:nZ+nGF*kDim_,x_,iBlock)
          
          A_GDB(     1-nGF:nX+nGF,1-nGF*jDim_:nY+(nGF-1)*jDim_,&
               1-nGF*kDim_:nZ+nGF*kDim_,y_,iBlock) = &
               A_GDB(1-nGF:nX+nGF,1-nGF*jDim_:nY+(nGF-1)*jDim_,&
               1-nGF*kDim_:nZ+nGF*kDim_,y_,iBlock) - 0.50*&
               E_GDB(1-nGF:nX+nGF,1-nGF*jDim_:nY+(nGF-1)*jDim_,&
               1-nGF*kDim_:nZ+nGF*kDim_,y_,iBlock)
          
          A_GDB(     1-nGF:nX+nGF, 1-nGF*jDim_:nY+nGF*jDim_,&
               1-nGF*kDim_:nZ+(nGF-1)*kDim_,z_,iBlock) = &
               A_GDB(1-nGF:nX+nGF, 1-nGF*jDim_:nY+nGF*jDim_,&
               1-nGF*kDim_:nZ+(nGF-1)*kDim_,z_,iBlock) - 0.50*&
               E_GDB(1-nGF:nX+nGF, 1-nGF*jDim_:nY+nGF*jDim_,&
               1-nGF*kDim_:nZ+(nGF-1)*kDim_,z_,iBlock)
          call get_b_from_a(iBlock)
       end do
       RETURN
    end if

    SpeedOfLightHalf_D = &
         SpeedOfLight_D*0.50

    !Advance field throught a half timestep
    !Array index is the coordinate of the gridpoint with +1/2 being
    !approximated as 1
    !                                                Ez (:,:,nZ+2,1)    
    !       _!_!x!x               _!_!_!_  Not used: Ex (nX+2,:,:,1)
    !       _!_!x!x               _!_!_!_            Ey (:,nY+2,:,1)
    !       _!_!_!                x!x!_!_  Bz(:,nY+2,:,1),Bz(nX+2,:,:,1)       
    !    -2  ! ! !                x!x!_!_  Bx(:,nY+2,:,1),Bx(:,:,nZ+2,1)
    !       -2                             By(nX+2,:,:,1),By(:,:,nZ+2,1)
    do iBlock = 1, nBlock
       do k=1-nGF*kDim_,nZ+(nGF-1)*kDim_ 
          do j=1-nGF*jDim_,nY+(nGF-1)*jDim_ 
             do i=1-nGF,nX+nGF
                B_GDB(i,j,k,x_,iBlock) = B_GDB(i,j,k,x_,iBlock) - &
                     SpeedOfLightHalf_D(y_)*&
                     (E_GDB(i,  j+1,k  ,z_,iBlock) - E_GDB(i,j,k,z_,iBlock)) 
                if(nDim==3)B_GDB(i,j,k,x_,iBlock) = B_GDB(i,j,k,x_,iBlock) + &
                     SpeedOfLightHalf_D(z_)*&
                     (E_GDB(i,  j,  k+1,y_,iBlock) - E_GDB(i,j,k,y_,iBlock))
             end do
          end do
       end do

       !Array index is the coordinate of the gridpoint with +1/2 being
       !approximated as 1
       !                                                Ez (:,:,nZ+2)    
       !       _!_!x!x               _!_!_!_  Not used: Ex (nX+2,:,:,1)
       !       _!_!x!x               _!_!_!_            Ey (:,nY+2,:,1)
       !       _!_!_!                x!x!_!_  Bz(:,nY+2,:,1),Bz(nX+2,:,:,1)     
       !    -2  ! ! !                x!x!_!_  Bx(:,nY+2,:,1),Bx(:,:,nZ+2,1)
       !       -2                             By(nX+2,:,:,1),By(:,:,nZ+2,1)
       do k=1-nGF*kDim_,nZ+(nGF-1)*kDim_ 
          do j=1-nGF*jDim_,nY+nGF*jDim_ 
             do i=1-nGF,nX+nGF-1
                B_GDB(i,j,k,y_,iBlock) = B_GDB(i,j,k,y_,iBlock) + &
                     SpeedOfLightHalf_D(x_)*&
                     (E_GDB(i+1,j,  k  ,z_,iBlock) - E_GDB(i,j,k,z_,iBlock))
                if(nDim==3)B_GDB(i,j,k,y_,iBlock) = B_GDB(i,j,k,y_,iBlock) - &
                     SpeedOfLightHalf_D(z_)*&
                     (E_GDB(i,  j,  k+1,x_,iBlock) - E_GDB(i,j,k,x_,iBlock))
             end do
          end do
       end do

       !Array index is the coordinate of the gridpoint with +1/2 being
       !approximated as 1
       !                                                Ez (:,:,nZ+2,1)    
       !       _!_!x!x               _!_!_!_  Not used: Ex (nX+2,:,:,1)
       !       _!_!x!x               _!_!_!_            Ey (:,nY+2,:,1)
       !       _!_!_!                x!x!_!_  Bz(:,nY+2,:,1),Bz(nX+2,:,:,1)     
       !    -2  ! ! !                x!x!_!_  Bx(:,nY+2,:,1),Bx(:,:,nZ+2,1)
       !       -2                             By(nX+2,:,:,1),By(:,:,nZ+2,1)
       do k=1-nGF*kDim_,nZ+nGF*kDim_ 
          do j=1-nGF*jDim_,nY+(nGF-1)*jDim_ 
             do i=1-nGF,nX+nGF-1
                B_GDB(i,j,k,z_,iBlock) = B_GDB(i,j,k,z_,iBlock) - &
                     SpeedOfLightHalf_D(x_)*&
                     (E_GDB(i+1,j,  k  ,y_,iBlock) - E_GDB(i,j,k,y_,iBlock)) + &
                     SpeedOfLightHalf_D(y_)*&
                     (E_GDB(i,  j+1,k  ,x_,iBlock) - E_GDB(i,j,k,x_,iBlock))
              end do
           end do
        end do
     end do
  end subroutine update_magnetic
  !-----------------------------------------------------------------!
  subroutine update_e
    integer::iBlock, i, j, k
    !---------------
    do iBlock = 1, nBlock
       do k=1,nZ
          do j=1,nY
             do i=0,nX
                E_GDB(i,j,k,x_,iBlock) = E_GDB(i,j,k,x_,iBlock) -&
                     Current_GDB(i,j,k,x_,iBlock) + &
                     SpeedOfLight_D(y_)*&
                     (B_GDB(i,j,k,z_,iBlock) - B_GDB(i,j-jDim_,k,z_,iBlock))
                if(nDim==3) E_GDB(i,j,k,x_,iBlock) = E_GDB(i,j,k,x_,iBlock) - &
                     SpeedOfLight_D(z_)*&
                     (B_GDB(i,j,k,y_,iBlock) - B_GDB(i,j,k-kDim_,y_,iBlock))
             end do
          end do
       end do
       do k=1,nZ
          do j=1-jDim_,nY
             do i=1,nX
                E_GDB(i,j,k,y_,iBlock) = E_GDB(i,j,k,y_,iBlock) -&
                     current_GDB(i,j,k,y_,iBlock)-&
                     SpeedOfLight_D(x_)*&
                     (B_GDB(i,j,k,z_,iBlock) - B_GDB(i-1,j,k,z_,iBlock))
                if(nDim==3)E_GDB(i,j,k,y_,iBlock) = E_GDB(i,j,k,y_,iBlock) + &
                     SpeedOfLight_D(z_)*&
                     (B_GDB(i,j,k,x_,iBlock) - B_GDB(i,j,k-kDim_,x_,iBlock))
             end do
          end do
       end do
       do k=1-kDim_,nZ
          do j=1,nY
             do i=1,nX
                E_GDB(i,j,k,z_,iBlock) = E_GDB(i,j,k,z_,iBlock) -&
                     Current_GDB(i,j,k,z_,iBlock) +&
                     SpeedOfLight_D(x_)*&
                     (B_GDB(i,j,k,y_,iBlock) - B_GDB(i-1,j,k,y_,iBlock))-&
                     SpeedOfLight_D(y_)*& 
                     (B_GDB(i,j,k,x_,iBlock) - B_GDB(i,j-jDim_,k,x_,iBlock))
             end do
          end do
       end do
    end do
  end subroutine update_e
  !=======================
  subroutine field_bc
    use PIC_ModBatlInterface, ONLY: NeiLevel_SB
    use PC_Batl_lib,          ONLY: Unset_
    use PC_BATL_grid,         ONLY: Xyz_DGB
    use PIC_ModLaserBeam,     ONLY: laser_beam, UseLaserBeam 
    integer :: i, j, k, iBlock
    real    :: x, y, z
    !---------------
    !\
    ! Along the directions at which the periodic boundary condition is used
    ! only the electric field at the faces inside the computation domain
    ! is calculated here, and the outer face values are filled in with the
    ! periodic boundary conditions. Along other directions the computational
    ! domain is extended by 1 layer of the gostcells and 
    ! the faces inside the extended domain are filled in here
    !/
    do iBlock = 1, nBlock
       if(NeiLevel_SB(1, iBlock)==Unset_)then
          ! Face X < 0    
          i = 0
          do k = 1, nZ
             do j=1-jDim_, nY
                E_GDB(i,j,k,y_,iBlock) = E_GDB(i,j,k,y_,iBlock)*&
                     (1 - SpeedOfLight_D(x_)) + &
                     SpeedOfLight_D(x_)*E_GDB(i+1,j,k,y_,iBlock)
                if(UseLaserBeam)&
                     call laser_beam(iDir=y_,    &
                     Xyz_D = 0.5* (Xyz_DGB(:,i,j,k,iBlock) + &
                     Xyz_DGB(:,i,j+jDim_,k,iBlock)), &
                     EField=E_GDB(i,j,k,y_,iBlock) )
             end do
          end do

          do k = 1 - kDim_, nZ
             do j = 1, nY
                E_GDB(i,j,k,z_,iBlock) = E_GDB(i,j,k,z_,iBlock)*&
                     (1 - SpeedOfLight_D(x_)) + &
                     SpeedOfLight_D(x_)*E_GDB(i+1,j,k,z_,iBlock)
                if(UseLaserBeam)&
                     call laser_beam(iDir=z_,    &
                     Xyz_D = 0.5* (Xyz_DGB(:,i,j,k,iBlock) + &
                     Xyz_DGB(:,i,j,k+kDim_,iBlock)),  &
                     EField=E_GDB(i,j,k,z_,iBlock) )
             end do
          end do
       end if
       if(NeiLevel_SB(2, iBlock)==Unset_)then
          ! Face X > 0    
          i = nX +1
          do k = 1, nZ
             do j = 1 - jDim_, nY
                E_GDB(i,j,k,y_,iBlock) = E_GDB(i,j,k,y_,iBlock)*&
                     (1 - SpeedOfLight_D(x_)) +&
                     SpeedOfLight_D(x_)* E_GDB(i-1,j,k,y_,iBlock)
             end do
          end do
          do k = 1 - kDim_, nZ
             do j = 1, nY
                E_GDB(i,j,k,z_,iBlock) = E_GDB(i,j,k,z_,iBlock)*&
                     (1 - SpeedOfLight_D(x_)) +&
                     SpeedOfLight_D(x_)*E_GDB(i-1,j,k,z_,iBlock)
             end do
          end do
       end if
    end do
  end subroutine field_bc
  !======================
  subroutine get_field_energy(Energy_V)
    !\
    ! Calculate the field energies, put the result onto the root PE
    !/
    use PIC_ModMain, ONLY: CellVolume, UseSharedField
    use PIC_ModProc
    use ModMpi
    real,intent(out)::Energy_V(6)
    integer::i,j,k,iBlock
    !-------------
    Energy_V = 0.0
    !\
    ! If all field information is available on the root PE
    ! the other PEs are not involved.
    !/
    if(UseSharedField.and.iProc/=0)RETURN
    do iBlock = 1, nBlock
       do k=1,nZ; do j=1,nY; do i=1,nX
          Energy_V(1) = Energy_V(1) + 0.250/(1 + kDim_)*&
               sum((B_GDB(i,j-jDim_:j,k-kDim_:k,x_,iBlock) - B0_D(x_))**2)
          
          Energy_V(2) = Energy_V(2) + 0.250/(1 + kDim_)*&
               sum((B_GDB(i-1:i,j,k-kDim_:k,y_,iBlock) - B0_D(y_))**2)
          
          Energy_V(3) = Energy_V(3) + 0.1250*&
               sum((B_GDB(i-1:i,j-1:j,k,z_,iBlock) - B0_D(z_))**2)
          
          
          Energy_V(4) = Energy_V(4) + 0.250*&
               sum(E_GDB(i-1:i,j,k,x_,iBlock)**2)
          
          Energy_V(5) = Energy_V(5) + 0.250*&
               sum(E_GDB(i,j-1:j,k,y_,iBlock)**2)
          
          Energy_V(6) = Energy_V(6) + 0.50/(1 + kDim_)*&
               sum(E_GDB(i,j,k-kDim_:k,z_,iBlock)**2)
       end do; end do; end do
    end do
    Energy_V = Energy_V*CellVolume
    if(UseSharedField)RETURN
    if(nProc==1)RETURN
    call mpi_reduce_real_array(Energy_V(1),6,MPI_SUM, 0, iComm, iError)
  end subroutine get_field_energy
end module PIC_ModField
