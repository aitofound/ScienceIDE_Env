module PIC_ModOutput

  use PIC_ModMain,ONLY: iStep, tSimulation, Dt, DX_D, &
       vInv, DxInv_D, UseSharedField 
  use PC_ModSize,ONLY: nDim, nCell_D, nX, nY, nZ, x_, y_, z_
  use PC_ModSize,ONLY: nBlock, nPType, jDim_, kDim_
  use PIC_ModParticles,     ONLY: M_P, Q_P, Wx_, Wz_, nPType
  use PC_ModParticleInField,ONLY: State_VGBI, add_moments
  use PIC_ModField,   ONLY: E_GDB,B_GDB
  use PC_ModMpi,     ONLY: pass_moments
  use PIC_ModProc
  use ModIoUnit,      ONLY: io_unit_new
  use PIC_ModLogFile, ONLY: nToWrite     !Number of test particles
  use PC_BATL_lib,    ONLY: CoordMin_D, CoordMax_D, CoordMin_DB
  use PC_BATL_tree,     ONLY: nRoot_D
  use ModUtilities, ONLY: CON_stop

  implicit none
  SAVE
  PRIVATE !Except  

  !\                                                                          
  !Unit for the log file                                                      
  !/                           
  integer :: iOutUnit = -1,iOutUnit_den=-1
  integer,  public :: nStepOut = -1, nStepOutMin = 0
  character(len=5),public :: TypeFile='real4' 
  !Array for saving coordinates, fields and moments in one timestep
  real,allocatable   :: PlotVar_VC(:,:,:,:)
  logical :: IsInitialized = .false.
  public :: PIC_save_files

contains
  !======================================================================
  subroutine PIC_save_files(TypeSave)
    use ModPlotFile,      ONLY: save_plot_file
    use PIC_ModParticles, ONLY: nPType
    use ModMpi  
    use PC_ModPhysics,    ONLY: No2Io_V, UnitT_, UnitX_
    character(len=*), intent(in):: TypeSave
    character(len=6) :: TypePosition
    real    :: Param_I(1:2*max(nPType,1))
    integer :: iSort
    character(len=15) :: NameFile='variables.outs'
    character(len=*),parameter :: FileDir='PC/plots/'
    integer :: iBlock=1
    
    character(len=:), allocatable, save:: NameVar
    character(len=*), parameter:: NameSub = 'save_files'
    !----------------------------------------------------------------------
    select case(TypeSave)
    case('INITIAL')
       if(nStepOut < 1          &! No output at all
            .or. IsInitialized  &! Done in the previous session
            )RETURN
       IsInitialized = .true.

       
       if(iProc==0) then
          allocate(character(len=69*max(nPType,1)) :: NameVar)
          write(NameVar(1:7),'(a7)')'QS0 MS0'
          do iSort=1,nPType
             write(NameVar((6*iSort-5):(6*iSort)),'(a4,i1,1x)') 'rhoS',iSort-1
             write(NameVar((15*iSort-14+6*nPType):(15*iSort+6*nPType)),&
                  '(3(a3,i1,1x))') 'uxS',iSort-1,'uyS',iSort-1,'uzS',iSort-1
             write(NameVar((4*iSort-3+21*nPType):(4*iSort+21*nPType)),&
                  '(a2,i1,1x)') 'pS',iSort-1
             write(NameVar((36*iSort-35+25*nPType):(36*iSort+25*nPType)),&
                  '(6(a4,i1,1x))') 'pXXS',iSort-1,'pYYS',iSort-1,'pZZS',&
                  iSort-1,'pXYS',iSort-1,'pXZS',iSort-1,'pYZS',iSort-1
             write(NameVar((4*iSort-3+61*nPType):(4*iSort+61*nPType)),&
                  '(a2,i1,1x)') 'QS',iSort-1
             write(NameVar((4*iSort-3+65*nPType):(4*iSort+65*nPType)),&
                  '(a2,i1,1x)') 'MS',iSort-1
          end do
          NameVar = 'Ex Ey Ez Bx By Bz '//NameVar
          TypePosition = 'rewind'
          if(nDim==3)then
             NameVar = 'x y z '//NameVar
          elseif(nDim==2) then
             NameVar = 'x y '//NameVar
          else
             NameVar = 'x '//NameVar
          end if
       end if
       
    case('NORMAL')
       !Check if this is time to save; if not, return
       !The calculation is done in advance_particles
       if(nStepOut<1.or.nStepOutMin > iStep.or.mod(iStep,nStepOut)/=0)&
            RETURN       
       TypePosition = 'append'
       
    case('FINAL')
       if(nStepOut < 1)RETURN
       !The calculation of  the moments is done in PIC_finalize
       TypePosition = 'append'
       
    case default
       call CON_stop('ALTOR '//NameSub// &
            ': Incorrect value for TypeSave='//trim(TypeSave))
    end select
    call write_moments
    call average_fields
    if(.not.UseSharedField.and.nProc>1)&
         call mpi_reduce_real_array(&
         PlotVar_VC(1,1,1,1), &
         (6 + 11*nPType)*product(nCell_D(1:nDim))*product(nRoot_D(1:nDim)),&
         MPI_SUM, 0, iComm, iError)
    if(iProc==0) then
       !Get cell-center averaged E and B; fields are the same on each Proc
       !Save the charge and mass ratio for each species in normalized unit
       Param_I(1:max(nPType,1))          = Q_P
       Param_I(max(nPType,1)+1:2*max(nPType,1)) = M_P
       
       call save_plot_file(FileDir//NameFile,                       &
            TypeFileIn = TypeFile,                                  &
            TypePositionIn = TypePosition,                          &
            nDimIn  = nDim,                                         &
            nStepIn = iStep,                                        &
            TimeIn  = tSimulation*No2Io_V(UnitT_),                  &
            ParamIn_I = Param_I,                                    &
            NameVarIn = NameVar,                                    &
            CoordMinIn_D = (CoordMin_D(1:nDim) + 0.50*Dx_D(1:nDim))*&
            No2Io_V(UnitX_),                                        &
            CoordMaxIn_D = (CoordMax_D(1:nDim) - 0.50*Dx_D(1:nDim))*&
            No2Io_V(UnitX_),                                        &
            VarIn_VIII = PlotVar_VC(:,:,:,:))
    end if
  end subroutine PIC_save_files
  !================================================================
  !Save the number density, velocity and pressure at certain timestep
  subroutine write_moments
    use PC_ModSize, ONLY: MaxDim
    integer :: iSort
    integer :: iVar,i,j,k, iBlock, iOffset_D(MaxDim) = 0
    !------------------------------------------------------------------
    
    !Periodic velocity BC. Need to be merged into other function later.
    if(UseSharedField)then
       !\
       !True array State_VGBI is available only on the root PE
       !/
       if(iProc/=0)RETURN !Version with UseSharedBlock=.true.
    end if
    if(.not.allocated(PlotVar_VC))&
         allocate(PlotVar_VC(6+11*nPType,&
         1:nX*nRoot_D(x_),1:nY*nRoot_D(y_),1:nZ*nRoot_D(z_)))
  
    PlotVar_VC = 0.0
    do iSort = 1, nPType
       do iBlock = 1, nBlock
          iOffset_D(1:nDim) = nint((CoordMin_DB(1:nDim,iBlock) - &
               CoordMin_D(1:nDim))/dX_D(1:nDim))
          !Looping over all the cell centers
          do k=1,nZ; do j=1,nY; do i=1,nX
             if(State_VGBI(1,i,j,k,iBlock,iSort)<=0.0)then
                PlotVar_VC(iSort+6,&
                  i+iOffset_D(x_),j+iOffset_D(y_),k+iOffset_D(z_)) = 0.0
                PlotVar_VC(nPType+3*iSort+4:nPType+3*iSort+6,&
                  i+iOffset_D(x_),j+iOffset_D(y_),k+iOffset_D(z_)) = 0.0
                PlotVar_VC(5*nPType+6*iSort+1:5*nPType+6*iSort+3,&
                  i+iOffset_D(x_),j+iOffset_D(y_),k+iOffset_D(z_)) = 0.0
                PlotVar_VC(5*nPType+6*iSort+4:5*nPType+6*iSort+6,&
                  i+iOffset_D(x_),j+iOffset_D(y_),k+iOffset_D(z_)) = 0.0
                CYCLE
             end if
             !The cell-centered velocity should be normalized by number density
             do iVar=2,4
                State_VGBI(iVar,i,j,k,iBlock,iSort) = &
                  State_VGBI(iVar,i,j,k,iBlock,iSort)/&
                  State_VGBI(1,i,j,k,iBlock,iSort)
             end do
             PlotVar_VC(iSort+6,&
                  i+iOffset_D(x_),j+iOffset_D(y_),k+iOffset_D(z_))  = &
                  abs(Q_P(iSort))*vInv*State_VGBI(1,i,j,k,iBlock,iSort)
             
             
             !Save velocity
             PlotVar_VC(nPType+3*iSort+4:nPType+3*iSort+6,&
                  i+iOffset_D(x_),j+iOffset_D(y_),k+iOffset_D(z_)) = &
                  State_VGBI(2:4,i,j,k,iBlock,iSort)
             !Save pressure tensor: Pxx Pyy Pzz Pxy Pxz Pyz
             PlotVar_VC(5*nPType+6*iSort+1:5*nPType+6*iSort+3,&
                  i+iOffset_D(x_),j+iOffset_D(y_),k+iOffset_D(z_)) = &
                  M_P(iSort)*vInv*(State_VGBI(5:7,i,j,k,iBlock,iSort) -&
                  State_VGBI(1,i,j,k,iBlock,iSort)*&
                  State_VGBI(2:4,i,j,k,iBlock,iSort)**2)
             
             PlotVar_VC(5*nPType+6*iSort+4,&
                  i+iOffset_D(x_),j+iOffset_D(y_),k+iOffset_D(z_)) = &
                  M_P(iSort)*vInv*(State_VGBI(8,i,j,k,iBlock,iSort)&
                  - State_VGBI(1,i,j,k,iBlock,iSort)*&
                  State_VGBI(2,i,j,k,iBlock,iSort)*&
                  State_VGBI(3,i,j,k,iBlock,iSort))
             PlotVar_VC(5*nPType+6*iSort+5,&
                  i+iOffset_D(x_),j+iOffset_D(y_),k+iOffset_D(z_)) = &
                  M_P(iSort)*vInv*(State_VGBI(9,i,j,k,iBlock,iSort)&
                  - State_VGBI(1,i,j,k,iBlock,iSort)*&
                  State_VGBI(2,i,j,k,iBlock,iSort)*&
                  State_VGBI(4,i,j,k,iBlock,iSort))
             PlotVar_VC(5*nPType+6*iSort+6,&
                  i+iOffset_D(x_),j+iOffset_D(y_),k+iOffset_D(z_)) = &
                  M_P(iSort)*vInv*(State_VGBI(10,i,j,k,iBlock,iSort)&
                  - State_VGBI(1,i,j,k,iBlock,iSort)*&
                  State_VGBI(3,i,j,k,iBlock,iSort)*&
                  State_VGBI(4,i,j,k,iBlock,iSort))
          end do; end do; end do
       
          !Save scalar pressure
          do k=1+iOffset_D(z_),nZ+iOffset_D(z_)
             do j=1+iOffset_D(y_),nY+iOffset_D(y_)
                do i=1+iOffset_D(x_),nX+iOffset_D(x_)
                   PlotVar_VC(4*nPType+iSort+6,i,j,k) = sum(PlotVar_VC(&
                        5*nPType+6*iSort+1:5*nPType+6*iSort+3,i,j,k))/3.0
                end do
             end do
          end do
       end do
    end do
  end subroutine write_moments
  !======================================================================
  subroutine average_fields
    use PC_ModSize, ONLY: MaxDim
    integer :: iBlock
    integer :: i, j, k, iOffset_D(MaxDim) = 0
    !--------------------
    if(UseSharedField)then
       !\
       ! True array of plot variables is only available
       ! on the root PE
       !/
       if(iProc/=0)RETURN
    end if
    do iBlock = 1, nBlock
       iOffset_D(1:nDim) = nint((CoordMin_DB(1:nDim,iBlock) - &
            CoordMin_D(1:nDim))/dX_D(1:nDim))
       do k=1,nZ; do j=1,nY; do i=1,nX
          ! Cell-centered averaged E field
          PlotVar_VC(1,i+iOffset_D(x_),j+iOffset_D(y_),k+iOffset_D(z_)) = &
               0.5*(E_GDB(i,j,k,1,iBlock)+E_GDB(i-1,j,k,1,iBlock))
          PlotVar_VC(2,i+iOffset_D(x_),j+iOffset_D(y_),k+iOffset_D(z_)) = &
               0.5*(E_GDB(i,j,k,2,iBlock)+E_GDB(i,j-jDim_,k,2,iBlock))
          PlotVar_VC(3,i+iOffset_D(x_),j+iOffset_D(y_),k+iOffset_D(z_)) = &
               0.5*(E_GDB(i,j,k,3,iBlock)+E_GDB(i,j,k-kDim_,3,iBlock))
          ! Cell-centered averaged B field
          PlotVar_VC(4,i+iOffset_D(x_),j+iOffset_D(y_),k+iOffset_D(z_)) = &
               1.0/((1 + jDim_)*(1 + kDim_))*&
               sum(B_GDB(i,j-jDim_:j,k-kDim_:k,1,iBlock))
          PlotVar_VC(5,i+iOffset_D(x_),j+iOffset_D(y_),k+iOffset_D(z_)) = &
               0.50/(1 + kDim_)*sum(B_GDB(i-1:i,j,k-kDim_:k,2,iBlock))
          PlotVar_VC(6,i+iOffset_D(x_),j+iOffset_D(y_),k+iOffset_D(z_)) = &
               0.50/(1 + jDim_)*sum(B_GDB(i-1:i,j-jDim_:j,k,3,iBlock))
       end do; end do; end do
    end do
  end subroutine average_fields
end module PIC_ModOutput
