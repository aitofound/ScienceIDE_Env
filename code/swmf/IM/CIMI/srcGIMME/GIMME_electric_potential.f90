module GIMME_potential

  ! set the matrix and solve the potential from the linear equaion
  !  Ri^2sin2Theta*jr_ij =
  !              A_ij Pot_i+1,j + B_ij Pot_ij + C_ij Pot_i-1,j + D_ij Pot_i,j+1
  !              + E_ij Pot_i,j-1
  ! which can be expressed as Ax=b where x is the potential and b is the source
  ! term.
  ! A preconditioned restarted GMRES routine is used to solve this equation.

contains
  !============================================================================
  subroutine get_gimme_potential

    use GIMME_iono_grid, ONLY: &
         UseFullSphere,nTheta,nPhi,Theta_G, Phi_G, Riono, &
         dPhi, iThetaGround, iPhiGround, Potential_G
    use GIMME_conductance, ONLY: A_C,B_C,C_C,D_C,E_C,SigmaP_G,SigmaH_G,Sigma0_G
    use GIMME_mag_input, ONLY: Jr_C,DoCoupleCimi,iThetaBC_I,PotBC_C
    use ModLinearSolver, ONLY: gmres, bicgstab,prehepta,Lhepta,Uhepta
    use ModUtilities, ONLY: CON_stop
    integer :: iPhi,iTheta,iMatrix, iRow, iColumn, iRowElements
    integer :: nMatrixElements,nMatrixRows
    integer,allocatable :: iRow_I(:), iCol_I(:)
    real,   allocatable :: Source_I(:),Potential_I(:)
    real,   allocatable :: A_I (:)

    ! integer, parameter :: MaxIteration = 1000, MaxInnerIteration=50
    integer, parameter :: MaxIteration = 100000, MaxInnerIteration=50
    real :: ToleranceAbs = 1e-2,ToleranceRel = 1e-2

    integer :: nIteration, iError=0, iCycle

    ! For versions of krylov solver in ModLinearSolver in share you need to specify
    ! the size of the Krylov subspace (nKrylov), and the maximum number of iterations
    !(MaxIteration). nKrylov basically sets the accuracy of the approximation, and
    ! MaxIteration/nKrylov defines the number of restarts
    ! integer :: nKrylov=5000
    integer :: nKrylov=50
    character(len=10) :: TypeSolver='gmres'
    ! character(len=10) :: TypeSolver='bicgstab'
    ! character(len=10)  :: TypeSolver

    ! for option gmres and bicgstab there is an option to use a preconditioner
    ! note mgmres has it's own preconditioner
    ! logical :: UsePreconditioner=.false.
    logical :: UsePreconditioner=.true.

    ! if we should use the previous solution as an initial guess
    logical :: UseInitialGuess = .true.
    ! When using preconditioner you also need to pass the separate main diagonal
    ! sub diagonal, superdiagonal, etc blocks as defined in ModLinearSolver
    real, allocatable :: d_I(:),e_I(:),f_I(:),e1_I(:),f1_I(:), e2_I(:), f2_I(:)
    integer :: iCount

    logical,save :: IsFirstCall=.true.
    ! external :: matvec_gimme
    !--------------------------------------------------------------------------

    ! set the number of matrix elements

    if (UseFullSphere) then
       ! one fewer row in the matrix since ground point is not considered
       nMatrixRows=nTheta*nPhi-1
       ! 5 elements per row but four points boardering the ground point
       ! have only four elements
       nMatrixElements = nMatrixRows*5-4

    else
       nMatrixRows = nTheta*nPhi

       ! 5 elements per row except for the nTheta points (of which there are nPhi)
       ! which have only 4
       nMatrixElements = nMatrixRows*5-nPhi
    endif

    if (.not.allocated(A_I))        allocate(A_I        (nMatrixElements))
    if (.not.allocated(iRow_I))     allocate(iRow_I     (nMatrixRows+1))
    if (.not.allocated(iCol_I))     allocate(iCol_I     (nMatrixElements))
    if (.not.allocated(Source_I))   allocate(Source_I   (nMatrixRows))
    if (.not.allocated(Potential_I))allocate(Potential_I(nMatrixRows))

    ! allocate the different diagonals when using the preconditioner)
    if (UsePreconditioner .and. .not.allocated(d_I)) then
       allocate(d_I(nMatrixRows))
       allocate(e_I(nMatrixRows))
       allocate(f_I(nMatrixRows))
       allocate(e1_I(nMatrixRows))
       allocate(f1_I(nMatrixRows))
       allocate(e2_I(nMatrixRows))
       allocate(f2_I(nMatrixRows))
    endif

    ! build up the sparse A matrix in compressed row  format where successive
    ! indicies define the number of elements on a given line
    iRow_I(1)=1
    iRow=1

    ! Set the A matrix and the corresponding column indicies
    iMatrix=0

    azimuth_loop: do iPhi=1,nPhi
       polar_loop: do iTheta=1,nTheta
          if (iTheta < iThetaBC_I(iTheta)) then
             ! case high latitude enforce BC

             ! skip ground point
             if (iTheta==iThetaGround .and. iPhi==iPhiGround) CYCLE polar_loop
             iRowElements=0
             call column_index(iTheta,iPhi-1,iColumn)
             if (iColumn>nMatrixRows)write(*,*) 'E ha', iTheta,iPhi
             if (iColumn /= -1) then
                iMatrix=iMatrix+1
                A_I(iMatrix)=0.0
                iCol_I(iMatrix)=iColumn
                iRowElements=iRowElements+1
             endif

             call column_index(iTheta-1,iPhi,iColumn)
             if (iColumn>nMatrixRows)write(*,*) 'C ha', iTheta,iPhi
             if (iColumn /= -1) then
                iMatrix=iMatrix+1
                A_I(iMatrix)=0.0
                iCol_I(iMatrix)=iColumn
                iRowElements=iRowElements+1
             endif

             call column_index(iTheta,iPhi,iColumn)
             if (iColumn>nMatrixRows)write(*,*) 'B ha', iTheta,iPhi
             if (iColumn /= -1) then
                iMatrix=iMatrix+1
                A_I(iMatrix)=1.0
                iCol_I(iMatrix)=iColumn
                iRowElements=iRowElements+1
             endif

             call column_index(iTheta+1,iPhi,iColumn)
             if (iColumn>nMatrixRows)write(*,*) 'A ha', iTheta,iPhi
             if (iColumn /= -1) then
                iMatrix=iMatrix+1
                A_I(iMatrix)=0.0
                iCol_I(iMatrix)=iColumn
                iRowElements=iRowElements+1
             endif

             call column_index(iTheta,iPhi+1,iColumn)
             if (iColumn>nMatrixRows)write(*,*) 'D ha', iTheta,iPhi
             if (iColumn /= -1) then
                iMatrix=iMatrix+1
                A_I(iMatrix)=0.0
                iCol_I(iMatrix)=iColumn
                iRowElements=iRowElements+1
             endif
          else
             ! low latitude, solve here

             ! skip ground point
             if (iTheta==iThetaGround .and. iPhi==iPhiGround) CYCLE polar_loop
             iRowElements=0
             call column_index(iTheta,iPhi-1,iColumn)
             if (iColumn>nMatrixRows)write(*,*) 'E ha', iTheta,iPhi
             if (iColumn /= -1) then
                iMatrix=iMatrix+1
                A_I(iMatrix)=E_C(iTheta,iPhi)
                iCol_I(iMatrix)=iColumn
                iRowElements=iRowElements+1
             endif

             call column_index(iTheta-1,iPhi,iColumn)
             if (iColumn>nMatrixRows)write(*,*) 'C ha', iTheta,iPhi
             if (iColumn /= -1) then
                iMatrix=iMatrix+1
                A_I(iMatrix)=C_C(iTheta,iPhi)
                iCol_I(iMatrix)=iColumn
                iRowElements=iRowElements+1
             endif

             call column_index(iTheta,iPhi,iColumn)
             if (iColumn>nMatrixRows)write(*,*) 'B ha', iTheta,iPhi
             if (iColumn /= -1) then
                iMatrix=iMatrix+1
                A_I(iMatrix)=B_C(iTheta,iPhi)
                iCol_I(iMatrix)=iColumn
                iRowElements=iRowElements+1
             endif

             call column_index(iTheta+1,iPhi,iColumn)
             if (iColumn>nMatrixRows)write(*,*) 'A ha', iTheta,iPhi
             if (iColumn /= -1) then
                iMatrix=iMatrix+1
                A_I(iMatrix)=A_C(iTheta,iPhi)
                iCol_I(iMatrix)=iColumn
                iRowElements=iRowElements+1
             endif

             call column_index(iTheta,iPhi+1,iColumn)
             if (iColumn>nMatrixRows)write(*,*) 'D ha', iTheta,iPhi
             if (iColumn /= -1) then
                iMatrix=iMatrix+1
                A_I(iMatrix)=D_C(iTheta,iPhi)
                iCol_I(iMatrix)=iColumn
                iRowElements=iRowElements+1
             endif
          endif
          ! set the compressed row index
          iRow_I(iRow+1)=iRow_I(iRow)+iRowElements
          iRow=iRow+1

       enddo polar_loop
    enddo azimuth_loop

    do iPhi=1,nPhi
       do iTheta = 1, nTheta
          if (iTheta==iThetaGround .and. iPhi==iPhiGround) CYCLE
          if (iTheta < iThetaBC_I(iTheta)) then
             call column_index(iTheta,iPhi,iColumn)
             Source_I(iColumn)=PotBC_C(iTheta,iPhi)
          else
             call column_index(iTheta,iPhi,iColumn)
             Source_I(iColumn)=Jr_C(iTheta,iPhi)*sin(Theta_G(iTheta))**2*Riono**2
          endif
       end do
    enddo

    iCount=0
    if(UsePreconditioner) then
       do iPhi=1,nPhi
          do iTheta = 1, nTheta
             iCount=iCount+1
             ! Source_I(iCount)=Jr_C(iTheta,iPhi)*sin(Theta_G(iTheta))**2*Riono**2
             if (iTheta < iThetaBC_I(iTheta)) then
                ! coef for iTheta,iPhi
                d_I(iCount) = 1.0
                ! coef for iTheta-1,iPhi
                e_I(iCount) = 0.0
                ! coef for iTheta+1,iPhi
                f_I(iCount) = 0.0
                ! coef for iTheta,iPhi-1
                e1_I(iCount)= 0.0
                ! coef for iTheta,iPhi+1
                f1_I(iCount)= 0.0
             else
                ! coef for iTheta,iPhi
                d_I(iCount) = B_C(iTheta,iPhi)
                ! coef for iTheta-1,iPhi
                e_I(iCount) = C_C(iTheta,iPhi)
                ! coef for iTheta+1,iPhi
                f_I(iCount) = A_C(iTheta,iPhi)
                ! coef for iTheta,iPhi-1
                e1_I(iCount)= E_C(iTheta,iPhi)
                ! coef for iTheta,iPhi+1
                f1_I(iCount)= D_C(iTheta,iPhi)
             endif
             ! if (iTheta==1) e_I(iCount)=0.0
             ! if (iTheta==nTheta) f_I(iCount)=0.0
             ! if (iPhi==1) e1_I(iCount)=0.0
             ! if (iPhi==nPhi) f1_I(iCount)=0.0
          enddo
       enddo

       ! use prehepta to obtain L and U from A
       call prehepta(nMatrixRows,1,nTheta,nMatrixRows,-0.5,d_I,e_I,f_I,e1_I,f1_I)

       ! precondition rhs so rhs' = U^-1 * L^-1* source
       call Lhepta(       nMatrixRows,1,nTheta,nMatrixRows,Source_I,d_I,e_I,e1_I)
       call Uhepta(.true.,nMatrixRows,1,nTheta,nMatrixRows,Source_I,f_I,f1_I)
    endif

    ! set initial guess
    !  if (.not.UseInitialGuess .or. IsFirstCall) then
    Potential_I = 0.0
    !     IsFirstCall=.false.
    !  endif

    ! call Solver
    select case(TypeSolver)
    case('gmres')
       ! use GMRES from the share library
       ! note nIteration has to be reset to MaxIteration each call
       nIteration = MaxIteration
       ! call gmres(matvec_gimme,Source_I,Potential_I,UseInitialGuess, nMatrixRows, &
       !     nKrylov, ToleranceRel, 'rel', nIteration, iError,.false.)
       call gmres(matvec_gimme,Source_I,Potential_I,UseInitialGuess, nMatrixRows, &
            nKrylov, ToleranceAbs, 'abs', nIteration, iError,.false.)

       if (iError /= 0 .and. iError /=3) then
          write(*,*)'IM error: gmres iError     = ', iError
          write(*,*)'IM error: gmres nIteration = ', nIteration
          call CON_stop('')
       endif
    case('bicgstab')
       ! use BICGSTAB from the share library
       ! do iCycle=1,10
       ! write(*,*) 'HA'
       ! note nIteration has to be reset to MaxIteration each call
       nIteration = 10*MaxIteration
       call bicgstab(matvec_gimme,Source_I,Potential_I,UseInitialGuess, nMatrixRows, &
            ToleranceRel, 'rel', nIteration, iError,.false.)

       if (iError /= 0 .and. iError /=3) then
          write(*,*)'IM error: bicgstab iError     = ', iError
          write(*,*)'IM error: bicgstab nIteration = ', nIteration
          call CON_stop('')
       endif

       ! if (iError==0 .or. iError==3) exit
       ! enddo
    case default
       ! use GMRES from the share library
       ! note nIteration has to be reset to MaxIteration each call
       nIteration = MaxIteration
       ! call gmres(matvec_gimme,Source_I,Potential_I,UseInitialGuess, nMatrixRows, &
       !     nKrylov, ToleranceRel, 'rel', nIteration, iError,.false.)
       call gmres(matvec_gimme,Source_I,Potential_I,UseInitialGuess, nMatrixRows, &
            nKrylov, ToleranceAbs, 'abs', nIteration, iError,.false.)

       if (iError /= 0 .and. iError /=3) then
          write(*,*)'IM error: gmres iError     = ', iError
          write(*,*)'IM error: gmres nIteration = ', nIteration
          call CON_stop('')
       endif

       ! use GMRES from mgmres
       ! call pmgmres_ilu_cr ( nMatrixRows, nMatrixElements, iRow_I, iCol_I, A_I, &
       !     Potential_I, Source_I, MaxIteration, MaxInnerIteration, ToleranceAbs, &
       !     ToleranceRel )
    end select
    write(*,*) maxval(Potential_I),minval(Potential_I)

    ! unravel Potential matrix
    iMatrix=1
    do iPhi=1,nPhi
       do iTheta = 1, nTheta
          ! skip the ground point when using the entire sphere
          if (iTheta==iThetaGround .and. iPhi==iPhiGround) CYCLE
          Potential_G(iTheta,iPhi)=Potential_I(iMatrix)
          iMatrix=iMatrix+1
       end do
    end do

    ! fill Ghost cells
    Potential_G(0,:)=Potential_G(1,:)
    Potential_G(nTheta+1,:)=Potential_G(nTheta,:)

    Potential_G(:,0)=Potential_G(:,nPhi)
    Potential_G(:,nPhi+1)=Potential_G(:,1)

  contains
    !==========================================================================
    ! For a given iTheta and iPhi, return iColumn.
    ! We got in order for each iPhi we step through iTheta. For iTheta = 0
    ! we go to iTheta=1 and iPhi on the other side of the pole.
    ! For iTheta >nTheta we return -1 meaning that this index should not be used
    ! For iPhi
    subroutine column_index(iThetaIn,iPhiIn,iColumnOut)
      integer, intent(in) :: iThetaIn,iPhiIn
      integer, intent(out) :: iColumnOut
      integer :: iThetaTmp, iPhiTmp
      !------------------------------------------------------------------------

      iThetaTmp=iThetaIn
      iPhiTmp=iPhiIn

      if(iPhiIn==0) iPhiTmp=nPhi
      if(iPhiIn==nPhi+1) iPhiTmp=1

      ! When iThetaIn is 0 we should take the point on the other side of the pole
      if (iThetaIn == 0) then
         iThetaTmp=1
         iPhiTmp = mod(iPhiIn+nPhi/2,nPhi-1)+1
      endif

      ! When iThetaIn is nTheta and we use the whole sphere
      ! we should take the point on the other side of the pole
      if (iThetaIn == nTheta+1 .and. UseFullSphere) then
         iThetaTmp=nTheta
         iPhiTmp = mod(iPhiIn+nPhi/2,nPhi-1)+1
      endif

      ! When not using the whole sphere andwhen iThetaIn is nTheta+1 &
      ! then return -1 since at this location we set BC
      if (iThetaTmp>nTheta .and. .not.UseFullSphere) then
         iColumnOut = -1
         RETURN
      endif

      ! When using the full sphere we need the ground reference point is not solved
      if (UseFullSphere .and. iThetaIn==iThetaGround .and.iPhiIn==iPhiGround) then
         iColumnOut=-1
         RETURN
      endif

      iColumnOut=nTheta*(iPhiTmp-1)+iThetaTmp

      ! When using the full sphere we need the ground reference point is skipped
      ! in the indexing
      if (UseFullSphere .and. iColumnOut>nTheta*(iPhiGround-1)+iThetaGround) then
         iColumnOut= iColumnOut-1
      endif

    end subroutine column_index
    !==========================================================================

    ! matrix multiplicaiton M*a=b for gimme to use the share library linear solver
    ! here M is the matrix connecting the potential (a) we are solving for with the
    ! source on the RHS (b)
    subroutine matvec_gimme(a,b,n)
      integer, intent(in) :: n
      real, intent(in) ::  a(n)
      real, intent(out) :: b(n)
      integer :: i,i1,i2

      !------------------------------------------------------------------------
      b=0.0
      do i=1,n
         i1 = iRow_I(i)
         i2 = iRow_I(i+1)-1
         ! b(iCol_I(i1:i2)) = b(iCol_I(i1:i2))+A_I(i1:i2)*a(i)
         b(i) = b(i)+dot_product(A_I(i1:i2),a(iCol_I(i1:i2)))
      end do

      ! if using a preconditioner we need to convert M to M'=U^-1*L^-1*M
      ! so we need to take b, which is the product of M and a and multiply
      ! by U^-1*L^-1
      if(UsePreconditioner)then
         call Lhepta(       n,1,nTheta,n,b,d_I,e_I,e1_I)
         call Uhepta(.true.,n,1,nTheta,n,b,f_I,f1_I)
      endif
    end subroutine matvec_gimme
    !==========================================================================

  end subroutine get_gimme_potential
  !============================================================================

end module GIMME_potential
!==============================================================================
