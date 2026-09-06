module ModCimiPlot

  use ModCimiGrid,	ONLY: nspec
  implicit none

  private ! except
  public :: Cimi_plot_eq, Cimi_plot_iono, Cimi_plot_fls, Cimi_plot_psd, &
       cimi_plot_log, cimi_plot_precip, Cimi_plot_boundary_check,&
       Cimi_plot_vl, Cimi_plot_vp, Cimi_plot_Lstar
  
  character(len=5), 		public	::	TypePlot = 'ascii'

  logical,			public	::	DoSaveLog = .false.
  real,				public	::	DtLogOut = 60.0
  real, 			public	::	DtOutput = 60.0
  
  logical,			public	::	DoSavePlot = .false.

  logical,			public	:: 	&
       DoSaveEq = .false.
  logical, 			public	::	&
       DoSaveSeparateEqFiles = .false.
  real,    			public	::	&
       DtEqOutput = 60.0
  
  logical,			public	::	&
       DoSaveIono = .false.
  logical, 			public	::	&
       DoSaveSeparateIonoFiles = .false.
  real,    			public	::	&
       DtIonoOutput = 60.0


  logical, 			public	::	&
       DoSaveLstar = .false.
  logical, 			public	::	&
       DoSaveSeparateLstarFiles = .false.
  real,    			public	::	&
       DtLstarOutput = 60.0

  logical, dimension( nspec ),	public	::	&
       DoSaveFlux = .false.
  logical, dimension( nspec ),	public	::	&
       DoSaveSeparateFluxFiles = .false.
  real,    dimension( nspec ),	public	::	&
       DtFluxOutput = 60.0
  
  logical, dimension( nspec ),	public	::	&
       DoSavePSD = .false.
  logical, dimension( nspec ),	public	::	&
       DoSaveSeparatePSDFiles = .false.
  real,    dimension( nspec ),	public	::	&
       DtPSDOutput = 60.0
  
  logical, dimension( nspec ),	public	::	&
       DoSaveVLDrift = .false.
  logical, dimension( nspec ),	public	::	&
       DoSaveSeparateVLDriftFiles = .false.
  real,    dimension( nspec ),	public	::	&
       DtVLDriftOutput = 60.0
  
  logical, dimension( nspec ),	public	::	&
       DoSaveVPDrift = .false.
  logical, dimension( nspec ),	public	::	&
       DoSaveSeparateVPDriftFiles = .false.
  real,    dimension( nspec ),	public	::	&
       DtVPDriftOutput = 60.0

  logical, dimension( nspec ),	public	::	&
       DoSavePreci = .false.
  logical, dimension( nspec ),	public	::	&
       DoSaveSeparatePreciFiles = .false.
  real,    dimension( nspec ),	public	::	&
       DtPreciOutput = 3600.0
  
  character(len=*), parameter :: NameHeader = 'CIMI output'

contains
  subroutine Cimi_plot_eq(nLat, nLon, X_C, Y_C, &
       Pressure_IC, PressurePar_IC, PressureHot_IC, PparHot_IC, &
       Den_IC, Beq_C, Volume_C, Potential_C, FAC_C, Time, Dt)
    
    use ModIoUnit,     		ONLY:	UnitTmp_
    use ModPlotFile,		ONLY:	save_plot_file
    use ModCimiRestart,		ONLY:	IsRestart
    use ModCimiPlanet,		ONLY:	nspec, NamePlotVar, &
         iPplot_I, iPparplot_I, iPhotplot_I, iPparhotplot_I, iNplot_I, &
         Beq_, Vol_, Pot_, FAC_, Lstar_, Plas_, nVar
    use ModLstar, 		ONLY:	Lstar_C
    use ModCimiGrid,		ONLY:	&
         PhiIono_C => phi, LatIono_C => xlatr
    use ModCimiTrace,		ONLY:	iba
    use ModPlasmasphere,	ONLY:   UseCorePsModel, PlasDensity_C
    
    integer, intent(in) :: nLat, nLon
    real,    intent(in) :: X_C(nLat,nLon), Y_C(nLat,nLon), Time, Dt
    real,    intent(in) :: Pressure_IC(nspec,nLat,nLon), &
                           PressurePar_IC(nspec,nLat,nLon), &
                           Den_IC(nspec,nLat,nLon), & 
                           Beq_C(nLat,nLon),Volume_C(nLat,nLon),   &
                           Potential_C(nLat,nLon), &
                           PressureHot_IC(nspec,nLat,nLon), &
                           PparHot_IC(nspec,nLat,nLon), &
                           FAC_C(nLat,nLon)
    real, allocatable   :: Coord_DII(:,:,:), PlotState_IIV(:,:,:)
    integer             :: iLat,iLon,iSpecies, nprint
    integer, parameter  :: x_=1, y_=2, nDim=2
    real                :: Theta, Phi
    character(len=35), save   :: NamePlotEq
    character(len=6)    :: TypePosition  ! 'rewind' or 'append'
    real, parameter     :: Gamma = 5./3., rBody = 1.0
    logical, save       :: IsFirstCall = .true.

    !--------------------------------------------------------------------------

    nprint = nint( Time / dt )

    if ( IsFirstCall ) &
         write(NamePlotEq,"(A17,I8.8,A5)") "IM/plots/CIMIeq_n",nprint,".outs"
        
    allocate(Coord_DII(nDim,nLat,nLon+1), PlotState_IIV(nLat,nLon+1,nVar))

    PlotState_IIV = 0.0
    Coord_DII     = 0.0

    !Set Coords
    Coord_DII(x_,:, 1:nLon) = X_C(:,1:nLon)
    Coord_DII(y_,:, 1:nLon) = Y_C(:,1:nLon)
    
    !fill ghost cells of Coords
    Coord_DII(x_,:, nLon+1) = X_C(:,1)
    Coord_DII(y_,:, nLon+1) = Y_C(:,1)
    
    !Set plot data
    do iSpecies = 1, nspec
       do iLon = 1, nLon
       PlotState_IIV( 1 : iba( iLon ), iLon, iPplot_I( iSpecies + 1 ) ) = &
            Pressure_IC( iSpecies, 1 : iba( iLon ), iLon ) 
       PlotState_IIV( 1 : iba( iLon ), iLon, iPparplot_I( iSpecies + 1 ) ) = &
            PressurePar_IC( iSpecies, 1 : iba( iLon ), iLon ) 
       PlotState_IIV( 1 : iba( iLon ), iLon, iPhotplot_I( iSpecies + 1 ) ) = &
            PressureHot_IC( iSpecies, 1 : iba( iLon ), iLon )
       PlotState_IIV( 1 : iba( iLon ), iLon, iPparhotplot_I( iSpecies + 1 ) ) = &
            PparHot_IC( iSpecies, 1 : iba( iLon ), iLon )
       PlotState_IIV( 1 : iba( iLon ),iLon, iNplot_I( iSpecies + 1 ) ) = &
            Den_IC( iSpecies, 1 : iba( iLon ), iLon )  
       PlotState_IIV( 1 : iba( iLon ), iLon, iPplot_I( 1 ) ) = &
            PlotState_IIV( 1 : iba( iLon ),iLon, iPplot_I( 1 ) ) + &
            Pressure_IC( iSpecies, 1 : iba( iLon ), iLon ) 
       PlotState_IIV( 1 : iba( iLon ), iLon, iPparplot_I( 1 ) ) = &
            PlotState_IIV(1:iba( iLon ), iLon, iPparplot_I( 1 ) ) + &
            PressurePar_IC( iSpecies, 1 : iba( iLon ), iLon ) 
       PlotState_IIV( 1 : iba( iLon ), iLon, iPhotplot_I( 1 ) ) = &
            PlotState_IIV(1:iba( iLon ), iLon, iPhotplot_I( 1 ) ) + &
            PressureHot_IC( iSpecies, 1 : iba( iLon ), iLon )
       PlotState_IIV( 1 : iba( iLon ),iLon, iPparhotplot_I( 1 ) ) = &
            PlotState_IIV( 1 : iba( iLon ), iLon, iPparhotplot_I( 1 ) ) + &
            PparHot_IC( iSpecies, 1 : iba( iLon ), iLon )
       PlotState_IIV( 1 : iba( iLon ), iLon, iNplot_I( 1 ) ) = &
            PlotState_IIV( 1 : iba( iLon ), iLon, iNplot_I( 1 ) ) + &
            Den_IC( iSpecies, 1 : iba( iLon ), iLon ) 
    end do
 end do
    do iLon=1,nLon
       PlotState_IIV( 1 : iba( iLon ), iLon, Beq_) = &
            Beq_C	( 1 : iba( iLon ), iLon )    
       PlotState_IIV( 1 : iba( iLon ), iLon, Vol_) = &
            Volume_C	( 1 : iba( iLon ), iLon )    
       PlotState_IIV( 1 : iba( iLon ), iLon, Pot_) = &
            Potential_C	( 1 : iba( iLon ), iLon )    
       PlotState_IIV( 1 : iba( iLon ), iLon, FAC_) = &
            FAC_C	( 1 : iba( iLon ), iLon )    
       PlotState_IIV( 1 : iba( iLon ), iLon, Lstar_) = &
            Lstar_C	( 1 : iba( iLon ), iLon )    
       PlotState_IIV( 1 : iba( iLon ), iLon, Plas_) = &
            PlasDensity_C	( 1 : iba( iLon ), iLon )
    enddo
    !fill ghost cells of plot data
    PlotState_IIV(:,nLon+1,iPplot_I(1))= &
         PlotState_IIV(:,1,iPplot_I(1))
    PlotState_IIV(:,nLon+1,iPparplot_I(1))= &
         PlotState_IIV(:,1,iPparplot_I(1))
    PlotState_IIV(:,nLon+1,iPhotplot_I(1))= &
         PlotState_IIV(:,1,iPhotplot_I(1))
    PlotState_IIV(:,nLon+1,iPparhotplot_I(1))= &
         PlotState_IIV(:,1,iPparhotplot_I(1))
    PlotState_IIV(:,nLon+1,iNplot_I(1))= &
         PlotState_IIV(:,1,iNplot_I(1))
    do iSpecies = 1,nspec
       PlotState_IIV(:,nLon+1,iPplot_I(iSpecies+1))= &
            PlotState_IIV(:,1,iPplot_I(iSpecies+1))
       PlotState_IIV(:,nLon+1,iPparplot_I(iSpecies+1))= &
            PlotState_IIV(:,1,iPparplot_I(iSpecies+1))
       PlotState_IIV(:,nLon+1,iPhotplot_I(iSpecies+1))= &
            PlotState_IIV(:,1,iPhotplot_I(iSpecies+1))
       PlotState_IIV(:,nLon+1,iPparhotplot_I(iSpecies+1))= &
            PlotState_IIV(:,1,iPparhotplot_I(iSpecies+1))
       PlotState_IIV(:,nLon+1,iNplot_I(iSpecies+1))= &
            PlotState_IIV(:,1,iNplot_I(iSpecies+1))  
    end do
    PlotState_IIV(:,nLon+1,Beq_) = Beq_C   (:,1)    
    PlotState_IIV(:,nLon+1,Vol_) = Volume_C(:,1)    
    PlotState_IIV(:,nLon+1,Pot_) = Potential_C(:,1)    
    PlotState_IIV(:,nLon+1,FAC_) = FAC_C(:,1)    
    PlotState_IIV(:,nLon+1,Lstar_) = Lstar_C(:,1)    
    PlotState_IIV(:,nLon+1,Plas_) = PlasDensity_C(:,1)
        
    TypePosition = 'append'
    if(IsFirstCall .and. .not. IsRestart) TypePosition = 'rewind'
    IsFirstCall = .false.

    call save_plot_file( NamePlotEq, TypePositionIn = TypePosition, &
         TypeFileIn = TypePlot, StringHeaderIn = NameHeader, &
         NameVarIn = NamePlotVar, &
         nStepIn = nint( Time / Dt ), TimeIn = Time, &
         nDimIn = 2, CoordIn_DII = Coord_DII, &
         VarIn_IIV = PlotState_IIV, ParamIn_I = (/ Gamma, rBody /) )
    
    deallocate( Coord_DII, PlotState_IIV )

  end subroutine Cimi_plot_eq

  subroutine Cimi_plot_iono(nLat, nLon, X_C, Y_C, &
       Pressure_IC, PressurePar_IC, PressureHot_IC, PparHot_IC, &
       Den_IC, Beq_C, Volume_C, Potential_C, FAC_C, Time, Dt)
    
    use ModIoUnit,     		ONLY:	UnitTmp_
    use ModPlotFile,		ONLY:	save_plot_file
    use ModCimiRestart,		ONLY:	IsRestart
    use ModCimiPlanet,		ONLY:	nspec, NamePlotVar, &
         iPplot_I, iPparplot_I, iPhotplot_I, iPparhotplot_I, iNplot_I, &
         Beq_, Vol_, Pot_, FAC_, Lstar_, Plas_, nVar
    use ModCimiGrid,		ONLY: 	PhiIono_C => phi, LatIono_C => xlatr
    use ModCimiTrace,		ONLY:	iba
    
    use ModPlasmasphere,	ONLY:	UseCorePsModel, PlasDensity_C
    use ModLstar,		ONLY:	Lstar_C
    
    integer, intent(in) :: nLat, nLon
    real,    intent(in) :: X_C(nLat,nLon), Y_C(nLat,nLon), Time, Dt
    real,    intent(in) :: Pressure_IC(nspec,nLat,nLon), &
                           PressurePar_IC(nspec,nLat,nLon), &
                           Den_IC(nspec,nLat,nLon), & 
                           Beq_C(nLat,nLon),Volume_C(nLat,nLon),   &
                           Potential_C(nLat,nLon), &
                           PressureHot_IC(nspec,nLat,nLon), &
                           PparHot_IC(nspec,nLat,nLon), &
                           FAC_C(nLat,nLon)
    real, allocatable   :: CoordIono_DII(:,:,:), PlotState_IIV(:,:,:)
    integer             :: iLat,iLon,iSpecies, nprint
    integer, parameter  :: x_=1, y_=2, nDim=2
    real                :: Theta, Phi
    character(len=32), save   :: NamePlotIono
    character(len=6)    :: TypePosition  ! 'rewind' or 'append'
    real, parameter     :: Gamma = 5./3., rBody = 1.0
    logical, save       :: IsFirstCall = .true.

    !--------------------------------------------------------------------------

    nprint = nint( Time / dt )

    if ( IsFirstCall ) &
         write(NamePlotIono,"(A19,I8.8,A5)") "IM/plots/CIMIiono_n",nprint,".outs"
        
    allocate(CoordIono_DII(nDim,nLat,nLon+1), PlotState_IIV(nLat,nLon+1,nVar))

    PlotState_IIV = 0.0
    CoordIono_DII = 0.0

    !Set Coords
    do iLon = 1,nLon
       do iLat = 1,nLat
          CoordIono_DII(x_,iLat, iLon) = &
               cos(LatIono_C(iLat))*cos(PhiIono_C(iLon))
          CoordIono_DII(y_,iLat, iLon) = &
               cos(LatIono_C(iLat))*sin(PhiIono_C(iLon))
       enddo
    enddo

    !fill ghost cells of Coords
    CoordIono_DII(x_,:, nLon+1) = CoordIono_DII(x_,:, 1)
    CoordIono_DII(y_,:, nLon+1) = CoordIono_DII(y_,:, 1)

    !Set plot data
    do iSpecies = 1, nspec
       do iLon = 1, nLon
       PlotState_IIV( 1 : iba( iLon ), iLon, iPplot_I( iSpecies + 1 ) ) = &
            Pressure_IC( iSpecies, 1 : iba( iLon ), iLon ) 
       PlotState_IIV( 1 : iba( iLon ), iLon, iPparplot_I( iSpecies + 1 ) ) = &
            PressurePar_IC( iSpecies, 1 : iba( iLon ), iLon ) 
       PlotState_IIV( 1 : iba( iLon ), iLon, iPhotplot_I( iSpecies + 1 ) ) = &
            PressureHot_IC( iSpecies, 1 : iba( iLon ), iLon )
       PlotState_IIV( 1 : iba( iLon ), iLon, iPparhotplot_I( iSpecies + 1 ) ) = &
            PparHot_IC( iSpecies, 1 : iba( iLon ), iLon )
       PlotState_IIV( 1 : iba( iLon ),iLon, iNplot_I( iSpecies + 1 ) ) = &
            Den_IC( iSpecies, 1 : iba( iLon ), iLon )  
       PlotState_IIV( 1 : iba( iLon ), iLon, iPplot_I( 1 ) ) = &
            PlotState_IIV( 1 : iba( iLon ),iLon, iPplot_I( 1 ) ) + &
            Pressure_IC( iSpecies, 1 : iba( iLon ), iLon ) 
       PlotState_IIV( 1 : iba( iLon ), iLon, iPparplot_I( 1 ) ) = &
            PlotState_IIV(1:iba( iLon ), iLon, iPparplot_I( 1 ) ) + &
            PressurePar_IC( iSpecies, 1 : iba( iLon ), iLon ) 
       PlotState_IIV( 1 : iba( iLon ), iLon, iPhotplot_I( 1 ) ) = &
            PlotState_IIV(1:iba( iLon ), iLon, iPhotplot_I( 1 ) ) + &
            PressureHot_IC( iSpecies, 1 : iba( iLon ), iLon )
       PlotState_IIV( 1 : iba( iLon ),iLon, iPparhotplot_I( 1 ) ) = &
            PlotState_IIV( 1 : iba( iLon ), iLon, iPparhotplot_I( 1 ) ) + &
            PparHot_IC( iSpecies, 1 : iba( iLon ), iLon )
       PlotState_IIV( 1 : iba( iLon ), iLon, iNplot_I( 1 ) ) = &
            PlotState_IIV( 1 : iba( iLon ), iLon, iNplot_I( 1 ) ) + &
            Den_IC( iSpecies, 1 : iba( iLon ), iLon ) 
    end do
 end do
    do iLon=1,nLon
       PlotState_IIV( 1 : iba( iLon ), iLon, Beq_) = &
            Beq_C	( 1 : iba( iLon ), iLon )    
       PlotState_IIV( 1 : iba( iLon ), iLon, Vol_) = &
            Volume_C	( 1 : iba( iLon ), iLon )    
       PlotState_IIV( 1 : iba( iLon ), iLon, Pot_) = &
            Potential_C	( 1 : iba( iLon ), iLon )    
       PlotState_IIV( 1 : iba( iLon ), iLon, FAC_) = &
            FAC_C	( 1 : iba( iLon ), iLon )    
       PlotState_IIV( 1 : iba( iLon ), iLon, Lstar_) = &
            Lstar_C	( 1 : iba( iLon ), iLon )    
       PlotState_IIV( 1 : iba( iLon ), iLon, Plas_) = &
            PlasDensity_C	( 1 : iba( iLon ), iLon )
    enddo
    !fill ghost cells of plot data
    PlotState_IIV(:,nLon+1,iPplot_I(1))= &
         PlotState_IIV(:,1,iPplot_I(1))
    PlotState_IIV(:,nLon+1,iPparplot_I(1))= &
         PlotState_IIV(:,1,iPparplot_I(1))
    PlotState_IIV(:,nLon+1,iPhotplot_I(1))= &
         PlotState_IIV(:,1,iPhotplot_I(1))
    PlotState_IIV(:,nLon+1,iPparhotplot_I(1))= &
         PlotState_IIV(:,1,iPparhotplot_I(1))
    PlotState_IIV(:,nLon+1,iNplot_I(1))= &
         PlotState_IIV(:,1,iNplot_I(1))
    do iSpecies = 1,nspec
       PlotState_IIV(:,nLon+1,iPplot_I(iSpecies+1))= &
            PlotState_IIV(:,1,iPplot_I(iSpecies+1))
       PlotState_IIV(:,nLon+1,iPparplot_I(iSpecies+1))= &
            PlotState_IIV(:,1,iPparplot_I(iSpecies+1))
       PlotState_IIV(:,nLon+1,iPhotplot_I(iSpecies+1))= &
            PlotState_IIV(:,1,iPhotplot_I(iSpecies+1))
       PlotState_IIV(:,nLon+1,iPparhotplot_I(iSpecies+1))= &
            PlotState_IIV(:,1,iPparhotplot_I(iSpecies+1))
       PlotState_IIV(:,nLon+1,iNplot_I(iSpecies+1))= &
            PlotState_IIV(:,1,iNplot_I(iSpecies+1))  
    end do
    PlotState_IIV(:,nLon+1,Beq_) = Beq_C   (:,1)    
    PlotState_IIV(:,nLon+1,Vol_) = Volume_C(:,1)    
    PlotState_IIV(:,nLon+1,Pot_) = Potential_C(:,1)    
    PlotState_IIV(:,nLon+1,FAC_) = FAC_C(:,1)    
    PlotState_IIV(:,nLon+1,Lstar_) = Lstar_C(:,1)    
    PlotState_IIV(:,nLon+1,Plas_) = PlasDensity_C(:,1)
        
    TypePosition = 'append'
    if(IsFirstCall .and. .not. IsRestart) TypePosition = 'rewind'
    IsFirstCall = .false.

    call save_plot_file( NamePlotIono, TypePositionIn = TypePosition, &
         TypeFileIn = TypePlot, StringHeaderIn = NameHeader, &
         NameVarIn = NamePlotVar, &
         nStepIn = nint( Time / Dt ), TimeIn = Time, &
         nDimIn = 2, CoordIn_DII = CoordIono_DII, &
         VarIn_IIV = PlotState_IIV, ParamIn_I = (/ Gamma, rBody /) )
    
    deallocate( CoordIono_DII, PlotState_IIV )

  end subroutine Cimi_plot_iono
  
  !============================================================================

  subroutine Cimi_plot_fls(flux,n,time)
    use ModIoUnit,		ONLY: 	UnitTmp_
    use ModCimi,		ONLY: 	energy, ebound
    use ModCimiGrid,		ONLY:	&
         nLat=>np, nLon=>nt, nEnergy=>neng, nPitchAng=>npit, &
         sinAo, xlat, xmlt
    use ModCimiPlanet,		ONLY:	&
         nSpecies => nspec, NameSpeciesExtension_I, rc, Lstar_
    use ModCimiTrace,		ONLY:	ro, bo, xmlto, irm
    use ModCimiRestart,		ONLY: 	IsRestart
    use ModImTime,		ONLY: 	iCurrentTime_I
    use ModLstar,		ONLY:	Lstar_C, Lstar_max
    
    real, intent(in) :: flux(nLat,nLon,nEnergy,nPitchAng), time
    
    real          :: parmod(1:10)=0.0, lat, ro1, xmlt1, bo1, &
         energy_temp(1:nEnergy)
    integer       :: iLat,iLon,k,m,n,i,nprint
    logical, dimension(nspec), save :: IsFirstCall = .true.
    character(len=15):: outnameSep
    character(len=40), dimension(nspec), save :: NamePlotFlux
    !--------------------------------------------------------------------------
    nprint=ifix(time/DtFluxOutput(n))
    write(outnameSep,"(i4.4,i2.2,i2.2,a,i2.2,i2.2,i2.2)") & 
         iCurrentTime_I(1),iCurrentTime_I(2),iCurrentTime_I(3),'_',&
         iCurrentTime_I(4),iCurrentTime_I(5),iCurrentTime_I(6)
    
    
    energy_temp(1:nEnergy)=energy(n,1:nEnergy)
    if (DoSaveSeparateFluxFiles(n)) then
       open(unit=UnitTmp_,&
            file='IM/plots/'//outnameSep//NameSpeciesExtension_I(n)//'.fls',&
            status='unknown')
       write(UnitTmp_,"(f10.6,5i6,6x,'! rc in Re,nr,ip,je,ig,ntime')") &
            rc,nLat-1,nLon,nEnergy,nPitchAng,nprint
       write(UnitTmp_,'(6f14.3)') (energy_temp(k),k=1,nEnergy)
       write(UnitTmp_,'(6f9.5)') (sinAo(m),m=1,nPitchAng)
       write(UnitTmp_,'(10f8.3)') (xlat(i),i=2,nLat)
    else
       if ( IsFirstCall( n ) ) then
          write(NamePlotFlux(n), '(A,I8.8,A)' ) &
               'IM/plots/CimiFlux_n', nprint, NameSpeciesExtension_I(n)//'.fls'
          open(unit = UnitTmp_,&
               file = TRIM( NameplotFlux( n ) ), status = 'UNKNOWN')
          write(UnitTmp_,"(f10.6,5i6,6x,'! rc in Re,nr,ip,je,ig,ntime')") &
               rc,nLat-1,nLon,nEnergy,nPitchAng,nprint
          write(UnitTmp_,'(6f14.3)') (energy_temp(k),k=1,nEnergy)
          write(UnitTmp_,'(6f9.5)') (sinAo(m),m=1,nPitchAng)
          write(UnitTmp_,'(10f8.3)') (xlat(i),i=2,nLat)
       else
          open(unit = UnitTmp_, file = TRIM( NamePlotFlux( n ) ), &
               status = 'OLD', position = 'APPEND')
       endif
    endif
    write(UnitTmp_,'(f12.8,f8.3,10f9.2,"    ! hour, L*max, parmod")') &
         time/3600.,Lstar_max,parmod(1:10)
    do iLat=2,nLat             ! Write fluxes @ fixed E & y grids
       do iLon=1,nLon
          lat=xlat(iLat)
          if (iLat.gt.irm(iLon)) lat=xlat(irm(iLon))
          ro1=ro(iLat,iLon)
          if (iLat.gt.irm(iLon)) ro1=ro(irm(iLon),iLon)
          bo1=bo(iLat,iLon)
          if (iLat.gt.irm(iLon)) bo1=bo(irm(iLon),iLon)
          xmlt1=xmlto(iLat,iLon)
          if (iLat.gt.irm(iLon)) xmlt1=xmlto(irm(iLon),iLon)
          write(UnitTmp_,'(f7.2,f6.1,2f8.3,1pe11.3,0p,f8.3,i6)') &
               lat,xmlt(iLon),ro1,xmlt1,bo1,Lstar_C(iLat,iLon)
          do k=1,nEnergy
             write( UnitTmp_, '(1p,12e11.3)' ) &
                  ( flux( iLat, iLon, k, m ), m = 1, nPitchAng )
          enddo
       enddo
    enddo
    close(UnitTmp_)
    IsFirstCall(n) = .false.

  end subroutine Cimi_plot_fls

  !============================================================================

  subroutine Cimi_plot_psd( psd, n, time, xmm, xk )
    use ModIoUnit,		ONLY: 	UnitTmp_
    use ModCimi, 		ONLY: 	energy, ebound
    use ModCimiGrid,		ONLY: 	&
         nLat => np, nLon => nt, nm, nk, xlat, xmlt
    use ModCimiPlanet,		ONLY: 	&
         nSpecies => nspec, re_m, NameSpeciesExtension_I, rc
    use ModCimiTrace,		ONLY:	ro, bo, xmlto, irm
    use ModCimiRestart,		ONLY:	IsRestart
    use ModImTime,		ONLY: 	iCurrentTime_I
    use ModConst,		ONLY:	cElectronCharge, cLightSpeed
    use ModLstar,		ONLY:	Lstarm
    
    real, intent(in) :: psd( nLat, nLon, nm, nk), xmm(nSpecies,0:nm+1),&
         xk(nk),time
    
    real          :: parmod(1:10)=0.0,lat,ro1,xmlt1,bo1
    integer       :: iLat,iLon,k,m,n,i,nprint
    logical, dimension(nspec), save :: IsFirstCall = .true.
    character(len=15):: outnameSep
    character(len=40), dimension(nspec), save :: NamePlotPSD
    !--------------------------------------------------------------------------
    nprint=ifix(time/DtPSDOutput(n))
    write(outnameSep,"(i4.4,i2.2,i2.2,a,i2.2,i2.2,i2.2)") & 
         iCurrentTime_I(1),iCurrentTime_I(2),iCurrentTime_I(3),'_',&
         iCurrentTime_I(4),iCurrentTime_I(5),iCurrentTime_I(6)
    
    if (DoSaveSeparatePSDFiles(n)) then
       open(unit=UnitTmp_,&
            file='IM/plots/'//outnameSep//NameSpeciesExtension_I(n)//'.psd',&
            status='unknown')
       write(UnitTmp_,"(f10.6,5i6,6x,'! rc in Re,nr,ip,nm,nk,ntime')") &
            rc,nLat-1,nLon,nint(nm/2.),nint(nk/2.),nprint
       ! Convert K grid from [ T^0.5 / m ] to [ nT^0.5 / R_E ]
       write(UnitTmp_,'(1p,7e11.3)') &
            ( xk( k ) * SQRT( 1.e9 ) / re_m, k = 1, nk, 2 )
       ! Convert xmm grid from [ J / T ] to [ keV / nT ]
       write(UnitTmp_,'(1p,7e11.3)') &
            ( xmm( n, m ) / 1e3 / cElectronCharge / 1e9, m = 1, nm, 2 )
       write(UnitTmp_,'(10f8.3)') (xlat(i),i=2,nLat)
    else
       if (IsFirstCall(n)) then
          write(NamePlotPSD(n),"(A,I8.8,A)") &
               "IM/plots/CimiPSD_n", nprint, NameSpeciesExtension_I(n)//'.psd'
          open( unit = UnitTmp_, &
               file = TRIM( NamePlotPSD( n ) ), status = 'UNKNOWN' )
          write(UnitTmp_,"(f10.6,5i6,6x,'! rc in Re,nr,ip,nm,nk,ntime')") &
               rc,nLat-1,nLon,nint(nm/2.),nint(nk/2.),nprint
          ! Convert K grid from [ T^0.5 / m ] to [ nT^0.5 / R_E ]
          write(UnitTmp_,'(1p,7e11.3)') &
               ( xk( k ) * SQRT( 1.e9 ) / re_m, k = 1, nk, 2 )
          ! Convert xmm grid from [ J / T ] to [ keV / nT ]
          write(UnitTmp_,'(1p,7e11.3)') &
               ( xmm( n, m ) / 1e3 / cElectronCharge / 1e9, m = 1, nm, 2 )
          write(UnitTmp_,'(10f8.3)') (xlat(i),i=2,nLat)
       else
          open(unit=UnitTmp_, file = TRIM( NamePlotPSD( n ) ), &
               status = 'OLD', position = 'APPEND')
       endif
    endif
    write(UnitTmp_,'(f12.8,10f9.2,"    ! hour,  parmod")') &
         time/3600.,parmod(1:10)
    do iLat=2,nLat             ! Write PSD @ fixed mu & K grids
       do iLon=1,nLon
          lat=xlat(iLat)
          if (iLat.gt.irm(iLon)) lat=xlat(irm(iLon))
          ro1=ro(iLat,iLon)
          if (iLat.gt.irm(iLon)) ro1=ro(irm(iLon),iLon)
          bo1=bo(iLat,iLon)
          if (iLat.gt.irm(iLon)) bo1=bo(irm(iLon),iLon)
          xmlt1=xmlto(iLat,iLon)
          if (iLat.gt.irm(iLon)) xmlt1=xmlto(irm(iLon),iLon)
          write(UnitTmp_,'(f7.2,f6.1,2f8.3,1pe11.3)') &
               lat,xmlt(iLon),ro1,xmlt1,bo1
          do m = 1, nm, 2
             write( UnitTmp_, '(1p,13es14.3E3)' ) &
                  ( psd( iLat, iLon, m, k ) * 1e51 * &
                  ( cElectronCharge / cLightSpeed ) ** 3, k = 1, nk, 2 )
          enddo
       enddo
    enddo
    close(UnitTmp_)
    IsFirstCall(n)=.false.
    
  end subroutine Cimi_plot_psd
  
  subroutine Cimi_plot_vl( vlEa, n, time )
    use ModIoUnit,	ONLY: 	UnitTmp_
    use ModCimi,	ONLY: 	energy, ebound
    use ModCimiGrid,	ONLY:	&
         nLat => np, nLon => nt, nEnergy => neng, &
         nPitchAng => npit, sinAo, xlat, xmlt
    use ModCimiPlanet, 	ONLY:	&
         nSpecies => nspec,NameSpeciesExtension_I, rc
    use ModCimiTrace,	ONLY:	ro, bo, xmlto, irm
    use ModCimiRestart,	ONLY: 	IsRestart
    use ModImTime,	ONLY:	iCurrentTime_I
    
    real, intent(in) :: &
         vlEa( nLat, nLon, nEnergy, nPitchAng ), time
    
    real          :: &
         parmod( 1 : 10 ) = 0.0, lat, ro1, xmlt1, bo1, &
         energy_temp( 1 : nEnergy )
    integer       :: iLat,iLon,k,m,n,i,nprint
    logical, dimension(nspec), save :: IsFirstCall = .true.
    character(len=15):: outnameSep
    character(len=40), dimension( nspec ), save :: NamePlotVL
    !--------------------------------------------------------------------------
    nprint=ifix(time/DtVLDriftOutput(n))
    write(outnameSep,"(i4.4,i2.2,i2.2,a,i2.2,i2.2,i2.2)") & 
         iCurrentTime_I(1),iCurrentTime_I(2),iCurrentTime_I(3),'_',&
         iCurrentTime_I(4),iCurrentTime_I(5),iCurrentTime_I(6)
    
    energy_temp(1:nEnergy)=energy(n,1:nEnergy)
    if (DoSaveSeparateVLDriftFiles(n)) then
       open(unit=UnitTmp_,&
            file='IM/plots/'//outnameSep//NameSpeciesExtension_I(n)//'.vl',&
            status='unknown')
       write(UnitTmp_,"(f10.6,5i6,6x,'! rc in Re,nr,ip,je,ig,ntime')") &
            rc,nLat-1,nLon,nEnergy,nPitchAng,nprint
       write(UnitTmp_,'(6f9.3)') (energy_temp(k),k=1,nEnergy)
       write(UnitTmp_,'(6f9.5)') (sinAo(m),m=1,nPitchAng)
       write(UnitTmp_,'(10f8.3)') (xlat(i),i=2,nLat)
    else
       if (IsFirstCall(n) .and. .not. IsRestart) then
          write( NamePlotVL( n ), '(A, I8.8, A)' )&
               'IM/plots/CimiDrift_n', nprint, NameSpeciesExtension_I(n)//'.vl'
          open(unit = UnitTmp_, file = TRIM( NamePlotVL( n ) ), status = 'UNKNOWN')
          write(UnitTmp_,"(f10.6,5i6,6x,'! rc in Re,nr,ip,je,ig,ntime')") &
               rc,nLat-1,nLon,nEnergy,nPitchAng,nprint
          write(UnitTmp_,'(6f9.3)') (energy_temp(k),k=1,nEnergy)
          write(UnitTmp_,'(6f9.5)') (sinAo(m),m=1,nPitchAng)
          write(UnitTmp_,'(10f8.3)') (xlat(i),i=2,nLat)
       else
          open(unit = UnitTmp_, file = TRIM( NamePlotVL( n ) ), &
               status = 'OLD', position = 'APPEND')
          
       endif
    endif
    write(UnitTmp_,'(f12.8,10f9.2,"    ! hour,  parmod")') &
         time/3600.,parmod(1:10)
    do iLat=2,nLat             ! Write fluxes @ fixed E & y grids
       do iLon=1,nLon
          lat=xlat(iLat)
          if (iLat.gt.irm(iLon)) lat=xlat(irm(iLon))
          ro1=ro(iLat,iLon)
          if (iLat.gt.irm(iLon)) ro1=ro(irm(iLon),iLon)
          bo1=bo(iLat,iLon)
          if (iLat.gt.irm(iLon)) bo1=bo(irm(iLon),iLon)
          xmlt1=xmlto(iLat,iLon)
          if (iLat.gt.irm(iLon)) xmlt1=xmlto(irm(iLon),iLon)
          write( UnitTmp_, '(F7.2, F6.1, 2F8.3, 1PE11.3)' ) &
               lat, xmlt( iLon ), ro1, xmlt1, bo1
          do k = 1, nEnergy
             write( UnitTmp_, '(1P,12E11.3)' ) &
                  ( vlEa( iLat, iLon, k, m ) , m = 1, nPitchAng )
          enddo
       enddo
    enddo
    close(UnitTmp_)
    IsFirstCall(n)=.false.
    
  end subroutine Cimi_plot_vl

  subroutine Cimi_plot_vp( vpEa, n, time )
    use ModIoUnit,		ONLY:	UnitTmp_
    use ModCimi,		ONLY:	energy, ebound
    use ModCimiGrid,		ONLY:	&
         nLat => np, nLon => nt, nEnergy => neng, nPitchAng => npit,&
         sinAo, xlat, xmlt
    use ModCimiPlanet,		ONLY:	&
         nSpecies => nspec, NameSpeciesExtension_I, rc
    use ModCimiTrace,		ONLY:	ro, bo, xmlto, irm
    use ModCimiRestart,		ONLY:	IsRestart
    use ModImTime,		ONLY:	iCurrentTime_I

    real, intent(in) 	:: vpEa( nLat, nLon, nEnergy, nPitchAng ), time
    real		:: &
         parmod(1:10) = 0.0, lat, ro1, xmlt1, bo1, energy_temp( 1 : nEnergy )
    integer		:: iLat, iLon, k, m, n, i, nprint
    logical, dimension(nspec), save :: IsFirstCall = .true.
    character(len=15):: outnameSep
    character(len=40), dimension(nspec), save :: NamePlotVP
    !--------------------------------------------------------------------------
    nprint=ifix(time/DtVPDriftOutput(n))
    write(outnameSep,"(i4.4,i2.2,i2.2,a,i2.2,i2.2,i2.2)") & 
         iCurrentTime_I(1),iCurrentTime_I(2),iCurrentTime_I(3),'_',&
         iCurrentTime_I(4),iCurrentTime_I(5),iCurrentTime_I(6)

    energy_temp( 1 : nEnergy ) = energy( n, 1 :nEnergy )
    if (DoSaveSeparateVPDriftFiles(n)) then
       open(unit=UnitTmp_,&
            file='IM/plots/'//outnameSep//NameSpeciesExtension_I(n)//'.vp',&
            status='unknown')

       write(UnitTmp_,"(f10.6,5i6,6x,'! rc in Re,nr,ip,je,ig,ntime')") &
                                !               rc,nlat-1,nLon,nEnergy,nPitchAng,nprint
            rc,1,nLon,nEnergy,nPitchAng,nprint          
       write(UnitTmp_,'(6f9.3)') (energy_temp(k),k=1,nEnergy)
       write(UnitTmp_,'(6f9.5)') (sinAo(m),m=1,nPitchAng)
       write(UnitTmp_,'(10f8.3)') (xlat(i),i=2,nLat)
    else
       if ( IsFirstCall( n ) ) then
          write(NamePlotVP(n), '(A,I8.8,A)') &
               'IM/plots/CimiDrift_n', nprint, NameSpeciesExtension_I(n)//'.vp'
          open(unit=UnitTmp_, file = TRIM( NamePlotVP( n ) ), status = 'UNKNOWN')
          write(UnitTmp_,"(f10.6,5i6,6x,'! rc in Re,nr,ip,je,ig,ntime')") &
               rc,nLat-1,nLon,nEnergy,nPitchAng,nprint
          write(UnitTmp_,'(6f9.3)') (energy_temp(k),k=1,nEnergy)
          write(UnitTmp_,'(6f9.5)') (sinAo(m),m=1,nPitchAng)
          write(UnitTmp_,'(10f8.3)') (xlat(i),i=2,nLat)
       else
          open(unit=UnitTmp_, file = TRIM( NamePlotVP( n ) ),&
               status = 'OLD', position = 'APPEND')
       endif
    endif
    write(UnitTmp_,'(f12.8,10f9.2,"    ! hour,  parmod")') &
         time/3600.,parmod(1:10)
    do iLat=2,nLat             ! Write fluxes @ fixed E & y grids
       do iLon=1,nLon
          lat=xlat(iLat)
          if (iLat.gt.irm(iLon)) lat=xlat(irm(iLon))
          ro1=ro(iLat,iLon)
          if (iLat.gt.irm(iLon)) ro1=ro(irm(iLon),iLon)
          bo1=bo(iLat,iLon)
          if (iLat.gt.irm(iLon)) bo1=bo(irm(iLon),iLon)
          xmlt1=xmlto(iLat,iLon)
          if (iLat.gt.irm(iLon)) xmlt1=xmlto(irm(iLon),iLon)
          write( UnitTmp_,'( F7.2, F6.1, 2F8.3, 1PE11.3 )') &
               lat, xmlt( iLon ), ro1, xmlt1, bo1
          do k = 1, nEnergy
             write( UnitTmp_, '(1P,12E11.3)' ) &
                  ( vpEa( iLat, iLon, k, m ), m = 1, nPitchAng )
          enddo
       enddo
    enddo
    close(UnitTmp_)
    IsFirstCall(n)=.false.

  end subroutine Cimi_plot_vp
  
  !============================================================================
  subroutine cimi_plot_log(Time)
    use ModNumConst, 		ONLY: 	cPi
    use ModConst, 		ONLY: 	cElectronCharge, cMu, cPi
    use ModCimiPlanet,		ONLY: 	&
         nSpecies => nspec, NamePlotVarLog, dipmom
    use ModCimiRestart,		ONLY:	IsRestart
    use ModIoUnit,		ONLY:	UnitTmp_
    use ModCIMI,        	ONLY:	nOperator, driftin, driftout, &
         rbsumGlobal, rcsumGlobal, dt, eChangeGlobal
    implicit none
    
    real, intent(in) :: Time
    logical, save :: IsFirstCall=.true.
    integer       :: iSpecies, nprint
    character(len=18), save :: LogFileName
    !--------------------------------------------------------------------------
    
    ! Calculate iteration number (just use time in seconds)
    nprint=nint(Time/dt)
    
    ! Open file and write header if no restart on first call
    if ( IsFirstCall ) then
       write(LogFileName,"(a6,i8.8,a4)") "CIMI_n",nprint,".log"
       open(unit=UnitTmp_,&
            file='IM/plots/'//LogFileName,&
            status='unknown')
       write(UnitTmp_,'(A)') 'CIMI Logfile'
       write(UnitTmp_,'(A)') TRIM(NamePlotVarLog)
       IsFirstCall = .false.
    else
       open(unit=UnitTmp_,&
            file='IM/plots/'//LogFileName,&
            status='old', position='append')      
       IsFirstCall = .false.
    endif
    
    ! Write out iteration number
    write(UnitTmp_,'(I18)',ADVANCE='NO') nprint
    
    ! write time 
    write(UnitTmp_,'(1es18.08E3)',ADVANCE='NO') time
    
    ! write dst calculated from the Dessler-Parker-Schopke relation.
    ! Magnetic field depression deltaB at Earth's Center given by
    !
    ! 	      deltaB = - mu_0 / 2 / pi * U_R / B_E / R_E^3,
    !
    ! where mu_0 is the vacuum permeability, U_R is the total total
    ! ring current energy given by the sum over species of rbsum, B_E
    ! is the strength of Earth's dipole magnetic field, and R_E is the
    ! Earth's radius.
    !
    ! Reference: Eq. 3.36 of Baumjohann and Treumann (2012), "Basic
    ! Space Plasma Physics, Revised Edition," Imperial College Press.
    write(UnitTmp_,'(1es18.08E3)',ADVANCE='NO') &
         -0.5 * cMu / cPi / dipmom * &
         SUM( rbsumglobal ) * 1000. * cElectronCharge * 10 ** ( 9. )

    ! write out the operator changes
    ! don't write previous time step operators (OpLossCone0, OpFLC0)
    do iSpecies=1,nSpecies
       if (iSpecies < nSpecies) then
          write(UnitTmp_,'(12es18.08E3)',ADVANCE='NO') &
               rbsumglobal(iSpecies), rcsumglobal(iSpecies), &
               eChangeGlobal(iSpecies,1:nOperator-2), &
               driftin(iSpecies), driftout(iSpecies)
       else
          write(UnitTmp_,'(12es18.08E3)') &
               rbsumglobal(iSpecies), rcsumglobal(iSpecies), &
               eChangeGlobal(iSpecies,1:nOperator-2), &
               driftin(iSpecies), driftout(iSpecies)
       endif
    enddo

    close(UnitTmp_)

  end subroutine cimi_plot_log

  subroutine cimi_plot_precip( n, time )
    use ModDstOutput,		ONLY: DstOutput
    use ModIoUnit,		ONLY: UnitTmp_
    use ModCimi,		ONLY: energy
    use ModCimiGrid,		ONLY: &
         nLat => np, nLon => nt, nEnergy => neng, xlat, xmlt
    use ModCimiPlanet,		ONLY: &
         nSpecies=>nspec, re_m, NameSpeciesExtension_I, rc
    use ModCimiTrace,		ONLY: ro, bo, xmlto, irm
    use ModCimiRestart,		ONLY: IsRestart
    ! for separate files only do need right now
    use ModImTime,		ONLY: iCurrentTime_I   
    use ModCimiInitialize, 	ONLY: dphi
    use ModCimi,		ONLY: Eje1, preP, preF
    
    implicit none
    
    real energy_temp(1:nEnergy), time
    
    logical, save :: IsFirstCall = .true.
    integer       :: n,i,j,k,nprint,iLat,iLon
    character(len=15):: outnameSep
    !--------------------------------------------------------------------------
    nprint=ifix(time/DtPreciOutput(n))
    write(outnameSep,"(i4.4,i2.2,i2.2,a,i2.2,i2.2,i2.2)") & 
         iCurrentTime_I(1),iCurrentTime_I(2),iCurrentTime_I(3),'_',&
         iCurrentTime_I(4),iCurrentTime_I(5),iCurrentTime_I(6)
    
!   area1=rc*rc*re_m*re_m*dphi
    energy_temp(1:nEnergy) = energy(n,1:nEnergy)
    if (DoSaveSeparatePreciFiles(n)) then
       open(unit=UnitTmp_,&
            file='IM/plots/'//outnameSep//NameSpeciesExtension_I(n)//'.preci',&
            status='unknown')
       write(UnitTmp_,"(f10.6,4i6,6x,'! rc in Re,nr,ip,je,ntime')") &
            rc,nLat,nLon,nEnergy,nprint
       write(UnitTmp_,'(10f8.3)') (xlat(i),i=1,nLat)
       write(UnitTmp_,'(10f8.3)') (xmlt(j),j=1,nLon)
       write(UnitTmp_,'(10f8.3)') (energy_temp(k),k=1,nEnergy)
    else
       if (IsFirstCall .and. .not. IsRestart) then
          open(unit=UnitTmp_,&
               file='IM/plots/CimiFlux'//NameSpeciesExtension_I(n)//'.preci',&
               status='unknown')
          
          write(UnitTmp_,"(f10.6,4i6,6x,'! rc in Re,nr,ip,je,ntime')") &
               rc,nLat,nLon,nEnergy,nprint
          write(UnitTmp_,'(10f8.3)') (xlat(i),i=1,nLat)
          write(UnitTmp_,'(10f8.3)') (xmlt(j),j=1,nLon)
          write(UnitTmp_,'(10f8.3)') (energy_temp(k),k=1,nEnergy)
       else
          open(unit=UnitTmp_,&
               file='IM/plots/CimiFlux'//NameSpeciesExtension_I(n)//'.preci',&
               status='old', position='append')
          
       endif
    endif
    
    write(UnitTmp_,*) time/3600., DstOutput, '      ! hour, Dst'
    
    do iLat=1,nLat             ! Write precipitation @ fixed E grid
       do iLon=1,nLon
          write(UnitTmp_,'(f8.3,f8.2,1pE12.4,a)') &
               xlat(iLat),xmlt(iLon),Eje1(n,iLat,iLon),&
               '  ! mlat(deg), mlt(hr), Eflux (mW/m2), particles'
          write(UnitTmp_,'(1p,6e12.4)') PreF(n,iLat,iLon,1:nEnergy+2)  
          write(UnitTmp_,'(1p,6e12.4)') PreP(n,iLat,iLon,1:nEnergy+2)
       enddo
    enddo
    close(UnitTmp_)
    IsFirstCall=.false.

  end subroutine cimi_plot_precip

  !============================================================================

  subroutine Cimi_plot_Lstar(xk,time)
    use ModIoUnit,	ONLY: UnitTmp_
    use ModCimiGrid,	ONLY: nLat=>np, nLon=>nt, &
         nm, nk, xlat,xmlt
    use ModCimiPlanet,	ONLY: nSpecies=>nspec, re_m, rc
    use ModCimiTrace,	ONLY: ro,bm,xmlto,irm
    use ModCimiRestart,	ONLY: IsRestart
    use ModImTime,      ONLY:iCurrentTime_I   ! for separate files
                                              ! only do need right now
    use ModLstar
    
    real, intent(in) :: xk(nk), time
    
    real          :: lat,ro1,xmlt1,bm1(nk)
    integer       :: iLat,iLon,k,m,n,i,nprint
    logical, save :: IsFirstCall = .true.
    character(len=15):: outnameSep
    character(len=40), save :: NamePlotLstar
    !--------------------------------------------------------------------------
    nprint=ifix(time/DtLstarOutput)
    write(outnameSep,"(i4.4,i2.2,i2.2,a,i2.2,i2.2,i2.2)") & 
         iCurrentTime_I(1),iCurrentTime_I(2),iCurrentTime_I(3),'_',&
         iCurrentTime_I(4),iCurrentTime_I(5),iCurrentTime_I(6)
    
    if (DoSaveSeparateLstarFiles) then
       open(unit=UnitTmp_,&
            file='IM/plots/'//outnameSep//'.lstar',&
            status='unknown')
       write(UnitTmp_,"(f10.6,4i6,6x,'! rc in Re,nr,ip,nm,nk,ntime')") &
            rc, nLat-1, nLon, nint(nk/2.), nprint            
       write(UnitTmp_,'(1p,7e11.3)') (xk(k)*sqrt(1.e9)/re_m,k=1,nk,2)
       write(UnitTmp_,'(8f8.3)') (xlat(i),i=2,nLat)
    else
       if (IsFirstCall) then
          write(NamePlotLstar, '(A,I8.8,A)' )&
               'IM/plots/Cimi_n', nprint, '.lstar'
          open(unit = UnitTmp_, file = TRIM( NamePlotLstar ), status = 'UNKNOWN')
          write(UnitTmp_,"(f10.6,4i6,6x,'! rc in Re,nr,ip,nm,nk,ntime')") &
               rc, nLat-1, nLon, nint(nk/2.), nprint            
          write(UnitTmp_,'(1p,7e11.3)') (xk(k)*sqrt(1.e9)/re_m,k=1,nk,2)
          write(UnitTmp_,'(8f8.3)') (xlat(i),i=2,nLat)
       else
          open(unit = UnitTmp_, file = TRIM( NamePlotLstar ),&
               status = 'OLD', position = 'APPEND')
       endif
    endif
    
    write(UnitTmp_,'(8f8.3)') &
         time/3600.,(lstarm_max(m),m=1,nk)
    do iLat=2,nLat            
       do iLon=1,nLon
          lat=xlat(iLat)
          if (iLat.gt.irm(iLon)) lat=xlat(irm(iLon))
          ro1=ro(iLat,iLon)
          if (iLat.gt.irm(iLon)) ro1=ro(irm(iLon),iLon)
          bm1(1:nk)=bm(iLat,iLon,1:nk)
          if (iLat.gt.irm(iLon)) bm1(1:nk)=bm(irm(iLon),iLon,1:nk)
          xmlt1=xmlto(iLat,iLon)
          if (iLat.gt.irm(iLon)) xmlt1=xmlto(irm(iLon),iLon)
          write(UnitTmp_,'(f7.2,f6.1,2f8.3)') &
               lat,xmlt(iLon),ro1,xmlt1
          write(UnitTmp_,'(7F08.3)') (Lstarm(iLat,iLon,k),k=1,nk,2)
          write(UnitTmp_,'(1p,7E10.3)') (bm1(k),k=1,nk,2)
       enddo
    enddo
    close(UnitTmp_)
    IsFirstCall=.false.
    
  end subroutine Cimi_plot_Lstar
  !============================================================================
    subroutine Cimi_plot_boundary_check(time)
    use ModIoUnit,      ONLY: UnitTmp_
    use ModCimiTrace,   ONLY: ro,bm,xmlto,irm
    use ModCimiRestart, ONLY: IsRestart
    use ModCimiGrid, ONLY: nlat=>np,nLon=>nt,neng
    use ModCimi, ONLY: phot,Pressure_IC,energy, &
         part_phot, dif_q3, eng_q3, vdr_q3, vexb
    use ModGmCimi,      ONLY: Den_IC,Temp_IC
    
    real, intent(in) :: time
    
    real   :: lat,ro1,xmlt1,eng1,vexb1,vdr1,dif1,phot1,den1,temp1,press1
    integer       :: iLat,iLon,k,m,n,i,nprint
    logical, save :: IsFirstCall = .true.
    !--------------------------------------------------------------------------
    
    nprint=ifix(time/DtOutput)
    
    if (IsFirstCall .and. .not. IsRestart) then
       open(unit=UnitTmp_,&
            file='IM/plots/Cimi.boundary.check',status='unknown')
       write(UnitTmp_,"('! iLon,iLat,xmlt,ro,phot,press,temp,den,eng,vexb,vdr1,dif1')")
       
       write(UnitTmp_,"(4i10,6x,'! nr,ip,neng,ntime')") &
            nLat-1,nLon,neng,nprint
       write(UnitTmp_,'(10f8.3)') (energy(1,i),i=1,neng)
    else
       open(unit=UnitTmp_,&
            file='IM/plots/Cimi.boundary.check',status='old',&
            position='append')
    endif
    write(UnitTmp_,"(f8.3,6x, '! time, h ')") time/3600.
    do iLon=1,nLon
       do iLat=2,nLat
          ro1=ro(iLat,iLon)
          xmlt1=xmlto(iLat,iLon)
          if (iLat.gt.irm(iLon)) xmlt1=0.0
          
          eng1=eng_q3(1,iLat,iLon)
          if (iLat.gt.irm(iLon)) eng1=0.0
          
          vexb1=vexb(1,iLat,iLon)
          if (iLat.gt.irm(iLon)) vexb1=0.0
          
          vdr1=vdr_q3(1,iLat,iLon)
          if (iLat.gt.irm(iLon)) vdr1=0.0
          
          dif1=dif_q3(1,iLat,iLon)        
          if (iLat.gt.irm(iLon)) dif1=0.0
          
          phot1 = Phot(1,iLat,iLon)
          press1 = Pressure_IC(1,Ilat,iLon)
          den1 = Den_IC(1,iLat,iLon)/1.e6
          temp1 = Temp_IC(1,iLat,iLon)/1000. ! in keV
          
          write(UnitTmp_,'(2i10,2f8.3,1p,8e11.3)') &
               iLon,iLat,xmlt1,ro1,phot1,press1,temp1,den1,eng1,vexb1,vdr1,dif1
          write(UnitTmp_,'(1p,6e12.4)') Part_phot(1,iLat,iLon,1:neng)
       enddo
    enddo
    close(UnitTmp_)
    IsFirstCall=.false.
    
  end subroutine Cimi_plot_boundary_check

  
end module ModCimiPlot

