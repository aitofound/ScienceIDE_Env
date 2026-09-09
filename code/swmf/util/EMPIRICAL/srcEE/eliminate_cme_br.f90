!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf
program magnetogram

  use EEE_ModMain, ONLY:  EEE_set_parameters, EEE_get_state_BC, &
       EEE_set_plot_range, EEE_initialize
  use EEE_ModCommonVariables, ONLY: &
       prefix, x_, y_, z_, DXyzPlot, Si2Io_V, &
       LongitudeCme, LatitudeCme, DoAddFluxRope, UnitB_
  use ModConst, ONLY: cPi, cTwoPi, cDegToRad, cRadToDeg
  use ModPlotFile, ONLY:save_plot_file, read_plot_file
  use ModReadParam, ONLY: read_file, read_init, read_var, read_line, &
       read_command
  use ModUtilities, ONLY: CON_stop
  use ModCoordTransform, ONLY: rlonlat_to_xyz
  use ModMpi, ONLY: MPI_COMM_SELF, MPI_init, MPI_finalize
  ! use ModLookupTable, ONLY: read_lookup_table_param, get_lookup_table
  use ModIoUnit, ONLY:  io_unit_new
  use ModUtilities, ONLY: split_string, join_string
  implicit none
  ! Number of indexes for magnetogram map
  integer :: nLong = 360, nLat = 180
  ! Magnetogram variables
  real, allocatable :: Var_VII(:,:,:), Longitude_I(:), Latitude_I(:), &
       BrCme_II(:,:)
  real :: Param_I(10)
  ! Loop indexes    ::
  integer           :: i, j, iUnit, iError, Long0, nVar, nParam, iVar
  real              :: Longitude, Latitude, Xyz_D(3), Rho, p, U_D(3), B_D(3),&
       Time
  character(LEN=100):: StringLine, NameCommand
  ! The file name for the original magnetogram and the transformed ones
  character(LEN=100):: NameMagnetogram     = 'map_01.out'
  character(LEN=100):: NameEliminatedCmeBr = 'map_01.out'
  character(LEN=400):: StringHeader
  ! Array for variable names
  integer, parameter:: MaxVar = 200
  character(len=20) :: NameVar_I(MaxVar)
  character(len=500):: NameVar
  !----------------------------------------------------------------------------
  write(*,'(a)')prefix//'Reading CME.in'
  call read_file('CME.in', iCommIn = MPI_COMM_SELF)
  call read_init
  READPARAM: do
     if(.not.read_line(StringLine) ) EXIT READPARAM
     if(.not.read_command(NameCommand)) CYCLE READPARAM
     select case(NameCommand)
     case('#END')
        EXIT READPARAM
     case('#CME')
        write(*,'(a)')prefix//'Reading CME Parameters'
        call EEE_set_parameters(NameCommand)
     case('#LOOKUPTABLE')
        ! Do nothing
     case('#MAGNETOGRAM')
        call read_var('NameMagnetogram',NameMagnetogram)
        write(*,'(a)')prefix//'Process magnetogram file='//NameMagnetogram
     case('#ELIMINATECMEBR')
        call read_var('NameEliminatedCmeBr',NameEliminatedCmeBr)
     case default
        call CON_stop(prefix//'Unknown command:'//NameCommand)
     end select
  end do READPARAM
  iUnit  = io_unit_new()
  if(.not.DoAddFluxRope)STOP
  call MPI_init(iError)
  call EEE_initialize(1.5e8, 1.5e6, &
       gamma=5.0/3.0, iCommIn=MPI_COMM_SELF, TimeNow=0.0)
  call read_plot_file(NameFile = NameMagnetogram, &
       iUnitIn   = iUnit,                  &
       n1Out     = nLong,                  &
       n2Out     = nLat,                   &
       NameVarOut= NameVar,                &
       nVarOut   = nVar,                   &
       StringHeaderOut = StringHeader,     &
       nParamOut = nParam,                 &
       ParamOut_I= Param_I,                &
       TimeOut   = Time,                   &
       iErrorOut = iError)
  if(iError>0)call CON_stop(prefix//'Cannot open file '//NameMagnetogram)
  write(*,*)prefix//'nLong=', nLong,' nLat=', nLat, ' nVar=', nVar,&
       ' nParam=', nParam, ' Time=', Time
  Long0 = nint(Param_I(1))
  write(*,*)prefix//'Long0=', Long0, ' Param_I=', Param_I(1:nParam)
  ! Allocate coord arrays
  allocate(Longitude_I(nLong), Latitude_I(nLat))
  ! Allocate data arrays
  allocate(Var_VII(nVar+1,nLong,nLat), BrCme_II(nLong,nLat))
  call read_plot_file(NameFile = NameMagnetogram, &
       iUnitIn   = iUnit,                  &
       Coord1Out_I   = Longitude_I,        &
       Coord2Out_I   = Latitude_I,         &
       VarOut_VII    = Var_VII(1:nVar,:,:))
  write(*,*)prefix//'Start to calculate CME Br'
  do j = 1,nLat
     Latitude = Latitude_I(j)*cDegToRad
     do i = 1,nLong
        ! Include the Phishift to correctly place the CME
        Longitude = (Longitude_I(i) + Long0)*cDegToRad
        call rlonlat_to_xyz(1.0,Longitude,Latitude, Xyz_D)
        call EEE_get_state_BC(Xyz_D, Rho, U_D, B_D, p, &
             Time = 0.0, nStep  = 0, nIter=0)
        BrCme_II(i,j) = sum(B_D*Xyz_D)*Si2Io_V(UnitB_)
     end do
  enddo
  write(*,*)prefix//'Eliminate CME Br'
  ! Eliminate Br
  Var_VII(1,:,:) = Var_VII(1,:,:) - BrCme_II
  Var_VII(nVar+1,:,:) =  - BrCme_II
  write(*,*)'Saving 2d magnetogram with eliminated CME Br'
  call split_string(NameVar, MaxVar, NameVar_I, iVar)
  ! Add new data column
  NameVar_I(nVar+4:iVar+1) = NameVar_I(nVar+3:iVar)
  NameVar_I(nVar+3) = 'CmeBr'
  call join_string(iVar+1, NameVar_I(1:iVar+1), NameVar)
  call save_plot_file(&
       NameFile = NameEliminatedCmeBr,&
       TypeFileIn = 'ascii',          &
       StringHeaderIn = StringHeader, &
       TimeIn = Time,                 &
       nDimIn = 2,                    &
       ParamIn_I = Param_I(1:nParam), &
       Coord1In_I= Longitude_I,       &
       Coord2In_I= Latitude_I,        &
       NameVarIn = NameVar,           &
       VarIn_VII = Var_VII)
  call MPI_finalize(iError)
end program magnetogram
!==============================================================================
