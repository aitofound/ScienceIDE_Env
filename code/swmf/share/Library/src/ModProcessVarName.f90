!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf

module ModProcessVarName

  ! Process variable names and count various things

  use ModUtilities, ONLY: CON_stop

  implicit none

  private ! except

  public:: process_var_name
  public:: nVarMax

  interface process_var_name
     module procedure process_var_list, process_var_string
  end interface

  integer, parameter:: nVarMax = 100   ! maximum number of state variables
  integer, parameter:: nSubstance = 34 ! number of distinct fluids/species

  ! Number of state variables associated with each substance to be standardized
  integer, parameter:: nVarPerSubstance = 9

  ! Number of allowed alternative names for each variable
  integer, parameter:: nSynonym = 3

  ! State variables not associated with fluids/species
  integer, parameter:: nVarExtra = 17

   ! Number of elements for charge state calculation
  integer, parameter:: nElementAll = 30

  ! Named indices for all substances (species or fluids)
  integer, parameter:: &
       H_    = 1,  &
       Hp_   = 2,  &
       HpSw_ = 3,  &
       HpPs_ = 4, &
       H2p_  = 5,  &
       O_    = 6,  &
       Op_   = 7,  &
       O2p_  = 8,  &
       He_   = 9,  &
       Hep_  = 10,  &
       He2p_ = 11, &
       OHp_  = 12, &
       N_    = 13, &
       Np_   = 14, &
       COp_  = 15, &
       CO2p_ = 16, &
       H2O_  = 17, &
       H2Op_ = 18, &
       H3Op_ = 19, &
       Mp_   = 20, &
       Lp_   = 21, &
       MHCp_ = 22, &
       HHCp_ = 23, &
       HNIp_ = 24, &
       Neu1_ = 25, &
       Neu2_ = 26, &
       Neu3_ = 27, &
       Neu4_ = 28, &
       Pui1_ = 29, &
       Pui2_ = 30, &
       Pui3_ = 31, &
       Pui4_ = 32, &
       El_   = 33, &
       Main_ = 34 ! main component, MHD/HD

  ! String array storing the standard names of all substances
  character(len=6):: NameSubstance_I(nSubstance) = [ &
       'H   ',  &
       'Hp  ',  &
       'HpSw',  &
       'HpPs',  &
       'H2p ',  &
       'O   ',  &
       'Op  ',  &
       'O2p ',  &
       'He  ',  &
       'Hep ',  &
       'He2p',  &
       'OHp ',  &
       'N   ',  &
       'Np  ',  &
       'COp ',  &
       'CO2p',  &
       'H2O ',  &
       'H2Op',  &
       'H3Op',  &
       'Mp  ',  &
       'Lp  ',  &
       'MHCp',  &
       'HHCp',  &
       'HNIp',  &
       'Neu1',  &
       'Neu2',  &
       'Neu3',  &
       'Neu4',  &
       'Pui1',  &
       'Pui2',  &
       'Pui3',  &
       'Pui4',  &
       'El  ',  &
       '    '  ] ! main component, MHD / HD

  ! named indices for basic state variables associated with a substance
  integer,parameter :: &
       Rho_   = 1, &
       RhoUx_ = 2, &
       RhoUy_ = 3, &
       RhoUz_ = 4, &
       p_     = 5, &
       Ppar_  = 6, &
       Pe_    = 7, &
       Pepar_ = 8, &
       Energy_= 9

  ! string array containing basic state variables associated with a substance
  character(len = 6) :: NameSubstanceVar_I(nVarPerSubstance) = [ &
       'Rho   ', &
       'Mx    ', &
       'My    ', &
       'Mz    ', &
       'P     ', &
       'Ppar  ', &
       'Pe    ', &
       'Pepar ', &
       'Energy'  ]

  ! string arrays containing variables not associated with any substance
  character(len=5) :: NameVarExtra_I(nVarExtra) = [ &
       'bx   ', &
       'by   ', &
       'bz   ', &
       'ex   ', &
       'ey   ', &
       'ez   ', &
       'te0  ', &
       'ew   ', &
       'eint ', &
       'ehot ', &
       'hyp  ', &
       'hype ', &
       'bperu', &
       'signb', &
       'wd   ', &
       'lperp', &
       'hplim']

  ! Named indices for all elements in charge state calculation
  character(len=2) :: NameElementAll_I(nElementAll) = [&
       'h ', &
       'he', &
       'li', &
       'be', &
       'b ', &
       'c ', &
       'n ', &
       'o ', &
       'f ', &
       'ne', &
       'na', &
       'mg', &
       'al', &
       'si', &
       'p ', &
       's ', &
       'cl', &
       'ar', &
       'k ', &
       'ca', &
       'sc', &
       'ti', &
       'v ', &
       'cr', &
       'mn', &
       'fe', &
       'co', &
       'ni', &
       'cu', &
       'zn' ]

  character(len=5) :: NameVarExtraStandardized_I(nVarExtra) = [ &
       'Bx   ', &
       'By   ', &
       'Bz   ', &
       'Ex   ', &
       'Ey   ', &
       'Ez   ', &
       'Te0  ', &
       'Ew   ', &
       'Eint ', &
       'Ehot ', &
       'Hyp  ', &
       'HypE ', &
       'BperU', &
       'SignB', &
       'wD   ', &
       'Lperp', &
       'HPLim']

  ! Array storing standarized variable names for all species / fluids
  character(len=20), allocatable :: SubstanceStandardName_II(:,:)

  ! Array storing all possible names
  character(len=20), allocatable:: Dictionary_III(:,:,:)

contains
  !============================================================================
  subroutine process_var_string(NameVar,  &
       nDensity, nSpeed, nP, nPpar, nWave, nMaterial, nChargeStateAll, &
          nPe, nPepar)

    use ModUtilities, ONLY: split_string, join_string

    character(len=*), intent(inout):: NameVar
    integer, optional, intent(out):: nDensity, nSpeed, nP, nPpar, nPe, nPepar
    integer, optional, intent(out):: nWave, nMaterial, nChargeStateAll

    integer            :: nVarName
    integer, parameter :: MaxNameVar = 100
    character(len=20)  :: NameVar_V(MaxNameVar)
    !--------------------------------------------------------------------------
    call split_string(NameVar, MaxNameVar, NameVar_V, nVarName)

    call process_var_list(nVarName, NameVar_V,  &
         nDensity, nSpeed, nP, nPpar, nWave, nMaterial, nChargeStateAll, &
         nPe, nPepar)

    call join_string(nVarName, NameVar_V(1:nVarName), NameVar)

  end subroutine process_var_string
  !============================================================================
  subroutine process_var_list(nVarName, NameVar_V,  &
       nDensity, nSpeed, nP, nPpar, nWave, nMaterial, nChargeStateAll, &
          nPe, nPepar)

    use ModUtilities, ONLY: lower_case

    integer,intent(in):: nVarName
    character(len=*), intent(inout):: NameVar_V(nVarName)
    integer, optional, intent(out):: nDensity, nSpeed, nP, nPpar, nPe, nPepar
    integer, optional, intent(out):: nWave, nMaterial, nChargeStateAll

    ! 1. Creates standard names and a dictionary for each standard name.
    !    The dictionary only contains the basic hydro quantities for
    !    different substances. Other quantities, e.g. magnetic field,
    !    that are not associated with a specific substance are
    !    stored separately. This allows us to avoid searching the complete
    !    dictionary when it is not needed.

    ! The dictionary is a string array:
    !    Dictionary_III(nSubstance, nVarPerSubstance, nSynonym)
    !
    !    where:
    !    - nSubstance is the number of possible species/ fluids
    !    - nVarPerSubstance enumarates the variables associated
    !              with each substance.
    !    - nSynonym is the number of alternative names representing the same
    !              physical quantity, used by different ModEquation files.
    !
    ! 2. Look up the elements of NameVar_V and replace them with standard names
    !    The look up procedure in the dictionary is done by
    !    call find_substance_replace_name
    !    Once a specific NameVarIn is found to be identical to a dictionary
    !    item, it is replaced with
    !        SubstanceStandardName_II(iSubstance,iVarPerSubstance)
    !
    ! 3. The number of fluids and species found are returned by
    !    nDensity and nSpeed.

    integer          :: nDistinctSubstanceVar_I(nVarPerSubstance)
    character(len=15):: NameVarIn
    integer          :: iName, iVar, iSubstanceFound = 0
    logical          :: IsFoundVar

    ! For charge state loop
    integer:: iElementAll
    character(len=4):: NameChargeStateFirst, NameChargeStateLast

    character(len=*), parameter:: NameSub = 'process_var_list'
    !--------------------------------------------------------------------------
    nDistinctSubstanceVar_I(:) = 0
    if(present(nWave))           nWave = 0
    if(present(nMaterial))       nMaterial = 0
    if(present(nChargeStateAll)) nChargeStateAll = 0

    ! create standard names and dictionary arrays
    allocate(SubstanceStandardName_II(nSubstance, nVarPerSubstance))
    allocate(Dictionary_III(nSubstance, nVarPerSubstance, nSynonym))
    call create_dictionary

    ! Look up each var name
    NAMELOOP: do iName = 1, nVarName
       ! init search
       IsFoundVar = .false.
       NameVarIn = NameVar_V(iName)
       call lower_case(NameVarIn)

       ! Don't look up in dictionary for: bx, by, bz, EInt, ew, pe, hyp, hype
       do iVar = 1, nVarExtra
          if(NameVarIn == NameVarExtra_I(iVar)) then
             NameVar_V(iName) = NameVarExtraStandardized_I(iVar)
             IsFoundVar = .true.
             CYCLE NAMELOOP
          end if
       end do

       ! check dictionary ( loop over density, momentum. pressure, energy)
       do iVar = 1, nVarPerSubstance
          call find_substance_replace_name
          if(IsFoundVar) then
             ! Count how many distinct substance variables are present
             nDistinctSubstanceVar_I(iVar) = &
                  nDistinctSubstanceVar_I(iVar) +1
             CYCLE NAMELOOP
          end if
       end do

       ! variable name may correspond to numbered wave/material
       ! These names are created  in BATSRUS:MH_set_parameters
       ! and need not be changed
       if (lge(NameVarIn, 'i01') .and. lle(NameVarIn, 'i99')) then
          if(present(nWave)) nWave = nWave + 1
          IsFoundVar = .true.
          CYCLE NAMELOOP
       end if

       if (lge(NameVarIn, 'm1') .and. lle(NameVarIn, 'm9')) then
          if(present(nMaterial)) nMaterial = nMaterial + 1
          IsFoundVar = .true.
          CYCLE NAMELOOP
       end if

       ! Charge state variable names by the elements
       do iElementAll = 1, nElementAll
          if(iElementAll < 9) then
             write(NameChargeStateLast,'(a,i1.1)')&
                  trim(NameElementAll_I(iElementAll)),iElementAll+1
             write(NameChargeStateFirst,'(a,i1.1)')&
                  trim(NameElementAll_I(iElementAll)),1
          else
             write(NameChargeStateLast,'(a,i2.2)')&
                  trim(NameElementAll_I(iElementAll)),iElementAll+1
             write(NameChargeStateFirst,'(a,i2.2)')&
                  trim(NameElementAll_I(iElementAll)),1
          end if

          if (lge(NameVarIn,NameChargeStateFirst) .and. &
               lle(NameVarIn,NameChargeStateLast)) then
             if(present(nChargeStateAll)) nChargeStateAll = nChargeStateAll + 1
             IsFoundVar = .true.
             CYCLE NAMELOOP
          end if
       end do

       if(.not. IsFoundVar) then
          write(*,*) 'ERROR: Var name not in dictionary: ',NameVarIn
          write(*,*) 'Please use standard variable names in ModEquation '// &
               'and recompile:'
          ! write(*,*) SubstanceStandardName_II
          write(*,*) ''
          call CON_stop(NameSub//': unknown variable '//NameVarIn)
       end if

    end do NAMELOOP

    if(present(nDensity)) nDensity = nDistinctSubstanceVar_I(Rho_)
    if(present(nSpeed))   nSpeed   = nDistinctSubstanceVar_I(RhoUx_)
    if(present(nP))       nP       = nDistinctSubstanceVar_I(P_)
    if(present(nPpar))    nPpar    = nDistinctSubstanceVar_I(Ppar_)
    if(present(nPe))      nPe      = nDistinctSubstanceVar_I(Pe_)
    if(present(nPepar))   nPepar   = nDistinctSubstanceVar_I(Pepar_)

    deallocate(Dictionary_III)
    deallocate(SubstanceStandardName_II)

  contains
    !==========================================================================
    subroutine find_substance_replace_name

      ! lookup var name in dictionary, replace with standard name

      use ModUtilities, ONLY: lower_case

      integer             :: iSubstance, iSynonym
      character(len=15)   :: DictionaryItem
      !------------------------------------------------------------------------
      do iSubstance = 1, nSubstance
         do iSynonym = 1, nSynonym
            DictionaryItem = Dictionary_III(iSubstance, iVar, iSynonym)
            if(len_trim(DictionaryItem) > 0) then
               call lower_case(DictionaryItem)
               if(NameVarIn ==  DictionaryItem) then
                  iSubstanceFound = iSubstance
                  IsFoundVar = .true.
                  NameVar_V(iName) = &
                       SubstanceStandardName_II(iSubstanceFound, iVar)
                  RETURN
               end if
            end if
         end do
      end do
    end subroutine find_substance_replace_name
    !==========================================================================
  end subroutine process_var_list
  !============================================================================
  subroutine create_standard_name

    integer   :: iVar, iSubstance
    ! loop over all possible species/fluids to fill in Name arrays
    !--------------------------------------------------------------------------
    do iSubstance = 1, nSubstance
       do iVar = 1, nVarPerSubstance
          SubstanceStandardName_II(iSubstance,iVar) = &
               ''//trim(NameSubstance_I(iSubstance))//NameSubstanceVar_I(iVar)
       end do
    end do

  end subroutine create_standard_name
  !============================================================================
  subroutine create_dictionary

    integer  :: iSubstance
    !--------------------------------------------------------------------------
    Dictionary_III(:,:,:) = ''

    ! first page in dictionary is a 2 by 2 array of standard names
    call create_standard_name
    Dictionary_III(:,:,1) = SubstanceStandardName_II

    ! fill in alternative names
    ! The names below are alternative names to the standard names, as
    ! used by existing ModEquation files.
    ! The use of standard names in equation files is encouraged.

    ! Alternative names for energy for all substances
    do iSubstance = 1, nSubstance
       Dictionary_III(iSubstance, Energy_, 2) = &
            ''//trim(NameSubstance_I(iSubstance))//'e'
    end do

    ! main plasma fluid
    Dictionary_III(Main_, RhoUx_,    2) = 'rhoux'
    Dictionary_III(Main_, RhoUy_,    2) = 'rhouy'
    Dictionary_III(Main_, RhoUz_,    2) = 'rhouz'

    ! H atoms
    Dictionary_III(H_, Rho_,   2) = 'rhoh'

    ! H+ ions
    Dictionary_III(Hp_, Rho_,   2) = 'h1p'
    Dictionary_III(Hp_, Rho_,   3) = 'hp'
    Dictionary_III(Hp_, RhoUx_, 2) = 'hpux'
    Dictionary_III(Hp_, RhoUy_, 2) = 'hpuy'
    Dictionary_III(Hp_, RhoUz_, 2) = 'hpuz'

    ! H2+ ions
    Dictionary_III(H2p_, Rho_,    2) = 'h2p'

    ! He atoms
    Dictionary_III(He_, Rho_,     2) = 'rhohe'

    ! O atoms
    Dictionary_III(O_, Rho_,      2) = 'rhoo'

    ! O+ ions
    Dictionary_III(Op_, Rho_,   2) = 'op'

    ! O2+ ions
    Dictionary_III(O2p_, Rho_,   2) = 'o2p'

    ! CO+ ions
    Dictionary_III(COp_, Rho_,   2) = 'cop'

    ! CO2+ ions
    Dictionary_III(CO2p_, Rho_,   2) = 'co2p'

    ! H2O molecules
    Dictionary_III(H2O_, Rho_,    2) = 'rhoh2o'

    ! H2O+ ions
    Dictionary_III(H2Op_, Rho_,   2) = 'h2op'
    Dictionary_III(H2Op_, Rho_,   3) = 'rhoh2op'

    ! H3O+ ions
    Dictionary_III(H3Op_, Rho_,   2) = 'h3op'

    ! OH+ ions
    Dictionary_III(OHp_, Rho_,   2) = 'ohp'

    ! Saturn fluids
    Dictionary_III(N_,    Rho_,   2) = 'rhon'

    ! Titan ions
    Dictionary_III(Mp_,   Rho_,   2) = 'mp'
    Dictionary_III(Lp_,   Rho_,   2) = 'lp'
    Dictionary_III(MHCp_, Rho_,   2) = 'mhcp'
    Dictionary_III(HHCp_, Rho_,   2) = 'hhcp'
    Dictionary_III(HNIp_, Rho_,   2) = 'hnip'

    ! solar wind
    Dictionary_III(HpSw_, Rho_,   2) = 'swhrho'
    Dictionary_III(HpSw_, RhoUx_, 2) = 'swhmx'
    Dictionary_III(HpSw_, RhoUy_, 2) = 'swhmy'
    Dictionary_III(HpSw_, RhoUz_, 2) = 'swhmz'
    Dictionary_III(HpSw_, p_,     2) = 'swhp'
    Dictionary_III(HpSw_, Energy_,2) = 'swhe'

    Dictionary_III(HpSw_, Rho_,   3) = 'rhosw'
    Dictionary_III(HpSw_, Energy_,3) = 'swe'

    ! ionosphere
    Dictionary_III(Hp_, Rho_,   2) = 'rhoion'

    ! Outer Heliosphere Pop1 / arbitrary neutral
    Dictionary_III(Neu1_, Rho_,   2) = 'neurho'
    Dictionary_III(Neu1_, RhoUx_, 2) = 'neumx'
    Dictionary_III(Neu1_, RhoUy_, 2) = 'neumy'
    Dictionary_III(Neu1_, RhoUz_, 2) = 'neumz'
    Dictionary_III(Neu1_, p_,     2) = 'neup'
    Dictionary_III(Neu1_, Energy_,2) = 'neue'

    ! Create Alternate names for arbitrary neutral
    Dictionary_III(Neu1_, Rho_,   3) = 'Neu1Rho'
    Dictionary_III(Neu1_, RhoUx_, 3) = 'Neu1Mx'
    Dictionary_III(Neu1_, RhoUy_, 3) = 'Neu1My'
    Dictionary_III(Neu1_, RhoUz_, 3) = 'Neu1Mz'
    Dictionary_III(Neu1_, p_,     3) = 'Neu1P'
    Dictionary_III(Neu1_, Energy_,3) = 'Neu1E'

    ! Outer Heliosphere Pop2 / arbitrary neutral
    Dictionary_III(Neu2_, Rho_,   2) = 'ne2rho'
    Dictionary_III(Neu2_, RhoUx_, 2) = 'ne2mx'
    Dictionary_III(Neu2_, RhoUy_, 2) = 'ne2my'
    Dictionary_III(Neu2_, RhoUz_, 2) = 'ne2mz'
    Dictionary_III(Neu2_, p_,     2) = 'ne2p'
    Dictionary_III(Neu2_, Energy_,2) = 'ne2e'

    ! Outer Heliosphere Pop3 / arbitrary neutral
    Dictionary_III(Neu3_, Rho_,   2) = 'ne3rho'
    Dictionary_III(Neu3_, RhoUx_, 2) = 'ne3mx'
    Dictionary_III(Neu3_, RhoUy_, 2) = 'ne3my'
    Dictionary_III(Neu3_, RhoUz_, 2) = 'ne3mz'
    Dictionary_III(Neu3_, p_,     2) = 'ne3p'
    Dictionary_III(Neu3_, Energy_,2) = 'ne3e'

    ! Outer Heliosphere Pop4 / arbitrary neutral
    Dictionary_III(Neu4_, Rho_,   2) = 'ne4rho'
    Dictionary_III(Neu4_, RhoUx_, 2) = 'ne4mx'
    Dictionary_III(Neu4_, RhoUy_, 2) = 'ne4my'
    Dictionary_III(Neu4_, RhoUz_, 2) = 'ne4mz'
    Dictionary_III(Neu4_, p_,     2) = 'ne4p'
    Dictionary_III(Neu4_, Energy_,2) = 'ne4e'

    ! Outer Heliosphere Pop1 / arbitrary pick up ion
    Dictionary_III(Pui1_, Rho_,   2) = 'pu1rho'
    Dictionary_III(Pui1_, RhoUx_, 2) = 'pu1mx'
    Dictionary_III(Pui1_, RhoUy_, 2) = 'pu1my'
    Dictionary_III(Pui1_, RhoUz_, 2) = 'pu1mz'
    Dictionary_III(Pui1_, p_,     2) = 'pu1p'
    Dictionary_III(Pui1_, Energy_,2) = 'pu1e'

    ! Outer Heliosphere Pop2 / arbitrary pick up ion
    Dictionary_III(Pui2_, Rho_,   2) = 'pu2rho'
    Dictionary_III(Pui2_, RhoUx_, 2) = 'pu2mx'
    Dictionary_III(Pui2_, RhoUy_, 2) = 'pu2my'
    Dictionary_III(Pui2_, RhoUz_, 2) = 'pu2mz'
    Dictionary_III(Pui2_, p_,     2) = 'pu2p'
    Dictionary_III(Pui2_, Energy_,2) = 'pu2e'

    ! Outer Heliosphere Pop3 / arbitrary pick up ion
    Dictionary_III(Pui3_, Rho_,   2) = 'pu3rho'
    Dictionary_III(Pui3_, RhoUx_, 2) = 'pu3mx'
    Dictionary_III(Pui3_, RhoUy_, 2) = 'pu3my'
    Dictionary_III(Pui3_, RhoUz_, 2) = 'pu3mz'
    Dictionary_III(Pui3_, p_,     2) = 'pu3p'
    Dictionary_III(Pui3_, Energy_,2) = 'pu3e'

    ! Outer Heliosphere Pop4 / arbitrary pick up ion
    Dictionary_III(Pui4_, Rho_,   2) = 'pu4rho'
    Dictionary_III(Pui4_, RhoUx_, 2) = 'pu4mx'
    Dictionary_III(Pui4_, RhoUy_, 2) = 'pu4my'
    Dictionary_III(Pui4_, RhoUz_, 2) = 'pu4mz'
    Dictionary_III(Pui4_, p_,     2) = 'pu4p'
    Dictionary_III(Pui4_, Energy_,2) = 'pu4e'

  end subroutine create_dictionary
  !============================================================================
end module ModProcessVarName
!==============================================================================
