module PT_ModSolver
    ! Written by Alex Shane
    ! Multiple solvers for SDEs

    use PT_ModRandom, ONLY: get_random_normal
    use PT_ModFieldline, ONLY: get_sde_coeffs_euler
    use PT_ModSize, ONLY: nDim
    use PT_ModTime, ONLY: PTTime
    implicit none
    SAVE

    real :: TimeStepFactor, MaxTimeStep

contains
    !==============================================================================
   subroutine read_param(NameCommand)

      use ModReadParam, ONLY: read_var
      use ModUtilities, ONLY: CON_stop

      character(len=*), intent(in):: NameCommand ! From PARAM.in
      character(len=*), parameter:: NameSub = 'read_param'
      !--------------------------------------------------------------------------
      select case(NameCommand)
      case('#SDE')
         call read_var('TimeStepFactor', TimeStepFactor)
         call read_var('MaxTimeStep', MaxTimeStep)
      case default
         call CON_stop(NameSub//' Unknown command '//NameCommand)
      end select

   end subroutine read_param
   !============================================================================
    subroutine euler_sde(X_I, TimeMax, Time, Timestep, Xnew_I)
        ! Solve SDE using Euler-Maruyama method
        ! Timestep is calculated inside get_sde_coeffs
        real, intent(in)    :: X_I(nDim), TimeMax, Time
        real, intent(out)   :: Timestep, Xnew_I(nDim)

        real :: Wk(nDim)
        real :: DriftCoeff(nDim), DiffCoeff(nDim)
        real :: RandomNormal

        call get_sde_coeffs_euler(X_I, Time, Timestep, DriftCoeff, DiffCoeff)
        ! Multiply by time step factor and limit maximum time step
        Timestep = min(Timestep*TimeStepFactor, MaxTimeStep)

        ! if timestep takes the particle past the maximum time - limit timestep
        if(Time+Timestep.gt.TimeMax) Timestep = TimeMax - Time

        ! get random normal variables 
        call get_random_normal(RandomNormal)
        Wk(1) = sqrt(Timestep)*RandomNormal
        ! No momentum diffusion. 
        Wk(2) = 0.0

        ! calculate changes to phase space variables
        Xnew_I = X_I + DriftCoeff * Timestep + DiffCoeff * Wk

    end subroutine euler_sde
    !==============================================================================
    ! subroutine milstein_sde(X_I, tStepMax, Time, Timestep, Xnew_I)
        ! Solve SDE using Milstein method
        ! Timestep is calculated inside get_sde_coeffs
        ! real, intent(in) :: X_I(nDim), tStepMax, Time
        ! real, intent(out) :: Timestep, Xnew_I(nDim)

        ! real :: Wk(nDim)
        ! real :: DriftCoeff(nDim), DiffCoeff(nDim), dDiffdX(nDim)
        ! real :: RandomNormal

        ! call get_sde_coeffs_milstein(X_I, Time, Timestep, DriftCoeff, DiffCoeff, dDiffdX)
        ! if(Time+Timestep.gt.tStepMax) Timestep = tStepMax - time

        ! call get_random_normal(RandomNormal)
        ! Wk(1) = sqrt(Timestep)*RandomNormal

        ! No momentum diffusion. 
        ! Wk(2) = 0.0

        ! Xnew_I = X_I + DriftCoeff * Timestep + DiffCoeff * Wk + dDiffdX * (Wk**2 - Timestep)

    ! end subroutine milstein_sde
    !==============================================================================
    ! subroutine rk2_sde(X_I, Time, Timestep, Xnew_I)
        !     ! Runge Kutta 2 Scheme for SDEs
        !     ! Roberts 2012  "Modify the improved Euler scheme..."
        !     real, intent(in) :: X_I(nDim), Time, Timestep
        !     real, intent(out) :: Xnew_I(nDim)

        !     real :: RandomNormal, RandomUniform, Wk, Sk
        !     real :: K1_I(nDim), K2_I(nDim) ! runge-kutta coefficients

        !     ! the same Wk and Sk needs to be used for both K1, K2
        !     call get_random_normal(RandomNormal)
        !     call random_number(RandomUniform)
            
        !     Sk = StratoFactor * sign(sqrt(Timestep), RandomUniform-0.5)
        !     Wk = sqrt(Timestep)*RandomNormal - Sk
            
        !     call calculate_k(X_I, Wk, Time, TimeStep, K1_I)

        !     Wk = sqrt(Timestep)*RandomNormal + Sk
        !     call calculate_k(X_I + K1_I, Wk, Time + Timestep, Timestep, K2_I)

        !     Xnew_I = X_I + 0.5 * (K1_I + K2_I)

    ! end subroutine rk2_sde
    !==============================================================================
    ! subroutine calculate_k(X_I, Wk, Time, Timestep, K_I)
            ! ! Calculate K1 and K2 for second order RK scheme for SDE
            ! real, intent(in) :: X_I(nDim), Wk, Time, Timestep
            ! real, intent(out) :: K_I(nDim)

            ! integer :: i
            ! real :: DriftCoeff(nDim), DiffCoeff(nDim) !, pDiff

            ! call get_sde_coeffs_rk2(X_I, Time, Timestep, DriftCoeff, DiffCoeff)
            ! do i = 1, nDim
            !     K_I(i) = Timestep * DriftCoeff(i) + Wk * DiffCoeff(i)
            ! end do

    ! end subroutine calculate_k
    !==============================================================================
end module PT_ModSolver
!==============================================================================
