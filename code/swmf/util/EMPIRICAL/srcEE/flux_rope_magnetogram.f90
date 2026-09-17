!  Copyright (C) 2002 Regents of the University of Michigan,
!  portions used with permission
!  For more information, see http://csem.engin.umich.edu/tools/swmf

program magnetogram

  use EEE_ModMain, ONLY:  EEE_set_parameters, EEE_get_state_bc, &
       EEE_set_plot_range, EEE_initialize
  use EEE_ModCommonVariables, ONLY: &
       prefix, x_, y_, z_, DXyzPlot, Si2Io_V, &
       LongitudeCme, LatitudeCme, OrientationCME, DoAddFluxRope, &
       DirCme_D, UnitB_
  use ModConst, ONLY: cPi, cTwoPi, cDegToRad, cRadToDeg
  use ModPlotFile, ONLY: save_plot_file
  use ModReadParam, ONLY: read_file, read_init, read_line, read_command
  use ModUtilities, ONLY: CON_stop
  use ModCoordTransform, ONLY: rot_xyz_rlonlat, rlonlat_to_xyz, &
       rot_xyz_mercator, rot_matrix_z
  use ModMpi, ONLY: MPI_COMM_SELF, MPI_init, MPI_finalize
  use ModMagnetogram, ONLY: iTableB0, init_magnetogram_lookup_table, &
       get_magnetogram_field
  use ModLookupTable, ONLY: read_lookup_table_param, get_lookup_table
  use ModIoUnit, ONLY:  io_unit_new

  implicit none

  ! Number of indexes for magnetogram map
  integer, parameter:: nLong = 360, nLat = 180
  ! Number of indexes for 3D plot
  integer           :: nXY = -1, nZ = -1
  ! Plot variables:
  real, allocatable :: Var_VN(:,:,:,:)
  ! Named indexes, maay be redefined depending on the use of potential and/or
  ! EE field
  integer:: B0x_ = 1, B0z_ = 3, B1x_ = 1, B1z_ = 3
  ! Rotational matrix of coordinate transformation:
  real              :: Rotate_DD(3,3)

  ! Loop indexes
  integer           :: i, j, k, iUnit, iError
  real              :: Long0
  real              :: Longitude, Latitude
  real              :: Longitude_I(nLong), Latitude_I(nLat)
  real              :: BSurface_DC(3,nLong,nLat) = 0
  real              :: XyzRLonLat_DD(3,3)
  real              :: Xyz_D(3), Rho, p, U_D(3), B_D(3)
  character(LEN=3)  :: TypeLatAxis
  character(LEN=100):: StringLine, NameCommand, NameVar, NameUnits
  character(LEN=*), parameter::  NameParam =&
       '  LongitudeCme LatitudeCme OrientationCme DXyz', String3Gs = 'Gs Gs Gs'
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
        write(*,*)'Reading CME Parameters'
        call EEE_set_parameters(NameCommand)
     case('#LOOKUPTABLE')
        call read_lookup_table_param
        call init_magnetogram_lookup_table
     case('#MAGNETOGRAM')
        ! Do nothing
     case default
        call CON_stop('Unknown command:'//NameCommand)
     end select
  end do READPARAM
  iUnit  = io_unit_new()
  if(DoAddFluxRope)then
     call MPI_init(iError)
     call EEE_initialize(1.5e8, 1.5e6, &
          gamma=5.0/3.0, iCommIn=MPI_COMM_SELF, TimeNow=0.0)
     open(iUnit, file='RunFRM', STATUS='old', IOSTAT=iError)
     if(iError==0)call make_frm_magnetogram
  end if
  ! Start 3D plot and 2D slices
  call EEE_set_plot_range(nXY, nZ)
  if(nXY < 1 .and. nZ < 1) call CON_stop('nXY or nZ less than 1')
  write(*,*)'nXY, nZ = ', nXY, nZ
  if(iTableB0 > 0.and.DoAddFluxRope)then
     B0x_   = 4; B0z_ = 6
     allocate(Var_VN(B1x_:B0z_, 2*nXY+1, 2*nXY+1, nZ+1))
     NameVar= 'b1x b1y b1z b0x b0y b0z'//NameParam
     NameUnits = String3Gs//' '//String3Gs
  elseif(iTableB0 > 0)then
     B0x_   = 1; B0z_ = 3
     allocate(Var_VN(B0x_:B0z_, 2*nXY+1, 2*nXY+1, nZ+1))
     NameVar= 'b0x b0y b0z'//NameParam
     NameUnits = String3Gs
  elseif(DoAddFluxRope)then

     NameVar= 'b1x b1y b1z'//NameParam
     NameUnits = String3Gs
     allocate(Var_VN(B1x_:B1z_, 2*nXY+1, 2*nXY+1, nZ+1))
  else
     call CON_stop('No 3D plots')
  end if
  ! Calculate rotation  matrix from  the CME  frame
  Rotate_DD  = matmul(rot_xyz_mercator(DirCme_D), &
       rot_matrix_z(OrientationCme*cDegToRad))
  do k = 1, nZ + 1
     do j = 1, 2*nXY + 1
        do i = 1, 2*nXY + 1
           Xyz_D = DirCme_D + DXyzPlot*matmul(Rotate_DD, &
                [i - 1 - nXY, j - 1 - nXY, k - 1] )
           if(DoAddFluxRope)then
              call EEE_get_state_bc(Xyz_D, Rho, U_D, B_D, p, &
                   Time=0.0, nStep=0, nIter=0)
              B_D = B_D*Si2Io_V(UnitB_)
              Var_VN(B1x_:B1z_,i,j,k) = matmul(B_D, Rotate_DD)
           end if
           if(iTableB0  > 0)then
              call get_magnetogram_field(Xyz_D, B_D)
              ! Convert Tesla to Gs:
              Var_VN(B0x_:B0z_,i,j,k) = 1e4*matmul(B_D, Rotate_DD)
           end if
        end do
     end do
  end do
  call save_plot_file( &
       NameFile = 'FRM_x=0.out', &
       TypeFileIn = 'ascii', &
       StringHeaderIn = 'Flux rope field and/or potential field.', &
       nDimIn = 2, &
       CoordMinIn_D = DXyzPlot*[-nXY, 0], &
       CoordMaxIn_D = DXyzPlot*[ nXY,nZ], &
       NameVarIn = 'y z '//NameVar, &
       NameUnitsIn = 'Rs Rs '//NameUnits, &
       VarIn_VII = Var_VN(B1x_:B0z_,nXY+1,:,:), &
       ParamIn_I = [LongitudeCme, LatitudeCme, OrientationCme, DXyzPlot])
  call save_plot_file( &
       NameFile = 'FRM_y=0.out', &
       TypeFileIn = 'ascii', &
       StringHeaderIn = 'Flux rope field and/or potential field.', &
       nDimIn = 2, &
       CoordMinIn_D = DXyzPlot*[-nXY, 0], &
       CoordMaxIn_D = DXyzPlot*[ nXY,nZ], &
       NameVarIn = 'x z '//NameVar, &
       NameUnitsIn = 'Rs Rs '//NameUnits, &
       VarIn_VII = Var_VN(B1x_:B0z_,:,nXY+1,:), &
       ParamIn_I = [LongitudeCme, LatitudeCme, OrientationCme, DXyzPlot] )
  call save_plot_file( &
       NameFile = 'FRM_z=0.out', &
       TypeFileIn = 'ascii', &
       StringHeaderIn = 'Flux rope field and/or potential field.', &
       nDimIn = 2, &
       CoordMinIn_D = DXyzPlot*[-nXY, -nXY], &
       CoordMaxIn_D = DXyzPlot*[ nXY,  nXY], &
       NameVarIn = 'x y '//NameVar,&
       NameUnitsIn = 'Rs Rs '//NameUnits, &
       VarIn_VII = Var_VN(B1x_:B0z_,:,:,1), &
       ParamIn_I = [LongitudeCme, LatitudeCme, OrientationCme, DXyzPlot] )
  call save_plot_file( &
       NameFile = 'FRM_3d.dat', &
       TypeFileIn = 'tec', &
       StringHeaderIn = 'Flux rope field and/or potential field.', &
       nDimIn = 3, &
       CoordMinIn_D = DXyzPlot*[-nXY, -nXY,  0], &
       CoordMaxIn_D = DXyzPlot*[ nXY,  nXY, nZ], &
       NameVarIn = 'x y z '//NameVar, &
       NameUnitsIn = 'Rs Rs Rs '//NameUnits, &
       VarIn_VIII = Var_VN, &
       ParamIn_I = [LongitudeCme, LatitudeCme, OrientationCme, DXyzPlot])

  if(DoAddFluxRope)call MPI_finalize(iError)

contains
  !============================================================================
  subroutine  make_frm_magnetogram

    ! Makes a map of the EE generated field to superpose on the obbserved
    ! magnetogram. Parameters of the magnetogram on which to superpose
    ! (longitude of left margin and the use of uniform or sin(lat) grid
    ! are red from RunFRM file. Is not invoked if the file does not exist
    !--------------------------------------------------------------------------
    read(iUnit,*) Long0
    write(*,'(a,f6.1)')prefix, Long0
    read(iUnit,*)TypeLatAxis
    write(*,'(a)')prefix//TypeLatAxis
    close(iUnit)
    do i = 1, nLong
       Longitude_I(i) = i - 0.5
    end do
    do i = 1, nLat
       select case(TypeLatAxis)
       case('uni')
          Latitude_I(i) = i - 90.5
       case('sin')
          Latitude_I(i) = asin((2.0*i - nLat - 1.0)/nLat)*cRadToDeg
       case default
       end select
    end do

    do j = 1, nLat
       Latitude = Latitude_I(j)*cDegToRad
       do i = 1,nLong
          ! Include the Phishift to correctly place the CME
          Longitude = (Longitude_I(i) + Long0)*cDegToRad
          call rlonlat_to_xyz(1.0, Longitude, Latitude, Xyz_D)
          call EEE_get_state_BC(Xyz_D, Rho, U_D, B_D, p, &
               Time=0.0, nStep=0, nIter=0)
          B_D = B_D*Si2Io_V(UnitB_)
          ! Convert to Br, BLon, BLat components
          XyzRLonLat_DD = rot_xyz_rlonlat(Lon=Longitude, Lat=Latitude)
          BSurface_DC(:,i,j) = matmul(B_D, XyzRLonLat_DD)
       end do
    enddo

    write(*,*)'Saving 2d Flux Rope output file'
    call save_plot_file( &
         NameFile = 'FRMagnetogram.out', &
         TypeFileIn = 'ascii', &
         StringHeaderIn = 'Flux rope magnetic field at 1 R_s', &
         nDimIn = 2, &
         ParamIn_I = [Long0], &
         CoordIn_I = Longitude_I, &
         Coord2In_I= Latitude_I, &
         NameVarIn = 'Longitude Latitude Br Blon Blat Long0', &
         NameUnitsIn = 'Deg Deg Gs Gs Gs', &
         VarIn_VII = BSurface_DC)

  end subroutine make_frm_magnetogram
  !============================================================================
end program magnetogram
!==============================================================================
