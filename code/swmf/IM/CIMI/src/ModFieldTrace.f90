Module ModDstOutput
       real:: DstOutput
EndModule ModDstOutput


Module ModCimiTrace
  use ModCimi, ONLY: energy
  use ModCimiGrid,ONLY: ir=>np, ip=>nt, iw=>nm , ik=>nk, neng, &
       MinLonPar,MaxLonPar,iProc,iComm,iProcMidnight,nProc,&
       iLonMidnight,nLonPar,nLonPar_P,nLonBefore_P, rb
  use ModCimiPlanet,ONLY: nspec, amu_I
  use ModUtilities, ONLY: CON_set_do_test, CON_stop

  implicit none

  real, allocatable :: &
       bo(:,:), ro(:,:), xmlto(:,:), sinA(:,:,:), &
       Have(:,:,:), pp(:,:,:,:,:), vel(:,:,:,:,:),&
       ekev(:,:,:,:,:), rmir(:,:,:), alscone(:,:,:,:,:),&
       tanA2(:,:,:), volume(:,:), bm(:,:,:), gamma(:,:,:,:),&
       xo(:,:), yo(:,:), tya(:,:,:), gridoc(:,:),phi2o(:,:),&
       Tbounce(:,:,:,:,:), ionization(:,:,:,:,:)

  real	  :: parmod(10)

  integer :: irm(ip), irm0(ip), iba(ip)

  integer :: iw2(nspec,ik)

  logical :: UseEllipse = .true.
  logical :: UseCorotation = .true.
  logical :: UsePotential = .true.
  
  logical :: UseSmooth = .false.
  real    :: SmoothWindow

  integer :: imod=3
  
  real :: NonMonoLength, NonMonoLengthThresh
  
  integer :: iLatTest = -1, iLonTest = -1

  real :: DtUpdateB ! update frequency of Bfield 

  real    :: DeltaRMax = 2.0 !Re
  real    :: xmltlim = 2.0 ! limit of field line warping in hour

  logical :: UseAltitudePrecip = .false., UsePrecipEnergyLoss = .false.
  ! About 510 km
  real :: maxPrecipAlt = 1.08

  !save 5 points around min B when using Tsy model. Otherwise this is from GM in that module. Also save B values at points for both Tsy and MHD
  real, allocatable :: CurvaturePointsXyz_IIID(:,:,:,:)
  real, allocatable :: BCurvaturePoints_III(:,:,:)
  
  public :: gather_field_trace
  public :: bcast_field_trace
  contains
  
  subroutine init_mod_field_trace
    
    if(allocated(bo)) RETURN
    allocate( bo(ir,ip),ro(ir,ip),xmlto(ir,ip),sinA(ir,ip,0:ik+1) )
    allocate(Have(ir,ip,ik),pp(nspec,ir,ip,iw,ik),vel(nspec,ir,ip,iw,ik),&
         ekev(nspec,ir,ip,iw,ik),rmir(ir,ip,ik),alscone(nspec,ir,ip,iw,ik),&
         Tbounce(nspec,ir,ip,iw,ik),ionization(nspec,ir,ip,iw,ik),&
         tanA2(ir,ip,0:ik+1),phi2o(ir,ip),&
         volume(ir,ip),bm(ir,ip,ik),gamma(ir,ip,iw,ik),&
         xo(ir,ip),yo(ir,ip),tya(ir,ip,0:ik+1),gridoc(ir,ip) )
    
  end subroutine init_mod_field_trace

  !***********************************************************************
  !                             fieldpara
  ! Routine calculates kinetic energy, velocity, y, latitude and altitude
  ! at mirror point, etc, for given magnetic moment, K and position for a
  ! given magnetic field configuration.
  ! Output: iba,irm,iw2,vel,ekev,pp,sinA,Have,alscone,ionization         
  !***********************************************************************
  subroutine fieldpara(t,dt,c,q,xlati,xmlt,phi,si,IsRestart)
    use ModCimiPlanet,		ONLY: &
         nspec, re => re_m, rc, xme => dipmom
    use ModNumConst,		ONLY: pi => cPi, cDegToRad
    use ModCimiInitialize,	ONLY: xmm
    use ModMpi
    use ModImTime,		ONLY: iCurrentTime_I
    use ModDstOutput,		ONLY: DstOutput
    use ModCimiUtil,    ONLY: locate1IM
    use CIMI_aurora, ONLY: calc_fang_loss
    
    ! uncomment when T04 Tracing fixed
    common/geopack/aa(10),sps,cps,bb(3),ps,cc(11),kk(2),dd(8)
    external tsyndipoleSM,MHD_B
    
    logical, intent(in) :: IsRestart
    real :: aa,sps,cps,bb,ps,cc,dd
    integer :: kk
    integer :: iday1

    integer, parameter :: np=10000,nd=3
    real :: t, dt, c
    real :: xlati(ir),phi(ip),si(0:ik+1),&
         si3(np),bm1(np),rm(np),rs(np),dss(np),&
         h3(np),bs(np),bba(np),&
         x1(ir),xmlt(ip),xli(0:ir),&
         ra(np),dssa(np),tya3(np)
    integer :: n,i,j,k,m,mir,npf,npf1,im,im1,im2,igood,ii,iTaylor, j1
    real    :: rNeighborMax
    integer :: iopt, n5, iout,n8,n7,n6,m0,n70,ib
    real    :: rlim,dre,xlati1,phi1,xmlt1,ro1,volume1,bo1,dss2
    real    :: dssm,rm1,rme,rl,cost,dssp
    real    :: sim,bmmx,rmm, tya33,h33,xmass,c2mo,c4mo2,ro2,pp1
    real    :: pijkm,pc,c2m,e,q,tcone1,tcone2,x

    real    cosa, &       ! cos of local pitch angle, a
            SqrtBmCosa,&  ! sqrt(Bm) * cos (a) 
            si3OverSqrtBm,&         ! si3 / sqrt(Bm)
            Dsi3OverSqrtBm(np),&    ! d si3 / sqrt(Bm)
            tya3Ro,&                ! tya, gyro-bounce path in RE
            Dtya3Ro(np),&           ! d tya, gyro-bounce path in RE
            Dh3(np),&     ! d h3, hydrogen density * tya
            bsi           ! =bs, averaged magnetic field strength btwn 2 points

    integer :: imax
    integer :: iBufferSend_I(ip)
    real :: R_12,R_24,xmltr,xBoundary(ip),BufferSend_I(ip),MajorAxis,MinorAxis,&
         MajorAxis2,MinorAxis2,sin2,Req2,xo1,xc,xCenter,rell2, bmin
    real, parameter :: LengthMax = 50.0
    integer :: iError
    

    logical, save ::  IsFirstCall=.true.

    real, allocatable :: BufferSend_C(:,:), BufferRecv_C(:,:)

    !--------------------------------------------------------------------------
    
    DstOutput=0.0
    ekev=0.0
    volume = 0.0
    bo = 0.0

    iopt=1               ! dummy parameter in t96_01 and t04_s 
    rlim=2.*rb
    !xmltlim=1.!2.           ! limit of field line warping in hour
    dre=0.06             ! r interval below the surface of the Earth
    n5=16                ! no. of point below the surface of the Earth
    !DeltaRMax = 0.75!2.0

    ! Sets the non-Monotonicity length threshold (in R_E)
    NonMonoLengthThresh = 1. 

    ! Save irm0
    irm0(1:ip)=irm(1:ip)

    ! uncomment when T04 Tracing fixed
    if (imod <= 2) then
       !  Determine parmod
       call TsyParmod(parmod)

       !  Call recalc to calculate the dipole tilt
       iday1=julianday(iCurrentTime_I(1),iCurrentTime_I(2),iCurrentTime_I(3)) 
       if (imod.le.2) then     
          ps=0.                ! force ps = 0 when using Tsy models
          cps=cos(ps)
          sps=sin(ps)
       else
          call recalc(iCurrentTime_I(1),iday1,iCurrentTime_I(4),&
               iCurrentTime_I(5),iCurrentTime_I(6))
       endif
    endif

    if (imod <= 2) then
       if (.not.allocated(CurvaturePointsXyz_IIID))&
            allocate(CurvaturePointsXyz_IIID(ir,ip,5,3))
    endif
    if (.not.allocated(BCurvaturePoints_III))&
         allocate(BCurvaturePoints_III(ir,ip,5))
    
    !initialize to zero
    ro=0.0
    phi2o=0.0
    volume=0.0
    !  Start field line tracing.  
    call timing_start('cimi_trace')
    LONGITUDE: do j=MinLonPar,MaxLonPar
       irm(j)=ir
       LATITUDE: do i=1,ir
          iout=0
          xlati1=xlati(i)*cDegToRad
          xli(i)=rc/cos(xlati1)/cos(xlati1)
          phi1=phi(j)                  ! +x corresponing to noon

          ! uncomment when T04 Tracing fixed
          if (imod.le.2) then
             call tsy_trace(i,j,rlim,re,rc,xlati1,phi1,t,ps,parmod,&
                  imod,np,npf1,dssa,bba,volume1,ro1,xmlt1,bo1,ra)
             
          endif
          if (imod.eq.3) call Mhd_trace_IM(xlati1,phi(j),re,i,j,np, &
               npf1,dssa,bba,volume1,ro1,xmlt1,bo1,ra)
          if ( i == iLatTest .and. j == iLonTest ) then
             write(*,*) "Time: ",t
             write(*,*) "npf1,xlati1*180.0/3.14,xmlt1,ro1"
             write(*,*) npf1,xlati1*180.0/3.14,xmlt1,ro1
             call IM_plot_fieldline(npf1,i,j,t,dssa,ra,bba) 
          endif
          volume(i,j)=volume1
          ro(i,j)=ro1
          if (i.gt.1) then
             if (ro(i,j).lt.ro(i-1,j)) ro(i,j)=ro(i-1,j)
          endif
          xmlto(i,j)=xmlt1
          bo(i,j)=bo1
          phi1=xmlt1*pi/12.+pi         ! phi1=0 corresponing to noon
          phi2o(i,j)=phi1



          xo(i,j)=ro1*cos(phi1)
          yo(i,j)=ro1*sin(phi1)
          
!          if (i==40 .and. j==1) &
!               write(*,*) 'i,j,xlati1, phi(j), xo(i,j),yo(i,j), xmlto(i,j),phi1',&
!               i,j,xlati1, phi(j),  xo(i,j),yo(i,j), xmlto(i,j),phi1 

          
          gridoc(i,j)=1.
          if (npf1.eq.0) gridoc(i,j)=0.

          !xmlto(i,j)=xmlt_1
          !if (j.gt.1.and.xmlto(i,j).lt.0.) xmlto(i,j)=xmlto(i,j)+24.
          !ro(i,j)=ra(ieq)
          !bo(i,j)=bba(ieq)
          !xo(i,j)=xa(ieq)
          !yo(i,j)=ya(ieq)
          !if (iout.ge.1) gridoc(i,j)=0.
          !if (iout.eq.0) gridoc(i,j)=1.
          !          if (j==25)then
          !             write(*,*) '!!!i,j',i,j
          !             write(*,*) 'npf1,xmlt1,irm(j)',npf1,xmlt1,irm(j)
          !             if (i==51) call con_stop('')
          !          endif
          if (npf1.eq.0) then             ! open field line
             irm(j)=i-1
             exit LATITUDE                   ! do next j                 
          endif

          if (xmlt1.gt.xmltlim.and.xmlt1.lt.(24.-xmltlim).and.&
               abs(xmlt1-xmlt(j)).gt.xmltlim) then   ! big warping in mlt
             irm(j)=i-1
             exit LATITUDE
          endif

          ! Excessively long lines are considered open 
          if (dssa(npf1) > LengthMax) then
             irm(j)=i-1
             exit LATITUDE
          endif

          ! Excessive Delta R in equatorial plane results in open lines
          if (i>2) then
             if (ro(i,j)-ro(i-1,j) > DeltaRMax) then
                irm(j)=i-1
                exit LATITUDE
             endif
          endif

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!!!!!!!  							!!!!!!!!!
!!!!!!!!!  							!!!!!!!!!
!!!!!!!!!		BUG DISCOVERED BY COLIN			!!!!!!!!!
!!!!!!!!!  OLD Version determined im1 (mirror point) at the  	!!!!!!!!!
!!!!!!!!!  half-length of the field line.			!!!!!!!!!
!!!!!!!!!  							!!!!!!!!!
!!!!!!!!!  							!!!!!!!!!
!!!!!!!!!  NEW VERSION: Sets im1 to the index corresponding to 	!!!!!!!!!
!!!!!!!!!  minimum B value along fieldline.			!!!!!!!!!
!!!!!!!!!  							!!!!!!!!!
!!!!!!!!!  							!!!!!!!!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

!!$          dss2=dssa(npf1)/2.      ! find the middle point
!!$          !write(*,*) '!!! start1, iLat,iLon,npf1',i,j,npf1
!!$          call locate1IM(dssa,npf1,dss2,im)
!!$
!!$          !write(*,*) '!!! end1'
!!$          im1=im
!!$          if ((dssa(im+1)-dss2).lt.(dss2-dssa(im))) im1=im+1

          im1 = MINLOC(bba( 1:npf1 ), 1)

          !save 5 points around min B for curvature scattering calculation
          if (npf1==1) then
             BCurvaturePoints_III(i,j,1:5) = -1.0
          else
             BCurvaturePoints_III(i,j,1) =bba(im1-2)
             BCurvaturePoints_III(i,j,2) =bba(im1-1)
             BCurvaturePoints_III(i,j,3) =bba(im1)
             BCurvaturePoints_III(i,j,4) =bba(im1+1)
             BCurvaturePoints_III(i,j,5) =bba(im1+2)
          endif
          
          npf=n5           ! make sure B decreases to bba(im1) and rises
          dssm=0.
          NonMonoLength=0.
          do m=1,npf1
             if (m.lt.npf1) dssm=dssm+(dssa(m+1)-dssa(m))
             igood=-1
             if (m.eq.1.or.m.eq.im1) igood=1
             if (m.gt.1.and.m.lt.im1.and.bba(m).gt.bba(im1).and.&
                  bba(m).lt.bm1(npf)) igood=1     ! B should be decreasing
             if (m.gt.im1.and.bba(m).gt.bba(im1).and.&
                  bba(m).gt.bm1(npf)) igood=1     ! B should be increasing
             if (igood.eq.1) then
                npf=npf+1
                bm1(npf)=bba(m)
                rm(npf)=ra(m)
                if (m.lt.npf1) dss(npf)=dssm
                dssm=0.                      ! reset dssm
                if (m.eq.im1) im2=npf        ! new im1
             else
                NonMonoLength = &
                     NonMonoLength + ( dssa( m + 1 ) - dssa( m ) )
             endif
          enddo

          if ( NonMonoLength .gt. NonMonoLengthThresh ) then

             irm(j)=i-1
             exit LATITUDE

          endif

          do m=1,n5               ! Add n5 points below rc
             rm1=rc-m*dre
             rme=rm1*re  
             rm(n5-m+1)=rm1
             rm(npf+m)=rm1
             bm1(n5-m+1)=sqrt(4.-3.*rm1/xli(i))*xme/rme**3 !assume dipole
             bm1(npf+m)=bm1(n5-m+1)
          enddo

          npf=npf+n5
          n=npf-1  ! new no. of intervals from N to S hemisphere
          do m=1,n
             rs(m)=0.5*(rm(m+1)+rm(m))
             bs(m)=0.5*(bm1(m+1)+bm1(m))
          enddo
          do m=1,n
             if (m.le.n5.or.m.ge.(npf-n5)) then
                rl=rs(m)/xli(i)
                cost=sqrt(1.-rl*rl)
                dss(m)=dre*sqrt(3.*cost*cost+1.)/2./cost
             endif
          enddo

          !                                <--dss(m)-->
          !      rs(1)                         rs(m)                     rs(n)
          !      bs(1)                         bs(m)                     bs(n)
          !    |-------|-----|......|------|----------|----|.......|---|-------|
          !  rm(1)                       rm(m)                               rm(n+1)
          !  bm(1)                       bm(m)                               bm(n+1)


          ! Field line integration
          do m=im2-1,1,-1 !im2 = middle of field line
             ! search for the southern conjugate point
             !  such that bm1(n7) < bm1(m) < bm1(n8)
             !  and       bs(n6)  < bm1(n7) < bs(n7)
             n8=npf
             SEARCH: do ii=m,n
                if (bm1(ii+1).ge.bm1(m)) then
                   n8=ii+1
                   EXIT SEARCH
                endif
             enddo SEARCH
             n7=n8-1    
             n6=n7-1

             ! get Dsi,Dty3,Dh3 for field line integration
             do ii=m,n7
                bsi=bs(ii)
                if (bm1(ii+1).ge.bm1(m)) bsi=0.5*(bm1(m)+bm1(ii))
                cosa=sqrt(1.-bsi/bm1(m))  ! sina^2 = bsi/bm1(m)
                Dsi3OverSqrtBm(ii)=cosa
                Dtya3Ro(ii)=1./cosa
                Dh3(ii)=Hden(rs(ii))/cosa
                if (cosa.eq.0.) then
                   Dtya3Ro(ii)=0.
                   Dh3(ii)=0.
                endif
             enddo
  
             ! calculate integration along the field line
             dssp=dss(n7)*(bm1(m)-bm1(n7))/(bm1(n8)-bm1(n7)) ! partial ds
             !   (1) si3 (si)
             si3OverSqrtBm=0.
             call closed2(np,m,n6,Dsi3OverSqrtBm,dss,si3OverSqrtBm)
             si3OverSqrtBm = si3OverSqrtBm + Dsi3OverSqrtBm(n7)*dssp
             si3(m)=re*sqrt(bm1(m))*si3OverSqrtBm  ! K in T^0.5*m
             !   (2) tya3 (tya)
             tya3Ro=0.
             call closed2(np,m,n6,Dtya3Ro,dss,tya3Ro)
             tya3Ro = tya3Ro + Dtya3Ro(n7)*dssp
             tya3(m) = 0.5*tya3Ro/ro(i,j)
             !   (3) h3 (h)
             h3(m)=0.
             call closed2(np,m,n6,Dh3,dss,h3(m))
             h3(m) = h3(m) + Dh3(n7)*dssp
       
          enddo ! end of m (1:im2)

          si3(im2)=0.               ! equatorially mirroring
          tya3(im2)=tya3(im2-1)
          h3(im2)=hden(rm(im2))
          ! bmin=bm1(im2)

          ! Calculate y, rmir (dist. of mirror point), T(y), bounced average [H]
          do m=0,ik+1 
             !write(*,*) '!!! iLat,iLon',i,j
             sim=si(m)                 ! get Bm @ given K & location
             call lintpIM(si3,bm1,im2,sim,bmmx)
             if (m.ge.1.and.m.le.ik) then
                bm(i,j,m)=bmmx
             endif
             !  sinA(i,j,m)=sqrt(bmin/bmmx)
             sinA(i,j,m)=bo(i,j)/bmmx
             sinA(i,j,m)=sqrt(sinA(i,j,m)) 
             if (sinA(i,j,m).gt.1.) sinA(i,j,m)=1.
             call lintpIM(si3,rm,im2,sim,rmm)
             if (m.ge.1.and.m.le.ik) then
                rmir(i,j,m)=rmm
             endif
             call lintpIM(si3,tya3,im2,sim,tya33)
             tya(i,j,m)=tya33
             call lintpIM(si3,h3,im2,sim,h33)
             ! bounce-ave [H]
             if (m.ge.1.and.m.le.ik)  then
                Have(i,j,m)=h33
             endif
          enddo
          ! test si (K) values
          !if (i.eq.30.and.j.eq.1) then
          !   write(*,*) 'K normalized by bm and ro',i,j
          !   write(*,*) 'm, a0(deg), K (cimi), K (analytic, dip)'
          !   do m=1,ik
          !      write(*,*) m,asin(sinA(i,j,m))*180./3.141592,&
          !        si(m)/sqrt(bm(i,j,m))/ro(i,j)/re,&
          !        2.*(1.38+0.32*sinA(i,j,m)*log(sinA(i,j,m))&
          !       -0.64*sqrt(sinA(i,j,m))-0.74*sinA(i,j,m))
          !   enddo
          !   stop
          !endif
          ! test tya values
          !if (i.eq.30.and.j.eq.1) then
          !   write(*,*) 'tya',i,j
          !   do m=0,ik+1
          !      write(*,*) m,sinA(i,j,m),tya(i,j,m),&
          !        1.38-0.32*(sinA(i,j,m)+sqrt(sinA(i,j,m)))
          !   enddo
          !   stop
          !endif
       enddo LATITUDE                              ! end of i loop
    enddo LONGITUDE                              ! end of j loop

    ! Fill in volumes and coordinates for open (?) field lines
    do j = MinLonPar,MaxLonPar
       do i=irm(j)+1,ir
          volume(i,j)=volume(irm(j),j)
          xo(i,j)=xo(irm(j),j)
          yo(i,j)=yo(irm(j),j)
       enddo
    end do
    call timing_stop('cimi_trace')

    ! Peridic boundary condition
    !do i=1,ir        
    !   volume(i,ip+1)=volume(i,1)
    !   xo(i,ip+1)=xo(i,1)
    !   yo(i,ip+1)=yo(i,1)
    !   gridoc(i,ip+1)=gridoc(i,1)
    !enddo

    !.....Calculate p, E, v, et (mc^2) at grid boundaries, and lifetime for
    !     loss cone particles
    do n=1,nspec
       xmass=1.673e-27*amu_I(n)
       c2mo=c*c*xmass ! rest mass
       c4mo2=c2mo*c2mo ! rest mass squared
       do j=MinLonPar,MaxLonPar
          do i=1,irm(j)
             ro2=2.*ro(i,j)*re
             do m=1,ik
                pp1=sqrt(2.*xmass*bm(i,j,m))
                tcone1=ro2*tya(i,j,m) 
                do k=1,iw
                   pijkm=pp1*sqrt(xmm(n,k))
                   pc=pijkm*c
                   c2m=sqrt(pc*pc+c4mo2) ! 
                   e=c2m-c2mo                 ! E in J
                   ekev(n,i,j,k,m)=e/1000./q    ! E in keV
                   gamma(i,j,k,m)=c2m/c2mo
                   pp(n,i,j,k,m)=pijkm  
                   vel(n,i,j,k,m)=pc*c/c2m
                   alscone(n,i,j,k,m)=1.
                   tcone2=tcone1/vel(n,i,j,k,m)      ! Tbounce/2
                   Tbounce(n,i,j,k,m)=tcone2
                   x=dt/tcone2
                   if(UseAltitudePrecip .and. rmir(i,j,m) < maxPrecipAlt) then
                     alscone(n,i,j,k,m) = exp(-x)
                     call calc_fang_loss( &
                        ionization(n,i,j,k,m), ekev(n,i,j,k,m), rmir(i,j,m), n)
                     if(.not.UsePrecipEnergyLoss) &
                        alscone = 1 - ionization(n,i,j,k,m) * &
                           (1. - alscone(n,i,j,k,m))
                   else
                     if (rmir(i,j,m).le.rc) then
                        alscone(n,i,j,k,m)=0.
                        if (x.le.80.) alscone(n,i,j,k,m)=exp(-x)
                     endif
                   end if
                enddo

             enddo
          enddo
       enddo
    enddo

    ! Reduce irm by 1 for convenience in calculating vl at i=irm+0.5
    do j=MinLonPar,MaxLonPar
       irm(j)=irm(j)-1
    enddo

    ! Find iba
    xBoundary = 0.0
    if (UseEllipse) then
       R_24=rb                 ! boundary distance at midnight
       do j=MinLonPar,MaxLonPar
          imax=irm(j)
          xmltr=xmlto(imax,j)*pi/12.
          xBoundary(j)=-ro(imax,j)*cos(xmltr)
       enddo

       !When nProc>1 gather xBoundary to all procs
       if (nProc>1)then
          BufferSend_I(:) = xBoundary(:)
          call MPI_ALLGATHERV(BufferSend_I(MinLonPar:MaxLonPar), nLonPar, &
               MPI_REAL, xBoundary, nLonPar_P, nLonBefore_P, MPI_REAL, iComm, &
               iError)
       end if

       R_12=0.95*maxval(xBoundary)    ! boundary distance at noon
       MajorAxis=0.5*(R_12+R_24)      ! major axis
       MinorAxis=min(R_12,R_24)       ! minor axis
       xCenter=0.5*(R_12-R_24)        ! center on x-axis
       MajorAxis2=MajorAxis*MajorAxis
       MinorAxis2=MinorAxis*MinorAxis
       do j=MinLonPar,MaxLonPar
          find_ib: do i=irm(j),1,-1
             xmltr=xmlto(i,j)*pi/12.
             sin2=sin(xmltr)*sin(xmltr)
             Req2=ro(i,j)*ro(i,j)
             xo1=-ro(i,j)*cos(xmltr)
             xc=xo1-xCenter
             if (sin2 > 0.0) then
                ! r^2 onellipse at x=xc
                rell2= & 
                     MinorAxis2*(1.-xc*xc/MajorAxis2)/sin2 
             elseif(xmlto(i,j) == 0.0) then
                rell2= R_24**2.0 
             elseif(xmlto(i,j) == 12.0) then
                rell2 = R_12**2.0
             endif

             if (Req2.le.rell2) then
                iba(j)=i
                exit find_ib
             endif
          enddo find_ib
       enddo
    else
       !use circle
       do j=MinLonPar,MaxLonPar
          do i=1,irm(j)
             x1(i)=ro(i,j)
          enddo
          call locate1IM(x1,irm(j),rb,ib)
          iba(j)=ib
       enddo

       !kludge add check to make sure iba is not too far past neighbor          
       ! need to loop both ways to ensure smooth boundary                       
       !When nProc>1 gather ro to all procs                                     
       !Gather ro to root to start                                              
       !!!if (nProc>1)then
       !!!   if (.not.allocated(BufferSend_C)) &
       !!!        allocate( BufferSend_C( ir, np ), BufferRecv_C( ir, np ))
       !!!   BufferSend_C(:,:)=ro(:,:)
       !!!   call MPI_ALLGATHERV(BufferSend_C(:,MinLonPar:MaxLonPar), ir*nLonPar, &
       !!!        MPI_REAL, BufferRecv_C, ir*nLonPar_P, ir*nLonBefore_P, &
       !!!        MPI_REAL, iComm, iError)
       !!!   ro(:,:)=BufferRecv_C(:,:)
       !!!
       !!!   !now gather iba
       !!!   iBufferSend_I(:) = iba(:)
       !!!   call MPI_ALLGATHERV(iBufferSend_I(MinLonPar:MaxLonPar), nLonPar, &
       !!!        MPI_INTEGER, iba, nLonPar_P, nLonBefore_P, MPI_INTEGER, iComm, &
       !!!        iError)
       !!!end if
       !!!
       !!!do j=MinLonPar,MaxLonPar
       !!!   j1=j-1
       !!!   if (j1 == 0) j1=ip
       !!!   rNeighborMax = ro(iba(j1),j1)+1.0
       !!!   if(ro(iba(j),j) > rNeighborMax ) then
       !!!      call locate1IM(ro(1:irm(j),j),irm(j),rNeighborMax,ib)
       !!!      iba(j)=ib
       !!!   endif
       !!!enddo
       !!!do j=MaxLonPar,MinLonPar
       !!!   j1=j+1
       !!!   if (j1 == ip+1) j1=1
       !!!   rNeighborMax = ro(iba(j1),j1)+1.0
       !!!   if(ro(iba(j),j) > rNeighborMax ) then
       !!!      call locate1IM(ro(1:irm(j),j),irm(j),rNeighborMax,ib)
       !!!      iba(j)=ib
       !!!   endif
       !!!enddo
    endif

    ! Find iw2(m) (max invariant grid that fits in output energy grid)
    ! Set for midnight
    if (iProc==iProcMidnight .and. IsFirstCall .and. .not.IsRestart) then
       do n=1,nspec
          do m=1,ik
             iw2(n,m)=iw
             find_iw2: do k=1,iw
                !if (ekev(irm(1),1,k,m).gt.energy(neng)) then
                if (ekev(n,irm(iLonMidnight),iLonMidnight,k,m).gt.energy(n,neng)) then
                   iw2(n,m)=k
                   exit find_iw2
                endif
             enddo find_iw2
          enddo
       enddo ! nspec
       IsFirstCall=.false.
    endif
    ! When nProc>1 broadcast iw2 to all processors
    ! For somereason this bcast fails for 2 procs but works for >2procs...
    !  if (nProc>1) call MPI_bcast(iw2,ik,MPI_INTEGER,iProcMidnight,iComm,iError)
    if (nProc>1) call MPI_bcast(iw2,ik*nspec,MPI_INTEGER,iProcMidnight,iComm,iError)

  end subroutine fieldpara

  !*****************************************************************************
  !                             TsyParmod
  !  Rountine calculates the parmod in Tsyganenko model.
  !*****************************************************************************
  subroutine TsyParmod(parmod)
    use ModImTime, ONLY: CurrentTime
    use ModIndicesInterfaces
    use ModConst,   ONLY: cProtonMass
    use ModDstOutput, ONLY: DstOutput
    use ModImIndices, ONLY: interpolate_dst, UseDstKyoto
    real parmod(10),w04(6),rr(6),xlamb(6),beta(6),gamm(6)
    integer :: iError

    real :: VelocitySW, DensitySW, VelocitySwTMP, DensitySwTMP
    real :: BySW, BzSW, BySwTMP, BzSwTMP
    real :: Dst, DstTMP

    ! variables for smoothing
    integer, parameter :: nSmooth = 30 !How many points to include in smoothing
    real    :: SmoothWindowLeft, SmoothWindowRight, DtSmoothWindow,SmoothTime
    integer :: iSmooth
    
    ! variables for History
    integer, parameter :: nHistory = 100 !How many points in history window
    real,parameter :: HistoryWindow=86400.0 !24 hours
    real    :: HistoryWindowLeft, DtHistoryWindow,HistoryTime
    integer :: iHistory
    
    real :: bs1, bs_n, del_t, ert, sk, tdiff, vsw_n, xnsw_n
    integer:: m

    !---------------------------------------------------------------------------

    ! Parameters for T04_S model
    data rr/0.39,0.7,0.031,0.58,1.15,0.88/     ! relaxation rate in hour^-1
    data xlamb/0.39,0.46,0.39,0.42,0.41,1.29/
    data beta/0.8,0.18,2.32,1.25,1.6,2.4/
    data gamm/0.87,0.67,1.32,1.29,0.69,0.53/

    parmod(1:10)=0.             ! initial values

    !Initialize inputs to zero
    DensitySW  = 0.0
    VelocitySW = 0.0
    BySW       = 0.0
    BzSW       = 0.0
    Dst        = 0.0
    
    !\
    !get inputs to calculate parmod and smooth if requested
    !/
    if(UseSmooth) then
       ! Define the parameters for the smoothing window (if used)
       SmoothWindowLeft  = CurrentTime-0.5*SmoothWindow
       SmoothWindowRight = CurrentTime+0.5*SmoothWindow
       DtSmoothWindow    = SmoothWindow/(nSmooth-1)
       ! Average over smoothing Window    
       do iSmooth = 0, nSmooth-1
          !set Time
          SmoothTime= SmoothWindowLeft+iSmooth*DtSmoothWindow
          
          !get SW parameters at smooth time
          call get_SW_V  (SmoothTime, VelocitySwTMP, iError)
          call get_SW_N  (SmoothTime, DensitySwTMP,  iError)
          call get_IMF_Bz(SmoothTime, BzSwTMP,       iError)
          call get_IMF_By(SmoothTime, BySwTMP,       iError)
          if(UseDstKyoto) then
             call interpolate_dst(SmoothTime,DstTMP)
          else
             call get_Dst   (SmoothTime, DstTMP,        iError)
          endif
          
          if (iError /= 0) then
             call con_stop&
                  ("IM_ERROR: Problem getting solar wind in TsyParmod")
          endif
          
          !Add to average
          DensitySW  = DensitySW  + DensitySwTMP / nSmooth
          VelocitySW = VelocitySW + VelocitySwTMP/ nSmooth

          BySW  = BySW  + BySwTMP / nSmooth
          BzSW  = BzSW  + BzSwTMP / nSmooth
          Dst   = Dst   + DstTMP / nSmooth
       end do
    else
       !set the density and velocity at the current time
       call get_SW_V  (CurrentTime, VelocitySW, iError)
       call get_SW_N  (CurrentTime, DensitySW,  iError)
       call get_IMF_Bz(CurrentTime, BzSW,       iError)
       call get_IMF_By(CurrentTime, BySW,       iError)
       if (UseDstKyoto) then
          call interpolate_dst(CurrentTime,Dst)
       else
          call get_Dst   (CurrentTime, Dst,        iError)
       endif
    end if
    
    !\
    ! After getting inputs, start to fill PARMOD array
    !/
    
    !  parmod(1): solar wind pressure in nPa
    parmod(1)=cProtonMass*DensitySW*1.e6*(VelocitySW**2)*1.e6/1.e-9    
    if (parmod(1).lt.2.0) parmod(1)=2.0  ! set min parmod(1) to 2.0

    !  parmod(2): Dst
    parmod(2)=Dst

    ! write Dst to a separate variable to be transferred to precipitation
    ! output:

    DstOutput=Dst




    !  parmod(3:4): IMF By, Bz in nT
    parmod(3)=BySW
    parmod(4)=BzSW

    !  Limit the values of parmod(1:4) in t96 model (imod=1)
    if (imod.eq.1) then
       if (parmod(1).gt.10.) parmod(1)=10.              
       if (parmod(2).lt.-100.) parmod(2)=-100.
       if (parmod(2).gt.20.) parmod(2)=20.
       if (parmod(3).lt.-10.) parmod(3)=-10.
       if (parmod(3).gt.10.) parmod(3)=10.
       if (parmod(4).lt.-10.) parmod(4)=-10.
       if (parmod(4).gt.10.) parmod(4)=10.
    endif

    !  parmod(5:10) for t04_s: w04(1:6) defined in Tsyganenko and Sitnov, 2005
    !  These take into acount the time history over the past 24 hours 
    !  (adjustable could be anything you want)
    if (imod.eq.2) then
       !initialize w04 to zero 
       w04(1:6)=0.
       ! define history parameters
       HistoryWindowLeft  = CurrentTime-HistoryWindow
       DtHistoryWindow    = HistoryWindow/(nHistory-1)   
       HISTORY: do iHistory = 0, nHistory-1
          !set Time
          HistoryTime= HistoryWindowLeft+iHistory*DtHistoryWindow
          
          !get SW parameters at history time
          call get_SW_V  (HistoryTime, VelocitySwTMP, iError)
          call get_SW_N  (HistoryTime, DensitySwTMP,  iError)
          call get_IMF_Bz(HistoryTime, BzSwTMP,       iError)
          
          tdiff = (HistoryTime-CurrentTime)/3600. !time difference in hour
          if (BzSwTMP <  0.) Bs1=-BzSwTMP
          if (BzSwTMP >= 0.) cycle HISTORY ! +ve Bz, no contribution to w04
          xnsw_n=DensitySwTMP/5.           ! normalized sw density
          vsw_n =VelocitySwTMP/400.        ! normalized sw velocity
          Bs_n=Bs1/5.                      ! normalized Bs
          do m=1,6
             ert=exp(rr(m)*tdiff)
             Sk=xnsw_n**xlamb(m)*vsw_n**beta(m)*Bs_n**gamm(m)
             w04(m)=w04(m)+Sk*ert
          enddo
       enddo HISTORY
       ! Set the average spacing between history points in hr
       del_t=DtHistoryWindow/3600.
       if (del_t <= 0.0) del_t=1.0/12.0                
       do m=1,6
          w04(m)=w04(m)*rr(m)*del_t
          parmod(m+4)=w04(m)
       enddo
    endif        ! end of if (imod.eq.2) 

    ! Set limit to parmod for t04_s
    if (imod.eq.2) then
       if (parmod(2).lt.-300.) parmod(2)=-300.     ! Dst
       if (parmod(8).gt.25.) parmod(8)=25.         ! partial ring current
       if (parmod(10).gt.100) parmod(10)=100.      ! region 2 current
    endif
  end subroutine TsyParmod
  
  !*****************************************************************************
  subroutine tsyndipoleSM(imod,iopt,parmod,ps,t,xsm,ysm,zsm,bxsm,bysm,bzsm)
    !  Routine calculate the total (T96 or T04 external and dipole fields) field
    !  in SM.
    
    use EGM_ModTsyganenko, ONLY: t96_01, t04_s, dipole, dipole_t96

    integer :: imod, iopt
    real    :: ps, t, xsm, ysm, zsm, bxsm, bysm, bzsm
    integer, parameter :: i_1=1, m_1=-1
    real :: bx,bxext, bxint, by,byint, byext, bz, bzint, bzext, xgsm,ygsm,zgsm
    real parmod(10)
    
    call smgsm(xsm,ysm,zsm,xgsm,ygsm,zgsm,i_1)
    if (imod.eq.0) then
       bxext=0.
       byext=0.
       bzext=0.

       call dipole_t96(0.,xgsm,ygsm,zgsm,bxint,byint,bzint)
    endif
    if (imod.eq.1) then 
       call t96_01(iopt,parmod,ps,xgsm,ygsm,zgsm,bxext,byext,bzext)
       call dipole_t96(ps,xgsm,ygsm,zgsm,bxint,byint,bzint)
    endif
    if (imod.eq.2) then
       call t04_s(iopt,parmod,ps,xgsm,ygsm,zgsm,bxext,byext,bzext)
       call dipole(ps,xgsm,ygsm,zgsm,bxint,byint,bzint)
    endif
    bx=bxint+bxext           ! gsm bx
    by=byint+byext           ! gsm by
    bz=bzint+bzext           ! gsm bz
    call smgsm(bxsm,bysm,bzsm,bx,by,bz,m_1)
    
  end subroutine tsyndipoleSM
  
  !=============================================================================
  subroutine tsy_trace(iLat,iLon,rlim,re,rc,xlati1,phi1,t,ps,parmod,imod,np, &
       npf1,dssa,bba,volume1,ro1,xmlt1,bo1,ra)
    ! Routine does field line tracing in Tsyganenko field.For a given xlati1 and
    ! phi1, output distance from the ionosphere,magnetic field, flux tube volume
    ! per unit flux and equatorial crossing point.
    ! npf1 is the number of point along that field line.npf1=0 if this isan open
    ! field line.
    !
    ! Input: re,rc,xlati1,phi1,t,ps,parmod,imod,np
    ! Output: npf1,dssa,bba,volume1,ro1,xmlt1,bo1    ! bba, bo1 in Tesla 

    use ModSort, ONLY: sort_quick
    use ModNumConst, ONLY: cPi
    implicit none
!    external tsyndipoleSM

    integer, parameter :: nd=3
    integer imod,np,npf1,i,j,n,ii,iopt,ind(np)
    integer, intent(in) :: iLat,iLon  
    real, intent(out)   :: ra(np)
    integer  :: ieq
    integer  :: ibmin
    real rlim,re,rc,xlati1,phi1,t,ps,parmod(10),dssa(np),bba(np),volume1,ro1,&
         xmlt1,bo1
    real xa(np),ya(np),za(np),x0(3),xend(3),f(3),t0,tend,h,h1,aza(np)
    real dir,pas,xwrk(4,nd),b_mid,dss(np),ss,yint(np)
    !real bba_abs(np)

    iopt=1               ! dummy variable for tsy models
    !  rlim=20.
    dir=-1.              ! start fieldline tracing from Northern hemisphere
    pas=0.1              ! fieldline tracing step in RE
    h=pas*dir

    ! initial
    x0(1)=rc*cos(xlati1)*cos(phi1)  ! sm x
    x0(2)=rc*cos(xlati1)*sin(phi1)  ! sm y
    x0(3)=rc*sin(xlati1)            ! sm z
    t0=0.
    npf1=1
    call tsyndipoleSM(imod,iopt,parmod,ps,t,x0(1),x0(2),x0(3),f(1),f(2),f(3))
    bba(1)=sqrt(f(1)*f(1)+f(2)*f(2)+f(3)*f(3))*1.e-9   ! B in T
    xa(1)=x0(1)
    ya(1)=x0(2)
    za(1)=x0(3)
    ra(1)=sqrt(xa(1)*xa(1)+ya(1)*ya(1)+za(1)*za(1))
    dssa(1)=0.

    ! start tracing
    trace: do
       call rk4(tsyndipoleSM,imod,iopt,parmod,ps,t,t0, &
            h,x0,xend,xwrk,nd,f,tend)
       npf1=npf1+1
       ra(npf1)=sqrt(xend(1)*xend(1)+xend(2)*xend(2)+xend(3)*xend(3))
       xa(npf1)=xend(1)
       ya(npf1)=xend(2)
       za(npf1)=xend(3)
       bba(npf1)=sqrt(f(1)*f(1)+f(2)*f(2)+f(3)*f(3))*1.e-9    ! B in T
       dssa(npf1)=dssa(npf1-1)+abs(h)

       if (ra(npf1).gt.rlim.or.npf1.gt.np) then
          npf1=0              ! open field line
          !        write(*,*) 'iLat,Lat,mlt',iLat,xlati1*180/3.14, xmlt1
          !        exit trace              
          return
       endif

       if (ra(npf1).le.rc) then               ! at south hemisphere
          ! reduce step size such that ra(npf1) is at rc
          h1=(ra(npf1-1)-rc)/(ra(npf1-1)-ra(npf1))
          ra(npf1)=rc
          xa(npf1)=xa(npf1-1)+(xend(1)-xa(npf1-1))*h1
          ya(npf1)=ya(npf1-1)+(xend(2)-ya(npf1-1))*h1
          za(npf1)=za(npf1-1)+(xend(3)-za(npf1-1))*h1
          call tsyndipoleSM(imod,iopt,parmod,ps,t, &
               xa(npf1),ya(npf1),za(npf1),f(1),f(2),f(3))
          bba(npf1)=sqrt(f(1)*f(1)+f(2)*f(2)+f(3)*f(3))*1.e-9   ! B in T
          dssa(npf1)=dssa(npf1-1)+abs(h)*h1

          ! Calculate the flux tube volume per magnetic flux (volume1)
          n=npf1-1 ! n = no. of intervals from N(rc) to S(rc) hemisphere
          do ii=1,n
             b_mid=0.5*(bba(ii)+bba(ii+1))
             dss(ii)=dssa(ii+1)-dssa(ii)
             yint(ii)=1./b_mid
          enddo
          call closed(n,yint,dss,ss)  ! use closed form
          if (iLat.ge.1.and.iLat.le.ir) volume1=ss*re   ! volume / flux
          exit trace      ! finish tracing this field line
       endif

       x0(1:nd)=xend(1:nd)
       t0=tend
    enddo trace           ! continue tracing along this field line

    ! find the equatorial crossing point
    aza(1:npf1)=abs(za(1:npf1))

    !bba_abs(1:npf1)=abs(bba(1:npf1))
    !ibmin=minloc(bba_abs(1:npf1),DIM=1)
    ibmin=minloc(abs(bba(1:npf1)),DIM=1)
    if (ibmin>-1) then
       bo1=bba(ibmin)
       ro1=ra(ibmin)

       !save 5 points around min b for curvature scattering
       CurvaturePointsXyz_IIID(iLat,iLon,1,1) = xa(ibmin-2)
       CurvaturePointsXyz_IIID(iLat,iLon,1,2) = ya(ibmin-2)
       CurvaturePointsXyz_IIID(iLat,iLon,1,3) = za(ibmin-2)
              
       CurvaturePointsXyz_IIID(iLat,iLon,2,1) = xa(ibmin-1)
       CurvaturePointsXyz_IIID(iLat,iLon,2,2) = ya(ibmin-1)
       CurvaturePointsXyz_IIID(iLat,iLon,2,3) = za(ibmin-1)
              
       CurvaturePointsXyz_IIID(iLat,iLon,3,1) = xa(ibmin)
       CurvaturePointsXyz_IIID(iLat,iLon,3,2) = ya(ibmin)
       CurvaturePointsXyz_IIID(iLat,iLon,3,3) = za(ibmin)
              
       CurvaturePointsXyz_IIID(iLat,iLon,4,1) = xa(ibmin+1)
       CurvaturePointsXyz_IIID(iLat,iLon,4,2) = ya(ibmin+1)
       CurvaturePointsXyz_IIID(iLat,iLon,4,3) = za(ibmin+1)
              
       CurvaturePointsXyz_IIID(iLat,iLon,5,1) = xa(ibmin+2)
       CurvaturePointsXyz_IIID(iLat,iLon,5,2) = ya(ibmin+2)
       CurvaturePointsXyz_IIID(iLat,iLon,5,3) = za(ibmin+2)
              
    endif
    !here we get xmlt1 at the bmin which is done exactly the same was as we
    ! did it before for the equator but now we use the bmin index. 
    xmlt1 = &
         atan2(-ya(ibmin),-xa(ibmin))*12./cPi   ! mlt in hr
    if (xmlt1.lt.0.) xmlt1=xmlt1+24.

  end subroutine tsy_trace
  !-----------------------------------------------------------------------------
  subroutine rk4(Bfield,imod,iopt,parmod,ps,t,t0,h,x0,xend,xwrk,nd,f,tend)
    !---------------------------------------------------------------------------
    !   *** FOURTH-ORDER RUNGE-KUTTA ***
    !   Solve xend = x0 + fn*h                
    ! fn is the unit vector of f
    
    integer :: imod,iopt, nd 
    real    :: ps,t,t0,h,tend
    real x0(nd),xend(nd),x00(3),xwrk(4,nd),f(nd),parmod(10)
    real    :: fmag
    integer :: i
    external Bfield
    
    call Bfield(imod,iopt,parmod,ps,t,x0(1),x0(2),x0(3),f(1),f(2),f(3))
    
    fmag=sqrt(f(1)*f(1)+f(2)*f(2)+f(3)*f(3))
    do i=1,nd
       x00(i)=x0(i)
       xwrk(1,i)=h*f(i)/fmag
       xend(i)=x00(i)+xwrk(1,i)/2.
       x0(i)=xend(i)
    enddo
    call Bfield(imod,iopt,parmod,ps,t,x0(1),x0(2),x0(3),f(1),f(2),f(3))
    fmag=sqrt(f(1)*f(1)+f(2)*f(2)+f(3)*f(3))
    do i=1,nd
       xwrk(2,i)=h*f(i)/fmag
       xend(i)=x00(i)+xwrk(2,i)/2.
       x0(i)=xend(i)
    enddo
    call Bfield(imod,iopt,parmod,ps,t,x0(1),x0(2),x0(3),f(1),f(2),f(3))
    fmag=sqrt(f(1)*f(1)+f(2)*f(2)+f(3)*f(3))
    do i=1,nd
       xwrk(3,i)=h*f(i)/fmag
       xend(i)=x00(i)+xwrk(3,i)
       x0(i)=xend(i)
    enddo
    call Bfield(imod,iopt,parmod,ps,t,x0(1),x0(2),x0(3),f(1),f(2),f(3))
    fmag=sqrt(f(1)*f(1)+f(2)*f(2)+f(3)*f(3))
    do i=1,nd
       xwrk(4,i)=h*f(i)/fmag
    enddo
    do i=1,nd
       xend(i)=x00(i)+(xwrk(1,i)+2.*xwrk(2,i)&
            +2.*xwrk(3,i)+xwrk(4,i))/6.
    enddo
    tend=t0+h
    
  end subroutine rk4
  !=============================================================================

  subroutine mhd_trace_IM (Lat,Lon,re,iLat,iLon,np, &
       nAlt,FieldLength_I,Bfield_I,volume1,ro1,xmlt1,bo1,RadialDist_I)

    use ModGmCimi
    use ModNumConst, ONLY: cPi

    integer,intent(in)  :: iLat,iLon,np
    real,   intent(in)  :: Lat,Lon,re
    ! bba, bo1 in Tesla 
    real,   intent(out) :: RadialDist_I(np),FieldLength_I(np),Bfield_I(np),&
         volume1,ro1,xmlt1,bo1
    integer,intent(out) :: nAlt

    ! Number of points covering the gap from 1 Re to rBody 
    integer, parameter :: MinAlt = 25

    integer, parameter :: nd=3
    integer ::i,j,n,ii,iopt,ind(np),iPoint,iAlt
    integer, parameter :: I_=1,S_=2,R_=3,B_=4
    real :: LatMin = .886  ! 50.7degrees, below this, fieldline 
    ! extends below 2.5Re


    real xa(np),ya(np),za(np),x0(3),xend(3),f(3),t0,tend,h,h1,aza(np)
    real dir,pas,xwrk(4,nd),rlim,Bmid,dss(np),ss,yint(np)
    character(len=*), parameter :: NameSub='mhd_trace_IM'
    Logical IsFoundLine,UseDipole
    !---------------------------------------------------------------------------

    !Calculate LatMin below which we must use dipole
    LatMin=acos(sqrt(1.0/(rBodyGM)))
    
    ! Put BufferLine_VI indexed by line number into StateLine_CIIV

    ! Start after MinAlt points inside rBody
    iAlt = MinAlt
    IsFoundLine=.false.
    UseDipole  =.false.
    FieldTrace: do iPoint = 1,nPoint

!       if(iLon==5 .and. iLat==22) &
!            write(*,*)'iLineIndex_II(iLon,iLat),int(StateLine_VI(I_,iPoint))'&
!            ,iLineIndex_II(iLon,iLat),int(StateLine_VI(I_,iPoint))

       if (iLineIndex_II(iLon,iLat) == int(StateLine_VI(I_,iPoint)))then
          !if(iLon==5)write(*,*)'!!! Found line for iLat,iLon',iLat,iLon
          !when line index found, fill in output arrays
          if(iAlt > np) &
               call CON_stop(NameSub//': iAlt exceeds np. Increase np in fieldpara!')
          Bfield_I(iAlt)     = StateLine_VI(B_,iPoint)
          FieldLength_I(iAlt)= StateLine_VI(S_,iPoint)
          RadialDist_I(iAlt) = StateLine_VI(R_,iPoint)

          iAlt = iAlt+1
          IsFoundLine=.true.
       elseif (iLineIndex_II(iLon,iLat) /= int(StateLine_VI(I_,iPoint)) &
            .and. IsFoundLine) then
          exit FieldTrace 
       endif
    end do FieldTrace

    ! Add points for the other end inside rBody
    nAlt = iAlt-1 + MinAlt
    
    ! Fill in points below rBody
    if (IsFoundLine) &
         call trace_dipoleIM(Re,abs(Lat),nAlt,MinAlt,FieldLength_I,&
         Bfield_I,RadialDist_I,Ro1)

    ! Field lines fully inside rBody
    if (abs(Lat) <= Latmin) then
       nAlt=2*MinAlt
       call trace_dipoleIM(Re,abs(Lat),nAlt,MinAlt,FieldLength_I,&
            Bfield_I,RadialDist_I,Ro1)
       xmlt1= mod((Lon)*12./cPi+12.0,24.0)   ! mlt in hr           
       if (xmlt1.lt.0.) xmlt1=xmlt1+24.
       bo1=Bfield_I(nAlt/2)
       IsFoundLine = .true.
       UseDipole   = .true.
    end if

    !Check if FieldLine is open
    if (.not. IsFoundLine) then
       nAlt = 0
       ro1  = 0.0
       xmlt1= 0.0
       return
    endif

    if (.not. UseDipole) then
       ro1=sqrt(sum(StateBmin_IIV(iLat,iLon,1:2)**2.0))
       xmlt1=&
            (atan2(-StateBmin_IIV(iLat,iLon,2),&
            -StateBmin_IIV(iLat,iLon,1)))&
            *12./cPi   ! mlt in hr

!       if (iLon==25 .and. .not.UseDipole) then
!                    write(*,*) '!!! iLat,iLon,iLineIndex_II(iLon,iLat),iPoint',iLat,iLon,iLineIndex_II(iLon,iLat),iPoint
!          write(*,*) '!!! StateBmin_IIV(iLat,iLon,2),-StateBmin_IIV(iLat,iLon,1),Lon,xmlt1',&
!               StateBmin_IIV(iLat,iLon,2),-StateBmin_IIV(iLat,iLon,1),Lon,xmlt1
!          call con_stop('')
!       endif



       if (xmlt1 < 0.) xmlt1=xmlt1+24.
       bo1=StateBmin_IIV(iLat,iLon,3)
    endif

    !Check that nAlt < np
    if (nAlt > np) then 
       !write(*,*) 'nAlt,np',nAlt,np
       !call CON_STOP('IM error: nAlt > np in mhd_trace_IM. Increase np and recompile.')
       !Treat line as open
       nAlt=0
       return
    endif
    
    ! Calculate the flux tube volume per magnetic flux (volume1)

    !write(*,*) '!!! iLat,iLon,nAlt',iLat,iLon,nAlt
    do ii=1,nAlt-1
       Bmid=0.5*(Bfield_I(ii)+Bfield_I(ii+1))
       dss(ii)=FieldLength_I(ii+1)-FieldLength_I(ii)
       yint(ii)=1./Bmid
    enddo
    call closed(nAlt-1,yint,dss,ss)  ! use closed form
    if (iLat >= 1 .and. iLat <= ir) volume1=ss*re   ! volume / flux
  end subroutine mhd_trace_IM
  
  !============================================================================
  subroutine lintpIM(xx,yy,n,x,y)
    !-----------------------------------------------------------------------
    !  Routine does 1-D interpolation.  xx must be increasing or decreasing
    !  monotonically.  x is between xx(j) and xx(j+1)
    integer :: n
    real xx(n),yy(n)
    integer :: ier = 0, i, j, jl, ju, jm 
    real    :: x, d, y
    !  Make sure xx is increasing or decreasing monotonically
    do i=2,n
       if (xx(n).gt.xx(1).and.xx(i).lt.xx(i-1)) then
          write(*,*) ' lintpIM: xx is not increasing monotonically '
          write(*,*) n,(xx(j),j=1,n)
          call CON_stop('IM ERROR')
       endif
       if (xx(n).lt.xx(1).and.xx(i).gt.xx(i-1)) then
          write(*,*) ' lintpIM: xx is not decreasing monotonically '
          write(*,*) 'i,xx(i),xx(i-1) ',i,xx(i-1),xx(i)
          write(*,*) n,(xx(j),j=1,n)
          call CON_stop('IM ERROR')
       endif
    enddo
    
    !  Set ier=1 if out of range
    if (xx(n).gt.xx(1)) then
       if (x.lt.xx(1).or.x.gt.xx(n)) ier=1
    else
       if (x.gt.xx(1).or.x.lt.xx(n)) ier=1
    endif
    if (ier.eq.1) then
       write(*,*) ' Error: ier.eq.1'
       write(*,*) ' x  ',x
       write(*,*) ' xx  ',xx
       call CON_stop('IM ERROR')
    endif
    !
    !    initialize lower and upper values
    !
    jl=1
    ju=n
    !
    !    if not dne compute a midpoint
    !
10  if(ju-jl.gt.1)then
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
    
  end subroutine lintpIM
  

  !============================================================================
  
  function Hden(x)
    !--------------------------------------------------------------------------
    ! Chamberlain model of [H] fitted by an exponential function.
    ! The fit matches Rairden et al. [1986] for radial distance
    ! from 1.08(rexob) to 12 Re.
    
    implicit none
    
    real x,rexob,rr,ar,Hden
    
    rexob=1.08       ! 1.08 = exobase in Rairden et al. [1986]
    rr=x
    if (rr.lt.rexob) rr=rexob
    ar=log(rr/rexob)       ! rexob = exobase in Rairden et al. [1986]
    Hden=1.e6*exp(10.692-4.4431*ar**0.715831)      ! H density in m^-3
    
  end function Hden
  
  !***********************************************************************
  !                          closed
  ! Routine performs numerical integration using closed form 
  !
  !  S = y1dx1 + y2dx2 + .. + yidxi + .. + yn-1dxn-1 + yndxn
  !                 
  !  where yi is the value at the middle of dxi
  !***********************************************************************
  subroutine closed(n,y,dx,s)
    integer, intent(in) :: n
    real,    intent(in) :: y(n),dx(n)
    real,    intent(out):: s
    integer :: i
    !--------------------------------------------------------------------------
    s=0.           
    do i=1,n     
       s=s+y(i)*dx(i) 
    enddo
    
  end subroutine closed


  !***********************************************************************
  !                          closed2
  ! Routine does similar to closed but from n1 to n2
  !
  !  S = y1dx1 + y2dx2 + .. + yidxi + .. + yn-1dxn-1 + yndxn
  !                 
  !  where yi is the value at the middle of dxi
  !***********************************************************************
  subroutine closed2(n,n1,n2,y,dx,s)
    integer, intent(in) :: n,n1,n2
    real,    intent(in) :: y(n),dx(n)
    real,    intent(out):: s
    integer :: i
    !--------------------------------------------------------------------------
    
    s=0.           
    do i=n1,n2     
       s=s+y(i)*dx(i) 
    enddo
    
  end subroutine closed2
  

  !=============================================================================
  integer function julianday(year, mon, day) result(Julian_Day)
    
    implicit none
    
    integer :: i
    integer, dimension(1:12) :: dayofmon
    integer :: year, mon, day
    
    dayofmon(1) = 31
    dayofmon(2) = 28
    dayofmon(3) = 31
    dayofmon(4) = 30
    dayofmon(5) = 31
    dayofmon(6) = 30
    dayofmon(7) = 31
    dayofmon(8) = 31
    dayofmon(9) = 30
    dayofmon(10) = 31
    dayofmon(11) = 30
    dayofmon(12) = 31
    
    if (mod(year,4).eq.0) dayofmon(2) = dayofmon(1) + 1
    Julian_Day = 0
    do i = 1, mon-1
       Julian_Day = Julian_Day + dayofmon(i)
    enddo
    Julian_Day = Julian_Day + day
    
  end function julianday

  subroutine gather_field_trace

    use ModCimiGrid,	ONLY:	&
         iProc, nProc, iComm, nLonPar, nLonPar_P, nLonBefore_P, &
         MinLonPar, MaxLonPar, nt, np, neng, npit, nm, nk, dlat, &
         phi, sinao, xlat, xmlt
    use ModMPI
    
    !Vars for mpi passing
    real, allocatable 		:: 	&
         BufferSend_C( :, : ), BufferRecv_C( :, : )
    integer, allocatable	::	&
         iReceiveCount_P(:), iDisplacement_P(:)
    integer, allocatable	::	&
         BufferSend_I(:), BufferRecv_I(:)
    integer :: iSendCount, iK, iError, iStatus_I( MPI_STATUS_SIZE )

    allocate( BufferSend_C( np, nt ), BufferRecv_C( np, nt ), &
         iReceiveCount_P( nProc ), iDisplacement_P( nProc ), &
         BufferSend_I( nt ), BufferRecv_I( nt ) )

    ! Gather to root
    iSendCount = np * nLonPar
    iReceiveCount_P = np * nLonPar_P
    iDisplacement_P = np * nLonBefore_P

    BufferSend_I(:)=irm(:)
    call MPI_GATHERV(BufferSend_I(MinLonPar:MaxLonPar), nLonPar, &
         MPI_INTEGER, BufferRecv_I, nLonPar_P, nLonBefore_P, &
         MPI_INTEGER, 0, iComm, iError)
    if (iProc==0) irm(:)=BufferRecv_I(:)
    
    BufferSend_I(:) = iba(:)
    call MPI_GATHERV(BufferSend_I(MinLonPar:MaxLonPar), nLonPar, &
         MPI_INTEGER, BufferRecv_I, nLonPar_P, nLonBefore_P, &
         MPI_INTEGER, 0, iComm, iError)
    if (iProc==0) iba(:)=BufferRecv_I(:)
    
    BufferSend_C(:,:)=volume(:,:)
    call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar), iSendCount, &
         MPI_REAL, BufferRecv_C, iReceiveCount_P, iDisplacement_P, &
         MPI_REAL, 0, iComm, iError)
    if (iProc==0) volume(:,:)=BufferRecv_C(:,:)
    
    BufferSend_C(:,:)=xo(:,:)
    call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar), iSendCount, &
         MPI_REAL, BufferRecv_C, iReceiveCount_P, iDisplacement_P, &
         MPI_REAL, 0, iComm, iError)
    if (iProc==0) xo(:,:)=BufferRecv_C(:,:)
    
    BufferSend_C(:,:)=yo(:,:)
    call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar), iSendCount, &
         MPI_REAL, BufferRecv_C, iReceiveCount_P, iDisplacement_P, &
         MPI_REAL, 0, iComm, iError)
    if (iProc==0) yo(:,:)=BufferRecv_C(:,:)
    
    BufferSend_C(:,:)=bo(:,:)
    call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar), iSendCount, &
         MPI_REAL, BufferRecv_C, iReceiveCount_P, iDisplacement_P, &
         MPI_REAL, 0, iComm, iError)
    if (iProc==0) bo(:,:) = BufferRecv_C(:,:)
    
    do iK = 1, nk
       
       BufferSend_C(:,:)=bm(:,:,iK)
       call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar),iSendCount, &
            MPI_REAL, BufferRecv_C,iReceiveCount_P, iDisplacement_P, &
            MPI_REAL, 0, iComm, iError)
       if (iProc==0) bm(:,:,iK)=BufferRecv_C(:,:)
       
    end do
    
    BufferSend_C(:,:)=xmlto(:,:)
    call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar), iSendCount, &
         MPI_REAL, BufferRecv_C, iReceiveCount_P, iDisplacement_P, &
         MPI_REAL, 0, iComm, iError)
    if (iProc==0) xmlto(:,:)=BufferRecv_C(:,:)
    
    BufferSend_C(:,:)=ro(:,:)
    call MPI_GATHERV(BufferSend_C(:,MinLonPar:MaxLonPar), iSendCount, &
         MPI_REAL, BufferRecv_C, iReceiveCount_P, iDisplacement_P, &
         MPI_REAL, 0, iComm, iError)
    if (iProc==0) ro(:,:)=BufferRecv_C(:,:)

    deallocate( iReceiveCount_P, iDisplacement_P, &
         BufferSend_C, BufferRecv_C, BufferSend_I, BufferRecv_I )
    
  end subroutine gather_field_trace
  

  subroutine bcast_field_trace

    use ModCimiGrid,	ONLY:	&
         iProc, nProc, iComm, nLonPar, nLonPar_P, nLonBefore_P, &
         MinLonPar, MaxLonPar, nt, np, neng, npit, nm, nk, dlat, &
         phi, sinao, xlat, xmlt
    use ModMPI

    integer 			:: iError

    
    call MPI_bcast( irm, nt, MPI_LOGICAL, 0, iComm, iError)
    call MPI_bcast( iba, nt, MPI_LOGICAL, 0, iComm, iError)
    call MPI_bcast( volume, np * nt * nk, MPI_LOGICAL, 0, iComm, iError)
    call MPI_bcast( xo, np * nt, MPI_LOGICAL, 0, iComm, iError)
    call MPI_bcast( yo, np * nt, MPI_LOGICAL, 0, iComm, iError)
    call MPI_bcast( bo, np * nt, MPI_LOGICAL, 0, iComm, iError)
    call MPI_bcast( bm, np * nt * nk, MPI_LOGICAL, 0, iComm, iError)
    call MPI_bcast( xmlto, np * nt, MPI_LOGICAL, 0, iComm, iError)
    call MPI_bcast( ro, np * nt, MPI_LOGICAL, 0, iComm, iError)
    
  end subroutine bcast_field_trace
  
end Module ModCimiTrace
