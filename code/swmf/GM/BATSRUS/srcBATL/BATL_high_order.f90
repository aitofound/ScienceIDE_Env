!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module BATL_high_order

  use ModUtilities, ONLY: CON_stop

  implicit none

  save

  private ! except

  ! Make (f(j+1/2) - f(j-1/2))/dx high order accurate.
  public:: correct_face_value

  ! Input:  f(i-3)...f(i+3), dx
  ! Output: (f(i+1/2) - f(i-1/2))/dx, which is 6th order accurate.
  public:: calc_center_first_derivate

  ! Input:  f(i-2)...f(i+3)
  ! Output: f(i+1/2), which is 6th order accurate.
  public:: calc_face_value

  ! 5th order prolongation for resolution change.
  public:: prolong_high_order_face_ghost

  ! 5th order restriction for resolution change.
  public:: restrict_high_order_reschange

  ! Make sure all the ghost cells are high order accurate.
  public:: correct_face_ghost_for_fine_block

  ! 5th order AMR.
  public:: prolong_high_order_amr
  public:: restrict_high_order_amr

  public:: limit_interpolation

contains
  !============================================================================
  real function correct_face_value(FaceValue, CellValue_I)
  !$acc routine seq

    ! FaceValue is at cell face. CellValue_I are cell centered.
    ! Return 6th order approximation

    real, intent(in):: CellValue_I(:), FaceValue

    real:: Der2, Der4
    real, parameter:: c1over6 = 1./6, c1over180 = 1./180
    !--------------------------------------------------------------------------
    Der2 = c1over6*(CellValue_I(2) - 2*FaceValue + CellValue_I(3))
    Der4 = c1over180*(16*FaceValue - &
         9*(CellValue_I(2) + CellValue_I(3)) + &
         CellValue_I(1) + CellValue_I(4))
    correct_face_value = FaceValue - Der2 + Der4

  end function correct_face_value
  !============================================================================
  real function calc_center_first_derivate(CellValue_I, DxIn)

    ! Calculate df/dx at x=x_i with f(k), where k = i-3,i-2 ... i+3.
    ! Directly combine CellValue_I to get df/dx can save some
    ! computation compare to the approach in this subroutine.

    real, intent(in):: CellValue_I(7)
    real, optional, intent(in):: DxIn

    real:: Dx
    real:: FaceL, FaceR
    real:: CorrectedFaceL, CorrectedFaceR
    integer, parameter:: i = 4
    !--------------------------------------------------------------------------
    Dx = 1.0
    if(present(DxIn)) Dx = DxIn

    FaceL = calc_face_value(CellValue_I(1:6))
    FaceR = calc_face_value(CellValue_I(2:7))

    CorrectedFaceL = correct_face_value(FaceL, CellValue_I(i-2:i+1))
    CorrectedFaceR = correct_face_value(FaceR, CellValue_I(i-1:i+2))

    calc_center_first_derivate = &
         (CorrectedFaceR - CorrectedFaceL)/Dx
  end function calc_center_first_derivate
  !============================================================================
  real function calc_face_value(CellValue_I, DoLimitIn, IsPositiveIn)
    ! Calculate f_{i+1/2} with f(k), where k = i-2,i-1 ... i+3
    real, intent(in):: CellValue_I(:) ! size of 6
    logical, optional, intent(in):: DoLimitIn, IsPositiveIn

    logical:: DoLimit
    real, parameter:: c3over256 = 3./256, c25over256 = 25./256, &
         c150over256 = 150./256
    real:: FaceValue
    real:: Distance_I(4) = [-1.5, -0.5, 0.5, 1.5]
    !--------------------------------------------------------------------------
    DoLimit = .false.
    if(present(DoLimitIn)) DoLimit = DoLimitIn

    FaceValue = c3over256*(CellValue_I(1) + CellValue_I(6)) - &
         c25over256*(CellValue_I(2) + CellValue_I(5)) + &
         c150over256*(CellValue_I(3) + CellValue_I(4))

    calc_face_value = FaceValue
    if(DoLimit) calc_face_value = &
         limit_interpolation(FaceValue, CellValue_I(2:5), Distance_I,&
         IsPositiveIn=IsPositiveIn)
  end function calc_face_value
  !============================================================================
  real function limit_interpolation(FaceOrig, CellValue_I, Distance_I, &
       ValueMaxIn, ValueMinIn, IsPositiveIn, IsCell1AccurateIn, &
       IsCell4AccurateIn)

    ! This Limiter works for ununiform grid interpolation.
    ! See (2.18) in 'Accurate Monotonicity-Preserving Schemes with
    ! Runge-Kutta Time Stepping' by A. Suresh & H. T. Huynh (1997)
    ! The return value should be less/larger than ValueMaxIn/ValueMinIn if
    ! they are present. Both of them should present at the same time.
    ! This option maybe useful, but have not been used anywhere so far.

    real, intent(in):: FaceOrig, CellValue_I(:), Distance_I(:) ! size 4
    real, optional, intent(in):: ValueMaxIn, ValueMinIn
    logical, optional, intent(in):: IsPositiveIn, IsCell1AccurateIn, &
         IsCell4AccurateIn

    logical:: IsPositive, IsCell1Accurate, IsCell4Accurate

    real:: FaceL, FaceR, FaceAV, FaceMD, FaceMin, FaceMax, ValueMax, ValueMin
    real:: Mp5Result, LowResult, w1, w2
    real,parameter:: c6 = 0.6
    !--------------------------------------------------------------------------
    IsPositive = .false.
    if(present(IsPositiveIn)) IsPositive = IsPositiveIn
    IsCell1Accurate=.true.
    if(present(IsCell1AccurateIn)) IsCell1Accurate=IsCell1AccurateIn
    IsCell4Accurate=.true.
    if(present(IsCell4AccurateIn)) IsCell4Accurate=IsCell4AccurateIn

    FaceAV = two_points_interpolation(CellValue_I(2:3), Distance_I(2:3))
    FaceL = two_points_interpolation(CellValue_I(1:2), Distance_I(1:2))
    FaceR = two_points_interpolation(CellValue_I(3:4), Distance_I(3:4))

    ! Use one side limiting.
    if(.not.IsCell1Accurate) FaceL = FaceR
    if(.not.IsCell4Accurate) FaceR = FaceL

    FaceMD = median(FaceAV, FaceL, FaceR)
    FaceMin = min(FaceMD, CellValue_I(2), CellValue_I(3))
    FaceMax = max(FaceMD, CellValue_I(2), CellValue_I(3))

    Mp5Result = median(FaceOrig, FaceMin, FaceMax)

    if(IsPositive) then
       w1 = abs(Distance_I(3))/(Distance_I(3) - Distance_I(2))
       w2 = 1 - w1
       LowResult = w1*CellValue_I(2) + w2*CellValue_I(3)
       if(Mp5Result < c6*LowResult) Mp5Result = LowResult
    endif

    ValueMax = Mp5Result
    if(present(ValueMaxIn)) ValueMax = ValueMaxIn
    ValueMin = Mp5Result
    if(present(ValueMinIn)) ValueMin = ValueMinIn

    limit_interpolation = median(Mp5Result, ValueMax, ValueMin)
  end function limit_interpolation
  !============================================================================
  real function median(a,b,c)

    real, intent(in):: a, b, c
    !--------------------------------------------------------------------------
    median = max(min(a, max(b,c)), min(b,c))

  end function median
  !============================================================================
  real function two_points_interpolation(Cell_I, Distance_I)

    ! Cell_I(i) is at xi, calculate the value at x=0.
    ! Distance_I(i) = xi - x0

    real, intent(in):: Cell_I(:), Distance_I(:)
    real:: c1, c2
    !--------------------------------------------------------------------------
    c1 = Distance_I(2)/(Distance_I(2) - Distance_I(1))
    c2 = -Distance_I(1)/(Distance_I(2) - Distance_I(1))
    two_points_interpolation = c1*Cell_I(1) + c2*Cell_I(2)

  end function two_points_interpolation
  !============================================================================
  subroutine restrict_high_order_reschange(CoarseCell, FineCell_III, &
       Ghost_I, DoSymInterpIn,IsPositiveIn)

    ! For 2D:
    !         _________________________________
    !         | u1|   |   |   |   |   |   |   |
    !         |___|___|___|___|___|___|___|___|
    !         | u2|   |   |   |   |   |   |   |
    !  _______|___|___|___|___|___|___|___|___|
    ! |       | u3| u7|   |   |   |   |   |   |
    ! |Coarse |__G1___|__G2___|__G3___|___|___|
    ! | u0    | u4| u8|   |   |   |   |   |   |
    ! |_______|___|___|___|___|___|___|___|___|
    !         | u5|   |   |   |   |   |   |   |
    !         |___|___|___|___|___|___|___|___|
    !         | u6|   |   |   |   |   |   |   |
    !         |___|___|___|___|___|___|___|___|

    ! First calculate the face value (f1) between u3 and u4 with u1, u2...u6.
    ! Face value between u7 and u8 (f2) and more face value can be got in the
    ! same way. 8 face values are needed.

    ! Use u0, f1, f2, f3, f4 to calculate the ghost cell G1.
    ! Use f1, f2, f3, f4, f5, f6 to interpolate G2
    ! Use f3, f4...f7,f8 to interpolate the third ghost cell (G3). But if
    ! there are only 6 cells, then use f1..f6 to interpolate G3 (not
    ! DoSymInterp).

    use BATL_size, ONLY: nJ, nk

    ! In 2D there is only 1 cell in the K direction
    integer, parameter:: k6 = min(nK, 6), j6 = min(nJ,6)

    real, intent(in) :: CoarseCell          ! value of coarse neighbor cell
    real, intent(in) :: FineCell_III(:,:,:) ! local fine cells: (8,j6,k6)
    real, intent(out):: Ghost_I(3)          ! coarse ghost cells for neighbor
    logical, optional, intent(in) :: DoSymInterpIn, IsPositiveIn

    ! Local variables
    real:: FineCell_I(8)
    real:: Ghost, Cell_I(4)
    real:: CellInput_I(6), CellInput_II(6,6)
    logical:: DoSymInterp, IsPositive

    ! Distances of coarse neighbor and fine cells from 1st ghost cell
    real:: Distance_I(4)=[-4,-1,1,3]
    real:: Distance1_I(4) = [-3,-1,1,3]

    ! Interpolation coefficients for G1
    real, parameter:: c1=-1./63, c2=5./12, c3 = 3./4, c4=-5./28, c5=1./36

    real:: Coef_I(6) = ([7./256, -45./256, 63./128, -105./128, &
         315./256, 63./256])

    logical, parameter:: DoLimit = .true. ! change only for debugging
    integer:: i

    character(len=*), parameter:: NameSub = 'restrict_high_order_reschange'
    !--------------------------------------------------------------------------
    ! use f3...f8 to interpolate G3.
    DoSymInterp = .true.
    if(present(DoSymInterpIn)) DoSymInterp = DoSymInterpIn

    IsPositive = .false.
    if(present(IsPositiveIn)) IsPositive = IsPositiveIn

    ! Integerpolate fine cell centers to line connecting coarse cell centers
    if(nK == 1) then  ! 2D resolution change
       do i = 1, 8
          ! Use a temporary variable CellInput_I to avoid compile error.
          CellInput_I(1:j6) = FineCell_III(i,:,1)
          FineCell_I(i) = calc_face_value(CellInput_I, DoLimit,&
               IsPositiveIn=IsPositive)
       enddo
    else
       ! 3D resolution change. Need more tests to make sure it works!!
       do i = 1, 8
          CellInput_II(1:j6,1:k6) = FineCell_III(I,:,:)
          FineCell_I(i) = &
               calc_edge_value(CellInput_II,DoLimit,IsPositiveIn=IsPositive)
       enddo
    endif

    ! High order interpolation for first ghost cell using coarse neighbor
    Ghost = c1*CoarseCell + c2*FineCell_I(1) + c3*FineCell_I(2) + &
         c4*FineCell_I(3) + c5*FineCell_I(4)

    ! Limit value if requested
    if(DoLimit) then
       Cell_I(1)   = CoarseCell
       Cell_I(2:4) = FineCell_I(1:3)
       Ghost_I(1)  = limit_interpolation(Ghost, Cell_I, Distance_I,&
            IsPositiveIn=IsPositive)
    else
       Ghost_I(1) = Ghost
    endif

    ! Interpolate G2 and G3 from fine edge values
    Ghost_I(2) = calc_face_value(FineCell_I(1:6), DoLimit,&
         IsPositiveIn=IsPositive)
    if(DoSymInterp) then
       Ghost_I(3) = calc_face_value(FineCell_I(3:8), DoLimit,&
            IsPositiveIn=IsPositive)
    else
       Ghost = sum(FineCell_I(1:6)*Coef_I)
       if(DoLimit) then
          Cell_I(1:3) = FineCell_I(4:6)
          Cell_I(4)  = FineCell_I(6)
          Ghost_I(3) = limit_interpolation(Ghost, Cell_I, Distance1_I,&
               IsPositiveIn=IsPositive)
       else
          Ghost_I(3) = Ghost
       endif
    endif

  end subroutine restrict_high_order_reschange
  !============================================================================
  real function calc_edge_value(CellValue_II,DoLimitIn,IsPositiveIn)

    ! For 3D, need more tests.

    real, intent(in) :: CellValue_II(6,6)
    logical, optional, intent(in):: DoLimitIn, IsPositiveIn

    logical:: DoLimit
    real:: CellValue_I(6)
    integer:: i
    integer:: iBegin=1, iEnd=6
    !--------------------------------------------------------------------------
    DoLimit = .true.
    if(present(DoLimitIn)) DoLimit = DoLimitIn

    do i = iBegin, iEnd
       CellValue_I(i) = calc_face_value(CellValue_II(i,:),DoLimit,&
            IsPositiveIn=IsPositiveIn)
    enddo
    calc_edge_value = calc_face_value(CellValue_I,DoLimit,&
         IsPositiveIn=IsPositiveIn)
  end function calc_edge_value
  !============================================================================
  subroutine get_ghost_for_fine_blk(CoarseCell_III, FineCell_I, Ghost_I, &
       UseOrder4In, IsAccurateIn_II, IsPositiveIn)

    ! 2D:
    ! __________________________
    ! |        |       |       |
    ! |        |       |  jM2  |
    ! |        |       |       |
    ! |________|_______|_______|
    ! |        |       |       |
    ! |        |       |  jM1   |
    ! |        |       |       |
    ! |________|_______|_______|_____________
    ! |        |       |       |   |    |   |
    ! |        |       |  j2   |___|____|___|
    ! |   *    |  *  ? |?  * ? |u0 | u1 |u2 |
    ! |________|_______|_______|___|____|___|
    ! |        |       |       |
    ! |        |       |  jP1   |
    ! |        |       |       |
    ! |________|_______|_______|
    ! |        |       |       |
    ! |        |       | jP2   |
    ! |        |       |       |
    ! | _______|_______|_______|

    ! The 3*5 cells represent CoarseCell_II. u0, u1, and u2 are FineCell_I.
    ! '?' are the ghost values needed.
    ! First calculate the values represented by '*', and then use these star
    ! values and u0, u1, u2 to calculate the ghost cells.

    use BATL_size, ONLY: nK

    real, intent(in):: CoarseCell_III(0:,0:,:), FineCell_I(:)
    real, intent(out):: Ghost_I(3)
    logical, optional, intent(in):: UseOrder4In, IsPositiveIn, &
         IsAccurateIn_II(0:,0:)

    integer, parameter:: Indexpp_=1,Indexp_=2,Index0_=3,Indexm_=4,Indexmm_=5
    real:: CoarseCell_I(3), CoarseCell_II(0:5,3)
    integer:: i
    logical:: DoLimit = .true., UseOrder4 = .true., IsPositive
    logical:: IsAccurate_II(0:5,0:5)

    ! Type=0: from jM2 to jP2 are accurate.
    ! Type=1: Only jM2 is not accurate.
    ! Type=2: Only jP2 is not accurate.
    ! Type=3: Both jM2 and jM1 are not accurate.
    integer:: iTypeAccurate
    !--------------------------------------------------------------------------
    DoLimit = .true.
    UseOrder4 = .false.
    if(present(UseOrder4In)) UseOrder4 = UseOrder4In
    IsPositive = .false.
    if(present(IsPositiveIn)) IsPositive = IsPositiveIn
    IsAccurate_II = .true.
    if(present(IsAccurateIn_II)) IsAccurate_II = IsAccurateIn_II

    if(nK == 1) then
       CoarseCell_II = CoarseCell_III(:,1,:)

       iTypeAccurate=0
       if(.not. all(IsAccurate_II(:,1))) then
          if(.not.IsAccurate_II(Index0_,1) .or. &
               .not. IsAccurate_II(Indexp_,1))then
             iTypeAccurate=-1
          elseif(.not. IsAccurate_II(Indexm_, 1)) then
             iTypeAccurate = 3
          elseif(.not. IsAccurate_II(Indexpp_,1) .and. &
               IsAccurate_II(Indexmm_,1)) then
             iTypeAccurate = 2
          elseif(.not. IsAccurate_II(Indexmm_,1) .and. &
               IsAccurate_II(Indexpp_,1)) then
             iTypeAccurate = 1
          endif
       endif

       if(iTypeAccurate<0) then
          CoarseCell_I = 0
       else
          do i = 1, 3
             CoarseCell_I(i) = &
                  interpolate_in_coarse_blk1d(CoarseCell_II(:,i), &
                  DoLimitIn=DoLimit, &
                  IsPositiveIn=IsPositive, &
                  iTypeInterpolateIn=iTypeAccurate)
          enddo
       endif
    else
       do i = 1, 3
          CoarseCell_I(i) = interpolate_in_coarse_blk2d(&
               CoarseCell_III(:,:,i), DoLimitIn=DoLimit,&
               UseOrder4In=UseOrder4,&
               IsAccurateIn_II=IsAccurate_II, &
               IsPositiveIn=IsPositive)
       enddo
    endif

    call interpolate_ghost_for_fine_blk(&
         CoarseCell_I,FineCell_I,Ghost_I,DoLimit,IsPositive)

  end subroutine get_ghost_for_fine_blk
  !============================================================================
  subroutine interpolate_ghost_for_fine_blk(CoarseCell_I, FineCell_I,Ghost_I,&
       DoLimit,IsPositive)

    real, intent(in):: CoarseCell_I(:), FineCell_I(:)
    real, intent(out):: Ghost_I(3)
    logical, intent(in):: DoLimit,IsPositive

    real, parameter:: c11=-4./231, c12=4./7,c13=5./7, c14=-1./3, c15=5./77
    real, parameter:: c21=-9./572, c22=1./6,c23=1.05, c24=-3./11, c25=14./195
    real, parameter:: c31=-9./286, c32=5./7,c33=0.5, c34=-20./77, c35=1./13
    real:: Ghost, Cell_I(4), Distance_I(4)
    !--------------------------------------------------------------------------
    Ghost = c11*CoarseCell_I(2) + c12*CoarseCell_I(1) + &
         c13*FineCell_I(1) + c14*FineCell_I(2) + c15*FineCell_I(3)

    if(DoLimit) then
       Distance_I(1) = -5; Distance_I(2) = -1
       Distance_I(3) = 2; Distance_I(4) = 4
       Cell_I(1) = CoarseCell_I(2); Cell_I(2) = CoarseCell_I(1)
       Cell_I(3) = FineCell_I(1); Cell_I(4) = FineCell_I(2)

       Ghost_I(1) = limit_interpolation(Ghost, Cell_I, Distance_I,&
            IsPositiveIn=IsPositive)
    else
       Ghost_I(1) = Ghost
    endif

    Ghost = c21*CoarseCell_I(3) + c22*CoarseCell_I(2) + &
            c23*CoarseCell_I(1) + c24*FineCell_I(1) + c25*FineCell_I(2)

    if(DoLimit) then
       Distance_I(1) = -7; Distance_I(2) = -3
       Distance_I(3) = 1; Distance_I(4) = 4
       Cell_I(1) = CoarseCell_I(3); Cell_I(2) = CoarseCell_I(2)
       Cell_I(3) = CoarseCell_I(1); Cell_I(4) = FineCell_I(1)

       Ghost_I(2) = limit_interpolation(Ghost, Cell_I, Distance_I,&
            IsPositiveIn=IsPositive)
    else
       Ghost_I(2) = Ghost
    endif

    Ghost = c31*CoarseCell_I(3) + c32*CoarseCell_I(2) + &
         c33*CoarseCell_I(1) + c34*FineCell_I(1) + c35*FineCell_I(2)

    if(DoLimit) then
       Distance_I(1) = -5; Distance_I(2) = -1
       Distance_I(3) = 3; Distance_I(4) = 6
       Cell_I(1) = CoarseCell_I(3); Cell_I(2) = CoarseCell_I(2)
       Cell_I(3) = CoarseCell_I(1); Cell_I(4) = FineCell_I(1)

       Ghost_I(3) = limit_interpolation(Ghost, Cell_I, Distance_I,&
            IsPositiveIn=IsPositive)
    else
       Ghost_I(3) = Ghost
    endif

  end subroutine interpolate_ghost_for_fine_blk
  !============================================================================
  real function interpolate_in_coarse_blk1d(Cell_I, DoLimitIn, UseOrder4In,&
       IsPositiveIn, iTypeInterpolateIn)

    real, intent(in):: Cell_I(0:)
    logical, optional, intent(in):: DoLimitIn, UseOrder4In, IsPositiveIn
    integer, optional, intent(in):: iTypeInterpolateIn

    logical:: DoLimit, UseOrder4, IsPositive
    integer:: iTypeInterpolate
    real   :: Cp3, Cpp, Cp, c0, Cm, Cmm
    real   :: CellLimit_I(4)
    real, parameter:: c1over4 = 0.25, c3over4 = 0.75, c6=0.6

    integer, parameter:: Ip3_=0, Ipp_=1, Ip_=2, I_=3, Im_=4, Imm_=5
    real:: Temp, Distance_I(4)=[-7,-3,1,5]
    logical :: IsCell1Accurate, IsCell4Accurate
    !--------------------------------------------------------------------------
    DoLimit = .true.
    if(present(DoLimitIn)) DoLimit = DoLimitIn
    UseOrder4 = .false.
    if(present(UseOrder4In)) UseOrder4 = UseOrder4In
    IsPositive = .false.
    if(present(IsPositiveIn)) IsPositive = IsPositiveIn

    iTypeInterpolate = 0
    if(present(iTypeInterpolateIn)) iTypeInterpolate = iTypeInterpolateIn

    CellLimit_I = Cell_I(Ipp_:Im_)
    IsCell1Accurate = .true.
    IsCell4Accurate = .true.
    if(UseOrder4 .or. iTypeInterpolate == 1) then
       ! Cell_I(5) is not used.
       Cp3=0; Cpp = -5./128;  Cp = 35./128
       c0  = 105./128; Cm = -7./128
       Cmm = 0
    elseif(iTypeInterpolate == 2) then
       ! Use Cell_I(ip_:imm_) fourth-order
       Cp3=0; Cpp = 0;  Cp = 15./128
       c0  = 135./128; Cm = -27./128
       Cmm = 5./128
       CellLimit_I(1)=CellLimit_I(2)
       IsCell1Accurate = .false.
    elseif(iTypeInterpolate == 3) then
       ! Use Cell_I(ip3_:i2_) fourth_order
       Cp3=7./128; Cpp = -33./128;  Cp = 77./128
       c0  = 77./128; Cm = 0
       Cmm = 0
       CellLimit_I(4) = CellLimit_I(3)
       IsCell4Accurate = .false.
    else
       Cp3 = 0; Cpp=-45./2048; Cp=105./512
       c0=945./1024 ; Cm=-63./512
       Cmm=35./2048
    endif

    Temp = Cp3*Cell_I(Ip3_) + Cpp*Cell_I(Ipp_) + Cp*Cell_I(Ip_) + &
         c0*Cell_I(I_) + Cm*Cell_I(Im_) + Cmm*Cell_I(Imm_)
    if(DoLimit) then
       interpolate_in_coarse_blk1d = &
            limit_interpolation(Temp, CellLimit_I, Distance_I, &
            IsCell1AccurateIn=IsCell1Accurate, &
            IsCell4AccurateIn=IsCell4Accurate)
    else
       interpolate_in_coarse_blk1d = Temp
    endif

    if(IsPositive) then
       Temp = c3over4*Cell_I(I_) + c1over4*Cell_I(Ip_)
       if(interpolate_in_coarse_blk1d < c6*Temp) &
            interpolate_in_coarse_blk1d = Temp
    endif

  end function interpolate_in_coarse_blk1d
  !============================================================================
  real function interpolate_in_coarse_blk_amr1d( &
       Cell_I, DoLimitIn, IsPositiveIn)

    real, intent(in):: Cell_I(:)
    logical, optional, intent(in):: DoLimitIn, IsPositiveIn

    logical:: DoLimit, IsPositive
    real   :: Cpp, Cp, c0, Cm, Cmm
    real, parameter:: c1over4 = 0.25, c3over4 = 0.75, c6=0.6

    integer, parameter:: Ipp_=1, Ip_=2, I_=3, Im_=4, Imm_=5
    real:: Temp, Distance_I(4)=[-7,-3,1,5]
    !--------------------------------------------------------------------------
    DoLimit = .true.
    if(present(DoLimitIn)) DoLimit = DoLimitIn
    IsPositive = .false.
    if(present(IsPositiveIn)) IsPositive = IsPositiveIn

    Cpp=-45./2048; Cp=105./512
    c0=945./1024 ; Cm=-63./512
    Cmm=35./2048

    Temp = Cpp*Cell_I(Ipp_) + Cp*Cell_I(Ip_) + &
         c0*Cell_I(I_) + Cm*Cell_I(Im_) + Cmm*Cell_I(Imm_)
    if(DoLimit) then
       interpolate_in_coarse_blk_amr1d = &
            limit_interpolation(Temp, Cell_I(Ipp_:Im_), Distance_I)
    else
       interpolate_in_coarse_blk_amr1d = Temp
    endif

    if(IsPositive) then
       Temp = c3over4*Cell_I(I_) + c1over4*Cell_I(Ip_)
       if(interpolate_in_coarse_blk_amr1d < c6*Temp) &
            interpolate_in_coarse_blk_amr1d = Temp
    endif

  end function interpolate_in_coarse_blk_amr1d
  !============================================================================
  real function interpolate_in_coarse_blk2d(Cell_II, DoLimitIn, &
       UseOrder4In,IsAccurateIn_II,IsPositiveIn)

    real, intent(in):: Cell_II(0:5,0:5)
    logical, optional,intent(in):: DoLimitIn, UseOrder4In,&
         IsAccurateIn_II(0:5,0:5), IsPositiveIn

    logical:: DoLimit, UseOrder4
    logical:: IsAccurate_II(0:5,0:5)
    real:: Cell_I(0:5)
    integer:: i, iTypeAccurate
    integer, parameter:: Indexpp_=1, Indexp_=2, Index0_=3, &
         Indexm_=4, Indexmm_=5
    !--------------------------------------------------------------------------
    DoLimit = .true.
    if(present(DoLimitIn)) DoLimit = DoLimitIn
    UseOrder4 = .false.
    if(present(UseOrder4In)) UseOrder4 = UseOrder4In
    IsAccurate_II = .true.
    if(present(IsAccurateIn_II)) IsAccurate_II = IsAccurateIn_II

    do i = 0, 5 ! Eliminate j dimension
       iTypeAccurate=0
       if(.not. all(IsAccurate_II(i,:))) then
          if(.not.IsAccurate_II(i, Index0_) .or. &
               .not. IsAccurate_II(i, Indexp_))then
             iTypeAccurate=-1
          elseif(.not. IsAccurate_II(i, Indexm_)) then
             iTypeAccurate = 3
          elseif(.not. IsAccurate_II(i, Indexpp_) .and. &
               IsAccurate_II(i, Indexmm_)) then
             iTypeAccurate = 2
          elseif(.not. IsAccurate_II(i, Indexmm_) .and. &
               IsAccurate_II(i, Indexpp_)) then
             iTypeAccurate = 1
          endif
       endif
       Cell_I(i) = &
            interpolate_in_coarse_blk1d(Cell_II(i,:), DoLimit, &
            UseOrder4, IsPositiveIn=IsPositiveIn,&
            iTypeInterpolateIn=iTypeAccurate)
    enddo

    iTypeAccurate=0
    if(.not. all(IsAccurate_II)) then
       if(.not.IsAccurate_II(Index0_,indexp_) .or. &
            .not. IsAccurate_II(Index0_,Indexp_))then
          iTypeAccurate=-1
       elseif(.not. IsAccurate_II(Indexm_, Index0_)) then
          iTypeAccurate = 3
       elseif(.not. IsAccurate_II(Indexpp_, Index0_) .and. &
            IsAccurate_II(Indexmm_,Index0_)) then
          iTypeAccurate = 2
       elseif(.not. IsAccurate_II(Indexmm_,Index0_) .and. &
            IsAccurate_II(Indexpp_,Index0_)) then
          iTypeAccurate = 1
       endif
    endif

    interpolate_in_coarse_blk2d = &
         interpolate_in_coarse_blk1d(Cell_I, DoLimit, UseOrder4,&
         IsPositiveIn=IsPositiveIn, iTypeInterpolateIn=iTypeAccurate)
  end function interpolate_in_coarse_blk2d
  !============================================================================
  subroutine prolong_high_order_face_ghost(&
       iBlock, nVar, Field1_VG, Field_VG, DoOrder5Face_G, IsPositiveIn_V)

    ! High order prolongation for simple resolution change (resolution
    ! change in only one direction).

    use BATL_tree, ONLY: DiLevelNei_IIIB, iNodeNei_IIIB
    use BATL_size, ONLY: MinI, MaxI, MinJ, MaxJ, MinK, MaxK, &
         nI, nJ, nK, i0_,i2_,j0_, j2_, nJp1_, nJm1_, k0_, k2_, nKp1_, nKm1_, &
         jm2_,jm1_,nJm2_,nJm1_,nJp2_,nJp3_,km2_,km1_,nKm2_,nKm1_,nKp2_,&
         nKp3_,im2_,im1_,nIm2_,nIm1_,nIp1_,nIp2_,nIp3_, i3_,j3_,k3_

    integer, intent(in) :: iBlock, nVar
    real, intent(inout) :: Field1_VG(nVar,MinI:MaxI,MinJ:MaxJ,MinK:MaxK)
    real, intent(inout) :: Field_VG(nVar,MinI:MaxI,MinJ:MaxJ,MinK:MaxK)
    logical, intent(in) :: DoOrder5Face_G(MinI:MaxI,MinJ:MaxJ,MinK:MaxK)
    logical, optional, intent(in):: IsPositiveIn_V(nVar)

    integer :: i1, j1, k1, i2, j2, k2
    integer :: iP1, iM1, jP1, jM1, kP1, kM1, &
         jP2, jM2, iP2, iM2, kP2, kM2, iP3, jP3, kP3, iVar

    real :: FieldFine_VI(nVar,3),Ghost_I(3)
    real :: FieldCoarse_VIII(nVar,0:5,0:5,3)

    integer:: iNode1, iNode2, iNode3, iNode0

    integer:: i1_I(0:5), i2_I(0:5)
    integer:: iPara1,iPara2, i, j, k

    logical:: IsAccurate_II(0:5,0:5), IsIpCoarse, IsImCoarse,&
         IsJpCoarse, IsJmCoarse, IsKmCoarse, IsKpCoarse

    integer:: iDir, jDir, kDir
    integer:: iBegin, iEnd, jBegin, jEnd, kBegin, kEnd

    logical:: IsPositive_V(nVar)

    ! If it is non-simple resolution change, some face ghost cells does not
    ! have enough information to reach 5th order accuracy, then use 4th order
    ! interpolation. These 4th order face ghost cells will be sent to neighbour
    ! block as edge/corner ghost cells. As face ghost cell value, it will
    ! be overwritten by remote prolongation (iSendStage == 3).
    logical:: UseOrder4

    character(len=*), parameter:: NameSub = 'prolong_high_order_face_ghost'
    !--------------------------------------------------------------------------
    IsPositive_V = .false.
    if(present(IsPositiveIn_V)) IsPositive_V = IsPositiveIn_V

    Field1_VG = Field_VG
    FieldCoarse_VIII=0

    ! Do six faces
    if(DiLevelNei_IIIB(-1,0,0,iBlock) == 1)then
       IsJmCoarse = .true.; IsJpCoarse = .true.
       IsKmCoarse = .true.; IsKpCoarse = .true.

       ! Find out whether edge neighbors are fine blocks.
       do iDir = -1, -1; do jDir = -1, 1; do kDir = -1*min(1,nK-1), min(1,nK-1)
          jBegin = 1; jEnd = nJ
          kBegin = 1; kEnd = nK

          ! Is edge neighbor fine (the same level as iBlock)?
          if(abs(jDir) + abs(kDir) /= 1) CYCLE
          if(DiLevelNei_IIIB(iDir,jDir,kDir,iBlock)/=0) CYCLE

          if(jDir == -1) IsJmCoarse = .false.
          if(jDir ==  1) IsJpCoarse = .false.
          if(kDir == -1) IsKmCoarse = .false.
          if(kDir ==  1) IsKpCoarse = .false.
       enddo; enddo; enddo

       do k1=kBegin, kEnd, 2; do j1=jBegin, jEnd, 2;
          do k2 = k1,k1+min(1,nK-1); do j2 = j1,j1+1
             jP1 = 3*j2 - 2*j1 -1 ; jM1 = 4*j1 -3*j2 +2
             jP2 = 7*j2 - 6*j1 -3; jM2 = 8*j1 - 7*j2 + 4
             jP3 = min(max(2*jP2-jP1,jm2_),nJp2_)

             iNode0 = iNodeNei_IIIB(0,0,1,iBlock)
             iNode1 = iNodeNei_IIIB(0,1,1,iBlock)
             iNode2 = iNodeNei_IIIB(0,2,1,iBlock)
             iNode3 = iNodeNei_IIIB(0,3,1,iBlock)

             if(DiLevelNei_IIIB(-1,-1,0,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(0,-1,0,iBlock) == 1 .or. &
                  iNode0 /= iNode1)) then
                ! Be careful what are stored in the corner ghost cells!!
                jP2 = max(-1, jP2)
                jM2 = max(-1, jM2)
             endif

             if(DiLevelNei_IIIB(-1,1,0,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(0,1,0,iBlock) == 1 .or. &
                  iNode2 /= iNode3)) then
                jP2 = min(jP2, nJ+2)
                jM2 = min(jM2, nJ+2)
             endif

             if(nK == 1)then
                kP1  = 1; kM1  = 1
                kP2 = 1; kM2 = 1
                kP3 = 1;
             else
                kP1  = 3*k2 - 2*k1 -1; kM1  = 4*k1 -3*k2 +2
                kP2 = 7*k2 - 6*k1 -3; kM2 = 8*k1 -7*k2 +4
                kP3 = min(max(2*kP2-kP1,km2_),nKp2_)
             end if

             iNode0 = iNodeNei_IIIB(0,1,0,iBlock)
             iNode1 = iNodeNei_IIIB(0,1,1,iBlock)
             iNode2 = iNodeNei_IIIB(0,1,2,iBlock)
             iNode3 = iNodeNei_IIIB(0,1,3,iBlock)

             if(DiLevelNei_IIIB(-1,0,-1,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(0,0,-1,iBlock) == 1 .or. &
                  iNode0 /= iNode1)) then
                kP2 = max(-1, kP2)
                kM2 = max(-1, kM2)
             endif

             if(DiLevelNei_IIIB(-1,0,1,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(0,0,1,iBlock) == 1 .or. &
                  iNode2 /= iNode3)) then
                kP2 = min(kP2, nK+2)
                kM2 = min(kM2, nK+2)
             endif

             ! Works for both 2D and 3D.
             i1_I(0) = jP3
             i1_I(1) = jP2
             i1_I(2) = jP1
             i1_I(3) = j2
             i1_I(4) = jM1
             i1_I(5) = jM2

             i2_I(0) = kP3
             i2_I(1) = kP2
             i2_I(2) = kP1
             i2_I(3) = k2
             i2_I(4) = kM1
             i2_I(5) = kM2

             IsAccurate_II = .true.
             do k = 0, min(5,nK)
                iPara2 = i2_I(k)
                do j = 0, 5
                   iPara1 = i1_I(j)
                   if(((iPara1 < 1 .and. .not.IsJmCoarse) .or. &
                        (iPara1 > nJ .and. .not.IsJpCoarse) .or. & ! j-dir
                        (iPara2 < 1 .and. .not.IsKmCoarse) .or. &
                        (iPara2 > nK .and. .not.IsKpCoarse))) & ! k-dir
                        IsAccurate_II(j,k) = .false.

                   do i = 0, im2_, -1
                      FieldCoarse_VIII(:,j,k,1-i) = &
                           Field1_VG(:,i,iPara1,iPara2)
                   end do
                enddo
             enddo

             FieldFine_VI(:,1) = Field1_VG(:,1,  j2,k2)
             FieldFine_VI(:,2) = Field1_VG(:,i2_,j2,k2)
             FieldFine_VI(:,3) = Field1_VG(:,i3_,j2,k2)

             UseOrder4 = .not.DoOrder5Face_G(i0_,j2,k2)
             do iVar = 1, nVar
                call get_ghost_for_fine_blk(FieldCoarse_VIII(iVar,:,:,:), &
                     FieldFine_VI(iVar,:), Ghost_I, UseOrder4, &
                     IsAccurateIn_II=IsAccurate_II, &
                     IsPositiveIn=IsPositive_V(iVar))
                Field_VG(iVar,i0_ ,j2,k2) = Ghost_I(1)
                Field_VG(iVar,im1_,j2,k2) = Ghost_I(2)
                Field_VG(iVar,im2_,j2,k2) = Ghost_I(3)
             enddo

          end do; end do
       end do; end do
    end if

    if(DiLevelNei_IIIB(1,0,0,iBlock) == 1)then
       IsJmCoarse = .true.; IsJpCoarse = .true.
       IsKmCoarse = .true.; IsKpCoarse = .true.

       ! Find out whether edge neighbors are fine blocks.
       do iDir = 1, 1; do jDir = -1, 1; do kDir = -1*min(1,nK-1), min(1,nK-1)
          jBegin = 1; jEnd = nJ
          kBegin = 1; kEnd = nK

          ! Is edge neighbor fine (the same level as iBlock)?
          if(abs(jDir) + abs(kDir) /= 1) CYCLE
          if(DiLevelNei_IIIB(iDir,jDir,kDir,iBlock)/=0) CYCLE

          if(jDir == -1) IsJmCoarse = .false.
          if(jDir ==  1) IsJpCoarse = .false.
          if(kDir == -1) IsKmCoarse = .false.
          if(kDir ==  1) IsKpCoarse = .false.
       enddo; enddo; enddo

       do k1=kBegin, kEnd, 2; do j1=jBegin, jEnd, 2
          do k2 = k1,k1+min(1,nK-1); do j2 = j1,j1+1
             jP1 = 3*j2 - 2*j1 -1 ; jM1 = 4*j1 -3*j2 +2
             jP2 = 7*j2 - 6*j1 -3; jM2 = 8*j1 - 7*j2 + 4
             jP3 = min(max(2*jP2-jP1,jm2_),nJp2_)

             iNode0 = iNodeNei_IIIB(3,0,1,iBlock)
             iNode1 = iNodeNei_IIIB(3,1,1,iBlock)
             iNode2 = iNodeNei_IIIB(3,2,1,iBlock)
             iNode3 = iNodeNei_IIIB(3,3,1,iBlock)

             if(DiLevelNei_IIIB(1,-1,0,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(0,-1,0,iBlock) == 1 .or. &
                  iNode0 /= iNode1)) then
                jP2 = max(-1, jP2)
                jM2 = max(-1, jM2)
             endif

             if(DiLevelNei_IIIB(1,1,0,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(0,1,0,iBlock) == 1 .or. &
                  iNode2 /= iNode3)) then
                jP2 = min(jP2, nJ+2)
                jM2 = min(jM2, nJ+2)
             endif

             if(nK == 1)then
                kP1 = 1; kM1 = 1
                kP2= 1; kM2= 1
                kP3=1
             else
                kP1  = 3*k2 - 2*k1 -1 ; kM1 = 4*k1 -3*k2 +2
                kP2 = 7*k2 - 6*k1 -3; kM2 = 8*k1 -7*k2 +4
                kP3 = min(max(2*kP2-kP1,km2_),nKp2_)
             end if

             iNode0 = iNodeNei_IIIB(3,1,0,iBlock)
             iNode1 = iNodeNei_IIIB(3,1,1,iBlock)
             iNode2 = iNodeNei_IIIB(3,1,2,iBlock)
             iNode3 = iNodeNei_IIIB(3,1,3,iBlock)

             if(DiLevelNei_IIIB(1,0,-1,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(0,0,-1,iBlock) == 1 .or. &
                  iNode0 /= iNode1)) then
                kP2 = max(-1, kP2)
                kM2 = max(-1, kM2)
             endif

             if(DiLevelNei_IIIB(1,0,1,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(0,0,1,iBlock) == 1 .or. &
                  iNode2 /= iNode3)) then
                kP2 = min(kP2, nK+2)
                kM2 = min(kM2, nK+2)
             endif

             i1_I(0) = jP3
             i1_I(1) = jP2
             i1_I(2) = jP1
             i1_I(3) = j2
             i1_I(4) = jM1
             i1_I(5) = jM2

             i2_I(0) = kP3
             i2_I(1) = kP2
             i2_I(2) = kP1
             i2_I(3) = k2
             i2_I(4) = kM1
             i2_I(5) = kM2

             IsAccurate_II = .true.
             do k = 0, min(5,nK)
                iPara2 = i2_I(k)
                do j = 0, 5
                   iPara1 = i1_I(j)
                   if(((iPara1 < 1 .and. .not.IsJmCoarse) .or. &
                        (iPara1 > nJ .and. .not.IsJpCoarse) .or. & ! j-dir
                        (iPara2 < 1 .and. .not.IsKmCoarse) .or. &
                        (iPara2 > nK .and. .not.IsKpCoarse))) & ! k-dir
                        IsAccurate_II(j,k) = .false.

                   do i = nIp1_, nIp3_
                      FieldCoarse_VIII(:,j,k,i-nI) = &
                           Field1_VG(:,i,iPara1,iPara2)
                   end do
                enddo
             enddo

             FieldFine_VI(:,1) = Field1_VG(:,nI,j2,k2)
             FieldFine_VI(:,2) = Field1_VG(:,nIm1_,j2,k2)
             FieldFine_VI(:,3) = Field1_VG(:,nIm2_,j2,k2)

             UseOrder4 = .not.DoOrder5Face_G(nIp1_,j2,k2)
             do iVar = 1, nVar
                call get_ghost_for_fine_blk(FieldCoarse_VIII(iVar,:,:,:),&
                     FieldFine_VI(iVar,:), Ghost_I, UseOrder4,&
                     IsAccurateIn_II = IsAccurate_II, &
                     IsPositiveIn=IsPositive_V(iVar))

                Field_VG(iVar,nIp1_,j2,k2) = Ghost_I(1)
                Field_VG(iVar,nIp2_,j2,k2) = Ghost_I(2)
                Field_VG(iVar,nIp3_,j2,k2) = Ghost_I(3)
             enddo

          end do; end do
       end do; end do
    end if

    if(DiLevelNei_IIIB(0,-1,0,iBlock) == 1)then

       IsImCoarse = .true.; IsIpCoarse = .true.
       IsKmCoarse = .true.; IsKpCoarse = .true.

       ! Find out whether edge neighbors are fine blocks.
       do iDir = -1, 1; do jDir = -1, -1; do kDir = -1*min(1,nK-1), min(1,nK-1)
          iBegin = 1; iEnd = nI
          kBegin = 1; kEnd = nK

          ! Is edge neighbor fine (the same level as iBlock)?
          if(abs(iDir) + abs(kDir) /= 1) CYCLE
          if(DiLevelNei_IIIB(iDir,jDir,kDir,iBlock)/=0) CYCLE

          if(iDir == -1) IsImCoarse = .false.
          if(iDir ==  1) IsIpCoarse = .false.
          if(kDir == -1) IsKmCoarse = .false.
          if(kDir ==  1) IsKpCoarse = .false.
       enddo; enddo; enddo

       do k1=kBegin, kEnd, 2; do i1=iBegin, iEnd, 2
          do k2 = k1,k1+min(1,nK-1); do i2 = i1,i1+1
             iP1  = 3*i2 - 2*i1 -1; iM1  = 4*i1 - 3*i2 + 2
             iP2 = 7*i2 - 6*i1 -3; iM2 = 8*i1 - 7*i2 + 4
             iP3 = min(max(2*iP2-iP1,im2_),nIp2_)

             iNode0 = iNodeNei_IIIB(0,0,1,iBlock)
             iNode1 = iNodeNei_IIIB(1,0,1,iBlock)
             iNode2 = iNodeNei_IIIB(2,0,1,iBlock)
             iNode3 = iNodeNei_IIIB(3,0,1,iBlock)

             if(DiLevelNei_IIIB(-1,-1,0,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(-1,0,0,iBlock) == 1 .or. &
                  iNode0 /= iNode1)) then
                iP2 = max(-1, iP2)
                iM2 = max(-1, iM2)
             endif

             if(DiLevelNei_IIIB(1,-1,0,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(1,0,0,iBlock) == 1 .or. &
                  iNode2 /= iNode3)) then
                iP2 = min(iP2, nI+2)
                iM2 = min(iM2, nI+2)
             endif

             if(nK == 1)then
                kP1  = 1; kM1  = 1
                kP2 = 1; kM2 = 1
                kP3 = 1;
             else
                kP1  = 3*k2 - 2*k1 -1 ; kM1 = 4*k1 -3*k2 +2
                kP2 = 7*k2 - 6*k1 -3; kM2 = 8*k1 -7*k2 +4
                kP3 = min(max(2*kP2-kP1,km2_),nKp2_)
             end if

             iNode0 = iNodeNei_IIIB(1,0,0,iBlock)
             iNode1 = iNodeNei_IIIB(1,0,1,iBlock)
             iNode2 = iNodeNei_IIIB(1,0,2,iBlock)
             iNode3 = iNodeNei_IIIB(1,0,3,iBlock)

             if(DiLevelNei_IIIB(0,-1,-1,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(0,0,-1,iBlock) == 1 .or. &
                  iNode0 /= iNode1)) then
                kP2 = max(-1, kP2)
                kM2 = max(-1, kM2)
             endif

             if(DiLevelNei_IIIB(0,-1,1,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(0,0,1,iBlock) == 1 .or. &
                  iNode2 /= iNode3)) then
                kP2 = min(kP2, nK+2)
                kM2 = min(kM2, nK+2)
             endif

             i1_I(0) = iP3
             i1_I(1) = iP2
             i1_I(2) = iP1
             i1_I(3) = i2
             i1_I(4) = iM1
             i1_I(5) = iM2

             i2_I(0) = kP3
             i2_I(1) = kP2
             i2_I(2) = kP1
             i2_I(3) = k2
             i2_I(4) = kM1
             i2_I(5) = kM2

             IsAccurate_II=.true.
             do k = 0, min(5,nK)
                iPara2 = i2_I(k)
                do i = 0, 5
                   iPara1 = i1_I(i)
                   if(((iPara1 < 1 .and. .not.IsImCoarse) .or. &
                        (iPara1 > nI .and. .not.IsIpCoarse) .or. & ! i-dir
                        (iPara2 < 1 .and. .not.IsKmCoarse) .or. &
                        (iPara2 > nK .and. .not.IsKpCoarse))) & ! k-dir
                        IsAccurate_II(i,k) = .false.

                   do j = 0, jm2_, -1
                      FieldCoarse_VIII(:,i,k,1-j) = &
                           Field1_VG(:,iPara1,j,iPara2)
                   enddo
                enddo
             enddo

             FieldFine_VI(:,1) = Field1_VG(:,i2,1,k2)
             FieldFine_VI(:,2) = Field1_VG(:,i2,j2_,k2)
             FieldFine_VI(:,3) = Field1_VG(:,i2,j3_,k2)

             UseOrder4 = .not.DoOrder5Face_G(i2,j0_,k2)
             do iVar = 1, nVar
                call get_ghost_for_fine_blk(FieldCoarse_VIII(iVar,:,:,:), &
                     FieldFine_VI(iVar,:), Ghost_I, UseOrder4,&
                     IsAccurateIn_II = IsAccurate_II, &
                     IsPositiveIn=IsPositive_V(iVar))
                Field_VG(iVar,i2,j0_ ,k2) = Ghost_I(1)
                Field_VG(iVar,i2,jm1_,k2) = Ghost_I(2)
                Field_VG(iVar,i2,jm2_,k2) = Ghost_I(3)
             enddo

          end do; end do
       end do; end do
    end if

    if(DiLevelNei_IIIB(0,1,0,iBlock) == 1)then
       IsImCoarse = .true.; IsIpCoarse = .true.
       IsKmCoarse = .true.; IsKpCoarse = .true.

       ! Find out whether edge neighbors are fine blocks.
       do iDir = -1, 1; do jDir = 1, 1; do kDir = -1*min(1,nK-1), min(1,nK-1)
          iBegin = 1; iEnd = nI
          kBegin = 1; kEnd = nK

          ! Is edge neighbor fine (the same level as iBlock)?
          if(abs(iDir) + abs(kDir) /= 1) CYCLE
          if(DiLevelNei_IIIB(iDir,jDir,kDir,iBlock)/=0) CYCLE

          if(iDir == -1) IsImCoarse = .false.
          if(iDir ==  1) IsIpCoarse = .false.
          if(kDir == -1) IsKmCoarse = .false.
          if(kDir ==  1) IsKpCoarse = .false.
       enddo; enddo; enddo

       do k1=kBegin, kEnd, 2; do i1=iBegin, iEnd, 2
          do k2 = k1,k1+min(1,nK-1); do i2 = i1,i1+1
             iP1 = 3*i2 - 2*i1 -1 ; iM1 = 4*i1 -3*i2 +2
             iP2 = 7*i2 - 6*i1 -3; iM2 = 8*i1 - 7*i2 + 4
             iP3 = min(max(2*iP2-iP1,im2_),nIp2_)

             iNode0 = iNodeNei_IIIB(0,3,1,iBlock)
             iNode1 = iNodeNei_IIIB(1,3,1,iBlock)
             iNode2 = iNodeNei_IIIB(2,3,1,iBlock)
             iNode3 = iNodeNei_IIIB(3,3,1,iBlock)

             if(DiLevelNei_IIIB(-1,1,0,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(-1,0,0,iBlock) == 1 .or. &
                  iNode0 /=iNode1)) then
                iP2 = max(-1, iP2)
                iM2 = max(-1, iM2)
             endif

             if(DiLevelNei_IIIB(1,1,0,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(1,0,0,iBlock) == 1 .or. &
                  iNode2 /= iNode3)) then
                iP2 = min(iP2, nI+2)
                iM2 = min(iM2, nI+2)
             endif

             if(nK == 1)then
                kP1 = 1; kM1 = 1
                kP2 = 1; kM2 = 1
                kP3 =1;
             else
                kP1 = 3*k2 - 2*k1 -1 ; kM1 = 4*k1 -3*k2 +2
                kP2 = 7*k2 - 6*k1 -3; kM2 = 8*k1 -7*k2 +4
                kP3 = min(max(2*kP2-kP1,km2_),nKp2_)
             end if

             iNode0 = iNodeNei_IIIB(1,3,0,iBlock)
             iNode1 = iNodeNei_IIIB(1,3,1,iBlock)
             iNode2 = iNodeNei_IIIB(1,3,2,iBlock)
             iNode3 = iNodeNei_IIIB(1,3,3,iBlock)

             if(DiLevelNei_IIIB(0,1,-1,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(0,0,-1,iBlock) == 1 .or. &
                  iNode0 /=iNode1)) then
                kP2 = max(-1, kP2)
                kM2 = max(-1, kM2)
             endif

             if(DiLevelNei_IIIB(0,1,1,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(0,0,1,iBlock) == 1 .or. &
                  iNode2 /= iNode3)) then
                kP2 = min(kP2, nK+2)
                kM2 = min(kM2, nK+2)
             endif

             i1_I(0) = iP3
             i1_I(1) = iP2
             i1_I(2) = iP1
             i1_I(3) = i2
             i1_I(4) = iM1
             i1_I(5) = iM2

             i2_I(0) = kP3
             i2_I(1) = kP2
             i2_I(2) = kP1
             i2_I(3) = k2
             i2_I(4) = kM1
             i2_I(5) = kM2

             IsAccurate_II = .true.
             do k = 0, min(5,nK)
                iPara2 = i2_I(k)
                do i = 0, 5
                   iPara1 = i1_I(i)
                   if(((iPara1 < 1 .and. .not.IsImCoarse) .or. &
                        (iPara1 > nI .and. .not.IsIpCoarse) .or. & ! i-dir
                        (iPara2 < 1 .and. .not.IsKmCoarse) .or. &
                        (iPara2 > nK .and. .not.IsKpCoarse))) & ! k-dir
                        IsAccurate_II(i,k) = .false.

                   do j = nJp1_, nJp3_
                      FieldCoarse_VIII(:,i,k,j-nJp1_+1) = &
                           Field1_VG(:,iPara1,j,iPara2)
                   end do
                enddo
             enddo

             FieldFine_VI(:,1) = Field1_VG(:,i2,nJ,   k2)
             FieldFine_VI(:,2) = Field1_VG(:,i2,nJm1_,k2)
             FieldFine_VI(:,3) = Field1_VG(:,i2,nJm2_,k2)

             UseOrder4 = .not.DoOrder5Face_G(i2,nJp1_,k2)

             do iVar = 1, nVar
                call get_ghost_for_fine_blk(FieldCoarse_VIII(iVar,:,:,:), &
                     FieldFine_VI(iVar,:), Ghost_I, UseOrder4,&
                     IsAccurateIn_II=IsAccurate_II, &
                     IsPositiveIn=IsPositive_V(iVar))
                Field_VG(iVar,i2,nJp1_, k2) = Ghost_I(1)
                Field_VG(iVar,i2,nJp2_, k2) = Ghost_I(2)
                Field_VG(iVar,i2,nJp3_, k2) = Ghost_I(3)
             enddo
          end do; end do
       end do; end do
    end if

    if(nK == 1) RETURN

    if(DiLevelNei_IIIB(0,0,-1,iBlock) == 1) then
       IsImCoarse = .true.; IsIpCoarse = .true.
       IsJmCoarse = .true.; IsJpCoarse = .true.

       ! Find out whether edge neighbors are fine blocks.
       do iDir = -1, 1; do jDir = -1, 1; do kDir = -1, -1
          iBegin = 1; iEnd = nI
          jBegin = 1; jEnd = nJ

          ! Is edge neighbor fine (the same level as iBlock)?
          if(abs(iDir) + abs(jDir) /= 1) CYCLE
          if(DiLevelNei_IIIB(iDir,jDir,kDir,iBlock)/=0) CYCLE

          if(iDir == -1) IsImCoarse = .false.
          if(iDir ==  1) IsIpCoarse = .false.
          if(jDir == -1) IsJmCoarse = .false.
          if(jDir ==  1) IsJpCoarse = .false.
       enddo; enddo; enddo

       do j1 = jBegin, jEnd, 2; do i1 = iBegin, iEnd, 2
          do j2 = j1, j1+1; do i2 = i1, i1+1
             iP1  = 3*i2 - 2*i1 -1; iM1  = 4*i1 - 3*i2 + 2
             iP2 = 7*i2 - 6*i1 -3; iM2 = 8*i1 - 7*i2 + 4
             iP3 = min(max(2*iP2-iP1,im2_),nIp2_)

             iNode0 = iNodeNei_IIIB(0,1,0,iBlock)
             iNode1 = iNodeNei_IIIB(1,1,0,iBlock)
             iNode2 = iNodeNei_IIIB(2,1,0,iBlock)
             iNode3 = iNodeNei_IIIB(3,1,0,iBlock)

             if(DiLevelNei_IIIB(-1,0,-1,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(-1,0,0,iBlock) == 1 .or. &
                  iNode0 /= iNode1)) then
                iP2 = max(-1, iP2)
                iM2 = max(-1, iM2)
             endif

             if(DiLevelNei_IIIB(1,0,-1,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(1,0,0,iBlock) == 1 .or. &
                  iNode2 /= iNode3)) then
                iP2 = min(iP2, nI+2)
                iM2 = min(iM2, nI+2)
             endif

             jP1 = 3*j2 - 2*j1 -1 ; jM1 = 4*j1 -3*j2 +2
             jP2 = 7*j2 - 6*j1 -3; jM2 = 8*j1 - 7*j2 + 4
             jP3 = min(max(2*jP2-jP1,jm2_),nJp2_)

             iNode0 = iNodeNei_IIIB(1,0,0,iBlock)
             iNode1 = iNodeNei_IIIB(1,1,0,iBlock)
             iNode2 = iNodeNei_IIIB(1,2,0,iBlock)
             iNode3 = iNodeNei_IIIB(1,3,0,iBlock)

             if(DiLevelNei_IIIB(0,-1,-1,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(0,-1,0,iBlock) == 1 .or. &
                  iNode0 /= iNode1)) then
                jP2 = max(-1, jP2)
                jM2 = max(-1, jM2)
             endif

             if(DiLevelNei_IIIB(0,1,-1,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(0,1,0,iBlock) == 1 .or. &
                  iNode2 /= iNode3)) then
                jP2 = min(jP2, nJ+2)
                jM2 = min(jM2, nJ+2)
             endif

             i1_I(0) = iP3
             i1_I(1) = iP2
             i1_I(2) = iP1
             i1_I(3) = i2
             i1_I(4) = iM1
             i1_I(5) = iM2

             i2_I(0) = jP3
             i2_I(1) = jP2
             i2_I(2) = jP1
             i2_I(3) = j2
             i2_I(4) = jM1
             i2_I(5) = jM2

             IsAccurate_II = .true.
             do j = 0, 5
                iPara2 = i2_I(j)
                do i = 0, 5
                   iPara1 = i1_I(i)
                   if( (iPara1 < 1 .and. .not.IsImCoarse) .or. &
                        (iPara1 > nI .and. .not.IsIpCoarse) .or. & ! j-dir
                        (iPara2 < 1 .and. .not.IsJmCoarse) .or. &
                        (iPara2 > nJ .and. .not.IsJpCoarse)) & ! k-dir
                        IsAccurate_II(i,j) = .false.

                   do k = 0, km2_, -1
                      FieldCoarse_VIII(:,i,j,1-k) = &
                           Field1_VG(:,iPara1,iPara2,k)
                   enddo
                enddo
             enddo

             FieldFine_VI(:,1) = Field1_VG(:,i2,j2,1)
             FieldFine_VI(:,2) = Field1_VG(:,i2,j2,k2_)
             FieldFine_VI(:,3) = Field1_VG(:,i2,j2,k3_)

             UseOrder4 = .not.DoOrder5Face_G(i2,j2,k0_)
             do iVar = 1, nVar
                call get_ghost_for_fine_blk(FieldCoarse_VIII(iVar,:,:,:), &
                     FieldFine_VI(iVar,:), Ghost_I, UseOrder4,&
                     IsAccurateIn_II=IsAccurate_II, &
                     IsPositiveIn=IsPositive_V(iVar))
                Field_VG(iVar,i2,j2,k0_ ) = Ghost_I(1)
                Field_VG(iVar,i2,j2,km1_) = Ghost_I(2)
                Field_VG(iVar,i2,j2,km2_) = Ghost_I(3)
             enddo
          enddo; enddo
       enddo; enddo

    endif

    if(DiLevelNei_IIIB(0,0,1,iBlock) == 1) then
       IsImCoarse = .true.; IsIpCoarse = .true.
       IsJmCoarse = .true.; IsJpCoarse = .true.

       ! Find out whether edge neighbors are fine blocks.
       do iDir = -1, 1; do jDir = -1, 1; do kDir = 1, 1
          iBegin = 1; iEnd = nI
          jBegin = 1; jEnd = nJ

          ! Is edge neighbor fine (the same level as iBlock)?
          if(abs(iDir) + abs(jDir) /= 1) CYCLE
          if(DiLevelNei_IIIB(iDir,jDir,kDir,iBlock)/=0) CYCLE

          if(iDir == -1) IsImCoarse = .false.
          if(iDir ==  1) IsIpCoarse = .false.
          if(jDir == -1) IsJmCoarse = .false.
          if(jDir ==  1) IsJpCoarse = .false.
       enddo; enddo; enddo

       do j1 = jBegin, jEnd, 2; do i1 = iBegin, iEnd, 2
          do j2 = j1, j1+1; do i2 = i1, i1+1
             iP1  = 3*i2 - 2*i1 -1; iM1  = 4*i1 - 3*i2 + 2
             iP2 = 7*i2 - 6*i1 -3; iM2 = 8*i1 - 7*i2 + 4
             iP3 = min(max(2*iP2-iP1,im2_),nIp2_)

             iNode0 = iNodeNei_IIIB(0,1,3,iBlock)
             iNode1 = iNodeNei_IIIB(1,1,3,iBlock)
             iNode2 = iNodeNei_IIIB(2,1,3,iBlock)
             iNode3 = iNodeNei_IIIB(3,1,3,iBlock)

             if(DiLevelNei_IIIB(-1,0,1,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(-1,0,0,iBlock) == 1 .or. &
                  iNode0 /= iNode1)) then
                iP2 = max(-1, iP2)
                iM2 = max(-1, iM2)
             endif

             if(DiLevelNei_IIIB(1,0,1,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(1,0,0,iBlock) == 1 .or. &
                  iNode2 /= iNode3)) then
                iP2 = min(iP2, nI+2)
                iM2 = min(iM2, nI+2)
             endif

             jP1  = 3*j2 - 2*j1 -1; jM1  = 4*j1 - 3*j2 +2
             jP2 = 7*j2 - 6*j1 -3; jM2 = 8*j1 - 7*j2 + 4
             jP3 = min(max(2*jP2-jP1,jm2_),nJp2_)

             iNode0 = iNodeNei_IIIB(1,0,3,iBlock)
             iNode1 = iNodeNei_IIIB(1,1,3,iBlock)
             iNode2 = iNodeNei_IIIB(1,2,3,iBlock)
             iNode3 = iNodeNei_IIIB(1,3,3,iBlock)

             if(DiLevelNei_IIIB(0,-1,1,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(0,-1,0,iBlock) == 1 .or. &
                  iNode0 /= iNode1)) then
                jP2 = max(-1, jP2)
                jM2 = max(-1, jM2)
             endif

             if(DiLevelNei_IIIB(0,1,1,iBlock) == 1 .and. &
                  (DiLevelNei_IIIB(0,1,0,iBlock) == 1 .or. &
                  iNode2 /= iNode3)) then
                jP2 = min(jP2, nJ+2)
                jM2 = min(jM2, nJ+2)
             endif

             i1_I(0) = iP3
             i1_I(1) = iP2
             i1_I(2) = iP1
             i1_I(3) = i2
             i1_I(4) = iM1
             i1_I(5) = iM2

             i2_I(0) = jP3
             i2_I(1) = jP2
             i2_I(2) = jP1
             i2_I(3) = j2
             i2_I(4) = jM1
             i2_I(5) = jM2

             IsAccurate_II = .true.
             do j = 0, 5
                iPara2 = i2_I(j)
                do i = 0, 5
                   iPara1 = i1_I(i)
                   if( (iPara1 < 1 .and. .not.IsImCoarse) .or. &
                        (iPara1 > nI .and. .not.IsIpCoarse) .or. & ! j-dir
                        (iPara2 < 1 .and. .not.IsJmCoarse) .or. &
                        (iPara2 > nJ .and. .not.IsJpCoarse)) & ! k-dir
                        IsAccurate_II(i,j) = .false.

                   do k = nkp1_, nkp3_
                      FieldCoarse_VIII(:,i,j,k-nkp1_+1) = &
                           Field1_VG(:,iPara1,iPara2,k)
                   enddo
                enddo
             enddo

             FieldFine_VI(:,1) = Field1_VG(:,i2,j2,nk   )
             FieldFine_VI(:,2) = Field1_VG(:,i2,j2,nKm1_)
             FieldFine_VI(:,3) = Field1_VG(:,i2,j2,nKm2_)

             UseOrder4 = .not.DoOrder5Face_G(i2,j2,nKp1_)
             do iVar = 1, nVar
                call get_ghost_for_fine_blk(FieldCoarse_VIII(iVar,:,:,:), &
                     FieldFine_VI(iVar,:), Ghost_I, UseOrder4,&
                     IsAccurateIn_II=IsAccurate_II, &
                     IsPositiveIn=IsPositive_V(iVar))
                Field_VG(iVar,i2,j2,nKp1_) = Ghost_I(1)
                Field_VG(iVar,i2,j2,nKp2_) = Ghost_I(2)
                Field_VG(iVar,i2,j2,nKp3_) = Ghost_I(3)
             enddo
          enddo; enddo
       enddo; enddo
    endif
  end subroutine prolong_high_order_face_ghost
  !============================================================================
  subroutine correct_face_ghost_for_fine_block(iBlock, nVar, Field_VG, &
       IsPositiveIn_V)

    use BATL_tree, ONLY: DiLevelNei_IIIB
    use BATL_size, ONLY: MinI, MaxI, MinJ, MaxJ, MinK, MaxK, &
         nI, nJ, nK

    integer, intent(in):: iBlock, nVar
    real, intent(inout):: Field_VG(nVar,MinI:MaxI,MinJ:MaxJ,MinK:MaxK)
    logical, optional,intent(in):: IsPositiveIn_V(nVar)

    logical:: IsPositive_V(nVar)
    logical:: IsCorrected_VG(nVar,MinI:MaxI,MinJ:MaxJ,MinK:MaxK)
    real:: Coef1_I(4) = ([-1./6, 2./3, 2./3, -1./6])
    real, parameter::  Distance_I(4) = [-2, -1, 1, 2]
    integer:: iDir, jDir, kDir, iDir1, jDir1, kDir1, nDir1, iDir2, jDir2, kDir2
    integer:: jBegin, jEnd, iBegin, iEnd, kBegin, kEnd
    integer:: Di, Dj, Dk, i, j, k
    integer:: Di1, Dj1, Dk1
    integer:: DiLevel, DiLevel1, iVar
    real:: Orig, CellValue_I(4)
    integer:: iFineNei_II(3,3), iEdge, iEdge1, nEdge, iStage
    logical:: IsFineNei_I(3)
    integer, parameter:: Edge1_ = 1, Edge2_ = 2, Corner_ = 3
    character(len=*), parameter:: NameSub = 'correct_face_ghost_for_fine_block'
    !--------------------------------------------------------------------------
    IsPositive_V = .false.
    if(present(IsPositiveIn_V)) IsPositive_V = IsPositiveIn_V

    if(nK == 1) then
       ! For example:

       !---------
       !|   |    |
       !|8__|_9__|
       !|   |    |
       !|6  | 7  |
       ! -------------------
       !|        | 4  |  5 |
       !|   1    |____|____|
       !|        |    |    |
       !|        | 2  | 3  |
       !--------------------

       ! For block 4, the ghost cells -2 <= i <= 0, nJ - 3 <= j <= nJ
       ! will be corredted.
       ! For block 7, the ghost cells nI-3 <= i <= nI, -2 <= j <= 0
       ! will be corredted.

       kDir = 0
       do iDir = -1, 1; do jDir = -1, 1
          if(iDir*jDir /=0) CYCLE
          DiLevel = DiLevelNei_IIIB(iDir,jDir,kDir,iBlock)
          if(DiLevel /= 1) CYCLE

          ! DiLevel = 1
          if(jDir == 0) then ! Resolution change in x-dir
             if(iDir ==1) then
                iBegin = nI+1; iEnd = nI+3; Di = 1
             else
                iBegin = 0; iEnd = -2; Di = -1
             endif

             do jDir1 = -1, 1, 2
                DiLevel1 = DiLevelNei_IIIB(iDir,jDir1,kDir,iBlock)
                if(DiLevel1 /=0) CYCLE

                if(jDir1 == -1) then
                   jBegin = 1; jEnd = 4; Dj = 1
                else
                   jBegin = nJ; jEnd = nJ - 3; Dj = -1
                endif

                k = 1
                do i = iBegin, iEnd, Di
                   do iVar = 1, nVar
                      CellValue_I(1) = Field_VG(iVar,i,jBegin-2*Dj,k)
                      CellValue_I(2) = Field_VG(iVar,i,jBegin-Dj,k)
                      CellValue_I(3) = Field_VG(iVar,i,jBegin+Dj,k)
                      CellValue_I(4) = Field_VG(iVar,i,jBegin+2*Dj,k)

                      Orig = sum(CellValue_I*Coef1_I)
                      Field_VG(iVar,i,jBegin,k) = limit_interpolation(Orig,&
                           CellValue_I, Distance_I,&
                           IsPositiveIn=IsPositive_V(iVar))
                   enddo ! iVar
                enddo ! i

             enddo ! jDir1

          elseif(iDir == 0) then ! Resolution change in y-dir.
             if(jDir == 1) then
                jBegin = nJ+1; jEnd = nJ+3; Dj = 1
             else
                jBegin = 0; jEnd = -2; Dj = -1
             endif

             do iDir1 = -1, 1, 2
                DiLevel1 = DiLevelNei_IIIB(iDir1,jDir,kDir,iBlock)
                if(DiLevel1 /=0) CYCLE

                if(iDir1 == -1) then
                   iBegin = 1; iEnd = 4; Di = 1
                else
                   iBegin = nI; iEnd = nI - 3; Di = -1
                endif

                k = 1
                do j = jBegin, jEnd, Dj
                   do iVar = 1, nVar
                      CellValue_I(1) = Field_VG(iVar,iBegin-2*Di,j,k)
                      CellValue_I(2) = Field_VG(iVar,iBegin-  Di,j,k)
                      CellValue_I(3) = Field_VG(iVar,iBegin  +  Di,j,k)
                      CellValue_I(4) = Field_VG(iVar,iBegin  +2*Di,j,k)

                      Orig = sum(CellValue_I*Coef1_I)
                      Field_VG(iVar,iBegin,j,k) = limit_interpolation(Orig,&
                           CellValue_I,Distance_I,&
                           IsPositiveIn=IsPositive_V(iVar))

                   enddo ! ivar
                enddo ! j
             enddo ! idir1
          endif

       enddo; enddo
    elseif(nK >1) then
       ! Example:
       ! ___________________________________________
       ! |                    |                    |
       ! |                    |                    |
       ! |                    |                    |
       ! |         7          |         8          |
       ! |                    |                    |
       ! |                    |                    |
       ! |                    |                    |
       ! |____________________|____________________|
       ! |                    |                    |
       ! |                    |                    |
       ! |                    |                    |
       ! |          5         |         6          |
       ! |                    |                    |
       ! |                    |                    |
       ! |                    |                    |
       ! |____________________|____________________|
       !               TOP LAYER

       ! ___________________________________________
       ! |                    |                    |
       ! |                    |                    |
       ! |                    |                    |
       ! |        3           |          4         |
       ! |                    |                    |
       ! |                    |                    |
       ! |                    |                    |        y
       ! |____________________|____________________|        |
       ! |                    |         |          |        |
       ! |                    |   11    |   12     |        -----> x
       ! |                    |         |          |
       ! |         1          |_________|__________|
       ! |                    |         |          |
       ! |                    |   9     |   10     |
       ! |                    |         |          |
       ! |____________________|_________|__________|
       !                 BOTTOM LAYER
       ! 9-12 are the top layer of previous coares block.

       ! To calculate the left face ghost cells of block 11, need to
       ! consider the refinement of block 3, 5, and 7.

       ! case 1: 3, 5, 7 are coarse, nothing need to do.
       ! case 2: only 3 or 5 is refined. 7 can be refined or not.
       !         Similar with the 2D case above.
       ! case 3: both 3 and 5 are refined. 7 can be refined or not.
       !         Some face ghost cells can be interpolated in y
       !         direction or z direction, and these cells use the
       !         average of both direction.
       ! case 4: only 7 is refined. The ghost cells: -2 <= i <= 0 .and.
       !         nJ-3 <= j <= nJ .and. nK <= k <= nK . are not 5th
       !         order. They are calculated with 4th order accurate
       !         interpolation in prolong_high_order_face_ghost.
       !         These cells may passed to block 9 and other block as
       !         edge/corner ghost cells. But themselves, as the face ghost
       !         of block 11, they will be overwritten with 5th order value
       !         when iSendStage is 3.

       ! WARNNING: This part is originally written to correct
       !           four ghost cell layers.
       !           Now, only one layer needs to be corrected and the logic
       !           becomes much simpler. Yuxi will simplify the code below.

       do kDir = -1, 1; do jDir = -1, 1; do iDir = -1, 1
          ! Loop through 6 faces.

          ! Check whether block 1 is coarse
          if(abs(kDir) + abs(jDir) + abs(iDir) /=1) CYCLE
          if(DiLevelNei_IIIB(iDir,jDir,kDir,iBlock) /=1) CYCLE

          ! Check whether block 3 and block 5 are refined.
          IsFineNei_I = .false.
          do kDir1 = -1, 1; do jDir1 = -1, 1; do iDir1 = -1, 1

             ! Only Check the neighbour blocks close to this coarse
             ! face neighbour.
             if(iDir /=0 .and. iDir1 /= iDir) CYCLE
             if(jDir /=0 .and. jDir1 /= jDir) CYCLE
             if(kDir /=0 .and. kDir1 /= kDir) CYCLE
             if(DiLevelNei_IIIB(iDir1,jDir1,kDir1,iBlock) /= 0) CYCLE

             ! Only check edge neighbours.
             nDir1 = abs(iDir1) + abs(jDir1) + abs(kDir1)
             if(nDir1 == 3) CYCLE

             ! At most 2 edges can be refined block.
             if(.not. IsFineNei_I(Edge1_)) then
                iEdge = Edge1_
             else
                iEdge = Edge2_
             endif
             if(IsFineNei_I(iEdge)) call CON_stop(NameSub//': error1')
             IsFineNei_I(iEdge) = .true.

             iFineNei_II(iEdge,1) = iDir1
             iFineNei_II(iEdge,2) = jDir1
             iFineNei_II(iEdge,3) = kDir1
          enddo; enddo; enddo

          ! Check whether block 7 is refined.
          if(.not. IsFineNei_I(Edge1_) .and. .not. IsFineNei_I(Edge2_)) then
             ! Only one corner neighbour can be refined now.
             do kDir1 = -1, 1; do jDir1 = -1, 1; do iDir1 = -1, 1
                if(iDir /=0 .and. iDir1 /= iDir) CYCLE
                if(jDir /=0 .and. jDir1 /= jDir) CYCLE
                if(kDir /=0 .and. kDir1 /= kDir) CYCLE

                if(DiLevelNei_IIIB(iDir1,jDir1,kDir1,iBlock) /= 0) CYCLE
                nDir1 = abs(iDir1) + abs(jDir1) + abs(kDir1)

                if(nDir1 /= 3) CYCLE
                if(IsFineNei_I(Corner_)) call CON_stop(NameSub//': error2')
                IsFineNei_I(Corner_) = .true.
                iFineNei_II(Corner_,1) = iDir1
                iFineNei_II(Corner_,2) = jDir1
                iFineNei_II(Corner_,3) = kDir1
             enddo; enddo; enddo
          endif

          nEdge = 1 ! case 2
          if(IsFineNei_I(Edge1_) .and. IsFineNei_I(Edge2_)) nEdge = 2 ! case 3
          IsCorrected_VG = .false.
          do iStage = 1, nEdge
             ! If nEdge is 2:
             ! iStage 1: calculate these cells can only interpolated in
             !           one direction
             ! iStage 2: for these cells can be interpolated in two
             !           directions, use the average.
             do iEdge = 1, 2
                if(.not. IsFineNei_I(iEdge)) CYCLE

                iDir1 = iFineNei_II(iEdge,1)
                jDir1 = iFineNei_II(iEdge,2)
                kDir1 = iFineNei_II(iEdge,3)

                if(nEdge == 2) then
                   iEdge1 = mod(iEdge,2) + 1
                   iDir2 = iFineNei_II(iEdge1,1)
                   jDir2 = iFineNei_II(iEdge1,2)
                   kDir2 = iFineNei_II(iEdge1,3)
                endif

                if(iDir /=0) then
                   if(iDir == 1) then
                      iBegin = nI + 1;  iEnd = nI + 3; Di = 1
                   elseif(iDir == -1) then
                      iBegin = 0; iEnd = -2; Di = -1
                   endif

                   if(jDir1 /=0) then
                      if(nEdge == 1) then
                         kBegin = 1; kEnd = nK; Dk = 1
                      else
                         if(iStage == 1) then
                            if(kDir2 ==1) then
                               kBegin = 1; kEnd = nK-1; Dk = 1
                            elseif(kDir2 == -1) then
                               kBegin = nK; kEnd = 2; Dk = -1
                            endif
                         else ! iStage == 2
                            if(kDir2 ==1) then
                               kBegin = nK; kEnd = nK; Dk = 1
                            elseif(kDir2 == -1) then
                               kBegin = 1; kEnd = 1; Dk = 1
                            endif
                         endif

                      endif

                      if(jDir1 == 1) then
                         jBegin = nJ; jEnd = jBegin; Dj = -1
                      elseif(jDir1 == -1) then
                         jBegin = 1; jEnd = jBegin; Dj = 1
                      endif

                      Di1 = 0; Dj1 = Dj; Dk1 = 0

                   elseif(kDir1 /=0) then
                      if(nEdge == 1) then
                         jBegin = 1; jEnd = nJ; Dj = 1
                      else
                         if(iStage == 1) then
                            if(jDir2 ==1) then
                               jBegin = 1; jEnd = nJ-1; Dj = 1
                            elseif(jDir2 == -1) then
                               jBegin = nJ; jEnd = 2; Dj = -1
                            endif
                         else ! iStage = 2
                            if(jDir2 ==1) then
                               jBegin = nJ; jEnd = nJ; Dj = 1
                            elseif(jDir2 == -1) then
                               jBegin = 1; jEnd = 1; Dj = 1
                            endif
                         endif
                      endif

                      if(kDir1 == 1) then
                         kBegin = nK; kEnd = kBegin; Dk = -1
                      elseif(kDir1 == -1) then
                         kBegin = 1; kEnd = kBegin; Dk = 1
                      endif

                      Di1 = 0; Dj1 = 0; Dk1 = Dk
                   endif

                elseif(jDir /=0) then
                   if(jDir == 1) then
                      jBegin = nJ + 1; jEnd = nJ + 3; Dj = 1
                   elseif(jDir == -1) then
                      jBegin = 0; jEnd = -2; Dj = -1
                   endif

                   if(iDir1 /=0) then
                      if(nEdge == 1) then
                         kBegin = 1; kEnd = nK; Dk = 1
                      else
                         if(iStage == 1) then
                            if(kDir2 ==1) then
                               kBegin = 1; kEnd = nK-1; Dk = 1
                            elseif(kDir2 == -1) then
                               kBegin = nK; kEnd = 2; Dk = -1
                            endif
                         else ! iStage == 2
                            if(kDir2 ==1) then
                               kBegin = nK; kEnd = nK; Dk = 1
                            elseif(kDir2 == -1) then
                               kBegin = 1; kEnd = 1; Dk = -1
                            endif
                         endif ! iStage

                      endif

                      if(iDir1 == 1) then
                         iBegin = nI; iEnd = iBegin; Di = -1
                      elseif(iDir1 == -1) then
                         iBegin = 1; iEnd = iBegin; Di = 1
                      endif

                      Di1 = Di; Dj1 = 0; Dk1 = 0

                   elseif(kDir1 /=0) then
                      if(nEdge == 1) then
                         iBegin = 1; iEnd = nI; Di = 1
                      else
                         if(iStage == 1) then
                            if(iDir2 ==1) then
                               iBegin = 1; iEnd = nI-1; Di = 1
                            elseif(iDir2 == -1) then
                               iBegin = nI; iEnd = 2; Di = -1
                            endif
                         else
                            if(iDir2 ==1) then
                               iBegin = nI; iEnd = nI; Di = 1
                            elseif(iDir2 == -1) then
                               iBegin = 1; iEnd = 1; Di = -1
                            endif
                         endif
                      endif

                      if(kDir1 == 1) then
                         kBegin = nK; kEnd = kBegin; Dk = -1
                      elseif(kDir1 == -1) then
                         kBegin = 1; kEnd = kBegin; Dk = 1
                      endif

                      Di1 = 0; Dj1 = 0; Dk1 = Dk
                   endif

                elseif(kDir /=0) then
                   if(kDir == 1) then
                      kBegin = nK + 1; kEnd = nK + 3; Dk = 1
                   elseif(kDir == -1) then
                      kBegin = 0; kEnd = -2; Dk = -1
                   endif

                   if(iDir1 /=0) then
                      if(nEdge == 1) then
                         jBegin = 1; jEnd = nJ; Dj = 1
                      else
                         if(iStage == 1) then
                            if(jDir2 ==1) then
                               jBegin = 1; jEnd = nJ-1; Dj = 1
                            elseif(jDir2 == -1) then
                               jBegin = nJ; jEnd = 2; Dj = -1
                            endif
                         else
                            if(jDir2 ==1) then
                               jBegin = nJ; jEnd = nJ; Dj = 1
                            elseif(jDir2 == -1) then
                               jBegin = 1; jEnd = 1; Dj = -1
                            endif
                         endif

                      endif

                      if(iDir1 == 1) then
                         iBegin = nI; iEnd = iBegin; Di = -1
                      elseif(iDir1 == -1) then
                         iBegin = 1; iEnd = iBegin; Di = 1
                      endif

                      Di1 = Di; Dj1 = 0; Dk1 = 0

                   elseif(jDir1 /=0) then
                      if(nEdge == 1) then
                         iBegin = 1; iEnd = nI; Di = 1
                      else
                         if(iStage == 1) then
                            if(iDir2 ==1) then
                               iBegin = 1; iEnd = nI-1; Di = 1
                            elseif(iDir2 == -1) then
                               iBegin = nI; iEnd = 2; Di = -1
                            endif
                         else ! iStage == 2
                            if(iDir2 ==1) then
                               iBegin = nI; iEnd = nI; Di = 1
                            elseif(iDir2 == -1) then
                               iBegin = 1; iEnd = 1; Di = -1
                            endif
                         endif

                      endif

                      if(jDir1 == 1) then
                         jBegin = nJ; jEnd = jBegin; Dj = -1
                      elseif(jDir1 == -1) then
                         jBegin = 1; jEnd = jBegin; Dj = 1
                      endif
                      Di1 = 0; Dj1 = Dj; Dk1 = 0
                   endif
                endif

                do k=kBegin,kEnd,Dk; do j=jBegin,jEnd,Dj; do i=iBegin,iEnd,Di
                   do iVar = 1, nVar
                      CellValue_I(1) = Field_VG(iVar,&
                           i-2*Di1,j-2*Dj1,k-2*Dk1)
                      CellValue_I(2) = Field_VG(iVar,&
                           i-  Di1,j-  Dj1,k-  Dk1)
                      CellValue_I(3) = Field_VG(iVar,&
                           i+  Di1,j+  Dj1,k+  Dk1)
                      CellValue_I(4) = Field_VG(iVar,&
                           i+2*Di1,j+2*Dj1,k+2*Dk1)

                      Orig = sum(CellValue_I*Coef1_I)
                      if(.not. IsCorrected_VG(iVar,i,j,k)) then
                         Field_VG(iVar,i,j,k)&
                              = limit_interpolation(Orig,&
                              CellValue_I,Distance_I,&
                              IsPositiveIn=IsPositive_V(iVar))
                         IsCorrected_VG(iVar,i,j,k) = .true.
                      else
                         Field_VG(iVar,i,j,k)&
                              = 0.5*(Field_VG(iVar,i,j,k)+ &
                              limit_interpolation(Orig,&
                              CellValue_I,Distance_I,&
                              IsPositiveIn=IsPositive_V(iVar)))
                      endif
                   enddo

                enddo; enddo; enddo

             enddo ! iEdge
          enddo ! iStage

       enddo; enddo; enddo
    endif

  end subroutine correct_face_ghost_for_fine_block
  !============================================================================
  real function prolong_high_order_amr(Cell_III, IsPositiveIn,DoTestMeIn)

    ! Calc 5th order refined cell for AMR.

    use BATL_size, ONLY: kRatio

    real, intent(in):: Cell_III(5,5,5)
    logical, optional,intent(in):: IsPositiveIn, DoTestMeIn

    logical:: IsPositive, DoTest
    real:: Cell_I(5), Cell_II(5,5)
    integer:: i, j, k
    real:: Temp, Distance_I(4)=[-7,-3,1,5]
    real:: CellLimit_I(4)
    character(len=*), parameter:: NameSub = 'prolong_high_order_amr'
    !--------------------------------------------------------------------------
    IsPositive = .false.
    if(present(IsPositiveIn)) IsPositive = IsPositiveIn

    DoTest = .false.
    if(present(DoTestMeIn)) DoTest = DoTestMeIn

    if(DoTest) write(*,*) NameSub

    if(kRatio == 2) then
       do j = 1, 5; do i = 1, 5
          Cell_II(i,j) = interpolate_in_coarse_blk_amr1d(&
               Cell_III(i,j,:), DoLimitIn=.true., IsPositiveIn = IsPositive)
          if(DoTest) then
             write(*,*) 'i = ',i, 'j = ',j
             write(*,*) 'Cell_III(i,j,:) = ',Cell_III(i,j,:)
             write(*,*) 'Cell_II(i,j) = ',Cell_II(i,j)
          endif
       enddo; enddo
       do i = 1, 4
          CellLimit_I(i) = Cell_III(i,i,i)
       enddo
    else
       k = 1
       Cell_II = Cell_III(:,:,k)
       do i = 1, 4
          CellLimit_I(i) = Cell_III(i,i,1)
       enddo
    endif

    do i = 1, 5 ! Eliminate j dimension
       Cell_I(i) = &
            interpolate_in_coarse_blk_amr1d(Cell_II(i,:), DoLimitIn=.true.,&
            IsPositiveIn = IsPositive)
       if(DoTest) then
          write(*,*) 'i = ',i
          write(*,*) 'Cell_II(i,:) = ',Cell_II(i,:)
          write(*,*) 'Cell_I(i) = ',Cell_I(i)
       endif

    enddo
    Temp = interpolate_in_coarse_blk_amr1d(Cell_I, DoLimitIn=.true.,&
         IsPositiveIn = IsPositive)
    prolong_high_order_amr = limit_interpolation(Temp,CellLimit_I,&
         Distance_I)

    if(DoTest) then
       write(*,*) 'Cell_I = ',Cell_I
       write(*,*) 'Temp = ',Temp
       write(*,*) 'CellLimit_I = ',CellLimit_I
       write(*,*) 'Distance_I  = ',Distance_I
    endif

  end function prolong_high_order_amr
  !============================================================================
  real function restrict_high_order_amr(Cell_III, IsPositiveIn)

    ! Calc 6th order coarsened cell for AMR.

    use BATL_size, ONLY: kRatio
    real, intent(in):: Cell_III(6,6,6)
    logical, optional, intent(in):: IsPositiveIn

    real:: Cell_I(6), Cell_II(6,6)
    integer:: i, j, k
    !--------------------------------------------------------------------------
    if(kRatio == 2) then
       do j = 1, 6; do i = 1, 6
          Cell_II(i,j) = calc_face_value(Cell_III(i,j,:), DoLimitIn=.true.,&
               IsPositiveIn=IsPositiveIn)
       enddo; enddo
    else
       k = 1
       Cell_II = Cell_III(:,:,k)
    endif

    do i = 1, 6
       Cell_I(i) = calc_face_value(Cell_II(i,:), DoLimitIn=.true.,&
            IsPositiveIn=IsPositiveIn)
    enddo
    restrict_high_order_amr = calc_face_value(Cell_I, DoLimitIn=.true.,&
         IsPositiveIn=IsPositiveIn)

  end function restrict_high_order_amr
  !============================================================================
end module BATL_high_order
!==============================================================================
