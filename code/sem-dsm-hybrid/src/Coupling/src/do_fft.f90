SUBROUTINE FOUR1(DATA, NN, ISIGN)
  ! Computes the FFT or inverse FFT of a complex array using the Cooley-Tukey
  ! algorithm.
  ! Arguments:
  !   DATA: Complex input/output array.
  !   NN: Size of the FFT (must be a power of 2).
  !   ISIGN: 1 for FFT, -1 for inverse FFT.

    IMPLICIT NONE
    INTEGER, INTENT(IN) :: NN, ISIGN
    COMPLEX(KIND=8), INTENT(INOUT) :: DATA(NN)
    INTEGER :: N, M, MMAX, ISTEP, I, J, K
    DOUBLE PRECISION :: WPR, WPI, WR, WI, WTEMP, THETA
    COMPLEX(KIND=8) :: TEMP

    ! Validate input size
    N = SIZE(DATA)
    IF (N /= NN) THEN
      PRINT *, "Error: Array size does not match NN"
      STOP
    END IF

    ! Bit-reversal permutation
    J = 1
    DO I = 1, N
      IF (J > I) THEN
        TEMP = DATA(J)
        DATA(J) = DATA(I)
        DATA(I) = TEMP
      END IF
      K = N / 2
      DO WHILE (J > K .AND. K >= 1)
        J = J - K
        K = K / 2
      END DO
      J = J + K
    END DO
    ! Danielson-Lanczos section
    MMAX = 1
    DO WHILE (MMAX < N)
      ISTEP = 2 * MMAX
      THETA = ISIGN * (2.0D0 * 3.141592653589793D0 / ISTEP)
      WPR = -2.0D0 * DSIN(0.5D0 * THETA)**2
      WPI = DSIN(THETA)
      WR = 1.0D0
      WI = 0.0D0
      DO M = 1, MMAX
        DO I = M, N, ISTEP
          J = I + MMAX
          TEMP = (WR* (1.0D0, 0.0D0) + WI * (0.0D0, 1.0D0)) * DATA(J)
          DATA(J) = DATA(I) - TEMP
          DATA(I) = DATA(I) + TEMP
        END DO
        WTEMP = WR
        WR = WR * WPR - WI * WPI + WR
        WI = WI * WPR + WTEMP * WPI + WI
      END DO
      MMAX = ISTEP
    END DO

    ! If inverse FFT, normalize the output
    !IF (ISIGN == -1) THEN
    !  DATA = DATA / N
    !END IF

  END SUBROUTINE

