!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf

module BATL_amr_criteria

  ! All AMR criteria have the following form:
  ! * Refinement criteria, RefineCrit..._I
  ! * Coarsening criteria,  CoarsenCrit...._I
  ! * Geometric area where it will be used, Area_I
  ! * Quantity to compare to, AmrCrit_IB
  !      (grid resolution or change in State variable)
  ! * Maximum refinement level/resolution, MaxLevelCrit_I

  ! Overview of varables used for AMR. The [..] indicates the used index ranges
  ! nAmrCritUsed: Total number of criteria used
  ! nAmrCrit: Total number of unique criteria used
  ! nExtCrit: Number of criteria coming form outside BATL, values in AmrCrit_IB
  ! nIntCrit: Number of criteria handled inside BATL, values in AmrCrit_IB
  ! nPhysCritUsed: Number of criteria given by #AMRCRITERIA.....
  ! nCritDxLevel: Number of geometric criteria among nPhysCritUsed
  ! nCritGrid: Number of criteria from #GRIDLEVEL/RESOLUTION
  ! nGeoCrit: =2, dx(1) and level(2)
  ! RefineCritAll_I[nAmrCritUsed] : Limit for criteria to do refinment
  ! CoarsenCritAll_I[nAmrCritUsed]: Limit for criteria for when to coarsen
  ! ResolutionLimit_I[nAmrCritUsed]:
  !      Criteria will not be applied for block with better resolution than set
  ! iResolutionLimit_I[nAmrCritUsed]:
  !      Index of ResolutionLimit_I for dx/level crit
  ! iMapToUniqCrit_I[nAmrCritUsed]: Map from used criteria to unique criteria
  ! AreaAll_I[maxarea]  : List of all ares used by the amr criteria
  ! AmrCirt_IB[nAmrCrit,nBlock]   : Store the criteria values we compare to
  ! iAreaIdx_II[nAreaMax,nAmrCritUsed] : Index of criteria in AreaAll_I
  ! nAreaPerCrit_I[nAmrCritUsed]  : Number of areas appied for a criteria

  use BATL_tree, ONLY: nDesiredRefine,nNodeRefine,nDesiredCoarsen, &
       nNodeCoarsen, DoStrictAmr, iRank_A, Rank_A, nNodeSort

  use BATL_region, ONLY: &
       init_region, clean_region, i_signed_region, block_inside_regions, &
       IsNewGeoParam, MaxArea, nArea, Area_I, Area, &
       MaxArea, nCritGrid

  use ModUtilities, ONLY: CON_stop
  use BATL_mpi, ONLY: iProc

  implicit none

  SAVE

  private ! except

  public:: set_amr_criteria
  public:: clean_amr_criteria
  public:: read_amr_criteria
  public:: calc_error_amr_criteria
  public:: set_amr_geometry
  public:: init_amr_criteria
  public:: is_masked_amr_criteria

  ! DoSortAmrCrit : true/false for generating the priority list iRank_A
  !
  ! DoSoftAmrCrit : standard behavior (false) is that criteria has to be
  ! refined/coarsened or we will stop the program. If true we will
  ! refine/coarsen them as much as we can, this is the default behavior for
  ! amr by percentage.

  logical, public :: &
       DoSortAmrCrit = .false., &
       DoSoftAmrCrit = .false., &
       DoAutoAmr     = .false.

  ! Need to init if PARAM.in was read (again)
  logical :: IsNewPhysParam = .false.

  ! Used for combination of sorted and not sorted criteria
  logical :: IsUnsortedAMR = .false.

  ! Try to make geometric dependence for refinement/coarsening
  logical, public :: DoCritAmr = .false.

  ! We have only two values to decide the criteria
  ! for geometric refinement: cell size (1) and AMR level (2)
  integer, public :: nGeoCrit = 2

  ! compatible with simple AMR (using #GRIDLEVEL / #GRIDRESOLUTION)
  logical:: IsSimpleAmr = .true.

  ! Storing all the AMR criteria
  integer, public:: nAmrCrit = 0
  integer:: nAmrCritUsed = 0
  real, allocatable, public:: AmrCrit_IB(:,:)

  ! Masking criteria for use
  logical, allocatable :: UseCrit_IB(:,:)
  logical, allocatable :: UseCrit_B(:)

  ! Choosing which blocks we want to refine is based on a list of criteria
  ! and a set of upper (refine)  and lower (coarsen) limits. The criteria
  ! can be external or calculated internally by estimating the
  ! numerical errors (calc_error_amr_criteria) based on the state variables.
  real,    allocatable:: CoarsenCritAll_I(:), RefineCritAll_I(:)
  integer, allocatable:: iVarCritAll_I(:), iResolutionLimit_I(:)
  real,    allocatable:: ResolutionLimit_I(:)
  integer, allocatable:: iAreaIdx_II(:,:)

  ! Storing names of areas for each criteria given by #AMRCRITERIA.....
  integer, allocatable :: nAreaPerCritAll_I(:)

  ! Percentage to refine and coarsen
  real ::  PercentRefine=0.0, PercentCoarsen=0.0

  ! Threshold limits for refine or unrefined the grid (length nCrit)
  real, allocatable:: CoarsenCritPhys_I(:), RefineCritPhys_I(:)

  ! Give max level (-) or resolution dependent on which index
  ! MaxGeoCritPhys_I points to
  real, allocatable   :: LevelMaxCritPhys_I(:)
  integer,allocatable :: MaxGeoCritPhys_I(:)

  ! iMapToUniqCrit_I map indexes from all indexes (nAmrCritUsed) to
  ! number of uniqe indexes (nAmrCrit) used by AmrCrit_IB
  integer, allocatable :: iMapToUniqCrit_I(:)

  ! Map the internal error criteria index to the state varable index
  integer, allocatable :: iMapToStateVar_I(:)

  ! Parameters used by calc_error_amr_criteria to estimate the errors
  real :: cAmrWavefilter = 1.0e-2
  real, parameter :: cEpsilon = 1.0d-8 ! avoid zero in denominator

  ! Number of geometric criteria from #AMRCRITERIALEVEL/RESOLUTION commands
  integer :: nCritDxLevel = 0

  integer :: nExtCrit = 0 ! Number of external + sorting criteia from BATSRUS
  integer :: nIntCrit = 0 ! Number of internal criteria, 2nd order err estimate
  integer :: nPhysCritUsed = 0 ! total criteria read by read_amr_criteria
  integer :: iStartCrit =0, iEndCrit=0 ! criteria segment we want to work on

  ! Storing names of areas for each criteria given by #AMRCRITERIA.....
  integer, public, allocatable :: nAreaPerCritPhys_I(:)

  ! The max difference in the criteria to say they have the same value,
  ! taking care of round-off errors
  real :: DeltaCriteria = 1.0e-8

  ! Number of characters on TypeCriteria
  integer, parameter :: nChar=200

  ! store area type for each criteria [nArea,nCrit]
  character(nChar), allocatable :: NameAreaPhys_II(:,:)

  ! Converting index form running block and proc number to node numbers
  integer, allocatable :: iNode_I(:)

  ! For usecellsize
  logical :: UseCellSize = .false.

  !$acc declare create(AmrCrit_IB, nAmrCrit, DoCritAmr, DoAutoAmr, DoStrictAmr)

contains
  !============================================================================
  subroutine init_amr_criteria

    use ModNumConst, ONLY: cRadToDeg
    use BATL_geometry, ONLY: &
         DomainSize_D, CellSizeRoot, IsLogRadius, IsGenRadius, Phi_
    use BATL_size, ONLY: MaxBlock, nBlock, nI, nIJK_D
    use BATL_tree, ONLY: Unused_B,  nRoot_D

    integer :: iBlock, iCrit, nCrit, iGeo, iArea, iAreaIdx, iVar

    logical, parameter :: DoTest = .false.

    character(len=*), parameter:: NameSub = 'init_amr_criteria'
    !--------------------------------------------------------------------------
    if(CellSizeRoot < 0.0)then
       ! Set maximum cell size in the first non-stretched dimension
       if(IsLogRadius .or. IsGenRadius)then
          CellSizeRoot = cRadToDeg*DomainSize_D(Phi_) &
               /(nRoot_D(Phi_)*nIJK_D(Phi_))
       else
          CellSizeRoot = DomainSize_D(1)/(nRoot_D(1) * nI)
       end if
    end if

    if(.not.(IsNewGeoParam .or. IsNewPhysParam)) RETURN

    nAmrCrit =  nIntCrit + nExtCrit + nGeoCrit

    if(allocated(AmrCrit_IB)) deallocate(AmrCrit_IB)
    allocate(AmrCrit_IB(nAmrCrit,MaxBlock))
    AmrCrit_IB = -1.0

    ! Merging Data from Geometrical and Physical refinement parameters
    nCrit = nPhysCritUsed + nCritGrid

    if(allocated(RefineCritAll_I))then
       deallocate(RefineCritAll_I,&
            CoarsenCritAll_I,&
            iVarCritAll_I,&
            ResolutionLimit_I,&
            iResolutionLimit_I,&
            iAreaIdx_II,&
            nAreaPerCritAll_I)
    endif

    if (allocated(iResolutionLimit_I)) deallocate(iResolutionLimit_I)
    if (allocated(iAreaIdx_II))        deallocate(iAreaIdx_II)
    if (allocated(nAreaPerCritAll_I))  deallocate(nAreaPerCritAll_I)
    if (allocated(ResolutionLimit_I))  deallocate(ResolutionLimit_I)

    allocate(RefineCritAll_I(nCrit),&
         CoarsenCritAll_I(nCrit),&
         iVarCritAll_I(nCrit),&
         ResolutionLimit_I(nCrit),&
         iResolutionLimit_I(nCrit),&
         iAreaIdx_II(MaxArea,nCrit),&
         nAreaPerCritAll_I(nCrit))

    iVarCritAll_I = 0

    if(nPhysCritUsed > 0 ) then
       ! Copy physics based criteria into the list of all criteria
       RefineCritAll_I(1:nPhysCritUsed)   = RefineCritPhys_I
       CoarsenCritAll_I(1:nPhysCritUsed)  = CoarsenCritPhys_I
       ResolutionLimit_I(1:nPhysCritUsed) = LevelMaxCritPhys_I
       iVarCritAll_I(1:nPhysCritUsed)     = iMapToUniqCrit_I
       nAreaPerCritAll_I(1:nPhysCritUsed) = nAreaPerCritPhys_I

       ! fixing indexing as "physics" criteria comes first
       do iCrit = 1, nPhysCritUsed
          iResolutionLimit_I(iCrit) = &
               max(0, maxval(iVarCritAll_I(1:nPhysCritUsed-nCritDxLevel))) + &
               MaxGeoCritPhys_I(iCrit)
       end do
    end if

    ! Initialize geometry based criteria
    call init_region

    ! Loop over the areas defined by #GRIDLEVEL/RESOLUTION commands
    ! That do not have a name

    ! Start geometric criteria index after the non-geometric criteria
    iCrit = nPhysCritUsed

    do iGeo = 1, nArea
       Area => Area_I(iGeo)

       ! Exclude named areas defined by #REGION / #AMRREGION commands
       if( Area%NameRegion /= "NULL") CYCLE

       ! Append this geometric AMR criterion to the array of all criteria
       iCrit = iCrit + 1
       if(Area%iLevel  < 0) then
          ! Level based criteria
          iVar = 2
          RefineCritAll_I(iCrit)   = Area%iLevel
          CoarsenCritAll_I(iCrit)  = RefineCritAll_I(iCrit) - 1
          ResolutionLimit_I(iCrit) = Area%iLevel
       else
          ! Resolution based criteria
          iVar = 1
          RefineCritAll_I(iCrit)   = Area%Resolution
          CoarsenCritAll_I(iCrit)  = Area%Resolution/2
          ResolutionLimit_I(iCrit) = Area%Resolution
       end if

       ! All geometrical criteria are comparisons to resolution or level
       if(nPhysCritUsed  > 0) then
          iVarCritAll_I(iCrit) = &
               maxval(iVarCritAll_I(1:nPhysCritUsed-nCritDxLevel)) + iVar
          iResolutionLimit_I(iCrit) = &
               maxval(iVarCritAll_I(1:nPhysCritUsed-nCritDxLevel)) + iVar
       else
          iVarCritAll_I(iCrit)      = iVar
          iResolutionLimit_I(iCrit) = iVar
       end if

       ! These are simple criteria associated with a single area
       iAreaIdx_II(1,iCrit) = iGeo
       nAreaPerCritAll_I(iCrit) = 1
    end do

    if(nPhysCritUsed > 0 ) then
       do iCrit = 1, nPhysCritUsed
          do iArea = 1, nAreaPerCritAll_I(iCrit)
             iAreaIdx_II(iArea,iCrit) = &
                  i_signed_region(adjustl(NameAreaPhys_II(iArea,iCrit)))
          end do
       end do
    end if

    ! Total number of refine/coarsening limmits
    nAmrCritUsed = nCrit

    ! Allocate Criteria mask
    if(.not. allocated(UseCrit_B)) allocate(UseCrit_B(MaxBlock))
    if(allocated(UseCrit_IB)) deallocate(UseCrit_IB)
    allocate(UseCrit_IB(nAmrCritUsed,MaxBlock))

    UseCrit_IB = .false.
    UseCrit_B  = .false.

    do iBlock = 1, nBlock
       if(Unused_B(iBlock)) CYCLE
       call set_amr_geometry(iBlock)
    end do

    if(DoTest .and. iProc == 0) then
       write(*,*) NameSub,' finished with'
       write(*,"(A17,100(F10.3))")"RefineCritAll_I   :", RefineCritAll_I
       write(*,"(A17,100(F10.3))")"CoarsenCritAll_I  :", CoarsenCritAll_I
       write(*,"(A17,100(F10.3))")"ResolutionLimit_I :", ResolutionLimit_I
       write(*,"(A17,100(I10))")  "iResolutionLimit_I:", iResolutionLimit_I
       write(*,"(A17,100(I10))")  "iVarCritAll_I     :", iVarCritAll_I
       write(*,"(A17,100(I10))")  "nAreaPerCritAll_I :", nAreaPerCritAll_I
       do iCrit = 1, nAmrCritUsed
          write(*,"(A11,I3)") "Criteria : ",iCrit
          do iArea = 1, nAreaPerCritAll_I(iCrit)
             iAreaIdx = iAreaIdx_II(iArea,iCrit)
             write(*,"(A14,A6,I4,A8,A10,A14,A10)") " ","Idx = ", iAreaIdx, &
                  " NameShape = ", Area_I(abs(iAreaIdx))%NameShape, &
                  " NameRegion = ", Area_I(abs(iAreaIdx))%NameRegion
          end do
       end do
       write(*,"(A17,100(I10))") "iVarCritAll_I:", iVarCritAll_I
       write(*,"(A17,100(I10))") "AmrCrit_IB   :", shape(AmrCrit_IB)
       write(*,*) ""
       do iCrit=1,nAmrCritUsed
          write(*,*) "iAreaIdx_II( :,iCrit) ", &
               iAreaIdx_II( 1:nAreaPerCritAll_I(iCrit),iCrit)
       end do
       write(*,*) ""
       write(*,*) " 1 : nAmrCritUsed = ", 1," : ",nAmrCritUsed
       write(*,*) " Geo range        = ", &
            nPhysCritUsed +1," : ", nAmrCritUsed
       write(*,*) " Phys range       = ",1," : ", nPhysCritUsed
       write(*,*) " Error Phys       = ",1," : ", nIntCrit
       write(*,*) " Ext calc range   = ",nIntCrit+1," : ",nIntCrit+nExtCrit
       write(*,*) " shape AmrCrit_IB = ",shape(AmrCrit_IB)
       write(*,*) ""
       write(*,*) ""
    end if

    ! All necessary things are updated
    IsNewPhysParam = .false.
    IsNewGeoParam  = .false.

    ! cleaning arrays for read_amr_criteria
    if(nPhysCritUsed > 0 ) deallocate( &
         RefineCritPhys_I,&
         CoarsenCritPhys_I,&
         LevelMaxCritPhys_I, &
         iMapToUniqCrit_I, &
         MaxGeoCritPhys_I,&
         nAreaPerCritPhys_I,&
         NameAreaPhys_II)

  end subroutine init_amr_criteria
  !============================================================================
  subroutine set_amr_criteria(nVar, State_VGB, nInCritExtUsed, &
       CritExt_IB, Used_GB, TypeAmrIn)

    use BATL_size, ONLY: MinI, MaxI, MinJ, MaxJ, MinK, MaxK,&
         MaxBlock,nBlock
    use BATL_tree, ONLY: Unused_B!,iStatusNew_A,Refine_

    integer,  intent(in)  :: nVar
    real,    intent(in),optional  :: & ! state variables
         State_VGB(nVar,MinI:MaxI,MinJ:MaxJ,MinK:MaxK,MaxBlock)
    integer, intent(in), optional :: nInCritExtUsed
    real, intent(in), optional :: CritExt_IB(nExtCrit, nBlock)
    logical, intent(in), optional:: &
         Used_GB(MinI:MaxI,MinJ:MaxJ,MinK:MaxK,MaxBlock)
    character(3), intent(in), optional:: TypeAmrIn
    character(3) :: TypeAmr
    integer :: iCrit, iBlock

    logical, parameter:: DoTest = .false.
    character(len=*), parameter:: NameSub = 'set_amr_criteria'
    !--------------------------------------------------------------------------

    ! nExtCritUsed = 0
    if(present(nInCritExtUsed)) then
       if(nInCritExtUsed /= nExtCrit) then
          write(*,*) nInCritExtUsed," /= ",nExtCrit
          write(*,*) ""
          call CON_stop("nExtCrit can only change by changes in PARAM.in")
       end if
    end if

    ! If Area_I(:)%TypeCoordIn is not empty, the Geometry may need to be
    ! updated. For example, in the solar wind simulation, the earthcone
    ! may use 'GSE' so that the cone position will needs to be updated
    ! if needed.
    if (any(Area_I(:)%TypeCoordIn /= '')) then
       do iBlock = 1, nBlock
          if(Unused_B(iBlock)) CYCLE
          call set_amr_geometry(iBlock)
       end do
    end if

    TypeAmr = 'all'
    if(present(TypeAmrIn))then
       TypeAmr = TypeAmrIn
       if(iProc == 0 .and. DoTest) write(*,*) NameSub,' TypeAmrIn =', TypeAmrIn
    end if

    ! Compatible with simple AMR using #GRIDLEVEL/#GRIDRESOLUTION
    if(IsSimpleAmr .and.  TypeAmr=='all') then
       if(nPhysCritUsed > 0) then
          TypeAmr = 'phy'
       else
          TypeAmr = 'geo'
       end if
       if(iProc == 0 .and. DoTest) write(*,*) &
            NameSub,' nPhysCritUsed, TypeAmr =', TypeAmr
    end if

    ! add external criteria into the list of all criteria
    if(present(CritExt_IB)) then
       do iBlock = 1, nBlock
          if(Unused_B(iBlock)) CYCLE
          do iCrit = 1, nExtCrit
             AmrCrit_IB(nIntCrit+iCrit,iBlock) = CritExt_IB(iCrit,iBlock)
          end do
       end do
    end if

    if(iProc == 0 .and. DoTest)write(*,*) NameSub, &
         ': TypeAmr, nAmrCritUsed,  nPhysCritUsed, nCritDxLevel=', &
         TypeAmr, nAmrCritUsed,  nPhysCritUsed, nCritDxLevel

    !-------- set up index Ranges and find blocks to refine/coarsen -------
    select case(TypeAmr)
    case('all')
       iStartCrit = 1
       iEndCrit   = nAmrCritUsed
    case('geo')
       iStartCrit = nPhysCritUsed - nCritDxLevel +1
       iEndCrit   = nAmrCritUsed
       if(iProc == 0 .and. DoTest)write(*,*) NameSub, &
            ' geo iStartCrit, iEndCrit=', iStartCrit, iEndCrit
       call apply_unsorted_criteria
       RETURN
    case('phy')
       iStartCrit = 1
       iEndCrit  = nPhysCritUsed - nCritDxLevel
    case default
       call CON_stop(NameSub // &
            ' ERROR: Unknown TypeAmr = '//TypeAmr)
    end select
    if(iProc == 0 .and. DoTest) write(*,*) NameSub, &
         ' iStartCrit, iEndCrit=', iStartCrit, iEndCrit

    ! Estimation of the numerical error
    if(nIntCrit >0) &
         call calc_error_amr_criteria(nVar, State_VGB, Used_GB=Used_GB)

    if(DoSortAmrCrit .or. .not.DoStrictAmr) then
       ! we make an AMR priority list
       ! Only sort Physics based AMR
       ! Geomtry criteria is all or none
       if(TypeAmr /= 'phy') then            ! ALL
          iStartCrit = nPhysCritUsed-nCritDxLevel +1
          iEndCrit   = nAmrCritUsed
          call apply_unsorted_criteria
          IsUnsortedAMR = .true.
       end if
       ! Physical criteria
       iStartCrit = 1
       iEndCrit  = nPhysCritUsed
       call sort_amr_criteria
    else
       ! refine only based on criteria
       call apply_unsorted_criteria
    end if

    IsUnsortedAMR = .false.

  end subroutine set_amr_criteria
  !============================================================================
  subroutine sort_amr_criteria

    ! the routine will find the candidate for refinement or coursing based
    ! upon the percentage of refinement and the percentage wanted refined and
    ! and coarsened. It will ONLY modify the criteria the values in the
    ! criteria list itself. The refinement criteria will overrule the
    ! percentage refinement.

    use BATL_mpi, ONLY: iComm, nProc
    use ModMpi
    use BATL_size, ONLY: nBlock
    use ModSort, ONLY: sort_quick
    use BATL_tree, ONLY: Unused_BP,iStatusNew_A, Refine_, Coarsen_, &
         Unset_, iNode_B, nNode, nNodeUsed, diffRange,Unused_B

    ! Array containing all criteria for all blocks gathered
    real, allocatable :: AllCrit_II(:,:)

    ! CritSort_II contains all criteria for all blocks
    ! iIdxSort_II gives the new indexes after sorting
    ! and reused for other functions.
    integer, allocatable :: iIdxSort_II(:,:)
    real, allocatable :: CritSort_II(:,:)
    integer, allocatable :: iRank_I(:)
    real, allocatable :: Rank_I(:), RankN_I(:)
    logical, allocatable ::  UseAllCrit_II(:,:)

    ! AllRank_II(Nodes given by iNode_I,2) [rank,N ranks for node]
    real, allocatable :: AllRank_II(:,:)

    ! translate sorting continous index to iNode_I index
    integer, allocatable :: iSortToNode_I(:)

    ! will have value 0: unset, 1:refine and -1:coursen to count
    ! the number of nodes to refine and/or coursen by criteria
    integer, allocatable :: nAmrTag_A(:)
    integer :: iCrit, iBlock, jProc, k
    integer :: iCritSort, iSort, iVarCrit, MaxNodeSort, iNodeSort
    integer :: iError
    real    :: Diff, Crit

    integer :: nTotBlocks, nBlockMax, iTotalCrit, iHelpCrit
    integer, allocatable :: nBlock_P(:), nReciveCont_P(:), nRecivDisp_P(:)
    integer, allocatable :: nUseReciveCont_P(:), nUseRecivDisp_P(:)

    ! indexes for AmrCrit_IB as we can have multiple crit using the same
    ! AmrCrit_IB criteria
    integer :: nVarStart, nVarEnd

    ! need extra array space to store temperal data
    !--------------------------------------------------------------------------
    iTotalCrit = 2
    iHelpCrit  = 3

    nVarStart = iVarCritAll_I(iStartCrit)
    nVarEnd   = iVarCritAll_I(iEndCrit)

    ! COMMENTS FOR FUTURE : we can use the Unused_B to only pick and send
    ! the data that is actively used, reducing communication

    !------------ Get information on all the blocks on all process -----------

    ! collect nBlock from each process
    allocate(nBlock_P(nProc),nReciveCont_P(nProc),nRecivDisp_P(nProc))
    allocate(nUseReciveCont_P(nProc),nUseRecivDisp_P(nProc))
    call MPI_Allgather(nBlock, 1,MPI_INTEGER, nBlock_P, 1, &
         MPI_INTEGER, iComm, iError)
    nBlockMax = maxval(nBlock_P)
    nTotBlocks = sum(nBlock_P)

    !------------ Collect all criteria for sorting and ranking ------------

    ! collect all criteria for the total grid
    allocate(AllCrit_II(nAmrCrit,nTotBlocks))
    allocate(UseAllCrit_II(nAmrCritUsed,nTotBlocks))
    AllCrit_II = -77

    ! store the Node indexing iNode_B for all processes
    if(allocated(iNode_I)) deallocate(iNode_I)
    allocate(iNode_I(nBlockMax*nProc))
    allocate(AllRank_II(nBlockMax*nProc,2))
    allocate(iSortToNode_I(nBlockMax*nProc))
    allocate(nAmrTag_A(nBlockMax*nProc))

    iNode_I = 0
    iSortToNode_I = -1
    AllRank_II =0
    nAmrTag_A = 0

    ! Set up the displacement and size for collecting the data for
    ! AllCrit_II
    nReciveCont_P = nBlock_P*nAmrCrit
    nRecivDisp_P(1) = 0
    do jProc=2,nProc
       nRecivDisp_P(jProc) = &
            nRecivDisp_P(jProc-1)+nBlock_P(jProc-1)*nAmrCrit
    end do

    ! Set up the displacement and size for collecting the data for
    ! UseAllCrit_II
    nUseReciveCont_P = nBlock_P*nAmrCritUsed
    nUseRecivDisp_P(1) = 0
    do jProc=2,nProc
       nUseRecivDisp_P(jProc) = &
            nUseRecivDisp_P(jProc-1)+nBlock_P(jProc-1)*nAmrCritUsed
    end do

    ! Get all criteria maskes, so we can exlude them
    call MPI_allgatherv(UseCrit_IB, nAmrCritUsed*nBlock, MPI_LOGICAL, &
         UseAllCrit_II, nUseReciveCont_P, nUseRecivDisp_P, &
         MPI_LOGICAL, iComm, iError)

    do iBlock=1,nBlock
       if(Unused_B(iBlock)) CYCLE
       ! print *,iProc," inode (iBlock) :: ",iBlock,  iNode_B(iBlock)
       if(iStatusNew_A(iNode_B(iBlock))== Refine_) then
          AmrCrit_IB(nVarStart:nVarEnd,iBlock) = -777.0
          ! print *,"WHAT Refine_", nVarStart,nVarEnd,iBlock,iNode_B(iBlock)
       end if
    end do

    ! gathering the criteria
    call MPI_allgatherv(AmrCrit_IB, nAmrCrit*nBlock, MPI_REAL, &
         AllCrit_II, nReciveCont_P, nRecivDisp_P, &
         MPI_REAL, iComm, iError)

    ! Set up the displacement and size for collecting the data for iNode_I
    nReciveCont_P = nBlock_P
    nRecivDisp_P(1) = 0
    do jProc=2,nProc
       nRecivDisp_P(jProc) = nRecivDisp_P(jProc-1)+nBlock_P(jProc-1)
    end do

    ! gathering the node indexes
    call MPI_allgatherv(iNode_B, nBlock,MPI_INTEGER, &
         iNode_I, nReciveCont_P, nRecivDisp_P, &
         MPI_INTEGER, iComm, iError)

    ! Setting up arrays for sorting and index handling
    allocate( &
         CritSort_II(nTotBlocks,1), &
         iIdxSort_II(nTotBlocks,iHelpCrit)  )

    ! iRank_A is public, so deallocate it if necessary
    if(allocated(iRank_A)) deallocate(iRank_A,Rank_A)
    allocate(iRank_I(nTotBlocks), Rank_I(nTotBlocks),&
         iRank_A(nTotBlocks), Rank_A(nTotBlocks), RankN_I(nTotBlocks))

    iRank_A      = 0
    Rank_A       = 0.0
    iRank_I      = 0
    Rank_I       = 0.0
    iIdxSort_II  = 0
    CritSort_II  = 0.0
    RankN_I      = -1.0
    iRank_I = nNode+1 ! larger than any possible rank
    iIdxSort_II(:,iHelpCrit) = 0 ! store info about if a block is already
    !                               marked for refinement(+1) or coarsening(-1)
    nNodeRefine  = 0
    nNodeCoarsen = 0
    MaxNodeSort = 0

    ! Working on 1 criteria at the time
    do iCrit=iStartCrit,iEndCrit

       iVarCrit = iVarCritAll_I(iCrit)
       ! copy criteria data into the sorting arrays
       iSort = 0
       do jProc = 1, nProc
          do iBlock = 1, nBlock_P(jProc)
             if(Unused_BP(iBlock,jProc-1)) CYCLE
             if(.not.UseAllCrit_II(iCrit,iBlock+nRecivDisp_P(jProc))) then
                CYCLE
             end if
             if(AllCrit_II(iVarCrit,iBlock+nRecivDisp_P(jProc)) == -777.0) then
                CYCLE
             end if
             iSort = iSort +1
             CritSort_II(iSort,1) = &
                  AllCrit_II(iVarCrit,iBlock+nRecivDisp_P(jProc))
             iIdxSort_II(iSort, 1) = iSort
             iSortToNode_I(iSort) = iBlock+nRecivDisp_P(jProc)
             ! RankN_I are recalulated each time, but at the moment we have no
             ! constant indexing so to get the corect match we do it each time
             RankN_I(iSortToNode_I(iSort)) = &
                  1.0/real(nTotBlocks &
                  *count(UseAllCrit_II(iStartCrit:iEndCrit, &
                  iBlock+nRecivDisp_P(jProc))))
          end do
       end do

       if(isort == 0) CYCLE

       nNodeSort = iSort
       MaxNodeSort = max(nNodeSort,MaxNodeSort)

       ! Sort each criteria. Pass in first index of array arguments
       call sort_quick(nNodeSort, &
            CritSort_II(1:nNodeSort,1), iIdxSort_II(1:nNodeSort,1))

       ! Store the sorting index in secound colum (iTotalCrit)
       iIdxSort_II(:,iTotalCrit) = iIdxSort_II(:,1)
       k = 1
       iSort = iIdxSort_II(1,iTotalCrit)
       ! Value of first element (min)
       iIdxSort_II(iSort,1) = k
       Crit = CritSort_II(iSort,1)

       ! group together criteria which has a diffrence of
       ! less then DeltaCriteria by giving them the same
       ! sort ranking
       do iCritSort = 2,nNodeSort
          iSort = iIdxSort_II(iCritSort,iTotalCrit)
          if((CritSort_II(iSort,1)- Crit) < DeltaCriteria) then
             iIdxSort_II(iSort,1) = k
          else
             k = k + 1
             iIdxSort_II(iSort,1) = k
             Crit = CritSort_II(iSort,1)
          end if
       end do

       do iSort = 1,nNodeSort
          iCritSort = iIdxSort_II(iSort,iTotalCrit)
          iNodeSort = iSortToNode_I(iCritSort)
          ! Use the minimum position (=rank) of the node in sorted criteria
          ! array to set up the new ranking order for each node when doing amr
          iRank_I(iNodeSort) = min(iRank_I(iNodeSort),iIdxSort_II(iCritSort,1))

          ! if some of the nodes have the same minimal rank we will use the
          ! average rank of all criteria as a secondary priority.
          Rank_I(iNodeSort) = Rank_I(iNodeSort) &
               + RankN_I(iNodeSort)*iIdxSort_II(iCritSort,1)
       end do

       ! To make sure that blocks that are marked for refinement/coarsening
       ! by thresholds will come at the top/bottom of the ranking list
       ! we will shift the values with the total number of blocks used.
       ! COMMENTS FOR FUTURE : the loop may be split for one that goes
       ! form the top and one that goes form bottom so it do not need to go
       ! though the whole list of elements

       ! Only if criteria are in use
       if(   RefineCritAll_I(iCrit) > -0.5 .and. &
            CoarsenCritAll_I(iCrit) > -0.5) then
          do iSort = nNodeSort,1,-1
             iCritSort = iIdxSort_II(iSort,iTotalCrit)
             iNodeSort = iSortToNode_I(iCritSort)
             ! Block has already ben marked
             ! if(iIdxSort_II(iCritSort,iHelpCrit) /= 0 ) CYCLE

             if(CritSort_II(iCritSort,1) > RefineCritAll_I(iCrit) &
                  .and. iIdxSort_II(iCritSort,iHelpCrit) /= 1) then
                ! Satisfies refinement threshold and not yet marked for
                ! refinement

                ! tag for refinment
                nAmrTag_A(iNodeSort) = 1

                ! Shift up the rank above sorted values
                Rank_I(iNodeSort) = Rank_I(iNodeSort) + nTotBlocks+1

                !! If node was originally marked for coarsening,
                !! now there is one fewer node to be coarsened
                !  if(iIdxSort_II(iCritSort,iHelpCrit) == -1) &
                !     nNodeCoarsen = nNodeCoarsen - 1

                ! Noe node is marked fore refinement
                ! nNodeRefine = nNodeRefine +1
                iIdxSort_II(iCritSort,iHelpCrit) = 1

             else if(CritSort_II(iCritSort,1) < CoarsenCritAll_I(iCrit) &
                  .and. iIdxSort_II(iCritSort,iHelpCrit) == 0) then
                ! Satisfies coarsening threshold and not yet marked at all !

                ! Shift down the rank below sorted values
                Rank_I(iNodeSort) = -Rank_I(iNodeSort)-nTotBlocks-1

                ! Now node is marked for coarsening
                ! nNodeCoarsen = nNodeCoarsen +1
                nAmrTag_A(iNodeSort) = -1
                iIdxSort_II(iCritSort,iHelpCrit) = -1
             end if
          end do
       end if

       nNodeCoarsen = count(nAmrTag_A == -1)
       nNodeRefine  = count(nAmrTag_A == 1)

    end do

    ! store the sorted indexing list for Rank_A
    iIdxSort_II(:,iTotalCrit) = 0

    ! The quick sort algorithm assume a continuous indexing order,
    ! this will not necessary be true so we need to make a
    ! mapping between the block numbers and the node numbers when
    ! there are unused blocks. This is stored in iIdxSort_II(iSort,1)
    k = 0
    iSort = 0
    do jProc = 1, nProc
       do iBlock = 1, nBlock_P(jProc)
          k = k +1
          if(Unused_BP(iBlock,jProc-1)) CYCLE
          if(.not.any(UseAllCrit_II(iStartCrit:iEndCrit,k))) CYCLE
          if(all(AllCrit_II(nVarStart:nVarEnd,k) == -777.0)) CYCLE
          iSort = iSort+1
          Rank_I(iSort) = iRank_I(k) + Rank_I(k)
          iIdxSort_II(iSort,1) = iNode_I(k)
          CritSort_II(iSort,1) = &
               AllCrit_II(iResolutionLimit_I(iStartCrit), &
               iBlock + nRecivDisp_P(jProc))
       end do
    end do
    nNodeSort = iSort

    ! Get the number of elements to refine by percentage
    ! taking into account that each refinements generate nChild-1 new blocks
    nDesiredRefine  = floor(PercentRefine*nNodeUsed/(100.0))
    nDesiredCoarsen = floor(PercentCoarsen*nNodeUsed/100.0)

    ! make sure thay do not take more then thay can
    nDesiredRefine  = min(nNodeSort,nDesiredRefine)
    nDesiredCoarsen = min(nNodeSort,nDesiredCoarsen)

    ! nNodeRefine and nNodeCoarsen are based on thresholds.
    ! If DoSoftAmrCrit is false, these have to be refined/coarsened,
    ! or else the code stops.
    ! If DoSoftAmrCrit is true, we change the threshold based
    ! values into "desired" values, so the code behaves as if
    ! the criteria were based on percentages.
    if(DoSoftAmrCrit .or. .not.DoStrictAmr) then
       nDesiredRefine  = max(nDesiredRefine,  nNodeRefine)
       nDesiredCoarsen = max(nDesiredCoarsen, nNodeCoarsen)
       nNodeRefine  = 0
       nNodeCoarsen = 0
    end if

    ! Finding final rank ordering
    call sort_quick(nNodeSort, Rank_I, iIdxSort_II(1,iTotalCrit))

    ! map local indexing in this subroutine to the node index
    do iSort = 1, nNodeSort
       iCritSort = iIdxSort_II(iSort,iTotalCrit)
       iRank_A(iSort) =iIdxSort_II(iCritSort,1)
       Rank_A(iSort) = Rank_I(iCritSort)
    end do

    ! making sure that the refinment and coarsning are starting from
    ! a position with have a transition in its criteria larger then diffRange
    if(nDesiredRefine > nNodeRefine .and.  nDesiredRefine < nNodeSort )then
       k=-1
       do iSort = nNodeSort-nDesiredRefine, nNodeSort-nNodeRefine
          diff =  abs(Rank_A(iSort) - Rank_A(nNodeSort- nDesiredRefine+1))
          if( diff > diffRange) EXIT
          k=k+1
       end do
       nDesiredRefine = nDesiredRefine - max(0,k)
    end if

    if(nDesiredCoarsen > nNodeCoarsen .and. nDesiredCoarsen < nNodeSort )then
       k=-1
       do iSort= nDesiredCoarsen+1, max(nNodeCoarsen,1), -1
          diff = abs(Rank_A(iSort) - Rank_A(nDesiredCoarsen))
          if(diff >diffRange) EXIT
          k=k+1
       end do
       nDesiredCoarsen = nDesiredCoarsen - max(0,k)
    end if

    ! Give iStatusNew_A based on the iRank_A and number of blocks we want for
    ! refinement and coarsening
    do iSort = nNodeSort, nNodeSort-max(nDesiredRefine,nNodeRefine)+1, -1
       if( maxval(ResolutionLimit_I(iStartCrit:iEndCrit)) &
            >= CritSort_II(iIdxSort_II(iSort,iTotalCrit),1)) CYCLE
       iStatusNew_A(iRank_A(iSort)) = Refine_
    end do

    if(IsUnsortedAMR) then
       ! Do not coarsen blocks not in the sorted list
       do iSort = max(nDesiredCoarsen, nNodeCoarsen),nNodeSort
          if(iStatusNew_A(iRank_A(iSort)) ==  Coarsen_)&
               iStatusNew_A(iRank_A(iSort)) = Unset_
       end do
    else
       do iSort = 1, max(nDesiredCoarsen, nNodeCoarsen)
          if(iStatusNew_A(iRank_A(iSort)) /= Refine_)&
               iStatusNew_A(iRank_A(iSort)) =  Coarsen_
       end do
    end if

    deallocate(iSortToNode_I)
    deallocate(AllRank_II)
    deallocate(UseAllCrit_II)
    deallocate(AllCrit_II)
    deallocate(CritSort_II, iIdxSort_II)
    deallocate(nBlock_P, nReciveCont_P, nRecivDisp_P)
    deallocate(iRank_I, Rank_I)
    deallocate(RankN_I)
    deallocate(nAmrTag_A)

  end subroutine sort_amr_criteria
  !============================================================================
  subroutine calc_error_amr_criteria(nVar, State_VGB,Used_GB )

    ! The AMR criteria method used here are adapted from L{\"o}hner (1987).
    ! The flash3 user guide have also a description used.
    ! The method use the relative error ,E (Factor), of the approximation to
    ! the differential equation:
    !
    !  dU      DU         d^2 U
    ! ----  = ----  + DX  ------
    !  dx      DX          dx^2
    !
    ! Giving a relative error
    !
    !             d^2 U
    !            -------
    !              dx^2
    !    E  =  -------------
    !           1     dU
    !          ---  ------
    !          DX     dx
    !
    ! To avoid problems with small linear gradient fluctuations in the
    ! denominator we add it with a fraction of its average value.
    ! And to avoid zero in the denominator we add it with cEpsilon.
    ! The final expression is:
    !
    !                               d^2 U
    !                              -------
    !                               dx^2
    !    E  =  ----------------------------------------------
    !           1     dU        1
    !          ---  ------  + ------ cAmrWavefilter*<U> + cEpsilon
    !          DX     dx       DX^2
    !
    ! We use a wide stencil to do the refinement in time and
    ! neglected the diagonal terms.
    !
    ! The error factor for each variable will be normalized before
    ! threshold evaluation and also the sum of all the factors over
    ! all variables will be evaluated for deciding the refinement.
    !
    ! The subroutine only sets the iStatusNew_A array.
    !
    ! OPTIONAL:
    ! The user can sent in additional factors  used for deciding refinement
    ! together with these refinements criteria. This will be then used
    ! in addition to the error estimate described earlier.

    use BATL_size, ONLY: MinI, MaxI, MinJ, MaxJ, MinK, MaxK,&
	 nI, nJ, nK, MaxDim, nDim, MaxBlock, nBlock

    integer,  intent(in)   :: nVar
    real,    intent(in) :: &                            ! state variables
         State_VGB(nVar,MinI:MaxI,MinJ:MaxJ,MinK:MaxK,MaxBlock)
    logical, intent(in), optional:: &
         Used_GB(MinI:MaxI,MinJ:MaxJ,MinK:MaxK,MaxBlock)  ! used cells

    real :: Crit, Crit_D(MaxDim)
    real, parameter :: InvNdim = 1.0/nDim
    real :: Numerator, Denominator
    integer:: iBlock, iCrit, i, j, k, iVar

    character(len=*), parameter:: NameSub = 'calc_error_amr_criteria'
    !--------------------------------------------------------------------------
    Crit        = 1.0
    Crit_D      = 0.0
    Numerator   = 0.0
    Denominator = 0.0

    if(nIntCrit > nVar) call CON_stop(NameSub// &
         ":: More criteria then variables")

    ! Calculate error estimates (1..nIntCrit)
    BLOCK: do iBlock = 1, nBlock

       ! Check refinement first
       do k = 1, nK; do j = 1, nJ; do i = 1, nI
          ! Only variables indexed in iVarCritAll_I will decide the refinement

          if(present(Used_GB)) then
             if(nDim == 1) then
                if(.not.all(Used_GB((i-2):(i+2),j, k,iBlock))) CYCLE
             end if
             if(nDim == 2) then
                if(.not.all(Used_GB((i-2):(i+2),(j-2):(j+2),k,iBlock))) CYCLE
             end if
             if(nDim == 3) then
                if(.not.all( &
                     Used_GB((i-2):(i+2),(j-2):(j+2),(k-2):(k+2),iBlock))) &
                     CYCLE
             end if
          end if

          do iCrit = 1, nIntCrit
             if(.not.UseCrit_IB(iCrit,iBlock)) CYCLE

             iVar = iMapToStateVar_I(iCrit)

             Crit_D =0.0

             Numerator = abs( &
                  State_VGB(iVar,i-2,j,k,iBlock)   - &
                  2.0*State_VGB(iVar,i,j,k,iBlock) + &
                  State_VGB(iVar,i+2,j,k,iBlock) )
             Denominator =  (&
                  abs(State_VGB(iVar,i+2,j,k,iBlock) - &
                  State_VGB(iVar,i,j,k,iBlock)) + &
                  abs(State_VGB(iVar,i,j,k,iBlock) - &
                  State_VGB(iVar,i-2,j,k,iBlock))) + &
                  cAmrWavefilter * (&
                  abs(State_VGB(iVar,i+2,j,k,iBlock)) + &
                  abs(2.0*State_VGB(iVar,i,j,k,iBlock)) + &
                  abs(State_VGB(iVar,i-2,j,k,iBlock)))
             Crit_D(1) = (Numerator/max(Denominator,cEpsilon))
             Crit = Crit_D(1)**2

             if(nDim >= 2) then
                Numerator =  abs( &
                     State_VGB(iVar,i,j-2,k,iBlock)   - &
                     2.0*State_VGB(iVar,i,j,k,iBlock) + &
                     State_VGB(iVar,i,j+2,k,iBlock) )
                Denominator = (&
                     abs(State_VGB(iVar,i,j+2,k,iBlock) - &
                     State_VGB(iVar,i,j,k,iBlock)) + &
                     abs(State_VGB(iVar,i,j,k,iBlock) - &
                     State_VGB(iVar,i,j-2,k,iBlock))) + &
                     cAmrWavefilter * (&
                     abs(State_VGB(iVar,i,j+2,k,iBlock)) + &
                     abs(2.0*State_VGB(iVar,i,j,k,iBlock)) + &
                     abs(State_VGB(iVar,i,j-2,k,iBlock)))
                Crit_D(2) = (Numerator/max(Denominator,cEpsilon))
                Crit = Crit + Crit_D(2)**2
             end if

             if(nDim >= 3) then
                Numerator = abs( &
                     State_VGB(iVar,i,j,k-2,iBlock)   - &
                     2.0*State_VGB(iVar,i,j,k,iBlock) + &
                     State_VGB(iVar,i,j,k+2,iBlock))
                Denominator = (&
                     abs(State_VGB(iVar,i,j,k+2,iBlock) - &
                     State_VGB(iVar,i,j,k,iBlock)) + &
                     abs(State_VGB(iVar,i,j,k,iBlock) - &
                     State_VGB(iVar,i,j,k-2,iBlock)) + &
                     cAmrWavefilter * (&
                     abs(State_VGB(iVar,i,j,k+2,iBlock)) + &
                     abs(2.0*State_VGB(iVar,i,j,k,iBlock)) + &
                     abs(State_VGB(iVar,i,j,k-2,iBlock))))
                Crit_D(3) = (Numerator/max(Denominator,cEpsilon))
                Crit = Crit + Crit_D(3)**2
             end if

             ! Crit = (sum(Crit_D(1:nDim)**2))*InvNdim
             Crit = Crit *InvNdim
             if( Crit >  AmrCrit_IB(iCrit,iBlock)) &
                  AmrCrit_IB(iCrit,iBlock) = Crit

          end do ! end nVar
       end do; end do; end do

    end do BLOCK

  end subroutine calc_error_amr_criteria
  !============================================================================
  subroutine apply_unsorted_criteria

    use BATL_size, ONLY: nBlock
    use BATL_tree, ONLY: iStatusNew_A, Refine_, Coarsen_, &
         Unused_B, iNode_B, iNode_B

    integer:: iBlock, iCrit, iVarCrit
    logical:: DoCoarsen
    logical:: DoChangeBlock
    real:: BlockRes ! resolution or level of the block
    real:: ResLimit ! max resolution/level

    logical, parameter:: DoTest = .false.

    character(len=*), parameter:: NameSub = 'apply_unsorted_criteria'
    !--------------------------------------------------------------------------
    BLOCK3:do iBlock = 1, nBlock

       if(Unused_B(iBlock)) CYCLE

       if(DoTest) write(*,*) NameSub, ' iBlock=', iBlock

       ! Block is to be refined already, no need to check
       if(iStatusNew_A(iNode_B(iBlock)) == Refine_) CYCLE

       ! Assume that block can be coarsened
       DoCoarsen = .true.

       ! Simple AMR allows blocks to be coarsened anywhere
       DoChangeBlock = IsSimpleAmr

       ! Check each AMR criteria applicable
       do iCrit = iStartCrit, iEndCrit

          if(DoTest) write(*,*) NameSub, ' iCrit, UseCrit=', &
               iCrit, UseCrit_IB(iCrit,iBlock)

          ! Criteria used in this block
          if(.not.UseCrit_IB(iCrit,iBlock)) CYCLE

          ! Block is evaluated by this criteria so it can change
          DoChangeBlock = .true.

          ! Index of the criteria "variable" used (e.g. density gradient)
          ! For geometric refinement it can also be grid level or dx.
          iVarCrit = iVarCritAll_I(iCrit)

          ! Current resolution or level of the block
          BlockRes = AmrCrit_IB(iResolutionLimit_I(iCrit),iBlock)
          ResLimit = ResolutionLimit_I(iCrit)

          if(DoTest) write(*,*) NameSub, &
               ' iVarCrit, AmrCrit_IB, RefineCrit, BlockRes, ResLimit=', &
               iVarCrit, AmrCrit_IB(iVarCrit,iBlock), BlockRes, ResLimit

          if(AmrCrit_IB(iVarCrit,iBlock) > RefineCritAll_I(iCrit) .and. &
               BlockRes > ResLimit)then

             ! Block should be refined because the AMR criteria value
             ! AmrCrit_IB(iVarCrit,iBlock) exceeds the level set for
             ! refinement in RefineCritAll_I, and the block has a resolution
             ! BlockRes that is larger than the limit ResLimit.
             ! Note that grid level is stored with a negative sign
             ! so this comparison works for both cell size and level.

             iStatusNew_A(iNode_B(iBlock)) = Refine_

             if(DoTest) write(*,*) NameSub, ' refine block'

             CYCLE BLOCK3
          else if(AmrCrit_IB(iVarCrit,iBlock) > CoarsenCritAll_I(iCrit) .and. &
               (BlockRes < 0 .and. BlockRes+1 > ResLimit &
               .or. BlockRes > 0 .and. 2*BlockRes > ResLimit) )then
             ! If any of the AMR criteria AmrCrit_IB(iVarCrit,iBlock)
             ! is above the coarsening limit, and the block would become
             ! coarser than required (for physics based criteria)
             ! the block should not be coarsened
             DoCoarsen = .false.

             if(DoTest)then
                write(*,*) NameSub, ' do not coarsen block, CoarsenCritAll=',&
                     CoarsenCritAll_I(iCrit)
                write(*,*) NameSub, ' iCrit, iResolutionLimit_I   =', &
                     iCrit, iResolutionLimit_I(iCrit)
                write(*,*)NameSub, ' AmrCrit(iVar), CoarsenCritAll=', &
                     AmrCrit_IB(iVarCrit,iBlock), CoarsenCritAll_I(iCrit)
                write(*,*)NameSub, ' BlockRes, ResolutionLimit=', &
                     BlockRes, ResLimit
             end if
          end if

       end do

       if(DoChangeBlock .and. DoCoarsen)then
          iStatusNew_A(iNode_B(iBlock)) =  Coarsen_
          if(DoTest) write(*,*) NameSub, ' coarsen block'
       end if

    end do BLOCK3

  end subroutine apply_unsorted_criteria
  !============================================================================
  subroutine read_amr_criteria(NameCommand, nCritInOut, NameCritOut_I,&
       NameStatVarIn_V, nStateVarIn, DoReadExtraOut)

    use ModReadParam, ONLY: read_var
    use ModUtilities, ONLY: lower_case
    use BATL_tree, ONLY: MaxTotalBlock, iTree_IA, MaxLevel_, MaxLevel
    use BATL_mpi, ONLY: iProc
    use ModUtilities, ONLY: split_string

    character(len=*), intent(in) :: NameCommand
    integer :: iCrit,iCritName,nCrit,iIntCrit,iStatVar

    ! Number and name of criteria to be used by BATSRUS
    integer,           optional, intent(inout) :: nCritInOut
    character(len=20), optional, intent(out)   :: NameCritOut_I(:)
    logical,           optional, intent(out)   :: DoReadExtraOut
    integer,           optional, intent(in)    :: nStateVarIn
    character(len=*),  optional, intent(in)    :: NameStatVarIn_V(:)

    ! Max number of componets in TypeCriteia
    integer, parameter :: nMaxComponents=20

    character(len=nChar) :: NameCrit_I(nMaxComponents)
    integer :: nCritArgs,iCritPhy
    character(len=nChar) :: NameCrit
    character(len=20) :: NameStatVar
    logical :: IsLevel, IsRes

    logical :: IsUniqueCritName, UseErrorCrit, DoReadExtra

    logical, parameter :: DoTest = .false.

    character(len=*), parameter:: NameSub = 'read_amr_criteria'
    !--------------------------------------------------------------------------
    DoReadExtra = .false.
    IsNewPhysParam = .true.

    DoSortAmrCrit = .not. DoStrictAmr
    select case(NameCommand)
    case("#AMRCRITERIA", "#AMRCRITERIALEVEL", "#AMRCRITERIARESOLUTION", &
         "#AMRCRITERIACELLSIZE")

       nCrit       = nCritInOut
       nAmrCrit    = nCrit
       iCritPhy = 0
       ! 'dx' and 'level' will be stored as the last indexes of the array
       ! nCritDxLevel will give number of geometric AMR agruments at the end
       nCritDxLevel  =  0
       if(nCrit == 0) RETURN

       ! deallocate,if they are already allocated
       if(allocated(CoarsenCritPhys_I)) &
            deallocate(CoarsenCritPhys_I, RefineCritPhys_I, &
            iMapToUniqCrit_I)
       if(allocated(LevelMaxCritPhys_I)) &
            deallocate(LevelMaxCritPhys_I,MaxGeoCritPhys_I)

       ! allocate all arrays
       allocate(CoarsenCritPhys_I(nCrit), RefineCritPhys_I(nCrit), &
            iMapToUniqCrit_I(nCrit),LevelMaxCritPhys_I(nCrit),&
            MaxGeoCritPhys_I(nCrit),nAreaPerCritPhys_I(nCrit),&
            NameAreaPhys_II(nMaxComponents,nCrit))

       nAreaPerCritPhys_I = 0
       NameAreaPhys_II = ""

       if(present(nStateVarIn) .and. .not. allocated(iMapToStateVar_I))&
            allocate(iMapToStateVar_I(nCrit))

       ! Can only have #AMRCRITERIALEVEL, #AMRCRITERIARESOLUTION,
       ! or #AMRCRITERIACELLSIZE in one session
       ! We always check for max Level
       MaxGeoCritPhys_I = 2
       IsLevel     = .false.
       IsRes       = .false.
       UseCellSize = .false.
       if(NameCommand == "#AMRCRITERIALEVEL") then
          IsLevel = .true.
       else if(NameCommand == "#AMRCRITERIARESOLUTION") then
          IsRes = .true.
          UseCellSize = .false.
          MaxGeoCritPhys_I = 1
       else if(NameCommand == "#AMRCRITERIACELLSIZE") then
          IsRes = .true.
          UseCellSize = .true.
          MaxGeoCritPhys_I = 1
       end if

       ! Turn off simple AMR behavior when using
       ! #AMRCRITERIALEVELS / #AMRCRITERIARESOLUTION / #AMRCRITERIACELLSIZE
       IsSimpleAmr = .not.( IsLevel .or. IsRes)

       nIntCrit = 1
       nCritInOut = 0
       NameCritOut_I = "NULL"
       iMapToStateVar_I = -777
       UseErrorCrit = .false.

       do iCrit = 1, nCrit

          IsUniqueCritName = .true.
          ! find index of the criteria from its name
          call read_var('TypeCriteria', NameCrit, IsLowerCase=.true.)

          call split_string(NameCrit, nMaxComponents, NameCrit_I, nCritArgs)

          NameCrit = trim(adjustl(NameCrit_I(1)))
          select case(NameCrit)
          case('error')
             iCritPhy = iCritPhy +1
             UseErrorCrit = .true.

             if(.not. present(nStateVarIn) &
                  .and. .not. present(NameStatVarIn_V))&
                  call CON_stop(NameCommand//' ERROR: Need a name table')

             ! find index associated with name => iStatVar
             do iStatVar = 1, nStateVarIn
                NameStatVar = NameStatVarIn_V(iStatVar)
                call lower_case(NameStatVar)
                if(NameStatVar == trim(adjustl(NameCrit_I(2))) ) EXIT
             end do
             ! find if index iStatVar exsit in list before
             do iIntCrit = 1, nIntCrit
                if(iMapToStateVar_I(iIntCrit) == iStatVar) then
                   IsUniqueCritName = .false.
                end if
             end do
             ! if unique it will be added to list
             if(IsUniqueCritName .and. iStatVar <= nStateVarIn) then
                iMapToStateVar_I(nIntCrit) = iStatVar
                iMapToUniqCrit_I(iCritPhy) = -nIntCrit
                nIntCrit = nIntCrit+1
             end if

             call set_crit_area(3,nCritArgs,NameCrit_I,iCritPhy)
          case('transient')
             iCritPhy = iCritPhy +1
             if(.not. present(DoReadExtraOut)) call CON_stop(NameCommand//&
                  ' ERROR: BATSRUS need flag to read more data')
             DoReadExtra = .true.

             ! Find out it the name has been used before
             do iCritName = 1, nCrit
                if(trim(adjustl(NameCrit_I(2)))==NameCritOut_I(iCritName)) then
                   iMapToUniqCrit_I(iCritPhy) = iCritName
                   IsUniqueCritName = .false.
                   EXIT
                end if
             end do

             ! Add it to the list if its unique
             if(IsUniqueCritName) then
                do iCritName = 1, nCritInOut+1
                   if(NameCritOut_I(iCritName) == "NULL" )then
                      iMapToUniqCrit_I(iCritPhy) = iCritName
                      NameCritOut_I(iCritName) = trim(adjustl(NameCrit_I(2)))
                      EXIT
                   end if
                end do
                nCritInOut = iMapToUniqCrit_I(iCritPhy)
             end if
             call set_crit_area(3,nCritArgs,NameCrit_I,iCritPhy)
          case('dx', 'dr', 'dphi')
             ! at this time we do not know the correct index for dx
             iMapToUniqCrit_I(nCrit-nCritDxLevel) = 10001
             call set_crit_area(2,nCritArgs,NameCrit_I,nCrit-nCritDxLevel)
          case('level')
             ! at this time we do not know the correct index for level
             iMapToUniqCrit_I(nCrit-nCritDxLevel) = 10002
             call set_crit_area(2,nCritArgs,NameCrit_I,nCrit-nCritDxLevel)
          case default
             iCritPhy = iCritPhy +1
             if(.not. present(NameCritOut_I) &
                  .and. .not. present(nCritInOut))&
                  call CON_stop(NameCommand//' ERROR: Need a name table')

             ! Find out if the name has been used before
             do iCritName = 1, nCrit
                if(trim(NameCrit) == trim(NameCritOut_I(iCritName))) then
                   iMapToUniqCrit_I(iCritPhy) = iCritName
                   IsUniqueCritName = .false.
                   EXIT
                end if
             end do

             ! Add it to the list if its unique
             if(IsUniqueCritName) then
                do iCritName = 1, nCritInOut+1
                   if(NameCritOut_I(iCritName) == "NULL" )then
                      iMapToUniqCrit_I(iCritPhy) = iCritName
                      NameCritOut_I(iCritName) = NameCrit
                      EXIT
                   end if
                end do
                nCritInOut = iMapToUniqCrit_I(iCritPhy)
             end if
             call set_crit_area(2,nCritArgs,NameCrit_I,iCritPhy)
          end select
          select case(NameCrit)
          case('level')
             call read_var('RefineTo',RefineCritPhys_I(nCrit-nCritDxLevel))
             call read_var('CoarsenTo',CoarsenCritPhys_I(nCrit-nCritDxLevel))
             RefineCritPhys_I(nCrit-nCritDxLevel) = &
                  -RefineCritPhys_I(nCrit-nCritDxLevel)
             CoarsenCritPhys_I(nCrit-nCritDxLevel) = &
                  -(CoarsenCritPhys_I(nCrit-nCritDxLevel)+1)
             LevelMaxCritPhys_I(nCrit-nCritDxLevel) = -MaxLevel
             if(IsRes) LevelMaxCritPhys_I(nCrit-nCritDxLevel) = 0.0
             nCritDxLevel = nCritDxLevel+1
          case('dx', 'dr', 'dphi')
             call read_var('RefineTo',RefineCritPhys_I(nCrit-nCritDxLevel))
             call read_var('CoarsenFrom',CoarsenCritPhys_I(nCrit-nCritDxLevel))
             LevelMaxCritPhys_I(nCrit-nCritDxLevel) = -MaxLevel
             if(IsRes)  LevelMaxCritPhys_I(nCrit-nCritDxLevel) = 0.0
             nCritDxLevel = nCritDxLevel+1
          case default
             call read_var('CoarsenLimit', CoarsenCritPhys_I(iCritPhy))
             call read_var('RefineLimit',  RefineCritPhys_I(iCritPhy))
             if(IsRes)then
                call read_var('MaxResolution', LevelMaxCritPhys_I(iCritPhy))
             elseif(IsLevel)then
                call read_var('MaxLevel', LevelMaxCritPhys_I(iCritPhy))
                ! Level is stored with negative sign
                LevelMaxCritPhys_I(iCritPhy) = -LevelMaxCritPhys_I(iCritPhy)
             end if
          end select
       end do

       if(UseErrorCrit)&
            call read_var('AmrWavefilter',cAmrWavefilter)
       if(DoReadExtra) &
            call read_var('DoReadExtraOut',DoReadExtraOut)

       DoCritAmr     = .true.
       DoAutoAmr     = .true.
       nExtCrit      = nCritInOut
       nIntCrit      = nIntCrit-1
       nAmrCritUsed  = nCrit
       nPhysCritUsed = nCrit

       ! making the error criteia come before the physics criteria
       where(iMapToUniqCrit_I > 10000)
          iMapToUniqCrit_I = iMapToUniqCrit_I-10000 +nCritInOut
       end where
       where(iMapToUniqCrit_I > 0)
          iMapToUniqCrit_I = iMapToUniqCrit_I + nIntCrit
       end where
       iMapToUniqCrit_I = abs(iMapToUniqCrit_I)

       ! Extra stuff for #AMRCRITERIA
       if(.not. (IsLevel .or. IsRes))then
          if(allocated(iTree_IA)) then
             LevelMaxCritPhys_I(1:nAmrCritUsed)= -maxval(iTree_IA(MaxLevel_,:))
          else
             ! if the grid is not initalized at this point, we set it to max
             ! value and let proper nesting take care of it
             LevelMaxCritPhys_I(1:nAmrCritUsed) = -MaxLevel
          end if
       end if

       if(DoTest .and. iProc == 0) then
          write(*,*) " nCritInOut          = ", nCritInOut
          write(*,*) " nIntCrit            = ", nIntCrit
          write(*,*) " nAmrCritUsed        = ", nAmrCritUsed
          write(*,*) " NameCritOut_I       = ", NameCritOut_I
          write(*,*) " nPhysCritUsed       = ", nPhysCritUsed
          write(*,*) " iMapToStateVar_I    = ", iMapToStateVar_I
          write(*,*) " iMapToUniqCrit_I    = ", iMapToUniqCrit_I
          write(*,*) " CoarsenCritPhys_I   = ", CoarsenCritPhys_I
          write(*,*) " RefineCritPhys_I    = ", RefineCritPhys_I
          write(*,*) " nCritDxLevel        = ", nCritDxLevel
          write(*,*) " nAreaPerCritPhys_I  = ", nAreaPerCritPhys_I
          if(IsLevel .or. IsRes) &
               write(*,*) " LevelMaxCritPhys_I  = ", LevelMaxCritPhys_I
          write(*,*) " AmrWavefilter       = ", cAmrWavefilter
       end if
    case("#AMR") ! compatibility with old system with BATSRUS amr options
       call read_var('PercentCoarsen', PercentCoarsen)
       call read_var('PercentRefine',  PercentRefine)
       call read_var('MaxTotalBlock',  MaxTotalBlock)
       DoSortAmrCrit = PercentCoarsen > 0.0 .or. PercentRefine > 0.0
    case("#AMRLIMIT")
       call read_var('PercentCoarsen', PercentCoarsen)
       call read_var('PercentRefine' , PercentRefine)
       call read_var('MaxTotalBlock',  MaxTotalBlock)
       call read_var('DiffCriteriaLevel',  DeltaCriteria)
       DoSortAmrCrit = PercentCoarsen > 0.0 .or. PercentRefine > 0.0
    case default
       call CON_stop(NameSub//'incorrect PARAM.in!')
    end select

    DoStrictAmr = .not. DoSortAmrCrit
  contains
    !==========================================================================
    subroutine set_crit_area(iStartElmt, nElmt, Name_I, iCrit)

      integer, intent(in) :: iStartElmt, nElmt, iCrit
      character(len=nChar), intent(in):: Name_I(nMaxComponents)

      integer:: i, iElmt
      !------------------------------------------------------------------------
      if(iStartElmt <= nElmt) then
         i = 0
         do iElmt = iStartElmt, nElmt
            i = i + 1
            NameAreaPhys_II(i,iCrit) = trim(adjustl(Name_I(iElmt)))
         end do
      else
         i = 0
         ! NameAreaPhys_II(1,iCrit) = "ALL"
      end if
      nAreaPerCritPhys_I(iCrit) = i

    end subroutine set_crit_area
    !==========================================================================
  end subroutine read_amr_criteria
  !============================================================================
  subroutine set_amr_geometry(iBlock)

    integer, intent(in) :: iBlock

    integer ::iCrit, nAreaCrit

    ! logical:: DoTest
    logical, parameter:: DoTest = .false.
    character(len=*), parameter:: NameSub = 'set_amr_geometry'
    !--------------------------------------------------------------------------
    if(nAmrCritUsed < 1) RETURN

    ! DoTest = iBlock == 1

    ! Find if Criteria should be used in block
    call set_block_dx_level(iBlock)
    do iCrit = 1, nAmrCritUsed
       nAreaCrit = nAreaPerCritAll_I(iCrit)

       if(nAreaCrit < 1)then
          UseCrit_IB(iCrit,iBlock) = .true.
       else
          call block_inside_regions( &
               iAreaIdx_II(1:nAreaCrit,iCrit), iBlock, 0, &
               IsInside=UseCrit_IB(iCrit,iBlock))
       end if

       if(DoTest)write(*,*) NameSub,' iCrit, nAreaCrit, UseCrit_IB=', &
            iCrit, nAreaPerCritAll_I(iCrit), UseCrit_IB(iCrit,iBlock),&
            Area_I(abs(iAreaIdx_II(nAreaPerCritAll_I(iCrit),iCrit)))%NameRegion
    end do

  end subroutine set_amr_geometry
  !============================================================================
  logical function is_masked_amr_criteria(iBlock,iCritExtIn)

    integer, intent(in) :: iBlock
    integer, optional, intent(in) :: iCritExtIn
    integer :: iCrit
    !--------------------------------------------------------------------------
    if(present(iCritExtIn)) then
       is_masked_amr_criteria = .false.

       ! loop over physics criteria
       do iCrit = nIntCrit+1, nIntCrit+nPhysCritUsed-nCritDxLevel
          if(iVarCritAll_I(iCrit) == nIntCrit+iCritExtIn) &
               is_masked_amr_criteria = is_masked_amr_criteria &
               .or. UseCrit_IB(iCrit,iBlock)
       end do
       is_masked_amr_criteria = .not. is_masked_amr_criteria
    else
       is_masked_amr_criteria = .not. &
            any(UseCrit_IB(nIntCrit+1:nIntCrit+nPhysCritUsed-nCritDxLevel,&
            iBlock))
    end if

  end function is_masked_amr_criteria
  !============================================================================
  subroutine clean_amr_criteria
    !--------------------------------------------------------------------------
    if(allocated(CoarsenCritAll_I))   deallocate(CoarsenCritAll_I)
    if(allocated(RefineCritAll_I))    deallocate(RefineCritAll_I)
    if(allocated(ResolutionLimit_I))  deallocate(ResolutionLimit_I)
    if(allocated(iVarCritAll_I))      deallocate(iVarCritAll_I)
    if(allocated(UseCrit_IB))         deallocate(UseCrit_IB)
    if(allocated(UseCrit_B))          deallocate(UseCrit_B)
    if(allocated(AmrCrit_IB))         deallocate(AmrCrit_IB)
    if(allocated(LevelMaxCritPhys_I)) deallocate(LevelMaxCritPhys_I)
    if(allocated(iMapToUniqCrit_I))   deallocate(iMapToUniqCrit_I)
    if(allocated(iMapToStateVar_I))   deallocate(iMapToStateVar_I)
    if(allocated(iNode_I))            deallocate(iNode_I)
    if(allocated(nAreaPerCritAll_I))  deallocate(nAreaPerCritAll_I)

    nAmrCrit      = 0
    nAmrCritUsed  = 0
    nExtCrit      = 1
    nIntCrit      = 0
    nCritDxLevel  = 0
    DeltaCriteria = 1.0e-8
    IsNewPhysParam = .true.

    call clean_region

  end subroutine clean_amr_criteria
  !============================================================================
  subroutine set_block_dx_level(iBlock)

    use BATL_grid, ONLY: Xyz_DNB,CellSize_DB
    use BATL_size, ONLY: nINode,nJNode,nKNode,nDim
    use BATL_geometry, ONLY: IsCartesianGrid, Phi_, IsLogRadius, IsGenRadius
    use BATL_tree, ONLY: iNode_B, iTree_IA, Level_
    use ModNumConst, ONLY: cRadToDeg

    integer, intent(in) :: iBlock

    real    :: LengthMax
    integer :: i,j,k
    !--------------------------------------------------------------------------
    if(.not. UseCellSize)then
       if(IsLogRadius .or. IsGenRadius)then
          LengthMax = cRadToDeg*CellSize_DB(Phi_,iBlock)
       else
          LengthMax = CellSize_DB(1,iBlock)
       end if
    elseif(IsCartesianGrid) then
       LengthMax = maxval(CellSize_DB(1:nDim,iBlock))
    else
       ! Find the longest cell edge in the block
       LengthMax = 0.0

       ! Get maximum length squared of i-edges
       do k = 1, nKNode; do j = 1, nJNode; do i = 2, nINode
          LengthMax = max(LengthMax,&
               sum((Xyz_DNB(:,i  ,j,k,iBlock) &
               -    Xyz_DNB(:,i-1,j,k,iBlock))**2))
       end do; end do ; end do

       if(nDim >1) then
          ! Get maximum length squared of j-edges
          do k = 1, nKNode; do j = 2, nJNode; do i = 1, nINode
             LengthMax = max(LengthMax, &
                  sum((Xyz_DNB(:,i,j  ,k,iBlock) &
                  -    Xyz_DNB(:,i,j-1,k,iBlock))**2))
          end do; end do ; end do
       end if

       if(nDim >2) then
          ! Get maximum length squared of k-edges
          do k = 2, nKNode; do j = 1, nJNode; do i = 1, nINode
             LengthMax = max(LengthMax,               &
                  sum((Xyz_DNB(:,i,j,k  ,iBlock)      &
                  -    Xyz_DNB(:,i,j,k-1,iBlock))**2))
          end do; end do ; end do
       end if

       ! Get maximum length
       LengthMax = sqrt(LengthMax)

    end if

    AmrCrit_IB(nAmrCrit-nGeoCrit+1:nAmrCrit,iBlock) = &
         [ LengthMax, -real(iTree_IA(Level_,iNode_B(iBlock))) ]

  end subroutine set_block_dx_level
  !============================================================================
end module BATL_amr_criteria
!==============================================================================
