!  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf
!-----------------------------------------------------------------------------
! $Id$
!
! Author: Aaron Ridley
!
! Comments: GITM main
!
! AGB 3/31/13: Added call for RCMR data assimilation
! AGB 10/23/13: Adapted RCMR call to new format
!-----------------------------------------------------------------------------

program GITM

  use ModInputs
  use ModTime
  use ModGITM
  use ModMpi
  use ModRCMR
  use ModSatellites, only: SatCurrentDat, SatAltDat, nRCMRSat
  use ModEUV, only: sza

  implicit none

  integer :: iBlock
  

  ! ------------------------------------------------------------------------
  ! initialize stuff
  ! ------------------------------------------------------------------------
  
  call init_mpi
  call start_timing("GITM")
  call delete_stop

  call init_planet
  call set_defaults

  call read_inputs(cInputFile)
  call set_inputs

  call initialize_gitm(CurrentTime)

  call write_output

  call report("Starting Main Time Loop",0)

  ! ------------------------------------------------------------------------
  ! Run for a few iterations
  ! ------------------------------------------------------------------------

  do while (CurrentTime < EndTime)

     call calc_pressure

     !!! We may have to split cMax and Dt calculation!!!
     if(RCMRFlag) then
        Dt = 2
     else
        Dt = 1.e32
     end if

     call calc_timestep_vertical
     if (.not. Is1D) call calc_timestep_horizontal

     if(RCMRFlag) then
        call run_RCMR
     endif

     call advance

     if (.not.IsFramework) then
        call check_stop
     endif
     
     iStep = iStep + 1

     call write_output
  end do

  ! ------------------------------------------------------------------------
  ! Finish run
  ! ------------------------------------------------------------------------

  call finalize_gitm

end program GITM


