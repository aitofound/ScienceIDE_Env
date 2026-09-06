!module DensityTemp
!
!  use ModCimiGrid, ONLY: ir=>np, ip=>nt !, ik=>nk
!
!  implicit none
!  
!  real 			:: 	density( ir, ip ) = 0.1
!
!  ! plasmaspheric density (m^-3) to define plasmapause; needed to
!  ! avoid applying chorus and hiss at the same time
!  real			:: 	densityP = 20.e6
!
!  
!  !public method
!  public :: simple_plasmasphere
!  
!contains
!  
!
!
!end module DensityTemp

!!!!!!!!!!!!!!!!  Main wave module !!!!!!!!!!!!!1
module ModWaveDiff
  
  use ModCimiGrid,	ONLY:	ir => np, ip => nt, iw => nm, ik => nk  
  use ModCimiPlanet,	ONLY:	nspec
  use ModCimiTrace,	ONLY: 	&
       ekev, tya, y => sinA, Bo, ro, xmlto, lintpIM, bm
  use ModNumConst,	ONLY:	pi => cPi
  use ModUtilities,	ONLY:	CON_STOP
  use CIMI_waves,         ONLY: cOmpe,hOmpe,ckeV,hkeV,cDaa,cDEE,cDaE,&
       hDaa,hDEE,hDaE,UseChorus,UseHiss,&
       ipc,iwc,iph,iwh,ipa,&
       Cpower0,Hpower0,BLc0,BLh0,&
       CHpower,HIpower,ompe,r_wave,cPA
  implicit none
  
  !real               :: DiffStartT = 0.
  private 
  ! these variables are needed for testing purposes only
  logical            :: testDiff_aa = .false.
  logical            :: testDiff_EE = .false.
  logical            :: testDiff_aE = .false.
  
  
  ! from module cimigrid_dim in CIMI: dimension of Qiuhua's UB chorus
  ! coef
  !integer,parameter :: ipu=6,iwu=31,ipa=89
  ! from module cimigrid_dim in CIMI: dimension of LB chorus & hiss
  ! diff coef
  !integer ipc,iwc,iph,iwh
  ! switches for three wave models integer iba(ip),iw2(nspec,ik)
  !integer ichor,iUBC,ihiss

  ! from module WaveDiffCoef in cimi
  !real, allocatable, dimension(:) :: cOmpe,ckeV,hOmpe,hkeV
  !real, allocatable, dimension(:,:,:) :: cDEE,cDaa,cDaE,hDEE,hDaa,hDaE
  !real cPA(ipa),uOmpe(ipu),ukeV(iwu), &
  !     uDEE(ipu,iwu,ipa),uDaa(ipu,iwu,ipa),uDaE(ipu,iwu,ipa)
  
  !from module cWpower in cimi
  !real CHpower(ir,ip),UBCpower(ir,ip),HIpower(ir,ip),ompe(ir,ip),r_wave
  !real Cpower0,Hpower0,BLc0,BLh0,BLu0
  
  ! from module constants
  !real,parameter :: re_m=6.375e6                ! earth's radius (m)
  !real,parameter :: e_mass=9.11e-31             ! electron mass in kg
  !real,parameter :: epsilon0=8.8542e-12         ! permittivity of free space
  
  ! For ae index
 ! real, allocatable :: TimeAeIndex_I(:), AeIndex_I(:)
 ! character(len=100) :: NameAeFile
 ! integer :: NumAeElements

  public :: diffuse_aa,diffuse_EE,diffuse_aE
  
contains

 
!!!!!!!!!!!!!!!!!!!   main  subroutine  for diffusion in pitch-angle !!!!!!!!!!!!

  subroutine diffuse_aa(f2,dt,xjac,iba,iw2)
    use ModPlasmasphere, ONLY:PlasDensity_C,PlasmaPauseDensity
    use ModMPI
    use ModCimiGrid,   ONLY: MinLonPar,MaxLonPar
    use ModCimiUtil, ONLY: lintp3IM,tridagIM
    
    implicit none
    
    integer,parameter :: ie=40 ! diffusion subroutine has its own energy grid
    integer i,j,m,k,irun,n,nn,ier
    integer iba(ip),iw2(nspec,ik)
    real f2(nspec,ir,ip,iw,ik),xjac(nspec,ir,iw),dt  ! xjac has different dimension from CIMI !
    
    
    real ein_log(ie),einlog, &
         f1d(iw),e1d(iw),ein(ie),ao(0:ik+1),dao(ik),Gjac(0:ik+1),DD(0:ik+1),&
         um(ik),up(ik),a1d(ik),b1d(ik),c1d(ik),f0(0:ik+1),uDaoao, &
         f2d(ie,ik),df(ie,ik),df1(ie),fr(ik),ekevlog(iw,ik), &
         hDaoao,Cfactor,Upower0,Hfactor,Daoao,Ufactor, &
         ckeV_log(iwc),hkeV_log(iwh)
    !ukeV_log(iwu)
    real u_max,u_max_log,ompe1,ro1,emin,emax,x0,x2,x,cDaoao
    real u_mx,u_mx_log,factor1,factor_1,DDm,DDp,ump_mx,xlam,alam,dpsd,ao_d
    real edmin,edmax
    character(len=6)  ::  tchar
    
    
    u_max=5.              ! magic number for numerical method from M.C. Fok
    u_max_log=log10(u_max)
    Upower0=10000.        ! coeff based on UB chorus power of (100pT)^2
    
    ckeV_log(:)=log10(ckeV(:))
    !ukeV_log(:)=log10(ukeV(:))
    hkeV_log(:)=log10(hkeV(:))
    
    
    ! determine edmax and edmin
    !if (iUBC.eq.1) edmax=ukeV(iwu)
    if (UseHiss) edmax=hkeV(iwh)
    if (UseChorus) edmax=ckeV(iwc)
    if (UseHiss) edmin=hkeV(1)
    if (UseChorus) edmin=ckeV(1)
    !if (iUBC.eq.1) edmin=ukeV(1)
    
    n=nspec  ! use last index to access electrons; different from CIMI
    
    !   do j=1,ip
    do j=MinLonPar,MaxLonPar
       do i=1,iba(j)
          ro1=ro(i,j)
          ompe1=ompe(i,j)                           ! fpe/fce
          Cfactor=(BLc0/bo(i,j))*CHpower(i,j)/Cpower0
          !Ufactor=(BLu0/bo(i,j))*UBCpower(i,j)/Upower0
          Ufactor=0.0
          Hfactor=(BLh0/bo(i,j))*HIpower(i,j)/Hpower0
          
          ! for testing; this is because we do not have realistic
          !        plasmasphere now
          !ompe1=cOmpe(1)

          
          if (ompe1.ge.cOmpe(1).and.ompe1.le.cOmpe(ipc).and.ro1.ge.r_wave) then
             ! Set up the energy grid, ein
             emin=minval(ekev(n,i,j,1,1:ik))
             emax=maxval(ekev(n,i,j,iw,1:ik))
             ein(1)=emin
             ein(ie)=emax
             ein_log(1)=log10(emin)
             ein_log(ie)=log10(emax)
             x0=ein_log(1)
             x2=ein_log(ie)
             x=(x2-x0)/(ie-1)
             do k=2,ie-1
                ein_log(k)=x0+(k-1)*x
                ein(k)=10.**ein_log(k)
             enddo
             
             ! Map psd to ein grid, f2d
             f2d(:,:)=0.
             do m=1,ik
                do k=1,iw
                   f1d(k)=-50.                      ! f1d is log(psd)
                   if(f2(n,i,j,k,m).gt.0.)&
                        f1d(k)=log10(f2(n,i,j,k,m)/xjac(n,i,k))
                   
                   if (k.gt.iw2(n,m)) f1d(k)=f1d(iw2(n,m))
                   ekevlog(k,m)=log10(ekev(n,i,j,k,m))
                   e1d(k)=ekevlog(k,m)              ! e1d is log(ekev)
                enddo
                
                tchar='difaa1'
                do k=1,ie
                   einlog=ein_log(k)
                   if (einlog.lt.e1d(1)) einlog=e1d(1)
                   if (einlog.le.e1d(iw)) then
                      call lintpIM_diff(e1d,f1d,iw,einlog,x,tchar)
                      f2d(k,m)=10.**x      ! f2d is psd
                   endif
                enddo
             enddo
             
             
             ! calcuate ao, dao, and Gjac
             do m=0,ik+1
                ao(m)=asin(y(i,j,m))
                Gjac(m)=tya(i,j,m)*y(i,j,m)*sqrt(1.-y(i,j,m)*y(i,j,m))
             enddo
             do m=1,ik
                dao(m)=0.5*(ao(m+1)-ao(m-1))
             enddo
             
             df=0.
             
             do k=1,ie      ! *****
                if (ein(k).ge.edmin.and.ein(k).le.edmax) then
                   ! calculate DD, Daoao*Gjac
                   do m=0,ik+1
                      ao_d=ao(m)*180./pi
                      if (ao_d.lt.cPA(1)) ao_d=cPA(1)
                      if (ao_d.gt.cPA(ipa)) ao_d=cPA(ipa)
                      cDaoao=0.
                      if (UseChorus.and.PlasDensity_C(i,j).le.PlasmaPauseDensity) then
                         if (ein(k).ge.ckeV(1).and.ein(k).le.ckeV(iwc)) &
                              call lintp3IM(cOmpe,ckeV_log,cPA,cDaa,ipc,iwc, &
                              ipa,ompe1,ein_log(k),ao_d,cDaoao)
                      endif
                      uDaoao=0.
                      !if (iUBC.eq.1.and.PlasDensity_Ci,j).le.PlasmaPauseDensity) then
                      !   if (ein(k).ge.ukeV(1).and.ein(k).le.ukeV(iwu)) &
                      !        call lintp3IM(uOmpe,ukeV_log,cPA,uDaa,ipu,iwu, &
                      !        ipa,ompe1,ein_log(k),ao_d,uDaoao)
                      !endif
                      hDaoao=0.
                      if(UseHiss.and.&
                           ompe1.ge.2..and.&
                           PlasDensity_C(i,j).gt.PlasmaPauseDensity)then
                         if (ein(k).ge.hkeV(1).and.ein(k).le.hkeV(iwh))&
                              call lintp3IM(hOmpe,hkeV_log,cPA,hDaa,iph,iwh, &
                              ipa,ompe1,ein_log(k),ao_d,hDaoao)
                      endif
                      
                      Daoao=cDaoao*Cfactor+uDaoao*Ufactor+hDaoao*Hfactor
                      DD(m)=Daoao*Gjac(m)
                      
                   enddo
                   
                   ! calculate up and um
                   u_mx=0.
                   do m=1,ik
                      factor_1=dao(m)*(ao(m)-ao(m-1))
                      factor1=dao(m)*(ao(m+1)-ao(m))
                      DDm=0.5*(DD(m)+DD(m-1))
                      DDp=0.5*(DD(m)+DD(m+1))
                      um(m)=dt*DDm/factor_1/Gjac(m)
                      up(m)=dt*DDp/factor1/Gjac(m)
                      if ( factor_1 .eq. 0 .or. &
                           factor1 .eq. 0 .or. &
                           Gjac(m) .eq. 0 ) then
                         write(*,*) 'WARNING: CIMI: Diffuseaa: '//&
                              'Null in denominator!'
                         ! if factor1 or factor_1 eq 0 check fied
                         ! tracing and Bo and sinA grid
                      endif
                      ump_mx=max(abs(up(m)),abs(um(m)))
                      if (ump_mx.gt.u_mx) u_mx=ump_mx
                   enddo
                   
                   ! reduce time step if u_mx > u_max
                   irun=ifix(u_mx/u_max)+1
                   do m=1,ik
                      um(m)=um(m)/irun
                      up(m)=up(m)/irun
                   enddo
                   u_mx=u_mx/irun     ! new u_mx < u_max
                   
                   ! determine the implicitness, xlam
                   if (u_mx.le.1.) then
                      xlam=0.5              ! Crank-Nicolson
                   else
                      u_mx_log=log10(u_mx)
                      xlam=0.5*u_mx_log/u_max_log+0.5
                   endif
                   alam=1.-xlam
                   
                   ! calculate a1d, b1d, c1d
                   do m=1,ik
                      a1d(m)=-xlam*um(m)
                      b1d(m)=1.+xlam*(um(m)+up(m))
                      c1d(m)=-xlam*up(m)
                   enddo
                   
                   ! start diffusion in ao
                   f0(1:ik)=f2d(k,1:ik)
                   do nn=1,irun
                      f0(0)=f0(1)
                      f0(ik+1)=f0(ik)
                      fr(1)=alam*um(1)*f0(0)+(1.-alam*(up(1)+um(1)))*f0(1)+ &
                           alam*up(1)*f0(2)+xlam*um(1)*f0(0)
                      do m=2,ik-1    ! calculate the RHS of matrix equation
                         fr(m)=alam*um(m)*f0(m-1)+(1.-alam*(up(m)+um(m)))*&
                              f0(m)+alam*up(m)*f0(m+1)
                      enddo
                      fr(ik)=alam*um(ik)*f0(ik-1)+&
                           (1.-alam*(up(ik)+um(ik)))*f0(ik) &
                           +alam*up(ik)*f0(ik+1)+xlam*up(ik)*f0(ik+1)
                      call tridagIM(a1d,b1d,c1d,fr,f0(1:ik),ik,ier)
                      if (ier.eq.1) &
                           call CON_STOP('ERROR in CIMI: Diffusionaa: '//&
                           'tridag failed,ier=1')
                      if (ier.eq.2) &
                           call CON_STOP('ERROR in CIMI: Diffusionaa: '//&
                           'tridag failed,ier=2')
                   enddo
                   
                   do m=1,ik
                      df(k,m)=f0(m)-f2d(k,m)    ! df is differential psd
                   enddo
                   
                endif
             enddo         ! end of do k=1,ie      ! *****
             
             ! map psd back to M grid
             tchar='difaa2'
             do m=1,ik
                do k=1,ie
                   df1(k)=df(k,m)
                enddo
                do k=1,iw2(n,m)
                   call lintpIM_diff(ein_log,df1,ie,ekevlog(k,m),dpsd,tchar)
                   f2(n,i,j,k,m)=f2(n,i,j,k,m)+xjac(n,i,k)*dpsd
                   if (f2(n,i,j,k,m).lt.0.) f2(n,i,j,k,m)=0.
                enddo
             enddo
          endif    ! end of if (ompe1.ge.cOmpe(1).and.ompe1.le.cOmpe(ipc).and...
          
       enddo
    enddo         ! end of j loop
    
  end subroutine diffuse_aa

!****************************************************************************
!                            diffuse_EE
!  Routine calculates the change of electron distributions due to
!  diffusion in E.
!****************************************************************************
  subroutine diffuse_EE(f2,dt,xmm,xjac,iw2,iba)
    
    use ModPlasmasphere, ONLY:PlasDensity_C,PlasmaPauseDensity
    use ModMPI
    use ModCimiGrid,   ONLY: MinLonPar,MaxLonPar
    use ModCimiUtil,   ONLY: lintp3IM,tridagIM
    
    !  use constants
    !  use cimigrid_dim CHECK je and ig
    implicit none
    integer i,j,k,m,k1,k2,iww,irun,nrun,ier,n
    real xjac(nspec,ir,iw),xmm(nspec,0:iw+1)  ! xjac and xmm have different dimension from CIMI !
    integer iba(ip),iw2(nspec,ik)
    real dt,Eo,f2(nspec,ir,ip,iw,ik),fl(iw), &
         xlam,alam,um(iw),up(iw),ompe1,ro1,PA1, &
         fr(iw),a1d(iw),b1d(iw),c1d(iw),&
         factor_1, &
         Hfactor,Upower0, &
         factor1,gjac_1,gjac1,gjac(iw),E1(0:iw),Ep,DEE(0:iw+1), &
         DDm,DDp,DEEc,DEEu,DEEh,u_mx,ump_mx,Enor(iw),dEn(iw), & ! CHECKED
         WM1,Wo,Cfactor,Ufactor,edmin,edmax, & ! CHECKED
         ckeV_log(iwc),hkeV_log(iwh),Ep_log ! CHECKED
    !,ukeV_log(iwu)
    Eo=511.               ! electron rest energy in keV
    xlam=0.5              ! implicitness in solving diffusion equation
    alam=1.-xlam
    Upower0=10000.        ! coeff based on UB chorus power of (100pT)^2
    
    ckeV_log(:)=log10(ckeV(:))
    !ukeV_log(:)=log10(ukeV(:))
    hkeV_log(:)=log10(hkeV(:))
    
    ! determine edmax and edmin
    !if (iUBC.eq.1) edmax=ukeV(iwu)
    if (UseHiss) edmax=hkeV(iwh)
    if (UseChorus) edmax=ckeV(iwc)
    if (UseHiss) edmin=hkeV(1)
    if (UseChorus) edmin=ckeV(1)
    !if (iUBC.eq.1) edmin=ukeV(1)
    
    n=nspec  ! use last index to access electrons; different from CIMI
    
    do j=MinLonPar,MaxLonPar
       do i=1,iba(j)
          ro1=ro(i,j)
          Cfactor=(BLc0/bo(i,j))*CHpower(i,j)/Cpower0
          Ufactor=0.0!(BLu0/bo(i,j))*UBCpower(i,j)/Upower0
          Hfactor=(BLh0/bo(i,j))*HIpower(i,j)/Hpower0
          ompe1=ompe(i,j)                         ! fpe/fce
          
          if (testDiff_EE) ompe1=cOmpe(1)
          
          if (ompe1.ge.cOmpe(1).and.ompe1.le.cOmpe(ipc).and.ro1.ge.r_wave) then
             do m=1,ik
                PA1=asin(y(i,j,m))*180./pi        ! pitch angle in degree
                if (PA1.lt.cPA(1)) PA1=cPA(1)
                if (PA1.gt.cPA(ipa)) PA1=cPA(ipa)
                do k=1,iw
                   Enor(k)=ekev(n,i,j,k,m)/Eo
                   gjac(k)=(Enor(k)+1.)*sqrt(Enor(k)*(Enor(k)+2.))
                enddo
                
                ! determine k1 and k2, corresponding to edmin and edmax
                k1=iw2(n,m)
                findk1: do k=2,iw2(n,m)
                   if (edmin.le.ekev(n,i,j,k,m)) then
                      k1=k
                      if (k1.eq.1) k1=2
                      exit findk1
                   endif
                enddo findk1

                k2=1
                findk2: do k=iw2(n,m)-1,1,-1
                   if (edmax.ge.ekev(n,i,j,k,m)) then
                      k2=k
                      exit findk2
                   endif
                enddo findk2
                iww=k2-k1+1
                
                !    if (i.eq.30 .and. j.eq.5) write(*,*) 'm',m,'energy range in D_ee',ekev(n,i,j,k1,m),ekev(n,i,j,k2,m),k1,k2
                
                ! find DEE/E^2 and dEn
                
                ! find DEE/E^2 and dEn
                Wo=Eo*1.6e-16/bm(i,j,m)   ! normalization factor of mag moment
                do k=k1-1,k2
                   WM1=sqrt(xmm(n,k)*xmm(n,k+1))
                   E1(k)=sqrt(2.*WM1/Wo+1.)-1.    ! normalized kinetic energy
                   Ep=E1(k)*Eo                    ! Ep in keV
                   Ep_log=log10(Ep)
                   if (k.ge.k1) dEn(k)=E1(k)-E1(k-1)
                   DEEc=0.
                   if (UseChorus.and.PlasDensity_C(i,j).le.PlasmaPauseDensity) then
                      if (Ep.ge.ckeV(1).and.Ep.le.ckeV(iwc)) call lintp3IM(cOmpe, &
                           ckeV_log,cPA,cDEE,ipc,iwc,ipa,ompe1,Ep_log,PA1,DEEc)
                   endif
                   DEEu=0.
                   !if (iUBC.eq.1.and.PlasDensity_Ci,j).le.PlasmaPauseDensity) then
                   !   if (Ep.ge.ukeV(1).and.Ep.le.ukeV(iwu)) call lintp3IM(uOmpe, &
                   !        ukeV_log,cPA,uDEE,ipu,iwu,ipa,ompe1,Ep_log,PA1,DEEu)
                   !endif
                   DEEh=0.
                   if(UseHiss.and.ompe1.ge.2..and.PlasDensity_C(i,j).gt.PlasmaPauseDensity)then
                      if (Ep.ge.hkeV(1).and.Ep.le.hkeV(iwh)) call lintp3IM(hOmpe, &
                           hkeV_log,cPA,hDEE,iph,iwh,ipa,ompe1,Ep_log,PA1,DEEh)
                   endif
                   DEE(k)=DEEc*Cfactor+DEEu*Ufactor+DEEh*Hfactor ! this is DEE/E^2
                   
                   if (testDiff_EE) then 
                      if ( ro(i,j).ge.3.0 .and. ro(i,j).le.5.0 ) then
                         ! artificially force diffusion time to be
                         ! 1/DEE sec; over 1/DEE the psd should evolve
                         ! at energy range E0~0.5MeV; this is how the
                         ! current setup is normalized.
                         DEE(k)=0.01   
                      else
                         DEE(k)=0.
                      endif
                   endif
                   
                enddo
                
                ! find the Courant number
                u_mx=0.
                do k=k1,k2
                   factor_1=dEn(k)*(Enor(k)-Enor(k-1))   ! normalized factor
                   factor1=dEn(k)*(Enor(k+1)-Enor(k))    !
                   gjac_1=(E1(k-1)+1.)*sqrt(E1(k-1)*(E1(k-1)+2.))
                   gjac1=(E1(k)+1.)*sqrt(E1(k)*(E1(k)+2.))
                   DDm=DEE(k-1)*E1(k-1)*E1(k-1)*gjac_1
                   DDp=DEE(k)*E1(k)*E1(k)*gjac1
                   if ( (factor_1.eq.0) .or. (factor1.eq.0) .or. (gjac_1.eq.0) &
                        .or. (gjac1.eq.0)) then
                      write(*,*) 'WARNING in CIMI: diffuse_EE:um,up:'//&
                           'Null in denominator!!!'
                   endif
                   um(k)=dt*DDm/factor_1/gjac(k)
                   up(k)=dt*DDp/factor1/gjac(k)
                   ump_mx=max(abs(up(k)),abs(um(k)))
                   if (ump_mx.gt.u_mx) u_mx=ump_mx
                enddo
                
                ! reduce time step size if up or um is too large
                irun=ifix(u_mx)+1
                do k=k1,k2
                   um(k)=um(k)/irun
                   up(k)=up(k)/irun
                   a1d(k)=-xlam*um(k)
                   b1d(k)=1.+xlam*(um(k)+up(k))
                   c1d(k)=-xlam*up(k)
                enddo
                ! Start diffusion in E
                do k=k1-1,k2+1
                   fl(k)=f2(n,i,j,k,m)/xjac(n,i,k)   ! fl is psd
                   if (xjac(n,i,k).eq.0) &
                        write(*,*) 'WARNING in CIMI: diffuse_EE: '//&
                        'xjac:Null in denominator!!!'  
                enddo
                ! force flat psd across the boundaries; this is done
                ! to force zero flux across the boundaries and close
                ! boundaries
                fl(k1-1)=fl(k1+1)
                fl(k1)=fl(k1+1)
                fl(k2+1)=fl(k2-1)
                fl(k2)=fl(k2-1)       
                
                do nrun=1,irun
                   fr(k1)=alam*um(k1)*fl(k1-1)+(1.-alam*(up(k1)+um(k1)))*&
                        fl(k1)+alam*up(k1)*fl(k1+1)+xlam*um(k1)*fl(k1-1)
                   do k=k1+1,k2-1    ! calculate the RHS of matrix equation
                      fr(k)=alam*um(k)*fl(k-1)+(1.-alam*(up(k)+um(k)))*&
                           fl(k)+alam*up(k)*fl(k+1)
                   enddo
                   fr(k2)=alam*um(k2)*fl(k2-1)+(1.-alam*(up(k2)+um(k2)))*&
                        fl(k2)+alam*up(k2)*fl(k2+1)+xlam*up(k2)*fl(k2+1)
                   !  if (i.eq.30 .and. j.eq.5) write(*,*) 'nrun',nrun,'fr1',fr(k1),fr(k1+1),'fr2',fr(k2-1),fr(k2)           
                   !  if (i.eq.30 .and. j.eq.5) write(*,*) 'nrun',nrun,'fl1',fl(k1),fl(k1+1),'fl2',fl(k2-1),fl(k2)
                   !  if (i.eq.30 .and. j.eq.5) write(*,*) 'nrun',nrun,'f2_right',f2(n,i,j,k1,m),&
                   !  f2(n,i,j,k1+1,m),'f2_left',f2(n,i,j,k2-1,m),f2(n,i,j,k2,m)
                   
                   call tridagIM(a1d(k1),b1d(k1),c1d(k1),fr(k1),fl(k1),iww,ier)
                   if (ier.eq.1) call CON_STOP('ERROR in CIMI: diffuse_EE:tridag failed,ier=1')
                   if (ier.eq.2) call CON_STOP('ERROR in CIMI: diffuse_EE:tridag failed,ier=2')
                enddo
                
                ! get back f2
                ! if (i.eq.30 .and. j.eq.5 .and. m.eq.5) write(*,*) 'psd, ', fl(k1:k2)
                do k=k1+1,k2-1 ! change made by NB to avoid two 'ghost cells'. Originally was do k=k1,k2
                   ! do k=k1,k2
                   if (fl(k).lt.0.) fl(k)=0.
                   f2(n,i,j,k,m)=fl(k)*xjac(n,i,k)
                enddo
                
             enddo       ! end of m=1,ik
          endif          ! end of if (ompe1.ge.cOmpe(1).and.ompe1.le.cOmpe(ipc)..)
          
       enddo       ! end of i loop
    enddo         ! end of j loop

  end subroutine diffuse_EE
 

!****************************************************************************
! diffuse_aE Routine solves cross diffusion in ao and E.
! ****************************************************************************
! The version from April 19, 2016 starts to be unstable after ~15-20
! time steps with dt=5 sec and constant Dae=0.05 Dae=0.05 is
! considered to be large in comparison with realistic values.
! Therefore, it is expected that in realistic simulations the
! accumulative error and instability will be washed away with other
! dominant processes like convection and precipitation.
  subroutine diffuse_aE(f2,dt,xjac,iw2,iba,time)

    use ModPlasmasphere, ONLY:PlasDensity_C,PlasmaPauseDensity
    use ModMPI
    use ModCimiGrid,   ONLY: MinLonPar,MaxLonPar
    use ModCimiUtil,   ONLY: lintp3IM,tridagIM
    
    implicit none
    integer,parameter :: ie=80
    integer i,j,m,k,k1,k2,nrun,n,ier,iww,nn
    integer iba(ip),iw2(nspec,ik),nrun2
    real f2(nspec,ir,ip,iw,ik),xjac(nspec,ir,iw),einlog, &
         ein_log(ie),&
         fl(0:ie+1),f1d(iw),ein(ie),ao(0:ik+1),dao(ik),ao1, &
         a1d(ie),b1d(ie),c1d(ie),e1d(iw),u_mx,u_mx1,u_mx2,dpsd,dt, &
         f2d0(ie,ik), &
         f2d(ie,0:ik+1), &
         df1(ie),fr(ie),ekevlog(iw,ik),Upower0, &
         Eo,Enor(ie),edmin,edmax, &
         gjacA(0:ik+1),Dae(ie,0:ik+1),Cfactor,Ufactor,Hfactor,DaEc,DaEu,DaEh,&
         u1(ie,ik),u2(ie,ik),u3(ie,ik),dtda2,dtda2dE2,Dkm,Dk_1m,Dkm_1,Dkm1, &
         Dk1m,gjacE(ie),ompe1,ro1,emin,emax,x0,x2,x, &
         ckeV_log(iwc),hkeV_log(iwh), &
         f2d1(ie,0:ik+1),f2d2(ie,0:ik+1),time
    !,ukeV_log(iwu)
    character(len=6) :: tchar
    
    Eo=511.               ! electron rest energy in keV
    Upower0=10000.        ! coeff based on UB chorus power of (100pT)^2
    
    ckeV_log(:)=log10(ckeV(:))
    !ukeV_log(:)=log10(ukeV(:))
    hkeV_log(:)=log10(hkeV(:))
    
    ! determine edmax and edmin
    !if (iUBC.eq.1) edmax=ukeV(iwu)
    if (UseHiss) edmax=hkeV(iwh)
    if (UseChorus) edmax=ckeV(iwc)
    if (UseHiss) edmin=hkeV(1)
    if (UseChorus) edmin=ckeV(1)
    !if (iUBC.eq.1) edmin=ukeV(1)
    
    ! check whether ie < ik
    if (ie.lt.ik) then
       write(*,*) 'CIMI Error, Diffuse_aE: ie.lt.ik'
       stop
    endif
    
    n=nspec  ! use last index to access electrons; different from CIMI
    !   do j=1,ip
    do j=MinLonPar,MaxLonPar
       do i=1,iba(j)
          ompe1=ompe(i,j)                         ! fpe/fce
          ro1=ro(i,j)
          Cfactor=(BLc0/bo(i,j))*CHpower(i,j)/Cpower0
          Ufactor=0.0!(BLu0/bo(i,j))*UBCpower(i,j)/Upower0
          Hfactor=(BLh0/bo(i,j))*HIpower(i,j)/Hpower0
          if (testDiff_aE) ompe1=cOmpe(1)
          if (ompe1.ge.cOmpe(1).and.ompe1.le.cOmpe(ipc).and.ro1.ge.r_wave) then
             ! Set up the energy grid, ein
             emin=minval(ekev(n,i,j,1,1:ik))
             emax=maxval(ekev(n,i,j,iw,1:ik))
             
             ein(1)=emin
             ein(ie)=emax
             ein_log(1)=log10(emin)
             ein_log(ie)=log10(emax)
             x0=log10(emin)
             x2=log10(emax)
             x=(x2-x0)/(ie-1)
             do k=1,ie
                if (k.gt.1.and.k.lt.ie) ein_log(k)=x0+(k-1)*x
                if (k.gt.1.and.k.lt.ie) ein(k)=10.**ein_log(k)
                Enor(k)=ein(k)/Eo             ! normalized kinetic energies
                gjacE(k)=(Enor(k)+1.)*sqrt(Enor(k)*(Enor(k)+2.))
             enddo
             ! Map psd to ein grid, f2d
             f2d(:,:)=0.
             do m=1,ik
                do k=1,iw
                   f1d(k)=-50.                      ! f1d is log(psd)
                   if(f2(n,i,j,k,m).gt.0.)f1d(k)=log10(f2(n,i,j,k,m)/xjac(n,i,k))
                   if (k.gt.iw2(n,m)) f1d(k)=f1d(iw2(n,m))
                   ekevlog(k,m)=log10(ekev(n,i,j,k,m))
                   e1d(k)=ekevlog(k,m)              ! e1d is log(ekev)
                enddo
                
                tchar='difae1'
                do k=1,ie
                   einlog=ein_log(k)
                   if (einlog.le.e1d(1)) einlog=e1d(1)
                   if (einlog.ge.e1d(iw)) einlog=e1d(iw)
                   call lintpIM_diff(e1d,f1d,iw,einlog,x,tchar)
                   f2d(k,m)=10.**x      ! f2d is psd
                enddo
             enddo
             f2d(1:ie,0)=f2d(1:ie,1)
             f2d(1:ie,ik+1)=f2d(1:ie,ik)
             f2d0(1:ie,1:ik)=f2d(1:ie,1:ik)
             
             ! calcuate ao, dao and gjacA
             
             ! calcuate ao, dao and gjacA
             do m=0,ik+1
                ao(m)=asin(y(i,j,m))
                gjacA(m)=tya(i,j,m)*y(i,j,m)*sqrt(1.-y(i,j,m)*y(i,j,m))
             enddo
             do m=1,ik
                dao(m)=0.5*(ao(m+1)-ao(m-1))
             enddo
             
             ! determine k1 and k2, corresponding to edmin and edmax
             k1=ie
             findk1: do k=1,ie
                if (edmin.le.ein(k)) then
                   k1=k
                   exit findk1
                endif
             enddo findk1
             k2=1
             findk2: do k=ie,1,-1
                if (edmax.ge.ein(k)) then
                   k2=k
                   exit findk2
                endif
             enddo findk2
             !    k1=max(k1,2)
             !    k2=min(k2,ie-1)
             k1=2
             k2=ie-1
             iww=k2-k1+1
             
             ! Find Dae
             do m=0,ik+1
                ao1=ao(m)*180./pi
                if (ao1.lt.cPA(1)) ao1=cPA(1)
                if (ao1.gt.cPA(ipa)) ao1=cPA(ipa)
                do k=k1-1,k2+1
                   DaEc=0.
                   if (UseChorus.and.PlasDensity_C(i,j).le.PlasmaPauseDensity) then
                      if (ein(k).ge.ckeV(1).and.ein(k).le.ckeV(iwc)) &
                           call lintp3IM(cOmpe,ckeV_log,cPA,cDaE,ipc,iwc,ipa,&
                           ompe1,ein_log(k),ao1,DaEc)
                   endif
                   DaEu=0.
                   !if (iUBC.eq.1.and.PlasDensity_Ci,j).le.PlasmaPauseDensity) then
                   !   if (ein(k).ge.ukeV(1).and.ein(k).le.ukeV(iwu)) &
                   !        call lintp3IM(uOmpe,ukeV_log,cPA,uDaE,ipu,iwu,ipa,&
                   !        ompe1,ein_log(k),ao1,DaEu)
                   !endif
                   DaEh=0.
                   if(UseHiss.and.ompe1.ge.2..and.PlasDensity_C(i,j).gt.PlasmaPauseDensity)then
                      if (ein(k).ge.hkeV(1).and.ein(k).le.hkeV(iwh)) &
                           call lintp3IM(hOmpe,hkeV_log,cPA,hDaE,iph,iwh,ipa, &
                           ompe1,ein_log(k),ao1,DaEh)
                   endif
                   Dae(k,m)=DaEc*Cfactor+DaEu*Ufactor+DaEh*Hfactor
                enddo
             enddo
             if (testDiff_aE) then
                if ( ro(i,j).ge.3.0 .and. ro(i,j).le.5.0 ) then
                   Dae(:,:)=0.005   ! artificially force diffusion time to be 1/DaE sec;
                   !  Dae(:,:)=0.0
                   ! over 1/DaE the psd should evolve at energy range
                   ! E0~0.5MeV; this is how the current setup is
                   ! normalized.
                else
                   Dae(:,:)=0.
                endif
             endif
             ! Find u1, u2, and u3
             
             ! Find u1, u2, and u3
             u1=0.
             u2=0.
             u3=0.
             do m=1,ik
                dtda2=dt/(ao(m+1)-ao(m-1))
                do k=k1,k2
                   Dkm=Enor(k)*Dae(k,m)
                   Dk_1m=Enor(k-1)*Dae(k-1,m)
                   Dk1m=Enor(k+1)*Dae(k+1,m)
                   Dkm_1=Enor(k)*Dae(k,m-1)
                   Dkm1=Enor(k)*Dae(k,m+1)
                   dtda2dE2=dtda2/(Enor(k+1)-Enor(k-1))
                   u1(k,m)=(gjacA(m+1)*Dkm1-gjacA(m-1)*Dkm_1)*dtda2dE2/gjacA(m)/2.
                   if (gjacA(m).eq.0) &
                        write(*,*) 'WARNING in CIMI: diffuse_aE: gjacA:Null in denominator!!!'
                   u2(k,m)=(gjacE(k+1)*Dk1m-gjacE(k-1)*Dk_1m)*dtda2dE2/gjacE(k)/2.
                   if (gjacE(k).eq.0) &
                        write(*,*) 'WARNING in CIMI: diffuse_aE: gjacE:Null in denominator!!!'
                   u3(k,m)=Dkm*dtda2dE2
                enddo
                u1(1,m)=u1(2,m)
                u1(ie,m)=u1(ie-1,m)
                u2(1,m)=u2(2,m)
                u2(ie,m)=u2(ie-1,m)
                u3(1,m)=u3(2,m)
                u3(ie,m)=u3(ie-1,m)
             enddo
             ! reduce time step if u_mx is too large
             u_mx1=maxval(u1)
             u_mx2=maxval(u2)
             u_mx=max(u_mx1,u_mx2)
             nrun=ifix(u_mx)+1
             
             
             u1=u1/nrun
             u2=u2/nrun
             u3=u3/nrun
             
             NN_LOOP: do nn=1,nrun
                ! First step of ADI: "diffusion in E"
                f2d1(:,:)=f2d(:,:)
                f2d2(:,:)=f2d(:,:)
                
                f2d1(k1-1,0:ik+1)=f2d1(k1,0:ik+1)   ! 'closed' boundary
                f2d1(k2+1,0:ik+1)=f2d1(k2,0:ik+1)                
                
                ADI_step1: do m=1,ik
                   do k=1,ie
                      a1d(k)=u1(k,m)
                      b1d(k)=1.
                      c1d(k)=-u1(k,m)
                   enddo
                   ! solving the tridiagonal matrix in E
                   do k=k1-1,k2+1
                      fl(k)=f2d1(k,m)
                   enddo
                   do k=k1,k2   ! fr: RHS of matrix equation
                      fr(k)=f2d1(k,m)+u2(k,m)*(f2d1(k,m+1)-f2d1(k,m-1))+u3(k,m)* &
                           (f2d1(k+1,m+1)+f2d1(k-1,m-1)-f2d1(k+1,m-1)-f2d1(k-1,m+1))
                   enddo
                   fr(k1)=fr(k1)-u1(k1,m)*fl(k1-1)   ! boundary condition
                   fr(k2)=fr(k2)+u1(k2,m)*fl(k2+1)   !
                   !      fr(k1)=fl(k1)-u1(k1,m)*fl(k1-1) ! boundary conditions NB - var2
                   !      fr(k2)=fl(k2)+u1(k2,m)*fl(k2+1) ! boundary conditions NB - var2
                   call tridagIM(a1d(k1:k2),b1d(k1:k2),c1d(k1:k2),fr(k1:k2), &
                        fl(k1:k2),iww,ier)
                   if (ier.eq.1) call CON_STOP('ERROR in CIMI: diffuse_aE,diff E:tridag failed,ier=1')
                   if (ier.eq.2) call CON_STOP('ERROR in CIMI: diffuse_aE,diff E:tridag failed,ier=2')
                   f2d2(k1:k2,m)=fl(k1:k2)
                enddo ADI_step1
                
                f2d2(1:ie,0)=f2d2(1:ie,1)
                f2d2(1:ie,ik+1)=f2d2(1:ie,ik)
                
                !    if (i.eq.30 .and. j.eq.5) write(*,*) nn, 't=t0 m=1:', f2d0(1:ie,1)
                !    if (i.eq.30 .and. j.eq.5) write(*,*) nn, 't=t0+dt m=1:', f2d(1:ie,1)
                !    if (i.eq.30 .and. j.eq.5) write(*,*) nn, 't=t0 m=ik:', f2d0(1:ie,ik)
                !    if (i.eq.30 .and. j.eq.5) write(*,*) nn, 't=t0+dt m=ik:', f2d(1:ie,ik)  
                
                ! Second step of ADI: "diffusion in a"
                ADI_step2: do k=k1,k2
                   do m=1,ik
                      a1d(m)=u2(k,m)
                      b1d(m)=1.
                      c1d(m)=-u2(k,m)
                   enddo
                   ! solving the tridiagonal matrix in a
                   fl(0:ik+1)=f2d2(k,0:ik+1)
                   do m=1,ik         ! fr: RHS of matrix equation
                      fr(m)=f2d2(k,m)+u1(k,m)*(f2d2(k+1,m)-f2d2(k-1,m))+u3(k,m)* &
                           (f2d2(k+1,m+1)+f2d2(k-1,m-1)-f2d2(k+1,m-1)-f2d2(k-1,m+1))
                   enddo
                   fr(1)=fr(1)-u2(k,1)*fl(0)         ! boundary condition
                   fr(ik)=fr(ik)+u2(k,ik)*fl(ik+1)   !
                   !              fr(1)=fl(1)-u2(k,1)*fl(0)         ! boundary conditions NB - var2
                   !              fr(ik)=fl(ik)+u2(k,ik)*fl(ik+1)   ! boundary conditions NB - var2
                   
                   call tridagIM(a1d,b1d,c1d,fr,fl(1:ik),ik,ier)
                   if (ier.eq.1) call CON_STOP('ERROR in CIMI: diffuse_aE,diff a:tridag failed,ier=1')
                   if (ier.eq.2) call CON_STOP('ERROR in CIMI: diffuse_aE,diff a:tridag failed,ier=2')
                   f2d(k,1:ik)=fl(1:ik)
                enddo ADI_step2
                f2d(1:ie,0)=f2d(1:ie,1)
                f2d(1:ie,ik+1)=f2d(1:ie,ik)
                ! third step
                !                           f2d_temp(:,:)=f2d(:,:)
                !             f2d_temp(k1-1:k2+1,0)=f2d_temp(k1-1:k2+1,1)
                !              f2d_temp(k1-1:k2+1,ik+1)=f2d_temp(k1-1:k2+1,ik)
                !
                !
                !                            ADI_step3: do k=k1,k2
                !                 do m=1,ik
                !                    a1d(m)=u2(k,m)
                !                    b1d(m)=1.
                !                    c1d(m)=-u2(k,m)
                !                 enddo
                !                 ! solving the tridiagonal matrix in a
                !                 fl(1:ik)=f2d(k,1:ik)
                !                 fl(0)=fl(1)          ! boundary condition
                !                 fl(ik+1)=fl(ik)      !
                !                 do m=1,ik         ! fr: RHS of matrix equation
                !                    fr(m)=f2d_temp(k,m)+u1(k,m)*(f2d_temp(k+1,m)-f2d_temp(k-1,m))+u3(k,m)*&
                !                          (f2d_temp(k+1,m+1)+f2d_temp(k-1,m-1)-f2d_temp(k+1,m-1)-f2d_temp(k-1,m+1))
                !                 enddo
                !
                !                  fr(1)=fl(1)-u2(k,1)*fl(0)         ! boundary condition
                !                  fr(ik)=fl(ik)+u2(k,ik)*fl(ik+1)
                !
                !
                !                 call tridagIM(a1d,b1d,c1d,fr,fl(1:ik),ik,ier)
                !                 if (ier.eq.1) call CON_STOP('ERROR in CIMI: diffuse_aE,diff a:tridag failed,ier=1')
                !                 if (ier.eq.2) call CON_STOP('ERROR in CIMI: diffuse_aE,diffa:tridag failed,ier=2')
                !                 f2d(k,1:ik)=fl(1:ik)
                !              enddo ADI_step3
                !              f2d(1:ie,0)=f2d(1:ie,1)
!!!              f2d(1:ie,ik+1)=f2d(1:ie,ik)
                !
                !              f2d_temp(:,:)=f2d(:,:)
                !              f2d_temp(k1-1:k2+1,0)=f2d_temp(k1-1:k2+1,1)
                !              f2d_temp(k1-1:k2+1,ik+1)=f2d_temp(k1-1:k2+1,ik)
                
                ! forth step:          
                !                           ! forth step of ADI: "diffusion in E"
                !              ADI_step4: do m=1,ik
                !                 do k=1,ie
                !!                   a1d(k)=u1(k,m)
                !                    b1d(k)=1.
                !                    c1d(k)=-u1(k,m)
                !                 enddo
                !                 ! solving the tridiagonal matrix in E
!!!                 do k=k1-1,k2+1
                !!                    fl(k)=f2d(k,m)
                !                 enddo
                !                   fl(k1-1)=fl(k1) ! by NB to close bondaries
                !                   fl(k2+1)=fl(k2)
                !                 do k=k1,k2   ! fr: RHS of matrix equation
                !                    fr(k)=f2d_temp(k,m)+u2(k,m)*(f2d_temp(k,m+1)-f2d_temp(k,m-1))+u3(k,m)*&
                !                          (f2d_temp(k+1,m+1)+f2d_temp(k-1,m-1)-f2d_temp(k+1,m-1)-f2d_temp(k-1,m+1))
                !                 enddo
                !                 fr(k1)=fr(k1)-u1(k1,m)*fl(k1-1)   ! boundary condition
                !                 fr(k2)=fr(k2)+u1(k2,m)*fl(k2+1)   !
                !              !   fr(k1)=fl(k1)-u1(k1,m)*fl(k1-1)    !
                !              !   fr(k2)=fl(k2)+u1(k2,m)*fl(k2+1)    !
                !  
                !                 call tridagIM(a1d(k1:k2),b1d(k1:k2),c1d(k1:k2),fr(k1:k2), &
                !                             fl(k1:k2),iww,ier)
                !                 if (ier.eq.1) call CON_STOP('ERROR in CIMI: diffuse_aE,diff E:tridag failed,ier=1')
                !                 if (ier.eq.2) call CON_STOP('ERROR in CIMI: diffuse_aE,diff E:tridag failed,ier=2')
                !                 f2d(k1:k2,m)=fl(k1:k2)
                !              enddo ADI_step4
                !              f2d(1:ie,0)=f2d(1:ie,1)
                !              f2d(1:ie,ik+1)=f2d(1:ie,ik)
                
             enddo NN_LOOP                  ! end of do nn=1,nrun
             
             ! if (i.eq.30 .and. j.eq.5) write(*,*) 'time',time,'t=t0 m=1:', f2d0(1:ie,1)
             ! if (i.eq.30 .and. j.eq.5) write(*,*) 'time',time,'t=t0+dt m=1:', f2d(1:ie,1)
             ! if (i.eq.30 .and. j.eq.5) write(*,*) 'time',time,'t=t0 m=ik:', f2d0(1:ie,ik)
             ! if (i.eq.30 .and. j.eq.5) write(*,*) 'time',time,'t=t0+dt m=ik:', f2d(1:ie,ik)           
             
             ! map psd back to M grid
             tchar='difae2'
             do m=1,ik
                do k=1,ie
                   df1(k)=f2d(k,m)-f2d0(k,m)
                enddo
                do k=1,iw2(n,m)
                   call lintpIM_diff(ein_log,df1,ie,ekevlog(k,m),dpsd,tchar)
                   !if (i.eq.30 .and. j.eq.5 .and. m.eq.ik) write(*,*) m,k,' m,k;',df1(k),dpsd,'df1,dpsd'
                   f2(n,i,j,k,m)=f2(n,i,j,k,m)+xjac(n,i,k)*dpsd
                   if (f2(n,i,j,k,m).lt.0.) f2(n,i,j,k,m)=0.
                enddo
             enddo
             
          endif  ! end if (ompe1.ge.cOmpe(1).and.ompe1.le.cOmpe(ipc).and.ro1..)
          
       enddo     ! end of i loop
    enddo       ! end of j loop
  end subroutine diffuse_aE
  
!============================================================================
  subroutine lintpIM_diff(xx,yy,n,x,y,tchar)
!-----------------------------------------------------------------------
!  Routine does 1-D interpolation.  xx must be increasing or
!  decreasing monotonically.  x is between xx(j) and xx(j+1)

    integer :: n
    real xx(n),yy(n)
    integer :: ier = 0, i, j, jl, ju, jm
    real    :: x, d, y, eps_low,eps_high
    character(len=6) :: tchar
    !  Make sure xx is increasing or decreasing monotonically
    do i=2,n
       if (xx(n).gt.xx(1).and.xx(i).lt.xx(i-1)) then
          write(*,*) ' lintpIM_diff: xx is not increasing monotonically '
          write(*,*) n,(xx(j),j=1,n)
          write(*,*) 'in:  ',tchar
          call CON_stop('IM ERROR: lintpIM_diff')
       endif
       if (xx(n).lt.xx(1).and.xx(i).gt.xx(i-1)) then
          write(*,*) ' lintpIM_diff: xx is not decreasing monotonically '
          write(*,*) n,(xx(j),j=1,n)
          write(*,*) 'in:  ',tchar
          call CON_stop('IM ERROR: lintpIM_diff')
       endif
    enddo
    
    eps_low=abs(x-xx(1))/abs(xx(1))
    eps_high=abs(x-xx(n))/abs(xx(n))
    If ((eps_low.gt.1.e-5) .and. (eps_high.gt.1.e-5)) Then  
       !  Set ier=1 if out of range
       if (xx(n).gt.xx(1)) then
          if ((x.lt.xx(1)).or.(x.gt.xx(n))) ier=1
       else
          if ((x.gt.xx(1)).or.(x.lt.xx(n))) ier=1
       endif
       if (ier.eq.1) then 
          write(*,*) ' Error: ier.eq.1'
          write(*,*) ' x  ',x
          write(*,*) ' xx  ',xx
          write(*,*) 'in:  ',tchar
          write(*,*) (x.lt.xx(1)),(x.gt.xx(n)),n
          write(*,*) abs(x-xx(1)),eps_low,eps_high
          call CON_stop('IM ERROR: lintpIM_diff')
       endif
       !
       
       
       !    initialize lower and upper values
       !
       jl=1
       ju=n
       !
       !    if not dne compute a midpoint
       
       !
10     if(ju-jl.gt.1)then
          jm=(ju+jl)/2
          !
          !    now replace lower or upper limit
          !
          if((xx(n).gt.xx(1)).eqv.(x.gt.xx(jm)))then
             jl=jm
          else
             ju=jm
          endif
          !
          !    try again
          !
          go to 10
       endif
       !
       !    this is j
       !
       j=jl      ! if x.le.xx(1) then j=1
       !                 if x.gt.x(j).and.x.le.x(j+1) then j=j
       !                 if x.gt.x(n) then j=n-1
       d=xx(j+1)-xx(j)
       y=(yy(j)*(xx(j+1)-x)+yy(j+1)*(x-xx(j)))/d
       
       !   if (x.eq.xx(1) .or. x.eq.xx(n)) then
       !     write(*,*) 'warning:diffusion:lintp:x=x(1) or x=x(n)'
       !     write(*,*) j,x,xx(j),xx(j+1),yy(j),yy(j+1),y
       !     write(*,*) 'in: ',tchar
       !   endif
    Else 
       if (eps_low.le.1.e-5) y=yy(1)
       if (eps_high.le.1.e-5) y=yy(n)
    Endif
  end subroutine lintpIM_diff

end module ModWaveDiff
