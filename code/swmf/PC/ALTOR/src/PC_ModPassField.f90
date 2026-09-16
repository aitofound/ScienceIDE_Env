!  Copyright (C) 2002 Regents of the University of Michigan, 
!  portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf
! This file contains the top level methods for ALTOR
!==========================
module PC_ModMpi
  use PIC_ModField
  use ModMpi
  use PIC_ModProc
  use PIC_ModMain, ONLY: UseSharedField, vInv, CellVolume
  use PC_BATL_pass_face_field, ONLY: &
       add_ghost_face_field, add_ghost_cell_field
  use PC_ModSize, ONLY: nBlock
  implicit none
  !structures
  integer,parameter::iBuffSizeMax=10 !In MByte
  integer,parameter,private::KByte=1024, M=KByte*KByte
  !Methods:
  public::pass_density
  public::pass_moments
  public::pass_current
contains
  subroutine pass_density(iSort)
    integer,intent(in) :: iSort
    integer,parameter::iProcIn = 0
    integer,parameter::iLength=max(&
       iBuffSizeMax*M/(4*(iRealPrec+1)*nX*nY),1)
    real,dimension(1,1:nX,1:nY,iLength)::Buff_G
    integer::k,kNew,iBlock
    !-----------------------------------------------------------
    !Set up periodic BC in all three dimensions.
    call add_ghost_cell_field(1,nGF,State_VGBI(1:1,:,:,:,:,iSort))
    if(nProc==1)RETURN
    !\
    ! if the blocks are distributed, no need to allreduce
    !/
    if(.not.UseSharedField)RETURN
    !the result is at PE=iProcIn only
    do iBlock = 1, nBlock
       k=0
       do while(k<nZ)
          kNew=min(nZ,k+iLength)
          call MPI_REDUCE(&
               State_VGBI(1:1,1:nX,1:nY,k+1:kNew,iBlock,iSort),&
               Buff_G(1:1,1:nX,1:nY,1:kNew-k),&
               nX*nY*(kNew-k),&
               MPI_REAL,&
               MPI_SUM,&
               iProcIn,iComm,iError)
          if(iProc==iProcIn)State_VGBI(1,1:nX,1:nY,k+1:kNew,&
               iBlock,iSort) = Buff_G(1,:,:,1:kNew-k)
          k=kNew
       end do
    end do
  end subroutine pass_density
  !======================================================
  subroutine pass_moments(iSort)
    integer,intent(in)::iSort
    integer,parameter::iProcIn = 0
    integer,parameter::iLength=max(&
         iBuffSizeMax*M/(40*(iRealPrec+1)*nX*nY),1)
    real,dimension(1:10,1:nX,1:nY,iLength)::Buff_DG
    integer::k, kNew, iBlock
    !-----------------------------------------------------------
    !Set up periodic BC in all three dimensions.
    call add_ghost_cell_field(10,nGF,State_VGBI(:,:,:,:,:,iSort))
    if(nProc==1)RETURN
    !\
    ! if the blocks are distributed, no need to allreduce
    !/
    if(.not.UseSharedField)RETURN
    !the result is at PE=iProcIn only
    do iBlock = 1, nBlock
       k=0
       do while(k<nZ)
          kNew=min(nZ,k+iLength)
          call MPI_REDUCE(&
               State_VGBI(1:10,1:nX,1:nY,k+1:kNew,iBlock,iSort),&
               Buff_DG(1:10,1:nX,1:nY,1:kNew-k),&
               10*nX*nY*(kNew-k),&
               MPI_REAL,&
               MPI_SUM,&
               iProcIn,iComm,iError)
          if(iProc==iProcIn)&
               State_VGBI(:,1:nX,1:nY,k+1:kNew,iBlock,iSort) = &
               Buff_DG(:,:,:,1:kNew-k)
          k=kNew
       end do
    end do
  end subroutine pass_moments
  !=============================================================
  subroutine pass_current
    use PC_BATL_size
    real,dimension(0:nX,1-jDim_:nY,1-kDim_:nZ,MaxDim)::Buff_G
    integer::iBlock
    !-------------------
    call add_ghost_face_field(iGCN,Current_GDB)
    if(nProc==1)RETURN
    !\
    ! if the blocks are distributed, the current through the internal
    ! faces is correctly calculated
    !/
    if(.not.UseSharedField)RETURN
    do iBlock = 1, nBlock
       call MPI_ALLREDUCE(&
            Current_GDB(0:nX,1-jDim_:nY,1-kDim_:nZ,:,iBlock),&
            Buff_G,&
            (nX+1)*(nY+jDim_)*(nZ+kDim_)*MaxDim,&
            MPI_REAL,&
            MPI_SUM,&
            iComm,iError)
       Current_GDB(0:nX,1-jDim_:nY,1-kDim_:nZ,:,iBlock) = Buff_G 
    end do
  end subroutine pass_current
  !=======================
  subroutine get_min_val_rho(RhoMin)
    real   , intent(out) :: RhoMin
    real                 :: RhoMinLocal
    !---------------------------------
    RhoMin = 0.0 !Initialize output parameter
    if(UseSharedField)then
       if(iProc/=0)RETURN
       RhoMin = minval(Aux_CB(:,:,:,1:nBlock))
    else
       RhoMinLocal = minval(Aux_CB(:,:,:,1:nBlock))
       if(nProc>1)then
          call MPI_REDUCE(RhoMinLocal, RhoMin, 1, &
               MPI_REAL, MPI_MIN, 0, iComm, iError)
       else
          RhoMin = RhoMinLocal
       end if
    end if
    RhoMin = RhoMin*vInv
  end subroutine get_min_val_rho
  !=======================
  subroutine get_max_val_rho(RhoMax)
    real   , intent(out) :: RhoMax
    real                 :: RhoMaxLocal
    !---------------------------------
    RhoMax = 0.0 !Initialize output parameter
    if(UseSharedField)then
       if(iProc/=0)RETURN
       RhoMax = maxval(Aux_CB(:,:,:,1:nBlock))
    else
       RhoMaxLocal = maxval(Aux_CB(:,:,:,1:nBlock))
       if(nProc>1)then
          call MPI_REDUCE(RhoMaxLocal, RhoMax, 1, &
               MPI_REAL, MPI_MAX, 0, iComm, iError)
       else
          RhoMax = RhoMaxLocal
       end if
    end if
    RhoMax = RhoMax*vInv
  end subroutine get_max_val_rho
  !=======================
  subroutine get_rho_avr(RhoAvr)
    use PC_ModSize, ONLY: nCell_D, nDim
    real   , intent(out) :: RhoAvr
    real                 :: RhoLoc_I(2), Rho_I(2)
    !-------------------------------
    RhoAvr = 0.0 !Initialize output parameter
    if(UseSharedField)then
       if(iProc/=0)RETURN
       RhoAvr = sum(Aux_CB(:,:,:,1:nBlock))/&
            (product(nCell_D(1:nDim))*nBlock*CellVolume)
    else
       RhoLoc_I(1) = sum(Aux_CB(:,:,:,1:nBlock))
       RhoLoc_I(2) = product(nCell_D(1:nDim))*nBlock*CellVolume
       if(nProc>1)then
          call MPI_REDUCE(RhoLoc_I, Rho_I, 2, &
               MPI_REAL, MPI_SUM, 0, iComm, iError)
       else
          Rho_I = RhoLoc_I
       end if
       if(iProc==0)RhoAvr = Rho_I(1)/Rho_I(2)
    end if
  end subroutine get_rho_avr
 !=======================
  subroutine get_rho_int(RhoInt)
    real   , intent(out) :: RhoInt
    real                 :: RhoIntLoc
    !-------------------------------
    RhoInt = 0.0 !Initialize output parameter
    if(UseSharedField)then
       if(iProc/=0)RETURN
       RhoInt = sum(Aux_CB(:,:,:,1:nBlock))
    else
       RhoIntLoc = sum(Aux_CB(:,:,:,1:nBlock))
       if(nProc>1)then
          call MPI_REDUCE(RhoIntLoc, RhoInt, 1, &
               MPI_REAL, MPI_SUM, 0, iComm, iError)
       else
          RhoInt = RhoIntLoc
       end if
    end if
  end subroutine get_rho_int
end module PC_ModMpi
   
    
