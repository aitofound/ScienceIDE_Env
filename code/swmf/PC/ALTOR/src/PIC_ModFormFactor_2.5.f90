module PIC_ModFormFactor
  use PC_ModSize, ONLY: nDim, MaxDim
  implicit none

  integer, parameter :: lOrderFF =  3 
  integer, parameter :: iDownFF  = -2, iUpFF = 1
  integer, parameter :: iExt     =  0

  !Structures
  !Discsrete particle coordinates
  integer,dimension(MaxDim) :: Node_D = 1, NodeNew_D = 1
 
  !Particle form-factors
  real,dimension(1:lOrderFF+1,MaxDim) :: HighFF_ID = 1.0, HighFFNew_ID = 1.0
               

  real,dimension(lOrderFF,MaxDim)::LowFF_ID = 1.0
contains
  subroutine get_form_factors(X_D, NodeOut_D, HighFFOut_ID)
    use ModNumConst,ONLY: cHalf
    !Input parameters
    !X_D should be mormalized by (/\Delta x,\Delta y,\Delta z/)
    real,dimension(nDim),intent(in) :: X_D

    !Output parameters
    !The cell-center index    
    integer,dimension(MaxDim)        ::NodeOut_D
    real,dimension(lOrderFF+1,MaxDim)::HighFFOut_ID

    !Normally both Node_D and HighFF_ID are members of this module

    real,dimension(nDim)::d_D
    real,parameter:: cThird =1.0/3.0, cTwoThird = 2.0/3.0, cSixth = 1.0/6.0
    integer::iDim
    !--------------------
    NodeOut_D(1:nDim) = floor(X_D + cHalf)

    d_D = X_D + cHalf - real(NodeOut_D(1:nDim))

    NodeOut_D(1:nDim) = NodeOut_D(1:nDim) + 1

    !Asterisk means non-zero FF value
    !Node-2!Node-1!Node  !Node+1!           
    !      !   ---!---   !      ! '----' means the particle position
    !      1*     2*     3*     ! Low
    !      !      !      !      !
    !  1*  !  2*  !  3*  !  4*  ! High
    !      !      !      !      !
    !   Node-2  Node-1 Node   Node+1
    do iDim=1,nDim
       LowFF_ID(1,iDim) = cHalf *( 1.0 - d_D(iDim))**2
       LowFF_ID(2,iDim) = 0.750 - (cHalf - d_D(iDim))**2
       LowFF_ID(3,iDim) = cHalf * d_D(iDim)**2

       HighFFOut_ID(1, iDim) = LowFF_ID(1,iDim)*(1.0 - d_D(iDim))*cThird
       HighFFOut_ID(2, iDim) = cSixth*(2.0 - d_D(iDim))**3 - 4.0*HighFFOut_ID(1, iDim)
       HighFFOut_ID(4, iDim) = LowFF_ID(3,iDim)*       d_D(iDim) *cThird
       HighFFOut_ID(3, iDim) = cSixth*(1.0 + d_D(iDim))**3 - 4.0*HighFFOut_ID(4, iDim)
    end do

 
  end subroutine get_form_factors
end module PIC_ModFormFactor
