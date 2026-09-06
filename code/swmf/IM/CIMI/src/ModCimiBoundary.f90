module ModCimiBoundary

  use ModCimiGrid,    ONLY: MinLonPar,MaxLonPar,nt,np
  use ModCimiPlanet,  ONLY: nSpec, amu_I, dFactor_I, tFactor_I
  use ModUtilities,   ONLY: CON_stop

  implicit none
  
  SAVE

  private ! EXCEPT:

  public :: cimi_set_boundary_mhd
  public :: cimi_set_boundary_empirical
  
  !when in standalone mode which boundary type do you use for the RC?
  logical,public :: UseBoundaryEbihara =.true.
  logical,public :: UseYoungEtAl =.false.
  logical,public :: CIMIboundary = .false.
  logical,public :: Outputboundary = .false.
  
  !boundary density in m^-3 and temperature in eV
  real, public :: BoundaryDens_IC(nSpec,nt)
  real, public :: BoundaryTemp_IC(nSpec,nt),BoundaryTempPar_IC(nSpec,nt)

contains
  !============================================================================
  subroutine cimi_set_boundary_mhd
    use ModGmCimi,      ONLY: Den_IC,Temp_IC,Temppar_IC,StateBmin_IIV,&
         TotalP_,TotalPpar_,TotalRho_, iBufferRho_I,iBufferP_I,&
         iBufferPpar_I, iLatMin,&
         UseTotalRhoGm, UseTotalPGm, UseTotalPparGm, UseMultiRhoGm , &
         UseMultiPGm, UseMultiPparGm, DoFeedbackPs, UsePeGm, TotalPe_      
    
    use ModCimiTrace,  ONLY: irm, iba
    use ModConst, ONLY: cProtonMass
    integer :: iSpecies, iLon, iLat, jLat
    real, parameter :: cJouleToEv=6.2415e18  
    real :: FactorTotalDens
    integer :: ib1
    !---------------------------------------------------------------------------

    ! If using separate GM electron pressure, all values should just be passed
    ! without any temperature partitioning
    if (UsePeGm) tFactor_I = 1.0
    
    !set boundary density and temperature inside irm
    if (UseTotalRhoGm &
         .and. .not. UseMultiRhoGm &
         .and. .not. UseMultiPGm   &
         .and. .not. UseMultiPparGm) then
     ! When not Multifluid we get the total density from Rho_MHD as follows:
     ! Rho = (m1n1+m2n2+...) = n * sum(m_i*dFactor_i)
     ! where sum(dFactor_i)=1 (over ions) and n_i=dFactor_i*n 
     ! n_i = dFactor_i*Rho_MHD/(sum(m_i*dFactor_i))
     ! n_total = Rho_MHD/sum(m_i*dfactor_i)
     ! FactorTotalDens = sum(m_i*dfactor_i)
     FactorTotalDens = sum(dFactor_I(1:nSpec-1)*amu_I(1:nSpec-1))
     do iSpecies = 1, nSpec
        do iLon=MinLonPar,MaxLonPar
           ! Set boundary index consistant with boundaryIM
           ib1=min(iba(iLon)+1,irm(iLon))
           do iLat=1,irm(iLon)
              ! Use boundary values for iLat>iLatMin
              jLat = max(iLat, iLatMin)

              !write(*,*)StateBmin_IIV(jLat,iLon,TotalRho_)
              Den_IC(iSpecies,iLat,iLon) = dFactor_I(iSpecies) * &
                   StateBmin_IIV(jLat,iLon,TotalRho_)&
                   /(cProtonMass * FactorTotalDens)
              Temp_IC(iSpecies,iLat,iLon) = tFactor_I(iSpecies) * &
                   StateBmin_IIV(jLat,iLon,TotalP_) &
                   * cProtonMass * FactorTotalDens &
                   / StateBmin_IIV(jLat,iLon,TotalRho_) &
                   * cJouleToEv
              if(UseTotalPparGm) then
                 Temppar_IC(iSpecies,iLat,iLon) = tFactor_I(iSpecies) * &
                      StateBmin_IIV(jLat,iLon,TotalPpar_) &
                      * cProtonMass * FactorTotalDens &
                      / StateBmin_IIV(jLat,iLon,TotalRho_) &
                      * cJouleToEv
              else
                 Temppar_IC(iSpecies,iLat,iLon) = Temp_IC(iSpecies,iLat,iLon)
              endif
           end do
        end do
     end do
  elseif (UseMultiRhoGm  &
       .and. UseTotalPGm &
       .and. .not. UseMultiPGm   &
       .and. .not. UseMultiPparGm) then
     ! Multispecies Case
     
     do iLon=MinLonPar,MaxLonPar
        ! Set boundary index consistant with boundaryIM
        ib1=min(iba(iLon)+1,irm(iLon))
        do iLat=1,irm(iLon) 
           ! Use boundary values for iLat>iLatMin
           jLat = max(iLat, iLatMin)
           do iSpecies = 1, nSpec-1
              Den_IC(iSpecies,iLat,iLon) = &
                   StateBmin_IIV(jLat,iLon,iBufferRho_I(iSpecies))&
                   / (cProtonMass*amu_I(iSpecies))
           enddo
           ! set electron density
           Den_IC(nSpec,iLat,iLon) = sum(Den_IC(1:nSpec-1,iLat,iLon))
           
           ! set temperatures
           Temp_IC(1:nSpec,iLat,iLon) = tFactor_I(1:nSpec) * &
                StateBmin_IIV(jLat,iLon,TotalP_)  &
                /(Den_IC(nSpec,iLat,iLon)) &
                * cJouleToEv
           if(UseTotalPparGm) then
              Temppar_IC(1:nSpec,iLat,iLon) = tFactor_I(iSpecies) * &
                   StateBmin_IIV(jLat,iLon,TotalPpar_) &
                   / (Den_IC(nSpec,iLat,iLon)) &
                   * cJouleToEv
           else
              Temppar_IC(:,iLat,iLon) = Temp_IC(:,iLat,iLon)
           endif
           
        end do
     end do


  elseif (UseMultiRhoGm .and. UseMultiPGm ) then
     !Multifluid Case

     do iLon=MinLonPar,MaxLonPar
        ! Set boundary index consistant with boundaryIM
        ib1=min(iba(iLon)+1,irm(iLon))
        do iLat=1,irm(iLon) 
           ! Use boundary values for iLat>iLatMin
           jLat = max(iLat, iLatMin)

           do iSpecies = 1, nSpec-1
              Den_IC(iSpecies,iLat,iLon) = &
                   StateBmin_IIV(jLat,iLon,iBufferRho_I(iSpecies))&
                   / (cProtonMass*amu_I(iSpecies))
              
              !set temperatures
              Temp_IC(iSpecies,iLat,iLon) = &
                   StateBmin_IIV(jLat,iLon,iBufferP_I(iSpecies))  &
                   /(Den_IC(iSpecies,iLat,iLon)) &
                   * cJouleToEv
              if(UseMultiPparGm) then
                 Temppar_IC(iSpecies,iLat,iLon) = &
                      StateBmin_IIV(jLat,iLon,iBufferPpar_I(iSpecies)) &
                      / (Den_IC(iSpecies,iLat,iLon)) &
                      * cJouleToEv
              else
                 Temppar_IC(iSpecies,iLat,iLon) = Temp_IC(iSpecies,iLat,iLon)
              endif
           enddo

           ! set electron density
           Den_IC(nSpec,iLat,iLon) = sum(Den_IC(1:nSpec-1,iLat,iLon))
           
           ! set electron temperatures
           Temp_IC(nSpec,iLat,iLon) = &
                sum(StateBmin_IIV(jLat,iLon,iBufferP_I(1:nSpec-1)))  &
                /(Den_IC(nSpec,iLat,iLon)) &
                * cJouleToEv
           Temppar_IC(nSpec,iLat,iLon) = Temp_IC(nSpec,iLat,iLon)
        end do
     end do
  else
     call CON_stop('Unknown condition in ModCimiBoundary')
  endif
  
  !Overwrite e temperature if solving for Pe in GM
  if (UsePeGm) then
     do iLon=MinLonPar,MaxLonPar
        ! Set boundary index consistant with boundaryIM
        ib1=min(iba(iLon)+1,irm(iLon))
        do iLat=1,irm(iLon)
           jLat = max(iLat, iLatMin)
           Temp_IC(nSpec,iLat,iLon) = StateBmin_IIV(jLat,iLon,TotalPe_)&
                / Den_IC(nSpec,iLat,iLon)* cJouleToEv
           Temppar_IC(nSpec,iLat,iLon) = Temp_IC(nSpec,iLat,iLon)
        end do
     end do
  endif
  
  ! Fill the boundary n, T, and T||
  do iSpecies = 1,nSpec
     BOUNDARY_FILL: do iLon=MinLonPar,MaxLonPar
        ! Fill boundary arrays
        ib1=min(iba(iLon)+1,irm(iLon))
        !write(*,*) iSpecies, ib1,iLon,Temp_IC (iSpecies,ib1,iLon) 
        BoundaryDens_IC(iSpecies,iLon) = &
             Den_IC (iSpecies,ib1,iLon)
        BoundaryTemp_IC(iSpecies,iLon) = &
             Temp_IC(iSpecies,ib1,iLon)
        BoundaryTempPar_IC(iSpecies,iLon) = &
             Temppar_IC(iSpecies,ib1,iLon)
        
     end do BOUNDARY_FILL
  enddo

  end subroutine cimi_set_boundary_mhd


  !============================================================================
  subroutine cimi_set_boundary_empirical
    use ModImTime,    ONLY: CurrentTime, TimeLagBoundary
    use ModCimiTrace,ONLY: iba, ro, xmlto, irm
    use ModGmCimi,      ONLY: Den_IC,Temp_IC
    use ModImIndices,   ONLY: UseKpApF107IndicesFile,get_im_indices_F107,&
         get_im_indices_Kp
    use ModIndicesInterfaces
    use ModNumConst,       ONLY: cPi
    
    
    real,parameter :: cCm3toM3 =1.0e6, cKevTOeV=1000.0
    real :: TimeBoundary, Bz, vsw, xnsw, Rtsy, phit
    integer :: iError, iLon, ib1, iSpecies
    
    real :: xn1(nt), xkt1(nt)
    !variable for young et al density split
    real :: O_H_ratio, F107, Kp
    !--------------------------------------------------------------------------
    
    if(tFactor_I(1) < 1) tFactor_I = tFactor_I / tFactor_I(1)
    ! If using Young et al and nSpec=3 then overwrite dFactor_I
    if(UseYoungEtAl .and. nSpec==3) then
       !Get Inputs
       if (UseKpApF107IndicesFile) then
          call get_im_indices_Kp(CurrentTime, Kp)
          call get_im_indices_F107(CurrentTime, F107)
       else
          call get_kp(CurrentTime, Kp, iError)
          call get_F107(CurrentTime, F107, iError)
       endif
       !Set O+:H+ ratio
       O_H_ratio = get_OtoH_young(F107, Kp)
       dFactor_I(1) = 1.0/(O_H_ratio+1.0)        !H+
       dFactor_I(2) = O_H_ratio/(O_H_ratio+1.0)  !O+

    endif
    
    If (UseBoundaryEbihara) then
       ! Use Ebihara-Ejiri-Borovsky or Tsyganenko-Mukai model for 
       ! boundary n and kT
       ! Set TimeBoundary to CurrentTime with a time lag
       TimeBoundary=CurrentTime - TimeLagBoundary
       call get_IMF_Bz(TimeBoundary, Bz, iError)
       call get_SW_V  (TimeBoundary, vsw, iError)
       call get_SW_N  (TimeBoundary, xnsw, iError)
       
       ! Density in m^-3, Ebihara & Ejiri 2000
       xn1(1:nt)=(0.025*xnsw+0.395)*cCm3toM3 
       
       ! kT_ion in eV, Borovsky etal 1998
       xkt1(1:nt)=(0.019*vsw-3.65)*cKevTOeV          
    else
       ! use Tsyganenko-Mukai PS model
       call get_IMF_Bz(CurrentTime, Bz, iError)
       call get_SW_V  (CurrentTime, vsw, iError)
       call get_SW_N  (CurrentTime, xnsw, iError)
       do iLon=1,nt  
          ! Set boundary index consistant with boundaryIM
          ib1=min(iba(iLon)+1,irm(iLon))
          Rtsy=ro(ib1,iLon)
          if (Rtsy.lt.10.) Rtsy=10.   ! model only for r > 10
          phit=xmlto(ib1,iLon)*cPi/12.
          call get_tsy_plasma(Bz,vsw,xnsw,Rtsy,phit,xkt1(iLon),xn1(iLon))
       enddo
    endif

    ! Fill boundary arrays for all species
    do iSpecies = 1, nSpec
       BoundaryDens_IC(iSpecies,1:nt) = dFactor_I(iSpecies)*xn1(1:nt)
       BoundaryTemp_IC(iSpecies,1:nt) = tFactor_I(iSpecies)*xkt1(1:nt)
       BoundaryTempPar_IC(iSpecies,1:nt) = BoundaryTemp_IC(iSpecies,1:nt)
    end do
 
 end subroutine cimi_set_boundary_empirical

 !============================================================================
 real function get_OtoH_young(F107, Kp) result(O_H_ratio)
   real, intent(in) :: F107, Kp
   O_H_ratio=4.5e-2*exp(0.17*Kp+0.01*F107)  ! O+/H+ ratio (Young etal 1982)
 end function get_OtoH_young
 
 !============================================================================
 !-----------------------------------------------------------------------------
 subroutine get_tsy_plasma(Bz,Vsw,Nsw,r,phi,t_tsy,n_tsy)
   !---------------------------------------------------------------------------
   ! Subroutine of Tsyganenko-Mukai plasma sheet model.
   ! Reference: Tsyganenko, N. A., and T. Mukai, Tail plasma sheet models 
   !               derived from Geotail particle data, J. Geophys. Res., 
   !               108(A3), 1136,doi:10.1029/2002JA009707, 2003.
   use ModIoUnit, ONLY: UnitTmp_
   implicit none
   integer, parameter :: n_points_tsy=16
   
   ! variable for reading and storing data parameters
   real, save :: a_tsy(6,16)
   real    :: coef1(6), tmp
   integer :: i

   real Bz,Bn,Bs,V,Vsw,r,rho,phi,N,Nsw,t_tsy,n_tsy,temp1,temp2,temp3
   logical :: IsFirstCall = .true.
   !---------------------------------------------------------------------------
   
   ! Read data for Tsyganenko-Mukai plasmasheet model
   if (IsFirstCall) then
      open(UnitTmp_,file='TSYGANENKO_MUKAI.dat',status='old')
      do i=1,n_points_tsy
         read(UnitTmp_,*) tmp,coef1
         a_tsy(1:6,i)=coef1(1:6)
      enddo
      close(UnitTmp_)
      IsFirstCall = .false.
   endif

   if (Bz.ge.0.) then
      Bn=Bz/5.
      Bs=0.
   else
      Bn=0.
      Bs=-Bz/5.
   endif
   
   V=Vsw/500.
   rho=r/10.
   N=Nsw/10.
   
   ! temperature final fit (equation (4), page 5)
   temp1=a_tsy(1,9)*(V**a_tsy(1,15))+a_tsy(1,10)*Bn+a_tsy(1,11)*Bs
   temp1=temp1*(-1.)*(rho-1.)
   temp2=a_tsy(1,12)*(V**a_tsy(1,16))+a_tsy(1,13)*Bn+a_tsy(1,14)*Bs
   temp2=temp2*(-1.)*(rho-1.)
   temp3=a_tsy(1,5)*V+a_tsy(1,6)*Bn+a_tsy(1,7)*Bs+a_tsy(1,8)*exp(temp2)
   t_tsy=a_tsy(1,1)*V+a_tsy(1,2)*Bn+a_tsy(1,3)*Bs+a_tsy(1,4)*exp(temp1)+ &
        (temp3*sin(phi)*sin(phi))
   
   ! density final fit (equation (5), page 6)
   temp1=a_tsy(3,1)+a_tsy(3,2)*(N**a_tsy(3,10))+a_tsy(3,3)*Bn+a_tsy(3,4)*V*Bs
   temp2=a_tsy(3,5)*(N**a_tsy(3,11))+a_tsy(3,6)*Bn+a_tsy(3,7)*V*Bs
   n_tsy=temp1*(rho**a_tsy(3,8)) + temp2*(rho*a_tsy(3,9))*sin(phi)*sin(phi)
   
 end subroutine get_tsy_plasma
 
end module ModCimiBoundary
