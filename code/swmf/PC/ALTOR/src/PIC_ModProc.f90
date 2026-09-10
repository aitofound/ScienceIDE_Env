!  Copyright (C) 2002 Regents of the University of Michigan, 
!  portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf
!==========================
module PIC_ModProc
  implicit none
  SAVE

  integer::iProc !PE rank
  integer::nProc !Number of PEs
  integer::iComm !Communicator
  integer::iError!Auxiliary integer to store an error number

end module PIC_ModProc
    
    
