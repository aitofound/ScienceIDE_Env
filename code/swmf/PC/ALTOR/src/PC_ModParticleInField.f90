module PC_ModParticleInField
  use PC_ModSize, ONLY:nBlock, nX, nY, nZ
  use PIC_ModField
  use PIC_ModFormFactor, ONLY: lOrderFF, iUpFF, iDownFF, iExt
  use PIC_ModFormfactor, ONLY: Node_D, NodeNew_D
  use PIC_ModFormFactor, ONLY: HighFF_ID, HighFFNew_ID, LowFF_ID
  implicit none
  SAVE
  
  !As long as Node_D and form-factors are defined, we
  !can get interpolated values from the structures of the
  !ModField class (electric and magnetic field) or calculate
  !the input from a given particle to the structures of the 
  !ModField class (density, electric current)
  !Methods
  public::e_interpolated_d !Interpolated electric field
  public::b_interpolated_d !Interpolated magnetic field
  public::add_density  !Adds an input to cell-centered density
  public::add_moments  !Adds an input to cell-centered density
                           !and velocity moments from a given particle
  public::add_current !Adds an input to an electric current
contains
  !================================
  function e_interpolated_d(iBlock)
    integer, intent(in)::iBlock
    !Sets the pointer to the fragment of E_GDB array to interpolate E
    real,dimension(1:3)::e_interpolated_d
    integer::i, j, k, n_D(3)=1, n1_D(3)
    !----------------

    e_interpolated_d = 0.0
    n_D(1:nDim) = Node_D(1:nDim) + iDownFF; n1_D = n_D -1
    
    do k=1, lOrderFF*kDim_ + 1
       do j=1, lOrderFF*jDim_ +1
          e_interpolated_d(x_) = e_interpolated_d(x_) +&
               sum(E_GDB(n_D(1)-iExt:lOrderFF+iExt+n1_D(1), &
               j+n1_D(2), k+n1_d(3), x_,iBlock)*LowFF_ID(:, x_))&
               *HighFF_ID(j, y_)*HighFF_ID(k, z_)
       end do
       do j=1-iExt*jDim_,(lOrderFF + iExt - 1)*jDim_ + 1
          e_interpolated_d(y_) = e_interpolated_d(y_) +&
               sum(E_GDB(n_D(1):lOrderFF+n_D(1), &
               j+n1_D(2), k+n1_D(3), y_,iBlock)*HighFF_ID(:,x_))&
               *LowFF_ID(j,y_)*HighFF_ID(k,z_)
       end do
    end do
    do k=1-iExt*kDim_,(lOrderFF + iExt -1)*kDim_ +1
       do j=1, lOrderFF*jDim_ + 1
          e_interpolated_d(z_) = e_interpolated_d(z_) +&
               sum(E_GDB(n_D(1):lOrderFF+n_D(1),&
               j+n1_D(2),k+n1_D(3),z_,iBlock)*HighFF_ID(:,x_))&
               *HighFF_ID(j,y_)*LowFF_ID(k,z_)
       end do
    end do
  end function e_interpolated_d

  !===========================
  function b_interpolated_d(iBlock)
    integer,intent(in)::iBlock
    !Sets the pointer to the fragment of the magnetic field array to 
    !interpolate B
    real,dimension(1:3)::b_interpolated_d
 
    integer::i, j, k, n_D(3)=1, n1_D(3)
    !--------------
    b_interpolated_d = 0.0

    n_D(1:nDim) = Node_D(1:nDim) + iDownFF; n1_D = n_D -1
    do k=1 - iExt*kDim_,(lOrderFF + iExt -1)*kDIm_ + 1
       do j=1 - iExt*jDim_,(lOrderFF + iExt - 1)*jDim_ +1
          b_interpolated_d(x_)= b_interpolated_d(x_)+ &
               sum(B_GDB(n_D(1):lOrderFF+n_D(1),&
               j+n1_D(2),k+n1_D(3),x_,iBlock)*HighFF_ID(:,x_))&
               *LowFF_ID(j,y_)*LowFF_ID(k,z_)
       end do
       do j=1,lOrderFF*kDim_ + 1
          b_interpolated_d(y_)= b_interpolated_d(y_)+ &
               sum(B_GDB(n_D(1)-iExt:lOrderFF+iExt+n1_D(1),&
               j+n1_D(2),k+n1_D(3),y_,iBlock)*LowFF_ID(:,x_))&
               *HighFF_ID(j,y_)*LowFF_ID(k,z_)
       end do
    end do
    do k=1,lOrderFF*kDim_ + 1
       do j=1-iExt*jDim_,(lOrderFF + iExt -1)*jDim_ + 1
          b_interpolated_d(z_)= b_interpolated_d(z_)+ &
               sum(B_GDB(n_D(1)-iExt:n1_D(1)+lOrderFF+iExt,&
               j+n1_D(2),k+n1_D(3),z_,iBlock)*LowFF_ID(:,x_))&
               *LowFF_ID(j,y_)*HighFF_ID(k,z_)
       end do
    end do
 
  end function b_interpolated_d
  !==============================================================
  subroutine add_moments(W_D,NodeIn_D,HighFFIn_ID,iBlock,iSort)
    !Adds an input to the density and velocity, from a given particle
    !using the same form factors
    real,   dimension(MaxDim),intent(in) :: W_D  !Velocity
    integer,dimension(MaxDim),intent(in) :: NodeIn_D
    real,   dimension(1+lOrderFF,MaxDim),intent(in)::HighFFIn_ID
    integer,intent(in):: iBlock,iSort
    integer:: i,j,k,i1,j1,k1
    real:: FFProduct, Var_V(10)
    !-----------------------   
    Var_V = (/1.0, W_D, W_D**2, W_D(1)*W_D(2), W_D(1)*W_D(3), W_D(2)*W_D(3)/)
    k1=0
    do k=NodeIn_D(3)+iDownFF*kDim_,NodeIn_D(3)+iUpFF*kDim_
       k1=k1+1
       j1=0
       do j=NodeIn_D(2)+iDownFF*jDim_,NodeIn_D(2)+iUpFF*jDim_
          j1=j1+1
          i1=0
          FFProduct=HighFFIn_ID(k1,z_)*HighFFIn_ID(j1,y_)
          do i=NodeIn_D(1)+iDownFF,NodeIn_D(1)+iUpFF
             i1=i1+1
             !Add the product of formfactors
             State_VGBI(:,i,j,k,iBlock,iSort) = State_VGBI(:,i,j,k,iBlock,iSort) + &
                  HighFFIn_ID(i1,x_)*FFProduct*Var_V  
          end do
       end do
    end do

  end subroutine add_moments
  !=========================
  subroutine add_density(NodeIn_D,HighFFIn_ID,iBlock,iSort)
    !Adds an input to the density only.
    integer,dimension(MaxDim),intent(in) :: NodeIn_D
    real,   dimension(1+lOrderFF,MaxDim),intent(in)::HighFFIn_ID
    integer,intent(in):: iBlock,iSort
    integer:: i,j,k,i1,j1,k1
    real:: FFProduct
    !-----------------------   
    k1=0
    do k=NodeIn_D(3)+iDownFF*kDim_,NodeIn_D(3)+iUpFF*kDim_
       k1=k1+1
       j1=0
       do j=NodeIn_D(2)+iDownFF*jDim_,NodeIn_D(2)+iUpFF*jDim_
          j1=j1+1
          i1=0
          FFProduct=HighFFIn_ID(k1,z_)*HighFFIn_ID(j1,y_)
          do i=NodeIn_D(1)+iDownFF,NodeIn_D(1)+iUpFF
             i1=i1+1
             !Add the product of formfactors
             State_VGBI(1,i,j,k,iBlock,iSort) = State_VGBI(1,i,j,k,iBlock,iSort) + &
                  HighFFIn_ID(i1,x_)*FFProduct
          end do
       end do
    end do

  end subroutine add_density
  !============================================
  subroutine add_current(QPerVDx_D,W_D,iBlock) 
    real,intent(in)::QPerVDx_D(MaxDim),W_D(x_:z_)
    integer,intent(in)::iBlock
    logical:: IsExtended
    !\
    ! For extended stencil
    !/
    real :: CurrentFragmentExt_GD(0:iUpFF-iDownFF+2,&
         0:iUpFF-iDownFF+2,&
         0:iUpFF-iDownFF+2,1:3)
    integer :: iD_D(3) = 0, iU_D(3) = 0
    real,parameter::sqrt13=1/1.732050808

    real,dimension(1:lOrderFF+1,MaxDim)::DeltaFPlus_ID,DeltaFMinus_ID
    real,dimension(1:lOrderFF  ,MaxDim)::FI_ID
    integer::i, j, k, iDim, n_D(3)=1, n1_D(3)
    !-------------------
    IsExtended = any(Node_D(1:nDim)/=NodeNew_D(1:nDim))

    if(IsExtended)then
       call get_current_extended

       iD_D = min(NodeNew_D - Node_D,0)
       iU_D = max(NodeNew_D - Node_D,0)

       Current_GDB(&
            Node_D(1)+iDownFF+iD_D(1):Node_D(1)+iUpFF+iU_D(1),&
            Node_D(2)+iDownFF*jDim_+iD_D(2):Node_D(2)+iUpFF*jDim_+iU_D(2),&
            Node_D(3)+iDownFF*kDim_+iD_D(3):Node_D(3)+iUpFF*kDim_+iU_D(3),&
            1:3,iBlock) = &
            Current_GDB( Node_D(1)+iDownFF+iD_D(1):Node_D(1)+iUpFF+iU_D(1),&
            Node_D(2)+iDownFF*jDim_+iD_D(2):Node_D(2)+iUpFF*jDim_+iU_D(2),&
            Node_D(3)+iDownFF*kDim_+iD_D(3):Node_D(3)+iUpFF*kDim_+iU_D(3),&
            1:3,iBlock) + & 
            CurrentFragmentExt_GD(1+iD_D(1):iUpFF-iDownFF+1+iU_D(1),&
            1+iD_D(2):(iUpFF-iDownFF)*jDim_+1+iU_D(2),&
            1+iD_D(3):(iUpFF-iDownFF)*kDim_+1+iU_D(3),&
            1:3)                
    else
       DeltaFPlus_ID  = HighFFNew_ID + HighFF_ID
       DeltaFMinus_ID = HighFFNew_ID - HighFF_ID

       FI_ID(1,:)=DeltaFMinus_ID(1,:)

       !Use recursive formula for FI
       do i=2,lOrderFF
          FI_ID(i,:) = FI_ID(i-1,:) + DeltaFMinus_ID(i,:)
       end do

       !To optimize algebra, multiply FI by -q*dt/4
       !and divide \DeltaFMinus by sqrt(3):
       do iDim = 1, nDIm
          FI_ID(:,iDim)=(-QPerVDx_D(iDim)*0.250)*FI_ID(:,iDim)
       end do
       !\
       ! For directions orthogonal to grid 
       ! (should be multiplied by 
       ! velocity-to-the speed of light ratio  
       !/
       do iDim = nDim+1, MaxDim
          FI_ID(:,iDim) = 0.250*QPerVDx_D(iDim)*W_D(iDim)
       end do

       DeltaFMinus_ID = sqrt13*DeltaFMinus_ID

       n_D(1:nDim) = Node_D(1:nDim) + iDownFF; n1_D = n_D -1


       !add current density:
       do k=1,lOrderFF*kDim_+1
          do j=1,lOrderFF*jDim_+1
             !\
             !  x_ currents
             !/
             Current_GDB(n_D(1):lOrderFF+n1_D(1),&
                  j+n1_D(2),k+n1_D(3),x_,iBlock) = &
                  Current_GDB(n_D(1):lOrderFF+n1_D(1),&
                  j+n1_D(2),k+n1_D(3),x_,iBlock) +&
                  FI_ID(:,x_)*(&
                  DeltaFPlus_ID (j,y_)*DeltaFPlus_ID (k,z_)+&
                  DeltaFMinus_ID(j,y_)*DeltaFMinus_ID(k,z_))
          end do
          do j=1,(lOrderFF-1)*jDim_+1
             !\
             !  y_ currents
             !/
             Current_GDB(n_D(1):lOrderFF+n_D(1),&
                  j+n1_D(2),k+n1_D(3),y_,iBlock) = &
                  Current_GDB(n_D(1):lOrderFF+n_D(1),&
                  j+n1_D(2),k+n1_D(3),y_,iBlock) + &
                  FI_ID(j,y_)*(&
                  DeltaFPlus_ID (:,x_)*DeltaFPlus_ID (k,z_)+&
                  DeltaFMinus_ID(:,x_)*DeltaFMinus_ID(k,z_))
          end do
       end do
       do k=1,(lOrderFF-1)*kDim_+1
          do j=1,lOrderFF*jDim_+1
             !\
             !  z_ currents
             !/
             Current_GDB(n_D(1):lOrderFF+n_D(1),&
                  j+n1_D(2),k+n1_D(3),z_,iBlock) = &
                  Current_GDB(n_D(1):lOrderFF+n_D(1),&
                  j+n1_D(2),k+n1_D(3),z_,iBlock) + &
                  FI_ID(k,z_)*(&
                  DeltaFPlus_ID (:,x_)*DeltaFPlus_ID (j,y_)+&
                  DeltaFMinus_ID(:,x_)*DeltaFMinus_ID(j,y_))
          end do
       end do
    end if

  contains
    subroutine get_current_extended
      real,dimension(0:iUpFF-iDownFF+2,MaxDim)::DeltaFPlusExt_ID
      real,dimension(0:iUpFF-iDownFF+2,MaxDim)::DeltaFMinusExt_ID
      real,dimension(0:iUpFF-iDownFF+1,MaxDim)::FIExt_ID
      !---------------
      CurrentFragmentExt_GD = 0.0
      DeltaFPlusExt_ID      = 0.0

      !\     
      !First, add the properly shifted new formfactor
      !/
      DeltaFPlusExt_ID(1+NodeNew_D(1)-Node_D(1):&
           1+NodeNew_D(1)-Node_D(1)+iUpFF-iDownFF,x_)=&
           HighFFNew_ID(:,x_)

      DeltaFPlusExt_ID(1+NodeNew_D(2)-Node_D(2):&
           1+NodeNew_D(2)-Node_D(2)+iUpFF-iDownFF,y_) = &
           HighFFNew_ID(:,y_)

      DeltaFPlusExt_ID(1+NodeNew_D(3)-Node_D(3):&
           1+NodeNew_D(3)-Node_D(3)+iUpFF-iDownFF,z_) = &
           HighFFNew_ID(:,z_)

      DeltaFMinusExt_ID = DeltaFPlusExt_ID

      !\
      !Then add and subtruct the unshifted old formfactor 
      !/          
      DeltaFPlusExt_ID(1:iUpFF-iDownFF+1,:)  = &
           DeltaFPlusExt_ID( 1:iUpFF-iDownFF+1,:) + HighFF_ID

      DeltaFMinusExt_ID(1:iUpFF-iDownFF+1,:) = &
           DeltaFMinusExt_ID(1:iUpFF-iDownFF+1,:) - HighFF_ID

      !\
      ! Using recurrent relationships, find F_I
      !/
      FIExt_ID(0,:)=DeltaFMinusExt_ID(0,:)

      do i=1,iUpFF-iDownFF+1
         FIExt_ID(i,:) = FIExt_ID(i-1,:) + DeltaFMinusExt_ID(i,:)
      end do

      !To optimize algebra, multiply FI by -q*4*\pi*dt/4
      !and divide \DeltaFMinus by sqrt(3):
      do iDim=1,nDim
         FIExt_ID(:,iDim) = (-QPerVDx_D(iDim)*0.250)*FIExt_ID(:,iDim)
      end do
      !\
      ! For directions orthogonal to grid 
      ! (should be multiplied by 
      ! velocity-to-the speed of light ratio  
      !/
      do iDim = nDim+1, MaxDim
         FIExt_ID(:,iDim) = 0.250*QPerVDx_D(iDim)*W_D(iDim)
      end do
      DeltaFMinusExt_ID = sqrt13*DeltaFMinusExt_ID

      !add current density:
      do k=0,iUpFF-iDownFF+2
         do j=0,iUpFF-iDownFF+2
            !\
            !  x_ currents
            !/
            CurrentFragmentExt_GD(0:iUpFF-iDownFF+1,j,k,x_) = &
                 FIExt_ID(:,x_)*(&
                 DeltaFPlusExt_ID( j,y_)*DeltaFPlusExt_ID( k,z_)+&
                 DeltaFMinusExt_ID(j,y_)*DeltaFMinusExt_ID(k,z_))
         end do
         do j=0,iUpFF-iDownFF+1
            !\
            !  y_ currents
            !/
            CurrentFragmentExt_GD(0:iUpFF-iDownFF+2,j,k,y_) = &
                 FIExt_ID(j,y_)*(&
                 DeltaFPlusExt_ID( :,x_)*DeltaFPlusExt_ID( k,z_)+&
                 DeltaFMinusExt_ID(:,x_)*DeltaFMinusExt_ID(k,z_))
         end do
      end do
      do k=0,iUpFF-iDownFF+1
         do j=0,iUpFF-iDownFF+2
            !\
            !  z_ currents
            !/
            CurrentFragmentExt_GD(0:iUpFF-iDownFF+2,j,k,z_) = &
                 FIExt_ID(k,z_)*(&
                 DeltaFPlusExt_ID( :,x_)*DeltaFPlusExt_ID( j,y_)+&
                 DeltaFMinusExt_ID(:,x_)*DeltaFMinusExt_ID(j,y_))
         end do
      end do
    end subroutine get_current_extended
  end subroutine add_current
  !=========================
end module PC_ModParticleInField

