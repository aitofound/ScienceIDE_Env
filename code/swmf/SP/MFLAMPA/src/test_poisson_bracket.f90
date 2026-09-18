!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
!===========================TESTS==============================================
module ModDiffusion

  ! Solve the diffusion term in the Parker equation
  ! Revision history:
  ! Prototype:SP/FLAMPA/src/SP_ModDiffusion.f90
  ! Adapted for the use in MFLAMPA (Dist_I is an input paramater,
  ! fixed contributions to M_I in the end points)-D.Borovikov, 2017
  ! Updated (identation, comments):  I.Sokolov, Dec.17, 2017
  use ModUtilities, ONLY: CON_stop
  implicit none

  PRIVATE
  public:: advance_diffusion1, tridiag
  interface advance_diffusion1
     module procedure advance_diffusion_s
     module procedure advance_diffusion_arr
  end interface advance_diffusion1

contains
  !============================================================================
  subroutine advance_diffusion_s(Dt, GcnL, nI, GcnR, Dist_I, F_I, &
       DOuter_I, DInner_I)

    ! Solve the diffusion equation:
    !         f_t-D_outer(D_inner*f_x)_x=0,
    ! with zero Neumann boundary condition, if GcnL=GcnR=0.
    ! With given Dirichlet CB at the left bondary, if GcnL=1.
    ! With given Dirichlet CB at the rightt bondary, if GcnR=1.
    ! The solution is advanced in time using fully implicit scheme.

    real,   intent(in   ):: Dt     ! Time step
    integer,intent(in   ):: GcnL, GcnR ! Number of left and right ghost cells
    integer,intent(in   ):: nI     ! Number of physical cells meshes
    ! Distance to the next mesh
    real,   intent(in   ):: Dist_I(1-GcnL:nI-1+GcnR)
    ! In:sol.to be advanced (including ghost cells)
    ! Out:advanced sol (only physical cells)
    real,   intent(inout):: F_I(1-GcnL:nI+GcnR)

    ! Laplace multiplier.
    real,   intent(in   ):: DOuter_I(nI)
    ! Diffusion coefficient
    real,   intent(in   ):: DInner_I(1-GcnL:nI+GcnR)
    real :: Dt_I(nI)
    !--------------------------------------------------------------------------
    Dt_I = Dt
    call advance_diffusion_arr(Dt_I, GcnL, nI, GcnR, Dist_I, F_I, &
         DOuter_I, DInner_I)
  end subroutine advance_diffusion_s
  !============================================================================
  subroutine advance_diffusion_arr(Dt_I, GcnL, nI, GcnR, Dist_I, F_I, &
       DOuter_I, DInner_I)

    ! Solve the diffusion equation:
    !         f_t-D_outer(D_inner*f_x)_x=0,
    ! with zero Neumann boundary condition. The solution is advanced in time
    ! using fully implicit scheme.

    use ModNumConst, ONLY: cTiny

    integer,intent(in   ):: GcnL, GcnR ! Number of left and right ghost cells
    integer,intent(in   ):: nI     ! Number of physical cells meshes
    real,   intent(in   ):: Dt_I(nI)   ! Time step
    ! Distance to the next mesh
    real,   intent(in   ):: Dist_I(1-GcnL:nI-1+GcnR)
    ! In:sol.to be advanced (including ghost cells)
    ! Out:advanced sol (only physical cells)
    real,   intent(inout):: F_I(1-GcnL:nI+GcnR)

    ! Laplace multiplier.
    real,   intent(in   ):: DOuter_I(nI)
    ! Diffusion coefficient
    real,   intent(in   ):: DInner_I(1-GcnL:nI+GcnR)
    ! Mesh spacing and face spacing.
    real                 :: DsFace_I(nI)

    ! Main, upper, and lower diagonals.
    real, dimension(nI)  :: Main_I, Upper_I, Lower_I, R_I
    integer:: i
    real:: Aux1, Aux2
    !--------------------------------------------------------------------------
    Lower_I = 0.0; Main_I  = 1.0; Upper_I = 0.0; R_I = F_I(1:nI)
    ! Within the framework of finite volume method, the cell
    ! volume is used, which is proportional to  the distance between
    ! the faces bounding the volume with an index, i, which is half of
    ! sum of distance between meshes i-1 and i (i.e. D_I(i-1) and that
    ! between meshes i and i+1 (which is D_I(i)):
    do i=2, nI-1
       DsFace_I(i) = max(0.5*(Dist_I(i) + Dist_I(i-1)),cTiny)
       ! In flux coordinates, the control volume associated with the
       ! given cell has a cross-section equal to (Magnetic Flux)/B,
       ! where the flux is a constant along the magnetic field line,
       ! set to one hereafter. Therefore, the diffusion equation has
       ! a following form:
       ! (DsFace_i/B_i)(f^(n+1) - f^n) = Flux_(i-1/2) - Flux_(i+1/2),
       ! where the particle density flux should be multiplied by the
       ! cross-section area too (magnetic flux factor is one!):
       !  Flux_(i-1/2) = (diffusion coefficient)_(i-1/2)/B_(i-1/2)*&
       !                 (f^(n+1)_(i-1) - f^(n+1)_i),
       !  The multiplier, DsFace_i/B_i, is denoted as DsFace_i/DOuter_i
       !  The face-centered combination,
       ! f^(n+1)_i-Dt*DOuter_I/DsFace_I*(&
       !     DInner_(i+1/2)*(f^(n+1)_(i+1)-f^(n+1)_i)/DsMesh_(i+1)-&
       !     DInner_(i-1/2)*(f^(n+1)_i -f^(n+1)_(i-1)/DsMesh_(i ))=f^n_i
       Aux1 = Dt_I(i)*DOuter_I(i)*0.50*(DInner_I(i  ) + DInner_I(i+1))/&
            (Dist_I(i)*DsFace_I(i))
       Aux2 = Dt_I(i)*DOuter_I(i)*0.50*(DInner_I(i-1) + DInner_I(i  ))/&
            (Dist_I(i-1)*DsFace_I(i))
       Main_I(i)  = Main_I(i) + Aux1 + Aux2
       Upper_I(i) = -Aux1
       Lower_I(i) = -Aux2
    end do
    ! For i=1:
    if(GcnL==0) then
       Aux1 = Dt_I(1)*DOuter_I(1)*0.50*(DInner_I(1) + DInner_I(2))/&
            (Dist_I(1)**2)
       Main_I( 1) = Main_I(1) + Aux1
       Upper_I(1) = -Aux1
    elseif(GcnL>0) then
       DsFace_I(1) = max(0.5*(Dist_I(1) + Dist_I(0)),cTiny)
       Aux1 = Dt_I(1)*DOuter_I(1)*0.50*(DInner_I(1) + DInner_I(2))/&
            (Dist_I(1)*DsFace_I(1))
       Aux2 = Dt_I(1)*DOuter_I(1)*0.50*(DInner_I(0) + DInner_I(1))/&
            (Dist_I(0)*DsFace_I(1))
       Main_I(1)  = Main_I(1) + Aux1 + Aux2
       Upper_I(1) = -Aux1
       Lower_I(1) = -Aux2
       R_I(1) = R_I(1) - Lower_I(1)*F_I(0)
    else
       call CON_stop('SP_advance_diffusion: ghost cell number can be 0 or 1!')
    end if
    ! For i=nI
    if(GcnR==0) then
       Aux2 = Dt_I(nI)*DOuter_I(nI)*0.50*(DInner_I(nI-1) + DInner_I(nI))/&
            (Dist_I(nI-1)**2)
       Main_I( nI) = Main_I(nI) + Aux2
       Lower_I(nI) = -Aux2
    elseif(GcnR>0) then
       DsFace_I(nI) = max(0.5*(Dist_I(nI) + Dist_I(nI-1)),cTiny)
       Aux1 = Dt_I(nI)*DOuter_I(nI)*0.50*(DInner_I(nI) + DInner_I(nI+1))/&
            (Dist_I(nI)*DsFace_I(nI))
       Aux2 = Dt_I(nI)*DOuter_I(nI)*0.50*(DInner_I(nI-1) + DInner_I(nI))/&
            (Dist_I(nI-1)*DsFace_I(nI))
       Main_I(nI)  = Main_I(nI) + Aux1 + Aux2
       Upper_I(nI) = -Aux1
       Lower_I(nI) = -Aux2
       R_I(nI) = R_I(nI) - Upper_I(nI)*F_I(nI+1)
    else
       call CON_stop('SP_advance_diffusion: ghost cell number can be 0 or 1!')
    end if
    call tridiag(nI,Lower_I,Main_I,Upper_I,R_I,F_I(1:nI))
  end subroutine advance_diffusion_arr
  !============================================================================
  subroutine tridiag(n, L_I, M_I, U_I, R_I, W_I)

    ! Solve tri-diagonal system of equations:
    !  ||m_1 u_1  0....        || ||w_1|| ||r_1||
    !  ||l_2 m_2 u_2...        || ||w_2|| ||r_2||
    !  || 0  l_3 m_3 u_3       ||.||w_3||=||r_3||
    !  ||...                   || ||...|| ||...||
    !  ||.............0 l_n m_n|| ||w_n|| ||r_n||
    ! From: Numerical Recipes, Chapter 2.6, p.40.

    ! input parameters
    integer,            intent(in):: n
    real, dimension(n), intent(in):: L_I, M_I ,U_I ,R_I
    ! Output parameters
    real, intent(out):: W_I(n)
    ! Misc
    integer:: j
    real:: Aux, Aux_I(2:n)
    !--------------------------------------------------------------------------
    if (M_I(1)==0.0) then
       write(*,*)' Error in tridiag: M_I(1)=0'
       stop
    end if
    Aux = M_I(1)
    W_I(1) = R_I(1)/Aux
    do j=2,n
       Aux_I(j) = U_I(j-1)/Aux
       Aux = M_I(j)-L_I(j)*Aux_I(j)
       if (Aux == 0.0) then
          write(*,*)'M_I(j), L_I(j), Aux_I(j) = ',&
               M_I(j),L_I(j),Aux_I(j)
          write(*,*)'  For j=',j
          write(*,*)'Tridiag failed'
          stop
       end if
       W_I(j) = (R_I(j)-L_I(j)*W_I(j-1))/Aux
    end do
    do j=n-1,1,-1
       W_I(j) = W_I(j)-Aux_I(j+1)*W_I(j+1)
    end do
  end subroutine tridiag
  !============================================================================
end module ModDiffusion
!==============================================================================
module ModTestPoissonBracket
  use ModPoissonBracket, ONLY: explicit, implicit2
  use ModUtilities, ONLY: CON_stop
  use ModNumConst, ONLY: cTwoPi
  use ModPlotFile, ONLY: save_plot_file
  use ModConst
  implicit none
  ! Pass the VDF from test_dsa_poisson to restart test_dsa_sa_mhd
  integer, parameter :: nX = 1000  !# of Lagrangian points
  integer, parameter :: nP = 40    ! 80  - for Fig5.Right Panel!# momentum bins
  real ::  VDFOld_G(-1:nX+2, -1:nP+2)
contains
  !============================================================================
  subroutine test_poisson_bracket(tFinal)

    real, intent(in) :: tFinal

    ! Misc:
    ! Gyration in a uniform magnetic field, nQ numper of points
    ! for the momentum grid, nP is number of point over azimuthal angle
    integer, parameter::  nQ = 300,  nP = 360

    ! Loop variables
    integer           ::  iQ, iStep

    ! Momentum max and min, in units of mc
    real, parameter   :: qMax = 10.0, qMin = 0.01

    ! Mesh size in \phi
    real, parameter   :: DeltaPhi = cTwoPi/nP
    real :: MomentumRatio, MomentumMin, MomentumMax
    real :: VDF_G(-1:nQ+2, -1:nP+2), Volume_G(0:nQ+1, 0:nP+1)
    real :: Hamiltonian_N(-1:nQ+1, -1:nP+1)
    real :: LogMomentum_I(0:nQ+1), Momentum2_I(-1:nQ+1)
    real :: Time, Dt, Source_C(nQ,nP)
    !--------------------------------------------------------------------------
    MomentumRatio = exp(log(qMax/qMin)/nQ)
    MomentumMax =  qMin/MomentumRatio
    Momentum2_I(-1) = MomentumMax**2
    Hamiltonian_N(-1,:) = Hamiltonian(Momentum2_I(-1))
    do iQ = 0, nQ+1
       MomentumMin = MomentumMax
       MomentumMax = MomentumMin*MomentumRatio
       Momentum2_I( iQ) = MomentumMax**2
       Hamiltonian_N(iQ,:) = Hamiltonian(Momentum2_I(iQ))
       Volume_G(iQ,:) = 0.5*DeltaPhi*&
            (Momentum2_I(iQ) - Momentum2_I(iQ-1))
       LogMomentum_I(iQ)   = 0.50*log10(MomentumMin*MomentumMax)
    end do
    ! Initial distribution function
    VDF_G = 0.0; VDF_G(1:nQ, 172:189) = 1.0; Source_C = 0.0
    ! Compiutation
    Time = 0.0; iStep = 0
    do
       call explicit(nQ, nP, VDF_G, Volume_G, Source_C, &
            Hamiltonian_N,   &
            CFLIn=0.99, DtOut = Dt, IsPeriodicIn_D=[.false.,.true.])
       iStep = iStep +1
       if(Time + Dt >= tFinal)then
          Dt = tFinal - Time
          call explicit(nQ, nP, VDF_G, Volume_G, Source_C, &
               Hamiltonian_N,   &
               DtIn = Dt, IsPeriodicIn_D=[.false.,.true.])
          VDF_G(1:nQ, 1:nP) = VDF_G(1:nQ, 1:nP) + Source_C
          EXIT
       else
          Time = Time + Dt
          VDF_G(1:nQ, 1:nP) = VDF_G(1:nQ, 1:nP) + Source_C
          ! Periodic boundary conditions
          VDF_G(1:nQ,-1:0 ) = VDF_G(1:nQ, nP-1:nP)
          VDF_G(1:nQ, nP+1:nP+2 ) = VDF_G(1:nQ, 1:2)
       end if
    end do
    call save_plot_file(NameFile='test_poisson.out', &
         TypeFileIn='ascii', TimeIn=tFinal, nStepIn = iStep, &
         NameVarIn='-p_y  p_x VDF', &
         CoordMinIn_D=[10.0**LogMomentum_I(1 ), 0.50*DeltaPhi], &
         CoordMaxIn_D=[10.0**LogMomentum_I(nQ), cTwoPi - 0.50*DeltaPhi], &
         Coord1In_I = 10.0**LogMomentum_I(1:nQ), &
         VarIn_II = VDF_G(1:nQ,1:nP) )
  end subroutine test_poisson_bracket
  !============================================================================
  real function Hamiltonian(P2)
    real, intent(in) :: P2 ! momentum squared
    !--------------------------------------------------------------------------
    Hamiltonian = sqrt(1.0 + P2)
  end function Hamiltonian
  !============================================================================
  subroutine test_poisson_2d(tFinal)

    real, intent(in) :: tFinal

    ! Misc:
    ! Harmonic oscillators, nQ is the number of meshes over coordinate,
    ! nP is number of meshes overr generalized momentum
    integer, parameter::  nQ = 300,  nP = 300

    ! Loop variables
    integer           ::  iQ, iP, iStep

    ! Mesh size
    real, parameter   :: DeltaQ = 24.0/nQ, DeltaP = 24.0/nP
    real :: VDF_G(-1:nQ+2, -1:nP+2)
    real :: Volume_G(0:nQ+1, 0:nP+1)
    real :: Hamiltonian_N(-1:nQ+1, -1:nP+1)
    real :: Time, Dt, Source_C(nQ,nP)
    real :: NormL2Init, NormL2, EnergyInit, Energy, qNode, pNode, Q, P
    !--------------------------------------------------------------------------
    ! Control volume, for a uniform rectangular grid
    Volume_G = DeltaQ*DeltaP
    ! Hamiltonian at the nodes
    do iP = -1, nP+1
       pNode = DeltaP*(iP - nP/2)
       do iQ = -1, nQ+1
          qNode = DeltaQ*(iQ - nQ/2)
          Hamiltonian_N(iQ,iP) = hamiltonian(qNode**2 + pNode**2)
       end do
    end do
    ! Initial distribution function
    VDF_G = 0.0
    do iP = -1, nP+2
       P = DeltaP*(iP - nP/2 - 0.50)
       do iQ = -1, nQ+2
          Q = DeltaQ*(iQ - nQ/2 - 0.50)
          if(abs(P) < -Q*tan(cPi/20.0).and.P**2+Q**2<=100)VDF_G(iQ,iP) = 1.0
       end do
    end do
    Source_C = 0.0
    ! Compiutation
    Time = 0.0; iStep = 0
    do
       call explicit(nQ, nP, VDF_G, Volume_G, Source_C, &
            Hamiltonian_N,   &
            CFLIn=0.99, DtOut = Dt)
       iStep = iStep +1
       if(Time + Dt >= tFinal)then
          Dt = tFinal - Time
          call explicit(nQ, nP, VDF_G, Volume_G, Source_C, &
               Hamiltonian_N,   &
               DtIn = Dt)
          VDF_G(1:nQ, 1:nP) = VDF_G(1:nQ, 1:nP) + Source_C
          EXIT
       else
          Time = Time + Dt
          VDF_G(1:nQ, 1:nP) = VDF_G(1:nQ, 1:nP) + Source_C
          write(*,*)Time
       end if
    end do
    call save_plot_file(NameFile='test_poisson2d.out', &
         TypeFileIn='ascii', TimeIn=tFinal, nStepIn = iStep, &
         NameVarIn='-Py    Px VDF', &
         CoordMinIn_D=[-12.0 + 0.50*DeltaQ, -12.0 +  0.50*DeltaP], &
         CoordMaxIn_D=[12.0 - 0.50*DeltaQ, 12.0 - 0.50*DeltaP], &
         VarIn_II = VDF_G(1:nQ,1:nP))

  end subroutine test_poisson_2d
  !============================================================================
  subroutine test_poisson_2d_smooth(tFinal)
    real, intent(in) :: tFinal

    ! Misc:
    ! Anharmonic oscillators, nQ is the number of meshes over coordinate,
    ! nP is number of meshes overr generalized momentum
    integer, parameter::  nQ = 300,  nP = 300

    ! Loop variables
    integer           ::  iQ, iP, iStep

    ! Mesh size
    real, parameter   :: DeltaQ = 24.0/nQ, DeltaP = 24.0/nP
    real :: VDF_G(-1:nQ+2, -1:nP+2), VDFFinal_G(-1:nQ+2,-1:nP+2)
    real :: Plot_VC(3,1:nQ,1:nP)
    real :: Volume_G(0:nQ+1, 0:nP+1)
    real :: Hamiltonian_N(-1:nQ+1, -1:nP+1)
    real :: Time, Dt, Source_C(nQ,nP), Error
    real :: NormL2Init, NormL2, EnergyInit, Energy, qNode, pNode, Q, P
    real, parameter :: WidthX = 6.0, WidthY = 6.0
    ! Control volume, for a uniform rectangular grid
    !--------------------------------------------------------------------------
    Volume_G = DeltaQ*DeltaP
    ! Hamiltonian at the nodes
    do iP = -1, nP+1
       pNode = DeltaP*(iP - nP/2)
       do iQ = -1, nQ+1
          qNode = DeltaQ*(iQ - nQ/2)
          Hamiltonian_N(iQ,iP) = hamiltonian(qNode**2 + pNode**2)
       end do
    end do
    ! Initial distribution function
    VDF_G = 0.0
    do iP = -1, nP+2
       P = DeltaP*(iP - nP/2 - 0.50)
       do iQ = -1, nQ+2
          Q = DeltaQ*(iQ - nQ/2 - 0.50)
          VDF_G(     iQ,iP) = initial_cap(Q,P)
          VDFFinal_G(iQ,iP) = final_cap(  Q,P)
       end do
    end do
    Plot_VC(1,:,:) = VDF_G(     1:nQ,1:nP)
    Plot_VC(2,:,:) = VDFFinal_G(1:nQ,1:nP)
    Source_C = 0.0
    ! Computation
    Time = 0.0; iStep = 0
    do
       call explicit(nQ, nP, VDF_G, Volume_G, Source_C, &
            Hamiltonian_N,   &
            CFLIn=0.50, DtOut = Dt)
       iStep = iStep +1
       if(Time + Dt >= tFinal)then
          Dt = tFinal - Time
          call explicit(nQ, nP, VDF_G, Volume_G, Source_C, &
               Hamiltonian_N,   &
               DtIn = Dt)
          Time = tFinal
          write(*,*) Time
          VDF_G(1:nQ, 1:nP) = VDF_G(1:nQ, 1:nP) + Source_C
          EXIT
       else
          Time = Time + Dt
          VDF_G(1:nQ, 1:nP) = VDF_G(1:nQ, 1:nP) + Source_C
          write(*,*)Time
       end if
    end do
    Plot_VC(3,1:nQ,1:nP) = VDF_G(1:nQ, 1:nP)
    Error = sum(abs(VDF_G(1:nQ,1:nP) - VDFFinal_G(1:nQ,1:nP))*&
         Volume_G(1:nQ,1:nP))/(WidthX*WidthY)
    write(*,*)'Error=',Error
    call save_plot_file(NameFile='test_poisson_2d_smooth.out', &
         TypeFileIn='real8', TimeIn=tFinal, nStepIn = iStep, &
         NameVarIn='-Py    Px   VDFInit VDFAnal VDFFinal  Error', &
         CoordMinIn_D=[-12.0 + 0.50*DeltaQ, -12.0 +  0.50*DeltaP], &
         CoordMaxIn_D=[12.0 - 0.50*DeltaQ, 12.0 - 0.50*DeltaP], &
         ParamIn_I=[Error], &
         VarIn_VII = Plot_VC)
  contains
    !==========================================================================
    real function initial_cap(x,y)
      real, intent(in) :: x,y
      real, parameter :: xCenter = -6.0, yCenter = 0.0
      !------------------------------------------------------------------------
      initial_cap = 0.0
      if(abs(x  - xCenter)<=WidthX/2.and.abs(y - yCenter)<=WidthY/2)then
         initial_cap = cos(cPi*(x - xCenter)/WidthX)**4*&
              cos(cPi*(y - yCenter)/WidthY)**4
      end if
    end function initial_cap
    !==========================================================================
    real function final_cap(x,y)
      real, intent(in) :: x,y
      real :: CosPhi,SinPhi,Phi, P, P2, OmegaT, xInit, yInit
      !------------------------------------------------------------------------
      P2  = x**2 + y**2
      OmegaT = tFinal/hamiltonian(P2)
      P  = sqrt(P2)
      CosPhi = x/P; SinPhi = y/P
      if(SinPhi>=0.0)then
         Phi =  acos(CosPhi)
      else
         Phi = -acos(CosPhi)
      end if
      xInit = P*cos(Phi + OmegaT)
      yInit = P*sin(Phi + OmegaT)
      final_cap = initial_cap(xInit,yInit)
    end function final_cap
    !==========================================================================
  end subroutine test_poisson_2d_smooth
  !============================================================================
  subroutine test_energy_conservation(tFinal)

    real, intent(in) :: tFinal

    ! Misc:
    ! Harmonic oscillators, nQ is the number of meshes over coordinate,
    ! nP is number of meshes overr generalized momentum
    integer, parameter::  nQ = 360,  nP = 360

    ! Loop variables
    integer           ::  iQ, iP, iStep

    ! Mesh size
    real, parameter   :: DeltaQ = 24.0/nQ, DeltaP = 24.0/nP
    real :: VDF_G(-1:nQ+2, -1:nP+2),VDFInitial_C(nQ,nP),PlotVar_VC(2,nQ,nP)
    real :: Volume_G(0:nQ+1, 0:nP+1)
    real :: Hamiltonian_N(-1:nQ+1, -1:nP+1)
    real :: Energy_C(nQ, nP)
    real :: Time, Dt, Source_C(nQ,nP)
    real :: NormL1Init, NormL1, EnergyInit, Energy, qNode, pNode, Q, P
    real, parameter :: CFL =0.99
    real, parameter:: pWidth = 10.0, qWidth = 1.0
    integer, parameter:: nPower = 4
    logical, parameter:: IsSmooth = .false.
    ! Control volume, for a uniform rectangular grid
    !--------------------------------------------------------------------------
    Volume_G = DeltaQ*DeltaP
    ! Hamiltonian at the nodes
    do iP = -1, nP+1
       pNode = DeltaP*(iP - nP/2)
       do iQ = -1, nQ+1
          qNode = DeltaQ*(iQ - nQ/2)
          Hamiltonian_N(iQ,iP) = 0.5*(qNode**2 + pNode**2)
       end do
    end do
    ! Energy at the cell centers
    do iP = 1, nP
       P = DeltaP*(iP - nP/2 - 0.50)
       do iQ = 1, nQ
          Q = DeltaQ*(iQ - nQ/2  - 0.50)
          Energy_C(iQ,iP) = 0.5*(Q**2 + P**2)
       end do
    end do
    ! Initial distribution function
    VDFInitial_C = 0.0
    do iP = 1, nP; do iQ = 1, nQ
       P = DeltaP*(iP - nP/2 - 0.5)
       Q = DeltaQ*(iQ - nQ/2 - 0.5)
       if(abs(Q) < qWidth .and. abs(P) < pWidth)then
          if(IsSmooth)then
             VDFInitial_C(iQ,iP) = cos(0.5*cPi*Q/qWidth)**nPower &
                  *                cos(0.5*cPi*P/pWidth)**nPower
          else
             VDFInitial_C(iQ,iP) = 1.0
          endif
       end if
    end do; end do

    VDF_G = 0.0; VDF_G(1:nQ, 1:nP) = VDFInitial_C
    Source_C = 0.0
    ! Initial NormL2 and Energy
    NormL1Init = sum(VDFInitial_C)
    EnergyInit = sum(VDFInitial_C*Energy_C)
    ! Compiutation
    Time = 0.0; iStep = 0
    do
       call explicit(nQ, nP, VDF_G, Volume_G, Source_C, &
            Hamiltonian_N,   &
            CFLIn=CFL, DtOut = Dt)
       iStep = iStep +1
       if(Time + Dt >= tFinal)then
          Dt = tFinal - Time
          call explicit(nQ, nP, VDF_G, Volume_G, Source_C, &
               Hamiltonian_N,   &
               DtIn = Dt)
          VDF_G(1:nQ, 1:nP) = VDF_G(1:nQ, 1:nP) + Source_C
          NormL1 = sum( abs(VDF_G(1:nQ,1:nP) - VDFInitial_C) )
          Energy = sum(VDF_G(1:nQ,1:nP)*Energy_C)
          write(*,*)tFinal, &
               NormL1/NormL1Init,&
               Energy/EnergyInit - 1.0
          EXIT
       else
          Time = Time + Dt
          VDF_G(1:nQ, 1:nP) = VDF_G(1:nQ, 1:nP) + Source_C
          NormL1 = sum( abs(VDF_G(1:nQ,1:nP) - VDFInitial_C) )
          Energy = sum(VDF_G(1:nQ,1:nP)*Energy_C)
          write(*,*)Time, &
               NormL1/NormL1Init,&
               Energy/EnergyInit - 1.0
       end if
    end do
    PlotVar_VC(1,:,:) = VDF_G(1:nQ,1:nP)
    PlotVar_VC(2,:,:) = VDFInitial_C
    call save_plot_file(NameFile='test_energy.out', &
         TypeFileIn='real8', TimeIn=tFinal, nStepIn = iStep, &
         NameVarIn='Q    P VDF VDFIni ErrorL1 EnergyDefect', &
         ParamIn_I=[NormL1/NormL1Init, Energy/EnergyInit - 1.0],&
         CoordMinIn_D=[-12.0 + 0.5*DeltaQ, -12.0 + 0.5*DeltaP], &
         CoordMaxIn_D=[12.0 - 0.5*DeltaQ, 12.0 - 0.5*DeltaP], &
         VarIn_VII = PlotVar_VC)

  end subroutine test_energy_conservation
  !============================================================================
  subroutine test_in_action_angle(tFinal)

    real, intent(in) :: tFinal

    ! Misc:
    ! Harmonic oscillators, nQ is the number of meshes over coordinate,
    ! nP is number of meshes overr generalized momentum
    integer, parameter::  nJ = 300,  nPhi = 360

    ! Loop variables
    integer           ::  iJ, iPhi, iStep

    ! Mesh size
    ! Momentum max and min
    real, parameter   :: JMax = 12.0, JMin = 0.01

    ! Mesh size in \phi
    real, parameter   :: DeltaPhi = cTwoPi/nPhi
    real :: MomentumRatio, MomentumMin, MomentumMax
    real :: VDF_G(-1:nJ+2, -1:nPhi+2), Volume_G(0:nJ+1, 0:nPhi+1)
    real :: VDFInitial_C(nJ,nPhi), PlotVar_VC(2,nJ,nPhi)

    real :: Hamiltonian_N(-1:nJ+1, -1:nPhi+1)
    real :: LogMomentum_I(0:nJ+1), Momentum2_I(-1:nJ+1)

    real :: Energy_C(nJ, nPhi)
    real :: Time, Dt, Source_C(nJ,nPhi)
    real :: NormL2Init, NormL2, EnergyInit, Energy, J, Phi, Q, P
    !--------------------------------------------------------------------------
    MomentumRatio = exp(log(JMax/JMin)/nJ)
    MomentumMax =  JMin/MomentumRatio
    Momentum2_I(-1) = MomentumMax**2
    Hamiltonian_N(-1,:) = 0.50*Momentum2_I(-1)
    do iJ = 0, nJ+1
       MomentumMin = MomentumMax
       MomentumMax = MomentumMin*MomentumRatio
       Momentum2_I( iJ) = MomentumMax**2
       Hamiltonian_N(iJ,:) = 0.50*Momentum2_I(iJ)
       Volume_G(iJ,:) = 0.5*DeltaPhi*&
            (Momentum2_I(iJ) - Momentum2_I(iJ-1))
       LogMomentum_I(iJ)   = 0.50*log10(MomentumMin*MomentumMax)
    end do
    ! Energy at the cell centers,  and initial distribution
    VDFInitial_C = 0.0
    do iPhi = 1, nPhi
       Phi = DeltaPhi*(iPhi - 0.50)
       do iJ = 1, nJ
          J = (Momentum2_I(iJ)*Momentum2_I(iJ-1))**0.250
          Energy_C(iJ,iPhi) = 0.5*J**2
          Q = J*cos(Phi)
          P = J*sin(Phi)
          ! Initial distribution function
          if(Q < -1.0.or.Q > 1.0.or.P < -10.0.or.P > 10.0)CYCLE
          VDFInitial_C(iJ,iPhi) = 1.0
       end do
    end do
    VDF_G = 0.0; VDF_G(1:nJ, 1:nPhi) = VDFInitial_C
    ! Periodic boundary conditions
    VDF_G(1:nJ,-1:0 )   = VDF_G(1:nJ, nPhi-1:nPhi)
    VDF_G(1:nJ, nPhi+1:nPhi+2 ) = VDF_G(1:nJ, 1:2)

    Source_C = 0.0
    ! Initial NormL2 and Energy
    NormL2Init = sum(VDFInitial_C**2*Volume_G(1:nJ,1:nPhi))
    EnergyInit = sum(VDFInitial_C*Energy_C*Volume_G(1:nJ,1:nPhi))
    ! Compiutation
    Time = 0.0; iStep = 0
    ! write(*,*)'Time NormL2/NormL2Init Energy/EnergyInit-1'
    do
       call explicit(nJ, nPhi, VDF_G, Volume_G, Source_C, &
            Hamiltonian_N,   &
            CFLIn=0.99, DtOut = Dt)
       iStep = iStep +1
       if(Time + Dt >= tFinal)then
          Dt = tFinal - Time
          call explicit(nJ, nPhi, VDF_G, Volume_G, Source_C, &
               Hamiltonian_N,   &
               DtIn = Dt)
          VDF_G(1:nJ, 1:nPhi) = VDF_G(1:nJ, 1:nPhi) + Source_C
          NormL2 = sum( (VDF_G(1:nJ,1:nPhi) - VDFInitial_C)**2 &
               *Volume_G(1:nJ,1:nPhi))
          Energy = sum(VDF_G(1:nJ,1:nPhi)*Energy_C &
               *Volume_G(1:nJ,1:nPhi))
          write(*,*)tFinal, &
               NormL2/NormL2Init,&
               Energy/EnergyInit - 1.0
          EXIT
       else
          Time = Time + Dt
          VDF_G(1:nJ, 1:nPhi) = VDF_G(1:nJ, 1:nPhi) + Source_C
          ! Periodic boundary conditions
          VDF_G(1:nJ,-1:0 )   = VDF_G(1:nJ, nPhi-1:nPhi)
          VDF_G(1:nJ, nPhi+1:nPhi+2 ) = VDF_G(1:nJ, 1:2)
          NormL2 = sum( (VDF_G(1:nJ,1:nPhi) - VDFInitial_C)**2 &
               *Volume_G(1:nJ,1:nPhi))
          Energy = sum(VDF_G(1:nJ,1:nPhi)*Energy_C &
               *Volume_G(1:nJ,1:nPhi))
          write(*,*)Time, &
               NormL2/NormL2Init,&
               Energy/EnergyInit - 1.0
       end if
    end do
    PlotVar_VC(1,:,:) = VDF_G(1:nJ,1:nPhi)
    PlotVar_VC(2,:,:) = VDFInitial_C
    call save_plot_file(NameFile='test_action_angle.out', &
         TypeFileIn='real8', TimeIn=tFinal, nStepIn = iStep, &
         NameVarIn='Q    P VDF VDFIni ErrorL2 EnergyDefect', &
         CoordMinIn_D=[10.0**LogMomentum_I(1 ), 0.50*DeltaPhi],&
         CoordMaxIn_D=[10.0**LogMomentum_I(nJ), cTwoPi - 0.50*DeltaPhi], &
         ParamIn_I=[NormL2/NormL2Init, Energy/EnergyInit - 1.0],&
         Coord1In_I = 10.0**LogMomentum_I(1:nJ), &
         VarIn_VII = PlotVar_VC)
  end subroutine test_in_action_angle
  !============================================================================
  subroutine test_dsa_poisson

    use ModDiffusion

    real,    parameter :: pMax = 100, pMin = 1
    real,    parameter :: tFinal = 600.00
    real,    parameter :: DtTrial = 1.0
    real ::  DtInv = 1/DtTrial
    real ::  MomentumRatio, MomentumMin, MomentumMax
    real ::  VDF_G(-1:nX+2, -1:nP+2)
    real ::  Volume_G(0:nX+1, 0:nP+1), VolumeNew_G(0:nX+1, 0:nP+1)
    real ::  VolumeX_I(0:nX+1), VolumeNewX_I(0:nX+1)
    real ::  VolumeP_I(0:nP+1)
    real ::  dVolumeDt_G(0:nX+1, 0:nP+1), dVolumeXDt_G(0:nX+1)
    real ::  DOuter_I(nX) = 1.0, dInner_I(0:nX+1) = 20.0 ! 20.0 For Fig5
    real ::  dHamiltonian02_FY(0:nX+1, -1:nP+1)
    real ::  LogMomentum_I(0:nP+1)     ! Cell centered, for plots
    real ::  Momentum3_I(-1:nP+1)
    real ::  Time = 0.0, Dt, DtNext, Source_C(nX,nP)
    real ::  Coord_I(-1:nX+2)  ! Time-dependent coordinate of a mesh
    real ::  Dist_I(-1:nX+1)   ! Distance from mesh i to mesh i+1

    ! Loop variables
    integer :: iX, iP, iStep
    !--------------------------------------------------------------------------
    Source_C(nX,nP) = 0.0

    ! Logarithmic grid in momentum. pMin is the value at the left face
    ! of the first physical cell, pMax is the value at the right face
    ! off the last cell. The momentum ratio at the faces of each cell is:
    MomentumRatio   = exp(log(pMax/pMin)/nP)

    ! The momentum at the left fface of 0th ghost cell is:
    MomentumMax     = pMin/MomentumRatio

    ! Calculate the generalized variable p^3/3 at each face, starting from
    ! the left fface of 0th cell
    Momentum3_I(-1) = MomentumMax**3/3

    ! For cells from 0 to nP+1 calculate face values and volume factor:
    do iP = 0, nP + 1
       MomentumMin = MomentumMax
       MomentumMax = MomentumMin*MomentumRatio
       Momentum3_I(iP) = MomentumMax**3/3
       VolumeP_I(iP)   = Momentum3_I(iP) - Momentum3_I(iP-1)
       ! Only for visualization log10 of the cell centered momentum
       LogMomentum_I(iP) = 0.50*log10(MomentumMin*MomentumMax)
    end do
    VDF_G = 1.0e-8; VDF_G(:,1) = 1/VolumeP_I(1)

    Time = 0.0; iStep = 0; DtNext = DtTrial
    call update_coords(Time)
    do
       Volume_G = VolumeNew_G
       VolumeX_I= VolumeNewX_I
       Dt = min(DtNext, tFinal - Time); DtInv = 1/Dt
       call update_coords(Time + Dt)
       dVolumeDt_G  = DtInv*(VolumeNew_G  - Volume_G)
       dVolumeXDt_G = DtInv*(VolumeNewX_I - VolumeX_I)
       do iP = -1, nP+1
          dHamiltonian02_FY(:,iP) = - Momentum3_I(iP)*dVolumeXDt_G
       end do
       call explicit(nX, nP, VDF_G, Volume_G, Source_C, &
            dHamiltonian02_FY=dHamiltonian02_FY,        &
            dVolumeDt_G = dVolumeDt_G,                  &
            DtIn=Dt,           & ! Input time step, which may be reduced
            CFLIn=0.98,        & ! Input CFL to calculate next time step
            DtOut=DtNext)        ! Calculation of next time step
       ! Correct final volumes, if Dt had been reduced
       VolumeNew_G  = Volume_G  + Dt*dVolumeDt_G
       VolumeNewX_I = VolumeX_I + Dt*dVolumeXDt_G
       iStep = iStep +1
       VDF_G(1:nX, 1:nP) = VDF_G(1:nX, 1:nP) + Source_C
       do iP =1, nP
          call advance_diffusion1(Dt,0,nX,1,Dist_I(1:nX),VDF_G(1:nX+1,&
               iP),DOuter_I(1:nX),DInner_I(1:nX+1))
       end do
       Time = Time + Dt
       if(Time> tFinal - 1.0e-8*DtNext)EXIT
       VDF_G(1:nX,-1:0 ) = 1.0e-8
       VDF_G(1:nX,nP+1:nP+2) = 1.0e-8
       VDF_G( 0,      :) = VDF_G(1,       :)
       VDF_G(-1,      :) = VDF_G(1,       :)
       VDF_G(nX+1:nX+2,:) = 1.0e-8
       VDF_G(nX+1:nX+2,1) = 1/VolumeP_I(1)
    end do
    call save_plot_file(NameFile='test_dsa_poisson.out',       &
         TypeFileIn='ascii', TimeIn=tFinal, nStepIn = iStep,   &
         NameVarIn='LogMomentum VDF'  ,                        &
         CoordMinIn_D=[ LogMomentum_I(1 ) ],                   &
         CoordMaxIn_D=[ LogMomentum_I(nP) ],                   &
         StringFormatIn = '(2F16.9)',                          &
         Coord1In_I = LogMomentum_I(1:nP),                     &
         VarIn_I = alog10(VDF_G(500,1:nP)))
    VDFOld_G = VDF_G
  contains
    !==========================================================================
    subroutine update_coords(Time)
      real, intent(in):: Time

      ! Effective width of the shock wave, in cell size
      integer, parameter:: nWidth = 1

      ! terms in the functions, describing the
      ! lagrangian point motion
      real, parameter:: cQuad = 0.375/nWidth, cConst = 0.375*nWidth
      ! Loop variables
      integer :: iX, iP
      real    :: CoordLagr
      ! Shock wave with the copmtression ratio of 4, with width ~1
      !------------------------------------------------------------------------
      do iX = -1, nX +2
         CoordLagr = iX - 0.50 -Time
         if(CoordLagr>=0.0) then
            Coord_I(iX) = CoordLagr + Time
         elseif(CoordLagr>=-real(nWidth)) then
            Coord_I(iX) = CoordLagr + Time + cQuad*CoordLagr**2
         else
            Coord_I(iX) = 0.250*CoordLagr -cConst + Time
         end if
      end do
      Dist_I(-1:nX+1) = Coord_I(0:nX+2) - Coord_I(-1:nX+1)
      VolumeNewX_I(0:nX+1) = 0.50*(Dist_I(-1:nX) + Dist_I(0:nX+1))
      do iP = 0, nP+1
         VolumeNew_G(0:nX+1,iP) = VolumeP_I(iP)*VolumeNewX_I(0:nX+1)
      end do
    end subroutine update_coords
    !==========================================================================
  end subroutine test_dsa_poisson
  !============================================================================
  subroutine test_dsa_sa_mhd

    use ModDiffusion

    real,    parameter :: pMax = 100, pMin = 1
    integer,    parameter :: nStep = 1000
    real ::  MomentumRatio, MomentumMin, MomentumMax
    real ::  VDF_G(-1:nX+2, -1:nP+2), Dt_C(nX,nP)
    real ::  Hamiltonian_N(-1:nX+1, -1:nP+1)
    real ::  Volume_G(0:nX+1, 0:nP+1)
    real ::  VolumeX_I(-1:nX+2)
    real ::  VolumeP_I(0:nP+1)
    real ::  DOuter_I(nX) = 1.0, dInner_I(0:nX+1) = 20.0
    real ::  LogMomentum_I(0:nP+1)     ! Cell centered, for plots
    real ::  Momentum3_I(-1:nP+1)
    real ::  Source_C(nX,nP)
    real ::  Coord_I(-1:nX+2)  ! Time-dependent coordinate of a mesh
    real ::  Dist_I(-1:nX+1)   ! Distance from mesh i to mesh i+1

    ! Loop variables
    integer :: iX, iP, iStep
    !--------------------------------------------------------------------------
    Source_C(nX,nP) = 0.0
    ! Logarithmic grid in momentum. pMin is the value at the left face
    ! of the first physical cell, pMax is the value at the right face
    ! off the last cell. The momentum ratio at the faces of each cell is:
    MomentumRatio   = exp(log(pMax/pMin)/nP)
    ! The momentum at the left fface of 0th ghost cell is:
    MomentumMax     = pMin/MomentumRatio
    ! Calculate the generalized variable p^3/3 at each face, starting from
    ! the left fface of 0th cell
    Momentum3_I(-1) = MomentumMax**3/3
    ! For cells from 0 to nP+1 calculate face values and volume factor:
    do iP = 0, nP + 1
       MomentumMin = MomentumMax
       MomentumMax = MomentumMin*MomentumRatio
       Momentum3_I(iP) = MomentumMax**3/3
       VolumeP_I(iP)   = Momentum3_I(iP) - Momentum3_I(iP-1)
       ! Only for visualization log10 of the cell centered momentum
       LogMomentum_I(iP) = 0.50*log10(MomentumMin*MomentumMax)
    end do
    VDF_G = VDFOld_G

    call update_coords(600.0)
    do iStep = 1, nStep
       call explicit(nX, nP, VDF_G, Volume_G, Source_C, &
            Hamiltonian12_N=Hamiltonian_N, CFLIn=0.98,  &
            IsSteadyState=.true., DtOut_C=Dt_C)

       VDF_G(1:nX, 1:nP) = VDF_G(1:nX, 1:nP) + Source_C
       do iP =1, nP
          call advance_diffusion1(Dt_C(:,iP),0,nX,1,Dist_I(1:nX),VDF_G(1:nX+1,&
               iP),DOuter_I(1:nX),DInner_I(1:nX+1))
       end do
       ! Time = Time + Dt
       VDF_G(1:nX,-1:0 ) = 1.0e-8
       VDF_G(1:nX,nP+1:nP+2) = 1.0e-8
       VDF_G( 0,      :) = VDF_G(1,       :)
       VDF_G(-1,      :) = VDF_G(1,       :)
       VDF_G(nX+1:nX+2,1) = 1/VolumeP_I(1)
    end do
    call save_plot_file(NameFile='test_dsa_sa_mhd.out', &
         TypeFileIn='ascii', nStepIn = nStep, &
         NameVarIn='LogMomentum VDF'  ,                  &
         CoordMinIn_D=[ LogMomentum_I(1 ) ],             &
         CoordMaxIn_D=[ LogMomentum_I(nP) ],             &
         StringFormatIn = '(2F16.9)',                    &
         Coord1In_I = LogMomentum_I(1:nP),               &
         VarIn_I = alog10(VDF_G(500,1:nP)))
  contains
    !==========================================================================
    subroutine update_coords(Time)

      real, intent(in):: Time

      ! Effective width of the shock wave, in cell size
      integer, parameter:: nWidth = 1

      ! terms in the functions, describing the
      ! lagrangian point motion
      real, parameter:: cQuad = 0.375/nWidth, cConst = 0.375*nWidth
      ! Loop variables
      integer :: iX, iP
      real    :: CoordLagr
      ! Shock wave with the copmtression ratio of 4, with width ~1
      !------------------------------------------------------------------------
      do iX = -1, nX +2
         CoordLagr = iX - 0.50 -Time
         if(CoordLagr>=0.0)then
            Coord_I(iX) = CoordLagr + Time
         elseif(CoordLagr>=-real(nWidth))then
            Coord_I(iX) = CoordLagr + Time + cQuad*CoordLagr**2
         else
            Coord_I(iX) = 0.250*CoordLagr -cConst + Time
         end if
      end do
      Dist_I(-1:nX+1) = Coord_I(0:nX+2) - Coord_I(-1:nX+1)
      VolumeX_I(0:nX+1) = 0.50*(Dist_I(-1:nX) + Dist_I(0:nX+1))
      VolumeX_I(-1)  = Dist_I(-1); VolumeX_I(nX+2) =  Dist_I(nX+1)
      do iP = 0, nP+1
         Volume_G(0:nX+1,iP) = VolumeP_I(iP)*VolumeX_I(0:nX+1)
      end do
      do iP = -1, nP+1
         Hamiltonian_N(-1:nX+1, iP)= -0.50*Momentum3_I(iP)*&
              (VolumeX_I(-1:nX+1) + VolumeX_I(0:nX+2))
      end do

    end subroutine update_coords
    !==========================================================================
  end subroutine test_dsa_sa_mhd
  !============================================================================
  subroutine test_dsa_impl
    real,    parameter :: pMax = 100, pMin = 1
    integer,    parameter :: nStep = 1000
    real ::  MomentumRatio, MomentumMin, MomentumMax
    real ::  VDF_G(-1:nX+2, -1:nP+2), Dt_C(nX,nP)
    real ::  Hamiltonian_N(-1:nX+1, -1:nP+1)
    real ::  Volume_G(0:nX+1, 0:nP+1)
    real ::  VolumeX_I(-1:nX+2)
    real ::  VolumeP_I(0:nP+1)
    real ::  DOuter_I(nX) = 1.0, dInner_I(0:nX+1) = 20.0, Error
    real ::  Diffusion_FX(0:nX,1:nP)
    real ::  LogMomentum_I(0:nP+1)     ! Cell centered, for plots
    real ::  Momentum3_I(-1:nP+1)
    real ::  Coord_I(-1:nX+2)  ! Time-dependent coordinate of a mesh
    real ::  Dist_I(-1:nX+1)   ! Distance from mesh i to mesh i+1

    ! Loop variables
    integer :: iX, iP, iStep
    !--------------------------------------------------------------------------

    ! Logarithmic grid in momentum. pMin is the value at the left face
    ! of the first physical cell, pMax is the value at the right face
    ! off the last cell. The momentum ratio at the faces of each cell is:
    MomentumRatio   = exp(log(pMax/pMin)/nP)
    ! The momentum at the left fface of 0th ghost cell is:
    MomentumMax     = pMin/MomentumRatio
    ! Calculate the generalized variable p^3/3 at each face, starting from
    ! the left fface of 0th cell
    Momentum3_I(-1) = MomentumMax**3/3
    ! For cells from 0 to nP+1 calculate face values and volume factor:
    do iP = 0, nP + 1
       MomentumMin = MomentumMax
       MomentumMax = MomentumMin*MomentumRatio
       Momentum3_I(iP) = MomentumMax**3/3
       VolumeP_I(iP)   = Momentum3_I(iP) - Momentum3_I(iP-1)
       ! Only for visualization log10 of the cell centered momentum
       LogMomentum_I(iP) = 0.50*log10(MomentumMin*MomentumMax)
    end do
    ! Initial condition for the distribution function
    VDF_G = VDFOld_G
    call update_coords(600.0)
    do iStep = 1, nStep
       call implicit2(nX, nP, VDF_G, Volume_G, &
            Hamiltonian12_N=Hamiltonian_N,  &
            Diffusion_FX=Diffusion_FX, CFLIn=98.0, &
            ErrorOut=Error)
       write(*,*)'iStep, Error=', iStep, Error
       ! IsTetradiagNoUu=.true.)
       VDF_G(1:nX,-1:0 ) = 1.0e-8
       VDF_G(1:nX,nP+1:nP+2) = 1.0e-8
       VDF_G( 0,      :) = VDF_G(1,       :)
       VDF_G(-1,      :) = VDF_G(1,       :)
       VDF_G(nX+1:nX+2,1) = 1/VolumeP_I(1)
    end do
    call save_plot_file(NameFile='test_dsa_impl.out', &
         TypeFileIn='ascii', nStepIn = nStep, &
         NameVarIn='LogMomentum VDF'  ,                  &
         CoordMinIn_D=[ LogMomentum_I(1 ) ],             &
         CoordMaxIn_D=[ LogMomentum_I(nP) ],             &
         StringFormatIn = '(2F16.9)',                    &
         Coord1In_I = LogMomentum_I(1:nP),               &
         VarIn_I = alog10(VDF_G(500,1:nP)))
  contains
    !==========================================================================
    subroutine update_coords(Time)

      real, intent(in):: Time

      ! Effective width of the shock wave, in cell size
      integer, parameter:: nWidth = 1

      ! terms in the functions, describing the
      ! lagrangian point motion
      real, parameter:: cQuad = 0.375/nWidth, cConst = 0.375*nWidth
      ! Loop variables
      integer :: iX, iP
      real    :: CoordLagr, DiffusionX_I(0:nX)
      ! Shock wave with the copmtression ratio of 4, with width ~1
      !------------------------------------------------------------------------
      ! Coord_I is the cell-centered coordinate
      do iX = -1, nX +2
         CoordLagr = iX - 0.50 -Time
         if(CoordLagr>=0.0)then
            Coord_I(iX) = CoordLagr + Time
         elseif(CoordLagr>=-real(nWidth))then
            Coord_I(iX) = CoordLagr + Time + cQuad*CoordLagr**2
         else
            Coord_I(iX) = 0.250*CoordLagr -cConst + Time
         end if
      end do
      ! Dist is the distance to the next cell center
      Dist_I(-1:nX+1) = Coord_I(0:nX+2) - Coord_I(-1:nX+1)
      VolumeX_I(0:nX+1) = 0.50*(Dist_I(-1:nX) + Dist_I(0:nX+1))
      VolumeX_I(-1)  = Dist_I(-1); VolumeX_I(nX+2) =  Dist_I(nX+1)
      do iP = 0, nP+1
         Volume_G(0:nX+1,iP) = VolumeP_I(iP)*VolumeX_I(0:nX+1)
      end do
      do iP = -1, nP+1
         Hamiltonian_N(-1:nX+1, iP)= -0.50*Momentum3_I(iP)*&
              (VolumeX_I(-1:nX+1) + VolumeX_I(0:nX+2))
      end do
      ! Get diffusion coefficients
      ! Flot (no-flux) BC at the left boundary:
      DiffusionX_I(0) = 0.0
      do iX = 1, nX
         DiffusionX_I(iX) = &
              DOuter_I(iX)*0.50*(DInner_I(iX) + DInner_I(iX+1))/Dist_I(iX)
      end do
      do iP = 1, nP
         Diffusion_FX(:,iP) =  VolumeP_I(iP)*DiffusionX_I
      end do
    end subroutine update_coords
    !==========================================================================
  end subroutine test_dsa_impl
  !============================================================================
end module ModTestPoissonBracket
!==============================================================================
module ModTestMultiPoisson
  ! Solve energetic particle transport (focused transport equation) via the
  ! numerical particle-conserved scheme with multi Poisson bracket:
  !   {f,H_1}_t,p^3/3 + {f,H_2}_s_L,\mu + {f,H_3}_p^3/3,\mu = 0
  ! Dimensionless parameters: Energy: GeV, Speed: c Mass: GeV/c^2 Length: c*sec
  ! In the code, label Q: s_L  P: \mu  R: ln(P^3/3)  NLoop: t
  ! this code consider only the most simple equation
  use ModPoissonBracket, ONLY: explicit
  use ModUtilities, ONLY: CON_stop
  use ModNumConst, ONLY: cTwoPi, cPi
  use ModPlotFile, ONLY: save_plot_file
  use ModConst, ONLY: cRmeProton, cGeV, &
       cMu, cAtomicMass, rSun, cLightSpeed
  implicit none
  ! Grid setups
  integer, parameter :: nQ = 160   ! Number of grid of s_L
  integer, parameter :: nP = 20    ! Number of grid of \mu = cos(pitch angle)
  integer, parameter :: nR = 60    ! Number of grid of ln(p^3/3) axis
  ! Unit setups
  real, parameter :: cRmeProtonGeV = cRmeProton/cGeV ! Proton mass: GeV/c^2
  real, parameter :: CoeffMuToxx = 27.0/14.0         ! DMuMu => Dxx constant
  ! Physical parameter reading setups
  real, parameter :: tEachFile = 100.0    ! Time range of each input file
  ! Number of variables in each line of the input files
  integer, parameter :: nVar = 14
  ! Position of the variables in the input line
  integer, parameter :: x_ = 2, y_ = 3, z_ = 4,    &
       rho_ = 5, ux_ = 7, uy_ = 8, uz_ = 9,        &
       Bx_ = 10, By_= 11, Bz_ = 12, Wave1_ = 13, Wave2_ = 14
  ! OLD and NEW variables from raw data files
  ! Raw data imported from MHD files
  real :: RawData1_II(nQ, nVar), RawData2_II(nQ, nVar)
  ! \Deltas/b, 1/(2B), ln(B\deltas^2) at Old time
  real :: DeltaSOverBOld_C(nQ), InvBOld_C(nQ), LnBDeltaS2Old_C(nQ)
  ! \Deltas/b, 1/(2B), ln(B\deltas^2) at New time
  real :: DeltaSOverBNew_C(nQ), InvBNew_C(nQ), LnBDeltaS2New_C(nQ)
  ! Midpoint for to consecutive points, \deltas
  real :: MidPoint_ID(nQ-1, x_:z_), DeltaS_I(nQ), DeltaSface_I(1:nQ-1)

contains
  !============================================================================
  subroutine test_multi_poisson(tFinal, TypeScatter)
    ! Test for multiple Poisson bracket

    real, intent(in) :: tFinal            ! Overall final Time
    ! No scatter, or advect and scatter, or only diffusion scatter
    character(LEN=*), intent(in) :: TypeScatter

    ! Local VARs:
    real, parameter  :: cMinVal = 1.0e-8  ! We set Min Value of VDF as 1.0e-8
    ! ------------ File ------------
    ! Unit for field line files: Old and new field lines
    integer, parameter :: nInFileOld = 8, nInFileNew = 9
    ! Stride of line of the data reading
    integer, parameter :: nStride = 25
    ! Lagr index that is gaped for field line at each time
    integer, parameter :: iLagrIdSkip = 20
    ! Read the redundant data
    real               :: GapData
    ! Heliocentric distance, for visualization
    real               :: HelioDist_I(nQ)
    ! Number of outputs:
    integer, parameter :: nOutputFile = 100
    ! The number index of output files in character
    character(LEN=3)   :: NameFileSuffix
    ! The data calculated directly from RawData (MHD data)
    real :: DeltaSOverB_C(nQ), dDeltaSOverBDt_C(nQ),  &
         InvB_C(nQ), bDuDt_C(nQ), dLnBDeltaS2Dt_C(nQ)

    ! ------------ Pitch angle ------------
    ! Now we have the parameters for "Q"-coordinate (s_L)
    ! We then consider the "P"-coordinate (mu)
    ! It is the cosine value of the pitch angle, varying from -1 to 1
    ! Here we calculate \delta\mu for each grid along the \mu axis
    real :: DeltaMu = 2.0/nP

    ! ------------ Energy and momentum ------------
    ! Last but not least, we focus on the "R"-coordinate (p^3/3)
    ! Min and Max value of particle kinetic energy, in units of GeV
    real, parameter :: EkMin = 1.0e-3, EkMax = 1.0
    ! Ln(P^3/3)_min, Ln(P^3/3)_max, \deltaLn(P^3/3)
    real            :: LnP3min, LnP3max, DeltaLnP3
    ! Momentum and velocity at the center of each cell
    real            :: Momentum_I(nR),  Velocity_I(nR)
    ! Momentum at the face of each cell and their distance \delta(P^3/3)
    real            :: MomentumFace_I(0:nR), DeltaP3_I(nR)
    ! Energy in unit of KeV, for output
    real            :: Energy_I(nR)

    ! ------------ Hamiltonian functions ------------
    ! Poisson bracket with regard to the first and second vars
    ! considering the case when there are more than one Poisson bracket
    ! and when there is a Poisson bracket with respect to the time
    real :: DeltaHamiltonian1_N(0:nQ+1, 0:nP+1, -1:nR+1)
    real :: Hamiltonian2_N(-1:nQ+1, -1:nP+1, 0:nR+1)
    real :: Hamiltonian3_N(0:nQ+1, -1:nP+1, -1:nR+1)

    ! ------------ Control volume ------------
    real :: Volume_G(0:nQ+1, 0:nP+1, 0:nR+1)    ! Control volume (phase)
    real :: dVolumeDt_G(0:nQ+1, 0:nP+1, 0:nR+1) ! Derivative regarding to time

    ! ------------ Distribution functions ------------
    real :: VDF_G(-1:nQ+2, -1:nP+2, -1:nR+2) ! Velocity distribution function
    real :: VDFOutput_II(nQ, nR)             ! VDF outputs
    real :: Source_C(nQ, nP, nR)             ! Source at each time step
    ! Parameters of the place of initial particles, for initial VDF_G in test
    real, parameter :: ParticleRange1 = 2.0, ParticleRange2 = 4.0

    ! ------------ Diffusion ------------
    real :: LambdaMuMu_II(nQ, nR)            ! lambda_\mu\mu from raw data

    ! ------------ Time variables ------------
    real            :: Time, Dt, DtNext      ! Simulation time, time step
    real            :: tOutput               ! Time of each output file
    logical         :: DoExit                ! Exit time marching
    real            :: Cfl = 0.99            ! CFL number

    ! ------------ Loop variables ------------
    integer :: iQ, iP, iR           ! For VDF variables
    integer :: iStep                ! For time marching
    integer :: iStride, iVar        ! For reading files
    integer :: iOutputFile          ! For output files
    !--------------------------------------------------------------------------

    call read_fieldline             ! Read MhData from saved files
    ! Calculate the Lagrangian particle positions
    HelioDist_I = sqrt(sum(RawData1_II(:, x_:z_)**2, DIM=2))*cLightSpeed/rSun

    ! Kinetic energy convert to momentum (1 MeV – 1 GeV) => (Pmin, Pmax)
    LnP3min = log(sqrt((EkMin + cRmeProtonGeV)**2 - cRmeProtonGeV**2)**3.0/3)
    LnP3max = log(sqrt((EkMax + cRmeProtonGeV)**2 - cRmeProtonGeV**2)**3.0/3)
    ! Get \DeltaLn(P^3/3)
    DeltaLnP3 = (LnP3max - LnP3min)/nR

    ! Then we can culculate P and deltaP^3/3
    MomentumFace_I(0) = (3.0*exp(LnP3min))**(1.0/3.0)
    do iR = 1, nR
       ! The momentum is calculated at each center of the cell
       Momentum_I(iR) = (3.0*exp(LnP3min + (iR-0.5)*DeltaLnP3))**(1.0/3.0)
       ! The momentum is calculated at the face of the cell
       MomentumFace_I(iR) = (3.0*exp(LnP3min + iR*DeltaLnP3))**(1.0/3.0)
    end do

    ! Calculate \deltaP^3/3 for each grid: Face - Face, like the Volume_P
    DeltaP3_I = MomentumFace_I(1:nR)**3.0/3 - MomentumFace_I(0:nR-1)**3.0/3
    ! Calculate the position for ln(p^3/3) axis, in the unit of KeV now
    Energy_I = 6.0+log10(sqrt(Momentum_I**2+cRmeProtonGeV**2) - cRmeProtonGeV)
    ! Considering the law of relativity, v=1/sqrt(1+m^2*c^2/p^2), v can be
    ! calculated as a function of p. Note that light speed is the unit of
    ! speed here, so we do not need to multiply c^2 in the following steps
    Velocity_I = 1.0/sqrt(1.0 + cRmeProtonGeV**2/Momentum_I**2)

    call init_test_VDF           ! Initialize the VDF
    ! Calculate VDF for output: integrate over the \mu axis
    VDFOutput_II = sum(VDF_G(1:nQ, 1:nP, 1:nR), dim=2)*DeltaMu
    ! Save the initial outpur of VDF
    call save_plot_file(NameFile='test_multipoisson_000.out',  &
         TypeFileIn = 'ascii', TimeIn = Time, nStepIn = iStep, &
         NameVarIn = 'sL logE VDF',                            &
         CoordMinIn_D = [HelioDist_I(1) ,    Energy_I(1)],     &
         CoordMaxIn_D = [HelioDist_I(nQ),    Energy_I(nR)],    &
         StringFormatIn = '(3F15.7)',                          &
         Coord1In_I = HelioDist_I,  Coord2In_I = Energy_I,     &
         VarIn_II = log10(VDFOutput_II(1:nQ, 1:nR)))

    ! Start the main simulation loop
    iStep = 0; Time = 0.0
    call calc_data_states
    call update_states(nQ, Time, dDeltaSOverBDt_C = dDeltaSOverBDt_C)
    ! Calculate the total control volume
    do iR = 1, nR
       do iQ = 1, nQ
          dVolumeDt_G(iQ, 0:nP+1, iR) = &
               dDeltaSOverBDt_C(iQ)*DeltaMu*DeltaP3_I(iR)
       end do
    end do
    ! Boundary conditions for total control volume time derivative
    dVolumeDt_G(0,    :,    :) = dVolumeDt_G(1 , :,  :)
    dVolumeDt_G(nQ+1, :,    :) = dVolumeDt_G(nQ, :,  :)
    dVolumeDt_G(:,    :,    0) = dVolumeDt_G(: , :,  1)
    dVolumeDt_G(:,    :, nR+1) = dVolumeDt_G(: , :, nR)

    ! Set up first trial to get DtNext
    call advance_advect(nP, nQ, nR, DeltaMu,                      &
         MomentumFace_I, dDeltaSOverBDt_C, DeltaHamiltonian1_N,   &
         DeltaP3_I, Velocity_I, InvB_C, Hamiltonian2_N,           &
         DeltaSOverB_C, dLnBDeltaS2Dt_C, bDuDt_C, Hamiltonian3_N, &
         iStep, Time, Dt, DtNext, Cfl,                            &
         Volume_G, dVolumeDt_G, VDF_G, Source_C)

    ! In the loop, we output several snapshots of VDF
    do iOutputFile = 1, nOutputFile
       tOutput = tFinal/real(nOutputFile)*real(iOutputFile)
       ! In this loop, we first update the RawData and calculate three
       ! Hamiltonian functions and the total control volume; then, we
       ! use subroutine explicit3 to calculate Source_C and update VDF
       do
          if(Time + DtNext >= tOutput) then
             Dt = tOutput - Time
             DoExit = .true.
          else
             DoExit = .false.
             Dt = DtNext
          end if

          if (trim(TypeScatter)=='no' .or.   &
               trim(TypeScatter)=='advect_scatter') then
             ! Use Poisson scheme for advection, and calculate Source_C
             call advance_advect(nP, nQ, nR, DeltaMu,                      &
                  MomentumFace_I, dDeltaSOverBDt_C, DeltaHamiltonian1_N,   &
                  DeltaP3_I, Velocity_I, InvB_C, Hamiltonian2_N,           &
                  DeltaSOverB_C, dLnBDeltaS2Dt_C, bDuDt_C, Hamiltonian3_N, &
                  iStep, Time, Dt, DtNext, Cfl,                            &
                  Volume_G, dVolumeDt_G, VDF_G, Source_C)

             if (DoExit) then
                ! This step is at tOutput
                VDF_G(1:nQ, 1:nP, 1:nR) = VDF_G(1:nQ, 1:nP, 1:nR) + Source_C
             else
                ! This step is not at tOutput
                ! Update VDF_G considering the time-dependent Volume_G

                Source_C = Source_C*Volume_G(1:nQ, 1:nP, 1:nR)
                ! Update DeltaSOverB_C for the calculation of volume
                call update_states(nQ, Time, DeltaSOverB_C = DeltaSOverB_C)
                ! Calculate the total control volume
                do iR = 1, nR
                   Volume_G(1:nQ, 0, iR) = DeltaSOverB_C*   &
                        DeltaMu*DeltaP3_I(iR)
                   do iP = 1, nP+1
                      Volume_G(1:nQ, iP, iR) = Volume_G(1:nQ, 0, iR)
                   end do
                end do

                ! Update VDF_G to CURRENT time: no BCs for (1:nQ, 1:nP, 1;nR)
                VDF_G(1:nQ, 1:nP, 1:nR) = VDF_G(1:nQ, 1:nP, 1:nR) +  &
                     Source_C/Volume_G(1:nQ, 1:nP, 1:nR)
             end if

             ! If there is no scatter, there is only advection
             ! If there is advect_scatter, we will include scattering effect
             if (trim(TypeScatter)=='advect_scatter') then
                call calc_scatter(nQ, nP, nR, Dt, DeltaMu,     &
                     Velocity_I, InvB_C, RawData1_II(:, rho_), &
                     LambdaMuMu_II, VDF_G(1:nQ, 1:nP, 1:nR))
             end if
          elseif (trim(TypeScatter)=='diffuse_scatter') then
             ! There is only scattering by diffusion
             call diffuse_scatter(nP, nQ, nR, Dt, DeltaMu,  &
                  Velocity_I, InvB_C, DeltaSface_I,         &
                  LambdaMuMu_II, VDF_G(1:nQ, 1:nP, 1:nR))
          else
             ! Unrecognized specified scatter type
             call CON_stop('test_multi_poisson: '//   &
                  'Unknown TypeScatter = '//TypeScatter)
          end if

          call set_VDF_Bc      ! Set VDF BCs (mu: reflection; others: outflow)
          iStep = iStep + 1    ! March step
          Time = Time + DtNext ! March time
          write(*,*) iStep, Time
          if(DoExit) EXIT      ! Reach tOutput, exit the current loop

       end do

       ! Set a min value for VDF_G
       VDF_G = max(VDF_G, cMinVal)
       ! Set the name of files, with the largest number being 999
       write(NameFileSuffix, '(I3.3)') iOutputFile
       ! Calculate VDF for output: integrate over the \mu axis
       VDFOutput_II = sum(VDF_G(1:nQ, 1:nP, 1:nR), dim=2)*DeltaMu

       ! Save the files
       call save_plot_file(NameFile='test_multipoisson_'          &
            //NameFileSuffix//'.out',   TypeFileIn = 'ascii',     &
            TimeIn=Time, nStepIn=iStep, NameVarIn='sL logE VDF',  &
            CoordMinIn_D = [HelioDist_I(1) ,    Energy_I(1)],     &
            CoordMaxIn_D = [HelioDist_I(nQ),    Energy_I(nR)],    &
            StringFormatIn = '(3F15.7)',                          &
            Coord1In_I = HelioDist_I,   Coord2In_I = Energy_I,    &
            VarIn_II = log10(VDFOutput_II(1:nQ, 1:nR)))
    end do

  contains
    !==========================================================================
    subroutine read_fieldline
      ! To read the raw data of fieldlines from the MHD simulations
      ! Import data from files
      !------------------------------------------------------------------------
      open(unit=nInFileOld, file='FieldLineOld.in')
      open(unit=nInFileNew, file='FieldLineNew.in')

      ! Get the lines that we need from the OLD data at OLD time
      GapData = 0.0
      do while(nint(GapData) /= iLagrIdSkip)
         read(nInFileOld, *) GapData
      end do
      do iQ = 1, nQ
         read(nInFileOld,*) (RawData1_II(iQ, iVar), iVar = 1, nVar)
         do iStride = 1, nStride-1
            read(nInFileOld,*)
         end do
      end do
      close(nInFileOld)

      ! Get the lines that we need from the NEW data at NEW time
      GapData = 0.0
      do while(nint(GapData) /= iLagrIdSkip)
         read(nInFileNew, *) GapData
      end do
      do iQ = 1, nQ
         read(nInFileNew,*) (RawData2_II(iQ, iVar), iVar = 1, nVar)
         do iStride = 1, nStride-1
            read(nInFileNew,*)
         end do
      end do
      close(nInFileNew)

      ! Normalization process: due to the differences of the units
      RawData1_II(:, x_ :z_ ) = RawData1_II(:, x_ :z_ )*rSun/cLightSpeed
      RawData1_II(:, ux_:uz_) = RawData1_II(:, ux_:uz_)/cLightSpeed
      RawData2_II(:, x_ :z_ ) = RawData2_II(:, x_ :z_ )*rSun/cLightSpeed
      RawData2_II(:, ux_:uz_) = RawData2_II(:, ux_:uz_)/cLightSpeed
    end subroutine read_fieldline
    !==========================================================================
    subroutine init_test_VDF
      ! Initialize the distribution function VDF_G
      !------------------------------------------------------------------------
      VDF_G = cMinVal

      ! Assign the VDF at positive \mu
      do iR = 1, nR
         do iQ = 1, nQ
            if(iQ <= ParticleRange1) then
               ! Assume momentum follow a power distribution: f ~ p^{-5}
               VDF_G(iQ, nP/2:nP, iR) = max(0.1*exp(-5.0/3.0* &
                    (LnP3min + iR*DeltaLnP3)), cMinVal)
            elseif(iQ <= ParticleRange2) then
               ! Use a linear slope for the middle part
               VDF_G(iQ, nP/2:nP, iR) = max(0.1*exp(-5.0/3.0* &
                    (LnP3min + iR*DeltaLnP3))*(ParticleRange2 - real(iQ))/ &
                    (ParticleRange2 - ParticleRange1), cMinVal)
            end if
         end do
      end do

      ! Set VDF Boundary Conditions (reflection for mu; outflow for others)
      call set_VDF_Bc
    end subroutine init_test_VDF
    !==========================================================================
    subroutine set_VDF_Bc
      ! Set Boundary Conditions for VDF

      ! Use reflective boundary condition for \mu axis
      !------------------------------------------------------------------------
      VDF_G(:,    0, :) = VDF_G(:,    1, :)
      VDF_G(:,   -1, :) = VDF_G(:,    2, :)
      VDF_G(:, nP+1, :) = VDF_G(:,   nP, :)
      VDF_G(:, nP+2, :) = VDF_G(:, nP-1, :)

      ! Use outflow boundary condition for s_L axis
      VDF_G(-1,   :, :) = VDF_G(1,    :, :)
      VDF_G(0,    :, :) = VDF_G(1,    :, :)
      VDF_G(nQ+1, :, :) = VDF_G(nQ,   :, :)
      VDF_G(nQ+2, :, :) = VDF_G(nQ,   :, :)

      ! Use outflow boundary condition for Ln(p^3/3) axis
      VDF_G(:, :,   -1) = VDF_G(:, :,    1)
      VDF_G(:, :,    0) = VDF_G(:, :,    1)
      VDF_G(:, :, nR+1) = VDF_G(:, :,   nR)
      VDF_G(:, :, nR+2) = VDF_G(:, :,   nR)
    end subroutine set_VDF_Bc
    !==========================================================================
    subroutine calc_data_states
      ! Calculate data states from the input files
      real :: MagneticOverWave_C(nQ), LarmorRadius13_C(nQ), Lmax_C(nQ)
      real, parameter :: CoeffMu = 6.0/(2.0**(2.0/3.0)*cPi**(5.0/3.0))

      ! Calculate values at OLD time
      ! Calculate midpoints
      !------------------------------------------------------------------------
      MidPoint_ID(1:nQ-1, x_:z_) = (RawData1_II(2:nQ,x_:z_)    &
           + RawData1_II(1:nQ-1,x_:z_))*0.5
      ! Calculate DeltaS
      DeltaS_I(2:nQ-1) = sqrt(sum((MidPoint_ID(2:nQ-1, x_:z_)  &
           - MidPoint_ID(1:nQ-2, x_:z_))**2, dim=2))
      ! Linear interpolate the deltas such that there will be nQ deltas
      DeltaS_I(1 ) = 2*sqrt(sum( &
           (MidPoint_ID(1, x_:z_) - RawData1_II(1, x_:z_))**2))
      DeltaS_I(nQ) = 2*sqrt(sum( &
           (MidPoint_ID(nQ-1, x_:z_) - RawData1_II(nQ, x_:z_))**2))
      ! Calculate 1/2B and \DeltaS/B at grid center
      InvBOld_C = 1.0/(2.0*sqrt(sum(RawData1_II(:, Bx_:Bz_)**2, dim=2)))
      DeltaSOverBOld_C = DeltaS_I*2.0*InvBOld_C
      LnBDeltaS2Old_C = log(1.0/(2.0*InvBOld_C)*DeltaS_I**2)

      ! At OLD time, we calculate LambdaMuMu_II
      ! Calculate DeltaSface_I: Distance of face at OLD time
      DeltaSface_I = sqrt(sum((RawData1_II(2:nQ, x_:z_)        &
           - RawData1_II(1:nQ-1, x_:z_))**2, dim=2))
      ! Calculate quantites related to magnetic field
      MagneticOverWave_C = (1.0/(2.0*InvBOld_C)**2 +           &
           cMu*sum(RawData1_II(:, wave1_:wave2_), dim=2))/     &
           (cMu*sum(RawData1_II(:, wave1_:wave2_), dim=2))
      LarmorRadius13_C = (1.0e9/(cLightSpeed**2/(2.0*InvBOld_C)))**(1.0/3.0)
      ! Mean free path in the unit of Rsun, i.e., Lmax = 0.03*Rsun
      Lmax_C = 0.03*sqrt(sum(RawData1_II(:, x_:z_)**2, dim=2))
      ! Calculate \lambda_mumu and \lambda_xx
      do iR = 1, nR
         LambdaMuMu_II(:, iR) = CoeffMu*MagneticOverWave_C*    &
              LarmorRadius13_C*Lmax_C**(2.0/3.0)*Momentum_I(iR)**(1.0/3.0)
      end do

      ! Calculate values at NEW time
      ! Calculate midpoints
      MidPoint_ID(1:nQ-1, x_:z_) = (RawData2_II(2:nQ,x_:z_)    &
           + RawData2_II(1:nQ-1,x_:z_))*0.5
      ! Calculate DeltaS
      DeltaS_I(2:nQ-1) = sqrt(sum((MidPoint_ID(2:nQ-1, x_:z_)  &
           - MidPoint_ID(1:nQ-2, x_:z_))**2, dim=2))
      ! Linear interpolate the deltas such that there will be nQ deltas
      DeltaS_I(1 ) = 2*sqrt(sum( &
           (MidPoint_ID(1, x_:z_) - RawData2_II(1, x_:z_))**2))
      DeltaS_I(nQ) = 2*sqrt(sum( &
           (MidPoint_ID(nQ-1, x_:z_) - RawData2_II(nQ, x_:z_))**2))
      ! Calculate 1/2B and \DeltaS/B at grid center
      InvBNew_C = 1.0/(2.0*sqrt(sum(RawData2_II(:, Bx_:Bz_)**2, dim=2)))
      DeltaSOverBNew_C = DeltaS_I*2.0*InvBNew_C
      LnBDeltaS2New_C = log(1.0/(2.0*InvBNew_C)*DeltaS_I**2)

    end subroutine calc_data_states
    !==========================================================================
  end subroutine test_multi_poisson
  !============================================================================
  subroutine update_states(nQ, Time, DeltaSOverB_C, &
       dDeltaSOverBDt_C, InvB_C, bDuDt_C, dLnBDeltaS2Dt_C)
    ! Update states according to the current time

    integer, intent(in) :: nQ     ! Number of grid along s_L axis
    real, intent(in)    :: Time   ! Current time
    real, optional, intent(inout) :: DeltaSOverB_C(nQ),   &
         dDeltaSOverBDt_C(nQ), InvB_C(nQ), bDuDt_C(nQ), dLnBdeltaS2Dt_C(nQ)
    ! Magnetic field strength B at cell center
    real :: B_C(nQ, 3)

    ! Calculate values for CURRENT time: here we use linear interpolation
    ! to get the data of each time step from every to consecutive files
    !--------------------------------------------------------------------------
    ! Calculate B at cell center
    if (present(bDuDt_C)) B_C = RawData1_II(:, Bx_:Bz_) + &
         (RawData2_II(:, Bx_:Bz_) - RawData1_II(:, Bx_:Bz_))/tEachFile*Time
    ! Calculate 1/2B
    if (present(InvB_C)) InvB_C = InvBOld_C + &
         (InvBNew_C - InvBOld_C)/tEachFile*Time
    ! Calculate \deltas/B
    if (present(DeltaSOverB_C)) DeltaSOverB_C = DeltaSOverBOld_C + &
         (DeltaSOverBNew_C - DeltaSOverBOld_C)/tEachFile*Time
    ! Calculate \deltas/B time derivative (for next time)
    if (present(dDeltaSOverBDt_C)) dDeltaSOverBDt_C =  &
         (DeltaSOverBNew_C - DeltaSOverBOld_C)/tEachFile
    ! Calculate Dln(B\deltas^2)/Dt
    if (present(dLnBdeltaS2Dt_C)) dLnBdeltaS2Dt_C = &
         (LnBDeltaS2New_C - LnBDeltaS2Old_C)/tEachfile
    ! Calculate b*Du/Dt
    if (present(bDuDt_C)) bDuDt_C = sum(B_C *                         &
         (RawData2_II(:, ux_:uz_) - RawData1_II(:, ux_:uz_)), dim=2)* &
         2.0*InvB_C/tEachFile
  end subroutine update_states
  !============================================================================
  subroutine calc_hamiltonian_1(nP, nQ, nR, DeltaMu,  &
       MomentumFace_I, dDeltaSOverBDt_C, DeltaHamiltonian1_N)
    ! Calculate the 1st Hamiltonian function with time:
    ! p^3/3*\deltas/B, regarding to tau and p^3/3

    integer, intent(in) :: nP, nQ, nR     ! Number of s_L, mu, ln(p^3/3) grids
    real, intent(in)    :: DeltaMu        ! Distance of adjacent two mu's
    real, intent(in)    :: MomentumFace_I(0:nR) ! Momentum at cell face
    real, intent(in)    :: dDeltaSOverBDt_C(nQ) ! D[delta(s_L)/B]/Dt
    real, intent(inout) :: DeltaHamiltonian1_N(0:nQ+1, 0:nP+1, -1:nR+1)
    integer             :: iP, iR         ! Loop variables
    ! Calculate the first Hamiltonian function
    !--------------------------------------------------------------------------

    do iR = 0, nR
       do iP = 0, nP+1
          DeltaHamiltonian1_N(1:nQ, iP, iR) =   &
               -MomentumFace_I(iR)**3.0/3.0*dDeltaSOverBDt_C
       end do
    end do

    ! Calculate the Hamiltonian function used actually：\tilde\deltaH
    DeltaHamiltonian1_N(1:nQ, 0:nP+1, 0:nR) =   &
         DeltaHamiltonian1_N(1:nQ, 0:nP+1, 0:nR)*DeltaMu

    ! Boundary condition of Hamiltonian function
    DeltaHamiltonian1_N(0   , :,    :) = DeltaHamiltonian1_N(1 , :,  :)
    DeltaHamiltonian1_N(nQ+1, :,    :) = DeltaHamiltonian1_N(nQ, :,  :)
    DeltaHamiltonian1_N(:   , :,   -1) = DeltaHamiltonian1_N(: , :,  0)
    DeltaHamiltonian1_N(:   , :, nR+1) = DeltaHamiltonian1_N(: , :, nR)
  end subroutine calc_hamiltonian_1
  !============================================================================
  subroutine calc_hamiltonian_2(nP, nQ, nR, DeltaMu,  &
       DeltaP3_I, Velocity_I, InvB_C, Hamiltonian2_N)
    ! Calculate the 2nd Hamiltonian function at each fixed time:
    ! (mu^2-1)*v/(2B), regarding to s_L and mu

    integer, intent(in) :: nP, nQ, nR     ! Number of s_L, mu, ln(p^3/3) grids
    real, intent(in)    :: DeltaMu        ! Distance of adjacent two mu
    real, intent(in)    :: DeltaP3_I(nR)  ! Distance of adjacent two p^3/3
    real, intent(in)    :: Velocity_I(nR) ! Particle velocity array
    real, intent(in)    :: InvB_C(nQ)     ! 1/(2B) array
    real, intent(inout) :: Hamiltonian2_N(-1:nQ+1, -1:nP+1, 0:nR+1)
    real                :: InvB_F(0:nQ)   ! Intermediate array
    integer             :: iP, iR         ! Loop variables
    !--------------------------------------------------------------------------
    ! Calculate 1/(2B) on the boundary of grid
    InvB_F(1:nQ-1) = (InvB_C(2:nQ) + InvB_C(1:nQ-1))*0.5
    InvB_F(0 )     = InvB_C(1 ) - (InvB_C(2 ) - InvB_C(1   ))*0.5
    InvB_F(nQ)     = InvB_C(nQ) + (InvB_C(nQ) - InvB_C(nQ-1))*0.5

    ! Calculate the second hamiltonian function = (mu^2-1)*v/(2B)
    do iR = 1, nR
       do iP = 0, nP
          Hamiltonian2_N(0:nQ, iP, iR) = ((iP*DeltaMu - 1.0)**2 &
               - 1.0)*Velocity_I(iR)*InvB_F
       end do
    end do

    ! Calculate the Hamiltonian function used actually: \tilde\deltaH
    do iR = 1, nR
       Hamiltonian2_N(0:nQ, 0:nP, iR) = &
            Hamiltonian2_N(0:nQ, 0:nP, iR)*DeltaP3_I(iR)
    end do

    ! Boundary condition of Hamiltonian function
    Hamiltonian2_N(-1  , :   , :   ) = Hamiltonian2_N(0 , :   , : )
    Hamiltonian2_N(nQ+1, :   , :   ) = Hamiltonian2_N(nQ, :   , : )
    Hamiltonian2_N(:   , -1  , :   ) = Hamiltonian2_N(: , 1   , : )
    Hamiltonian2_N(:   , nP+1, :   ) = Hamiltonian2_N(: , nP-1, : )
    Hamiltonian2_N(:   , :   , 0   ) = Hamiltonian2_N(: , :   , 1 )
    Hamiltonian2_N(:   , :   , nR+1) = Hamiltonian2_N(: , :   , nR)
  end subroutine calc_hamiltonian_2
  !============================================================================
  subroutine calc_hamiltonian_3(nP, nQ, nR, DeltaMu, MomentumFace_I,  &
       DeltaSOverB_C, dLnBDeltaS2Dt_C, bDuDt_C, Hamiltonian3_N)
    ! Calculate the 3rd Hamiltonian function at each fixed time:
    ! (1-mu^2)/2 * [ mu*(p^3/3)*(3\vec{b}\vec{b}:\nabla\vec{u} - div\vec{u})
    ! + ProtonMass*p^2*(\vec{b}*d\vec{u}/dt) ], regarding to p^3/3 and mu
    ! Here we list the variables in the analytical function:
    ! (3\vec{b}\vec{b}:\nabla\vec{u} - div\vec{u}) = d(ln(B*ds^2))/dt
    ! ProtonMass = cRmeProtonGeV, in the unit of GeV/c^2
    ! \vec{b}*d\vec{u}/dt = bDuDt_C

    integer, intent(in) :: nP, nQ, nR     ! Number of s_L, mu, ln(p^3/3) grids
    real, intent(in)    :: DeltaMu        ! Distance of adjacent two mu
    real, intent(in)    :: MomentumFace_I(0:nR) ! Momentum at cell face
    real, intent(in)    :: DeltaSOverB_C(nQ), dLnBDeltaS2Dt_C(nQ), bDuDt_C(nQ)
    real, intent(inout) :: Hamiltonian3_N(0:nQ+1, -1:nP+1, -1:nR+1)
    integer             :: iP, iQ, iR     ! Loop variables
    ! Calculate the third hamiltonian function
    !--------------------------------------------------------------------------
    do iR = 0, nR
       do iP = 0, nP
          Hamiltonian3_N(1:nQ, iP, iR) = 0.5*(1.0 - (iP*DeltaMu-1.0)**2)*   &
               ((iP*DeltaMu-1.0)*MomentumFace_I(iR)**3.0/3.0*               &
               dLnBDeltaS2Dt_C + cRmeProtonGeV*MomentumFace_I(iR)**2*bDuDt_C)
       end do
    end do

    ! Calculate the Hamiltonian function used actually：\tilde\deltaH
    do iQ = 1, nQ
       Hamiltonian3_N(iQ, 0:nP, 0:nR) = &
            Hamiltonian3_N(iQ,0:nP,0:nR)*DeltaSOverB_C(iQ)
    end do

    ! Boundary condition of Hamiltonian function
    Hamiltonian3_N(0   , :   , :   ) = Hamiltonian3_N(1 , :   , : )
    Hamiltonian3_N(nQ+1, :   , :   ) = Hamiltonian3_N(nQ, :   , : )
    Hamiltonian3_N(:   , -1  , :   ) = Hamiltonian3_N(: , 1   , : )
    Hamiltonian3_N(:   , nP+1, :   ) = Hamiltonian3_N(: , nP-1, : )
    Hamiltonian3_N(:   , :   , -1  ) = Hamiltonian3_N(: , :   , 0 )
    Hamiltonian3_N(:   , :   , nR+1) = Hamiltonian3_N(: , :   , nR)
  end subroutine calc_hamiltonian_3
  !============================================================================
  subroutine advance_advect(nP, nQ, nR, DeltaMu,               &
       MomentumFace_I, dDeltaSOverBDt_C, DeltaHamiltonian1_N,  &
       DeltaP3_I, Velocity_I, InvB_C, Hamiltonian2_N,          &
       DeltaSOverB_C, dLnBDeltaS2Dt_C, bDuDt_C, Hamiltonian3_N,&
       iStep, Time, Dt, DtNext, CflIn,                         &
       Volume_G, dVolumeDt_G, VDF_G, Source_C)
    ! Advect transport equation by multi_Poisson scheme

    integer, intent(in) :: nP, nQ, nR     ! Number of s_L, mu, ln(p^3/3) grids
    real, intent(in)    :: DeltaMu        ! Distance of adjacent two mu's
    real, intent(in)    :: MomentumFace_I(0:nR) ! Momentum at cell face
    real, intent(inout) :: dDeltaSOverBDt_C(nQ) ! D[delta(s_L)/B]/Dt
    real, intent(inout) :: DeltaHamiltonian1_N(0:nQ+1, 0:nP+1, -1:nR+1)
    real, intent(in)    :: DeltaP3_I(nR)  ! Distance of adjacent two p^3/3
    real, intent(in)    :: Velocity_I(nR) ! Particle velocity array
    real, intent(inout) :: InvB_C(nQ)     ! 1/(2B) array
    real, intent(inout) :: Hamiltonian2_N(-1:nQ+1, -1:nP+1, 0:nR+1)
    real, intent(inout) :: DeltaSOverB_C(nQ), dLnBDeltaS2Dt_C(nQ), bDuDt_C(nQ)
    real, intent(inout) :: Hamiltonian3_N(0:nQ+1, -1:nP+1, -1:nR+1)
    integer, intent(in) :: iStep          ! Current step index
    real, intent(in)    :: Time           ! Current physical time
    real, intent(inout) :: Dt             ! DtIn, can be reduced
    real, intent(inout) :: DtNext         ! DtOut, i.e., DtNext
    real, intent(in)    :: CflIn          ! Input CFL number
    real, intent(inout) :: Volume_G(0:nQ+1, 0:nP+1, 0:nR+1)    ! Control Volume
    real, intent(in)    :: dVolumeDt_G(0:nQ+1, 0:nP+1, 0:nR+1) ! dVolume/dt
    real, intent(inout) :: VDF_G(-1:nQ+2, -1:nP+2, -1:nR+2)    ! VDF
    real, intent(inout) :: Source_C(nQ, nP, nR) ! Source at each time step
    integer             :: iP, iR         ! Loop variables

    ! Advect by multiple Poisson bracket considering the pitch angle
    !--------------------------------------------------------------------------
    call update_states(nQ, Time,                &
         DeltaSOverB_C = DeltaSOverB_C,         &
         dDeltaSOverBDt_C = dDeltaSOverBDt_C,   &
         InvB_C = InvB_C, bDuDt_C = bDuDt_C,    &
         dLnBDeltaS2Dt_C = dLnBDeltaS2Dt_C)     ! Update states first
    Hamiltonian2_N = 0.0
    Hamiltonian3_N = 0.0

    ! Calculate 1st Hamiltonian function used in the time-dependent
    ! poisson bracket: {f_jk, (p^3/3)*(DeltaS/B)}_{tau, p^3/3}
    call calc_hamiltonian_1(nP, nQ, nR, DeltaMu,   &
         MomentumFace_I, dDeltaSOverBDt_C, DeltaHamiltonian1_N)
    ! Calculate 2nd Hamiltonian function used in the time-dependent
    ! poisson bracket: {f_jk, (mu^2-1)*v/(2B)}_{s_L, mu}
    call calc_hamiltonian_2(nP, nQ, nR, DeltaMu,   &
         DeltaP3_I, Velocity_I, InvB_C, Hamiltonian2_N)
    ! Calculate 3rd Hamiltonian function used in the time-dependent
    ! poisson bracket: {f_jk, (1-mu^2)/2 * [ mu*(p^3/3)*
    ! d(ln(B*ds^2))/dt + ProtonMass*p^2*bDuDt_C ]}_{p^3/3, mu}
    call calc_hamiltonian_3(nP, nQ, nR, DeltaMu, MomentumFace_I,  &
         DeltaSOverB_C, dLnBDeltaS2Dt_C, bDuDt_C, Hamiltonian3_N)

    ! Calculate the total control volume
    do iR = 1, nR
       Volume_G(1:nQ, 0, iR) = DeltaSOverB_C*DeltaMu*DeltaP3_I(iR)
       do iP = 1, nP+1
          Volume_G(1:nQ, iP, iR) = Volume_G(1:nQ, 0, iR)
       end do
    end do

    ! Boundary conditions for total control volume
    Volume_G(0,    :,    :) = Volume_G(1 , :,  :)
    Volume_G(nQ+1, :,    :) = Volume_G(nQ, :,  :)
    Volume_G(:,    :,    0) = Volume_G(: , :,  1)
    Volume_G(:,    :, nR+1) = Volume_G(: , :, nR)

    ! First trial: Need DtNext as DtOut
    if (iStep == 0) then
       call explicit(nQ, nP, nR, VDF_G, Volume_G, Source_C, &
            Hamiltonian12_N = Hamiltonian2_N,               &
            Hamiltonian23_N = -Hamiltonian3_N,              &
            dHamiltonian03_FZ = DeltaHamiltonian1_N,        &
            dVolumeDt_G = dVolumeDt_G,                      &
            DtOut = DtNext, CFLIn=CflIn)
       ! Subsequent trials: with DtIn=Dt and DtOut=DtNext
    else
       call explicit(nQ, nP, nR, VDF_G, Volume_G, Source_C, &
            Hamiltonian12_N = Hamiltonian2_N,               &
            Hamiltonian23_N = -Hamiltonian3_N,              &
            dHamiltonian03_FZ = DeltaHamiltonian1_N,        &
            dVolumeDt_G = dVolumeDt_G,                      &
            DtIn = Dt, DtOut = DtNext, CFLIn=CflIn)
    end if

  end subroutine advance_advect
  !============================================================================
  subroutine calc_scatter(nQ, nP, nR, Dt, DeltaMu,    &
       Velocity_I, InvB_C, n_I, LambdaMuMu_II, VDF_G)
    ! Calculate scatter: \deltaf/\deltat = (Dmumu*f_mu)_mu

    use ModDiffusion, ONLY: tridiag
    integer, intent(in) :: nQ, nP, nR     ! Number of s_L, mu, ln(p^3/3) grids
    real, intent(in) :: Dt                ! Time step
    real, intent(in) :: DeltaMu           ! Distance of adjacent two mu
    real, intent(in) :: Velocity_I(nR)    ! Particle velocity in units of c
    real, intent(in) :: InvB_C(nQ)        ! 1/(2B) along s_L axis
    real, intent(in) :: n_I(nQ)           ! Density in the unit of amu/m^3
    real, intent(in) :: LambdaMuMu_II(nQ, nR)   ! lambda_\mu\mu from data
    real, intent(inout) :: VDF_G(nQ, nP, nR)    ! VDF at s_L, mu, ln(p^3/3)

    ! LOCAL VARs
    ! Dmumu for each fixed iQ and iR. Dmumu is the coefficient
    ! of diffusion along the pitch angle, \mu
    real :: DMuMu_I(0:nP)
    ! Factorize DMuMu, each factor being only a function of s_L, \mu, ln(p^3/3)
    real :: FactorMu_F(0:nP)
    ! Lower, main, upper diagonal, output values for the scatter calculation
    real :: L_I(nP), M_I(nP), U_I(nP), W_I(nP)
    ! Physical VARs
    real :: LowerLimMu           ! Lower limit of Factor_mu
    real :: DtOverDMu2           ! (\Delta t) / (\Delta \mu^2)
    real :: Mu_I(0:nP)           ! Face centered value of the pitch angle
    real :: AlfvenSpeed_I(nQ)    ! Alfven wave speed
    integer :: iQ, iP, iR        ! Loop integers
    integer :: switch1, Pnumber  ! Control parameter
    !--------------------------------------------------------------------------

    DtOverDMu2 = Dt/DeltaMu**2   ! (\Delta t) / (\Delta \mu^2)

    ! Calculate factorized diffusion coefficient
    LowerLimMu = (1.0 - DeltaMu**2)*abs(DeltaMu)**(2.0/3.0)
    do iP = 0, nP
       Mu_I(iP) = -1.0 + real(iP)*DeltaMu
       FactorMu_F(iP) = (1.0 - Mu_I(iP)**2)*abs(Mu_I(iP))**(2.0/3.0)
    end do

    ! Get the Alfven wave speed for each s_L
    AlfvenSpeed_I = 0.5/(InvB_C*cLightSpeed*sqrt(n_I*cAtomicMass*cMu))
    ! Calculate the effect of scatter along \mu axis for each fixed iR and iQ
    do iR = 1, nR
       do iQ = 1, nQ
          DMuMu_I = 0.0
          switch1 = 1

          ! For each pitch angle, we will set DMuMu and solve VDF for scatter
          do iP = 0, nP
             ! Control whether we will floor the value of D_\mu\mu or not
             if(Velocity_I(iR)*abs(Mu_I(iP)) >= 10.0*AlfvenSpeed_I(iQ)) then
                DMuMu_I(iP) = Velocity_I(iR)/LambdaMuMu_II(iQ, iR)* &
                     FactorMu_F(iP)*DtOverDMu2
             else
                if(switch1==1) Pnumber = iP
                switch1 = 0
                ! Set the lower limit of D_\mu\mu when |\mu| is close to zero
                DMuMu_I(iP) = Velocity_I(iR)/LambdaMuMu_II(iQ,iR)* &
                     max(FactorMu_F(Pnumber), LowerLimMu)*DtOverDMu2
             end if
          end do

          ! Set up coefficients for solving the linear equation set
          L_I = -DMuMu_I(0:nP-1)
          U_I = -DMuMu_I(1:nP  )
          M_I = 1.0 - L_I - U_I
          W_I = VDF_G(iQ, 1:nP, iR)

          ! For each pitch angle, we finally solve VDF for scattering effect
          call tridiag(nP, L_I, M_I, U_I, W_I, VDF_G(iQ, 1:nP, iR))
       end do
    end do
  end subroutine calc_scatter
  !============================================================================
  subroutine diffuse_scatter(nP, nQ, nR, Dt, DeltaMu, &
       Velocity_I, InvB_C, DeltaSface_I, LambdaMuMu_II, VDF_G)
    ! Calculate scatter_diffusion: \deltaf/\deltat = (Dmumu*f_mu)_mu

    use ModDiffusion, ONLY: advance_diffusion1
    integer, intent(in) :: nQ, nP, nR     ! Number of s_L, mu, ln(p^3/3) grids
    real, intent(in) :: Dt                ! Time step
    real, intent(in) :: DeltaMu           ! Distance of adjacent two mu
    real, intent(in) :: Velocity_I(nR)    ! Particle velocity in units of c
    real, intent(in) :: InvB_C(nQ)        ! 1/(2B) along s_L axis
    real, intent(in) :: DeltaSface_I(nQ-1)! Distance between s_L centers
    real, intent(in) :: LambdaMuMu_II(nQ, nR)   ! lambda_\mu\mu from data
    real, intent(inout) :: VDF_G(nQ, nP, nR)    ! VDF at s_L, mu, ln(p^3/3)

    ! LOCAL VARs

    real :: DInner_I(nQ), DOuter_I(nQ)    ! Inner/Outer diffusion coffecients
    real :: Dist_I(nQ)                    ! Distance between s_L centers
    real :: dxx(nQ)                       ! Dxx diffusion coffecient
    integer :: iQ, iP, iR                 ! Loop integers

    !--------------------------------------------------------------------------
    Dist_I = 1.0                          ! Initial Dist_I
    Dist_I(1: nQ-1) = DeltaSface_I        ! Assign DeltaSface_I(1:nQ-1)

    ! Calculate the effect of scatter along s_L axis for each fixed iR and iP
    do iR = 1, nR
       do iP = 1, nP
          ! Set inner and outer coefficients for diffusion equation
          DInner_I = 2.0/3.0*Velocity_I(iR)*CoeffMuToxx* &
               LambdaMuMu_II(:, iR)*InvB_C
          DOuter_I = 0.50/InvB_C          ! There should be no Dt here!!!
          ! Finally we diffuse the VDF for scattering effect
          call advance_diffusion1(Dt, 0, nQ, 0, Dist_I,  &
               VDF_G(1:nQ, iP, iR), DOuter_I, DInner_I)
       end do
    end do
    dxx = DInner_I*0.5/InvB_C
  end subroutine diffuse_scatter
  !============================================================================
end module ModTestMultiPoisson
!==============================================================================
module ModHillVortex

  use ModPoissonBracket, ONLY: explicit
  use ModUtilities, ONLY: CON_stop
  use ModPlotFile, ONLY: save_plot_file
  use ModConst

  implicit none

  PRIVATE ! Except
  public :: test_hill_vortex

contains
  !============================================================================
  subroutine test_hill_vortex

    ! Grid in RSph-Theta variables
    ! Theta goes from 0 to 360 because it is mirrored over the pole for plots
    integer, parameter:: nR = 54, nTheta = 2*nR
    real,    parameter:: Dr = 4.50/nR, Cfl = 0.9

    ! Streamlines:
    integer, parameter:: r_ = 2, Ur_ = 2, z_ = 1, Uz_  = 1

    ! Position and width of the smooth bump or sharp ellipsoid
    real, parameter:: xCenter = -3.0, yCenter = 0.0, WidthX = 3.0, WidthY = 4.0
    logical:: IsSmooth = .true.

    real:: x, y, z, r, Uz, Ur, Vel_VC(Uz_:Ur_,-500:500,-500:500), RSph

    ! Loop variables
    integer:: i, j, iR, iTheta, iStep, iPlot

    ! Mesh size in Theta
    real, parameter   :: dTheta = cTwoPi/nTheta
    real :: VDF_G(-1:nR+2,-1:nTheta), Volume_G(0:nR+1, 0:nTheta/2+1)
    real :: R_I(-1:nR+1), Theta_I(-1:nTheta+1), CosTheta_I(-1:nTheta/2+1)
    real :: Hamiltonian_N(-1:nR+1, -1:nTheta/2+1)
    real :: Time, Dt, Source_C(nR,nTheta/2), tFinal
    real :: PlotVar_VC(5,nR,nTheta), DfDt_C(nR,nTheta), DfDt2_C(nR,nTheta)
    real :: Error, Error2, URSph, UTheta
    logical :: DoExit
    !--------------------------------------------------------------------------
    do j = -500, 500
       r = 0.01*j
       do i = -500, 500
          z = 0.01*i
          RSph = sqrt(z**2 + r**2)
          if(RSph < 1.0)then
             ! Inside the vortex
             Uz =  1.50*(-1.0 + r**2 + RSph**2)
             Ur = -1.50*r*z
          else
             Uz = 1.0 - 1.0/RSph**3 + 1.50*r**2/RSph**5
             Ur = -1.50*r*z/RSph**5
          end if
          Vel_VC(Uz_:Ur_, i, j) = [Uz, Ur]
       end do
    end do
    call save_plot_file( &
         NameFile = 'streamlines.out',                &
         StringHeaderIn='Hill vortex velocity field', &
         NameVarIn='z r Uz Ur'  ,                     &
         CoordMinIn_D=[-5.0, -5.0],                   &
         CoordMaxIn_D=[ 5.0,  5.0 ],                  &
         StringFormatIn = '(4F16.4)',                 &
         VarIn_VII = Vel_VC)

    ! Initialize coords
    ! These are cell face coordinates:
    ! r_I(iR) is the radius at iR+1/2, Theta_I(iTheta) is at iTheta+1/2.
    do iR = -1, nR + 1
       R_I(iR) = iR*Dr + 0.5
    end do
    do iTheta = -1, nTheta + 1
       Theta_I(iTheta) = iTheta*dTheta
    end do
    do iTheta = -1, nTheta/2+1
       CosTheta_I(iTheta) = cos(Theta_I(iTheta))
    end do
    ! Overwrite ghost cell values
    CosTheta_I(nTheta/2+1) = CosTheta_I(nTheta/2-1)
    CosTheta_I(-1) = CosTheta_I(1)

    ! Calculate Hamiltonian
    do iTheta = -1, nTheta/2+1
       ! Calculate theta-dependent factor
       Hamiltonian_N(:, iTheta) = cTwoPi*(1.0 - CosTheta_I(iTheta)**2)
       do iR = -1, nR/9 ! Unit radius
          Hamiltonian_N(iR,iTheta) = Hamiltonian_N(iR,iTheta)*0.750*&
               R_I(iR)**2*(R_I(iR)**2 - 1.0)
       end do
       do iR = nR/9+1, nR+1
          Hamiltonian_N(iR,iTheta) = Hamiltonian_N(iR,iTheta)*0.50*&
               R_I(iR)**2*(1.0 - 1.0/R_I(iR)**3)
       end do
    end do
    ! write(*,*)'Maxval(Hamiltonian)=',maxval(Hamiltonian_N)

    ! Calculate volume
    ! angle-dependent factor
    do iTheta = 1, nTheta/2
       Volume_G(0:nR+1,iTheta) = (CosTheta_I(iTheta-1) - CosTheta_I(iTheta))*&
            (R_I(0:nR+1)**3 - R_I(-1:nR)**3)*cTwoPi/3
    end do
    Volume_G(0:nR+1,0) = (CosTheta_I(0) - CosTheta_I(-1)) &
         *(R_I(0:nR+1)**3 - R_I(-1:nR)**3)*cTwoPi/3
    Volume_G(0:nR+1,nTheta/2+1) = &
         (CosTheta_I(nTheta/2+1) - CosTheta_I(nTheta/2)) &
         *(R_I(0:nR+1)**3 - R_I(-1:nR)**3)*cTwoPi/3
    ! write(*,*)'Minval(Volume)=',minval(Volume_G)
    ! Periodic boundary conditions
    Source_C = 0.0

    ! Initial conditions:
    VDF_G = 0.0; DfDt_C = 0.0; DfDt2_C = 0.0
    do iTheta = 1, nTheta; do iR = 1, nR
       x = cos(Theta_I(iTheta) - dTheta/2)*(R_I(iR) - dR/2) - xCenter
       y = sin(Theta_I(iTheta) - dTheta/2)*(R_I(iR) - dR/2) - yCenter
       if(IsSmooth)then
          ! inside a WidthX*WidthY rectangle centered on xCenter,yCenter
          ! cos^2(kx*(x-xCenter))*cos^2(ky*(y-yCenter))
          if(abs(x) < WidthX/2 .and. abs(y) < WidthY/2) then
             VDF_G(iR,iTheta) = cos(cPi*x/WidthX)**4 * cos(cPi*y/WidthY)**4
             z = x + xCenter; r = y + yCenter
             RSph = sqrt(z**2 + r**2)
             Uz = 1.0 - 1.0/RSph**3 + 1.50*r**2/RSph**5
             Ur = -1.50*r*z/RSph**5
             ! Anaylitical (df/dt=-u.grad f), to compare with numeric source
             DfDt_C(iR,iTheta) =cos(cPi*x/WidthX)**3 *cos(cPi*y/WidthY)**3&
                  *(Uz*cPi/WidthX*sin(cPi*x/WidthX)*cos(cPi*y/WidthY) + &
                  Ur*cPi/WidthY*cos(cPi*x/WidthX)*sin(cPi*y/WidthY))*4.0
          end if
       else
          if( (x/WidthX)**2 + (y/WidthY)**2 < 0.25) &
               VDF_G(iR,iTheta) = 1.0
       end if
    end do; end do
    if(IsSmooth)then
       do iTheta = 1, nTheta; do iR = 1, nR
          x = cos(Theta_I(iTheta) - dTheta/2)*(R_I(iR) - dR/2) - xCenter
          y = sin(Theta_I(iTheta) - dTheta/2)*(R_I(iR) - dR/2) - yCenter
          if(abs(x) < WidthX/2 .and. abs(y) < WidthY/2) then
             VDF_G(iR,iTheta) = cos(cPi*x/WidthX)**4 * cos(cPi*y/WidthY)**4
             z = x + xCenter; r = y + yCenter
             RSph = sqrt(z**2 + r**2)
             Uz = 1.0 - 1.0/RSph**3 + 1.50*r**2/RSph**5
             Ur = -1.50*r*z/RSph**5
             URsph = (Uz*z + Ur*r)/RSph
             UTheta = (-Uz*r + Ur*z)/RSph
             ! By differentiating  df/dt=-u.grad f, the second order derivative
             ! may be evaluated: d2f/dt2=-u.grad(df/dt), so that analytical
             ! df/dt may be differentiated numerically:
             DfDt2_C(iR,iTheta) =-0.50*(&
                  URsph*(DfDt_C(iR+1,iTheta) - DfDt_C(iR-1,iTheta))/Dr + &
                  UTheta*(DfDt_C(iR,iTheta+1) - DfDt_C(iR,iTheta-1))/&
                  (RSph*dTheta) )
          end if
       end do; end do
    end if

    Time = 0.0; iStep = 0

    ! Save initial conditions
    call save_plot_file('hill.outs', 'rewind', 'real8', &
         'Hill vortex', iStep, Time, &
         NameVarIn='r Theta Rho'  , &
         CoordMinIn_D=[0.5 + 0.5*Dr, 180.0/nTheta],&
         CoordMaxIn_D=[5.0 - 0.5*Dr, 360.0 - 180.0/nTheta],&
         VarIn_II = VDF_G(1:nR,1:nTheta) )

    call save_plot_file('hillcut.outs', 'rewind', 'real8', &
         'Hill vortex', iStep, Time, &
         NameVarIn='r Theta Rho'  , &
         CoordMinIn_D=[0.5 + 0.5*Dr, 180 + 180.0/nTheta],&
         CoordMaxIn_D=[5.0 - 0.5*Dr, 360 - 180.0/nTheta],&
         VarIn_II = VDF_G(1:nR,nTheta/2+1:nTheta) )
    if(IsSmooth)then
       ! Calculate source for small time step, to compare with
       ! analytical DfDt
       call explicit(nR, nTheta/2, VDF_G(-1:nR+2,-1:nTheta/2+2), Volume_G,&
            Source_C, Hamiltonian_N,   &
            CFLIn=1.0e-6, DtOut = Dt)
       PlotVar_VC(1,:,:) = VDF_G(1:nR,1:nTheta)
       PlotVar_VC(2,:,:) = DfDt_C(1:nR,1:nTheta)
       PlotVar_VC(3,:,1:nTheta/2) = Source_C/Dt
       Error = sum(abs(PlotVar_VC(2,:,1:nTheta/2) - &
            PlotVar_VC(3,:,1:nTheta/2))*&
            Volume_G(1:nR,1:nTheta/2))/(4/3.0*cPi*5.0**3)
       call explicit(nR, nTheta/2, VDF_G(-1:nR+2,-1:nTheta/2+2), Volume_G,&
            Source_C, Hamiltonian_N,   &
            CFLIn=CFL, DtOut = Dt)
       PlotVar_VC(4,:,:) = 0.50*Dt*DfDt2_C(1:nR,1:nTheta)
       PlotVar_VC(5,:,1:nTheta/2) = Source_C/Dt
       Error2 = sum(abs(&
            PlotVar_VC(2,:,1:nTheta/2) + PlotVar_VC(4,:,1:nTheta/2) - &
            PlotVar_VC(5,:,1:nTheta/2))*&
            Volume_G(1:nR,1:nTheta/2))/(4/3.0*cPi*5.0**3)
       do iTheta = nTheta/2 + 1, nTheta
          ! Symmetric prolongation for visualization
          PlotVar_VC(3,:, iTheta) = PlotVar_VC(3,:, 1+nTheta-iTheta)
          PlotVar_VC(5,:, iTheta) = PlotVar_VC(5,:, 1+nTheta-iTheta)
       end do
       call save_plot_file('InitialSource.out', 'rewind','real8', &
            'Hill vortex', iStep, Time, &
            NameVarIn=&
            'z r Rho DfDt Source DfDt2 Source2 Error Error2', &
            CoordMinIn_D=[0.5 + 0.5*Dr, 180.0/nTheta],&
            CoordMaxIn_D=[5.0 - 0.5*Dr, 360.0 - 180.0/nTheta],&
            ParamIn_I=[Error, Error2],&
            VarIn_VII = PlotVar_VC )
       write(*,*)'Error=', Error, ' Error2=', Error2
       ! stop
    end if
    ! Computation
    do iPlot = 0, 99
       tFinal = (iPlot + 1)*0.1
       DoExit = .false.
       PLOT:do
          call explicit(nR, nTheta/2, VDF_G(-1:nR+2,-1:nTheta/2+2), Volume_G,&
               Source_C, Hamiltonian_N,   &
               CFLIn=Cfl, DtOut = Dt)
          iStep = iStep +1
          if(Time + Dt >= tFinal)then
             Dt = tFinal - Time
             call explicit(nR, nTheta/2, VDF_G(-1:nR+2,-1:nTheta/2+2), &
                  Volume_G, Source_C, Hamiltonian_N, DtIn = Dt)
             VDF_G(1:nR, 1:nTheta/2) = VDF_G(1:nR, 1:nTheta/2) + Source_C
             Time = tFinal
             DoExit = .true.
          else
             Time = Time + Dt
             VDF_G(1:nR, 1:nTheta/2) = VDF_G(1:nR, 1:nTheta/2) + Source_C
          end if
          do iTheta = nTheta/2 + 1, nTheta
             ! Symmetric prolongation
             VDF_G(1:nR, iTheta) = VDF_G(1:nR, 1+nTheta-iTheta)
          end do
          ! Periodic prolongation
          VDF_G(1:nR,-1:0) = VDF_G(1:nR, nTheta-1:nTheta)
          write(*,*)'Time=',Time
          if(DoExit) EXIT PLOT
       end do PLOT
       call save_plot_file('hill.outs', 'append', 'real8', &
            'Hill vortex', iStep, tFinal, &
            NameVarIn='r Theta Rho'  , &
            CoordMinIn_D=[0.50 + 0.50*Dr, 180.0/nTheta],&
            CoordMaxIn_D=[5.0 - 0.50*Dr, 360.0 - 180.0/nTheta],&
            VarIn_II = VDF_G(1:nR,1:nTheta) )
       call save_plot_file('hillcut.outs', 'append', 'real8', &
            'Hill vortex', iStep, tFinal, &
            NameVarIn='r Theta Rho'  , &
            CoordMinIn_D=[0.5 + 0.5*Dr, 180 + 180.0/nTheta],&
            CoordMaxIn_D=[5.0 - 0.5*Dr, 360 - 180.0/nTheta],&
            VarIn_II = VDF_G(1:nR,nTheta/2+1:nTheta) )
    end do
  end subroutine test_hill_vortex
  !============================================================================
end module ModHillVortex
!==============================================================================
module ModStochastic
  use ModPoissonBracket, ONLY: explicit
  use ModUtilities, ONLY: CON_stop
  use ModPlotFile, ONLY: save_plot_file
  use ModConst
  implicit none
  PRIVATE ! Except
  public :: test_stochastic
  ! Streamlines:
  integer, parameter :: nPointPer2Pi = 180
  integer, parameter :: nJ = (4*nPointPer2Pi)/2, nTheta=nPointPer2Pi
  real, parameter    :: Delta = cTwoPi/nPointPer2Pi
  ! Loop variables
  integer           ::  iJ, iTheta, iStep, iPlot
  ! Mesh size in \Theta
  real :: VDF_G(  -1:nTheta+2, -1-nJ:nJ+2)
  real :: Volume_G(0:nTheta+1,   -nJ:nJ+1)
  real :: Theta_I(-1:nTheta+1), Action_I(-1-nJ:nJ+1), Action
  real :: HamiltonianFree_N(-1:nTheta+1, -1-nJ:nJ+1)
  real :: HamiltonianPush_N(-1:nTheta+1, -1-nJ:nJ+1)
  real :: Time, Dt, Source_C(nTheta,1-nJ:nJ), tFinal
  logical :: DoAgain
  character(LEN=11)::NameFile
  !----------------------------------------------------------------------------
contains
  !============================================================================
  subroutine test_stochastic(KChirikov)
    real, intent(in) :: KChirikov ! Left hand side in the Chhirikov criterion
    !--------------------------------------------------------------------------
    ! Uniform Delta*Delta grid:
    Volume_G = Delta**2
    do iJ = -1-nJ, nJ+1
       Action_I(iJ)  = iJ*Delta
    end do
    do iTheta = -1, nTheta+1
       Theta_I(iTheta) = iTheta*Delta
    end do
    ! Free rotator:
    do iJ = -1-nJ, nJ+1
       HamiltonianFree_N(-1:nTheta+1, iJ) = 0.50*Action_I(iJ)**2
    end do
    ! Sinusoidal perturbation
    do iTheta = -1, nTheta+1
       HamiltonianPush_N(iTheta, -1-nJ:nJ+1) = cos(Theta_I(iTheta))
    end do
    ! Account for:
    ! 1. Amplitude factor, KChirikov
    ! 2. Dirac delta-function, by dividing the amplitude by the short time
    !    interval and apply the perturbation only within this interval
    ! 3. Sum up both Hamiltonians:
    HamiltonianPush_N = (KChirikov/0.10)*HamiltonianPush_N + HamiltonianFree_N
    ! Initial distribution function: zero at high energies and in ghost cells:
    VDF_G(-1:nTheta+2,-1-nJ:nJ+2) =  exp(-12.50)
    do iJ  = 1, nJ
       Action = 0.50*(Action_I(iJ-1) + Action_I(iJ))
       if(Action < 1.0)then
          VDF_G(:,iJ) = exp(-12.50*Action**2)
       else
          EXIT
       end if
    end do
    do iJ  = 0, 1-nJ,-1
       Action = 0.50*(Action_I(iJ-1) + Action_I(iJ))
       if(abs(Action) < 1.0)then
          VDF_G(:,iJ) = exp(-12.50*Action**2)
       else
          EXIT
       end if
    end do
    Source_C = 0.0
    tFinal  = 0.0
    ! Compiutation
    Time = 0.0; iStep = 0
    call save_plot_file(NameFile='initial.out', &
         TypeFileIn='real8', TimeIn=tFinal, nStepIn = iStep, &
         NameVarIn='theta JT/2pi Log10VDF'  , &
         CoordMinIn_D=[         0.50*Delta, -2.0 + 0.50/nPointPer2Pi],&
         CoordMaxIn_D=[cTwoPi - 0.50*Delta,  2.0 - 0.50/nPointPer2Pi],&
         VarIn_II = log10(VDF_G(1:nTheta,1-nJ:nJ)) )
    do iPlot = 0, 999
       tFinal = tFinal + 0.1
       DoAgain = .true.
       do while(DoAgain)
          call explicit(nTheta,2*nJ, VDF_G, Volume_G,&
               Source_C, HamiltonianPush_N,   &
               CFLIn=0.5, DtOut = Dt,IsPeriodicIn_D=[.true.,.false.])
          iStep = iStep +1
          if(Time + Dt >= tFinal)then
             Dt = tFinal - Time
             call explicit(nTheta,2*nJ,VDF_G,Volume_G,&
                  Source_C, HamiltonianPush_N,   &
                  DtIn = Dt,IsPeriodicIn_D=[.true.,.false.])
             VDF_G(1:nTheta,1-nJ:nJ) = VDF_G(1:nTheta,1-nJ:nJ) + Source_C
             Time = tFinal
             DoAgain = .false.
          else
             Time = Time + Dt
             VDF_G(1:nTheta,1-nJ:nJ) = VDF_G(1:nTheta,1-nJ:nJ) + Source_C
          end if
          ! Periodic prolongation
          VDF_G(-1:0,:) = VDF_G(nTheta-1:nTheta,:)
          VDF_G(nTheta+1:nTheta+2,:) = VDF_G(1:2,:)
          write(*,*)'Time=',Time
       end do
       tFinal = tFinal + 0.9
       DoAgain = .true.
       do while(DoAgain)
          call explicit(nTheta,2*nJ, VDF_G, Volume_G,&
               Source_C, HamiltonianFree_N,   &
               CFLIn=0.99, DtOut = Dt,IsPeriodicIn_D=[.true.,.false.])
          iStep = iStep +1
          if(Time + Dt >= tFinal)then
             Dt = tFinal - Time
             call explicit(nTheta,2*nJ,VDF_G,Volume_G,&
                  Source_C, HamiltonianFree_N,   &
                  DtIn = Dt, IsPeriodicIn_D=[.true.,.false.])
             VDF_G(1:nTheta,1-nJ:nJ) = VDF_G(1:nTheta,1-nJ:nJ) + Source_C
             Time = tFinal
             DoAgain = .false.
          else
             Time = Time + Dt
             VDF_G(1:nTheta,1-nJ:nJ) = VDF_G(1:nTheta,1-nJ:nJ) + Source_C
          end if
          ! Periodic prolongation
          VDF_G(-1:0,:) = VDF_G(nTheta-1:nTheta,:)
          VDF_G(nTheta+1:nTheta+2,:) = VDF_G(1:2,:)
          write(*,*)'Time=',Time
       end do
       write(NameFile,'(a,i3.3,a)')'stoc',iPlot,'.out'
       call save_plot_file(NameFile=NameFile, &
            TypeFileIn='real8', TimeIn=tFinal, nStepIn = iStep, &
            NameVarIn='theta JT/2pi Log10VDF'  , &
            CoordMinIn_D=[         0.50*Delta, -2.0 + 0.50/nPointPer2Pi],&
            CoordMaxIn_D=[cTwoPi - 0.50*Delta,  2.0 - 0.50/nPointPer2Pi],&
            VarIn_II = log10(VDF_G(1:nTheta,1-nJ:nJ)) )
    end do
  end subroutine test_stochastic
  !============================================================================
end module ModStochastic
!==============================================================================
program test_program
  use ModTestPoissonBracket, ONLY: test_poisson_bracket, test_dsa_sa_mhd, &
       test_dsa_poisson, test_energy_conservation, test_in_action_angle,  &
       test_poisson_2d, test_poisson_2d_smooth, test_dsa_impl
  use ModNumConst, ONLY: cTwoPi
  use ModHillVortex, ONLY: test_hill_vortex
  use ModStochastic, ONLY: test_stochastic
  use ModTestMultiPoisson, ONLY: test_multi_poisson

  implicit none
  !----------------------------------------------------------------------------
  write(*,*)' Start poisson bracket tests'

  call test_poisson_bracket(cTwoPi)       ! nightly test1
  call test_poisson_2d(cTwoPi)            ! nightly test2
  call test_dsa_poisson                   ! nightly test3
  ! call test_poisson_2d_smooth(cTwoPi)
  ! call test_stochastic(1.2)
  ! call test_hill_vortex
  ! call test_energy_conservation(cTwoPi)
  ! call test_in_action_angle(cTwoPi)
  call test_dsa_sa_mhd                    ! for Fig 5.Right Panel
  call test_dsa_impl

  ! Tests of focused transport equation:
  ! Advection without any scattering
  ! call test_multi_poisson(100.0, TypeScatter='no')
  ! Advection and scattering
  ! call test_multi_poisson(100.0, TypeScatter='advect_scatter')
  ! Scatter only by diffusion
  ! call test_multi_poisson(100.0, TypeScatter='diffuse_scatter')
  write(*,*)' Finish poisson bracket tests'

end program test_program
!==============================================================================
