module task_dimension ! ---------------------------------------------
  integer, parameter :: nDim=1 ! task dimension 
end module task_dimension
!
module variables_box ! ----------------------------------------------
  use task_dimension
  integer, parameter :: mx=80 ! mesh resolution per lambda
  integer, parameter :: mx2=mx ! ghost in 1D
  integer, parameter :: mt=100 !mx*2 ! the number of time steps per cycle 
  integer,dimension(nDim),parameter:: nMask=(/mx*0/) ! mask 0/equal 
  integer, parameter :: nx=mx*100+nMask(1)*2 !number of cells in X!for:E,B,Z,C
  integer,parameter:: nMax(nDim)=(/nx/) !grid number along directions 
  integer,parameter:: nMaxProd=nx !grid number in the box
end module variables_box
!
module moving_box
  use variables_box, only : mx,mt
  logical :: logMove=.false.!.true.! to move the box !Mask is not needed
  logical :: logAdd=.false. ! to add particles (case: nPartSorts=1 only)
  integer, parameter :: ntMove=mt*5 !each ntMove instant to shift the frame 
  integer :: nxMove=mx*5 ! shift nxMove cells </>0 to the left/right
  ! transition: +, reflection: -
  real :: tMove=24.8
  real :: tAddStop=1249.8 !stop to add particles
end module moving_box
!
module variables_particles ! ----------------------------------------
  use task_dimension
  use variables_box, only : mx,nMask
  integer, parameter :: nPartMax=500000 ! maximal number of particles
  integer, parameter :: nPartSorts=2 ! 0,1,2 - no part, only e-, e-&i+ TEST:=1
  integer, parameter :: nPartSortMax=2 ! nPartSorts <= nPartSortMax
  integer, dimension(nPartSortMax) :: np1, np2 ! limits of particles 
  integer :: nPartCell ! number of particles per cell
  !for initial distribution - could be moved to a separate module: nPart,iBound
  integer :: nPart(nDim)=(/100/) ! part/cell along x,y,z
  integer :: iBound1(nDim)=(/mx*1/)+nMask !lower bound for part in cells
  integer :: iBound2(nDim)=(/mx*0.5+2*0/)+nMask !upper bound for part in cells
end module variables_particles
!
module fields ! -----------------------------------------------------
  use variables_box
  real, dimension(1:3, 0:nx+2 ) :: E,B !???
  real :: Z( 1:nx ) ! density ???? Real_4 ???
  real :: C( 1:3, 0:nx+2 ) ! current
  real :: rMask( 0:nMask(1)-1 ) !mask array
  real, dimension(1:nMaxProd+180*36*100) :: &
       rCommArray, &!(kind=4)?!Communication Array
       rCommArrayGlob !Global real 1D Communication Array
  integer, dimension(1:2000000) :: iCommArray, & !integer Communication Array
       iCommArrayGlob ! Global integer 1D Communication Array
end module fields
!
module particles ! --------------------------------------------------
  use task_dimension
  use variables_particles
  logical,parameter :: logIoniz=.false. ! ionization =.true., .false.
  integer,parameter :: iLogRad=1 ! 1 or 0 !!! radiation reaction for e-
  logical :: logRadIn
  real :: R( nDim+3, nPartMax) ! Rx, Ry, Rz, Px, Py, Pz (normalized P/Mc) 
  integer,parameter :: nRD=1 ! only for test, >= 1 
  real :: RD( nRD, nPartMax/2*(3-nPartSorts)*iLogRad+1) !for radiation reaction
  integer, dimension(nPartMax) :: nCharge ! =0 at start if logIoniz=T, or =1 
end module particles
!
module variables_data !--------------------------------------------- 
  use task_dimension
  use variables_particles, only : nPartSortMax
  integer :: iTime! current time step
  integer :: iStart ! for exchange : wave front location moving with c
  integer :: nPhase(nDim+3)=(/200,200,100,100/) !number/grids in phase space (100-200)
  real :: time, dt, dtHalf 
  real :: timeMax=100.1 ! max time
  real :: fieldInit(6) ! constant field initiation 
  real :: dx(nDim), dxHalf(nDim), dxInv(nDim) !space variables
  real :: dtdxInv(nDim), dtdxInvHalf(nDim), dxWeight(3) !
  real :: xStart ! wave front location moving with speed of light
  real :: xMin(nDim), xMax(nDim) ! box limits
  real :: xMinPart(nDim+3),xMaxPart(nDim+3) ! local particle limits 
  real :: xMinPartGlob(nDim+3),xMaxPartGlob(nDim+3) ! global particle limits
  real :: weight ! particle weight
  real, dimension(nPartSortMax) :: charge ! particle charge
  real, dimension(nPartSortMax) :: mass ! particle mass
  real, dimension(nPartSortMax) :: dtChargeMassInvHalf ! for particle motion
  integer,parameter :: nBeam=1, nBeamMax=2 ! the number of beams 
  real :: phaseShift(nBeamMax) ! =0: Ey=cos, Ez=sin
  real :: amplitude(nBeamMax) ! dimensionless amplitude 
  real :: omega ! plasma frequency in laser frequencies
  real :: polarization(nBeamMax,2:3) ! amplitude coefficients along y,z
  integer :: nProfile(nBeamMax)=(/2,1/) ! 1=Gauss 2=Fronts 3=exponential fronts
  integer :: nFocus=1 !=0/1 plane/focus !>=2D
  integer :: jBeam ! current beam
  real :: focus(nBeamMax,1:3) ! distance to focus along X,Y,Z
  real :: beamSize(nBeamMax,3),beamMiddle(nBeamMax,3),beamLimit(nBeamMax,2) 
  real :: beamFront(nBeamMax,2),beamFrontCoef(nBeamMax,2)
  !beam size,middle,limits,front along x
  integer :: iError=0 !error number
  !  character (len=*) :: nameLocal !location for error report ???
  logical :: logAntenna
  logical :: logTest ! for test particle motion: zero-th current
  character (len=16) :: nameLocal !location for error report
  character (len=6) :: nameTimeStep
  character (len=1), dimension(nPartSortMax) :: namePart=(/'e','i'/)
end module variables_data
!
module variables_out
  use variables_particles, only : nPartSortMax
  use variables_box, only : mx,mt
  use particles, only : iLogRad
  integer, dimension(nPartSortMax) :: nPartWork ! N working particles /mom/ener
  real, dimension(nPartSortMax,3)  :: mom ! sum momentum for particles 
  real :: rad ! total radiation - only for electrons
  integer,parameter :: nAngle= 180 *iLogRad+ 1*(1-iLogRad) !180 
  integer,parameter :: nAngle2= 36 *iLogRad+ 1*(1-iLogRad) !36
  integer,parameter :: nFreq= 100 *iLogRad+ 1*(1-iLogRad)
  integer,parameter :: nChi= 101 *iLogRad+ 2*(1-iLogRad)
  integer,parameter :: nAngleProd=nAngle*nAngle2
  integer,parameter :: nFreqProd=nAngle*nAngle2*nFreq
  integer,parameter :: nFreqChiProd=nFreq*nChi
  real,parameter :: FreqMax=1.0e10 !Max energy - 10^10 mc^2, or 0.511 GeV
  real :: rFreqMax !alog(FreqMax)
  ! to read frequency: exp( x/nFreq*alog(FreqMax))
real,parameter :: ChiMax=1.0d0 !Max value of chi
  real, dimension(nAngle,nAngle2):: radAngle ! radiation ang distr 
  !real, dimension(nAngle,nAngle2,nFreq,nChi):: radFreq ! radiation ang distr
  real, dimension(nChi):: radChi ! a lookup table for I/I_cl
  real, dimension(nFreq,nChi):: radFreqChi0,radFreqChi1 & ! radiation QED distr
       ,radFreqChiCos0,radFreqChiCos1
real, dimension(nPartSortMax) :: enerKin ! kinetic energy for particles 
  real :: enerFieldE(3),enerFieldB(3) ! energy in electric and magnetic fields
  integer :: nOutputField=mt*10 !step output for fields
  integer :: nOutputPhase=mt*10 !step output for phases
  integer :: nOutputPhaseAll=mt*1000 !step output for phases
  integer :: nOutputTime=1 !mt !step output for energy evolution
  logical :: logPrint !printing
  logical :: logMomEner=.true. !caculation of momenta and energy
  logical :: logOutput !data output
  character (len=2) :: format_output='u' !'f','u'
end module variables_out
!
module constants ! ----------------------------------------------------
  real, parameter :: PI=&
     &3.1415926535897932384626433832795028841971693993751058209749446 
  real, parameter :: PI2=PI+PI
  real, parameter :: ZERO=0.0000000000000000000000000000000000000000000000000
  real, parameter :: ONE=1.00000000000000000000000000000000000000000000000000
  real, parameter :: TWO=2.00000000000000000000000000000000000000000000000000
  real, parameter :: THREE=3.000000000000000000000000000000000000000000000000
  real, parameter :: FOUR=4.000000000000000000000000000000000000000000000000
  real, parameter :: FIVE=5.000000000000000000000000000000000000000000000000
  real, parameter :: HALF=0.5000000000000000000000000000000000000000000000000
  real, parameter :: THIRD=.3333333333333333333333333333333333333333333333333
  real, parameter :: QUARTER=0.2500000000000000000000000000000000000000000000
  real, parameter :: SIXTH=0.166666666666666666666666666666666666666666666667
  real, parameter :: EIGHTH=0.12500000000000000000000000000000000000000000000
  real, parameter :: THREE_FOURTH=0.75000000000000000000000000000000000000000
  real, parameter :: constVelLight=2.99792458e10
  real, parameter :: constLaserT=2.66666666666666666666666666666666666666e-15
  real :: constRad,constCompton,constComptonInv,constLambda
end module constants
!
