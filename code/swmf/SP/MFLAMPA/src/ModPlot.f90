!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
module SP_ModPlot

  ! Methods for saving plots

  use ModIoUnit, ONLY: UnitTmp_
  use ModNumConst, ONLY: cDegToRad, cRadToDeg, cTolerance
  use ModPlotFile, ONLY: save_plot_file, read_plot_file
  use ModUtilities, ONLY: open_file, close_file, remove_file, CON_stop
#ifdef _OPENACC
  use ModUtilities, ONLY: norm2
#endif
  use SP_ModAngularSpread, ONLY: get_normalized_spread,  &
       nSpreadLon, nSpreadLat, SpreadLon_I, SpreadLat_I, &
       IsReadySpreadPoint, IsReadySpreadGrid
  use SP_ModChannel, ONLY: FluxChannelInit_V, Flux_VIB,    &
       Flux0_, FluxMax_, NameFluxChannel_I, NameFluxUnit_I
  use SP_ModDistribution, ONLY: KinEnergyIo_G, Momentum_G, Distribution_CB
  use SP_ModGrid, ONLY: nVar, nMHData, nLine, nLineAll, &
       iLineAll0, search_line, MHData_VIB, State_VIB, iShock_IB, &
       nVertex_B, NameVar_V, Shock_, LagrID_, X_, Y_, Z_, R_,    &
       TypeCoordSystem, nP, nMu, IsMuAvg, &
       UseAppendedRead, UseAppendedWrite
  use SP_ModProc, ONLY: iProc
  use SP_ModSize, ONLY: nVertexMax, nDim
  use SP_ModTime, ONLY: SPTime, iIter, StartTime, StartTimeJulian
  use SP_ModUnit, ONLY: NameFluxUnit, NameDiffFluxUnit, &
       NameEnergyUnit, NameVarUnit_V, Si2Io_V, UnitFlux_

  implicit none

  SAVE

  private ! except

  public:: init          ! Initialize module parameters
  public:: read_param    ! Read module parameters
  public:: save_plot_all ! Save output (plot) files
  public:: finalize      ! Save final list

  ! the output directory
  character(len=*), public, parameter :: NamePlotDir = "SP/IO2/"

  ! If true the time tag format is YYYYMMDDHHMMSS
  logical, public:: UseDateTime = .false.

  ! UseAppendedRead and UseAppendedWrite are in ModGrid to avoid
  ! circular dependency between ModPlot and ModReadMhData

  character(len=*), parameter, public :: NameMHData    = "MH_data"
  character(len=*), parameter, public :: NameDistrData = "Distr"
  character(len=*), parameter, public :: NameFluxData  = "Flux"

  ! number of different output file tags
  integer, public :: nTag = 0

  ! ------------ Local variables ------------

  ! Number of plot file types, to be read from param file
  integer:: nFileOut = 0
  ! Types of output files in terms of output dataa
  integer, parameter:: &
                                ! Background mhd data
       MH1D_      = 0, & ! along each line
       MH2D_      = 1, & ! at given radius as Lon-Lat plot
       MHTime_    = 2, & ! at given radius as time series for field lines
                                ! Distribution
       Distr1D_   = 3, & ! along each line
       Distr2D_   = 4, & ! at given radius as Lon-Lat plot
       DistrTime_ = 5, & ! at given radius as time series for field lines
       DistrTraj_ = 6, & ! along the specified spacecraft trajectory
                                ! Flux
       Flux2D_    = 7, & ! at a given radius on rectangular Lon-Lat grid
       FluxTime_  = 8, & ! at a given radius as time series on Lon-Lat grid
       FluxX0_    = 9, & ! on the X=0 plane (longitude = 90 or 270 deg)
       FluxY0_    = 10,& ! on the Y=0 plane (longitude = 0 or 180 deg)
       FluxZ0_    = 11,& ! on the Z=0 plane (latitude = 0 deg)
                                ! Shock Skeleton
       Shock2D_   = 12,& ! shock stencils as Lon-Lat plot at each time step
       ShockTime_ = 13   ! shock stencils saved as time series for field lines
  ! Momentum or energy axis for Distribution plots
  integer, parameter:: &
       Momentum_  = 1, &
       Energy_    = 2
  ! Heliocentric distance or field line length axis for Distribution plots
  integer, parameter:: &
       Distance_  = 1, &
       LineLength_= 2
  ! Plot types for distribution function
  integer, parameter:: &
       CDF_       = 1, &
       DEFIo_     = 2, & ! Distribution*Momentum**2 [#/cm**2/s/sr/energy unit]
       DEFSi_     = 3    ! Distribution*Momentum**2 [#/m**2/s/sr/energy unit]

  type TypePlotFile
     ! Full set of information, for each plot
     !
     ! ------ General information ------
     ! kind of data printed to a file
     ! = MH1D_ or MH2D_ or MHTime_ or Distr1D_ or Flux2D_ or FluxTime_
     integer:: iKindData
     ! file name extension: .dat or .out
     character(len=4 ):: NameFileExtension
     ! file type: tec, tcp, idl, ascii, real8
     character(len=20):: TypeFile
     ! additional info to put into header
     character(len=300):: StringHeaderAux
     ! plot-dependent logical, to store whether it is the first call
     ! for the given plot; USED ONLY IN write_mh_time  FOR NOW!!!
     logical:: IsFirstCall
     ! names of variables to be written
     character(len=500):: NameVarPlot
     ! names of auxilary parameters
     character(len=300):: NameAuxPlot
     ! output buffer: use first a few DIMs until the last one
     real, pointer:: Buffer_II(:,:,:)
     !
     ! ------ MH data ------
     ! variables from the state vector to be written
     logical:: DoPlot_V(X_:nVar)
     ! whether fluxes are to be written
     logical:: DoPlotFlux
     ! total numbers of variables to be written
     integer:: nMhdVar, nExtraVar, nFluxVar
     ! their indices in the state vectors
     integer, pointer:: iVarMhd_V(:), iVarExtra_V(:)
     !
     ! ------ Distribution ------
     ! Momentum or energy axis for distribution plots
     integer:: iScale     ! = Momentum_ or Energy_
     ! Heliocentric distance or field line length for distribution plots
     integer:: iDistance  ! = Distance_ or LineLength_
     ! type out output (CDF or differential energy flow)
     integer:: iTypeDistr ! = CDF_, DEFIo_, or DEFSi_
     ! Data on the sphere
     ! radius of the sphere the data to be written at
     real:: Radius
     ! whether to compute and ouput average position and angular spread
     logical:: DoPlotSpread
     !
     ! ------ Flux through sphere ------
     ! angular coords of point where output is requested
     real    :: Lon, Lat
     ! spread of flux of an individual line over grid
     real, pointer:: Spread_II(:,:)
     integer :: iRange_I(4)      ! iLonMin, iLonMax, iLatMin, iLatMax
     ! used in the plance cut:
     ! RMin/MaxCut is needed for x/y/z=0 cut
     ! LatMinCut, LatMaxCut is needed for x/y=0 cut
     real    :: RMinCut, RMaxCut, LatMinCut, LatMaxCut
     integer :: nRangeCut_I(2) ! {nR, nLon} for z=0 or {nR, nLat} for x/y=0
  end type TypePlotFile
  ! Indexes in iRange_I
  integer, parameter :: iLonMin_ = 1, iLonMax_ = 2, iLatMin_ = 3, iLatMax_ = 4
  ! All plot files
  type(TypePlotFile), allocatable :: File_I(:)

  ! Arrays used to visualize the distribution function
  real :: Log10Momentum_G(0:nP+1), Log10KinEnergyIo_G(0:nP+1)
  real :: Log10Si2IoFlux

  ! auxilary array, used to write data on a sphere
  ! contains integers 1:nLineAll
  integer, allocatable :: iNodeIndex_I(:)

  ! info for MH1D header and tag list
  logical :: DoWriteHeader = .false.
  ! whether this is the initial call for MH_data energy channel list
  logical :: DoInitEChannel = .true.
  ! name of the header file
  character(len=*), parameter :: NameHeaderFile = NameMHData//'.H'
  ! name of the tag list file
  character(len=*), parameter :: NameTagFile  = NameMHData//'.lst'
  ! name of the energy channel in MH_data file(s)
  character(len=*), parameter :: NameEChannelFile = NameMHData//'_EChannel.H'

  integer :: nOutput = 1, iTimeOutput = 0
  real    :: DtOutput = -1.0
  public  :: DtOutput, iTimeOutput
  character(len=20) :: TypeMHDataFile

  ! If DoSaveInitial=.false.,the initial files are not saved
  logical :: DoSaveInitial = .true.
  logical :: DoInit        = .false.

contains
  !============================================================================
  subroutine read_param(NameCommand)

    use ModUtilities, ONLY: split_string, lower_case
    use ModReadParam, ONLY: read_var
    use SP_ModTime, ONLY: IsSteadyState
    use SP_ModConnectivity, ONLY: read_connectivity_param => read_param
    character(len=*), intent(in):: NameCommand
    ! set parameters of output files: file format, kind of output etc.
    character(len=300):: StringPlot
    ! loop variables
    integer:: iFile, iLineAll, iVar
    integer:: nStringPlot
    character(len=20):: TypeFile, KindData, StringPlot_I(2*nVar)
    logical:: IsDEFSi = .false.

    character(len=*), parameter:: NameSub = 'read_param'
    !--------------------------------------------------------------------------
    select case(NameCommand)
    case("#SAVEPLOT")
       ! initialize auxilary arrays, used to write data on a sphere
       ! contains integers 1:nLineAll
       if(.not. allocated(iNodeIndex_I)) allocate(iNodeIndex_I(nLineAll))
       do iLineAll = 1, nLineAll
          iNodeIndex_I(iLineAll) = iLineAll
       end do

       ! number of output files
       call read_var('nFileOut', nFileOut)
       ! check correctness
       if(nFileOut == 0) RETURN ! no output file requested
       if(nFileOut < 0) call CON_stop(NameSub//': incorrect SAVEPLOT section')
       DoInit = .true.
       if(allocated(File_I))&
            call CON_stop(NameSub//'Only a single #SAVEPLOT is allowed')
       ! allocate the storage for file info
       allocate(File_I(nFileOut))
       do iFile = 1, nFileOut
          File_I(iFile)%iRange_I = 0
       end do

       ! read info about each file
       do iFile = 1, nFileOut
          ! reset and read the file info
          StringPlot = ''
          call read_var('StringPlot', StringPlot)

          ! identify DEF before lower_case(StringPlot)
          if(index(StringPlot, 'DEF')>0 .and. (index(StringPlot, 'distr1d')>0 &
               .or. index(StringPlot, 'distraj')>0)) IsDEFSi = .true.
          ! make comparison case insensitive: convert strings to lower case
          call lower_case(StringPlot)

          ! put individual variables and format names in separate array entries
          call split_string(StringPlot, StringPlot_I, nStringPlot)

          ! data kind is the first entry
          KindData = StringPlot_I(1)
          ! check whether set properly
          select case(KindData)
          case('mh1d')
             File_I(iFile) % iKindData = MH1D_
             call check_range(iFile)
          case('mh2d')
             File_I(iFile) % iKindData = MH2D_
          case('mhtime')
             if(IsSteadyState)call CON_stop(NameSub//&
                  ": mhtime kind of data isn't allowed in steady-state")
             File_I(iFile) % iKindData = MHTime_
             call check_range(iFile)
          case('distr1d')
             File_I(iFile) % iKindData = Distr1D_
             ! Check range
             call check_range(iFile)
          case('distr2d')
             File_I(iFile) % iKindData = Distr2D_
          case('distrtime')
             File_I(iFile) % iKindData = DistrTime_
             call check_range(iFile)
          case('distrtraj')
             File_I(iFile) % iKindData = DistrTraj_
          case('flux2d')
             File_I(iFile) % iKindData = Flux2D_
          case('fluxtime')
             if(IsSteadyState)call CON_stop(NameSub//&
                  ": mhtime kind of data isn't allowed in steady-state")
             File_I(iFile) % iKindData = FluxTime_
          case('fluxx0')
             File_I(iFile) % iKindData = FluxX0_
          case('fluxy0')
             File_I(iFile) % iKindData = FluxY0_
          case('fluxz0')
             File_I(iFile) % iKindData = FluxZ0_
          case('shock2d')
             File_I(iFile) % iKindData = Shock2D_
          case('shocktime')
             File_I(iFile) % iKindData = ShockTime_
          case default
             call CON_stop(NameSub//&
                  ": kind of data isn't properly set in PARAM.in")
          end select

          ! format of output is the last entry
          TypeFile = StringPlot_I(nStringPlot)
          ! check whether set properly
          select case(TypeFile)
          case('tec', 'tcp')
             File_I(iFile) % NameFileExtension='.dat'
             File_I(iFile) % TypeFile  ='tec'
          case('idl', 'ascii')
             File_I(iFile) % NameFileExtension='.out'
             File_I(iFile) % TypeFile  ='ascii'
          case('real4', 'real8')
             File_I(iFile) % NameFileExtension='.out'
             File_I(iFile) % TypeFile  = TypeFile
          case default
             call CON_stop(NameSub//&
                  ": output format isn't properly set in PARAM.in")
          end select

          ! reset variables' and parameters' names
          File_I(iFile) % NameVarPlot = ''
          File_I(iFile) % NameAuxPlot = ''
          select case(File_I(iFile) % TypeFile)
          case('tec', 'tcp')
             File_I(iFile) % StringHeaderAux = ''
          case default
             File_I(iFile) % StringHeaderAux = 'Units:'
          end select

          ! based on kind of data process the requested output
          select case(File_I(iFile) % iKindData)
          case(MH1D_)
             call process_mh
             ! add particle index to variable names
             File_I(iFile) % NameVarPlot = &
                  NameVar_V(LagrID_)//' '//&
                  trim(File_I(iFile) % NameVarPlot)
             do iVar = LagrID_,Z_
                File_I(iFile)%NameAuxPlot = &
                     trim(File_I(iFile)%NameAuxPlot)//&
                     ' '//NameVar_V(iVar)
             end do
             File_I(iFile) % NameAuxPlot = &
                  trim(File_I(iFile) % NameAuxPlot)//&
                  ' iShock RShock StartTime StartTimeJulian'
             TypeMHDataFile = File_I(iFile) % TypeFile
             DoWriteHeader = .true.
          case(MH2D_)
             call process_mh
             ! add line index, lon and lat to variable names
             select case(File_I(iFile) % TypeFile)
             case('tec', 'tcp')
                File_I(iFile) % NameVarPlot = &
                     'LineIndex '//trim(File_I(iFile) % NameVarPlot)// &
                     ' Longitude_[deg] Latitude_[deg]'
             case default
                File_I(iFile) % NameVarPlot = &
                     'LineIndex '//trim(File_I(iFile) % NameVarPlot)// &
                     ' Longitude Latitude'
                File_I(iFile) % StringHeaderAux = &
                     trim(File_I(iFile)%StringHeaderAux)//' deg deg'
             end select
             File_I(iFile) % NameAuxPlot = &
                  trim(File_I(iFile) % NameAuxPlot) // &
                  ' StartTime StartTimeJulian'
             if(File_I(iFile) % DoPlotSpread) &
                  File_I(iFile) % NameAuxPlot = &
                  trim(File_I(iFile) % NameAuxPlot) // &
                  ' LonAverage LatAverage AngleSpread'
             ! get radius
             call read_var('Radius', File_I(iFile) % Radius)
          case(MHTime_)
             call process_mh
             ! add time interval index to variable names
             File_I(iFile) % NameVarPlot = &
                  'Time '//trim(File_I(iFile) % NameVarPlot)
             File_I(iFile) % NameAuxPlot = &
                  trim(File_I(iFile) % NameAuxPlot) // &
                  ' StartTime StartTimeJulian'
             ! get radius
             call read_var('Radius', File_I(iFile) % Radius)
             ! reset indicator of the first call
             File_I(iFile) % IsFirstCall = .true.
          case(Distr1D_)
             call process_distr
          case(Distr2D_)
             call process_distr
             ! get radius
             call read_var('Radius', File_I(iFile) % Radius)
          case(DistrTime_)
             call process_distr
             ! get radius
             call read_var('Radius', File_I(iFile) % Radius)
             ! reset indicator of the first call
             File_I(iFile) % IsFirstCall = .true.
          case(DistrTraj_)
             call process_distr
          case(Flux2D_)
             ! mark flux to be written
             File_I(iFile) % DoPlotFlux = .true.
             ! add longitude and latitude with units to variable names
             select case(File_I(iFile) % TypeFile)
             case('tec', 'tcp')
                File_I(iFile) % NameVarPlot = 'Longitude_[deg] Latitude_[deg]'
             case default
                File_I(iFile) % NameVarPlot = 'Longitude Latitude '
                File_I(iFile) % StringHeaderAux = &
                     trim(File_I(iFile)%StringHeaderAux)//' deg deg'
             end select
             File_I(iFile) % NameAuxPlot = &
                  trim(File_I(iFile) % NameAuxPlot) // &
                  ' StartTime StartTimeJulian'
             call read_var('Radius', File_I(iFile) % Radius)
             File_I(iFile)%iRange_I = 1
             call read_var('nLonFlux2d', File_I(iFile)%iRange_I(2))
             call read_var('nLatFlux2d', File_I(iFile)%iRange_I(4))
          case(FluxTime_)
             ! mark flux to be written
             File_I(iFile) % DoPlotFlux = .true.
             ! add time interval index to variable names
             File_I(iFile) % NameVarPlot = 'Time '
             File_I(iFile) % NameAuxPlot = &
                  ' StartTime StartTimeJulian Longitude Latitude'
             ! get radius
             call read_var('Radius', File_I(iFile) % Radius)
             ! get longitude and latitude of location being tracked
             call read_var('Longitude', File_I(iFile) % Lon)
             File_I(iFile) % Lon = File_I(iFile) % Lon * cDegToRad
             call read_var('Latitude', File_I(iFile) % Lat)
             File_I(iFile) % Lat = File_I(iFile) % Lat * cDegToRad
             ! reset indicator of the first call
             File_I(iFile) % IsFirstCall = .true.
          case(FluxX0_)
             call process_flux0(X_)
             ! read in parameters
             call read_var('RMin', File_I(iFile) % RMinCut)
             call read_var('RMax', File_I(iFile) % RMaxCut)
             call read_var('nRFlux2d', File_I(iFile) % nRangeCut_I(1))
             call read_var('LatMin', File_I(iFile) % LatMinCut)
             call read_var('LatMax', File_I(iFile) % LatMaxCut)
             call read_var('nLatFlux2d', File_I(iFile) % nRangeCut_I(2))
          case(FluxY0_)
             call process_flux0(Y_)
             ! read in parameters
             call read_var('RMin', File_I(iFile) % RMinCut)
             call read_var('RMax', File_I(iFile) % RMaxCut)
             call read_var('nRFlux2d', File_I(iFile) % nRangeCut_I(1))
             call read_var('LatMin', File_I(iFile) % LatMinCut)
             call read_var('LatMax', File_I(iFile) % LatMaxCut)
             call read_var('nLatFlux2d', File_I(iFile) % nRangeCut_I(2))
          case(FluxZ0_)
             call process_flux0(Z_)
             ! read in parameters
             call read_var('RMin', File_I(iFile) % RMinCut)
             call read_var('RMax', File_I(iFile) % RMaxCut)
             call read_var('nRFlux2d', File_I(iFile) % nRangeCut_I(1))
             call read_var('nLonFlux2d', File_I(iFile) % nRangeCut_I(2))
          case(Shock2D_)
             call process_shock
             ! add line index to variable names
             File_I(iFile) % NameVarPlot = &
                  'LineIndex '//trim(File_I(iFile) % NameVarPlot)
             File_I(iFile) % NameAuxPlot = &
                  trim(File_I(iFile) % NameAuxPlot) // &
                  ' StartTime StartTimeJulian'
          case(ShockTime_)
             call process_shock
             ! add time interval index to variable names
             File_I(iFile) % NameVarPlot = &
                  'Time '//trim(File_I(iFile) % NameVarPlot)
             File_I(iFile) % NameAuxPlot = &
                  trim(File_I(iFile) % NameAuxPlot) // &
                  ' StartTime StartTimeJulian'
             ! reset indicator of the first call
             File_I(iFile) % IsFirstCall = .true.
          end select
       end do ! iFile
       ! Check consistency: only 1 MH1D file can be requested
       if(count(File_I(1:nFileOut) % iKindData == MH1D_,1) > 1) &
            call CON_stop(NameSub//&
            ": only one MH1D output file can be requested")
    case("#NOUTPUT")
       call read_var('nOutput', nOutput)
       if(nOutput < 1)then
          call read_var('DtOutput', DtOutput)
       end if
    case('#USEDATETIME')
       call read_var('UseDateTime', UseDateTime)
    case('#APPENDMHDATA')
       call read_var('UseAppendedRead', UseAppendedRead)
       call read_var('UseAppendedWrite', UseAppendedWrite)
    case('#SAVEINITIAL')
       call read_var('DoSaveInitial', DoSaveInitial)
    case('#NTAG')
       call read_var('nTag', nTag)
    case default
       call CON_stop('Unknown command '//NameCommand//' in '//NameSub)
    end select

  contains
    !==========================================================================
    subroutine check_range(iFile)
      integer, intent(in) :: iFile
      !------------------------------------------------------------------------
      if(trim(StringPlot_I(2))=='range') then
         call read_var('iLonMin',File_I(iFile)%iRange_I(1))
         call read_var('iLonMax',File_I(iFile)%iRange_I(2))
         call read_var('iLatMin',File_I(iFile)%iRange_I(3))
         call read_var('iLatMax',File_I(iFile)%iRange_I(4))
      elseif(trim(StringPlot_I(2))=='single') then
         call read_var('iLon',File_I(iFile)%iRange_I(1))
         call read_var('iLat',File_I(iFile)%iRange_I(3))
         File_I(iFile)%iRange_I(2:4:2) = File_I(iFile)%iRange_I(1:3:2)
      else
         RETURN
      end if
      ! Remove 'range' or 'single' from StringPlot
      StringPlot_I(1:nStringPlot-1) = &
           StringPlot_I(2:nStringPlot)
      nStringPlot = nStringPlot
    end subroutine check_range
    !==========================================================================
    subroutine process_mh

      ! process variables to plot
      ! NOTE: for iKindData == MH1D_ certain variables are always printed:
      !       Rho_, T_, Ux_:Uz_, Bx_:Bz_, Wave1_, Wave2_
      integer:: iVar, iVarMhd, iVarExtra, iStringPlot
      character(len=10) :: NameVarLowerCase

      ! reset
      !------------------------------------------------------------------------
      File_I(iFile) % DoPlot_V   = .false.
      File_I(iFile) % DoPlotFlux = .false.
      File_I(iFile)%DoPlotSpread = .false.

      ! for MH1D_ minimal set of variables is printed
      if(File_I(iFile) % iKindData == MH1D_)then
         ! for MH1D_ minimal set of variables is printed
         File_I(iFile) % DoPlot_V(1:nMHData) = .true.
      else
         ! coordinates are always printed
         File_I(iFile) % DoPlot_V(X_:Z_) = .true.
      end if

      ! determine, which variables were requested to be in the output file
      ! skip first and last
      do iStringPlot = 2, nStringPlot - 1
         ! if the string is flux, then save ALL the fluxes
         if(trim(StringPlot_I(iStringPlot)) == 'flux') then
            File_I(iFile)%DoPlotFlux = .true.
            CYCLE
         end if

         ! check names of individual variables
         do iVar = 1, nVar
            NameVarLowerCase = NameVar_V(iVar)
            call lower_case(NameVarLowerCase)
            if(StringPlot_I(iStringPlot) == NameVarLowerCase)&
                 File_I(iFile)%DoPlot_V(iVar) = .true.
            CYCLE
         end do

         ! for data on a sphere compute and output average position and spread
         if(trim(StringPlot_I(iStringPlot)) == 'spread' .and.  &
              File_I(iFile)%iKindData == MH2D_ .and. nLineAll > 1)then
            File_I(iFile)%DoPlotSpread = .true.
            CYCLE
         end if
      end do ! iStringPlot

      File_I(iFile) % nMhdVar   = count(File_I(iFile)%DoPlot_V(1:nMhData))
      File_I(iFile) % nExtraVar = count(File_I(iFile)%DoPlot_V(1+nMhData:nVar))

      ! indices in the state vectors
      if(File_I(iFile)%nMhdVar>0) allocate(File_I(iFile)%iVarMhd_V( &
           File_I(iFile)%nMhdVar))
      if(File_I(iFile)%nExtraVar>0) allocate(File_I(iFile)%iVarExtra_V( &
           File_I(iFile)%nExtraVar))
      ! determine indices and names of variables
      iVarMhd = 0; iVarExtra = 0
      do iVar = 1, nVar
         if(.not.File_I(iFile)%DoPlot_V(iVar)) CYCLE
         File_I(iFile)%NameVarPlot = &
              trim(File_I(iFile)%NameVarPlot)//' '//&
              trim(NameVar_V(iVar))
         select case(File_I(iFile)%TypeFile)
         case('tec', 'tcp')
            File_I(iFile)%NameVarPlot = &
                 trim(File_I(iFile)%NameVarPlot)//'_['//&
                 trim(NameVarUnit_V(iVar))//']'
         case default
            File_I(iFile)%StringHeaderAux = &
                 trim(File_I(iFile)%StringHeaderAux)//&
                 ' '//trim(NameVarUnit_V(iVar))
         end select
         if(iVar > nMhData)then
            iVarExtra = iVarExtra + 1
            File_I(iFile)%iVarExtra_V(iVarExtra) = iVar
         else
            iVarMhd = iVarMhd + 1
            File_I(iFile)%iVarMhd_V(iVarMhd) = iVar
         end if
      end do ! iVar

    end subroutine process_mh
    !==========================================================================
    subroutine process_distr

      ! process output parameters for distribution output
      integer:: iStringPlot
      character(len=20):: NameScale, NameDistance, NameVar

      ! only 1 variable (at most) is printed

      !------------------------------------------------------------------------
      File_I(iFile) % nMhdVar = 0
      if(IsMuAvg) then
         ! Do not save Mu
         File_I(iFile) % nExtraVar = 1
      else
         ! Do save Mu
         File_I(iFile) % nExtraVar = 2
      end if
      ! reset
      File_I(iFile) % DoPlot_V   = .false.
      File_I(iFile) % DoPlotFlux = .false.
      File_I(iFile)%DoPlotSpread = .false.

      ! reset string with variables' names and put defaults
      NameScale    = 'Log10Momentum'
      NameDistance = 'Distance'
      NameVar      = 'Log10DiffEnergyFlux'
      File_I(iFile) % iScale     = Momentum_
      File_I(iFile) % iTypeDistr = DEFIo_
      do iStringPlot = 2, nStringPlot - 1
         ! may contain type of output scale (momentum/energy)
         ! and output flux (canonical distr func/differential energy flux)
         select case(StringPlot_I(iStringPlot))
         case('momentum')
            File_I(iFile) % iScale = Momentum_
            NameScale = 'Log10Momentum'
         case('energy')
            File_I(iFile) % iScale = Energy_
            NameScale = 'Log10Energy'
         case('distance')
            File_I(iFile) % iDistance = Distance_
            NameDistance = 'Distance'
         case('length', 'linelength')
            File_I(iFile) % iDistance = LineLength_
            NameDistance = 'FieldLineLength'
         case('cdf')
            ! canonical distribution function
            File_I(iFile) % iTypeDistr = CDF_
            NameVar = 'Log10Distribution'
         case('def')
            if(IsDEFSi) then
               ! differential flux, in the unit of [#/m**2/s/sr/energy unit]
               File_I(iFile) % iTypeDistr = DEFSi_
            else
               ! differential flux, in the unit of [#/cm**2/s/sr/energy unit]
               File_I(iFile) % iTypeDistr = DEFIo_
            end if
            NameVar = 'Log10DiffEnergyFlux'
         end select
      end do

      ! form the name with variables' names
      ! first index: energy/momentum
      File_I(iFile) % NameVarPlot = trim(NameScale)
      ! second (possible) index: mu = cos(pitch angle)
      if(.not. IsMuAvg) &
           File_I(iFile) % NameVarPlot = &
           trim(File_I(iFile) % NameVarPlot) // ' Mu '
      ! third and last index: depends on the format
      if(File_I(iFile) % iKindData == Distr1D_) then
         File_I(iFile) % NameVarPlot = &
              trim(File_I(iFile) % NameVarPlot) // ' ' // &
              trim(NameDistance) // ' ' // trim(NameVar)
      elseif(File_I(iFile) % iKindData == Distr2D_) then
         File_I(iFile) % NameVarPlot = &
              trim(File_I(iFile) % NameVarPlot) &
              // ' LineIndex ' // trim(NameVar)
      elseif(File_I(iFile) % iKindData == DistrTime_) then
         File_I(iFile) % NameVarPlot = &
              trim(File_I(iFile) % NameVarPlot) // ' Time ' // trim(NameVar)
      else
         File_I(iFile) % NameVarPlot = &
              trim(File_I(iFile) % NameVarPlot) // ' ' // trim(NameVar)
      end if

      ! header 1: [Momentum/Energy unit]
      select case(File_I(iFile) % iScale)
      case(Momentum_)
         File_I(iFile) % StringHeaderAux = &
              trim(File_I(iFile) % StringHeaderAux) // &
              ' log10[(p_{INJ}/[Si])*kg*m/s]'
      case(Energy_)
         File_I(iFile) % StringHeaderAux = &
              trim(File_I(iFile) % StringHeaderAux) // &
              ' log10[' // trim(NameEnergyUnit) // ']'
      end select

      ! header 2: cosine pitch angle unit, if saving all mu's values
      if(.not. IsMuAvg) &
           File_I(iFile) % StringHeaderAux = &
           trim(File_I(iFile) % StringHeaderAux) // ' [1]'

      ! header 3: Rsun for both S_ and D_ when saving Distr1D
      ! LagrID when saving Distr2D, or no/skip when saving DisTraj
      if(File_I(iFile) % iKindData == Distr1D_) then
         File_I(iFile) % StringHeaderAux = &
              trim(File_I(iFile) % StringHeaderAux) // &
              ' ' // NameVarUnit_V(R_)
      elseif(File_I(iFile) % iKindData == Distr2D_) then
         File_I(iFile) % StringHeaderAux = &
              trim(File_I(iFile) % StringHeaderAux) // ' LineIndex'
      elseif(File_I(iFile) % iKindData == DistrTime_) then
         File_I(iFile) % StringHeaderAux = &
              trim(File_I(iFile) % StringHeaderAux) // ' sec'
      end if

      ! header 4: [CDF or def/DEF unit]
      select case(File_I(iFile) % iTypeDistr)
      case(CDF_)
         File_I(iFile) % StringHeaderAux = &
              trim(File_I(iFile) % StringHeaderAux) // &
              ' log10[#/m**2/s/sr/' // trim(NameEnergyUnit) &
              // '*(p_{INJ}/(kg*m/s))**2]'
      case(DEFIo_)
         File_I(iFile) % StringHeaderAux = &
              trim(File_I(iFile) % StringHeaderAux) // &
              ' log10[' // trim(NameDiffFluxUnit) // ']'
      case(DEFSi_)
         File_I(iFile) % StringHeaderAux = &
              trim(File_I(iFile) % StringHeaderAux) // &
              ' log10[#/m**2/s/sr/' // trim(NameEnergyUnit) // ']'
      end select

    end subroutine process_distr
    !==========================================================================
    subroutine process_flux0(iVarPlane)
      integer, intent(in) :: iVarPlane
      integer :: iCoor
      !------------------------------------------------------------------------
      ! reset
      File_I(iFile) % DoPlotSpread = .false.
      ! mark flux to be written
      File_I(iFile) % DoPlotFlux   = .true.
      ! together with the X and Y coordinates
      File_I(iFile) % DoPlot_V     = .false.
      File_I(iFile) % DoPlot_V(X_) = .true.
      File_I(iFile) % DoPlot_V(Y_) = .true.
      File_I(iFile) % nMhdVar = count(File_I(iFile)%DoPlot_V(1:nMhData))

      select case(File_I(iFile) % TypeFile)
      case('tec', 'tcp')
         if(iVarPlane == Z_) then
            ! Z=0 plane: ij = Longitude, R
            File_I(iFile) % NameVarPlot = trim(File_I(iFile) % NameVarPlot)// &
                 'Longitude_[deg] '//trim(NameVar_V(R_))// &
                 '_['//trim(NameVarUnit_V(R_))//']'
         else
            ! X/Y=0 plane: ij = Latitude, R
            ! also add lon(= 90/270 or 0/180 deg) to variable names
            File_I(iFile) % NameVarPlot = trim(File_I(iFile) % NameVarPlot)// &
                 'Latitude_[deg] '//trim(NameVar_V(R_))// &
                 '_['//trim(NameVarUnit_V(R_))//']'//' Longitude_[deg]'
         end if
         do iVar = X_, Z_
            if(iVar == iVarPlane) CYCLE
            File_I(iFile) % NameVarPlot = &
                 trim(File_I(iFile) % NameVarPlot)//' '// &
                 trim(NameVar_V(iVar))//'_['//trim(NameVarUnit_V(iVar))//']'
         end do
      case default
         if(iVarPlane == Z_) then
            ! Z=0 plane: ij = Longitude, R
            File_I(iFile) % NameVarPlot = trim(File_I(iFile) % NameVarPlot)// &
                 'Longitude '//trim(NameVar_V(R_))
            File_I(iFile) % StringHeaderAux = &
                 trim(File_I(iFile) % StringHeaderAux)// &
                 ' deg '//trim(NameVarUnit_V(R_))
         else
            ! X/Y=0 plane: ij = Latitude, R
            ! also add lon(= 90/270 or 0/180 deg) to variable names
            File_I(iFile) % NameVarPlot = trim(File_I(iFile) % NameVarPlot)// &
                 'Latitude '//trim(NameVar_V(R_))//' Longitude'
            File_I(iFile) % StringHeaderAux = &
                 trim(File_I(iFile) % StringHeaderAux)// &
                 ' deg '//trim(NameVarUnit_V(R_))//' deg'
         end if
         do iVar = X_, Z_
            if(iVar == iVarPlane) CYCLE
            File_I(iFile) % NameVarPlot = &
                 trim(File_I(iFile) % NameVarPlot)//' '//trim(NameVar_V(iVar))
            File_I(iFile) % StringHeaderAux = &
                 trim(File_I(iFile) % StringHeaderAux) &
                 //' '//trim(NameVarUnit_V(iVar))
         end do
      end select

      ! additional parameters about the time
      File_I(iFile) % NameAuxPlot = trim(File_I(iFile) % NameAuxPlot) // &
           ' StartTime StartTimeJulian'

      ! include coord1 and coord2 among x/y/z except for iVarPlane
      allocate(File_I(iFile)%iVarMhd_V(File_I(iFile)%nMhdVar))
      iCoor = 1
      do iVar = X_, Z_
         if(iVar == iVarPlane) CYCLE
         File_I(iFile)%iVarMhd_V(iCoor) = iVar
         iCoor = iCoor + 1
      end do

    end subroutine process_flux0
    !==========================================================================
    subroutine process_shock

      ! process output parameters for shock skeleton
      use SP_ModShock, ONLY: ShockID_, CompRatio_, nShockVar, &
           DoSaveStateShock, NameVarShock_V, NameVarShockUnit_V
      ! loop variables for variables
      integer :: iVarExtra
      ! only location and physical variables at the shock surface are printed
      !------------------------------------------------------------------------
      File_I(iFile) % nMhdVar = 0
      File_I(iFile) % nExtraVar = nShockVar

      ! indices in the extra state vectors
      if(File_I(iFile)%nMhdVar>0) allocate(File_I(iFile)%iVarMhd_V( &
           File_I(iFile)%nMhdVar))
      if(File_I(iFile)%nExtraVar>0) allocate(File_I(iFile)%iVarExtra_V( &
           File_I(iFile)%nExtraVar))

      ! reset
      File_I(iFile) % DoPlot_V   = .false.
      File_I(iFile) % DoPlotFlux = .false.
      File_I(iFile)%DoPlotSpread = .false.
      ! we do save the states for shock
      DoSaveStateShock = .true.

      ! Save ShockID_, X_:Z_, RLonLat_, and CompRatio_ for shock
      do iVarExtra = ShockID_, CompRatio_
         File_I(iFile)%NameVarPlot = trim(File_I(iFile)%NameVarPlot)//&
              " "//trim(NameVarShock_V(iVarExtra))
         select case(File_I(iFile)%TypeFile)
         case('tec', 'tcp')
            File_I(iFile)%NameVarPlot = &
                 trim(File_I(iFile)%NameVarPlot)//'_['//&
                 trim(NameVarShockUnit_V(iVarExtra))//']'
         case default
            File_I(iFile)%StringHeaderAux = &
                 trim(File_I(iFile)%StringHeaderAux)//&
                 ' '//trim(NameVarShockUnit_V(iVarExtra))
         end select
         File_I(iFile)%iVarExtra_V(iVarExtra-ShockID_+1) = iVarExtra-ShockID_+1
      end do

    end subroutine process_shock
    !==========================================================================
  end subroutine read_param
  !============================================================================
  subroutine init

    use SP_ModGrid, ONLY: nLon, nLat
    ! initialize the file matrix
    ! storage for existing tags (possible during restart
    character(len=50),allocatable:: StringTag_I(:)
    ! full tag file name
    character(len=100)::NameFile
    ! loop variable
    integer:: iTag, iFile, iVar

    character(len=*), parameter:: NameSub = 'init'
    !--------------------------------------------------------------------------
    if(.not.DoInit) RETURN
    DoInit = .false.
    ! Array for plotting distribution function
    Log10Momentum_G    = log10(Momentum_G)
    Log10KinEnergyIo_G = log10(KinEnergyIo_G)
    Log10Si2IoFlux     = log10(Si2Io_V(UnitFlux_))

    ! Finalize setting output files:
    ! number and names of flux channels are known at this point;
    ! also, allocate buffers for output data
    do iFile = 1, nFileOut
       File_I(iFile) % nFluxVar = 0
       if(File_I(iFile) % DoPlotFlux)then
          File_I(iFile) % nFluxVar = FluxMax_ - Flux0_ + 1
          do iVar = Flux0_, FluxMax_
             File_I(iFile) % NameVarPlot = &
                  trim(File_I(iFile) % NameVarPlot)//' '//&
                  trim(NameFluxChannel_I(iVar))
             select case(File_I(iFile) % TypeFile)
             case('tec', 'tcp')
                File_I(iFile) % NameVarPlot = &
                     trim(File_I(iFile) % NameVarPlot)//'_['//&
                     trim(NameFluxUnit_I(iVar))//']'
             case default
                File_I(iFile) % StringHeaderAux = &
                     trim(File_I(iFile) % StringHeaderAux)//&
                     ' '//trim(NameFluxUnit_I(iVar))
             end select
          end do
       end if
       ! prepare the output data containers
       select case(File_I(iFile) % iKindData)
       case(MH1D_)
          allocate(File_I(iFile) % Buffer_II( &
               File_I(iFile)%nMhdVar + File_I(iFile)%nExtraVar + &
               File_I(iFile)%nFluxVar, 1:nVertexMax, 1))
          if(all(File_I(iFile)%iRange_I==0))&
               File_I(iFile)%iRange_I = [1, nLon, 1, nLat]
       case(MH2D_)
          ! extra space is reserved for longitude and latitude
          allocate(File_I(iFile) % Buffer_II( &
               2 + File_I(iFile)%nMhdVar + File_I(iFile)%nExtraVar + &
               File_I(iFile)%nFluxVar, 1:nLineAll, 1))
       case(MHTime_)
          ! note extra space reserved for time of the output
          allocate(File_I(iFile) % Buffer_II( &
               1 + File_I(iFile)%nMhdVar + File_I(iFile)%nExtraVar + &
               File_I(iFile)%nFluxVar, 1, 1))
          if(all(File_I(iFile)%iRange_I==0))&
               File_I(iFile)%iRange_I = [1, nLon, 1, nLat]
       case(Distr1D_)
          allocate(File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, 1:nVertexMax))
          if(all(File_I(iFile)%iRange_I==0))&
               File_I(iFile)%iRange_I = [1, nLon, 1, nLat]
       case(Distr2D_)
          allocate(File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, 1:nLineAll))
       case(DistrTime_)
          allocate(File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, 1))
          if(all(File_I(iFile)%iRange_I==0))&
               File_I(iFile)%iRange_I = [1, nLon, 1, nLat]
       case(DistrTraj_)
          allocate(File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, 1))
       case(Flux2D_)
          ! extra space is reserved for sum of spreads
          allocate(File_I(iFile) % Buffer_II(File_I(iFile)%nFluxVar, &
               File_I(iFile)%iRange_I(2), File_I(iFile)%iRange_I(4)))
       case(FluxTime_)
          if(.not.IsReadySpreadPoint) call CON_stop(NameSub//  &
               ": Angular spread parameters haven't been set;" &
               //" use #SPREADSOLIDANGLE in PARAM.in")
          ! extra space reserved for time of the output
          allocate(File_I(iFile) % Buffer_II(1+File_I(iFile)%nFluxVar, 1, 1))
       case(FluxX0_, FluxY0_)
          ! account for lon=90/270 or 0/180 deg situation
          allocate(File_I(iFile) % Buffer_II( &
               1 + File_I(iFile)%nMhdVar + File_I(iFile)%nFluxVar, &
               2*File_I(iFile)%nRangeCut_I(2), File_I(iFile)%nRangeCut_I(1)))
       case(FluxZ0_)
          allocate(File_I(iFile) % Buffer_II( &
               File_I(iFile)%nMhdVar + File_I(iFile)%nFluxVar, &
               File_I(iFile)%nRangeCut_I(2), File_I(iFile)%nRangeCut_I(1)))
       case(Shock2D_)
          ! extra space reserved for line index
          allocate(File_I(iFile) % Buffer_II( &
               File_I(iFile)%nMhdVar + File_I(iFile)%nExtraVar, 1:nLineAll, 1))
       case(ShockTime_)
          ! extra space reserved for line index
          allocate(File_I(iFile) % Buffer_II( &
               1 + File_I(iFile)%nMhdVar + File_I(iFile)%nExtraVar, 1, 1))
       end select
    end do

    ! Reset/trim NameTagFile if nTag==0/nTag>0; the latter happens at restart.

    ! During the run new tags are continuously appended to NameTagFile,
    ! however, only when the run is succesfully finalized the list of tags is
    ! considered to be valid, i.e. new #NTAG is written to the header file.

    ! If previous run hasn't been properly finalized, NameTagFile may be
    ! inconsistent with nTag, therefore the file is trimmed according to nTag

    if(iProc/=0) RETURN ! done only by the root
    ! Tag list file is only used in non-appended write mode
    if(UseAppendedWrite) RETURN
    ! full file name
    NameFile = trim(NamePlotDir)//trim(NameTagFile)
    if(nTag > 1) then
       allocate(StringTag_I(nTag - 1))
       call open_file(file=NameFile, status='old', NameCaller=NameSub)
       do iTag = 1, nTag - 1
          read(UnitTmp_,'(a)') StringTag_I(iTag)
       end do
       call close_file
    end if
    call open_file(file=NameFile, status='replace', NameCaller=NameSub)
    if(nTag > 1) then
       do iTag = 1, nTag - 1
          write(UnitTmp_,'(a)') StringTag_I(iTag)
       end do
       deallocate(StringTag_I)
    end if
    call close_file

  end subroutine init
  !============================================================================
  subroutine save_plot_all(IsInitialOutputIn)

    ! write the output data

    use ModMpi
    use SP_ModChannel, ONLY: get_integral_flux
    use SP_ModDistribution, ONLY: Mu_C
    use SP_ModSatellite, ONLY: nSat, write_satellite_file
    use SP_ModConnectivity, ONLY: DoSaveConnectivity, &
         write_connectivity_satellite_files
    use SP_ModGrid, ONLY: Used_B
    use SP_ModProc, ONLY: iComm, nProc, iError
    use SP_ModTime, ONLY: IsSteadyState

    logical, intent(in), optional:: IsInitialOutputIn

    ! loop variables
    integer:: iFile
    integer:: iKindData
    integer:: iTimeOutputNew
    logical:: IsInitialOutput

    character(len=*), parameter:: NameSub = 'save_plot_all'
    !--------------------------------------------------------------------------
    if(present(IsInitialOutputIn))then
       IsInitialOutput = IsInitialOutputIn
    else
       IsInitialOutput = .false.
    end if
    if(nFileOut == 0)then
       if(DoSaveConnectivity) call write_connectivity_satellite_files(IsInitialOutput)
       RETURN
    end if
    ! Fluxes are needed by every output, the initial one included: after a
    ! restart the initial output rewrites the record at the restart time,
    ! which would otherwise be written with the Flux_VIB fill value.
    call get_integral_flux
    ! check whether this is a call for initial output
    if(IsInitialOutput)then
       if(nSat > 0) call write_satellite_file(.true.)
       if(DoSaveConnectivity) call write_connectivity_satellite_files(.true.)
       if(.not.DoSaveInitial)RETURN
    end if

    ! Check how often the output is needed
    if(IsInitialOutput)then
       ! Skip the check
    elseif(nOutput > 0)then
       ! Output if iIter is a multiple of nOutput
       if(mod(iIter,nOutput)/=0)RETURN
    elseif(DtOutput > 0.0 .and. .not.IsSteadyState)then
       iTimeOutputNew = int(SPTime/DtOutput)
       if(iTimeOutputNew > iTimeOutput)then
          iTimeOutput = iTimeOutputNew
       else
          RETURN
       end if
    end if
    if(nSat > 0) call write_satellite_file(.false.)
    if(DoSaveConnectivity) call write_connectivity_satellite_files(.false.)
    ! Save outputs into each file (according to StringPlot)
    do iFile = 1, nFileOut
       iKindData = File_I(iFile) % iKindData

       ! during initial call only background 1D data is printed
       if(IsInitialOutput .and. &
            iKindData /= MH1D_ .and. iKindData /= Distr1D_) iKindData = -1

       select case(iKindData)
       case(MH1D_)
          call write_mh_1d
          call write_mh_channel
       case(MH2D_)
          call write_mh_2d
          call write_mh_channel
       case(MHTime_)
          call write_mh_time
          call write_mh_channel
       case(Distr1D_)
          call write_distr_1d
       case(Distr2D_)
          call write_distr_2d
       case(DistrTime_)
          call write_distr_time
       case(DistrTraj_)
          call write_distr_traj
       case(Flux2D_)
          call write_flux_2d
          call write_mh_channel
       case(FluxTime_)
          call write_flux_time
          call write_mh_channel
       case(FluxX0_)
          call write_flux_xy0(X_)
          call write_mh_channel
       case(FluxY0_)
          call write_flux_xy0(Y_)
          call write_mh_channel
       case(FluxZ0_)
          call write_flux_z0
          call write_mh_channel
       case(Shock2D_)
          call write_shock_2d
       case(ShockTime_)
          call write_shock_time
       end select
    end do
  contains
    !==========================================================================
    subroutine write_mh_1d

      ! write output with 1D MH data in the format to be read by IDL/TECPLOT;
      ! separate file is created for each field line, and the name format is:
      ! MH_data_<iLon>_<iLat>_n<ddhhmmss>_n<iIter>.{out/dat}

      use SP_ModGrid, ONLY: FootPoint_VB, NoShock_

      ! name of the output file
      character(len=100) :: NameFile
      ! header for the file
      character(len=500) :: StringHeader
      ! loop variables
      integer :: iLine
      ! index of the last particle on the field line
      integer :: iEnd
      ! for better readability
      integer :: nMhdVar, nExtraVar, nFluxVar
      ! shock location
      integer :: iShock
      integer, parameter :: RShock_     = Z_ + 2
      integer, parameter :: StartTime_  = RShock_ + 1
      integer, parameter :: StartJulian_= StartTime_ + 1
      real :: Param_I(LagrID_:StartJulian_)
      ! timetag
      character(len=15) :: StringTime
      ! Flag to truncate appended files on first write after restart
      logical, save :: DoTruncate = .true.
      character(len=*), parameter:: NameSub = 'write_mh_1d'
      !------------------------------------------------------------------------
      ! If there are more than one processors working on the same field line,
      ! we only save the data for the first nLineAll processors.
      if(nProc > nLineAll .and. iProc >= nLineAll) RETURN

      ! On first call after restart with appended write, truncate files
      ! to nTag-1 blocks; the nTag-th block will be rewritten by
      ! the current (initial) output
      if(DoTruncate) then
         if(UseAppendedWrite .and. nTag > 1) call truncate_appended(iFile)
         DoTruncate = .false.
      end if

      ! Update number of time tags on all processors;
      ! on restart, the initial output rewrites the nTag-th block,
      ! so do not increment nTag in that case
      if(.not.(IsInitialOutput .and. nTag > 0)) nTag = nTag + 1
      ! Write tag list file only on proc 0 in non-appended mode
      if(iProc==0) then
         if(.not.UseAppendedWrite) then
            ! add to the tag list file
            NameFile = trim(NamePlotDir)//trim(NameTagFile)
            call open_file(file=NameFile, position='append', status='unknown', &
                 NameCaller=NameSub)
            if(UseDateTime) then
               call get_date_time_string(SPTime, StringTime)
               write(UnitTmp_,'(a,i6.6)') 'e'//StringTime//'_n',iIter
               write(*,'(a,i6.6)')'Write plot file e'//StringTime//'_n',iIter
            else
               call get_time_string(SPTime, StringTime(1:8))
               write(UnitTmp_,'(a,i6.6)') 't'//StringTime(1:8)//'_n',iIter
               write(*,'(a,i6.6)')'Write plot file t'//StringTime(1:8)//'_n',iIter
            end if
            call close_file
         else
            write(*,'(a,i6,a,es12.5)')'Append MH_data step',iIter,' t=',SPTime
         end if
      end if

      ! Write ouput files themselves
      nMhdVar   = File_I(iFile) % nMhdVar
      nExtraVar = File_I(iFile) % nExtraVar
      nFluxVar  = File_I(iFile) % nFluxVar

      ! header for the output file
      StringHeader = 'MFLAMPA: data along a field line; '//&
           'Coordindate system: '//trim(TypeCoordSystem)//'; '&
           //trim(File_I(iFile)%StringHeaderAux)

      do iLine = 1, nLine
         if(.not.Used_B(iLine)) CYCLE
         if(do_skip(iLine, File_I(iFile)%iRange_I))CYCLE
         if(UseAppendedWrite) then
            ! Fixed name per field line — no iteration tag
            call make_file_name( &
                 StringBase    = NameMHData,                      &
                 iLine         = iLine,                           &
                 NameExtension = File_I(iFile)%NameFileExtension, &
                 NameOut       = NameFile)
         else
            call make_file_name( &
                 StringBase    = NameMHData,                      &
                 iLine         = iLine,                           &
                 iIter         = iIter,                           &
                 NameExtension = File_I(iFile)%NameFileExtension, &
                 NameOut       = NameFile)
         end if

         ! reset the output buffer
         File_I(iFile) % Buffer_II = 0.0

         ! get min and max particle indexes on this field line
         iEnd = nVertex_B(iLine)
         ! fill the output buffer
         if(nMhdVar>0)File_I(iFile) % Buffer_II(1:nMhdVar, 1:iEnd, 1) = &
              MHData_VIB(File_I(iFile) % iVarMhd_V(1:nMhdVar), 1:iEnd, iLine)
         if(nExtraVar>0)File_I(iFile) % Buffer_II(nMhdVar+1:nMhdVar+nExtraVar,&
              1:iEnd, 1) = State_VIB(File_I(iFile) % iVarExtra_V(1:nExtraVar),&
              1:iEnd, iLine)
         if(File_I(iFile) % DoPlotFlux)File_I(iFile) % Buffer_II(&
              nMhdVar + nExtraVar + 1:nMhdVar + nExtraVar + nFluxVar,&
              1:iEnd, 1) = Flux_VIB(Flux0_:FluxMax_, 1:iEnd, iLine)

         ! Parameters
         Param_I(LagrID_:Z_) = FootPoint_VB(LagrID_:Z_,iLine)
         ! shock location
         if(iShock_IB(Shock_,iLine)/=NoShock_) then
            iShock             = iShock_IB(Shock_,iLine)
            Param_I(RShock_)   = State_VIB(R_,iShock,iLine)
            Param_I(RShock_-1) = real(iShock)
         else
            Param_I(RShock_-1:RShock_) = -1.0
         end if
         ! start time in seconds from base year
         Param_I(StartTime_)  = StartTime
         ! start time in Julian date
         Param_I(StartJulian_)= StartTimeJulian
         ! print data to file
         call save_plot_file(&
              NameFile        = NameFile, &
              TypePositionIn  = merge('append', 'rewind', &
                   UseAppendedWrite .and. nTag > 1), &
              StringHeaderIn  = StringHeader, &
              TypeFileIn      = File_I(iFile) % TypeFile, &
              nDimIn          = 1, &
              TimeIn          = SPTime, &
              nStepIn         = iIter, &
              CoordMinIn_D    = [MHData_VIB(LagrID_,1,iLine)], &
              CoordMaxIn_D    = [MHData_VIB(LagrID_,iEnd,iLine)], &
              NameVarIn       = &
              trim(File_I(iFile) % NameVarPlot) // ' ' // &
              trim(File_I(iFile) % NameAuxPlot), &
              VarIn_VI        = &
              File_I(iFile) % Buffer_II(1:nMhdVar + nExtraVar + nFluxVar, &
              1:iEnd, 1),&
              ParamIn_I       = Param_I(LagrID_:StartJulian_))
      end do !  iLine

    end subroutine write_mh_1d
    !==========================================================================
    subroutine truncate_appended(iFile)
      ! Truncate appended MH_data output files: keep only blocks
      ! with nStep < iIter. The block at iIter will be rewritten
      ! by the initial output.
      use ModIoUnit, ONLY: io_unit_new
      integer, intent(in) :: iFile
      integer :: iLine, iUnit, nVertex, iVertex, nStepBlock
      logical :: IsFound
      character(len=100) :: NameFile, NameFileTmp
      character(len=2000) :: StringLine, StringLine1, StringLine2
      !------------------------------------------------------------------------
      do iLine = 1, nLine
         if(.not.Used_B(iLine)) CYCLE
         if(do_skip(iLine, File_I(iFile)%iRange_I)) CYCLE
         call make_file_name( &
              StringBase    = NameMHData,                      &
              iLine         = iLine,                           &
              NameExtension = File_I(iFile)%NameFileExtension, &
              NameOut       = NameFile)
         inquire(file=NameFile, exist=IsFound)
         if(.not.IsFound) CYCLE
         NameFileTmp = trim(NameFile)//'.tmp'
         iUnit = io_unit_new()
         call open_file(iUnitIn=iUnit, file=NameFile, status='old', &
              NameCaller='truncate_appended')
         call open_file(file=NameFileTmp, &
              NameCaller='truncate_appended')
         do
            ! Read first two header lines
            read(iUnit, '(a)', end=200) StringLine1
            read(iUnit, '(a)', end=200) StringLine2
            read(StringLine2, *) nStepBlock
            ! Stop if block step >= restart step
            if(nStepBlock >= iIter) EXIT
            ! Write the two header lines
            write(UnitTmp_, '(a)') trim(StringLine1)
            write(UnitTmp_, '(a)') trim(StringLine2)
            ! Read and copy nVertex line
            read(iUnit, '(a)', end=200) StringLine
            write(UnitTmp_, '(a)') trim(StringLine)
            read(StringLine, *) nVertex
            ! Copy params and column name lines
            read(iUnit, '(a)', end=200) StringLine
            write(UnitTmp_, '(a)') trim(StringLine)
            read(iUnit, '(a)', end=200) StringLine
            write(UnitTmp_, '(a)') trim(StringLine)
            ! Copy data lines
            do iVertex = 1, nVertex
               read(iUnit, '(a)', end=200) StringLine
               write(UnitTmp_, '(a)') trim(StringLine)
            end do
         end do
200      continue
         close(iUnit)
         call close_file
         call execute_command_line( &
              'mv '//trim(NameFileTmp)//' '//trim(NameFile))
      end do
    end subroutine truncate_appended
    !==========================================================================
    subroutine write_mh_2d

      ! write output with 2D MH data in the format to be read by IDL/TECPLOT;
      ! single file is created for all field lines, name format is
      ! MH_data_R=<Radius [AU]>_t<ddhhmmss>_n<iIter>.{out/dat}

      use ModCoordTransform, ONLY: xyz_to_rlonlat

      ! name of the output file
      character(len=100):: NameFile
      ! header of the output file
      character(len=500):: StringHeader
      ! loop variables
      integer :: iLine, iVarPlot, iVarIndex
      ! indexes of corresponding node, latitude and longitude
      integer :: iLineAll
      ! index of particle just above the radius
      integer :: iAbove
      ! xyz coordinate of intersection point or average direction
      real    :: Xyz_D(nDim)
      ! longitude and latitude intersection point
      real    :: LonPoint, LatPoint
      ! interpolation weight
      real    :: Weight
      ! auxilary variable for xyz_to_rlonlat call
      real    :: Aux
      ! for better readability
      integer :: nMhdVar, nExtraVar, nFluxVar, iVarLon, iVarLat
      ! skip a field line not reaching radius of output sphere
      logical :: DoPrint_I(nLineAll)
      ! additional parameters
      integer, parameter:: StartTime_  = 1
      integer, parameter:: StartJulian_= StartTime_ + 1
      integer, parameter:: LonAv_ = StartTime_ + 2
      integer, parameter:: LatAv_ = StartTime_ + 3
      integer, parameter:: AngleSpread_= StartTime_ + 4
      integer :: nParam
      real    :: Param_I(1:AngleSpread_)

      character(len=*), parameter:: NameSub = 'write_mh_2d'
      !------------------------------------------------------------------------
      nExtraVar = File_I(iFile)%nExtraVar
      nMhdVar   = File_I(iFile)%nMhdVar
      nFluxVar  = File_I(iFile)%nFluxVar

      ! Positions for longitude and Latitude
      iVarLon = nMhdVar + nExtraVar + 1
      iVarLat = iVarLon + 1
      if(File_I(iFile) % DoPlotSpread)then
         nParam = AngleSpread_
      else
         nParam = StartJulian_
      end if

      ! header for the output file
      StringHeader = &
           'MFLAMPA: data on a sphere at fixed heliocentric distance; '//&
           'Coordindate system: '//trim(TypeCoordSystem)//'; '&
           //trim(File_I(iFile)%StringHeaderAux)

      ! set the file name
      call make_file_name( &
           StringBase    = NameMHData,                      &
           Radius        = File_I(iFile) % Radius,          &
           iIter         = iIter,                           &
           NameExtension = File_I(iFile)%NameFileExtension, &
           NameOut       = NameFile)

      ! reset the output buffer
      File_I(iFile) % Buffer_II = 0.0

      ! reset, all field lines are printed reaching output sphere
      DoPrint_I = .false.

      ! go over all lines on the processor and find the point of
      ! intersection with output sphere if present
      do iLine = 1, nLine
         if(.not.Used_B(iLine)) CYCLE
         iLineAll = iLineAll0 + iLine

         ! find the particle just above the given radius
         call search_line(iLine, File_I(iFile)%Radius, &
              iAbove, DoPrint_I(iLineAll), Weight)
         DoPrint_I(iLineAll) = DoPrint_I(iLineAll) .and. iAbove /= 1

         ! if no intersection found -> proceed to the next line
         if(.not.DoPrint_I(iLineAll)) CYCLE

         ! intersection is found => get data at that location;
         ! find coordinates of intersection
         Xyz_D = &
              MHData_VIB(X_:Z_, iAbove-1, iLine) * (1-Weight) + &
              MHData_VIB(X_:Z_, iAbove,   iLine) *    Weight
         call xyz_to_rlonlat(Xyz_D, Aux, LonPoint, LatPoint)
         ! put longitude and latitude to output
         File_I(iFile) % Buffer_II(iVarLon, iLineAll, 1) = LonPoint
         File_I(iFile) % Buffer_II(iVarLat, iLineAll, 1) = LatPoint
         ! interpolate each requested variable
         do iVarPlot = 1, nMhdVar
            iVarIndex = File_I(iFile) % iVarMhd_V(iVarPlot)
            File_I(iFile) % Buffer_II(iVarPlot, iLineAll, 1) = &
                 MHData_VIB(iVarIndex, iAbove-1, iLine) * (1-Weight) + &
                 MHData_VIB(iVarIndex, iAbove,   iLine) *    Weight
         end do
         do iVarPlot = 1, nExtraVar
            iVarIndex = File_I(iFile) % iVarExtra_V(iVarPlot)
            File_I(iFile) % Buffer_II(nMhdVar + iVarPlot, iLineAll, 1) = &
                 State_VIB(iVarIndex, iAbove-1, iLine) * (1-Weight) + &
                 State_VIB(iVarIndex, iAbove,   iLine) *    Weight
         end do
         if(File_I(iFile) % DoPlotFlux)&
              File_I(iFile)%Buffer_II(1+iVarLat:nFluxVar+iVarLat, iLineAll, 1)&
              = Flux_VIB(Flux0_:FluxMax_, iAbove-1, iLine) * (1-Weight) +     &
              Flux_VIB(  Flux0_:FluxMax_, iAbove,   iLine) *    Weight
      end do !  iLine

      ! gather interpolated data on the source processor
      if(nProc > 1)then
         call MPI_reduce_real_array(File_I(iFile) % Buffer_II, &
              nLineAll*(iVarLat + nFluxVar), MPI_SUM, 0, iComm, iError)
         call MPI_reduce_logical_array(DoPrint_I, &
              nLineAll, MPI_LOR, 0, iComm, iError)
         if(iProc > 0) RETURN
      end if
      ! start time in seconds from base year
      Param_I(StartTime_)   = StartTime
      ! start time in Julian date
      Param_I(StartJulian_) = StartTimeJulian

      ! spread data: average lon and lat, angle spread
      if(File_I(iFile) % DoPlotSpread)then
         ! average direction (not normalized)
         Xyz_D = sum(File_I(iFile) % Buffer_II(X_:Z_, :, 1), DIM=2, &
              MASK=spread(DoPrint_I, 1, nDim))
         call xyz_to_rlonlat(Xyz_D, Aux, Param_I(LonAv_), Param_I(LatAv_))
         ! angular spread/variance
         Param_I(AngleSpread_) = sqrt(sum(acos(min(1.0, max(-1.0, &
              cos(Param_I(LatAv_) - &
              pack(File_I(iFile)%Buffer_II(iVarLat, :, 1),MASK=DoPrint_I)) + &
              cos(Param_I(LatAv_))* &
              cos(pack(File_I(iFile)%Buffer_II(iVarLat,:,1),MASK=DoPrint_I))* &
              (cos(Param_I(LonAv_)- &
              pack(File_I(iFile)%Buffer_II(iVarLon,:,1),MASK=DoPrint_I))-1.0)))&
              )**2) / (count(DoPrint_I) - 1))
         Param_I([LonAv_, LatAv_, AngleSpread_]) = &
              Param_I([LonAv_, LatAv_, AngleSpread_]) * cRadToDeg
      end if

      ! convert angles
      File_I(iFile) % Buffer_II([iVarLon,iVarLat], :, 1) = &
           File_I(iFile) % Buffer_II([iVarLon,iVarLat], :, 1) * cRadToDeg

      ! print data to file
      call save_plot_file(&
           NameFile      = NameFile, &
           StringHeaderIn= StringHeader, &
           TypeFileIn    = File_I(iFile) % TypeFile, &
           nDimIn        = 1, &
           TimeIn        = SPTime, &
           nStepIn       = iIter,  &
           ParamIn_I     = Param_I(1:nParam), &
           Coord1In_I    = real(pack(iNodeIndex_I, MASK=DoPrint_I)),&
           NameVarIn     = &
           trim(File_I(iFile) % NameVarPlot) // ' ' // &
           trim(File_I(iFile) % NameAuxPlot), &
           VarIn_VI      = &
           reshape(&
           pack(File_I(iFile) % Buffer_II(1:iVarLat+nFluxVar,1:nLineAll,1),&
           MASK = spread(DoPrint_I, DIM=1, NCOPIES=iVarLat + nFluxVar)),   &
           [iVarLat + nFluxVar, count(DoPrint_I)] ))

    end subroutine write_mh_2d
    !==========================================================================
    subroutine write_mh_time

      ! write output w/time series MH data in format to be read by IDL/TECPLOT;
      ! a file is created for each field lines, name format is
      ! MH_data_R=<Radius [AU]>_<iLon>_<iLat>.{out/dat}
      ! the file has no timetag as it is updated during the run

      ! name of the output file
      character(len=100):: NameFile
      ! header for the file
      character(len=500):: StringHeader
      ! loop variables
      integer :: iLine, iVarPlot, iVarIndex
      ! index of particle just above the radius
      integer :: iAbove
      ! interpolation weight
      real    :: Weight
      ! for better readability
      integer :: nMhdVar, nExtraVar, nFluxVar
      ! skip a field line if it fails to reach radius of output sphere
      logical :: DoPrint
      ! size of the already written data
      integer :: nDataLine
      ! current size of the buffer
      integer :: nBufferSize
      ! whether the output file already exists
      logical :: IsPresent
      ! additional parameters
      integer, parameter:: StartTime_  = 1
      integer, parameter:: StartJulian_= StartTime_ + 1
      real    :: Param_I(1:StartJulian_)

      character(len=*), parameter:: NameSub = 'write_mh_time'
      !------------------------------------------------------------------------
      ! If there are more than one processors working on the same field line,
      ! we only save the data for the first nLineAll processors.
      if(nProc > nLineAll .and. iProc >= nLineAll) RETURN

      nMhdVar   = File_I(iFile) % nMhdVar
      nExtraVar = File_I(iFile) % nExtraVar
      nFluxVar  = File_I(iFile) % nFluxVar
      ! set header
      StringHeader = &
           'MFLAMPA: data on a field line at fixed heliocentric distance; '//&
           'Coordindate system: '//trim(TypeCoordSystem)//'; '&
           //trim(File_I(iFile)%StringHeaderAux)

      ! at first call, remove files if they exist to reset time series output
      if(File_I(iFile) % IsFirstCall) then
         ! mark that the 1st call has already happened
         File_I(iFile) % IsFirstCall = .false.
         ! go over list of lines and remove file for each one
         do iLine = 1, nLine
            if(.not.Used_B(iLine)) CYCLE
            ! set the file name
            call make_file_name( &
                 StringBase    = NameMHData,                        &
                 Radius        = File_I(iFile) % Radius,            &
                 iLine         = iLine,                             &
                 NameExtension = File_I(iFile) % NameFileExtension, &
                 NameOut       = NameFile)
            call remove_file(NameFile)
         end do
      end if

      ! reset the output buffer
      File_I(iFile) % Buffer_II = 0.0

      ! go over all lines on the processor and find the point of intersection
      ! with output sphere if present
      do iLine = 1, nLine
         if(.not.Used_B(iLine)) CYCLE
         if(do_skip(iLine, File_I(iFile)%iRange_I))CYCLE
         ! reset, the field line is printed unless fail to reach output sphere
         DoPrint = .true.

         ! find the particle just above the given radius
         call search_line(iLine, File_I(iFile)%Radius, iAbove, DoPrint, Weight)
         DoPrint = DoPrint .and. iAbove /= 1

         ! if no intersection found -> proceed to the next line
         if(.not.DoPrint) CYCLE

         ! set the file name
         call make_file_name( &
              StringBase    = NameMHData,                        &
              Radius        = File_I(iFile) % Radius,            &
              iLine         = iLine,                             &
              NameExtension = File_I(iFile) % NameFileExtension, &
              NameOut       = NameFile)

         ! if file already exists -> read its content
         nDataLine = 0
         inquire(FILE=NameFile, EXIST=IsPresent)
         if(IsPresent) then
            ! first, determine its size
            call read_plot_file(&
                 NameFile   = NameFile, &
                 TypeFileIn = File_I(iFile) % TypeFile, &
                 n1Out      = nDataLine)
            ! if buffer is too small then reallocate it
            nBufferSize = ubound(File_I(iFile)%Buffer_II, DIM=2)
            if(nBufferSize < nDataLine + 1) then
               deallocate(File_I(iFile) % Buffer_II)
               allocate(File_I(iFile) % Buffer_II(&
                    1 + nMhdVar + nExtraVar + nFluxVar, 2*nBufferSize, 1))
            end if

            ! read the data itself
            call read_plot_file(&
                 NameFile   = NameFile,                  &
                 TypeFileIn = File_I(iFile) % TypeFile,  &
                 Coord1Out_I= File_I(iFile) % Buffer_II( &
                 1+nMhdVar+nExtraVar+nFluxVar, :, 1),    &
                 VarOut_VI  = File_I(iFile) % Buffer_II( &
                 1:nMhdVar+nExtraVar+nFluxVar, :, 1))
         end if

         ! add new data
         nDataLine = nDataLine + 1
         ! put time into buffer
         File_I(iFile) % Buffer_II(&
              1+nMhdVar+nExtraVar+nFluxVar, nDataLine, 1) = SPTime
         ! interpolate data and fill buffer
         ! interpolate each requested variable
         do iVarPlot = 1, nMhdVar
            iVarIndex = File_I(iFile) % iVarMhd_V(iVarPlot)
            File_I(iFile) % Buffer_II(iVarPlot, nDataLine, 1) = &
                 MhData_VIB(iVarIndex, iAbove-1, iLine) * (1-Weight) + &
                 MhData_VIB(iVarIndex, iAbove,   iLine) *    Weight
         end do
         do iVarPlot = 1, nExtraVar
            iVarIndex = File_I(iFile) % iVarExtra_V(iVarPlot)
            File_I(iFile) % Buffer_II(nMhdVar+iVarPlot, nDataLine, 1) = &
                 State_VIB(iVarIndex, iAbove-1, iLine) * (1-Weight) + &
                 State_VIB(iVarIndex, iAbove,   iLine) *    Weight
         end do
         if(File_I(iFile) % DoPlotFlux) &
              File_I(iFile) % Buffer_II(1 + nMhdVar + nExtraVar: &
              nFluxVar + nMhdVar + nExtraVar, nDataLine, 1) =    &
              Flux_VIB(Flux0_:FluxMax_, iAbove-1, iLine) * (1-Weight) + &
              Flux_VIB(Flux0_:FluxMax_, iAbove,   iLine) *    Weight

         ! start time in seconds from base year
         Param_I(StartTime_)  = StartTime
         ! start time in Julian date
         Param_I(StartJulian_)= StartTimeJulian

         ! reprint data to file
         call save_plot_file(&
              NameFile      = NameFile, &
              StringHeaderIn= StringHeader, &
              TypeFileIn    = File_I(iFile) % TypeFile, &
              nDimIn        = 1, &
              TimeIn        = SPTime, &
              nStepIn       = iIter, &
              ParamIn_I     = Param_I, &
              Coord1In_I    = &
              File_I(iFile) % Buffer_II(1 + nMhdVar + nExtraVar + nFluxVar, &
              1:nDataLine, 1), &
              NameVarIn     = &
              trim(File_I(iFile) % NameVarPlot) // ' ' // &
              trim(File_I(iFile) % NameAuxPlot), &
              VarIn_VI      = &
              File_I(iFile) % Buffer_II(1:nMhdVar + nExtraVar + nFluxVar, &
              1:nDataLine, 1))
      end do !  iLine

    end subroutine write_mh_time
    !==========================================================================
    subroutine write_mh_channel

      ! write output for energy channels saved in MH data files
      ! a file is output once with the name being NameEChannelFile, i.e.,
      ! MH_data_EChannel.H, only created in the first initial call

      use SP_ModChannel, ONLY: nFluxChannel, nFluxChannelSat, &
           FluxFirst_, FluxLast_, EFlux_, NameChannelSource_I,     &
           EChannelLoIo_I, EChannelHiIo_I, EChannelMidIo_I
      use SP_ModDistribution, ONLY: EnergyInjIo, EnergyMaxIo
      use SP_ModUnit, ONLY: NameEnergyFluxUnit

      ! header for the file
      character(len=200) :: StringHeader, StringColumn
      ! loop variable
      integer:: iFlux
      ! string format for energy channels saved
      character(len=7)   :: StringFormatEChannel
      character(len=100) :: StringFormatLine

      character(len=*), parameter:: NameSub = 'write_mh_channel'
      !------------------------------------------------------------------------
      if(.not.DoInitEChannel) RETURN
      ! performed on root proc only
      if(iProc/=0) RETURN

      ! set header
      write(StringHeader,'(a,i2,1X,a,i2)') &
           'MFLAMPA: all flux channels in MH_data; nFluxChannel=', &
           nFluxChannel, 'nFluxChannelSat=', nFluxChannelSat
      StringColumn = 'FluxChannel   ChannelSource   EnergyLow' // &
           '   EnergyHigh   EnergyGeoMean   EnergyUnit   FluxChannelUnit'
      ! set string format
      StringFormatEChannel = 'es12.4'
      StringFormatLine = '(a14,1X,a12,1X,'// &
           trim(StringFormatEChannel)//',1X,'// &
           trim(StringFormatEChannel)//',1X,'// &
           trim(StringFormatEChannel)//',4X,a3,3X,a12)'

      ! open the file
      call open_file(file=trim(NamePlotDir)//trim(NameEChannelFile), &
           NameCaller=NameSub)
      ! write the header and column descriptions
      write(UnitTmp_,'(a)') StringHeader
      write(UnitTmp_,'(a)') StringColumn

      ! write flux_total info
      write(UnitTmp_,trim(StringFormatLine)) &
           trim(NameFluxChannel_I(Flux0_)), "General", &
           EnergyInjIo, EnergyMaxIo, &
           sqrt(EnergyInjIo*EnergyMaxIo), &
           trim(NameEnergyUnit), trim(NameFluxUnit)
      ! write the flux channel list info, line by line
      do iFlux = FluxFirst_, FluxLast_
         write(UnitTmp_,trim(StringFormatLine)) &
              trim(NameFluxChannel_I(iFlux)), &
              trim(NameChannelSource_I(iFlux)), &
              EChannelLoIo_I(iFlux), EChannelHiIo_I(iFlux), &
              EChannelMidIo_I(iFlux), &
              trim(NameEnergyUnit), trim(NameFluxUnit_I(iFlux))
      end do
      ! write eflux info
      write(UnitTmp_,trim(StringFormatLine)) &
           trim(NameFluxChannel_I(EFlux_)), "General", &
           EnergyInjIo, EnergyMaxIo, &
           sqrt(EnergyInjIo*EnergyMaxIo), &
           trim(NameEnergyUnit), trim(NameEnergyFluxUnit)

      ! close the file
      call close_file

      ! finish the initial call
      DoInitEChannel = .false.

    end subroutine write_mh_channel
    !==========================================================================
    subroutine write_distr_1d

      ! write output with 1D Distribution in the format read by IDL/TECPLOT;
      ! separate file is created for each field line, and the name format is:
      ! Distr_<R/S>_<cdf/def/DEF>_<iLon>_<iLat>_e<ddhhmmss>_n<iIter>.{out/dat}

      use SP_ModGrid, ONLY: S_

      ! name of the output file
      character(len=100) :: NameFile
      ! header of the output file
      character(len=500) :: StringHeader
      character(len=2)   :: TypeDist
      character(len=4)   :: TypeDistr
      ! loop variables
      integer :: iLine
      ! indexes of corresponding node, latitude and longitude
      integer :: iLat, iLon
      ! index of the last particle on the field line
      integer :: iEnd
      ! scale and conversion factor
      real    :: Scale_G(0:nP+1)
      ! index for the length or distance axis
      integer :: iDistance

      character(len=*), parameter:: NameSub = 'write_distr_1d'
      !------------------------------------------------------------------------
      ! If there are more than one processors working on the same field line,
      ! we only save the data for the first nLineAll processors.
      if(nProc > nLineAll .and. iProc >= nLineAll) RETURN

      ! set header
      StringHeader = 'MFLAMPA: Distribution data along a field line, with ' &
           //trim(File_I(iFile) % StringHeaderAux)

      ! set the momentum or kinetic energy axis (first axis)
      select case(File_I(iFile) % iScale)
      case(Momentum_)
         Scale_G = Log10Momentum_G
      case(Energy_)
         Scale_G = Log10KinEnergyIo_G
      end select

      ! set the index for the distance axis (second axis)
      select case(File_I(iFile) % iDistance)
      case(Distance_)
         iDistance = R_
         TypeDist = '_R'
      case(LineLength_)
         iDistance = S_
         TypeDist = '_S'
      end select

      ! set the file name
      select case(File_I(iFile) % iTypeDistr)
      case(CDF_)
         TypeDistr = '_cdf'
      case(DEFIo_)
         TypeDistr = '_def'
      case(DEFSi_)
         TypeDistr = '_DEF'
      end select

      do iLine = 1, nLine
         if(.not.Used_B(iLine)) CYCLE
         if(do_skip(iLine, File_I(iFile)%iRange_I))CYCLE
         ! set the file name
         call make_file_name( &
              StringBase    = NameDistrData//TypeDistr//TypeDist, &
              iLine         = iLine,                              &
              iIter         = iIter,                              &
              NameExtension = File_I(iFile) % NameFileExtension,  &
              NameOut       = NameFile)

         ! get max particle indexes on this field line
         iEnd = nVertex_B(iLine)
         File_I(iFile) % Buffer_II = 0.0

         ! the actual distribution, in logarithm
         File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, 1:iEnd) = log10( &
              Distribution_CB(0:nP+1, 1:nMu, 1:iEnd, iLine))
         ! account for the requested output
         select case(File_I(iFile) % iTypeDistr)
         case(CDF_)
            ! do nothing
         case(DEFIo_)
            File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, 1:iEnd) =      &
                 File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, 1:iEnd) + &
                 2.0*reshape(spread(Log10Momentum_G, DIM=2,         &
                 NCOPIES=nMu*iEnd), [nP+2, nMu, iEnd]) + Log10Si2IoFlux
         case(DEFSi_)
            File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, 1:iEnd) =      &
                 File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, 1:iEnd) + &
                 2.0*reshape(spread(Log10Momentum_G, DIM=2,         &
                 NCOPIES=nMu*iEnd), [nP+2, nMu, iEnd])
         end select

         ! print data to file
         ! Note: here, by including nDimIn, all the Coord{1, 2(, 3)}In_I
         ! arrays are not in the size of 1; otherwise, they will be switched
         if(IsMuAvg) then
            ! not necessary to save Mu
            call save_plot_file( &
                 NameFile       = NameFile, &
                 StringHeaderIn = StringHeader, &
                 TypeFileIn     = File_I(iFile) % TypeFile, &
                 nDimIn         = 2, &
                 TimeIn         = SPTime,  &
                 nStepIn        = iIter,   &
                 Coord1In_I     = Scale_G, &
                 Coord2In_I     = State_VIB(iDistance, 1:iEnd, iLine), &
                 NameVarIn      = &
                 trim(File_I(iFile) % NameVarPlot) // ' ' // &
                 trim(File_I(iFile) % NameAuxPlot), &
                 VarIn_II       = &
                 File_I(iFile) % Buffer_II(0:nP+1, nMu, 1:iEnd))
         else
            ! pitch-angle dependent, necessary to save
            call save_plot_file( &
                 NameFile       = NameFile, &
                 StringHeaderIn = StringHeader, &
                 TypeFileIn     = File_I(iFile) % TypeFile, &
                 nDimIn         = 3, &
                 TimeIn         = SPTime,  &
                 nStepIn        = iIter,   &
                 Coord1In_I     = Scale_G, &
                 Coord2In_I     = Mu_C,    &
                 Coord3In_I     = State_VIB(iDistance, 1:iEnd, iLine), &
                 NameVarIn      = &
                 trim(File_I(iFile) % NameVarPlot) // ' ' // &
                 trim(File_I(iFile) % NameAuxPlot), &
                 VarIn_III      = &
                 File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, 1:iEnd))
         end if
      end do !  iLine

    end subroutine write_distr_1d
    !==========================================================================
    subroutine write_distr_2d

      ! write output with 2D Distribution in the format read by IDL/TECPLOT;
      ! separate file is created for each field line, and the name format is:
      ! Distr_<cdf/def/DEF>_R=<Radius [AU]>_e<ddhhmmss>_n<iIter>.{out/dat}

      ! name of the output file
      character(len=100) :: NameFile
      ! header of the output file
      character(len=500) :: StringHeader
      character(len=4)   :: TypeDistr
      ! loop variables
      integer :: iLine
      ! indexes of corresponding node, latitude and longitude
      integer :: iLineAll
      ! index of particle just above the radius
      integer :: iAbove
      ! interpolation weight
      real    :: Weight
      ! scale and conversion factor
      real    :: Scale_G(0:nP+1)
      ! skip a field line not reaching radius of output sphere
      logical :: DoPrint_I(nLineAll)

      character(len=*), parameter:: NameSub = 'write_distr_2d'
      !------------------------------------------------------------------------

      ! set header
      StringHeader = &
           'MFLAMPA: Distribution data on a sphere at '//&
           'fixed heliocentric distance; Coordindate system: '//&
           trim(TypeCoordSystem)//'; '//trim(File_I(iFile)%StringHeaderAux)

      ! set the momentum or kinetic energy axis (first axis)
      select case(File_I(iFile) % iScale)
      case(Momentum_)
         Scale_G = Log10Momentum_G
      case(Energy_)
         Scale_G = Log10KinEnergyIo_G
      end select

      ! determine string for the saved filename
      select case(File_I(iFile) % iTypeDistr)
      case(CDF_)
         TypeDistr = '_cdf'
      case(DEFIo_)
         TypeDistr = '_def'
      case(DEFSi_)
         TypeDistr = '_DEF'
      end select

      ! set the file name
      call make_file_name( &
           StringBase    = NameDistrData // TypeDistr,        &
           Radius        = File_I(iFile) % Radius,            &
           iIter         = iIter,                             &
           NameExtension = File_I(iFile) % NameFileExtension, &
           NameOut       = NameFile)

      ! reset the output buffer
      File_I(iFile) % Buffer_II = 0.0

      ! reset, all field lines are printed reaching output sphere
      DoPrint_I = .false.

      do iLine = 1, nLine
         if(.not.Used_B(iLine)) CYCLE
         iLineAll = iLineAll0 + iLine

         ! find the particle just above the given radius
         call search_line(iLine, File_I(iFile) % Radius, &
              iAbove, DoPrint_I(iLineAll), Weight)
         DoPrint_I(iLineAll) = DoPrint_I(iLineAll) .and. iAbove /= 1

         ! if no intersection found -> proceed to the next line
         if(.not.DoPrint_I(iLineAll)) CYCLE

         ! intersection is found -> get Distribution data at that location
         File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, iLineAll) = log10( &
              Distribution_CB(0:nP+1, 1:nMu, iAbove-1, iLine) * (1-Weight) + &
              Distribution_CB(0:nP+1, 1:nMu, iAbove,   iLine) *    Weight)
      end do !  iLine

      ! account for the requested output, only on the source processor
      if(iProc==0) then
         select case(File_I(iFile) % iTypeDistr)
         case(CDF_)
            ! do nothing
         case(DEFIo_)
            File_I(iFile) % Buffer_II = File_I(iFile) % Buffer_II + &
                 2.0*reshape(spread(Log10Momentum_G, DIM=2, &
                 NCOPIES=nMu*nLineAll), [nP+2, nMu, nLineAll]) + Log10Si2IoFlux
         case(DEFSi_)
            File_I(iFile) % Buffer_II = File_I(iFile) % Buffer_II + &
                 2.0*reshape(spread(Log10Momentum_G, DIM=2, &
                 NCOPIES=nMu*nLineAll), [nP+2, nMu, nLineAll])
         end select
      end if

      ! gather interpolated data on the source processor
      if(nProc > 1)then
         call MPI_reduce_real_array(File_I(iFile) % Buffer_II, &
              (nP+2)*nMu*nLineAll, MPI_SUM, 0, iComm, iError)
         call MPI_reduce_logical_array(DoPrint_I, &
              nLineAll, MPI_LOR, 0, iComm, iError)
      end if
      ! print data to file
      if(iProc==0) then
         ! Note: here, by including nDimIn, all the Coord{1, 2(, 3)}In_I
         ! arrays are not in the size of 1; otherwise, they will be switched
         if(IsMuAvg) then
            ! not necessary to save Mu
            call save_plot_file( &
                 NameFile      = NameFile, &
                 StringHeaderIn= StringHeader, &
                 TypeFileIn    = File_I(iFile) % TypeFile, &
                 nDimIn        = 2, &
                 TimeIn        = SPTime,  &
                 nStepIn       = iIter,   &
                 Coord1In_I    = Scale_G, &
                 Coord2In_I    = real(pack(iNodeIndex_I, MASK=DoPrint_I)), &
                 NameVarIn     = &
                 trim(File_I(iFile) % NameVarPlot) // ' ' // &
                 trim(File_I(iFile) % NameAuxPlot), &
                 VarIn_II      = reshape( &
                 pack(File_I(iFile) % Buffer_II(0:nP+1, nMu, 1:nLineAll), &
                 MASK=spread(DoPrint_I, DIM=1, NCOPIES=nP+2)), &
                 [nP+2, count(DoPrint_I)]))
         else
            ! pitch-angle dependent, necessary to save
            call save_plot_file( &
                 NameFile      = NameFile, &
                 StringHeaderIn= StringHeader, &
                 TypeFileIn    = File_I(iFile) % TypeFile, &
                 nDimIn        = 3, &
                 TimeIn        = SPTime,  &
                 nStepIn       = iIter,   &
                 Coord1In_I    = Scale_G, &
                 Coord2In_I    = Mu_C,    &
                 Coord3In_I    = real(pack(iNodeIndex_I, MASK=DoPrint_I)), &
                 NameVarIn     = &
                 trim(File_I(iFile) % NameVarPlot) // ' ' // &
                 trim(File_I(iFile) % NameAuxPlot), &
                 VarIn_III     = reshape( &
                 pack(File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, 1:nLineAll), &
                 MASK=reshape(spread(DoPrint_I, DIM=1, NCOPIES=(nP+2)*nMu), &
                 [nP+2, nMu, nLineAll])), [nP+2, nMu, count(DoPrint_I)]))
         end if
      end if

    end subroutine write_distr_2d
    !==========================================================================
    subroutine write_distr_time

      ! write output w/time series Distribution in format read by IDL/TECPLOT;
      ! separate file is created for each field line, and the name format is:
      ! Distr_<cdf/def/DEF>_R=<Radius [AU]>_<iLon>_<iLat>.{out/dat}
      ! the file has no timetag as it is updated during the run

      ! name of the output file
      character(len=100) :: NameFile
      ! header for the file
      character(len=500) :: StringHeader
      character(len=4)   :: TypeDistr
      ! loop variables
      integer :: iLine
      ! index of particle just above the radius
      integer :: iAbove
      ! interpolation weight
      real    :: Weight
      ! scale and conversion factor
      real    :: Scale_G(0:nP+1)
      ! scale and conversion factor
      real, allocatable :: SPTime_I(:)
      ! skip a field line if it fails to reach radius of output sphere
      logical :: DoPrint
      ! size of the already written data
      integer :: nTimeSaved
      ! current size of the buffer
      integer :: nBufferSize
      ! whether the output file already exists
      logical :: IsPresent

      character(len=*), parameter:: NameSub = 'write_distr_time'
      !------------------------------------------------------------------------
      ! If there are more than one processors working on the same field line,
      ! we only save the data for the first nLineAll processors.
      if(nProc > nLineAll .and. iProc >= nLineAll) RETURN

      ! set header
      StringHeader = &
           'MFLAMPA: Distribution data on a sphere at '//&
           'fixed heliocentric distance; Coordindate system: '//&
           trim(TypeCoordSystem)//'; '//trim(File_I(iFile)%StringHeaderAux)

      ! set the momentum or kinetic energy axis (first axis)
      select case(File_I(iFile) % iScale)
      case(Momentum_)
         Scale_G = Log10Momentum_G
      case(Energy_)
         Scale_G = Log10KinEnergyIo_G
      end select

      ! determine string for the saved filename
      select case(File_I(iFile) % iTypeDistr)
      case(CDF_)
         TypeDistr = '_cdf'
      case(DEFIo_)
         TypeDistr = '_def'
      case(DEFSi_)
         TypeDistr = '_DEF'
      end select

      ! at first call, remove files if they exist to reset time series output
      if(File_I(iFile) % IsFirstCall)then
         ! mark that the 1st call has already happened
         File_I(iFile) % IsFirstCall = .false.
         ! go over list of lines and remove file for each one
         do iLine = 1, nLine
            if(.not.Used_B(iLine)) CYCLE
            ! set the file name
            call make_file_name( &
                 StringBase    = NameDistrData // TypeDistr,        &
                 Radius        = File_I(iFile) % Radius,            &
                 iLine         = iLine,                             &
                 NameExtension = File_I(iFile) % NameFileExtension, &
                 NameOut       = NameFile)
            call remove_file(NameFile)
         end do
      end if

      ! reset the output buffer
      File_I(iFile) % Buffer_II = 0.0

      ! go over all lines on the processor and find the point of intersection
      ! with output sphere if present
      do iLine = 1, nLine
         if(.not.Used_B(iLine)) CYCLE
         if(do_skip(iLine, File_I(iFile)%iRange_I))CYCLE
         ! reset, the field line is printed unless fail to reach output sphere
         DoPrint = .true.

         ! find the particle just above the given radius
         call search_line(iLine, File_I(iFile)%Radius, iAbove, DoPrint, Weight)
         DoPrint = DoPrint .and. iAbove /= 1

         ! if no intersection found -> proceed to the next line
         if(.not.DoPrint) CYCLE

         ! set the file name
         call make_file_name( &
              StringBase    = NameDistrData // TypeDistr,        &
              Radius        = File_I(iFile) % Radius,            &
              iLine         = iLine,                             &
              NameExtension = File_I(iFile) % NameFileExtension, &
              NameOut       = NameFile)

         ! if file already exists -> read its content
         nTimeSaved = 0
         inquire(FILE=NameFile, EXIST=IsPresent)
         if(IsPresent) then
            ! first, determine its size
            if(IsMuAvg) then
               call read_plot_file( &
                    NameFile   = NameFile,                 &
                    TypeFileIn = File_I(iFile) % TypeFile, &
                    n2Out      = nTimeSaved)
            else
               call read_plot_file( &
                    NameFile   = NameFile,                 &
                    TypeFileIn = File_I(iFile) % TypeFile, &
                    n3Out      = nTimeSaved)
            end if

            ! if buffer is too small then reallocate it
            nBufferSize = ubound(File_I(iFile)%Buffer_II, DIM=3)
            if(nBufferSize < nTimeSaved + 1) then
               deallocate(File_I(iFile)%Buffer_II)
               allocate(File_I(iFile)%Buffer_II(0:nP+1, 1:nMu, 2*nBufferSize))
            end if
            ! also allocate the SPTime_I array for the SPTime saved
            allocate(SPTime_I(1:nTimeSaved+1))

            ! read the data itself
            if(IsMuAvg) then
               call read_plot_file( &
                    NameFile    = NameFile,                 &
                    TypeFileIn  = File_I(iFile) % TypeFile, &
                    Coord2Out_I = SPTime_I,                 &
                    VarOut_II   = File_I(iFile) % Buffer_II(0:nP+1, nMu, :))
            else
               call read_plot_file( &
                    NameFile    = NameFile,                 &
                    TypeFileIn  = File_I(iFile) % TypeFile, &
                    Coord3Out_I = SPTime_I,                 &
                    VarOut_III  = File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, :))
            end if
         end if

         ! add new data
         nTimeSaved = nTimeSaved + 1
         ! may need to allocate SPTime_I (e.g., at the first step)
         if(.not. allocated(SPTime_I)) allocate(SPTime_I(1:nTimeSaved))
         ! put time into SPTime_I and the buffer
         SPTime_I(nTimeSaved) = SPTime
         File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, nTimeSaved) = log10( &
              Distribution_CB(0:nP+1, 1:nMu, iAbove-1, iLine) * (1-Weight) + &
              Distribution_CB(0:nP+1, 1:nMu, iAbove,   iLine) *    Weight)

         ! account for the requested output
         select case(File_I(iFile) % iTypeDistr)
         case(CDF_)
            ! do nothing
         case(DEFIo_)
            File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, nTimeSaved) = &
                 File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, nTimeSaved) + &
                 2.0*spread(Log10Momentum_G, DIM=2, NCOPIES=nMu) + &
                 Log10Si2IoFlux
         case(DEFSi_)
            File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, nTimeSaved) = &
                 File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, nTimeSaved) + &
                 2.0*spread(Log10Momentum_G, DIM=2, NCOPIES=nMu)
         end select

         ! reprint data to file
         ! Note: here, by including nDimIn, all the Coord{1, 2(, 3)}In_I
         ! arrays are not in the size of 1; otherwise, they will be switched
         if(IsMuAvg) then
            ! not necessary to save Mu
            call save_plot_file( &
                 NameFile      = NameFile, &
                 StringHeaderIn= StringHeader, &
                 TypeFileIn    = File_I(iFile) % TypeFile, &
                 nDimIn        = 2, &
                 TimeIn        = SPTime,   &
                 nStepIn       = iIter,    &
                 Coord1In_I    = Scale_G,  &
                 Coord2In_I    = SPTime_I, &
                 NameVarIn     = &
                 trim(File_I(iFile) % NameVarPlot) // ' ' // &
                 trim(File_I(iFile) % NameAuxPlot), &
                 VarIn_II      = &
                 File_I(iFile) % Buffer_II(0:nP+1, nMu, 1:nTimeSaved))
         else
            ! pitch-angle dependent, necessary to save
            call save_plot_file( &
                 NameFile      = NameFile, &
                 StringHeaderIn= StringHeader, &
                 TypeFileIn    = File_I(iFile) % TypeFile, &
                 nDimIn        = 3, &
                 TimeIn        = SPTime,   &
                 nStepIn       = iIter,    &
                 Coord1In_I    = Scale_G,  &
                 Coord2In_I    = Mu_C,     &
                 Coord3In_I    = SPTime_I, &
                 NameVarIn     = &
                 trim(File_I(iFile) % NameVarPlot) // ' ' // &
                 trim(File_I(iFile) % NameAuxPlot), &
                 VarIn_III     = &
                 File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, 1:nTimeSaved))
         end if
         ! may need to deallocate SPTime_I
         if(allocated(SPTime_I)) deallocate(SPTime_I)
      end do !  iLine

    end subroutine write_distr_time
    !==========================================================================
    subroutine write_distr_traj

      ! write the Distribution function along given spacecraft trajectories;
      ! there are a few steps taken to achieve this:
      !     1. Search the TRAJECTORY and read the satellite locations
      !     2. Set up the TRIANGULATIONs as a mesh for interpolation
      !     3. Find the cell of the satellite location and the weights
      !     4. INTERPOLATE log(distribution) on the triangular mesh
      !     5. Set the file name and SAVE the results
      ! separate file is created for each satellite, and the name format is:
      ! Distribution_<Satellite>_<cdf/def/DEF>_e<ddhhmmss>_n<iIter>.{out/dat}

      use SP_ModSatellite, ONLY: nSat, NameSat_I, Distribution_III, &
           DoTrackSatellite_I, DoTestTri

      ! name of the output file
      character(len=100) :: NameFile
      ! header of the output file
      character(len=500) :: StringHeader
      character(len=4)   :: TypeDistr
      ! scale and conversion factor
      real    :: Scale_G(0:nP+1)

      ! loop variables
      integer :: iSat, iMu, i
      character(len=*), parameter:: NameSub = 'write_distr_traj'
      !------------------------------------------------------------------------
      ! If there are more than one processors working on the same field line,
      ! we only save the data for the first nLineAll processors.
      if(nProc > nLineAll .and. iProc >= nLineAll) RETURN

      ! set the momentum or kinetic energy axis
      select case(File_I(iFile) % iScale)
      case(Momentum_)
         Scale_G = Log10Momentum_G
      case(Energy_)
         Scale_G = Log10KinEnergyIo_G
      end select

      ! set the file name: saved cdf/def/DEF (f or f*p**2, in which unit)
      select case(File_I(iFile) % iTypeDistr)
      case(CDF_)
         TypeDistr = 'cdf_'
      case(DEFIo_)
         TypeDistr = 'def_'
      case(DEFSi_)
         TypeDistr = 'DEF_'
      end select
      ! Save outputs for each satellite
      TRI_SATELLITE: do iSat = 1, nSat

         ! set header
         StringHeader = 'MFLAMPA: Distribution along the trajectory of ' &
              //trim(NameSat_I(iSat))//', with outputs: '  &
              //trim(File_I(iFile)%StringHeaderAux)
         if(DoTestTri) StringHeader = &
              "Test Triangulation in "//trim(StringHeader)

         ! If we can track the satellite: we do triangulation and interpolation
         ! Otherwise the outputs will be 0.0 but the simulations will not stop
         if(.not.(DoTrackSatellite_I(iSat) .or. DoTestTri))CYCLE TRI_SATELLITE

         if(iProc==0)then
            File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, 1) = &
                 log10(Distribution_III(:, :, iSat))
            call make_file_name( &
                 StringBase    = TypeDistr // trim(NameSat_I(iSat)), &
                 Time = SPTime,                                      &
                 NameExtension = File_I(iFile)%NameFileExtension,    &
                 NameOut       = NameFile)
            ! Account for the requested output
            select case(File_I(iFile) % iTypeDistr)
            case(CDF_)
               ! Do nothing
            case(DEFIo_)
               File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, 1) = Log10Si2IoFlux + &
                    File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, 1) + &
                    2.0*spread(Log10Momentum_G, DIM=2, NCOPIES=nMu)
            case(DEFSi_)
               File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, 1) = &
                    File_I(iFile) % Buffer_II(0:nP+1, 1:nMu, 1) + &
                    2.0*spread(Log10Momentum_G, DIM=2, NCOPIES=nMu)
            end select

            ! print data to file
            if(IsMuAvg) then
               ! not necessary to save Mu
               call save_plot_file( &
                    NameFile       = NameFile, &
                    StringHeaderIn = StringHeader, &
                    TypeFileIn     = File_I(iFile) % TypeFile, &
                    nDimIn         = 1, &
                    TimeIn         = SPTime, &
                    nStepIn        = iIter, &
                    Coord1In_I     = Scale_G, &
                    NameVarIn      = &
                    trim(File_I(iFile) % NameVarPlot) // ' ' // &
                    trim(File_I(iFile) % NameAuxPlot), &
                    VarIn_I        = File_I(iFile) % Buffer_II(0:nP+1,nMu,1))
            else
               ! pitch-angle dependent, necessary to save
               call save_plot_file( &
                    NameFile       = NameFile, &
                    StringHeaderIn = StringHeader, &
                    TypeFileIn     = File_I(iFile) % TypeFile, &
                    nDimIn         = 2, &
                    TimeIn         = SPTime, &
                    nStepIn        = iIter, &
                    Coord1In_I     = Scale_G, &
                    Coord2In_I     = Mu_C, &
                    NameVarIn      = &
                    trim(File_I(iFile) % NameVarPlot) // ' ' // &
                    trim(File_I(iFile) % NameAuxPlot), &
                    VarIn_II       = File_I(iFile) % Buffer_II(0:nP+1,1:nMu,1))
            end if
         end if
         ! if this is the test for the triangulation, just exit
         if(DoTestTri) EXIT TRI_SATELLITE
      end do TRI_SATELLITE ! iSat

    end subroutine write_distr_traj
    !==========================================================================
    subroutine write_flux_2d

      ! write file with fluxes in the format to be read by IDL/TECPLOT;
      ! single file is created for all field lines, and the name format is
      ! Flux_R=<Radius [AU]>_t<ddhhmmss>_n<iIter>.{out/dat}
      ! name of the output file

      use SP_ModTriangulate, ONLY: &
           intersect_surf, build_trmesh, interpolate_trmesh
      use SP_ModChannel, ONLY: distr_to_flux
      use ModCoordTransform, ONLY: rlonlat_to_xyz
      real :: Distr_II(0:nP+1,nMu), Xyz_D(nDim), Longitude, Latitude
      ! header of the output file
      character(len=500):: StringHeader
      character(len=100):: NameFile
      ! loop variables
      integer:: iLon, iLat, iStencil_I(3)
      ! additional parameters
      integer, parameter:: StartTime_  = 1
      integer, parameter:: StartJulian_= StartTime_ + 1
      real :: Param_I(1:StartJulian_)

      character(len=*), parameter:: NameSub = 'write_flux_2d'
      !------------------------------------------------------------------------
      ! header for the output file
      StringHeader = &
           'MFLAMPA: flux data on a sphere at fixed heliocentric distance;'//&
           ' Coordindate system: '//trim(TypeCoordSystem)//'; '&
           //trim(File_I(iFile)%StringHeaderAux)

      ! set the file name
      call make_file_name( &
           StringBase    = NameFluxData,                      &
           Radius        = File_I(iFile) % Radius,            &
           Time          = SPTime,                            &
           iIter         = iIter,                             &
           NameExtension = File_I(iFile) % NameFileExtension, &
           NameOut       = NameFile)

      ! reset the output buffer
      File_I(iFile) % Buffer_II = 0.0
      call intersect_surf(File_I(iFile)%Radius)
      if(iProc/=0) RETURN
      call build_trmesh()
      do iLat = File_I(iFile)%iRange_I(3), File_I(iFile)%iRange_I(4)
         Latitude = -90.0 + (iLat - 0.5)*180.0/File_I(iFile)%iRange_I(4)
         do iLon = File_I(iFile)%iRange_I(1), File_I(iFile)%iRange_I(2)
            Longitude = (iLon - 0.5)*360.0/File_I(iFile)%iRange_I(2)
            call rlonlat_to_xyz([File_I(iFile)%Radius, &
                 Longitude*cDegToRad, Latitude*cDegToRad], Xyz_D)
            call interpolate_trmesh(Xyz_D, DistrInterp_II = Distr_II, &
                 iStencilOut_I=iStencil_I)
            if(all(iStencil_I==-1)) then
               File_I(iFile)%Buffer_II(:, iLon, iLat) = &
                    FluxChannelInit_V(Flux0_:FluxMax_)
            else
               call distr_to_flux(Distr_II(1:nP, :), &
                    File_I(iFile)%Buffer_II(:, iLon, iLat))
            end if
         end do
      end do !  iLine

      ! start time in seconds from base year
      Param_I(StartTime_)   = StartTime
      ! start time in Julian date
      Param_I(StartJulian_) = StartTimeJulian

      ! print data to file
      if(iProc==0) &
           call save_plot_file( &
           NameFile      = NameFile, &
           StringHeaderIn= StringHeader, &
           TypeFileIn    = File_I(iFile) % TypeFile, &
           nDimIn        = 2, &
           TimeIn        = SPTime, &
           nStepIn       = iIter, &
           ParamIn_I     = Param_I(StartTime_:StartJulian_), &
           CoordMinIn_D  = [ 180.0/File_I(iFile)%iRange_I(2),&
           -90.0 + 90.0/File_I(iFile)%iRange_I(4)],&
           CoordMaxIn_D  = [ 360.0 - 180.0/File_I(iFile)%iRange_I(2),&
           90.0 - 90.0/File_I(iFile)%iRange_I(4)], &
           NameVarIn     = &
           trim(File_I(iFile) % NameVarPlot) // ' ' // &
           trim(File_I(iFile) % NameAuxPlot), &
           VarIn_VII     = File_I(iFile) % Buffer_II)

    end subroutine write_flux_2d
    !==========================================================================
    subroutine write_flux_time

      ! write file with fluxes in the format to be read by IDL/TECPLOT;
      ! single time series file is created for all field lines with name format
      ! Flux_R=<Radius [AU]>_Lon=<Longitude[deg]>_Lat=<Latitude[deg]>.{out/dat}

      ! name of the output file
      character(len=100):: NameFile
      ! header of the output file
      character(len=500):: StringHeader
      ! loop variables
      integer:: iLine
      ! index of particle just above the radius
      integer:: iAbove
      ! interpolation weight
      real:: Weight
      real:: Spread
      ! for better readability
      integer:: nFluxVar
      ! skip a field line not reaching radius of output sphere
      logical:: DoPrint
      ! size of the already written data
      integer:: nDataLine
      ! current size of the buffer
      integer:: nBufferSize
      ! whether the output file already exists
      logical:: IsPresent
      ! additional parameters
      integer, parameter:: StartTime_  = 1
      integer, parameter:: StartJulian_= StartTime_ + 1
      integer, parameter:: Longitude_  = StartTime_ + 2
      integer, parameter:: Latitude_   = StartTime_ + 3
      real :: Param_I(1:Latitude_)

      character(len=*), parameter:: NameSub = 'write_flux_time'
      !------------------------------------------------------------------------
      ! If there are more than one processors working on the same field line,
      ! we only save the data for the first nLineAll processors.
      if(nProc > nLineAll .and. iProc >= nLineAll) RETURN

      nFluxVar = File_I(iFile) % nFluxVar

      ! set header
      StringHeader = &
           'MFLAMPA: flux data on a sphere at fixed heliocentric distance; '//&
           'Coordindate system: '//trim(TypeCoordSystem)//'; '&
           //trim(File_I(iFile)%StringHeaderAux)

      ! set file name
      call make_file_name( &
           StringBase    = NameFluxData,                   &
           Radius        = File_I(iFile) % Radius,         &
           Longitude     = File_I(iFile) % Lon,            &
           Latitude      = File_I(iFile) % Lat,            &
           NameExtension = File_I(iFile)%NameFileExtension,&
           NameOut       = NameFile)

      ! at first call, remove files if they exist to reset time series output
      if(File_I(iFile) % IsFirstCall)then
         ! mark that the 1st call has already happened
         File_I(iFile) % IsFirstCall = .false.
         if(iProc==0) call remove_file(NameFile)
      end if

      ! reset data line counter
      nDataLine = 0

      if(iProc==0)then
         ! if file already exists -> read its content
         inquire(FILE=NameFile, EXIST=IsPresent)
         if(IsPresent)then
            ! first, determine its size
            call read_plot_file(&
                 NameFile   = NameFile, &
                 TypeFileIn = File_I(iFile) % TypeFile, &
                 n1Out      = nDataLine)
            ! if buffer is too small then reallocate it
            nBufferSize = ubound(File_I(iFile) % Buffer_II, DIM=2)
            if(nBufferSize < nDataLine + 1)then
               deallocate(File_I(iFile)%Buffer_II)
               allocate(File_I(iFile)%Buffer_II(nFluxVar+1, 2*nBufferSize, 1))
            end if

            ! read the data itself
            call read_plot_file(&
                 NameFile   = NameFile, &
                 TypeFileIn = File_I(iFile) % TypeFile, &
                 Coord1Out_I= File_I(iFile) % Buffer_II(1+nFluxVar, :, 1), &
                 VarOut_VI  = File_I(iFile) % Buffer_II(1:nFluxVar, :, 1))
         end if
      end if

      ! add new data
      nDataLine = nDataLine + 1
      ! reset the output buffer
      File_I(iFile) % Buffer_II(1:nFluxVar, nDataLine, 1) = 0.0
      ! put time into buffer
      File_I(iFile) % Buffer_II(nFluxVar+1, nDataLine, 1) = SPTime

      ! go over all lines on the processor and find the point of intersection
      ! with output sphere if present and compute contribution to fluxes
      do iLine = 1, nLine
         if(.not.Used_B(iLine)) CYCLE
         ! reset, the field line contrubtes unless fail to reach output sphere
         DoPrint = .true.

         ! find the particle just above the given radius
         call search_line(iLine, File_I(iFile)%Radius, iAbove, DoPrint, Weight)
         DoPrint = DoPrint .and. iAbove /= 1

         ! if no intersection found -> proceed to the next line
         if(.not.DoPrint) CYCLE

         ! interpolate data and fill buffer
         call get_normalized_spread(iLine, File_I(iFile)%Radius, &
              File_I(iFile)%Lon, File_I(iFile)%Lat, Spread)

         ! apply spread to excess fluxes above background/initial flux
         File_I(iFile) % Buffer_II(1:nFluxVar, nDataLine, 1) = &
              File_I(iFile)%Buffer_II(1:nFluxVar, nDataLine, 1) + Spread * (&
              Flux_VIB(Flux0_:FluxMax_, iAbove-1, iLine) * (1-Weight) + &
              Flux_VIB(Flux0_:FluxMax_, iAbove,   iLine) *    Weight  - &
              FluxChannelInit_V(Flux0_:FluxMax_))
      end do !  iLine

      ! gather interpolated data on the source processor
      if(nProc > 1) call MPI_reduce_real_array(File_I(iFile) % Buffer_II( &
           1:nFluxVar, nDataLine, 1), nFluxVar, MPI_SUM, 0, iComm, iError)
      if(iProc==0)then
         ! add background/initial flux back
         File_I(iFile) % Buffer_II(1:nFluxVar, nDataLine, 1) = &
              File_I(iFile)%Buffer_II(1:nFluxVar, nDataLine, 1) + &
              FluxChannelInit_V(Flux0_:FluxMax_)

         ! start time in seconds from base year
         Param_I(StartTime_)  = StartTime
         ! start time in Julian date
         Param_I(StartJulian_)= StartTimeJulian
         ! logitude and latitude of the location
         Param_I(Longitude_) = File_I(iFile) % Lon * cRadToDeg
         Param_I(Latitude_)  = File_I(iFile) % Lat * cRadToDeg

         ! reprint data to file
         call save_plot_file(&
              NameFile      = NameFile, &
              StringHeaderIn= StringHeader, &
              TypeFileIn    = File_I(iFile) % TypeFile, &
              nDimIn        = 1, &
              TimeIn        = SPTime,  &
              nStepIn       = iIter,   &
              ParamIn_I     = Param_I, &
              Coord1In_I    = &
              File_I(iFile) % Buffer_II(1+nFluxVar, 1:nDataLine, 1), &
              NameVarIn     = &
              trim(File_I(iFile) % NameVarPlot) // ' ' // &
              trim(File_I(iFile) % NameAuxPlot), &
              VarIn_VI      = &
              File_I(iFile) % Buffer_II(1:nFluxVar, 1:nDataLine, 1))
      end if

    end subroutine write_flux_time
    !==========================================================================
    subroutine write_flux_xy0(iVarPlane)

      ! write file with fluxes in the format to be read by IDL/TECPLOT;
      ! single file is created for the {x/y}=0 plane, and the name format is
      ! Flux_{x/y}=0_t<ddhhmmss>_n<iIter>.{out/dat}
      ! name of the output file

      use SP_ModTriangulate, ONLY: UsePoleTri, &
           intersect_surf, build_trmesh, interpolate_trmesh
      use SP_ModChannel, ONLY: distr_to_flux
      use ModCoordTransform, ONLY: rlonlat_to_xyz
      integer, intent(in) :: iVarPlane
      real :: Distr_II(0:nP+1,nMu), Xyz_D(nDim)
      ! get the radius and latitude of each shell
      real, allocatable :: Radius_I(:), Latitude_I(:)
      real :: dR, dLogR, dLat
      real :: Longitude_I(2)
      ! header of the output file
      character(len=500):: StringHeader
      character(len=100):: NameFile
      ! loop variables
      integer :: iR, iLat, iLon, iVarPlot, iStencil_I(3)
      ! for better readability
      integer :: nRCut, nLatCut, nMhdVar, nFluxVar, iVarLon
      ! additional parameters
      integer, parameter:: StartTime_  = 1
      integer, parameter:: StartJulian_= StartTime_ + 1
      real :: Param_I(1:StartJulian_)
      character(len=*), parameter:: NameSub = 'write_flux_xy0'
      !------------------------------------------------------------------------
      nMhdVar   = File_I(iFile)%nMhdVar
      nFluxVar  = File_I(iFile)%nFluxVar
      iVarLon   = 1
      nRCut     = File_I(iFile)%nRangeCut_I(1)
      nLatCut   = File_I(iFile)%nRangeCut_I(2)

      select case(iVarPlane)
      case(X_)
         ! header for the output file: x=0
         StringHeader = 'MFLAMPA: flux data in the x=0 plane;'//&
              ' Coordindate system: '//trim(TypeCoordSystem)//'; '&
              //trim(File_I(iFile)%StringHeaderAux)
         ! set the file name: x=0
         call make_file_name( &
              StringBase    = NameFluxData//'_x=0',              &
              Time          = SPTime,                            &
              iIter         = iIter,                             &
              NameExtension = File_I(iFile) % NameFileExtension, &
              NameOut       = NameFile)
         ! Set Longitude_I: 90 and 270 deg
         Longitude_I = [90.0, 270.0]
      case(Y_)
         ! header for the output file: y=0
         StringHeader = 'MFLAMPA: flux data in the y=0 plane;'//&
              ' Coordindate system: '//trim(TypeCoordSystem)//'; '&
              //trim(File_I(iFile)%StringHeaderAux)
         ! set the file name: y=0
         call make_file_name( &
              StringBase    = NameFluxData//'_y=0',              &
              Time          = SPTime,                            &
              iIter         = iIter,                             &
              NameExtension = File_I(iFile) % NameFileExtension, &
              NameOut       = NameFile)
         ! Set Longitude_I: 0 and 180 deg
         Longitude_I = [0.0, 180.0]
      case default
         call CON_stop('Unknown iVarPlane '// &
              NameVar_V(iVarPlane)//'=0 in'//NameSub)
      end select

      ! radius & longitude of each shell
      allocate(Radius_I(nRCut))
      dR = (File_I(iFile)%RMaxCut - File_I(iFile)%RMinCut)/real(nRCut)
      dLogR = log(File_I(iFile)%RMaxCut/File_I(iFile)%RMinCut)/real(nRCut)
      do iR = 1, nRCut
         ! get the radius: combination of linear and exponential growth
         Radius_I(iR) = 0.5*File_I(iFile)%RMinCut * exp(iR*dLogR) &
              + 0.5*(File_I(iFile)%RMinCut + (real(iR)-1.0)*dR)
      end do
      allocate(Latitude_I(nLatCut))
      dLat = (File_I(iFile)%LatMaxCut - File_I(iFile)%LatMinCut)/real(nLatCut)
      do iLat = 1, nLatCut
         Latitude_I(iLat) = File_I(iFile)%LatMinCut + (iLat - 0.5)*dLat
      end do

      ! include the pole for triangulation just in case we need it
      UsePoleTri = .true.

      ! reset the output buffer
      File_I(iFile) % Buffer_II = 0.0
      do iR = 1, nRCut
         call intersect_surf(Radius_I(iR))
         if(iProc/=0) CYCLE
         call build_trmesh()
         do iLon = 1, size(Longitude_I)
            do iLat = 1, nLatCut
               call rlonlat_to_xyz([Radius_I(iR), &
                    Longitude_I(iLon)*cDegToRad, &
                    Latitude_I(iLat)*cDegToRad], Xyz_D)
               ! save the Lon(=90/270 or 0/180 deg), Y/X and Z coordinates
               File_I(iFile)%Buffer_II(iVarLon, &
                    iLat+(iLon-1)*nLatCut, iR) = Longitude_I(iLon)
               File_I(iFile)%Buffer_II(iVarLon+1, &
                    iLat+(iLon-1)*nLatCut, iR) = &
                    Xyz_D(File_I(iFile)%iVarMhd_V(1))
               File_I(iFile)%Buffer_II(iVarLon+2, &
                    iLat+(iLon-1)*nLatCut, iR) = &
                    Xyz_D(File_I(iFile)%iVarMhd_V(2))
               ! interpolate the distribution function
               call interpolate_trmesh(Xyz_D, DistrInterp_II = Distr_II, &
                    iStencilOut_I=iStencil_I)
               if(all(iStencil_I==-1)) then
                  File_I(iFile)%Buffer_II(iVarLon+nMhdVar+1: &
                       iVarLon+nMhdVar+nFluxVar, iLat, iR) = &
                       FluxChannelInit_V(Flux0_:FluxMax_)
               else
                  call distr_to_flux(Distr_II(1:nP, :), &
                       File_I(iFile)%Buffer_II(iVarLon+nMhdVar+1: &
                       iVarLon+nMhdVar+nFluxVar, iLat, iR))
               end if
            end do
         end do
      end do !  iR on iProc

      ! start time in seconds from base year
      Param_I(StartTime_)   = StartTime
      ! start time in Julian date
      Param_I(StartJulian_) = StartTimeJulian

      ! print data to file
      if(iProc==0) &
           call save_plot_file( &
           NameFile      = NameFile, &
           StringHeaderIn= StringHeader, &
           TypeFileIn    = File_I(iFile) % TypeFile, &
           nDimIn        = 2, &
           TimeIn        = SPTime, &
           nStepIn       = iIter, &
           ParamIn_I     = Param_I(StartTime_:StartJulian_), &
           Coord1In_I    = [Latitude_I, Latitude_I], &
           Coord2In_I    = Radius_I, &
           NameVarIn     = &
           trim(File_I(iFile) % NameVarPlot) // ' ' // &
           trim(File_I(iFile) % NameAuxPlot), &
           VarIn_VII     = File_I(iFile) % Buffer_II)
      deallocate(Radius_I, Latitude_I)

    end subroutine write_flux_xy0
    !==========================================================================
    subroutine write_flux_z0

      ! write file with fluxes in the format to be read by IDL/TECPLOT;
      ! single file is created for the z=0 plane, and the name format is
      ! Flux_z=0_t<ddhhmmss>_n<iIter>.{out/dat}
      ! name of the output file

      use SP_ModTriangulate, ONLY: &
           intersect_surf, build_trmesh, interpolate_trmesh
      use SP_ModChannel, ONLY: distr_to_flux
      use ModCoordTransform, ONLY: rlonlat_to_xyz
      real :: Distr_II(0:nP+1,nMu), Xyz_D(nDim)
      ! get the radius and longitude of each shell
      real, allocatable :: Radius_I(:), Longitude_I(:)
      real :: dR, dLogR
      ! header of the output file
      character(len=500):: StringHeader
      character(len=100):: NameFile
      ! loop variables
      integer :: iR, iLon, iVarPlot, iStencil_I(3)
      ! for better readability
      integer :: nRCut, nLonCut, nMhdVar, nFluxVar
      ! additional parameters
      integer, parameter:: StartTime_  = 1
      integer, parameter:: StartJulian_= StartTime_ + 1
      real :: Param_I(1:StartJulian_)
      character(len=*), parameter:: NameSub = 'write_flux_z0'
      !------------------------------------------------------------------------
      nMhdVar   = File_I(iFile)%nMhdVar
      nFluxVar  = File_I(iFile)%nFluxVar
      nRCut     = File_I(iFile)%nRangeCut_I(1)
      nLonCut   = File_I(iFile)%nRangeCut_I(2)

      ! header for the output file
      StringHeader = 'MFLAMPA: flux data in the z=0 plane;'//&
           ' Coordindate system: '//trim(TypeCoordSystem)//'; '&
           //trim(File_I(iFile)%StringHeaderAux)

      ! set the file name
      call make_file_name( &
           StringBase    = NameFluxData//'_z=0',              &
           Time          = SPTime,                            &
           iIter         = iIter,                             &
           NameExtension = File_I(iFile) % NameFileExtension, &
           NameOut       = NameFile)

      ! radius & longitude of each shell
      allocate(Radius_I(nRCut))
      dR = (File_I(iFile)%RMaxCut - File_I(iFile)%RMinCut)/real(nRCut)
      dLogR = log(File_I(iFile)%RMaxCut/File_I(iFile)%RMinCut)/real(nRCut)
      do iR = 1, nRCut
         ! get the radius: combination of linear and exponential growth
         Radius_I(iR) = 0.5*File_I(iFile)%RMinCut * exp(iR*dLogR) &
              + 0.5*(File_I(iFile)%RMinCut + (real(iR)-1.0)*dR)
      end do
      allocate(Longitude_I(nLonCut))
      do iLon = 1, nLonCut
         Longitude_I(iLon) = (iLon - 0.5)*360.0/nLonCut
      end do

      ! reset the output buffer
      File_I(iFile) % Buffer_II = 0.0
      do iR = 1, nRCut
         call intersect_surf(Radius_I(iR))
         if(iProc/=0) CYCLE
         call build_trmesh()
         do iLon = 1, nLonCut
            call rlonlat_to_xyz([Radius_I(iR), &
                 Longitude_I(iLon)*cDegToRad, 0.0], Xyz_D)
            ! save the X and Y coordinates
            File_I(iFile)%Buffer_II(1, iLon, iR) = &
                 Xyz_D(File_I(iFile)%iVarMhd_V(1))
            File_I(iFile)%Buffer_II(2, iLon, iR) = &
                 Xyz_D(File_I(iFile)%iVarMhd_V(2))
            ! interpolate the distribution function
            call interpolate_trmesh(Xyz_D, DistrInterp_II = Distr_II, &
                 iStencilOut_I=iStencil_I)
            if(all(iStencil_I==-1)) then
               File_I(iFile)%Buffer_II(nMhdVar+1:nMhdVar+nFluxVar, iLon, iR)= &
                    FluxChannelInit_V(Flux0_:FluxMax_)
            else
               call distr_to_flux(Distr_II(1:nP, :), File_I(iFile)%Buffer_II( &
                    nMhdVar+1:nMhdVar+nFluxVar, iLon, iR))
            end if
         end do
      end do !  iR on iProc

      ! start time in seconds from base year
      Param_I(StartTime_)   = StartTime
      ! start time in Julian date
      Param_I(StartJulian_) = StartTimeJulian

      ! print data to file
      if(iProc==0) &
           call save_plot_file( &
           NameFile      = NameFile, &
           StringHeaderIn= StringHeader, &
           TypeFileIn    = File_I(iFile) % TypeFile, &
           nDimIn        = 2, &
           TimeIn        = SPTime, &
           nStepIn       = iIter, &
           ParamIn_I     = Param_I(StartTime_:StartJulian_), &
           Coord1In_I    = Longitude_I, &
           Coord2In_I    = Radius_I, &
           NameVarIn     = &
           trim(File_I(iFile) % NameVarPlot) // ' ' // &
           trim(File_I(iFile) % NameAuxPlot), &
           VarIn_VII     = File_I(iFile) % Buffer_II)
      deallocate(Radius_I, Longitude_I)

    end subroutine write_flux_z0
    !==========================================================================
    subroutine write_shock_2d

      ! write output with shock surface skeleton in the format to
      ! be read and visualized by IDL/TECPLOT; name format is
      ! MH_data_shock_t<ddhhmmss>_n<iIter>.{out/dat}

      use SP_ModShock, ONLY: ShockID_, XShock_, CompRatio_, StateShock_VIB

      ! name of the output file
      character(len=100):: NameFile
      ! header of the output file
      character(len=500):: StringHeader
      ! loop variables
      integer :: iLine, iVarPlot, iVarIndex
      ! indexes of corresponding node, latitude and longitude
      integer :: iLineAll
      ! for better readability
      integer :: nExtraVar
      ! index of shock location on the field line
      integer :: iShock
      ! additional parameters
      integer, parameter:: StartTime_  = 1
      integer, parameter:: StartJulian_= StartTime_ + 1
      integer :: nParam
      real    :: Param_I(1:StartJulian_)

      character(len=*), parameter:: NameSub = 'write_shock_2d'
      !------------------------------------------------------------------------

      nExtraVar = File_I(iFile)%nExtraVar
      nParam = StartJulian_

      ! header for the output file
      StringHeader = &
           'MFLAMPA: shock surface coordinates; Coordindate system: '//&
           trim(TypeCoordSystem)//'; '//trim(File_I(iFile)%StringHeaderAux)

      ! set the file name
      call make_file_shock_name( &
           StringBase    = NameMHData,                      &
           iIter         = iIter,                           &
           NameExtension = File_I(iFile)%NameFileExtension, &
           NameOut       = NameFile)

      ! reset the output buffer
      File_I(iFile) % Buffer_II = 0.0

      ! go over all lines on the processor and find the shock index
      do iLine = 1, nLine
         if(.not.Used_B(iLine)) CYCLE
         iLineAll = iLineAll0 + iLine

         ! assign Shock location
         ! for ShockID_:
         iVarPlot = 1
         iVarIndex = File_I(iFile) % iVarExtra_V(iVarPlot)
         iShock = iShock_IB(Shock_, iLine)
         File_I(iFile) % Buffer_II(iVarIndex, iLineAll, 1) = real(iShock)
         ! for {X,Y,Z,R,Lat,Lon}Shock_ and CompRatio_:
         iVarPlot = iVarPlot + XShock_ - ShockID_
         iVarIndex = File_I(iFile) % iVarExtra_V(iVarPlot)
         File_I(iFile) % Buffer_II(iVarIndex:iVarIndex+CompRatio_-XShock_, &
              iLineAll, 1) = StateShock_VIB(XShock_:CompRatio_, iLine)
      end do !  iLine

      ! gather interpolated data on the source processor
      if(nProc > 1) call MPI_reduce_real_array(File_I(iFile) % Buffer_II, &
           nLineAll*nExtraVar, MPI_SUM, 0, iComm, iError)

      ! start time in seconds from base year
      Param_I(StartTime_)   = StartTime
      ! start time in Julian date
      Param_I(StartJulian_) = StartTimeJulian

      ! print data to file
      if(iProc==0) &
           call save_plot_file(&
           NameFile      = NameFile, &
           StringHeaderIn= StringHeader, &
           TypeFileIn    = File_I(iFile) % TypeFile, &
           nDimIn        = 1, &
           TimeIn        = SPTime, &
           nStepIn       = iIter,  &
           ParamIn_I     = Param_I(1:nParam), &
           Coord1In_I    = real(iNodeIndex_I),&
           NameVarIn     = &
           trim(File_I(iFile) % NameVarPlot) // ' ' // &
           trim(File_I(iFile) % NameAuxPlot), &
           VarIn_VI      = &
           File_I(iFile) % Buffer_II(1:nExtraVar,1:nLineAll,1))

    end subroutine write_shock_2d
    !==========================================================================
    subroutine write_shock_time

      ! write output w/time series of the shock surface skeleton in the
      ! format to be read and visualized by IDL/TECPLOT; name format is
      ! MH_data_shock_<iLon>_<iLat>.{out/dat}

      use SP_ModShock, ONLY: ShockID_, XShock_, CompRatio_, StateShock_VIB

      ! name of the output file
      character(len=100):: NameFile
      ! header of the output file
      character(len=500):: StringHeader
      ! loop variables
      integer :: iLine, iVarPlot, iVarIndex
      ! for better readability
      integer :: nExtraVar
      ! index of shock location on the field line
      integer :: iShock
      ! size of the already written data
      integer :: nDataLine
      ! current size of the buffer
      integer :: nBufferSize
      ! whether the output file already exists
      logical :: IsPresent
      ! additional parameters
      integer, parameter:: StartTime_  = 1
      integer, parameter:: StartJulian_= StartTime_ + 1
      integer :: nParam
      real    :: Param_I(1:StartJulian_)

      character(len=*), parameter:: NameSub = 'write_shock_time'
      !------------------------------------------------------------------------
      ! If there are more than one processors working on the same field line,
      ! we only save the data for the first nLineAll processors.
      if(nProc > nLineAll .and. iProc >= nLineAll) RETURN

      nExtraVar = File_I(iFile) % nExtraVar
      nParam = StartJulian_

      ! header for the output file
      StringHeader = &
           'MFLAMPA: shock surface evolved along a field line; '//&
           '; Coordindate system: '//trim(TypeCoordSystem)// &
           '; '//trim(File_I(iFile)%StringHeaderAux)

      ! at first call, remove files if they exist to reset time series output
      if(File_I(iFile) % IsFirstCall) then
         ! mark that the 1st call has already happened
         File_I(iFile) % IsFirstCall = .false.
         ! go over list of lines and remove file for each one
         do iLine = 1, nLine
            if(.not.Used_B(iLine)) CYCLE
            ! set the file name
            call make_file_shock_name( &
                 StringBase    = NameMHData,                      &
                 iLine         = iLine,                           &
                 NameExtension = File_I(iFile)%NameFileExtension, &
                 NameOut       = NameFile)
            call remove_file(NameFile)
         end do
      end if

      ! reset the output buffer
      File_I(iFile) % Buffer_II = 0.0

      ! go over all lines on the processor and find the shock index
      do iLine = 1, nLine
         if(.not.Used_B(iLine)) CYCLE

         ! set the file name
         call make_file_shock_name( &
              StringBase    = NameMHData,                      &
              iLine         = iLine,                           &
              NameExtension = File_I(iFile)%NameFileExtension, &
              NameOut       = NameFile)

         ! if file already exists -> read its content
         nDataLine = 0
         inquire(FILE=NameFile, EXIST=IsPresent)
         if(IsPresent) then
            ! first, determine its size
            call read_plot_file(&
                 NameFile   = NameFile, &
                 TypeFileIn = File_I(iFile) % TypeFile, &
                 n1Out      = nDataLine)
            ! if buffer is too small then reallocate it
            nBufferSize = ubound(File_I(iFile)%Buffer_II, DIM=2)
            if(nBufferSize < nDataLine + 1) then
               deallocate(File_I(iFile) % Buffer_II)
               allocate(File_I(iFile) % Buffer_II( &
                    1+nExtraVar, 2*nBufferSize, 1))
            end if

            ! read the data itself
            call read_plot_file(&
                 NameFile   = NameFile,                  &
                 TypeFileIn = File_I(iFile) % TypeFile,  &
                 Coord1Out_I= File_I(iFile) % Buffer_II(1+nExtraVar, :, 1), &
                 VarOut_VI  = File_I(iFile) % Buffer_II(1:nExtraVar, :, 1))
         end if

         ! add new data
         nDataLine = nDataLine + 1
         ! put time into buffer
         File_I(iFile) % Buffer_II(1+nExtraVar, nDataLine, 1) = SPTime
         ! get shock location
         iShock = iShock_IB(Shock_, iLine)
         ! for ShockID_:
         iVarPlot = 1
         iVarIndex = File_I(iFile) % iVarExtra_V(iVarPlot)
         File_I(iFile) % Buffer_II(iVarIndex, nDataLine, 1) = real(iShock)
         ! for {X,Y,Z,R,Lat,Lon}Shock_ and CompRatio_:
         iVarPlot = iVarPlot + XShock_ - ShockID_
         iVarIndex = File_I(iFile) % iVarExtra_V(iVarPlot)
         File_I(iFile) % Buffer_II(iVarIndex:iVarIndex+CompRatio_-XShock_, &
              nDataLine, 1) = StateShock_VIB(XShock_:CompRatio_, iLine)

         ! start time in seconds from base year
         Param_I(StartTime_)  = StartTime
         ! start time in Julian date
         Param_I(StartJulian_)= StartTimeJulian

         ! reprint data to file
         call save_plot_file(&
              NameFile      = NameFile, &
              StringHeaderIn= StringHeader, &
              TypeFileIn    = File_I(iFile) % TypeFile, &
              nDimIn        = 1, &
              TimeIn        = SPTime, &
              nStepIn       = iIter, &
              ParamIn_I     = Param_I, &
              Coord1In_I    = &
              File_I(iFile) % Buffer_II(1+nExtraVar, 1:nDataLine, 1), &
              NameVarIn     = &
              trim(File_I(iFile) % NameVarPlot) // ' ' // &
              trim(File_I(iFile) % NameAuxPlot), &
              VarIn_VI      = &
              File_I(iFile) % Buffer_II(1:nExtraVar, 1:nDataLine, 1))
      end do !  iLine

    end subroutine write_shock_time
    !==========================================================================
    logical function do_skip(iLine, iRange_I)

      use SP_ModGrid, ONLY: iblock_to_lon_lat, iLonOffset, iLatOffset
      integer, intent(in) :: iLine, iRange_I(iLonMin_:iLatMax_)
      integer :: iLon, iLat
      !------------------------------------------------------------------------
      call iblock_to_lon_lat(iLine, iLon, iLat)
      iLon = iLon - iLonOffset
      iLat = iLat - iLatOffset
      do_skip = any([iLon,iLat] < iRange_I(iLonMin_:iLatMin_:2)).or.&
           any([iLon,iLat] > iRange_I(iLonMax_:iLatMax_:2))
    end function do_skip
    !==========================================================================
  end subroutine save_plot_all
  !============================================================================
  subroutine finalize

    use ModUtilities, ONLY: cTab
    use SP_ModGrid, ONLY: nLat, nLon

    ! write the header file that contains necessary information
    ! for reading input files in a separate run

    character(len=*), parameter:: NameSub = 'finalize'
    !--------------------------------------------------------------------------
    if(.not.DoWriteHeader) RETURN
    ! performed on root proc only
    if(iProc/=0) RETURN
    ! write the header file
    call open_file(file=trim(NamePlotDir)//trim(NameHeaderFile), &
         NameCaller=NameSub)
    write(UnitTmp_,*)
    write(UnitTmp_,'(a)')'#CHECKGRIDSIZE'
    write(UnitTmp_,'(i8,a)') nVertexMax, cTab//cTab//'nVertexMax'
    write(UnitTmp_,'(i8,a)') nLat,       cTab//cTab//'nLat'
    write(UnitTmp_,'(i8,a)') nLon,       cTab//cTab//'nLon'
    write(UnitTmp_,*)
    write(UnitTmp_,'(a)')'#MHDATA'
    write(UnitTmp_,'(a)') trim(TypeMHDataFile)//cTab//cTab//'TypeFile'
    write(UnitTmp_,'(i8,a)') nTag,              cTab//cTab//'nFileRead'
    write(UnitTmp_,'(a)') trim(NameTagFile)//cTab//cTab//'NameTagFile'
    write(UnitTmp_,*)
    write(UnitTmp_,'(a)')'#END'
    write(UnitTmp_,*)
    call close_file

  end subroutine finalize
  !============================================================================
  subroutine get_time_string(Time, StringTime)

    ! the subroutine converts real variable Time into a string,
    ! the structure of the string is 'ddhhmmss',
    ! i.e shows number of days, hours, minutes and seconds
    ! after the beginning of the simulation

    real,             intent(in) :: Time
    character(len=8), intent(out):: StringTime

    !--------------------------------------------------------------------------
    StringTime = '99999999'  ! This is the value if the time is too large

    if(Time < 100.0*86400) &
         write(StringTime,'(i2.2,i2.2,i2.2,i2.2)') &
         int(                  Time          /86400.), & ! # days
         int((Time-(86400.*int(Time/86400.)))/ 3600.), & ! # hours
         int((Time-( 3600.*int(Time/ 3600.)))/   60.), & ! # minutes
         int( Time-(   60.*int(Time/   60.)))            ! # seconds

  end subroutine get_time_string
  !============================================================================
  subroutine get_date_time_string(Time, StringTime)

    use ModTimeConvert, ONLY: time_real_to_int
    ! the subroutine converts real variable Time into a string,
    ! the structure of the string is 'ddhhmmss',
    ! i.e shows number of days, hours, minutes and seconds
    ! after the beginning of the simulation
    real,              intent(in) :: Time
    character(len=15), intent(out):: StringTime
    integer :: iTime_I(7)
    !--------------------------------------------------------------------------
    call time_real_to_int(StartTime+Time, iTime_I)
    write(StringTime,'(i4.4,i2.2,i2.2,a,i2.2,i2.2,i2.2)')&
         iTime_I(1:3),'_',iTime_I(4:6)

  end subroutine get_date_time_string
  !============================================================================
  subroutine make_file_name(StringBase, Radius, Longitude, Latitude, &
       iLine, iIter, Time, NameExtension, NameOut)

    ! creates a string with file name and stores in NameOut;
    ! result is as follows:
    !   StringBase[_R=?.?][_Lon=?.?_Lat=?.?][_???_???][_t?_n?].NameExtension
    ! parts in [] are written if present: Radius, iLineAll, iIter
    use SP_ModGrid, ONLY: iblock_to_lon_lat
    character(len=*),   intent(in) :: StringBase
    real,    optional,  intent(in) :: Radius
    real,    optional,  intent(in) :: Longitude
    real,    optional,  intent(in) :: Latitude
    integer, optional,  intent(in) :: iLine
    integer, optional,  intent(in) :: iIter
    real,    optional,  intent(in) :: Time
    character(len=*),   intent(in) :: NameExtension
    character(len=100), intent(out):: NameOut

    ! timetag
    character(len=15)  :: StringTime
    ! write format
    character(len=100) :: StringFmt
    ! lon, lat indexes corresponding to iLineAll
    integer:: iLon, iLat

    !--------------------------------------------------------------------------
    write(NameOut,'(a)') trim(NamePlotDir)//trim(StringBase)

    if(present(Radius)) write(NameOut,'(a,i4.4,f0.2)') &
         trim(NameOut)//'_R_', int(Radius), Radius - int(Radius)

    if(present(Longitude) .and. present(Latitude))then
       ! avoid rounding issue due to conversion from degree to radians and back
       if(abs(nint(Longitude*cRadToDeg)-Longitude*cRadToDeg) < cTolerance)then
          write(NameOut,'(a,i3.3,a)') &
               trim(NameOut)//'_Lon_', nint(Longitude*cRadToDeg), '.00'
       else
          write(NameOut,'(a,i3.3,f0.2)') &
               trim(NameOut)//'_Lon_', int(Longitude*cRadToDeg),&
               Longitude*cRadToDeg - int(Longitude*cRadToDeg)
       end if

       if(abs(nint(Latitude*cRadToDeg)-Latitude*cRadToDeg) < cTolerance)then
          if(Latitude < 0.0)then
             write(StringFmt,'(a)') '(a,i3.2,a)'
          else
             write(StringFmt,'(a)') '(a,i2.2,a)'
          end if
          write(NameOut,StringFmt) &
               trim(NameOut)//'_Lat_', nint(Latitude*cRadToDeg), '.00'
       else
          if(Latitude < 0.0)then
             write(StringFmt,'(a)') '(a,i3.2,f0.2)'
          else
             write(StringFmt,'(a)') '(a,i2.2,f0.2)'
          end if
          write(NameOut,StringFmt) &
               trim(NameOut)//'_Lat_', int(Latitude*cRadToDeg),&
               abs(Latitude*cRadToDeg - int(Latitude*cRadToDeg))
       end if
    end if

    if(present(iLine))then
       call iblock_to_lon_lat(iLine, iLon, iLat)
       write(NameOut,'(a,i3.3,a,i3.3)') trim(NameOut)//'_',iLon,'_',iLat
    end if

    if(present(iIter))then
       if(UseDateTime)then
          call get_date_time_string(SPTime, StringTime)
          write(NameOut,'(a,i6.6)') &
               trim(NameOut)//'_e'//StringTime//'_n', iIter
       else
          call get_time_string(SPTime, StringTime(1:8))
          write(NameOut,'(a,i6.6)') &
               trim(NameOut)//'_t'//StringTime(1:8)//'_n', iIter
       end if
    elseif(present(Time))then
       if(UseDateTime)then
          call get_date_time_string(Time, StringTime)
          write(NameOut,'(a,i6.6)') &
               trim(NameOut)//'_e'//StringTime
       else
          call get_time_string(Time, StringTime(1:8))
          write(NameOut,'(a,i6.6)') &
               trim(NameOut)//'_t'//StringTime(1:8)
       end if
    end if
    write(NameOut,'(a)') trim(NameOut)//trim(NameExtension)

  end subroutine make_file_name
  !============================================================================
  subroutine make_file_shock_name(StringBase, &
       iLine, iIter, NameExtension, NameOut)

    ! creates a string with file name for the shock and stores in NameOut;
    ! result is as follows:
    !   StringBase_Shock[_???_???][_t?_n?].NameExtension
    ! parts in [] are written if present: Radius, iLineAll, iIter
    use SP_ModGrid, ONLY: iblock_to_lon_lat
    character(len=*),   intent(in) :: StringBase
    integer, optional,  intent(in) :: iLine
    integer, optional,  intent(in) :: iIter
    character(len=*),   intent(in) :: NameExtension
    character(len=100), intent(out):: NameOut

    ! timetag
    character(len=15)  :: StringTime
    ! write format
    character(len=100) :: StringFmt
    ! lon, lat indexes corresponding to iLineAll
    integer:: iLon, iLat

    !--------------------------------------------------------------------------
    write(NameOut,'(a)') trim(NamePlotDir)//trim(StringBase)//'_Shock'

    if(present(iLine))then
       call iblock_to_lon_lat(iLine, iLon, iLat)
       write(NameOut,'(a,i3.3,a,i3.3)') trim(NameOut)//'_',iLon,'_',iLat
    end if

    if(present(iIter))then
       if(UseDateTime)then
          call get_date_time_string(SPTime, StringTime)
          write(NameOut,'(a,i6.6)') &
               trim(NameOut)//'_e'//StringTime//'_n', iIter
       else
          call get_time_string(SPTime, StringTime(1:8))
          write(NameOut,'(a,i6.6)') &
               trim(NameOut)//'_t'//StringTime(1:8)//'_n', iIter
       end if
    end if

    write(NameOut,'(a)') trim(NameOut)//trim(NameExtension)

  end subroutine make_file_shock_name
  !============================================================================
end module SP_ModPlot
!==============================================================================
