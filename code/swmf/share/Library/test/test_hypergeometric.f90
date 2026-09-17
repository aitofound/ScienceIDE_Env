!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
program test
  use ModHyperGeometric
  use ModPlotFile
  implicit none
  integer, parameter :: nStep = 400, SCR_ = 7, L0Ext_=1, ExtApprox_ = 2
  integer, parameter :: L0Int_ = 3,    IntApprox_ = 4
  integer, parameter :: L0Tor_ = 5,    TorApprox_ = 6
  real   , parameter :: DeltaKappaPrime = 0.00050
  integer            :: iLoop
  real               :: R0, a, KappaPrime, KappaPrime2, Kappa2, Kappa3
  real :: Var_VI(L0Ext_:SCR_,nStep), Coord_I(nSTep)
  !----------------------------------------------------------------------------
  !!!!!!!!!!!!!!!!!!!!!! Plot inductances !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  do iLoop = 1, nStep
     KappaPrime  = iLoop*DeltaKappaPrime
     KappaPrime2 =  KappaPrime**2
     a  = 2*KappaPrime/(1 - KappaPrime2)
     R0 = (1 + KappaPrime2)/(1 - KappaPrime2)
     Coord_I(iLoop) = a/R0
     Var_VI(SCR_,iLoop)   = scr_inductance(KappaPrime2)
     Var_VI(L0Ext_,iLoop) = l0_ext_inductance(KappaPrime2)
     Var_VI(ExtApprox_,iLoop) = R0*log(8.0/Coord_I(iLoop)) - 2.0
     Var_VI(L0Int_,iLoop) = l0_int_inductance(KappaPrime2)
     Var_VI(IntApprox_,iLoop) = 0.250
     Var_VI(L0Tor_,iLoop) = l0_tor_inductance(KappaPrime2)
     Var_VI(TorApprox_,iLoop) = 0.50
  end do
  call save_plot_file(NameFile='test_inductance.out', &
       TypeFileIn='ascii'                     ,&
       NameVarIn = &
       'aspect L_Ext ExtApprox L_Int IntApprox L_Tor TorApprox SCR_', &
       StringFormatIn = '(8es18.10)'          ,&
       Coord1In_I = Coord_I                   ,&
       VarIn_VI = Var_VI(L0Ext_:SCR_,1:nStep) )
end program test
!==============================================================================

