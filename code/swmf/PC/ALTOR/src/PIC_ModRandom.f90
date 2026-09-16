!  Copyright (C) 2002 Regents of the University of Michigan, 
!  portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf
!The random number generator.
!From the Buneman code.
module PIC_ModRandom
  implicit none
  private !Except
  integer::iSeed_I(10)=0
  !Methods
  public::rand
  public::init_rand
contains
  subroutine init_rand(iSeedIn,iIndexIn)
    integer,optional,intent(in)::iSeedIn,iIndexIn
    integer:: iIndex
    if(present(iIndexIn))then
       iIndex = iIndexIn
    else
       iIndex = 1
    end if
    if(present(iSeedIn))then
       iSeed_I(iIndex) = iSeedIn
    else
       iSeed_I(iIndex)=1
    end if
  end subroutine init_rand
  !-----------------------------------------------------------------!
  !===============================================================C
  REAL FUNCTION RAND(iIndexIn)
    integer,optional,intent(in)::iIndexIn
    integer:: iIndex
    if(present(iIndexIn))then
       iIndex = iIndexIn
    else
       iIndex = 1
    end if
    ISEED_I(iIndex)=ISEED_I(iIndex)*48828125
    IF(ISEED_I(iIndex) < 0) ISEED_I(iIndex)=(ISEED_I(iIndex)+2147483647)+1
    if(iSeed_I(iIndex)==0) iSeed_I(iIndex)=1
    RAND=FLOAT(ISEED_I(iIndex))/2147483647
  END FUNCTION RAND
end module PIC_ModRandom
