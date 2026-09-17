module GIMME_plots

  private ! except

  public :: gimme_plot

contains
  !============================================================================
  subroutine gimme_plot

    use GIMME_iono_grid, ONLY: &
         UseFullSphere,nTheta,nPhi,Theta_G, Phi_G, Riono, &
         dPhi, iThetaGround, iPhiGround, rPlanet,Potential_G,StartTime, &
         TimeSimulation,Year_,Month_,Day_,Hour_,Minute_,Second_
    use GIMME_mag_input, ONLY: Jr_C
    use GIMME_conductance, ONLY: SigmaP_G,SigmaH_G,Sigma0_G
    use ModIoUnit, ONLY: UnitTmp_
    use ModTimeConvert, ONLY: time_real_to_int

    ! integer, parameter ::UnitTmp_=101
    real,   allocatable :: Jr_G(:,:)
    integer :: iPhi,iTheta
    real :: xcoord,ycoord,zcoord

    real :: CurrentTime
    integer :: iCurrentTime_I(7)
    character(len=100) :: NamePlot2D, NamePlot3D

    !--------------------------------------------------------------------------
    if (.not.allocated(Jr_G))allocate(Jr_G(0:nTheta+1,0:nPhi+1))

    ! fill Jr_G
    Jr_G(1:nTheta,1:nPhi) = Jr_C(1:nTheta,1:nPhi)
    ! fill Ghost cells
    Jr_G(0,:)=Jr_G(1,:)
    Jr_G(nTheta+1,:)=Jr_G(nTheta,:)

    Jr_G(:,0)=Jr_G(:,nPhi)
    Jr_G(:,nPhi+1)=Jr_G(:,1)

    ! get the current time
    CurrentTime=StartTime+TimeSimulation
    call time_real_to_int(CurrentTime,iCurrentTime_I)

    ! write the output file names
    write(NamePlot2D,"(a,i4.4,i2.2,i2.2,i2.2,i2.2,i2.2,a)")&
    'IM/plots/gimme_ionosphere2D_',iCurrentTime_I(Year_),iCurrentTime_I(Month_),&
         iCurrentTime_I(Day_),iCurrentTime_I(Hour_),iCurrentTime_I(Minute_),&
         iCurrentTime_I(Second_),'.dat'

    ! write the output file names
    write(NamePlot3D,"(a,i4.4,i2.2,i2.2,i2.2,i2.2,i2.2,a)")&
    'IM/plots/gimme_ionosphere3D_',iCurrentTime_I(Year_),iCurrentTime_I(Month_),&
         iCurrentTime_I(Day_),iCurrentTime_I(Hour_),iCurrentTime_I(Minute_),&
         iCurrentTime_I(Second_),'.dat'

    ! write output to file for plotting
    open(UnitTmp_,file=NamePlot2D)
    write(UnitTmp_,'(a)') &
         'VARIABLES = "theta", "phi", "Pot", "Jr", "SigmaP", "SigmaH", "Sigma0"'
    write(UnitTmp_,'(a,i3,a,i3,a)') 'Zone I=', nTheta+2, &
         ', J=', nPhi+2,', DATAPACKING=POINT'

    do iPhi=0,nPhi
       do iTheta=1,nTheta+1
          if (iPhi==0) then
             write(UnitTmp_,"(100es18.10)") &
                  Theta_G(iTheta), 0.0-dPhi, Potential_G(iTheta,iPhi),&
                  Jr_G(iTheta,iPhi), SigmaP_G(iTheta,iPhi), &
                  SigmaH_G(iTheta,iPhi), Sigma0_G(iTheta,iPhi)
          else
             write(UnitTmp_,"(100es18.10)") &
                  Theta_G(iTheta), Phi_G(iPhi), Potential_G(iTheta,iPhi),&
                  Jr_G(iTheta,iPhi), SigmaP_G(iTheta,iPhi), &
                  SigmaH_G(iTheta,iPhi), Sigma0_G(iTheta,iPhi)
          endif
       enddo
    enddo
    close(UnitTmp_)

    ! write output to file for 3D plotting
    open(UnitTmp_,file=NamePlot3D)
    write(UnitTmp_,'(a)') &
         'VARIABLES = "X [R]", "Y [R]", "Z [R]", "Pot", "Jr", "SigmaP", "SigmaH", "Sigma0"'
    write(UnitTmp_,'(a,i3,a,i3,a)') 'Zone I=', nTheta+1, &
         ', J=', nPhi+1,', DATAPACKING=POINT'

    do iPhi = 0, nPhi
       do iTheta = 1, nTheta+1
          if (iPhi==0) then
             xcoord=rIono/rPlanet*sin(Theta_G(iTheta))*cos(-dPhi)
             ycoord=rIono/rPlanet*sin(Theta_G(iTheta))*sin(-dPhi)
             zcoord=rIono/rPlanet*cos(Theta_G(iTheta))
             write(UnitTmp_,"(100es18.10)") &
                  xcoord,ycoord,zcoord,Potential_G(iTheta,iPhi),&
                  Jr_G(iTheta,iPhi),SigmaP_G(iTheta,iPhi), &
                  SigmaH_G(iTheta,iPhi), Sigma0_G(iTheta,iPhi)
          else
             xcoord=rIono/rPlanet*sin(Theta_G(iTheta))*cos(Phi_G(iPhi))
             ycoord=rIono/rPlanet*sin(Theta_G(iTheta))*sin(Phi_G(iPhi))
             zcoord=rIono/rPlanet*cos(Theta_G(iTheta))
             if (iTheta==iThetaGround .and. iPhi==iPhiGround) then
                write(UnitTmp_,"(100es18.10)") &
                     xcoord,ycoord,zcoord,0.0,&
                     Jr_G(iTheta,iPhi),SigmaP_G(iTheta,iPhi),&
                     SigmaH_G(iTheta,iPhi),&
                     Sigma0_G(iTheta,iPhi)
             else
                write(UnitTmp_,"(100es18.10)") &
                     xcoord,ycoord,zcoord,Potential_G(iTheta,iPhi),&
                     Jr_G(iTheta,iPhi),SigmaP_G(iTheta,iPhi), &
                     SigmaH_G(iTheta,iPhi), Sigma0_G(iTheta,iPhi)
             endif
          endif
       enddo
    enddo
    close(UnitTmp_)

  end subroutine gimme_plot
  !============================================================================
end module GIMME_Plots
!==============================================================================
