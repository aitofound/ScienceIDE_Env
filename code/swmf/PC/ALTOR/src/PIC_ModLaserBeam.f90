!  Copyright (C) 2002 Regents of the University of Michigan, 
!  portions used with permission 
!  For more information, see http://csem.engin.umich.edu/tools/swmf

module PIC_ModLaserBeam

  ! Gaussian field distribution in the focal plane is created via the !
  ! boundary conditions for electric fields at the left x-boundary    !
  ! of the domain. We use denotations and terms identical to those    !
  ! used in https://en.wikipedia.org/wiki/Gaussian_beam with the only !
  ! exception of using the beam propagating along x axis              !

  use PC_ModSize,  ONLY: nDim, x_, y_, z_, MaxDim
  use PC_BATL_lib, ONLY: CoordMin_D, CoordMax_D
  use PIC_ModMain, ONLY: tSimulation, Dt
  use ModNumConst, ONLY: cPi, cTwoPi
  use PIC_ModProc, ONLY: iProc
  use ModUtilities, ONLY: CON_stop

  implicit none
  SAVE
  PRIVATE

  logical, public :: UseLaserBeam = .false.
  !LaserAmplitude in focus 
  !\
  !Polarization {Ey/=0 Ez=0} p-pol, {Ey=0 Ez/=0} s-pol 
  !Polarization {Ey/=0 Ez/=0} circular, elliptical
  !/
  real :: AmplitudeFocus_D(2:3) = 0.0 
  !\
  ! Phase shift, such that at the initial time instant and in the
  ! central point of the pulse the field is proportional to
  ! Ey_center ~ cos(PhaseShift), Ez_center ~ sin(PhaseShift)
  !/
  !\
  ! While reading: In cycles, wavelengths
  ! Upon reading is converted into dimensionless form k*width 
  ! by multiplying by cTwoPi
  !/
  real :: PhaseShift = 0.0 
  !\
  ! While reading: In cycles, wavelengths
  ! Upon reading is converted into dimensionless form k*width 
  ! by multiplying by cTwoPi
  !/
  real :: PulseWidthFocus_D(nDim)= 0.0
  !\
  ! WidthHalfMax2Gaussian=1.0/sqrt(2.0*alog(2.0))
  ! If WidthGaussian = WidthHalfMax* WidthHalfMax2Gaussian,
  ! then the relative amplitude in the points 
  ! (+/-)0.5*WidthHalfMax from the beam axis equals 
  ! exp(-(0.5*WidthHalfMax/WidthGaussian)**2)=sqrt(0.5)
  ! so that the intensity in these points is 0.5 of the
  ! axial one, in a full agreement with the definition 
  ! of the Full Width at Half Maximal (Intensity) - FWHM
  !/
  !real :: WidthHalfMax2Gaussian 
  real :: GaussianWidthFocus_D(nDim)
  !\
  ! Dimensionless coordinates of the focal point
  ! While reading, they are set with respect to the 
  ! left corner and may be expressed in terms of
  ! wavelengths or fractions of the computational 
  ! domain size.
  !/
  real :: XyzFocus_D(nDim)= 1.0
  !\
  ! Time instants when the beamed pulse starts and ends
  ! passing through the domain boundary at the beam axis.
  !/          
  real :: TimePulseBegin = 0.0, TimePulseEnd = 0.0
  !\
  ! X coordinate of the laser pulse center in the 
  ! initial time instant
  !/

  real :: xPulseCenter = 0.0
  !
  !Pulse longitudinal envelope: Default-Steplike, 
  !1-Gauss, 2-Cosine (FWHM - full width at half maximum)
  integer :: nEnvelope = 1
  !\
  ! Gaussian width at the domain boundary (well before focusing)
  !/
  real    :: GaussianWidthBoundary_D(2:nDim) = -1.0
  !\
  ! Field amplitudes at the boundary 
  ! that the focused field equals given AmplitudeFocus_D
  real    :: AmplitudeBoundary_D(2:3) = -1.0
  !\
  ! public members
  !/
  public  :: read_laser_beam !reads the laser pulsed beam parameters
  public  :: laser_beam      !modifies the boundary field for laser beam 
  public  :: check_laser_beam
  !                                 
contains
  subroutine laser_beam(iDir, Xyz_D, EField)
    !\
    ! The electric field parameter EField has an intent inout
    ! The input value is found from 'noreflect' boundary condition 
    !/
    !\
    ! Calculate electric field iDir component in the point, Xyz_D 
    !/
    integer,intent(in)        :: iDir
    real,   intent(in)        :: Xyz_D(MaxDim)
    real,   intent(inout)     :: EField !Before and after assignment 
    !-------------------------
    !\
    ! FocalDistance for the point Xyz_D
    !/
    real    :: FocalDistance           = 0.0
    !\
    ! Ratio of local amplitude at the boundary to 
    ! that at the center of the pulsed beam 
    ! is calculated as the product of
    ! factors along different directions
    !/
    real    :: AmplitudeFactor_D(nDim) = 0.0
    !\
    ! The parameters of the field to be calculated
    !/    
    real    :: LocalAmplitude = 0.0
    real    :: LocalPhase     = 0.0
    !\
    !Misc
    !/
    real    :: Aux
    integer :: iDim
    real    :: timereal
    !==========================================
    !\
    ! Works for the left boundary along x only. 
    ! Beam is parallel to x-axis
    !/
    !\
    ! When the back front of the pulse passed through the boundary
    ! at the axis, the pulse no longer intersects the boundary.
    !/
    if(tSimulation > timePulseEnd)then
       UseLaserBeam = .false.
       RETURN
    end if
    !\
    ! FocalDistance for the point Xyz_D
    !/                                   
    FocalDistance = sqrt(sum((XyzFocus_D(1:nDim) - Xyz_D(1:nDim))**2))
    timeReal = tSimulation + FocalDistance - (XyzFocus_D(x_) - Xyz_D(x_))
    AmplitudeFactor_D(:)=0.0
    !
    if(timeReal<=timePulseEnd.and.timeReal>=timePulseBegin) then
       !
       LocalPhase = FocalDistance + tSimulation -(XyzFocus_D(x_) - xPulseCenter)
       !
       !==========================along X
       select case(nEnvelope)
       case(1)
          Aux =(LocalPhase/ GaussianWidthFocus_D(x_))**2
          if(Aux < 30.0)AmplitudeFactor_D(x_) = exp(-Aux)
       case(2)
          AmplitudeFactor_D(x_) = &
               cos(LocalPhase/PulseWidthFocus_D(x_)*cPi/2)
       case default
          AmplitudeFactor_D(x_) = 1.0
       end select
       !==========================along Y, Z====
       do iDim = y_, nDim
          Aux = ((Xyz_D(iDIm) - XyzFocus_D(iDim))/&
               GaussianWidthBoundary_D(iDim))**2
          if(Aux < 30.0)AmplitudeFactor_D(iDim) = exp(-Aux)
       end do

       LocalAmplitude = product(AmplitudeFactor_D(1:nDim))*&
            AmplitudeBoundary_D(iDir)
       LocalPhase = LocalPhase + PhaseShift
       !
       select case(iDir)
       case(y_)
          EField = LocalAmplitude*cos(LocalPhase)
       case(z_)
          EField = LocalAmplitude*sin(LocalPhase)
       case default
          call CON_stop(&
               'Only the beam propagation along x axis is implemented')
       end select
    end if
  end subroutine laser_beam
  !                                
  !================================================                          
  !=========reading command #LASERBEAM=============                       
  !================================================                        
  subroutine read_laser_beam
    use ModReadParam,ONLY: read_var
    integer:: iDim
    UseLaserBeam = .true.
    !======================================READING BEGIN          
    !AmplitudeFocus_D in focus
    !Polarization {Ey/=0 Ez=0} p-pol, {Ey=0 Ez/=0} s-pol 
    !Polarization {Ey/=0 Ez/=0} circular, elliptical
    call read_var('AmplitudeFocus',AmplitudeFocus_D(2))
    call read_var('AmplitudeFocus',AmplitudeFocus_D(3))
    !\
    ! PhaseShift: at t=0  
    ! Ey(xCenter)=cos(PhaseShift) 
    ! Ez(xCenter)=sin(PhaseShift); 
    call read_var('PhaseShift',PhaseShift)
    do iDim = 1, nDim
       call read_var('PulseWidthFocus',PulseWidthFocus_D(iDim))
    end do 
    do iDim = 1, nDim
       call read_var('XyzFocus',XyzFocus_D(iDim))
    end do
    !
    !Pulse longitudinal envelope: Default-Steplike, 
    !1-Gauss, 2-Cosine (FWHM - full width at half maximum) 
    !Transverse envelope - Gauss, exp(-2) at +-Width/2 
    call read_var('nEnvelope',nEnvelope)
  end subroutine read_laser_beam
  !========================================READING END
  subroutine check_laser_beam
    use PC_ModPhysics, ONLY: Io2No_V, No2Io_V, UnitX_, UnitT_
    use PIC_ModMain,   ONLY: TypeFieldBC_S, Dx_D
    real :: WidthHalfMax2Gaussian
    real :: ZK, ZrK_D(2:nDim), FocusDistanceMax
    !
    if(TypeFieldBC_S(1)/='laserbeam')then
       UseLaserBeam = .false.
       RETURN
    end if
    !=======================TRANSFORMATION OF DATA BEGIN
    PulseWidthFocus_D = PulseWidthFocus_D*Io2No_V(UnitX_)
    XyzFocus_D        = XyzFocus_D       *Io2No_V(UnitX_)
    PhaseShift        = PhaseShift       *Io2No_V(UnitT_)
    !\                             
    !For Gaussian distribution: Intensity/2 at pulse width 
    !/
    WidthHalfMax2Gaussian = 1.0/sqrt(2.0*alog(2.0)) !0.849322 
    GaussianWidthFocus_D = PulseWidthFocus_D*&
          WidthHalfMax2Gaussian
    !
    !=========================TRANSFORMATION OF DATA END
    !\
    !pulse diameter at x=0 is given by equation
    !w(z)=w(0)*sqrt(1+(Z/Zr)^2) 
    !see https://en.wikipedia.org/wiki/Gaussian_beam
    !zr = \pi w(0)^2/\lambda
    !Dimensionless lengths are multiplied by k, which gives
    ZrK_D = 0.5*GaussianWidthFocus_D(2:nDim)**2
    ZK = XyzFocus_D(x_) - (CoordMin_D(x_) - Dx_D(x_)*0.5)
    GaussianWidthBoundary_D = GaussianWidthFocus_D(2:nDim)*&
         sqrt(1.0 + (ZK/ZrK_D)**2)
    AmplitudeBoundary_D =  AmplitudeFocus_D*&
         sqrt(product(GaussianWidthFocus_D(2:nDim)/&
         GaussianWidthBoundary_D))
    FocusDistanceMax = sqrt(ZK**2 + sum(&
         max((CoordMax_D(2:nDim) - XyzFocus_D(2:nDim))**2,&
         (XyzFocus_D(2:nDim) - CoordMin_D(2:nDim))**2)))
    TimePulseBegin = FocusDistanceMax - ZK
    ! 


    !Double pulse width for Gaussian and Cosine envelopes
    if(nEnvelope==1)then
       xPulseCenter = CoordMin_D(x_) - Dx_D(x_) - &
            timePulseBegin - 3*PulseWidthFocus_D(1)
       timePulseEnd = timePulseBegin + PulseWidthFocus_D(1)*6.0
    elseif(nEnvelope==2) then 
       xPulseCenter = CoordMin_D(x_) - Dx_D(x_) - &
            timePulseBegin - PulseWidthFocus_D(1)
       timePulseEnd = timePulseBegin + PulseWidthFocus_D(1)*2.0
    else 
       xPulseCenter =  CoordMin_D(x_) - Dx_D(x_) - &
            timePulseBegin - PulseWidthFocus_D(1)/2.0
       timePulseEnd = timePulseBegin + PulseWidthFocus_D(1)
    end if
                                                                                
    if(iProc==0)then
       write(*,'(a,es13.5)')'Laser pulse of duration',&
            PulseWidthFocus_D(1)*No2Io_V(UnitX_)
       write(*,'(a,2es13.5)')'is focused to a spot of',&
               PulseWidthFocus_D(2:nDim)*No2Io_V(UnitX_)
       write(*,'(a,3es13.5)')'  at the focal point Xyz=',XyzFocus_D(1:nDim)&
            *No2Io_V(UnitX_)
       write(*,'(a,2es13.5)')'PulseWidthBoundary_D=',&
            GaussianWidthBoundary_D*No2Io_V(UnitX_)
       write(*,'(a,3es13.5)')'timePulseBegin,timePulseEnd,xPulseCenter=',&
            timePulseBegin*No2Io_V(UnitT_),timePulseEnd*No2Io_V(UnitT_), &
            xPulseCenter*No2Io_V(UnitX_)
       write(*,'(a,es13.5,a,i5)')'Pulse should be focused at time=',&
            (XyzFocus_D(x_)-xPulseCenter)*No2Io_V(UnitT_),&
            ', it=', floor((XyzFocus_D(x_) - xPulseCenter)/dt)
    end if  
  end subroutine check_laser_beam
end module PIC_ModLaserBeam
