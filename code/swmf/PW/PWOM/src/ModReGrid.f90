!module to hold routines for regriding the pwom grid
Module ModReGrid
  use ModUtilities,    ONLY: CON_stop
  use ModMpi
  use ModPWOM, only: nTotalLine,iProc, nProc, iComm, &
       ThetaLine_I, PhiLine_I, nLine,iLineGlobal_I=>iLineGlobal,State_CVI,Time,&
       GeoMagLon_I,GeoMagLat_I,xLine_I,yLine_I,zLine_I,xLineOld_I,yLineOld_I,&
       zLineOld_I, rLowerBoundary, UseParticles
  implicit none
  private !except

  !a variable type to hold escential information about a line
  Type :: line
     !processor corresponding to global line number
     integer :: iProc
     
     !local line index on its proc
     integer :: iLineLocal

     !coordinate on unit sphere
     real :: Xyz_D(3)

     !line coordinate
     real :: Theta, Phi
     real :: Lat, Lon

     !Is this a north or South line? 
     logical :: IsNorth

     !number of particles on the line
     integer :: nParticle

     !Is this line an anchor point for the grid edge
     logical :: IsAnchor=.false.
   contains
     procedure :: calc_xyz => line_calc_xyz
     
  end type line

  type(line),allocatable :: Lines_I(:)
  
  integer,   allocatable :: nLine_P(:)

  integer, parameter :: X_=1, Y_=2, Z_=3

  integer :: nNorth=0, nSouth=0
  !index array to get North and South Lines
  integer,allocatable :: iIndexNorth_I(:),iIndexSouth_I(:)

  !triangulation vars
  integer, allocatable :: listN_I(:),lptrN_I(:), lendN_I(:),&
       listS_I(:), lptrS_I(:), lendS_I(:)  

  !the remap grid
  integer :: nRemapPointN=-1,nRemapPointS=-1
  real,allocatable :: RemapThetaN_I(:),RemapPhiN_I(:)
  real,allocatable :: RemapThetaS_I(:),RemapPhiS_I(:)
  
  Type :: map
     !global line index of nodes of triangle for interpolation
     integer :: iNode1, iNode2, iNode3

     !local line index (on it's proc) of nodes of triangle for interpolation
     integer :: iNodeLocal1, iNodeLocal2, iNodeLocal3

     !number of particles on each line
     integer :: nParticle1, nParticle2, nParticle3

     !proc holding each node
     integer :: iProc1, iProc2, iProc3

     !interpolation weight for each node
     real :: weight1, weight2, weight3

     !location of new line
     real :: Theta, Phi

     !Global index of line to be replaced in remap
     integer :: iLineGlobal

     !local index of line to be replaced in remap
     integer :: iLineLocal

     !Proc to hold remapped line
     integer :: iProc
  end type map

  ! map arrays for the current remap plan for north and south
  type(map),allocatable :: RemapN_I(:), RemapS_I(:)

  !number of points to remap
  integer :: nPointsToRemapN, nPointsToRemapS

  logical, public :: DoSavePoints=.false.
  logical, public :: DoRegrid=.false.
  real   , public :: DtRegrid = 600.0

  !vars for anchor points
  integer,parameter    :: nAnchorZones = 16

  !vars for adapting the remap grid
  logical, public :: DoAdaptGrid = .true.
  integer, public :: nAdaptPointsN = 30
  integer, public :: nAdaptPointsS = 30

  !the base remap grid which is uniformally distrubted over the cap
  integer :: nRemapPointBaseN=-1,nRemapPointBaseS=-1
  real,allocatable :: RemapThetaBaseN_I(:),RemapPhiBaseN_I(:)
  real,allocatable :: RemapThetaBaseS_I(:),RemapPhiBaseS_I(:)

  !triangulation vars for base remap grid
  integer, allocatable :: listBaseN_I(:),lptrBaseN_I(:), lendBaseN_I(:),&
       listBaseS_I(:), lptrBaseS_I(:), lendBaseS_I(:)
  

  !adaptive remap criteria on a theta,phi grid
  real, allocatable :: AdaptCriteria_G(:,:)
  integer :: nThetaAdapt=-1, nPhiAdapt=-1
  real :: DphiAdapt, DthetaAdapt
  real,allocatable  :: PhiAdapt_G(:,:), ThetaAdapt_G(:,:)
  Character(len=10),public :: TypeAdaptCriteria='Jr'
  
  !main calling routine is public
  public :: regrid_lines
  public :: update_remap_criteria
contains
  !type bound proceedure to update line xyz
  subroutine line_calc_xyz(this)
    use ModNumConst, ONLY: cRadToDeg
    class(line) :: this
    !--------------------------------------------------------------------------
    this%Xyz_D(X_) = sin(this%Theta)*cos(this%Phi)
    this%Xyz_D(Y_) = sin(this%Theta)*sin(this%Phi)
    this%Xyz_D(Z_) = cos(this%Theta)

    if (this%Xyz_D(Z_) > 0) then
       this%IsNorth = .true.
    else
       this%IsNorth = .false.
    endif

    this%Lat = 90.0 - cRadToDeg*this%Theta
    this%Lon = cRadToDeg*this%Phi
  end subroutine line_calc_xyz
  !============================================================================
  ! initialize the regriding by gathering field line info to iproc0
  ! and building list of north and south lines
  subroutine init_regrid
    use ModParticle, only:create_particle_mpi_data_type, get_particles_line
    integer ::iError, iProcList, iLine, iLineGlobal,iNorth,iSouth,nParticleLine
    ! MPI status variable
    integer :: iStatus_I(MPI_STATUS_SIZE)
    !--------------------------------------------------------------------------
    
    if (.not.allocated(Lines_I)) allocate(Lines_I(nTotalLine))

    if (.not.allocated(nLine_P)) allocate(nLine_P(0:nProc-1))

    if(UseParticles) then
       call create_particle_mpi_data_type
    endif
    
    if (iProc > 0) then
       !send number of lines on iProc
       call MPI_send(nLine,1,MPI_INTEGER,0,1,iComm,iError)
              
       !loop over lines to send other line characteristics
       do iLine=1,nLine
          call MPI_send(iLineGlobal_I(iLine),1,MPI_INTEGER,0,2,iComm,iError)
          call MPI_send(ThetaLine_I(iLine),1,MPI_REAL,0,3,iComm,iError)
          call MPI_send(PhiLine_I(iLine),1,MPI_REAL,0,4,iComm,iError)

          if(UseParticles) then
             call get_particles_line(iLine,nParticleLine)
             call MPI_send(nParticleLine,1,MPI_INTEGER,0,5,iComm,iError)
          end if
       enddo
    else
       !loop over procs >1 and recieve
       if (nProc>0) then
          do iProcList = 1,nProc-1
             call MPI_recv(nLine_P(iProcList),1,MPI_INTEGER,iProcList,1,iComm,&
                  iStatus_I,iError)
             do iLine = 1,nLine_P(iProcList)
                call MPI_recv(iLineGlobal,1,MPI_INTEGER,iProcList,2,iComm,&
                     iStatus_I,iError)
                Lines_I(iLineGlobal)%iLineLocal=iLine
                Lines_I(iLineGlobal)%iProc=iProcList
                call MPI_recv(Lines_I(iLineGlobal)%Theta,1,MPI_REAL,&
                     iProcList,3,iComm,iStatus_I,iError)
                call MPI_recv(Lines_I(iLineGlobal)%Phi,1,MPI_REAL,&
                     iProcList,4,iComm,iStatus_I,iError)

                if(UseParticles) then
                   call MPI_recv(Lines_I(iLineGlobal)%nParticle,1,&
                        MPI_INTEGER,iProcList,5,iComm,iStatus_I,iError)
                end if
             enddo
          end do
       endif
       
       !now fill in proc=0 values
       nLine_P(0) = nLine
       do iLine=1,nLine
          Lines_I(iLineGlobal_I(iLine))%iLineLocal=iLine
          Lines_I(iLineGlobal_I(iLine))%iProc=0
          Lines_I(iLineGlobal_I(iLine))%Theta=ThetaLine_I(iLine)
          Lines_I(iLineGlobal_I(iLine))%Phi=PhiLine_I(iLine)

          if (UseParticles) then
             call get_particles_line(iLine,nParticleLine)
             Lines_I(iLineGlobal_I(iLine))%nParticle=nParticleLine
          endif
       enddo

       !loop over Lines_I and update the xyz and find total number of north and
       !south lines
       do iLine=1,nTotalLine
          call Lines_I(iLine)%calc_xyz

          if (Lines_I(iLine)%IsNorth) then
             nNorth=nNorth+1
          else
             nSouth=nSouth+1
          endif
       enddo

       !allocate index arrays for north and south lines
       if (.not.allocated(iIndexNorth_I) .and. nNorth>0) &
            allocate(iIndexNorth_I(nNorth))
       if (.not.allocated(iIndexSouth_I) .and. nSouth>0) &
            allocate(iIndexSouth_I(nSouth))

       !loop over lines and set index arrays for north and south lines
       iNorth=0
       iSouth=0
       do iLine=1,nTotalLine
          if (Lines_I(iLine)%IsNorth) then
             iNorth=iNorth+1
             iIndexNorth_I(iNorth)=iLine
          else
             iSouth=iSouth+1
             iIndexSouth_I(iSouth)=iLine
          endif
       enddo

       !allocate triangulation arrays
       if (nNorth>0) then
          allocate(listN_I(6*(nNorth-2)))
          allocate(lptrN_I(6*(nNorth-2)))
          allocate(lendN_I(6*(nNorth)))
       endif

       if (nSouth>0) then
          allocate(listS_I(6*(nSouth-2)))
          allocate(lptrS_I(6*(nSouth-2)))
          allocate(lendS_I(6*(nSouth)))
       endif
       
    endif

    !distrubute the umber of north and south lines
    call MPI_bcast(nNorth,1,MPI_INTEGER,0,iComm,iError)
    call MPI_bcast(nSouth,1,MPI_INTEGER,0,iComm,iError)

    if (iProc == 0) then
       !define the remap grid in each hemisphere
       if (nNorth>0) then
          if (.not.allocated(RemapThetaN_I)) then
             allocate(RemapThetaN_I(nNorth))
             allocate(RemapPhiN_I(nNorth))
          endif
          if (DoAdaptGrid) then
             call define_base_remap_grid(nNorth-nAdaptPointsN,.true.)
             !update the number of adapt points to include any left out of base
             nAdaptPointsN = nNorth-nRemapPointBaseN
             nRemapPointN=nNorth
             
             !Allocate triangulation for base remap grid
             !allocate triangulation arrays
             if (nRemapPointN>0) then
                allocate(listBaseN_I(6*(nRemapPointN-2)))
                allocate(lptrBaseN_I(6*(nRemapPointN-2)))
                allocate(lendBaseN_I(6*(nRemapPointN)))
             endif
             
             !add in adapt points
             !call adapt_remap_grid
             
          else
             call define_base_remap_grid(nNorth,.true.)
             nRemapPointN=nRemapPointBaseN
             RemapThetaN_I=RemapThetaBaseN_I
             RemapPhiN_I=RemapPhiBaseN_I
          endif
          
          
       endif
       
       if (nSouth>0) then
          if (.not.allocated(RemapThetaS_I)) then
             allocate(RemapThetaS_I(nSouth))
             allocate(RemapPhiS_I(nSouth))
          endif
          if (DoAdaptGrid) then
             call define_base_remap_grid(nSouth-nAdaptPointsS,.false.)
             !update the number of adapt points to include any left out of base
             nAdaptPointsS = nSouth-nRemapPointBaseS
             nRemapPointS=nSouth
             
             !Allocate triangulation for base remap grid
             !allocate triangulation arrays
             if (nRemapPointS>0) then
                allocate(listBaseS_I(6*(nRemapPointS-2)))
                allocate(lptrBaseS_I(6*(nRemapPointS-2)))
                allocate(lendBaseS_I(6*(nRemapPointS)))
             endif
             
             !add in adapt points
             !call adapt_remap_grid
          else
             call define_base_remap_grid(nSouth,.false.)
             nRemapPointS=nRemapPointBaseS
             RemapThetaS_I=RemapThetaBaseS_I
             RemapPhiS_I=RemapPhiBaseS_I
          endif
       endif

    endif

    !distrubute the umber of north and south remap points
    call MPI_bcast(nRemapPointN,1,MPI_INTEGER,0,iComm,iError)
    call MPI_bcast(nRemapPointS,1,MPI_INTEGER,0,iComm,iError)

    
  end subroutine init_regrid
  !============================================================================
  ! Define the initial base remap grid for a given hemisphere. Approach is to
  ! distribute points on the the spherical cap with as close to equal
  ! areas as possible. Note that it is possible that the number of remap point
  ! would be less than the number of available points. This is ok as it would
  ! mean that we remap to a fewer number of points. this should only be called
  ! by processor 0
  subroutine define_base_remap_grid(nPoint,IsNorth)
    use ModIoUnit, ONLY: UnitTmp_
    use ModNumConst, ONLY: cPi
    integer, intent(in) :: nPoint
    logical, intent(in) :: IsNorth
    integer :: iCount
    real :: CapArea, Area
    real :: dTheta, dPhi
    integer iTheta,iPhi, mTheta, mPhi
    real :: ThetaCap
    real :: rCap, hcap,theta,phi
    
    logical,parameter :: DoTest = .true.
    !---------------------------------------------------------------------------

    ThetaCap=40.0*cPi/180.0

    !allocate remap grid if not allocated
    if (IsNorth) then
       if (.not.allocated(RemapThetaBaseN_I)) then
          allocate(RemapThetaBaseN_I(nPoint))
          allocate(RemapPhiBaseN_I(nPoint))
       else
          call con_stop('PW ERROR: defining a base remap grid can only be called once')
       endif
    else
       if (.not.allocated(RemapThetaBaseS_I)) then
          allocate(RemapThetaBaseS_I(nPoint))
          allocate(RemapPhiBaseS_I(nPoint))
       else
          call con_stop('PW ERROR: defining a base remap grid can only be called once')
       endif
    endif
    
    !get cap area
    rCap = sin(ThetaCap)
    hCap = 1.0-cos(ThetaCap)
    CapArea = 2.0*cPi*(rCap**2+hCap**2)
        
    iCount=0
    Area=CapArea/nPoint/2
    mTheta = ceiling(ThetaCap/sqrt(Area))
    dTheta = ThetaCap/mTheta
    dPhi =Area/dTheta
    
    Theta_Loop: do iTheta = 0,mTheta-1
       theta = ThetaCap*(itheta+0.5)/mTheta
       mPhi = ceiling(2.0*cPi*sin(theta)/dPhi)
       do iPhi=0,mPhi-1
          phi = 2.0*cPi*iPhi/mPhi
          !write(*,*) theta, phi
          iCount=iCount+1
          if(IsNorth) then
             RemapThetaBaseN_I(iCount)=theta
             RemapPhiBaseN_I(iCount)=phi
             nRemapPointBaseN = iCount
          else
             RemapThetaBaseS_I(iCount)=cPi-1.0*theta
             RemapPhiBaseS_I(iCount)=phi
             nRemapPointBaseS = iCount
          endif
          
          !make sure you do not exceed the max number of points
          if (iCount==nPoint) exit Theta_Loop
       enddo
    enddo Theta_Loop
    
    !print to tecplot file for testing
    if(DoTest) then
       if(IsNorth) then
          open(UnitTmp_,file='PW/plots/RemapGridNorth.dat')
       else
          open(UnitTmp_,file='PW/plots/RemapGridSouth.dat')
       endif
       write(UnitTmp_,'(a)') &
            'VARIABLES = "X", "Y", "Z"'
       write(UnitTmp_,'(a,i3,a,i3,a)') 'Zone I=', iCount, ', DATAPACKING=POINT'
       if (IsNorth) then
          do iTheta=1,iCount
             write(UnitTmp_,"(100es18.10)") &
                  sin(RemapThetaBaseN_I(iTheta))*cos(RemapPhiBaseN_I(iTheta)), &
                  sin(RemapThetaBaseN_I(iTheta))*sin(RemapPhiBaseN_I(iTheta)), &
                  cos(RemapThetaBaseN_I(itheta))
          enddo
          close(UnitTmp_)
       else
          do iTheta=1,iCount
             write(UnitTmp_,"(100es18.10)") &
                  sin(RemapThetaBaseS_I(iTheta))*cos(RemapPhiBaseS_I(iTheta)), &
                  sin(RemapThetaBaseS_I(iTheta))*sin(RemapPhiBaseS_I(iTheta)), &
                  cos(RemapThetaBaseS_I(itheta))
          enddo
           close(UnitTmp_)
       endif
    endif
  end subroutine define_base_remap_grid

  !============================================================================
  ! routine to get triangulation for north and south base remap grids,
  ! this is only called on iProc0. This is needed for grid adaptation
  subroutine get_triangulation_base_remap_grid
    use ModTriangulateSpherical,ONLY:trmesh, trplot
    use ModIoUnit, ONLY: UnitTmp_
    real, allocatable :: xNorth_I(:),yNorth_I(:),zNorth_I(:)
    real, allocatable :: xSouth_I(:),ySouth_I(:),zSouth_I(:)
    integer :: iLine, iError,TimeOut
    real    :: Theta, Phi
    logical :: DoSaveTriangulate = .False.
    Character(len=100) :: NameFile
    character (len=*),parameter :: NameSub='get_triangulation_base_remap_grid'
    !--------------------------------------------------------------------------

    if (iProc>0) &
         call con_stop(NameSub//' called by iProc>0')
    
    !allocate x,y,z arrays for north and south
    if (.not.allocated(xNorth_I) .and. nRemapPointBaseN>0) &
         allocate(xNorth_I(nRemapPointBaseN))
    if (.not.allocated(yNorth_I) .and. nRemapPointBaseN>0) &
         allocate(yNorth_I(nRemapPointBaseN))
    if (.not.allocated(zNorth_I) .and. nRemapPointBaseN>0) &
         allocate(zNorth_I(nRemapPointBaseN))

    if (.not.allocated(xSouth_I) .and. nRemapPointBaseS>0) &
         allocate(xSouth_I(nRemapPointBaseS))
    if (.not.allocated(ySouth_I) .and. nRemapPointBaseS>0) &
         allocate(ySouth_I(nRemapPointBaseS))
    if (.not.allocated(zSouth_I) .and. nRemapPointBaseS>0) &
         allocate(zSouth_I(nRemapPointBaseS))

    
    !construct north triangulation
    if (nRemapPointBaseN>0) then
       !unpack positions 
       do iLine=1,nRemapPointBaseN
          Theta=RemapThetaBaseN_I(iLine)
          Phi=RemapPhiBaseN_I(iLine)
          
          xNorth_I(iLine) = sin(Theta)*cos(Phi)
          yNorth_I(iLine) = sin(Theta)*sin(Phi)
          zNorth_I(iLine) = cos(Theta)

       enddo

       !write(*,*) 'TestB', Time
       !write(*,*) nNorth
       !do iLine=1,nNorth
       !   write(*,*) iLine,xNorth_I(iLine),yNorth_I(iLine),zNorth_I(iLine)
       !enddo
       
       !create the triangulation
       call trmesh ( nRemapPointBaseN, xNorth_I, yNorth_I, zNorth_I, &
            listBaseN_I, lptrBaseN_I, lendBaseN_I, iError )

       if ( iError == -2 ) then
          write(*,*)NameSub, &
               ' WARNING: Error in TRMESH, First three nodes are collinear'
          call CON_stop(NameSub//' Problem With Triangulation')
       else if ( iError > 0 ) then
          write(*,*) 'ERROR: duplicate node iError=',iError
          write(*,*)NameSub// &
               ' ERROR: Error in TRMESH, Duplicate nodes encountered'
          call CON_stop(NameSub//' Problem With Triangulation')
       end if
    endif
    
    !construct north triangulation
    if (nSouth>0) then
       !unpack positions 
       do iLine=1,nRemapPointBaseS
          Theta=RemapThetaBaseS_I(iLine)
          Phi=RemapPhiBaseS_I(iLine)
          
          xSouth_I(iLine) = sin(Theta)*cos(Phi)
          ySouth_I(iLine) = sin(Theta)*sin(Phi)
          zSouth_I(iLine) = cos(Theta)
       enddo

       !create the triangulation
       call trmesh ( nRemapPointBaseS, xSouth_I, ySouth_I, zSouth_I, &
            listBaseS_I, lptrBaseS_I, lendBaseS_I, iError )

       if ( iError == -2 ) then
          write(*,*)NameSub, &
               ' WARNING: Error in TRMESH, First three nodes are collinear'
          call CON_stop(NameSub//' Problem With Triangulation')
       else if ( iError > 0 ) then
          write(*,*)NameSub// &
               ' ERROR: Error in TRMESH, Duplicate nodes encountered'
          call CON_stop(NameSub//' Problem With Triangulation')
       end if
    endif

    !write triangulation output if requested
    if (DoSaveTriangulate) then
       if (nRemapPointBaseN>0) then
          ! Northern Hemi
          TimeOut=int(Time)
          write(NameFile,"(a,i8.8,a)") &
               'PW/plots/TriangulationBaseNorth_',TimeOut,'.eps'
          open ( UnitTmp_, file = NameFile)
          call trplot ( UnitTmp_, 7.5, 90.0, 0.0, 90.0, nRemapPointBaseN, &
               xNorth_I, yNorth_I, zNorth_I, listBaseN_I, lptrBaseN_I, &
               lendBaseN_I, 'test1 triangulation',.true., iError )
          close(UnitTmp_)
       endif
       if (nRemapPointBaseS>0) then
          ! Southern Hemi
          TimeOut=int(Time)
          write(NameFile,"(a,i8.8,a)") &
               'PW/plots/TriangulationBaseSouth_',TimeOut,'.eps'
          open ( UnitTmp_, file = NameFile)
          call trplot ( UnitTmp_, 7.5, 90.0, 0.0, 90.0, nRemapPointBaseS, &
               xSouth_I, ySouth_I, zSouth_I, listBaseS_I, lptrBaseS_I, &
               lendBaseS_I, 'test1 triangulation',.true., iError )
          close(UnitTmp_)
       endif
    endif
  end subroutine get_triangulation_base_remap_grid

  !============================================================================
  ! adapt the remap grid
  subroutine adapt_remap_grid
    use ModInterpolate, ONLY: bilinear
    use ModSort, ONLY: sort_quick
    use ModTriangulateSpherical,ONLY:trlist
    use ModNumConst, ONLY: cTwoPi
    integer,parameter :: nRow=6
    integer :: iError,iTriangle,iNode,iNode_I(3),iPoint
    real    :: Theta_I(3),Phi_I(3)
    real    :: Xyz_DI(3,3)
    real, save,allocatable :: &
         XyzCentroidN_DI(:,:),ThetaCentroidN_I(:), PhiCentroidN_I(:),&
         XyzCentroidS_DI(:,:),ThetaCentroidS_I(:), PhiCentroidS_I(:)
    real, allocatable :: CriteriaCentroidN_I(:),CriteriaCentroidS_I(:)
    integer,save, allocatable :: ltriBaseN_II(:,:),ltriBaseS_II(:,:)
    integer,save             :: nTriangleBaseN,nTriangleBaseS
    
    integer,allocatable:: IndexSortN_I(:),IndexSortS_I(:)
    logical, save :: IsFirstCallN=.true., IsFirstCallS=.true.
    !--------------------------------------------------------------------------

    !Refine north first
    if (nRemapPointN>0) then
       ! convert the triangulation into a triangle list
       if (.not.allocated(ltriBaseN_II))&
            allocate(ltriBaseN_II(nRow,2*nRemapPointN-4))
       call trlist ( nRemapPointBaseN, listBaseN_I, lptrBaseN_I, &
            lendBaseN_I, nRow, nTriangleBaseN, ltriBaseN_II, iError)
       !write(*,*) 'nTriangleBaseN',nTriangleBaseN
       !write(*,*) 'listBaseN_I',listBaseN_I
       !write(*,*) 'lptrBaseN_I',lptrBaseN_I
       !write(*,*) 'lendBaseN_I',lendBaseN_I
       !allocate arrays to hold centroid information of base grid
       if (.not.allocated(XyzCentroidN_DI))  &
            allocate(XyzCentroidN_DI(3,nTriangleBaseN))
       if (.not.allocated(ThetaCentroidN_I)) &
            allocate(ThetaCentroidN_I(nTriangleBaseN))
       if (.not.allocated(PhiCentroidN_I))   &
            allocate(PhiCentroidN_I(nTriangleBaseN))
       if (.not.allocated(CriteriaCentroidN_I))   &
            allocate(CriteriaCentroidN_I(nTriangleBaseN))
       
       !loop over triangles and evaluate value of refinment criteria at centroid
       if (IsFirstCallN) then
          do iTriangle = 1, nTriangleBaseN
             !extract node indices for each triangle
             iNode_I(1)=ltriBaseN_II(1,iTriangle)
             iNode_I(2)=ltriBaseN_II(2,iTriangle)
             iNode_I(3)=ltriBaseN_II(3,iTriangle)
             
             !for each node, get the theta and phi position
             do iNode = 1,3
                Theta_I(iNode) = RemapThetaBaseN_I(iNode_I(iNode))
                Phi_I  (iNode) = RemapPhiBaseN_I(iNode_I(iNode))
                
                !for each theta and phi position, get the xyz position
                Xyz_DI(1,iNode) = sin(Theta_I(iNode))*cos(Phi_I(iNode))
                Xyz_DI(2,iNode) = sin(Theta_I(iNode))*sin(Phi_I(iNode))
                Xyz_DI(3,iNode) = cos(Theta_I(iNode))
             enddo
             
             !Get and save centroid
             XyzCentroidN_DI(1,iTriangle)=sum(Xyz_DI(1,1:3))/3.0
             XyzCentroidN_DI(2,iTriangle)=sum(Xyz_DI(2,1:3))/3.0
             XyzCentroidN_DI(3,iTriangle)=sum(Xyz_DI(3,1:3))/3.0
             
             ThetaCentroidN_I(iTriangle) = acos(XyzCentroidN_DI(3,iTriangle))
             PhiCentroidN_I(iTriangle)   = &
                  modulo(atan2(XyzCentroidN_DI(2,iTriangle),&
                  XyzCentroidN_DI(1,iTriangle)),cTwoPi)
          enddo
          IsFirstCallN=.false.
       endif

       !write(*,*) 'nTriangleBaseN',nTriangleBaseN
       ! get remap criteria value at centroid
       !write(*,*) 'maxval(AdaptCriteria_G)',maxval(AdaptCriteria_G)
       !call con_stop('')
       do iTriangle=1,nTriangleBaseN
          !write(*,*)'ha',PhiCentroidN_I(iTriangle)/DphiAdapt+1.0,ThetaCentroidN_I(iTriangle)/DthetaAdapt+1.0,DphiAdapt,DthetaAdapt
          !write(*,*)'ha',PhiCentroidN_I(iTriangle),ThetaCentroidN_I(iTriangle)
          CriteriaCentroidN_I(iTriangle) = &
               bilinear(AdaptCriteria_G, 0,nPhiAdapt+1,0,nThetaAdapt+1, &
               (/ PhiCentroidN_I(iTriangle)/DphiAdapt+1.0,&
               ThetaCentroidN_I(iTriangle)/DthetaAdapt+1.0 /) )
          !write(*,*) iTriangle,CriteriaCentroidN_I(iTriangle)
       end do
       
       if (.not.allocated(IndexSortN_I)) allocate(IndexSortN_I(nTriangleBaseN))
       
       !get index array that sorts CriteriaCentroid_I from
       !lowest to biggest value
       call sort_quick(nTriangleBaseN,CriteriaCentroidN_I,IndexSortN_I)
       
       !take the centroid points (up to nAdaptPointsN) with the largest criteria
       !and add them to the remap grid. First Points are the base grid.
       RemapThetaN_I(1:nRemapPointBaseN)= RemapThetaBaseN_I(1:nRemapPointBaseN)
       RemapPhiN_I(1:nRemapPointBaseN)  = RemapPhiBaseN_I(1:nRemapPointBaseN)
       
       iPoint=nRemapPointBaseN
       do iTriangle=nTriangleBaseN,nTriangleBaseN-nAdaptPointsN+1,-1
          iNode = IndexSortN_I(iTriangle)
          iPoint = iPoint+1
          RemapThetaN_I(iPoint) = ThetaCentroidN_I(iNode)
          RemapPhiN_I(iPoint)   = PhiCentroidN_I(iNode)
          !write(*,*) iPoint, nRemapPointBaseN, nNorth,CriteriaCentroidN_I(iNode),maxval(AdaptCriteria_G)
       enddo
       
       
       deallocate(IndexSortN_I)
    endif

    !Refine south 
    if (nRemapPointS>0) then
       ! convert the triangulation into a triangle list
       if (.not.allocated(ltriBaseS_II))&
            allocate(ltriBaseS_II(nRow,2*nRemapPointS-4))
       call trlist ( nRemapPointBaseS, listBaseS_I, lptrBaseS_I, &
            lendBaseS_I, nRow, nTriangleBaseS, ltriBaseS_II, iError)
       
       !allocate arrays to hold centroid information of base grid
       if (.not.allocated(XyzCentroidS_DI))  &
            allocate(XyzCentroidS_DI(3,nTriangleBaseS))
       if (.not.allocated(ThetaCentroidS_I)) &
            allocate(ThetaCentroidS_I(nTriangleBaseS))
       if (.not.allocated(PhiCentroidS_I))   &
            allocate(PhiCentroidS_I(nTriangleBaseS))
       if (.not.allocated(CriteriaCentroidS_I))   &
            allocate(CriteriaCentroidS_I(nTriangleBaseS))
       
       !loop over triangles and evaluate value of refinment criteria at centroid
       if (IsFirstCallS) then
          do iTriangle = 1, nTriangleBaseS
             !extract node indices for each triangle
             iNode_I(1)=ltriBaseS_II(1,iTriangle)
             iNode_I(2)=ltriBaseS_II(2,iTriangle)
             iNode_I(3)=ltriBaseS_II(3,iTriangle)
             
             !for each node, get the theta and phi position
             do iNode = 1,3
                Theta_I(iNode) = RemapThetaBaseS_I(iNode_I(iNode))
                Phi_I  (iNode) = RemapPhiBaseS_I(iNode_I(iNode))
                
                !for each theta and phi position, get the xyz position
                Xyz_DI(1,iNode) = sin(Theta_I(iNode))*cos(Phi_I(iNode))
                Xyz_DI(2,iNode) = sin(Theta_I(iNode))*sin(Phi_I(iNode))
                Xyz_DI(3,iNode) = cos(Theta_I(iNode))
             enddo
             
             !Get and save centroid
             XyzCentroidS_DI(1,iTriangle)=sum(Xyz_DI(1,1:3))/3.0
             XyzCentroidS_DI(2,iTriangle)=sum(Xyz_DI(2,1:3))/3.0
             XyzCentroidS_DI(3,iTriangle)=sum(Xyz_DI(3,1:3))/3.0
             
             ThetaCentroidS_I(iTriangle) = acos(XyzCentroidS_DI(3,iTriangle))
             PhiCentroidS_I(iTriangle)   = &
                  modulo(atan2(XyzCentroidS_DI(2,iTriangle),&
                  XyzCentroidS_DI(1,iTriangle)),cTwoPi)
             
          enddo
          IsFirstCallS=.false.
       endif
       ! get remap criteria value at centroid
       do iTriangle=1,nTriangleBaseS
          CriteriaCentroidS_I(iTriangle) = &
               bilinear(AdaptCriteria_G, 0,nPhiAdapt+1,0,nThetaAdapt+1, &
               (/ PhiCentroidS_I(iTriangle)/DphiAdapt+1.0,&
               ThetaCentroidS_I(iTriangle)/DthetaAdapt+1.0 /) )
       end do
       
       if (.not.allocated(IndexSortS_I)) allocate(IndexSortS_I(nTriangleBaseS))
       
       !get index array that sorts CriteriaCentroid_I from
       !lowest to biggest value
       call sort_quick(nTriangleBaseS,CriteriaCentroidS_I,IndexSortS_I)
       
       !take the centroid points (up to nAdaptPointsN) with the largest criteria
       !and add them to the remap grid. First Points are the base grid.
       RemapThetaS_I(1:nRemapPointBaseS)= RemapThetaBaseS_I(1:nRemapPointBaseS)
       RemapPhiS_I(1:nRemapPointBaseS)  = RemapPhiBaseS_I(1:nRemapPointBaseS)
       
       iPoint=nRemapPointBaseS
       do iTriangle=nTriangleBaseS,nTriangleBaseS-nAdaptPointsS+1,-1
          iNode = IndexSortS_I(iTriangle)
          iPoint = iPoint+1
          RemapThetaS_I(iPoint) = ThetaCentroidS_I(iNode)
          RemapPhiS_I(iPoint)   = PhiCentroidS_I(iNode)
       enddo
       
       
       deallocate(IndexSortS_I)
    endif

    
  end subroutine adapt_remap_grid

  !============================================================================
  ! Update the remap adapt criteria values. Pass values on remap grid
  subroutine update_remap_criteria(nThetaIn,nPhiIn, ThetaIn_G,PhiIn_G,&
       AdaptCriteriaIn_G)
    use ModIoUnit, ONLY: UnitTmp_
    use ModNumConst, ONLY: cTwoPi
    integer, intent(in) :: nThetaIn,nPhiIn
    real   , intent(in) :: ThetaIn_G(0:nPhiIn+1,0:nThetaIn+1)
    real   , intent(in) :: PhiIn_G(0:nPhiIn+1,0:nThetaIn+1)
    real   , intent(in) :: AdaptCriteriaIn_G(0:nPhiIn+1,0:nThetaIn+1)
    integer :: iPhi, iTheta
    !---------------------------------------------------------------------------

    !on allocate arrays, save the adapt criteria grids
    if (.not. allocated(AdaptCriteria_G)) then
       nThetaAdapt = nThetaIn
       nPhiAdapt   = nPhiIn
       allocate(AdaptCriteria_G(0:nPhiAdapt+1, 0:nThetaAdapt+1))
       allocate(PhiAdapt_G(0:nPhiAdapt+1, 0:nThetaAdapt+1))
       allocate(ThetaAdapt_G(0:nPhiAdapt+1, 0:nThetaAdapt+1))

       ThetaAdapt_G = ThetaIn_G
       PhiAdapt_G   = PhiIn_G
       ! set Dtheta and Dphi
       !DthetaAdapt = maxval( ThetaAdapt_G(1:nPhiAdapt,1:nThetaAdapt)) &
       !write(*,*) ThetaAdapt_G
       DthetaAdapt = maxval( ThetaAdapt_G(1,1:nThetaAdapt)) &
            / (nThetaAdapt - 1)
       DphiAdapt   = cTwoPi / (nPhiAdapt - 1)
    endif

    !set the adapt criteria
    AdaptCriteria_G = AdaptCriteriaIn_G

    !write(*,*) 'test',maxval(AdaptCriteriaIn_G)

    !write adapt criteria
!    open(UnitTmp_,file='PW/plots/AdaptCriteria.dat')
!    write(UnitTmp_,'(a)') &
!            'VARIABLES = "X", "Y", "Z", "Var"'
!    write(UnitTmp_,'(a,i3,a,i3,a,i3,a)') 'Zone I=', nPhiAdapt,', J=', nThetaAdapt, ', DATAPACKING=POINT'
!    do iTheta=1,nThetaAdapt
!       do iPhi=1,nPhiAdapt
!          write(UnitTmp_,"(100es18.10)") &
!               sin(ThetaAdapt_G(iPhi,iTheta))*cos(PhiAdapt_G(iPhi,iTheta)), &
!               sin(ThetaAdapt_G(iPhi,iTheta))*sin(PhiAdapt_G(iPhi,iTheta)), &
!               cos(ThetaAdapt_G(iPhi,iTheta)), AdaptCriteria_G(iPhi,iTheta)
!       enddo
!    enddo
!    close(UnitTmp_)
  end subroutine update_remap_criteria
  
  !============================================================================
  ! routine to gather the grid info from across procs to the zero proc
  subroutine update_grid_info
    use ModParticle, only:get_particles_line
    integer ::iError, iProcList, iLine, iLineGlobal,iNorth,iSouth,nParticleLine
    ! MPI status variable
    integer :: iStatus_I(MPI_STATUS_SIZE)
    !--------------------------------------------------------------------------

    if (iProc > 0) then
       !loop over lines to send other line characteristics
       do iLine=1,nLine
          call MPI_send(iLineGlobal_I(iLine),1,MPI_INTEGER,0,2,iComm,iError)
          call MPI_send(ThetaLine_I(iLine),1,MPI_REAL,0,3,iComm,iError)
          call MPI_send(PhiLine_I(iLine),1,MPI_REAL,0,4,iComm,iError)

          if(UseParticles) then
             call get_particles_line(iLine,nParticleLine)
             call MPI_send(nParticleLine,1,MPI_INTEGER,0,5,iComm,iError)
          end if
       enddo
    else
       !loop over procs >1 and recieve
       if (nProc>0) then
          do iProcList = 1,nProc-1
             do iLine = 1,nLine_P(iProcList)
                call MPI_recv(iLineGlobal,1,MPI_INTEGER,iProcList,2,iComm,&
                     iStatus_I,iError)
                call MPI_recv(Lines_I(iLineGlobal)%Theta,1,MPI_REAL,&
                     iProcList,3,iComm,iStatus_I,iError)
                call MPI_recv(Lines_I(iLineGlobal)%Phi,1,MPI_REAL,&
                     iProcList,4,iComm,iStatus_I,iError)

                if(UseParticles) then
                   call MPI_recv(Lines_I(iLineGlobal)%nParticle,1,&
                        MPI_INTEGER,iProcList,5,iComm,iStatus_I,iError)
                end if

                !update the xyz location
                call Lines_I(iLineGlobal)%calc_xyz
             enddo
          enddo
       endif
       
       !now fill in proc=0 values
       nLine_P(0) = nLine
       !write(*,*) 'TestA', Time
       do iLine=1,nLine
          Lines_I(iLineGlobal_I(iLine))%Theta=ThetaLine_I(iLine)
          Lines_I(iLineGlobal_I(iLine))%Phi=PhiLine_I(iLine)
          !write(*,*) iLine,Lines_I(iLineGlobal_I(iLine))%Theta,Lines_I(iLineGlobal_I(iLine))%Phi
          call Lines_I(iLine)%calc_xyz
          !write(*,*) iLine,Lines_I(iLineGlobal_I(iLine))%Xyz_D

          if (UseParticles) then
             call get_particles_line(iLine,nParticleLine)
             Lines_I(iLineGlobal_I(iLine))%nParticle=nParticleLine
          endif
       enddo

       !update the triangulation with new line coords
       call get_triangulation
    end if

  end subroutine update_grid_info
  !============================================================================
  ! routine to get triangulation for north and south grids, only call on iProc0
  subroutine get_triangulation
    use ModTriangulateSpherical,ONLY:trmesh, trplot
    use ModIoUnit, ONLY: UnitTmp_
    real, allocatable :: xNorth_I(:),yNorth_I(:),zNorth_I(:)
    real, allocatable :: xSouth_I(:),ySouth_I(:),zSouth_I(:)
    integer :: iLine, iError,TimeOut
    logical :: DoSaveTriangulate = .True.
      Character(len=100) :: NameFile
    character (len=*),parameter :: NameSub='get_triangulation'
    !--------------------------------------------------------------------------

    if (iProc>0) call con_stop('get_triangulate called by iProc>0')
    
    !allocate x,y,z arrays for north and south
    if (.not.allocated(xNorth_I) .and. nNorth>0) &
         allocate(xNorth_I(nNorth))
    if (.not.allocated(yNorth_I) .and. nNorth>0) &
         allocate(yNorth_I(nNorth))
    if (.not.allocated(zNorth_I) .and. nNorth>0) &
         allocate(zNorth_I(nNorth))

    if (.not.allocated(xSouth_I) .and. nSouth>0) &
         allocate(xSouth_I(nSouth))
    if (.not.allocated(ySouth_I) .and. nSouth>0) &
         allocate(ySouth_I(nSouth))
    if (.not.allocated(zSouth_I) .and. nSouth>0) &
         allocate(zSouth_I(nSouth))

    
    !construct north triangulation
    if (nNorth>0) then
       !unpack positions 
       do iLine=1,nNorth
          xNorth_I(iLine) = Lines_I(iIndexNorth_I(iLine))%Xyz_D(X_)
          yNorth_I(iLine) = Lines_I(iIndexNorth_I(iLine))%Xyz_D(Y_)
          zNorth_I(iLine) = Lines_I(iIndexNorth_I(iLine))%Xyz_D(Z_)
       enddo

       !write(*,*) 'TestB', Time
       !write(*,*) nNorth
       !do iLine=1,nNorth
       !   write(*,*) iLine,xNorth_I(iLine),yNorth_I(iLine),zNorth_I(iLine)
       !enddo
       
       !create the triangulation
       call trmesh ( nNorth, xNorth_I, yNorth_I, zNorth_I, &
            listN_I, lptrN_I, lendN_I, iError )

       if ( iError == -2 ) then
          write(*,*)NameSub, &
               ' WARNING: Error in TRMESH, First three nodes are collinear'
          call CON_stop(NameSub//' Problem With Triangulation')
       else if ( iError > 0 ) then
          write(*,*) 'ERROR: duplicate node iError=',iError
          write(*,*)NameSub// &
               ' ERROR: Error in TRMESH, Duplicate nodes encountered'
          call CON_stop(NameSub//' Problem With Triangulation')
       end if
    endif
    
    !construct north triangulation
    if (nSouth>0) then
       !unpack positions 
       do iLine=1,nSouth
          xSouth_I(iLine) = Lines_I(iIndexSouth_I(iLine))%Xyz_D(X_)
          ySouth_I(iLine) = Lines_I(iIndexSouth_I(iLine))%Xyz_D(Y_)
          zSouth_I(iLine) = Lines_I(iIndexSouth_I(iLine))%Xyz_D(Z_)
       enddo

       !create the triangulation
       call trmesh ( nSouth, xSouth_I, ySouth_I, zSouth_I, &
            listS_I, lptrS_I, lendS_I, iError )

       if ( iError == -2 ) then
          write(*,*)NameSub, &
               ' WARNING: Error in TRMESH, First three nodes are collinear'
          call CON_stop(NameSub//' Problem With Triangulation')
       else if ( iError > 0 ) then
          write(*,*)NameSub// &
               ' ERROR: Error in TRMESH, Duplicate nodes encountered'
          call CON_stop(NameSub//' Problem With Triangulation')
       end if
    endif

    !write triangulation output if requested
    if (DoSaveTriangulate) then
       if (nNorth>0) then
          ! Northern Hemi
          TimeOut=int(Time)
          write(NameFile,"(a,i8.8,a)") &
               'PW/plots/TriangulationNorth_',TimeOut,'.eps'
          open ( UnitTmp_, file = NameFile)
          call trplot ( UnitTmp_, 7.5, 90.0, 0.0, 90.0, nNorth, &
               xNorth_I, yNorth_I, zNorth_I, listN_I, lptrN_I, &
               lendN_I, 'test1 triangulation',.true., iError )
          close(UnitTmp_)
       endif
       if (nSouth>0) then
          ! Southern Hemi
          TimeOut=int(Time)
          write(NameFile,"(a,i8.8,a)") &
               'PW/plots/TriangulationSouth_',TimeOut,'.eps'
          open ( UnitTmp_, file = NameFile)
          call trplot ( UnitTmp_, 7.5, 90.0, 0.0, 90.0, nSouth, &
               xSouth_I, ySouth_I, zSouth_I, listS_I, lptrS_I, &
               lendS_I, 'test1 triangulation',.true., iError )
          close(UnitTmp_)
       endif
    endif
  end subroutine get_triangulation

  !============================================================================
  ! set anchor points (lines that don't remap) to anchor the grid and keep it
  ! from creeping inward after remap. 
  subroutine set_anchor_points
    integer,allocatable :: iAnchor_I(:),SmallestLat_I(:)
    integer :: iMltBin, iLine
    !--------------------------------------------------------------------------
    if (nAnchorZones==0) return
    if (nNorth>0) then
       if(.not.allocated(iAnchor_I)) allocate(iAnchor_I(nAnchorZones))
       if(.not.allocated(SmallestLat_I)) allocate(SmallestLat_I(nAnchorZones))
       iAnchor_I(:)=0
       SmallestLat_I(:)=90.0
       
       !Sort lines into Longitude bins
       do iLine = 1,nNorth
          !initialize anchor value to false
          Lines_I(iIndexNorth_I(iLine))%IsAnchor=.false.
          
          !find mlt bin
          iMltBin = &
               floor(Lines_I(iIndexNorth_I(iLine))%Lon/360.0*nAnchorZones)+1

          !test if lat is lower than prior point
          if (abs(Lines_I(iIndexNorth_I(iLine))%Lat) &
               < abs(SmallestLat_I(iMltBin)))then
             !unAnchor previous anchor point
             if (iAnchor_I(iMltBin) /= 0) then
                Lines_I(iAnchor_I(iMltBin))%IsAnchor=.false.
             endif

             !set new anchor point
             iAnchor_I(iMltBin) = iIndexNorth_I(iLine)
             Lines_I(iAnchor_I(iMltBin))%IsAnchor=.true.
             SmallestLat_I(iMltBin)=Lines_I(iAnchor_I(iMltBin))%Lat
          endif
       enddo
       deallocate(iAnchor_I,SmallestLat_I)
    end if

    !now set south anchor points
    if (nSouth>0) then
       if(.not.allocated(iAnchor_I)) allocate(iAnchor_I(nAnchorZones))
       if(.not.allocated(SmallestLat_I)) allocate(SmallestLat_I(nAnchorZones))
       iAnchor_I(:)=0
       SmallestLat_I(:)=90.0
       
       !Sort lines into Longitude bins
       do iLine = 1,nSouth
          !initialize anchor value to false
          Lines_I(iIndexSouth_I(iLine))%IsAnchor=.false.
          
          !find mlt bin
          iMltBin = &
               floor(Lines_I(iIndexSouth_I(iLine))%Lon/360.0*nAnchorZones)+1

          !test if lat is lower than prior point
          if (abs(Lines_I(iIndexSouth_I(iLine))%Lat) &
               < abs(SmallestLat_I(iMltBin)))then
             !unAnchor previous anchor point
             if (iAnchor_I(iMltBin) /= 0) then
                Lines_I(iAnchor_I(iMltBin))%IsAnchor=.false.
             endif

             !set new anchor point
             iAnchor_I(iMltBin) = iIndexSouth_I(iLine)
             Lines_I(iAnchor_I(iMltBin))%IsAnchor=.true.
             SmallestLat_I(iMltBin)=Lines_I(iAnchor_I(iMltBin))%Lat
          endif
       enddo
       deallocate(iAnchor_I,SmallestLat_I)
    end if
  end subroutine set_anchor_points
  
  !============================================================================
  ! fill in the RemapN_I and RemapS_I arrays that hold the remapping plan
  subroutine set_regrid_plan
    use ModTriangulateSpherical, ONLY: find_triangle_sph
    use ModSort, ONLY: sort_quick
    integer :: iLine, iLineTmp, iCount
    real,    allocatable :: CoordXyz_DI(:,:)
    logical, allocatable :: IsAvailable_I(:)
    real :: Xyz_D(3)
    real :: Area1, Area2,Area3
    integer :: iNode1, iNode2, iNode3
    logical :: IsTriangleFound
    integer,allocatable:: IndexSortN_I(:),IndexSortS_I(:)
    real,allocatable:: DistN_I(:),DistS_I(:)
    !--------------------------------------------------------------------------

    
    !allocate remap plan arrays to maximum possible number of points on
    ! remap grid
    if (nNorth>0) then
       if (.not.allocated(RemapN_I)) allocate(RemapN_I(nRemapPointN))
       if (.not.allocated(IndexSortN_I)) allocate(IndexSortN_I(nNorth))
       if (.not.allocated(DistN_I)) allocate(DistN_I(nNorth))
       
    endif
    
    if (nSouth>0) then
       if (.not.allocated(RemapS_I)) allocate(RemapS_I(nRemapPointS))
       if (.not.allocated(IndexSortS_I)) allocate(IndexSortS_I(nSouth))
       if (.not.allocated(DistS_I)) allocate(DistS_I(nSouth))
    endif

    
    !Start with the north grid
    if (nNorth>0) then
       !initially all lines in hemisphere are available for remap
       allocate(IsAvailable_I(nNorth))
       IsAvailable_I(:) = .true.
             
       !repack xyz coordinate of lines into CoordXyz_DI
       allocate(CoordXyz_DI(3,nNorth))
       do iLine = 1,nNorth
          CoordXyz_DI(:,iLine) = Lines_I(iIndexNorth_I(iLine))%Xyz_D
       enddo

       !Counter for remap
       iCount=0

       !loop over remap points but leave room for anchor points
       do iLine = 1,nRemapPointN - nAnchorZones 
          !get Xyz of remap grid point
          Xyz_D(X_) = sin(RemapThetaN_I(iLine))*cos(RemapPhiN_I(iLine))
          Xyz_D(Y_) = sin(RemapThetaN_I(iLine))*sin(RemapPhiN_I(iLine))
          Xyz_D(Z_) = cos(RemapThetaN_I(iLine))
          
          !find triangle that contains the remap point
          call find_triangle_sph(Xyz_D, nNorth, &
               CoordXyz_DI(:,:), listN_I, lptrN_I, lendN_I, &
               Area1, Area2, Area3, IsTriangleFound, &
               iNode1,iNode2,iNode3)

          !kludge
          !if (RemapThetaN_I(iLine) > 0.55) IsTriangleFound=.false.
          !write(*,*) iLine, NRemapPointN,RemapThetaN_I(iLine),RemapPhiN_I(iLine),iNode1,iNode2,iNode3
          !write(*,*) iLine, NRemapPointN,RemapThetaN_I(iLine),RemapPhiN_I(iLine),&
          !     Lines_I(iIndexNorth_I(iNode1))%theta,Lines_I(iIndexNorth_I(iNode1))%phi,&
          !     Lines_I(iIndexNorth_I(iNode2))%theta,Lines_I(iIndexNorth_I(iNode2))%phi,&
          !     Lines_I(iIndexNorth_I(iNode3))%theta,Lines_I(iIndexNorth_I(iNode3))%phi&
          !     ,Area1,Area2,Area3

          if(IsTriangleFound) then
             ! we can remap to this point
             iCount=iCount+1
             !set global line index of each node
             RemapN_I(iCount)%iNode1 = iIndexNorth_I(iNode1)
             RemapN_I(iCount)%iNode2 = iIndexNorth_I(iNode2)
             RemapN_I(iCount)%iNode3 = iIndexNorth_I(iNode3)

             !set local line index of node on it's proc
             RemapN_I(iCount)%iNodeLocal1 = &
                  Lines_I(iIndexNorth_I(iNode1))%iLineLocal
             RemapN_I(iCount)%iNodeLocal2 = &
                  Lines_I(iIndexNorth_I(iNode2))%iLineLocal
             RemapN_I(iCount)%iNodeLocal3 = &
                  Lines_I(iIndexNorth_I(iNode3))%iLineLocal

             !set iProc that each node is on
             RemapN_I(iCount)%iProc1 = Lines_I(iIndexNorth_I(iNode1))%iProc
             RemapN_I(iCount)%iProc2 = Lines_I(iIndexNorth_I(iNode2))%iProc
             RemapN_I(iCount)%iProc3 = Lines_I(iIndexNorth_I(iNode3))%iProc

             if (UseParticles) then
                !set number of particles on each node
                RemapN_I(iCount)%nParticle1 = &
                     Lines_I(iIndexNorth_I(iNode1))%nParticle
                RemapN_I(iCount)%nParticle2 = &
                     Lines_I(iIndexNorth_I(iNode2))%nParticle
                RemapN_I(iCount)%nParticle3 = &
                     Lines_I(iIndexNorth_I(iNode3))%nParticle
             endif
             
             !set the interpolation weights (area of the sub triangles)
             RemapN_I(iCount)%weight1 = Area1
             RemapN_I(iCount)%weight2 = Area2
             RemapN_I(iCount)%weight3 = Area3

             !set the location of the remaped line from the remap grid
             RemapN_I(iCount)%Theta = RemapThetaN_I(iLine)
             RemapN_I(iCount)%Phi   = RemapPhiN_I(iLine)

             ! determine which global line will be moved to the remap
             ! find distance of remap point to lines
             
             do iLineTmp =1,nNorth
                DistN_I(iLineTmp) = &
                     sum((Xyz_D - Lines_I(iIndexNorth_I(iLineTmp))%Xyz_D)**2)
             enddo
             
             !get index array that sorts Dist_I from lowest to biggest
             call sort_quick(nNorth,DistN_I,IndexSortN_I)

             Line_Search:do iLineTmp =1,nNorth
                !write(*,*) iLineTmp, IsAvailable_I(IndexSortN_I(iLineTmp))
                if (IsAvailable_I(IndexSortN_I(iLineTmp)) .and. .not.&
                     Lines_I(iIndexNorth_I(IndexSortN_I(iLineTmp)))%IsAnchor) &
                     then
                   RemapN_I(iCount)%iLineGlobal=&
                        iIndexNorth_I(IndexSortN_I(iLineTmp))
                   RemapN_I(iCount)%iLineLocal =&
                        Lines_I(iIndexNorth_I(IndexSortN_I(iLineTmp)))%iLineLocal
                   RemapN_I(iCount)%iProc=&
                        Lines_I(iIndexNorth_I(IndexSortN_I(iLineTmp)))%iProc
                   !make this line no longer available for remap
                   IsAvailable_I(IndexSortN_I(iLineTmp))=.false.
                   exit Line_Search
                endif
             end do Line_Search
          endif
             ! determine which global line will be moved to the remap
             ! start by looking at availability of nodes of triangle to
             ! minimize sends and recieves
             
!             if (IsAvailable_I(iNode1)) then
!                !set the line index and proc that is bring replaced with remap
!                RemapN_I(iCount)%iLineGlobal=RemapN_I(iCount)%iNode1
!                RemapN_I(iCount)%iLineLocal =&
!                     Lines_I(iIndexNorth_I(iNode1))%iLineLocal
!                RemapN_I(iCount)%iProc=RemapN_I(iCount)%iProc1
!                !make this line no longer available for remap
!                IsAvailable_I(iNode1)=.false.
!             elseif(IsAvailable_I(iNode2)) then
!                !set the line index and proc that is bring replaced with remap
!                RemapN_I(iCount)%iLineGlobal=RemapN_I(iCount)%iNode2
!                RemapN_I(iCount)%iLineLocal =&
!                     Lines_I(iIndexNorth_I(iNode2))%iLineLocal
!                RemapN_I(iCount)%iProc=RemapN_I(iCount)%iProc2
!                !make this line no longer available for remap
!                IsAvailable_I(iNode2)=.false.
!             elseif(IsAvailable_I(iNode3)) then
!                !set the line index and proc that is bring replaced with remap
!                RemapN_I(iCount)%iLineGlobal=RemapN_I(iCount)%iNode3
!                RemapN_I(iCount)%iLineLocal =&
!                     Lines_I(iIndexNorth_I(iNode3))%iLineLocal
!                RemapN_I(iCount)%iProc=RemapN_I(iCount)%iProc3
!                !make this line no longer available for remap
!                IsAvailable_I(iNode3)=.false.
!             else
!                !seach for any available line to be remaped
!                Line_Search:do iLineTmp =1,nNorth
!                   if (IsAvailable_I(iLineTmp)) then
!                      !set the line index and proc that is bring replaced
!                      !with remap
!                      RemapN_I(iCount)%iLineGlobal=&
!                           iIndexNorth_I(iLineTmp)
!                      RemapN_I(iCount)%iLineLocal =&
!                           Lines_I(iIndexNorth_I(iLineTmp))%iLineLocal
!                      RemapN_I(iCount)%iProc=&
!                           Lines_I(iIndexNorth_I(iLineTmp))%iProc
!                      !make this line no longer available for remap
!                      IsAvailable_I(iLineTmp)=.false.
!                      exit Line_Search
!                   endif
!                enddo Line_Search
!             endif
!          endif

          !kludge
          !if (iCount==100) then
          !   write(*,*)'iLineGlobal', RemapN_I(iCount)%iLineGlobal
          !   write(*,*)'iLineLocal', RemapN_I(iCount)%iLineLocal
          !   write(*,*)'iProc', RemapN_I(iCount)%iProc
          !   write(*,*)'iProc1', RemapN_I(iCount)%iProc1
          !   write(*,*)'iProc2', RemapN_I(iCount)%iProc2
          !   write(*,*)'iProc3', RemapN_I(iCount)%iProc3
          !   write(*,*)'iNode1', RemapN_I(iCount)%iNode1
          !   write(*,*)'iNode2', RemapN_I(iCount)%iNode2
          !   write(*,*)'iNode3', RemapN_I(iCount)%iNode3
          !   write(*,*)'iNodeLocal1', RemapN_I(iCount)%iNodeLocal1
          !   write(*,*)'iNodeLocal2', RemapN_I(iCount)%iNodeLocal2
          !   write(*,*)'iNodeLocal3', RemapN_I(iCount)%iNodeLocal3
             !endif
       end do
       !record number of point to remap
       nPointsToRemapN = iCount
       deallocate(CoordXyz_DI)
       deallocate(IsAvailable_I)
    endif
    
    !Now build S grid plan
    if (nSouth>0) then
       !initially all lines in hemisphere are available for remap
       allocate(IsAvailable_I(nSouth))
       IsAvailable_I(:) = .true.
       
       !repack xyz coordinate of lines into CoordXyz_DI
       allocate(CoordXyz_DI(3,nSouth))
       do iLine = 1,nSouth
          CoordXyz_DI(:,iLine) = Lines_I(iIndexSouth_I(iLine))%Xyz_D
       enddo

       !Counter for remap
       iCount=0
       do iLine = 1,nRemapPointS-nAnchorZones
          !get Xyz of remap grid point
          Xyz_D(X_) = sin(RemapThetaS_I(iLine))*cos(RemapPhiS_I(iLine))
          Xyz_D(Y_) = sin(RemapThetaS_I(iLine))*sin(RemapPhiS_I(iLine))
          Xyz_D(Z_) = cos(RemapThetaS_I(iLine))
          
          !find triangle that contains the remap point
          call find_triangle_sph(Xyz_D, nSouth, &
               CoordXyz_DI(:,:), listS_I, lptrS_I, lendS_I, &
               Area1, Area2, Area3, IsTriangleFound, &
               iNode1,iNode2,iNode3)

          if(IsTriangleFound) then
             ! we can remap to this point
             iCount=iCount+1
             !set global line index of each node
             RemapS_I(iCount)%iNode1 = iIndexSouth_I(iNode1)
             RemapS_I(iCount)%iNode2 = iIndexSouth_I(iNode2)
             RemapS_I(iCount)%iNode3 = iIndexSouth_I(iNode3)

             if (UseParticles) then
                !set number of particles on each node
                RemapS_I(iCount)%nParticle1 = &
                     Lines_I(iIndexSouth_I(iNode1))%nParticle
                RemapS_I(iCount)%nParticle2 = &
                     Lines_I(iIndexSouth_I(iNode2))%nParticle
                RemapS_I(iCount)%nParticle3 = &
                     Lines_I(iIndexSouth_I(iNode3))%nParticle
             endif
             
             !set local line index of node on it's proc
             RemapS_I(iCount)%iNodeLocal1 = &
                  Lines_I(iIndexSouth_I(iNode1))%iLineLocal
             RemapS_I(iCount)%iNodeLocal2 = &
                  Lines_I(iIndexSouth_I(iNode2))%iLineLocal
             RemapS_I(iCount)%iNodeLocal3 = &
                  Lines_I(iIndexSouth_I(iNode3))%iLineLocal

             
             !set iProc that each node is on
             RemapS_I(iCount)%iProc1 = Lines_I(iIndexSouth_I(iNode1))%iProc
             RemapS_I(iCount)%iProc2 = Lines_I(iIndexSouth_I(iNode2))%iProc
             RemapS_I(iCount)%iProc3 = Lines_I(iIndexSouth_I(iNode3))%iProc

             !set the interpolation weights (area of the sub triangles)
             RemapS_I(iCount)%weight1 = Area1
             RemapS_I(iCount)%weight2 = Area2
             RemapS_I(iCount)%weight3 = Area3

             !set the location of the remaped line from the remap grid
             RemapS_I(iCount)%Theta = RemapThetaS_I(iLine)
             RemapS_I(iCount)%Phi   = RemapPhiS_I(iLine)

             ! determine which global line will be moved to the remap
             ! find distance of remap point to lines
             
             do iLineTmp =1,nSouth
                DistS_I(iLineTmp) = &
                     sum((Xyz_D - Lines_I(iIndexSouth_I(iLineTmp))%Xyz_D)**2)
             enddo
             
             !get index array that sorts Dist_I from lowest to biggest
             call sort_quick(nSouth,DistS_I,IndexSortS_I)

             Line_Search_South:do iLineTmp =1,nSouth
                if (IsAvailable_I(IndexSortS_I(iLineTmp)).and. .not.&
                     Lines_I(iIndexSouth_I(IndexSortS_I(iLineTmp)))%IsAnchor) &
                     then
                   RemapS_I(iCount)%iLineGlobal=&
                        iIndexSouth_I(IndexSortS_I(iLineTmp))
                   RemapS_I(iCount)%iLineLocal =&
                        Lines_I(iIndexSouth_I(IndexSortS_I(iLineTmp)))%iLineLocal
                   RemapS_I(iCount)%iProc=&
                        Lines_I(iIndexSouth_I(IndexSortS_I(iLineTmp)))%iProc
                   !make this line no longer available for remap
                   IsAvailable_I(IndexSortS_I(iLineTmp))=.false.
                   exit Line_Search_South
                endif
             end do Line_Search_South
          endif
             
!             ! determine which global line will be moved to the remap
!             ! start by looking at availability of nodes of triangle to
!             ! minimize sends and recieves
!             
!             if (IsAvailable_I(iNode1)) then
!                !set the line index and proc that is bring replaced with remap
!                RemapS_I(iCount)%iLineGlobal=RemapS_I(iCount)%iNode1
!                RemapS_I(iCount)%iLineLocal =&
!                     Lines_I(iIndexSouth_I(iNode1))%iLineLocal
!                RemapS_I(iCount)%iProc=RemapS_I(iCount)%iProc1
!                !make this line no longer available for remap
!                IsAvailable_I(iNode1)=.false.
!             elseif(IsAvailable_I(iNode2)) then
!                !set the line index and proc that is bring replaced with remap
!                RemapS_I(iCount)%iLineGlobal=RemapS_I(iCount)%iNode2
!                RemapS_I(iCount)%iLineLocal =&
!                     Lines_I(iIndexSouth_I(iNode2))%iLineLocal
!                RemapS_I(iCount)%iProc=RemapS_I(iCount)%iProc2
!                !make this line no longer available for remap
!                IsAvailable_I(iNode2)=.false.
!             elseif(IsAvailable_I(iNode3)) then
!                !set the line index and proc that is bring replaced with remap
!                RemapS_I(iCount)%iLineGlobal=RemapS_I(iCount)%iNode3
!                RemapS_I(iCount)%iLineLocal =&
!                     Lines_I(iIndexSouth_I(iNode3))%iLineLocal
!                RemapS_I(iCount)%iProc=RemapS_I(iCount)%iProc3
!                !make this line no longer available for remap
!                IsAvailable_I(iNode3)=.false.
!             else
!                !seach for any available line to be remaped
!                Line_Search_South:do iLineTmp =1,nSouth
!                   if (IsAvailable_I(iLineTmp)) then
!                      !set the line index and proc that is bring replaced with remap
!                      RemapS_I(iCount)%iLineGlobal=&
!                           iIndexSouth_I(iLineTmp)
!                      RemapS_I(iCount)%iLineLocal =&
!                           Lines_I(iIndexSouth_I(iLineTmp))%iLineLocal
!                      RemapS_I(iCount)%iProc=&
!                           Lines_I(iIndexSouth_I(iLineTmp))%iProc
!                      
!                      !make this line no longer available for remap
!                      IsAvailable_I(iLineTmp)=.false.
!                      exit Line_Search_South
!                   endif
!                enddo Line_Search_South
!             endif
!          endif
       end do
       !record number of point to remap
       nPointsToRemapS = iCount
       deallocate(CoordXyz_DI)
       deallocate(IsAvailable_I)
    endif
  end subroutine set_regrid_plan

  !============================================================================
  ! Distribute the regrid plan to all procs
  subroutine distribute_regrid_plan
    integer :: iError, iLine
    !--------------------------------------------------------------------------
    !distrubute the number of remap points
    if (nNorth>0 .and. nProc>1)  &
         call MPI_bcast(nPointsToRemapN,1,MPI_INTEGER,0,iComm,iError)
    if (nSouth>0 .and. nProc>1)  &
         call MPI_bcast(nPointsToRemapS,1,MPI_INTEGER,0,iComm,iError)
    
    !allocate the remap plans for iProc>0 if not already allocated
    if (nNorth>0 .and. iProc>0) then
       if (.not.allocated(RemapN_I)) allocate(RemapN_I(nRemapPointN))
    endif
    
    if (nSouth>0 .and. iProc>0) then
       if (.not.allocated(RemapS_I)) allocate(RemapS_I(nRemapPointS))
    endif

    if (nNorth>0 .and. nProc>1) then
       ! loop over all remap lines and bcast the map to all procs
       do iLine = 1, nPointsToRemapN
          call MPI_bcast(RemapN_I(iLine)%iNode1,1,MPI_INTEGER,0,iComm,iError)
          call MPI_bcast(RemapN_I(iLine)%iNode2,1,MPI_INTEGER,0,iComm,iError)
          call MPI_bcast(RemapN_I(iLine)%iNode3,1,MPI_INTEGER,0,iComm,iError)
          
          call MPI_bcast(RemapN_I(iLine)%iNodeLocal1,1,MPI_INTEGER,0,iComm,&
               iError)
          call MPI_bcast(RemapN_I(iLine)%iNodeLocal2,1,MPI_INTEGER,0,iComm,&
               iError)
          call MPI_bcast(RemapN_I(iLine)%iNodeLocal3,1,MPI_INTEGER,0,iComm,&
               iError)
          if (UseParticles) then
             call MPI_bcast(RemapN_I(iLine)%nParticle1,1,MPI_INTEGER,0,iComm,&
                  iError)
             call MPI_bcast(RemapN_I(iLine)%nParticle2,1,MPI_INTEGER,0,iComm,&
                  iError)
             call MPI_bcast(RemapN_I(iLine)%nParticle3,1,MPI_INTEGER,0,iComm,&
                  iError)
          endif
          
          call MPI_bcast(RemapN_I(iLine)%iProc1,1,MPI_INTEGER,0,iComm,iError)
          call MPI_bcast(RemapN_I(iLine)%iProc2,1,MPI_INTEGER,0,iComm,iError)
          call MPI_bcast(RemapN_I(iLine)%iProc3,1,MPI_INTEGER,0,iComm,iError)

          call MPI_bcast(RemapN_I(iLine)%weight1,1,MPI_REAL,0,iComm,iError)
          call MPI_bcast(RemapN_I(iLine)%weight2,1,MPI_REAL,0,iComm,iError)
          call MPI_bcast(RemapN_I(iLine)%weight3,1,MPI_REAL,0,iComm,iError)

          call MPI_bcast(RemapN_I(iLine)%theta,1,MPI_REAL,0,iComm,iError)
          call MPI_bcast(RemapN_I(iLine)%phi,1,MPI_REAL,0,iComm,iError)

          call MPI_bcast(RemapN_I(iLine)%iLineGlobal,1,MPI_INTEGER,0,iComm,&
               iError)
          call MPI_bcast(RemapN_I(iLine)%iLineLocal,1,MPI_INTEGER,0,iComm,&
               iError)

          call MPI_bcast(RemapN_I(iLine)%iProc,1,MPI_INTEGER,0,iComm,&
               iError)
       enddo
    endif

    if (nSouth>0 .and. nProc>1) then
       ! loop over all remap lines and bcast the map to all procs
       do iLine = 1, nPointsToRemapS
          call MPI_bcast(RemapS_I(iLine)%iNode1,1,MPI_INTEGER,0,iComm,iError)
          call MPI_bcast(RemapS_I(iLine)%iNode2,1,MPI_INTEGER,0,iComm,iError)
          call MPI_bcast(RemapS_I(iLine)%iNode3,1,MPI_INTEGER,0,iComm,iError)
          
          call MPI_bcast(RemapS_I(iLine)%iNodeLocal1,1,MPI_INTEGER,0,iComm,&
               iError)
          call MPI_bcast(RemapS_I(iLine)%iNodeLocal2,1,MPI_INTEGER,0,iComm,&
               iError)
          call MPI_bcast(RemapS_I(iLine)%iNodeLocal3,1,MPI_INTEGER,0,iComm,&
               iError)

          if (UseParticles) then
             call MPI_bcast(RemapS_I(iLine)%nParticle1,1,MPI_INTEGER,0,iComm,&
                  iError)
             call MPI_bcast(RemapS_I(iLine)%nParticle2,1,MPI_INTEGER,0,iComm,&
                  iError)
             call MPI_bcast(RemapS_I(iLine)%nParticle3,1,MPI_INTEGER,0,iComm,&
                  iError)
          endif
          
          
          call MPI_bcast(RemapS_I(iLine)%iProc1,1,MPI_INTEGER,0,iComm,iError)
          call MPI_bcast(RemapS_I(iLine)%iProc2,1,MPI_INTEGER,0,iComm,iError)
          call MPI_bcast(RemapS_I(iLine)%iProc3,1,MPI_INTEGER,0,iComm,iError)

          call MPI_bcast(RemapS_I(iLine)%weight1,1,MPI_REAL,0,iComm,iError)
          call MPI_bcast(RemapS_I(iLine)%weight2,1,MPI_REAL,0,iComm,iError)
          call MPI_bcast(RemapS_I(iLine)%weight3,1,MPI_REAL,0,iComm,iError)

          call MPI_bcast(RemapS_I(iLine)%theta,1,MPI_REAL,0,iComm,iError)
          call MPI_bcast(RemapS_I(iLine)%phi,1,MPI_REAL,0,iComm,iError)

          call MPI_bcast(RemapS_I(iLine)%iLineGlobal,1,MPI_INTEGER,0,iComm,&
               iError)
          call MPI_bcast(RemapS_I(iLine)%iLineLocal,1,MPI_INTEGER,0,iComm,&
               iError)

          call MPI_bcast(RemapS_I(iLine)%iProc,1,MPI_INTEGER,0,iComm,&
               iError)
       enddo
    endif

    
  !start by distributing the Lines_I array to all proc
  end subroutine distribute_regrid_plan
  !============================================================================
  ! execute the regrid plan
  subroutine apply_regrid
    use ModPWOM, only:nAlt, nVar, UseParticles
    use ModNumConst, ONLY: cRadToDeg
    use ModParticle, only: allocate_particle_recv,post_particle_line_recv, &
         post_particle_line_send, check_particle_recv,interp_particle_line
    ! How many remaps on our proc
    integer :: nRemapLocal
    !counter for remap number
    integer :: iRemap

    !mpi vars
    integer :: iStatus_I(MPI_STATUS_SIZE)
    integer,allocatable :: iRequest_I(:)
    integer :: iError
    
    ! array to hold the incomming state arrays
    real,allocatable :: StateRecv_CVI(:,:,:)

    !array to convert global index to locally recieved nodes
    integer, allocatable :: iNode_I(:)
    
    integer :: nNode, iNode, iLine, nRequest, iRequest
    
    !array to act as a mask to tell if node is alrady allocated to send/recv
    logical, allocatable :: IsRecvNode_I(:),IsSendNode_IP(:,:)
    !--------------------------------------------------------------------------
    if (.not. allocated(IsRecvNode_I)) allocate(IsRecvNode_I(nTotalLine))
    if (.not. allocated(IsRecvNode_I)) allocate(IsRecvNode_I(nTotalLine))
    if (.not. allocated(IsSendNode_IP)) allocate(IsSendNode_IP(nTotalLine,0:nProc-1))
    if (.not. allocated(iNode_I)) allocate(iNode_I(nTotalLine))
    if (.not. allocated(iRequest_I)) allocate(iRequest_I(nTotalLine))
    
    
    
    !regrid North first
    if (nNorth>0) then

       nNode = 0
       nRemapLocal = 0
       IsRecvNode_I(:) =.false.
       
       ! go through plan and count up number of remaps on given proc
       ! count up also how many nodes are to be sent to this proc.
       do iLine=1,nPointsToRemapN
          if (iProc == RemapN_I(iLine)%iProc) then
             nRemapLocal = nRemapLocal + 1
             associate(&
                  iNode1 => RemapN_I(iLine)%iNode1,&
                  iNode2 => RemapN_I(iLine)%iNode2,&
                  iNode3 => RemapN_I(iLine)%iNode3,&
                  iNodeLocal1 => RemapN_I(iLine)%iNodeLocal1,&
                  iNodeLocal2 => RemapN_I(iLine)%iNodeLocal2,&
                  iNodeLocal3 => RemapN_I(iLine)%iNodeLocal3,&
                  iProc1 => RemapN_I(iLine)%iNode1,&
                  iProc2 => RemapN_I(iLine)%iNode2,&
                  iProc3 => RemapN_I(iLine)%iNode3)
               
               !check if node is already listed to recv if not add it to the
               !total count
               if (.not.IsRecvNode_I(iNode1))then
                  IsRecvNode_I(iNode1) =.true.
                  nNode=nNode+1
               endif

               if (.not.IsRecvNode_I(iNode2))then
                  IsRecvNode_I(iNode2) =.true.
                  nNode=nNode+1
               endif

               if (.not.IsRecvNode_I(iNode3))then
                  IsRecvNode_I(iNode3) =.true.
                  nNode=nNode+1
               endif
             end associate
          endif
       end do
       
       !\
       ! Allocate needed arrays now that we know number of remap points & nodes
       !/
       if (allocated(StateRecv_CVI)) deallocate(StateRecv_CVI)
       allocate(StateRecv_CVI(nAlt,nVar,nNode))

       if(UseParticles) then
          call allocate_particle_recv(nNode,nTotalLine)
       endif
       
       !\
       ! Loop over plan and post all recieves
       !/
       !reset recv node array to false
       IsRecvNode_I(:) =.false.
       !set the node map to -1 
       iNode_I(:) =-1

       iNode=0
       nRequest=0
       do iLine=1,nPointsToRemapN
          if (iProc == RemapN_I(iLine)%iProc) then
             associate(&
                  iNode1 => RemapN_I(iLine)%iNode1,&
                  iNode2 => RemapN_I(iLine)%iNode2,&
                  iNode3 => RemapN_I(iLine)%iNode3,&
                  iNodeLocal1 => RemapN_I(iLine)%iNodeLocal1,&
                  iNodeLocal2 => RemapN_I(iLine)%iNodeLocal2,&
                  iNodeLocal3 => RemapN_I(iLine)%iNodeLocal3,&
                  nParticle1 => RemapN_I(iLine)%nParticle1,&
                  nParticle2 => RemapN_I(iLine)%nParticle2,&
                  nParticle3 => RemapN_I(iLine)%nParticle3,&
                  iProc1 => RemapN_I(iLine)%iProc1,&
                  iProc2 => RemapN_I(iLine)%iProc2,&
                  iProc3 => RemapN_I(iLine)%iProc3)
               
               !check if node is already listed to recv if not update local node
               ! index and post a recv or just copy if it is a local line
               if (.not.IsRecvNode_I(iNode1))then
                  IsRecvNode_I(iNode1) =.true.
                  iNode=iNode+1
                  !set index array for node number (how to translate global
                  !node number to passed list)
                  iNode_I(iNode1)=iNode
                  ! if the needed node is local then copy the state, if not
                  ! then post a non blocking recv for node state. Use the
                  ! global index as the recieve tag
                  if (iProc == iProc1) then
                     StateRecv_CVI(:,:,iNode)=State_CVI(:,:,iNodeLocal1)
                  else
                     nRequest=nRequest+1
                     call MPI_irecv(StateRecv_CVI(:,:,iNode),nAlt*nVar,MPI_REAL,&
                          iProc1,iNode1,&
                          iComm,iRequest_I(nRequest),iError)
                  endif
                  if(UseParticles) then
                     call post_particle_line_recv(iNode,nParticle1,iProc1,&
                          iNode1,iNodeLocal1,nRequest)
                  endif
               endif
               if (.not.IsRecvNode_I(iNode2))then
                  IsRecvNode_I(iNode2) =.true.
                  iNode=iNode+1
                  !set index array for node number (how to translate global
                  !node number to passed list)
                  iNode_I(iNode2)=iNode
                  ! if the needed node is local then copy the state, if not
                  ! then post a non blocking recv for node state. Use the
                  ! global index as the recieve tag
                  if (iProc == iProc2) then
                     StateRecv_CVI(:,:,iNode)=State_CVI(:,:,iNodeLocal2)
                  else
                     nRequest=nRequest+1
                     call MPI_irecv(StateRecv_CVI(:,:,iNode),nAlt*nVar,MPI_REAL,&
                          iProc2,iNode2,&
                          iComm,iRequest_I(nRequest),iError)
                  endif
                  if(UseParticles) then
                     call post_particle_line_recv(iNode,nParticle2,iProc2,&
                          iNode2,iNodeLocal2,nRequest)
                  endif
               endif
               if (.not.IsRecvNode_I(iNode3))then
                  IsRecvNode_I(iNode3) =.true.
                  iNode=iNode+1
                  !set index array for node number (how to translate global
                  !node number to passed list)
                  iNode_I(iNode3)=iNode
                  ! if the needed node is local then copy the state, if not
                  ! then post a non blocking recv for node state. Use the
                  ! global index as the recieve tag
                  if (iProc == iProc3) then
                     StateRecv_CVI(:,:,iNode)=State_CVI(:,:,iNodeLocal3)
                  else
                     nRequest=nRequest+1
                     call MPI_irecv(StateRecv_CVI(:,:,iNode),nAlt*nVar,MPI_REAL,&
                          iProc3,iNode3,&
                          iComm,iRequest_I(nRequest),iError)
                  endif
                  if(UseParticles) then
                     call post_particle_line_recv(iNode,nParticle3,iProc3,&
                          iNode3,iNodeLocal3,nRequest)
                  endif
               endif
             end associate
          endif
       end do

       !make sure all recv are posted before starting the sends
       call MPI_barrier(iComm,iError)

       !\
       ! Loop over plan and post all sends
       !/
       !reset recv node array to false
       IsSendNode_IP(:,:) =.false.
       
       do iLine=1,nPointsToRemapN
          !check node proc against current proc
          associate(&
               iNode1 => RemapN_I(iLine)%iNode1,&
               iNode2 => RemapN_I(iLine)%iNode2,&
               iNode3 => RemapN_I(iLine)%iNode3,&
               iNodeLocal1 => RemapN_I(iLine)%iNodeLocal1,&
               iNodeLocal2 => RemapN_I(iLine)%iNodeLocal2,&
               iNodeLocal3 => RemapN_I(iLine)%iNodeLocal3,&
               iProcRecv => RemapN_I(iLine)%iProc,&
               iProc1 => RemapN_I(iLine)%iProc1,&
               iProc2 => RemapN_I(iLine)%iProc2,&
               iProc3 => RemapN_I(iLine)%iProc3)
            
            if (iProc == iProc1) then
               !check if node is already listed to send to a particular proc
               if (.not.IsSendNode_IP(iNode1,iProcRecv))then
                  IsSendNode_IP(iNode1,iProcRecv) =.true.
                  !if recieving proc is same as current proc no send, otherwise
                  !send the state information. The message tag is the global
                  !proc number
                  if (iProc /= iProcRecv) then
                     call MPI_send(State_CVI(:,:,iNodeLocal1),nAlt*nVar,MPI_REAL,&
                          iProcRecv,iNode1,iComm,iError)
                     if(UseParticles) then
                        call post_particle_line_send(iNodeLocal1,iProcRecv,&
                             iNode1)
                     endif
                  endif
               endif
            end if
          
            if (iProc == iProc2) then
               !check if node is already listed to send to a particular proc
               if (.not.IsSendNode_IP(iNode2,iProcRecv))then
                  IsSendNode_IP(iNode2,iProcRecv) =.true.
                  !if recieving proc is same as current proc no send, otherwise
                  !send the state information. The message tag is the global
                  !proc number
                  if (iProc /= iProcRecv) then
                     call MPI_send(State_CVI(:,:,iNodeLocal2),nAlt*nVar,MPI_REAL,&
                          iProcRecv,iNode2,iComm,iError)
                     if(UseParticles) then
                        call post_particle_line_send(iNodeLocal2,iProcRecv,&
                             iNode2)
                     endif
                  endif
               endif
            endif

            if (iProc == iProc3) then
               !check if node is already listed to send to a particular proc
               if (.not.IsSendNode_IP(iNode3,iProcRecv))then
                  IsSendNode_IP(iNode3,iProcRecv) =.true.
                  !if recieving proc is same as current proc no send, otherwise
                  !send the state information. The message tag is the global
                  !proc number
                  if (iProc /= iProcRecv) then
                     !if (iProc==5) write(*,*) iNodeLocal3,iProc,nLine,iLine
                     call MPI_send(State_CVI(:,:,iNodeLocal3),nAlt*nVar,MPI_REAL,&
                          iProcRecv,iNode3,iComm,iError)
                     if(UseParticles) then
                        call post_particle_line_send(iNodeLocal3,iProcRecv,&
                             iNode3)
                     endif
                  endif
               endif
            endif
          end associate
       end do
       
       !\
       ! Loop over requests and make sure recieves have completed 
       !/
       do iRequest=1,nRequest
          call MPI_wait(iRequest_I(iRequest),iStatus_I,iError)
       enddo
       !now check the particle recieves
       if(UseParticles) then
          call check_particle_recv(nRequest)
       endif
       call MPI_barrier(iComm,iError)
       
       !\
       ! Loop over plan and apply the remap
       !/
       do iLine=1,nPointsToRemapN
          if (iProc == RemapN_I(iLine)%iProc) then
             associate(&
                  iNode1 => RemapN_I(iLine)%iNode1,&
                  iNode2 => RemapN_I(iLine)%iNode2,&
                  iNode3 => RemapN_I(iLine)%iNode3,&
                  weight1 => RemapN_I(iLine)%weight1,&
                  weight2 => RemapN_I(iLine)%weight2,&
                  weight3 => RemapN_I(iLine)%weight3,&
                  iLineLocal => RemapN_I(iLine)%iLineLocal,&
                  Theta      => RemapN_I(iLine)%Theta,&
                  Phi        => RemapN_I(iLine)%Phi)

               !if (iProc==0 .and. iLineLocal>1) then
               !   write(*,*) 'iLineLocal', iLineLocal
               !   write(*,*) 'Theta,Phi',Theta,Phi
               !   write(*,*) 'node1', Lines_I(iNode1)%theta,Lines_I(iNode1)%phi,weight1
               !   write(*,*) 'node2', Lines_I(iNode2)%theta,Lines_I(iNode2)%phi,weight2
               !   write(*,*) 'node3', Lines_I(iNode3)%theta,Lines_I(iNode3)%phi,weight3
               !   write(*,*) 'iNode_I(iNode1)',iNode_I(iNode1)
               !   write(*,*) 'iNode_I(iNode2)',iNode_I(iNode2)
               !   write(*,*) 'iNode_I(iNode3)',iNode_I(iNode3)
               !   !call con_stop('')
               !endif
               
               !interpolate solution to make new state vars
                State_CVI(:,:,iLineLocal) = &
                    weight1*StateRecv_CVI(:,:,iNode_I(iNode1)) &
                    +weight2*StateRecv_CVI(:,:,iNode_I(iNode2))&
                    +weight3*StateRecv_CVI(:,:,iNode_I(iNode3))

                if(UseParticles) then
                   call interp_particle_line(weight1,weight2,weight3,&
                        iNode_I(iNode1),iNode_I(iNode2),iNode_I(iNode3),&
                        iLineLocal)
                end if
                
           !     write(*,*) 'before theta phi', ThetaLine_I(iLineLocal),PhiLine_I(iLineLocal)
                               
               !update position
               ThetaLine_I(iLineLocal) = Theta
               PhiLine_I(iLineLocal) = Phi
               ! Get the GeoMagnetic latitude and longitude 
               GeoMagLat_I(iLineLocal) = &
                    90.0 - ThetaLine_I(iLineLocal)*cRadToDeg
               GeoMagLon_I(iLineLocal) = &
                    PhiLine_I(iLineLocal)*cRadToDeg

               xLine_I(iLineLocal)      = &
                    rLowerBoundary*sin(ThetaLine_I(iLineLocal))&
                    *cos(PhiLine_I(iLineLocal))
               
               yLine_I(iLineLocal)      = &
                    rLowerBoundary*sin(ThetaLine_I(iLineLocal))&
                    *sin(PhiLine_I(iLineLocal))
               
               zLine_I(iLineLocal)      = &
                    rLowerBoundary*cos(ThetaLine_I(iLineLocal))
               
               xLineOld_I(iLineLocal)   = xLine_I(iLineLocal)
               yLineOld_I(iLineLocal)   = yLine_I(iLineLocal)
               zLineOld_I(iLineLocal)   = zLine_I(iLineLocal)
          !     write(*,*) 'after theta phi', ThetaLine_I(iLineLocal),PhiLine_I(iLineLocal)
                              
             end associate
          end if
          
       end do
    endif
    
    !\
    ! Apply regrid plan to Southern hemisphere lines
    !/

    if (nSouth>0) then

       nNode = 0
       nRemapLocal = 0
       IsRecvNode_I(:) =.false.
       
       ! go through plan and count up number of remaps on given proc
       ! count up also how many nodes are to be sent to this proc.
       do iLine=1,nPointsToRemapS
          if (iProc == RemapS_I(iLine)%iProc) then
             nRemapLocal = nRemapLocal + 1
             associate(&
                  iNode1 => RemapS_I(iLine)%iNode1,&
                  iNode2 => RemapS_I(iLine)%iNode2,&
                  iNode3 => RemapS_I(iLine)%iNode3,&
                  iNodeLocal1 => RemapS_I(iLine)%iNodeLocal1,&
                  iNodeLocal2 => RemapS_I(iLine)%iNodeLocal2,&
                  iNodeLocal3 => RemapS_I(iLine)%iNodeLocal3,&
                  iProc1 => RemapS_I(iLine)%iNode1,&
                  iProc2 => RemapS_I(iLine)%iNode2,&
                  iProc3 => RemapS_I(iLine)%iNode3)
               
               !check if node is already listed to recv if not add it to the
               !total count
               if (.not.IsRecvNode_I(iNode1))then
                  IsRecvNode_I(iNode1) =.true.
                  nNode=nNode+1
               endif

               if (.not.IsRecvNode_I(iNode2))then
                  IsRecvNode_I(iNode2) =.true.
                  nNode=nNode+1
               endif

               if (.not.IsRecvNode_I(iNode3))then
                  IsRecvNode_I(iNode3) =.true.
                  nNode=nNode+1
               endif
             end associate
          endif
       end do
       
       !\
       ! Allocate needed arrays now that we know number of remap points & nodes
       !/
       if (allocated(StateRecv_CVI)) deallocate(StateRecv_CVI)
       allocate(StateRecv_CVI(nAlt,nVar,nNode))

       if(UseParticles) then
          call allocate_particle_recv(nNode,nTotalLine)
       endif
       
       !\
       ! Loop over plan and post all recieves
       !/
       !reset recv node array to false
       IsRecvNode_I(:) =.false.
       !set the node map to -1 
       iNode_I(:) =-1

       iNode=0
       nRequest=0
       do iLine=1,nPointsToRemapS
          if (iProc == RemapS_I(iLine)%iProc) then
             associate(&
                  iNode1 => RemapS_I(iLine)%iNode1,&
                  iNode2 => RemapS_I(iLine)%iNode2,&
                  iNode3 => RemapS_I(iLine)%iNode3,&
                  iNodeLocal1 => RemapS_I(iLine)%iNodeLocal1,&
                  iNodeLocal2 => RemapS_I(iLine)%iNodeLocal2,&
                  iNodeLocal3 => RemapS_I(iLine)%iNodeLocal3,&
                  nParticle1 => RemapS_I(iLine)%nParticle1,&
                  nParticle2 => RemapS_I(iLine)%nParticle2,&
                  nParticle3 => RemapS_I(iLine)%nParticle3,&
                  iProc1 => RemapS_I(iLine)%iProc1,&
                  iProc2 => RemapS_I(iLine)%iProc2,&
                  iProc3 => RemapS_I(iLine)%iProc3)
               
               !check if node is already listed to recv if not update local node
               ! index and post a recv or just copy if it is a local line
               if (.not.IsRecvNode_I(iNode1))then
                  IsRecvNode_I(iNode1) =.true.
                  iNode=iNode+1
                  !set index array for node number (how to translate global
                  !node number to passed list)
                  iNode_I(iNode1)=iNode
                  ! if the needed node is local then copy the state, if not
                  ! then post a non blocking recv for node state. Use the
                  ! global index as the recieve tag
                  if (iProc == iProc1) then
                     StateRecv_CVI(:,:,iNode)=State_CVI(:,:,iNodeLocal1)
                  else
                     nRequest=nRequest+1
                     call MPI_irecv(StateRecv_CVI(:,:,iNode),nAlt*nVar,MPI_REAL,&
                          iProc1,iNode1,&
                          iComm,iRequest_I(nRequest),iError)
                  endif
                  if(UseParticles) then
                     call post_particle_line_recv(iNode,nParticle1,iProc1,&
                          iNode1,iNodeLocal1,nRequest)
                  endif
               endif
               if (.not.IsRecvNode_I(iNode2))then
                  IsRecvNode_I(iNode2) =.true.
                  iNode=iNode+1
                  !set index array for node number (how to translate global
                  !node number to passed list)
                  iNode_I(iNode2)=iNode
                  ! if the needed node is local then copy the state, if not
                  ! then post a non blocking recv for node state. Use the
                  ! global index as the recieve tag
                  if (iProc == iProc2) then
                     StateRecv_CVI(:,:,iNode)=State_CVI(:,:,iNodeLocal2)
                  else
                     nRequest=nRequest+1
                     call MPI_irecv(StateRecv_CVI(:,:,iNode),nAlt*nVar,MPI_REAL,&
                          iProc2,iNode2,&
                          iComm,iRequest_I(nRequest),iError)
                  endif
                  if(UseParticles) then
                     call post_particle_line_recv(iNode,nParticle2,iProc2,&
                          iNode2,iNodeLocal2,nRequest)
                  endif
               endif
               if (.not.IsRecvNode_I(iNode3))then
                  IsRecvNode_I(iNode3) =.true.
                  iNode=iNode+1
                  !set index array for node number (how to translate global
                  !node number to passed list)
                  iNode_I(iNode3)=iNode
                  ! if the needed node is local then copy the state, if not
                  ! then post a non blocking recv for node state. Use the
                  ! global index as the recieve tag
                  if (iProc == iProc3) then
                     StateRecv_CVI(:,:,iNode)=State_CVI(:,:,iNodeLocal3)
                  else
                     nRequest=nRequest+1
                     call MPI_irecv(StateRecv_CVI(:,:,iNode),nAlt*nVar,MPI_REAL,&
                          iProc3,iNode3,&
                          iComm,iRequest_I(nRequest),iError)
                  endif
                  if(UseParticles) then
                     call post_particle_line_recv(iNode,nParticle3,iProc3,&
                          iNode3,iNodeLocal3,nRequest)
                  endif
               endif
             end associate
          endif
       end do

       !make sure all recv are posted before starting the sends
       call MPI_barrier(iComm,iError)

       !\
       ! Loop over plan and post all sends
       !/
       !reset recv node array to false
       IsSendNode_IP(:,:) =.false.

       do iLine=1,nPointsToRemapS
          !check node proc against current proc
          associate(&
               iNode1 => RemapS_I(iLine)%iNode1,&
               iNode2 => RemapS_I(iLine)%iNode2,&
               iNode3 => RemapS_I(iLine)%iNode3,&
               iNodeLocal1 => RemapS_I(iLine)%iNodeLocal1,&
               iNodeLocal2 => RemapS_I(iLine)%iNodeLocal2,&
               iNodeLocal3 => RemapS_I(iLine)%iNodeLocal3,&
               iProcRecv => RemapS_I(iLine)%iProc,&
               iProc1 => RemapS_I(iLine)%iProc1,&
               iProc2 => RemapS_I(iLine)%iProc2,&
               iProc3 => RemapS_I(iLine)%iProc3)
            
            if (iProc == iProc1) then
               !check if node is already listed to send to a particular proc
               if (.not.IsSendNode_IP(iNode1,iProcRecv))then
                  IsSendNode_IP(iNode1,iProcRecv) =.true.
                  !if recieving proc is same as current proc no send, otherwise
                  !send the state information. The message tag is the global
                  !proc number
                  if (iProc /= iProcRecv) then
                     call MPI_send(State_CVI(:,:,iNodeLocal1),nAlt*nVar,MPI_REAL,&
                          iProcRecv,iNode1,iComm,iError)
                     if(UseParticles) then
                        call post_particle_line_send(iNodeLocal1,iProcRecv,&
                             iNode1)
                     endif
                  endif
               endif
            end if
          
            if (iProc == iProc2) then
               !check if node is already listed to send to a particular proc
               if (.not.IsSendNode_IP(iNode2,iProcRecv))then
                  IsSendNode_IP(iNode2,iProcRecv) =.true.
                  !if recieving proc is same as current proc no send, otherwise
                  !send the state information. The message tag is the global
                  !proc number
                  if (iProc /= iProcRecv) then
                     call MPI_send(State_CVI(:,:,iNodeLocal2),nAlt*nVar,MPI_REAL,&
                          iProcRecv,iNode2,iComm,iError)
                     if(UseParticles) then
                        call post_particle_line_send(iNodeLocal2,iProcRecv,&
                             iNode2)
                     endif
                  endif
               endif
            endif

            if (iProc == iProc3) then
               !check if node is already listed to send to a particular proc
               if (.not.IsSendNode_IP(iNode3,iProcRecv))then
                  IsSendNode_IP(iNode3,iProcRecv) =.true.
                  !if recieving proc is same as current proc no send, otherwise
                  !send the state information. The message tag is the global
                  !proc number
                  if (iProc /= iProcRecv) then
                     !if (iProc==5) write(*,*) iNodeLocal3,iProc,nLine,iLine
                     call MPI_send(State_CVI(:,:,iNodeLocal3),nAlt*nVar,MPI_REAL,&
                          iProcRecv,iNode3,iComm,iError)
                     if(UseParticles) then
                        call post_particle_line_send(iNodeLocal3,iProcRecv,&
                             iNode3)
                     endif
                  endif
               endif
            endif
          end associate
       end do
       
       !\
       ! Loop over requests and make sure recieves have completed 
       !/
       do iRequest=1,nRequest
          call MPI_wait(iRequest_I(iRequest),iStatus_I,iError)
       enddo

       !now check the particle recieves
       if(UseParticles) then
          call check_particle_recv(nRequest)
       endif

       call MPI_barrier(iComm,iError)

       !\
       ! Loop over plan and apply the remap
       !/
       do iLine=1,nPointsToRemapS
          if (iProc == RemapS_I(iLine)%iProc) then
             associate(&
                  iNode1 => RemapS_I(iLine)%iNode1,&
                  iNode2 => RemapS_I(iLine)%iNode2,&
                  iNode3 => RemapS_I(iLine)%iNode3,&
                  weight1 => RemapS_I(iLine)%weight1,&
                  weight2 => RemapS_I(iLine)%weight2,&
                  weight3 => RemapS_I(iLine)%weight3,&
                  iLineLocal => RemapS_I(iLine)%iLineLocal,&
                  Theta      => RemapS_I(iLine)%Theta,&
                  Phi        => RemapS_I(iLine)%Phi)

               !interpolate solution to make new state vars
               State_CVI(:,:,iLineLocal) = &
                    weight1*StateRecv_CVI(:,:,iNode_I(iNode1)) &
                    +weight2*StateRecv_CVI(:,:,iNode_I(iNode2))&
                    +weight3*StateRecv_CVI(:,:,iNode_I(iNode3))

               if(UseParticles) then
                  call interp_particle_line(weight1,weight2,weight3,&
                       iNode_I(iNode1),iNode_I(iNode2),iNode_I(iNode3),&
                       iLineLocal)
               end if
               
               !update position
               ThetaLine_I(iLineLocal) = Theta
               PhiLine_I(iLineLocal) = Phi
               ! Get the GeoMagnetic latitude and longitude 
               GeoMagLat_I(iLineLocal) = &
                    90.0 - ThetaLine_I(iLineLocal)*cRadToDeg
               GeoMagLon_I(iLineLocal) = &
                    PhiLine_I(iLineLocal)*cRadToDeg
               xLine_I(iLineLocal)      = &
                    rLowerBoundary*sin(ThetaLine_I(iLineLocal))&
                    *cos(PhiLine_I(iLineLocal))
               
               yLine_I(iLineLocal)      = &
                    rLowerBoundary*sin(ThetaLine_I(iLineLocal))&
                    *sin(PhiLine_I(iLineLocal))
               
               zLine_I(iLineLocal)      = &
                    rLowerBoundary*cos(ThetaLine_I(iLineLocal))
     
               xLineOld_I(iLineLocal)   = xLine_I(iLineLocal)
               yLineOld_I(iLineLocal)   = yLine_I(iLineLocal)
               zLineOld_I(iLineLocal)   = zLine_I(iLineLocal)
             end associate
          end if
          
       end do
    endif
    

  end subroutine apply_regrid

  !============================================================================
  ! main subroutine call to build and execute the regrid plan
  subroutine regrid_lines
    logical,save :: IsFirstCall = .true.
    !---------------------------------------------------------------------------
    
    if(IsFirstCall) then
       call init_regrid
       IsFirstCall=.false.
    end if

    if (DoAdaptGrid .and. iProc==0) then
       call get_triangulation_base_remap_grid
       call adapt_remap_grid
    endif
    !if(iProc==0) then
       !write(*,*) 'line list before update_grid_info'
       ! call print_line_list
    !endif
    !gather grid info from all procs to zero proc
    call update_grid_info

    !if(iProc==0) then
    !   write(*,*) 'line list after update_grid_info, time = ', Time
    !   call print_line_list
    !endif
    
    !zero proc gets the triangulation for the grid and build the regrid plan
    if(iProc==0) then
       call get_triangulation
       call set_anchor_points
       call set_regrid_plan

       !save ponts and triangulation before applying the regrid plan
       if (DoSavePoints)&
            call save_plot_points
    endif
    
    !if(iProc==0) then
    !   write(*,*) 'print map at time = ', Time
    !   call print_map
    !endif
    
    !distribute the regrid plan to all Procs
    call distribute_regrid_plan

    !apply the regrid plan on all procs
    call apply_regrid

    if(iProc==0) then
       write(*,*) 'PW Regrid at Time = ', Time
       write(*,*) 'PW Regrid North points = ', nPointsToRemapN
       write(*,*) 'PW Regrid South points = ', nPointsToRemapS
    endif
  end subroutine regrid_lines
  !==========================================================================
  subroutine print_line_list
    integer:: iLine
    !-------------------------------------------------------------------------
    do iLine=1,nTotalLine
       write(*,*) 'iLine = ', iLine
       write(*,*) '    iProc      ', Lines_I(iLine)%iProc
       write(*,*) '    iLineLocal ', Lines_I(iLine)%iLineLocal
       write(*,*) '    Xyz_D      ', Lines_I(iLine)%Xyz_D
       write(*,*) '    Theta      ', Lines_I(iLine)%Theta
       write(*,*) '    Phi        ', Lines_I(iLine)%Phi
       write(*,*) '    Lat        ', Lines_I(iLine)%Lat
       write(*,*) '    Lon        ', Lines_I(iLine)%Lon
       write(*,*) '    IsNorth    ', Lines_I(iLine)%IsNorth
    enddo
  end subroutine print_line_list

  !==========================================================================
  subroutine print_map
    integer:: iLine
    !-------------------------------------------------------------------------
    do iLine=1,nPointsToRemapN
       write(*,*) 'iLine remap =  ', iLine
       write(*,*) '    iLineGlobal', RemapN_I(iLine)%iLineGlobal
       write(*,*) '    iLineLocal ', RemapN_I(iLine)%iLineLocal
       write(*,*) '    iProc      ', RemapN_I(iLine)%iProc
       write(*,*) '    Theta      ', RemapN_I(iLine)%Theta
       write(*,*) '    Phi        ', RemapN_I(iLine)%Phi
       write(*,*) '    iNode1     ', RemapN_I(iLine)%iNode1
       write(*,*) '    iNode2     ', RemapN_I(iLine)%iNode2
       write(*,*) '    iNode3     ', RemapN_I(iLine)%iNode3
       write(*,*) '    iNodeLocal1', RemapN_I(iLine)%iNodeLocal1
       write(*,*) '    iNodeLocal2', RemapN_I(iLine)%iNodeLocal2
       write(*,*) '    iNodeLocal3', RemapN_I(iLine)%iNodeLocal3
       write(*,*) '    weight1     ', RemapN_I(iLine)%weight1
       write(*,*) '    weight2     ', RemapN_I(iLine)%weight2
       write(*,*) '    weight3     ', RemapN_I(iLine)%weight3
    enddo
  end subroutine print_map

  !==========================================================================
  ! routine to save the current points and triangulation ahead of remap
  subroutine save_plot_points
    use ModPWOM, only: Time
    use ModIoUnit, ONLY: UnitTmp_
    use ModTriangulateSpherical,ONLY:trlist
    integer,parameter :: nRow=6
    integer,allocatable :: ltri_II(:,:)
    integer :: nTriangle,iError,iTriangle,iNode,iTimeOut
    Character(len=100) :: NameFile
    !--------------------------------------------------------------------
    !save the north triangulation first
    if (nNorth>0) then
       iTimeOut=int(Time)
       write(NameFile,"(a,i8.8,a)") &
            'PW/plots/NorthPoints_Time',iTimeOut,'.dat'
       open(UnitTmp_,file=NameFile)
       
       if (allocated(ltri_II) ) deallocate(ltri_II)
       allocate(ltri_II(nRow,2*nNorth-4))
       call trlist ( nNorth, listN_I, lptrN_I, lendN_I, nrow, &
            nTriangle, ltri_II, iError)
       
       write(UnitTmp_,'(a)') &
            'VARIABLES = "X"  "Y" "Z"'
       write(UnitTmp_,'(a)') &
            'ZONE T="Triangulation North"'
       write(UnitTmp_,'(a,1es18.10)') &
            'SolutionTime = ', Time
       write(UnitTmp_,'(a,i3,a,i3,a)') 'NODES =', nNorth, &
            ', ELEMENTS = ',nTriangle, &
            ', DATAPACKING=POINT, ZONETYPE=FETRIANGLE'
       do iNode=1,nNorth
          write(UnitTmp_,"(100es18.10)") Lines_I(iIndexNorth_I(iNode))%Xyz_D
       enddo
       do iTriangle=1,nTriangle
          write(UnitTmp_,'(i4,i4,i4)') ltri_II(1:3,iTriangle)
       enddo
       close(UnitTmp_)
    endif

    !now svae the south points
    if (nSouth>0) then
       iTimeOut=int(Time)
       write(NameFile,"(a,i8.8,a)") &
            'PW/plots/SouthPoints_Time',iTimeOut,'.dat'
       open(UnitTmp_,file=NameFile)
       
       if (allocated(ltri_II) ) deallocate(ltri_II)
       allocate(ltri_II(nRow,2*nSouth-4))
       call trlist ( nSouth, listS_I, lptrS_I, lendS_I, nrow, &
            nTriangle, ltri_II, iError)
       
       write(UnitTmp_,'(a)') &
            'VARIABLES = "X"  "Y" "Z"'
       write(UnitTmp_,'(a)') &
            'ZONE T="Triangulation South"'
       write(UnitTmp_,'(a,1es18.10)') &
            'SolutionTime = ', Time
       write(UnitTmp_,'(a,i3,a,i3,a)') 'NODES =', nSouth, &
            ', ELEMENTS = ',nTriangle, &
            ', DATAPACKING=POINT, ZONETYPE=FETRIANGLE'
       do iNode=1,nSouth
          write(UnitTmp_,"(100es18.10)") Lines_I(iIndexSouth_I(iNode))%Xyz_D
       enddo
       do iTriangle=1,nTriangle
          write(UnitTmp_,'(i4,i4,i4)') ltri_II(1:3,iTriangle)
       enddo
       close(UnitTmp_)
    endif
    
  end subroutine save_plot_points
  
end Module ModReGrid
