program PostPwIdl
  
  ! Read PW lines and create 2D altitude slices

  use ModPlotFile, ONLY: save_plot_file, read_plot_file
  use ModNumConst, ONLY: cDegToRad,cPi
  use ModTriangulateSpherical,ONLY:trmesh, trplot, find_triangle_sph,trprnt
  implicit none

  ! This is copied from ModKind, because PostIDL.exe may be compiled with
  ! different precision then the rest of the codes.
  integer, parameter :: Real4_=selected_real_kind(6,30)
  integer, parameter :: Real8_=selected_real_kind(12,100)
  integer, parameter :: nByteReal = 4 + (1.00000000041 - 1.0)*10000000000.0
  
  integer, parameter :: UnitTmp_=99, nDimIn = 1, nDimOut = 2
  integer, parameter :: x_=1,y_=2,z_=3, Lat_=1, Lon_=2
!  character(len=20) ::  TypePlot='ascii'
  character(len=20) ::  TypePlot='real8'
  real, parameter   :: rEarth=6375.0
  logical, parameter:: UseDipole=.true.
  integer :: nLine, nAltOut, nAlt, nTime, iLine, iTime, nVar, nParam, nStep
  character(len=100), allocatable :: NameFile_I(:)
  character(len=100) :: NameHeader, Type, NameOut, NamePlotVar
  real, allocatable :: PlotState_IV(:,:), PlotState_IIIV(:,:,:,:), &
       Coord1_III(:,:,:), Coord2_III(:,:,:), Coord3_III(:,:,:), &
       Coord_I(:),Param_I(:)
  real :: theta0,theta, phi, Time,L,r
  integer :: iVar,iAlt
  integer :: iTheta, iPhi, iThetaTmp,iPhiTmp
  
  
  !for triangulation
  integer, allocatable :: list1_I(:),lptr1_I(:), &
       lend1_I(:)
  integer :: iError

  real, allocatable :: CoordXyzPw1_DII(:,:,:)
  
  !for interpolation
  real :: Area1, Area2, Area3, Xyz_D(3)
  logical :: IsTriangleFound
  integer ::iNode1, iNode2, iNode3
  
  !for output grid
  real,allocatable :: CoordAltOut_I(:),CoordThetaOut_I(:),CoordPhiOut_I(:)
  integer,parameter :: nTheta=20, nPhi=180
  real,allocatable :: PlotStateOut_CV(:,:,:,:),CoordXyzOut_CV(:,:,:,:)
  real :: dPhi, dTheta

  integer, parameter :: iTimeOut=200
  
  !------------------------------------------------------------------------------
  
  
   ! Read information from STDIN
  read(*,'(a)') TypePlot
  read(*,'(i5)')nLine
  read(*,'(i5)')nTime

  ! Allocate and fill filename array
  allocate(NameFile_I(nLine))
  do iLine=1,nLine
     read(*,'(a)') NameFile_I(iLine)
  end do
  
  ! Read Header from the first file to get nVar and nAlt
  call read_plot_file(NameFile_I(1), n1Out = nAlt, nVarOut = nVar,    &
       TypeFileIn=TypePlot,StringHeaderOut = NameHeader,              & 
       NameVarOut = NamePlotVar, nStepOut= nStep,TimeOut=Time,        &
       nParamOut=nParam,iUnitIn = UnitTmp_)  

  !Now allocate PlotState and coord arrays
  allocate (PlotState_IV(nAlt,nVar),PlotState_IIIV(nTime,nLine,nAlt,nVar))
  allocate (Coord1_III(nTime,nAlt,nLine), Coord2_III(nTime,nAlt,nLine), &
       Coord3_III(nTime,nAlt,nLine), Param_I(nParam), Coord_I(nAlt))

  !open file for writing
 ! select case(TypePlot)
 ! case('formatted','ascii')
 ! case('real8','real4')
 !    open(UnitTmp_, file=NameOut, status='replace',form='unformatted')
 ! end select

  NamePlotVar(1:9) = 'Y X Z Lon'
  
  !set up arrays for triangulation
  allocate( &
       CoordXyzPw1_DII(3,nLine,nTime), &
       list1_I(6*(nLine-2)), &
       lptr1_I(6*(nLine-2)), &
       lend1_I(nLine))
  
  ! Initialize arrays to zero
  CoordXyzPw1_DII = 0
  list1_I = 0
  lend1_I = 0
  lptr1_I = 0
  
  
  
  do iLine = 1, nLine
     do iTime=1,nTime 
        write(*,*) NameFile_I(iLine),itime
        call read_plot_file(NameFile_I(iLine), ParamOut_I=Param_I,          &
             TypeFileIn=TypePlot, nStepOut= nStep,TimeOut=Time,             &
             CoordOut_I = Coord_I, VarOut_IV = PlotState_IV,                &
             iUnitIn = UnitTmp_)
        
        ! Fill PlotState_IIV
        PlotState_IIIV(iTime,iLine, :, :) = PlotState_IV(:, :)
        
        ! Replace Lat,r, and Lon with x, y and z
        theta0 = (90.0-PlotState_IV(1, Lat_))*cDegToRad
        phi   = PlotState_IV(1, Lon_)       *cDegToRad
        L=(1.0/sin(theta0))**2.0
        !y goes into Coord1, x goes into Coord2
        if (UseDipole) then
           do iAlt=1,nAlt
              r=(rEarth+Coord_I(iAlt))/rEarth
              theta=cPi/2.0-acos(min(sqrt((r*(sin(theta0))**2.0)),1.0))

              Coord1_III(iTime,iAlt,iLine) = -r * sin(theta)*sin(phi)
              Coord2_III(iTime,iAlt,iLine) =  r * sin(theta)*cos(phi)
              Coord3_III(iTime,iAlt,iLine) =  r * cos(theta)
           enddo
        else
           Coord1_III(iTime,iAlt,iLine) = -1.0 * sin(theta0)*sin(phi)
           Coord2_III(iTime,iAlt,iLine) =  1.0 * sin(theta0)*cos(phi)
           Coord3_III(iTime,iAlt,iLine) = Coord_I(iAlt)/rEarth
        endif

        !for output time extract triangulation 
        CoordXyzPw1_DII(x_,iLine,iTime) = sin(theta0)*cos(phi)
        CoordXyzPw1_DII(y_,iLine,iTime) = sin(theta0)*sin(phi)
        CoordXyzPw1_DII(z_,iLine,iTime) = cos(theta0)
        
        !PlotState_IIV(iLine, 1, x_) = 1.0 * sin(theta)*cos(phi)
     enddo
     close(UnitTmp_)
  enddo

  write(*,*) 'Finished data read'

  !fill CoordXyzPw1_DI with field line positions on unit sphere 
  
  !Construct a triangulation on the unit sphere
  call trmesh ( nLine, CoordXyzPw1_DII(x_,1:nLine,iTimeOut), &
         CoordXyzPW1_DII(y_,1:nLine,iTimeOut), CoordXyzPW1_DII(z_,1:nLine,iTimeOut), &
         list1_I, lptr1_I, lend1_I, iError )

  if ( iError == -2 ) then
     write(*,*)&
          ' WARNING: Error in TRMESH, First three nodes are collinear'
     stop
  else if ( iError > 0 ) then
     write(*,*)&
          ' ERROR: Error in TRMESH, Duplicate nodes encountered'
     stop
  end if

!  call trprnt ( nLine, CoordXyzPw1_DII(x_,1:nLine,iTimeOut), &
!       CoordXyzPW1_DII(y_,1:nLine,iTimeOut), CoordXyzPW1_DII(z_,1:nLine,iTimeOut), 0,&
!       list1_I, lptr1_I, lend1_I)
!  
!  !write out plot of triangulation for test
!  open ( UnitTmp_, file = 'TestTriangulation.eps' )
!  call trplot ( UnitTmp_, 7.5, 90.0, 0.0, 30.0, nLine, &
!       CoordXyzPw1_DII(x_,1:nLine,iTimeOut), &
!       CoordXyzPw1_DII(y_,1:nLine,iTimeOut), &
!       CoordXyzPw1_DII(z_,1:nLine,iTimeOut), list1_I, lptr1_I, &
!       lend1_I, 'test1 triangulation',.true., iError )
!  close(UnitTmp_)
  
  write(*,*) 'Finished triangulation'
  
  !setup output grid
  allocate(CoordAltOut_I(nAlt),CoordThetaOut_I(nTheta),CoordPhiOut_I(nPhi))
  CoordAltOut_I=Coord_I
  dTheta = 40.0/nTheta
  write(*,*)'testa'
  do iTheta=1,nTheta
     write(*,*) iTheta
     CoordThetaOut_I(iTheta) = (dTheta*real(iTheta))*cDegToRad
  enddo
  write(*,*)'test1'
  
  dPhi = 360.0/nPhi
  do iPhi=1,nPhi
     CoordPhiOut_I(iPhi) = dPhi*real(iPhi)*cDegToRad
  enddo

  allocate(CoordXyzOut_CV(nAlt,nTheta,nPhi,3))
  !set xyz of output grid
  do iAlt=1,nAlt
     do iTheta=1,nTheta
        do iPhi=1,nPhi
           r=(rEarth+CoordAltOut_I(iAlt))/rEarth
           !theta=cPi/2.0 &
           !     - acos(min(sqrt((r*(sin(CoordThetaOut_I(iTheta)))**2.0)),1.0))
           theta = CoordThetaOut_I(iTheta)
           phi=CoordPhiOut_I(iPhi)
           
           CoordXyzOut_CV(iAlt,iTheta,iPhi,x_) =  r*sin(theta)*cos(phi)
           CoordXyzOut_CV(iAlt,iTheta,iPhi,y_) =  r*sin(theta)*sin(phi)
           CoordXyzOut_CV(iAlt,iTheta,iPhi,z_) =  r*cos(theta)
        enddo
     enddo
  enddo
  write(*,*) 'Finished Output Grid Creation'
  
  !allocate array to hold interpolated output
  allocate (PlotStateOut_CV(nAlt,nTheta,nPhi,nVar))
  
  !loop over output grid and interpolate
  do iAlt=1,nAlt
     do iTheta=1,nTheta
        do iPhi=1,nPhi
           theta=CoordThetaOut_I(iTheta)
           phi=CoordPhiOut_I(iPhi)
           !find xyz on the unit sphere
           Xyz_D(x_)= sin(theta)*cos(phi)
           Xyz_D(y_)= sin(theta)*sin(phi)
           Xyz_D(z_)= cos(theta)

           
           !Find triangle containing point Xyz_D and get interpolation weights
           Area1=0
           Area2=0
           Area3=0
           call find_triangle_sph(Xyz_D, nLine, &
                CoordXyzPw1_DII(:,1:nLine,iTimeOut), &
                list1_I, lptr1_I, lend1_I, Area1, Area2, Area3, IsTriangleFound,&
                iNode1,iNode2,iNode3)
           
           !interpolation when foundtriangle otherwise set to 0
           if (IsTriangleFound) then
              PlotStateOut_CV(iAlt,iTheta,iPhi,:)=&
                   Area1*PlotState_IIIV(iTimeOut,iNode1, iAlt, :)+ &
                   Area2*PlotState_IIIV(iTimeOut,iNode2, iAlt, :)+ &
                   Area3*PlotState_IIIV(iTimeOut,iNode3, iAlt, :)
           else
              PlotStateOut_CV(iAlt,iTheta,iPhi,:)=0.0
           endif

        enddo
     enddo
  enddo

    write(*,*) 'Finished interpolation'
  ! write out new plotfile
  
        ! write out coordinates and variables line by line
  
  !  do iTime=1,nTime
  write(NameOut,"(a,i8.8,a)") &
       'plots/3DPw',iTimeOut,'.dat'
  open(UnitTmp_, file=NameOut, status='replace')
  write(UnitTmp_,'(a)') &
       'VARIABLES = "X [R]", "Y [R]", "Z [R]", "Lat", "Lon", "uO", "uH", "uHe", "ue", "lgnO", "lgnH", '//&
       '"lgnHe", "lgne", "TO", "TH", "THe", "Te", "MO", "MH", "MHe", "Me", "Ef", "Pe"' 
  write(UnitTmp_,'(a,i3,a,i3,a,i9,a)') 'Zone I=', nPhi+1, ', J=', nTheta+1,&
       ', K=',nAlt,', DATAPACKING=POINT'
  do iAlt=1,nAlt
     do iTheta=0,nTheta
        do iPhi=1,nPhi+1
           if (iPhi==nPhi+1) then
              if (iTheta==0) then
                 !write theta and phi ghost cell
                 iPhiTmp=mod(iPhi+nPhi/2,nPhi)+1
                 write(UnitTmp_, "(100es18.10)") &
                      CoordXyzOut_CV(iAlt,1,iPhiTmp,x_),&
                      CoordXyzOut_CV(iAlt,1,iPhiTmp,y_),&
                      CoordXyzOut_CV(iAlt,1,iPhiTmp,z_),&
                      PlotStateOut_CV(iAlt,1,iPhiTmp,:)
              else
                 !write only phi ghost cell
                 write(UnitTmp_, "(100es18.10)") &
                      CoordXyzOut_CV(iAlt,iTheta,1,x_),&
                      CoordXyzOut_CV(iAlt,iTheta,1,y_),&
                      CoordXyzOut_CV(iAlt,iTheta,1,z_),&
                      PlotStateOut_CV(iAlt,iTheta,1,:)
              endif
           else
              if (iTheta==0) then
                 !write only theta ghost cell
                 iPhiTmp=mod(iPhi+nPhi/2,nPhi)+1
                 write(UnitTmp_, "(100es18.10)") &
                      CoordXyzOut_CV(iAlt,1,iPhiTmp,x_),&
                      CoordXyzOut_CV(iAlt,1,iPhiTmp,y_),&
                      CoordXyzOut_CV(iAlt,1,iPhiTmp,z_),&
                      PlotStateOut_CV(iAlt,1,iPhiTmp,:)
              else
                 !write regular cells
                 write(UnitTmp_, "(100es18.10)") &
                      CoordXyzOut_CV(iAlt,iTheta,iPhi,x_),&
                      CoordXyzOut_CV(iAlt,iTheta,iPhi,y_),&
                      CoordXyzOut_CV(iAlt,iTheta,iPhi,z_),&
                      PlotStateOut_CV(iAlt,iTheta,iPhi,:)
              endif
           end if
        end do
     enddo
  enddo
  close(UnitTmp_)
  !enddo
  
 !    case('real8','real4')
 !       write(UnitTmp_)  trim(NameHeader)
 !       write(UnitTmp_)  nStep, Time, -2, 1, nVar
 !       write(UnitTmp_)  nLine,1
 !       write(UnitTmp_)  Param_I
 !       write(UnitTmp_)  trim(NamePlotVar)
 !       
 !       ! write out coordinates and variables line by line
 !       do iLine=1,nLine
 !          write(UnitTmp_) &
 !               Coord1_I(iLine), Coord2_I(iLine),PlotState_IIV(iLine,1,:) 
 !       end do
 !          
 !    end select
        
     
     

!
!   call save_plot_file(NameOut, TypePositionIn='rewind',     &
!             TypeFileIn=TypePlot,StringHeaderIn = NameHeader,               & 
!             NameVarIn = NamePlotVar, nStepIn= nStep,TimeIn=Time,           &
!             nDimIn=2,Coord1In_I = Coord1_I,Coord2In_I = Coord2_I,         &
!             VarIn_IIV = PlotState_IIV, ParamIn_I = Param_I,                &
!             IsCartesianIn=.false.)
!     else
!        call save_plot_file(NameOut, TypePositionIn='append',     &
!             TypeFileIn=TypePlot,StringHeaderIn = NameHeader,               & 
!             NameVarIn = NamePlotVar, nStepIn= nStep,TimeIn=Time,           &
!             nDimIn=2,Coord1In_I = Coord1_I,Coord2In_I = Coord2_I,         &
!             VarIn_IIV = PlotState_IIV, ParamIn_I = Param_I,                &
!             IsCartesianIn=.false.)
!     end if
  
  

end program PostPwIdl
!=============================================================================

subroutine CON_stop(String)

  ! This routine is needed for ModPlotFile

  implicit none

  character(len=*), intent(in):: String
  write(*,*) 'ERROR in PostIDL: '//String
  stop

end subroutine CON_stop
