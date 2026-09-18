
module ModSources

!  Add Diagostic Radiative Heating and Cooling terms
!  S. W. Bougher: 11-10-04
!  Add EGWD2 momentum and energy terms
!  S. W. Bougher: 18-05-24
!  Add SIR terms
!  S. W. Bougher: 21-06-15

  use ModSizeGitm
  use GITM_planet, only: nSpecies,nSpeciesTotal,nIons

  !\
  ! Sources for neutral temperature
  !/

  real, dimension(nLons, nLats, nAlts) :: &
       Conduction, NOCooling, OCooling, &
       AuroralHeating, JouleHeating, IonPrecipHeating, &
       EddyCond,EddyCondAdia,MoleConduction

  real, allocatable :: EuvHeating(:,:,:,:)
  real, allocatable :: eEuvHeating(:,:,:,:)
  real, allocatable :: RadCooling(:,:,:,:)
  real, allocatable :: RadCoolingRate(:,:,:,:)
  real, allocatable :: RadCoolingErgs(:,:,:,:)
  real, allocatable :: EuvHeatingErgs(:,:,:,:)
  real, allocatable :: LowAtmosRadRate(:,:,:,:)
  real, allocatable :: QnirTOT(:,:,:,:)
  real, allocatable :: QnirLTE(:,:,:,:)
  real, allocatable :: CirLTE(:,:,:,:)

  !\
  ! Sources from Yigit EGWD2 scheme (momentum/energy equations)
  !/

  ! GWDrag(1:nLons, 1:nLats, 1:nAlts, :, iBlock)
  ! GWDrag(1:nLons, 1:nLats, 1:nAlts, iEast_, iBlock)  = udrag
  ! GWDrag(1:nLons, 1:nLats, 1:nAlts, iNorth_, iBlock)  = vdrag
  real, allocatable :: GWDrag(:,:,:,:,:)

  ! GWIHeat(1:nLons, 1:nLats, 1:nAlts, iBlock)
  ! GWIHeat(1:nLons, 1:nLats, 1:nAlts, iBlock)= gwheat_ir
  real, allocatable :: GWIHeat(:,:,:,:)
  ! GWDHeat(1:nLons, 1:nLats, 1:nAlts, iBlock)
  ! GWDHeat(1:nLons, 1:nLats, 1:nAlts, iBlock)= gwheat_dif
  real, allocatable :: GWDHeat(:,:,:,:)

  ! Additional GW variables (1:nLons, 1:nLats, 1:nAlts, iBlock)
  real, allocatable :: GW_net_heating(:,:,:,:)
  real, allocatable :: GW_beta_tot(:,:,:,:)
  real, allocatable :: GW_beta_non(:,:,:,:)
  real, allocatable :: GW_beta_ext(:,:,:,:)


  !\
  ! Sources from DP 2017 for Secondary Ionization Scheme
  !/

  ! SIR(1:nLons, 1:nLats, 1:nAlts, iWave, iBlock):  Presently just ICO2
    real, allocatable :: SIR(:,:,:,:,:)

  !\
  ! Reactions used in chemistry output
  ! i.e. in2p_e -->  n2+ + e
  !      ino_n  -->  no + n
  !/

  
  integer, parameter :: nReactions = 56
  real :: ChemicalHeatingSpecies(nLons, nLats, nAlts,nReactions)
  real :: ChemicalHeatingS(nReactions) 
  real :: NeutralSourcesTotal(nAlts,nSpeciesTotal)
  real :: NeutralLossesTotal(nAlts,nSpeciesTotal)
  real, allocatable :: ISourcesTotal(:,:,:,:,:)
  real, allocatable :: ILossesTotal(:,:,:,:,:)
  real, allocatable :: ReactionRateS(:,:,:,:,:)

  integer, parameter ::    in2p_e = 1
  integer, parameter ::    io2p_e = 2
  integer, parameter ::    in2p_o = 3
  integer, parameter ::    inop_e = 4
  integer, parameter ::    inp_o2 = 5
  integer, parameter ::    ino_n  = 6
  integer, parameter ::    iop_o2 = 7
  integer, parameter ::    in_o2  = 8
  integer, parameter ::    io2p_n = 9
  integer, parameter ::    io2p_no= 10
  integer, parameter ::    io2p_n2= 11
  integer, parameter ::    in2p_o2= 12
  integer, parameter ::    inp_o  = 13
  integer, parameter ::    iop_n2 = 14
  integer, parameter ::    io1d_n2 = 15
  integer, parameter ::    io1d_o2 = 16
  integer, parameter ::    io1d_o  = 17
  integer, parameter ::    io1d_e  = 18
  integer, parameter ::    in2d_o2 = 19
  integer, parameter ::    iop2d_e = 20
  integer, parameter ::    in2d_o = 21
  integer, parameter ::    in2d_e = 22
  integer, parameter ::    iop2d_n2 =23
  integer, parameter ::    iop2p_e = 24
  integer, parameter ::    iop2p_o = 25
  integer, parameter ::    iop2p_n2 = 26

  !\
  ! Stuff for auroral energy deposition and ionization
  !/

  real, dimension(:), allocatable :: &
       ED_grid, ED_Energies, ED_Flux, ED_Ion, ED_Heating
  integer :: ED_N_Energies, ED_N_Alts
  real, dimension(nAlts) :: ED_Interpolation_Weight
  integer, dimension(nAlts) :: ED_Interpolation_Index

  real, allocatable :: AuroralIonRateS(:,:,:,:,:)
  real, allocatable :: AuroralHeatingRate(:,:,:,:)
  real, allocatable :: IonPrecipIonRateS(:,:,:,:,:)
  real, allocatable :: IonPrecipHeatingRate(:,:,:,:)
  real :: ChemicalHeatingRate(nLons, nLats, nAlts)

  real :: HorizontalTempSource(nLons, nLats, nAlts)

  real :: Diffusion(nLons,nLats,nAlts,nSpecies)
  real :: NeutralFriction(nLons,nLats,nAlts,nSpecies) = 0.0
  real :: IonNeutralFriction(nLons,nLats,nAlts,nSpecies)

  real, allocatable :: KappaEddyDiffusion(:,:,:,:)

contains
  !=========================================================================
  subroutine init_mod_sources

    if(allocated(EuvHeating)) return
    allocate(EuvHeating(nLons, nLats, nAlts,nBlocks))
    allocate(eEuvHeating(nLons, nLats, nAlts,nBlocks))
    allocate(RadCooling(nLons, nLats, nAlts,nBlocks))
    allocate(RadCoolingRate(nLons, nLats, nAlts,nBlocks))
    allocate(RadCoolingErgs(nLons, nLats, nAlts,nBlocks))
    allocate(EuvHeatingErgs(nLons, nLats, nAlts,nBlocks))
    allocate(LowAtmosRadRate(nLons, nLats, nAlts,nBlocks))
    LowAtmosRadRate = 0.0
    allocate(QnirTOT(nLons, nLats, nAlts,nBlocks))
    allocate(QnirLTE(nLons, nLats, nAlts,nBlocks))
    allocate(CirLTE(nLons, nLats, nAlts,nBlocks))
    allocate(ISourcesTotal(nLons,nLats,nAlts,nIons-1,nBlocks))
    allocate(ILossesTotal(nLons,nLats,nAlts,nIons-1,nBlocks))
    allocate(AuroralIonRateS(nLons,nLats,nAlts,nSpecies,nBlocks))
    allocate(AuroralHeatingRate(nLons,nLats,nAlts,nBlocks))
    allocate(IonPrecipIonRateS(nLons,nLats,nAlts,nSpecies,nBlocks))
    allocate(IonPrecipHeatingRate(nLons,nLats,nAlts,nBlocks))
    allocate(KappaEddyDiffusion(nLons,nLats,-1:nAlts+2,nBlocks))
    allocate(GWDrag(nLons, nLats, nAlts, 3, nBlocks))
    allocate(GWIHeat(nLons, nLats, nAlts, nBlocks))
    allocate(GWDHeat(nLons, nLats, nAlts, nBlocks))
    allocate(GW_net_heating(nLons, nLats, nAlts, nBlocks))
    allocate(GW_beta_tot(nLons, nLats, nAlts, nBlocks))
    allocate(GW_beta_non(nLons, nLats, nAlts, nBlocks))
    allocate(GW_beta_ext(nLons, nLats, nAlts, nBlocks))
    allocate(SIR(nLons, nLats, nAlts, 59, nBlocks))
    allocate(ReactionRateS(nLons,nLats,nAlts,nReactions,nBlocks))

    ChemicalHeatingS = 0.0
    AuroralHeating = 0.0
    KappaEddyDiffusion = 0.0
    
  end subroutine init_mod_sources
  !=========================================================================
  subroutine clean_mod_sources

    if(.not.allocated(EuvHeating)) return
    deallocate(EuvHeating)
    deallocate(eEuvHeating)
    deallocate(RadCooling)
    deallocate(RadCoolingRate)
    deallocate(RadCoolingErgs)
    deallocate(EuvHeatingErgs)
    deallocate(LowAtmosRadRate)
    deallocate(QnirTOT)
    deallocate(QnirLTE)
    deallocate(CirLTE)
    deallocate(ISourcesTotal)
    deallocate(ILossesTotal)
    deallocate(AuroralIonRateS)
    deallocate(AuroralHeatingRate)
    deallocate(IonPrecipIonRateS)
    deallocate(IonPrecipHeatingRate)
    deallocate(KappaEddyDiffusion)
    deallocate(GWDrag)
    deallocate(GWIHeat)
    deallocate(GWDHeat)
    deallocate(GW_net_heating)
    deallocate(GW_beta_tot)
    deallocate(GW_beta_non)
    deallocate(GW_beta_ext)
    deallocate(SIR)
    deallocate(ReactionRateS)

  end subroutine clean_mod_sources
  !=========================================================================
end module ModSources
