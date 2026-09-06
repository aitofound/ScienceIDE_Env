!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
! Fast scheme for NLTE cooling rates at 15um by CO2 in a Martian GCM !
!                 Version dlvr11_03. 2012.                           !
! Software written and provided by IAA/CSIC, Granada, Spain,         !
! under ESA contract "Mars Climate Database and Physical Models"     !
! Person of contact: Miguel Angel Lopez Valverde  valverde@iaa.es    !
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
c**********************************************************************

c     Includes the following old 1-D model files/subroutines

c     -MZTCRSUB_dlvr11.f
c     *dinterconnection
c     *planckd
c     *leetvt
c     -MZTFSUB_dlvr11_02.f
c     *initial
c     *intershphunt
c     *interstrhunt
c     *intzhunt
c     *intzhunt_cts
c     *rhist
c     *we_clean
c     *mztf_correccion
c     *mzescape_normaliz
c     *mzescape_normaliz_02
c     -interdpESCTVCISO_dlvr11.f
c     -hunt_cts.f
c     -huntdp.f
c     -hunt.f
c     -interdp_limits.f
c     -interhunt2veces.f
c     -interhunt5veces.f
c     -interhuntdp3veces.f
c     -interhuntdp4veces.f
c     -interhuntdp.f
c     -interhunt.f
c     -interhuntlimits2veces.f
c     -interhuntlimits5veces.f
c     -interhuntlimits.f
c     -lubksb_dp.f
c     -ludcmp_dp.f
c     -LUdec.f
c     -mat_oper.f
c     *unit
c     *diago
c     *invdiag
c     *samem
c     *mulmv
c     *trucodiag
c     *trucommvv
c     *sypvmv
c     *mulmm
c     *resmm
c     *sumvv
c     *sypvvv
c     *zerom
c     *zero4m
c     *zero3m
c     *zero2m
c     *zerov
c     *zero4v
c     *zero3v
c     *zero2v
c     -suaviza.f

c**********************************************************************


c     *** Old MZTCRSUB_dlvr11.f ***

!************************************************************************

!      subroutine dinterconnection ( v, vt )


************************************************************************

!      implicit none
!      include 'nlte_paramdef.h'

c     argumentos
!      real*8 vt(nl), v(nl)

c     local variables
!      integer 	i

c     *************
!
!      do i=1,nl
!         v(i) = vt(i)
!      end do

!      return
!      end

c***********************************************************************
      function planckdp(tp,xnu)
c***********************************************************************

      implicit none

      include 'nlte_paramdef.h'

      real*8 planckdp
      real*8 xnu
      real tp

      planckdp = gamma*xnu**3.0d0 / exp( ee*xnu/dble(tp) )
                                !erg cm-2.sr-1/cm-1.

c     end
      return
      end

c***********************************************************************
      subroutine leetvt

c***********************************************************************

      implicit none

      include 'nlte_paramdef.h'
      include 'nlte_commons.h'

c     local variables
      integer i
      real*8	zld(nl), zyd(nzy)
      real*8  xvt11(nzy), xvt21(nzy), xvt31(nzy), xvt41(nzy) 

c***********************************************************************

      do i=1,nzy
         zyd(i) = dble(zy(i))
         xvt11(i)= dble( ty(i) )
         xvt21(i)= dble( ty(i) )
         xvt31(i)= dble( ty(i) )
         xvt41(i)= dble( ty(i) )
      end do

      do i=1,nl
         zld(i) = dble( zl(i) )
      enddo
      call interhuntdp4veces ( v626t1,v628t1,v636t1,v627t1, zld,nl,
     $     xvt11, xvt21, xvt31, xvt41, zyd,nzy, 1 )


c     end
      return
      end


c     *** MZTFSUB_dlvr11_02.f ***


c     ****************************************************************
      subroutine initial

c     ****************************************************************

      implicit none

      include 'nlte_paramdef.h'
      include 'nlte_commons.h'

c     local variables
      integer 	i

c     ***************

      eqw = 0.0d00
      aa = 0.0d00
      cc = 0.0d00
      dd = 0.0d00

      do i=1,nbox
         ccbox(i) = 0.0d0
         ddbox(i) = 0.0d0
      end do

      return
      end

c     **********************************************************************

      subroutine intershphunt (i, alsx,adx,xtemp)

c     **********************************************************************

      implicit none

      include 'nlte_paramdef.h'
      include 'nlte_commons.h'

c     arguments
      real*8 alsx(nbox_max),adx(nbox_max) ! Output
      real*8 xtemp(nbox_max)    ! Input
      integer    i              ! I , O

c     local variables
      integer 	k
      real*8          factor
      real*8    temperatura     ! para evitar valores ligeramnt out of limits

c     ***********

      do 1, k=1,nbox
         temperatura = xtemp(k)
         if (abs(xtemp(k)-thist(1)).le.0.01d0) then
            temperatura=thist(1)
         elseif (abs(xtemp(k)-thist(nhist)).le.0.01d0) then
            temperatura=thist(nhist)
         elseif (xtemp(k).lt.thist(1)) then
            temperatura=thist(1)
            write (*,*) ' WARNING intershphunt/ Too low atmosph Tk:'
            write (*,*) ' WARNING      k,xtemp = ', k,xtemp(k)
            write (*,*) ' Minimum Tk in histogram used : ', thist(1)
         elseif (xtemp(k).gt.thist(nhist)) then
            temperatura=thist(nhist)
            write (*,*) ' WARNING intershphunt/ Very high atmosph Tk:'
            write (*,*) ' WARNING      k,xtemp = ', k,xtemp(k)
            write (*,*) ' Max Tk in histogram used : ', thist(nhist)
         endif
         call huntdp ( thist,nhist, temperatura, i )
         if ( i.eq.0 .or. i.eq.nhist ) then
	    write (*,*) ' HUNT/ Limits input grid:',
     @           thist(1),thist(nhist)
	    write (*,*) ' HUNT/ location in grid:', xtemp(k)
            stop ' INTERSHP/ Interpolation error. T out of Histogram.'
         endif
         factor = 1.d0 /  (thist(i+1)-thist(i))
         alsx(k) = (( xls1(i,k)*(thist(i+1)-xtemp(k)) +
     @        xls1(i+1,k)*(xtemp(k)-thist(i)) )) * factor
         adx(k)  = (( xld1(i,k)*(thist(i+1)-xtemp(k)) +
     @        xld1(i+1,k)*(xtemp(k)-thist(i)) )) * factor
 1    continue

      return
      end

c     **********************************************************************

      subroutine interstrhunt (i, stx, ts, sx, xtemp )

c     **********************************************************************

      implicit none

      include 'nlte_paramdef.h'
      include 'nlte_commons.h'

c     arguments
      real*8 		stx     ! output, total band strength
      real*8		ts      ! input, temp for stx
      real*8		sx(nbox_max) ! output, strength for each box
      real*8		xtemp(nbox_max) ! input, temp for sx
      integer 	i

c     local variables
      integer 	k
      real*8          factor
      real*8    temperatura

c     ***********

      do 1, k=1,nbox
         temperatura = xtemp(k)
         if (abs(xtemp(k)-thist(1)).le.0.01d0) then
            temperatura=thist(1)
         elseif (abs(xtemp(k)-thist(nhist)).le.0.01d0) then
            temperatura=thist(nhist)
         elseif (xtemp(k).lt.thist(1)) then
            temperatura=thist(1)
            write (*,*) ' WARNING interstrhunt/ Too low atmosph Tk:'
	    write (*,*) ' WARNING     k,xtemp(k) = ', k,xtemp(k)
	    write (*,*) ' Minimum Tk in histogram used : ', thist(1)
         elseif (xtemp(k).gt.thist(nhist)) then
            temperatura=thist(nhist)
            write (*,*) ' WARNING interstrhunt/ Very high atmosph Tk:'
	    write (*,*) ' WARNING     k,xtemp(k) = ', k,xtemp(k)
	    write (*,*) ' Max Tk in histogram used : ', thist(nhist)
         endif
         call huntdp ( thist,nhist, temperatura, i )
         if ( i.eq.0 .or. i.eq.nhist ) then
	    write(*,*)'HUNT/ Limits input grid:',
     $           thist(1),thist(nhist)
	    write(*,*)'HUNT/ location in grid:',xtemp(k)
            stop'INTERSTR/1/ Interpolation error. T out of Histogram.'
         endif
         factor = 1.d0 /  (thist(i+1)-thist(i))
         sx(k) = ( sk1(i,k)   * (thist(i+1)-xtemp(k))
     @        + sk1(i+1,k) * (xtemp(k)-thist(i))  ) * factor
 1    continue


      temperatura = ts
      if (abs(ts-thist(1)).le.0.01d0) then
         temperatura=thist(1)
      elseif (abs(ts-thist(nhist)).le.0.01d0) then
         temperatura=thist(nhist)
      elseif (ts.lt.thist(1)) then
         temperatura=thist(1)
         write (*,*) ' WARNING interstrhunt/ Too low atmosph Tk:'
         write (*,*) ' WARNING            ts = ', temperatura
         write (*,*) ' Minimum Tk in histogram used : ', thist(1)
      elseif (ts.gt.thist(nhist)) then
         temperatura=thist(nhist)
         write (*,*) ' WARNING interstrhunt/ Very high atmosph Tk:'
         write (*,*) ' WARNING            ts = ', temperatura
         write (*,*) ' Max Tk in histogram used : ', thist(nhist)
      endif
      call huntdp ( thist,nhist, temperatura, i )
      if ( i.eq.0 .or. i.eq.nhist ) then
         write (*,*) ' HUNT/ Limits input grid:',
     @        thist(1),thist(nhist)
         write (*,*) ' HUNT/ location in grid:', ts
         stop ' INTERSTR/2/ Interpolat error. T out of Histogram.'
      endif
      factor = 1.d0 /  (thist(i+1)-thist(i))
      stx = 0.d0
      do k=1,nbox
         stx = stx + no(k) * ( sk1(i,k)*(thist(i+1)-ts) +
     @        sk1(i+1,k)*(ts-thist(i)) ) * factor
      end do


      return
      end

c     **********************************************************************

      subroutine intzhunt (k, h, aco2,ap,amr,at, con)

c     k lleva la posicion de la ultima llamada a intz , necesario para
c     que esto represente una aceleracion real.
c     **********************************************************************

      implicit none
      include 'nlte_paramdef.h'
      include 'nlte_commons.h'

c     arguments
      real		h       ! i
      real*8		con(nzy) ! i
      real*8		aco2, ap, at, amr ! o
      integer		k       ! i

c     local variables
      real          factor

c     ************

      call hunt ( zy,nzy, h, k )
      factor =  (h-zy(k)) /  (zy(k+1)-zy(k))
      ap = dble( exp( log(py(k)) + log(py(k+1)/py(k)) * factor ) )
      aco2 = dlog(con(k)) + dlog( con(k+1)/con(k) ) * dble(factor)
      aco2 = exp( aco2 )
      at = dble( ty(k) + (ty(k+1)-ty(k)) * factor )
      amr = dble( mr(k) + (mr(k+1)-mr(k)) * factor )


      return
      end

c     **********************************************************************

      subroutine intzhunt_cts (k, h, nzy_cts_real, 
     @     aco2,ap,amr,at, con)

c     k lleva la posicion de la ultima llamada a intz , necesario para
c     que esto represente una aceleracion real.
c     **********************************************************************

      implicit none
      include 'nlte_paramdef.h'
      include 'nlte_commons.h'

c     arguments
      real		h       ! i
      real*8		con(nzy_cts) ! i
      real*8		aco2, ap, at, amr ! o
      integer		k       ! i
      integer         nzy_cts_real ! i

c     local variables
      real          factor

c     ************

      call hunt_cts ( zy_cts,nzy_cts, nzy_cts_real, h, k )
      factor =  (h-zy_cts(k)) /  (zy_cts(k+1)-zy_cts(k))
      ap = dble( exp( log(py_cts(k)) +
     @     log(py_cts(k+1)/py_cts(k)) * factor ) )
      aco2 = dlog(con(k)) + dlog( con(k+1)/con(k) ) * dble(factor)
      aco2 = exp( aco2 )
      at = dble( ty_cts(k) + (ty_cts(k+1)-ty_cts(k)) * factor )
      amr = dble( mr_cts(k) + (mr_cts(k+1)-mr_cts(k)) * factor )


      return
      end


c     **********************************************************************

      real*8 function we_clean  ( y,pl, xalsa, xalda )

c     **********************************************************************

      implicit none

      include 'nlte_paramdef.h'

c     arguments
      real*8		y       ! I. path's absorber amount * strength
      real*8          pl        ! I. path's partial pressure of CO2
      real*8          xalsa     ! I.  Self lorentz linewidth for 1 isot & 1 box
      real*8          xalda     ! I.  Doppler linewidth        "           "

c     local variables
      integer 	i
      real*8 		x,wl,wd,wvoigt
      real*8		cn(0:7),dn(0:7)
      real*8          factor, denom
      real*8          pi, pi2, sqrtpi

c     data blocks
      data cn/9.99998291698d-1,-3.53508187098d-1,9.60267807976d-2,
     @     -2.04969011013d-2,3.43927368627d-3,-4.27593051557d-4,
     @     3.42209457833d-5,-1.28380804108d-6/
      data dn/1.99999898289,5.774919878d-1,-5.05367549898d-1,
     @     8.21896973657d-1,-2.5222672453,6.1007027481,
     @     -8.51001627836,4.6535116765/

c     ***********

      pi = 3.141592
      pi2= 6.28318531
      sqrtpi = 1.77245385

      x=y / ( pi2 * xalsa*pl )


c     Lorentz
      wl=y/sqrt(1.0d0+pi*x/2.0d0)

c     Doppler
      x = y / (xalda*sqrtpi)
      if (x .lt. 5.0d0) then
         wd = cn(0)
         factor = 1.d0
         do i=1,7
            factor = factor * x
	    wd = wd + cn(i) * factor
         end do
         wd = xalda * x * sqrtpi * wd
      else
         wd = dn(0)
         factor = 1.d0 / log(x)
         denom = 1.d0
         do i=1,7
            denom = denom * factor
	    wd = wd + dn(i) * denom
         end do
         wd = xalda * sqrt(log(x)) * wd
      end if

c     Voigt
      wvoigt = wl*wl + wd*wd - (wd*wl/y)*(wd*wl/y)

      if ( wvoigt .lt. 0.0d0 ) then
       write (*,*) ' Subroutine WE/ Error in Voift EQS calculation'
       write (*,*) '  WL, WD, X, Y = ', wl, wd, x, y
       stop '  ERROR : Imaginary EQW. Revise spectral data. '
      endif

      we_clean = sqrt( wvoigt )


      return
      end


c     ***********************************************************************

      subroutine mztf_correccion (coninf, con, ib )

c     ***********************************************************************

      implicit none

      include 'nlte_paramdef.h'
      include 'nlte_commons.h'

c     arguments
      integer		ib
      real*8		con(nzy), coninf

!     local variables
      integer 	i, isot
      real*8	tvt0(nzy), tvtbs(nzy), zld(nl),zyd(nzy)
      real*8  xqv, xes, xlower, xfactor

c     *********

      isot = 1
      nu11 = dble( nu(1,1) )

      do i=1,nzy
         zyd(i) = dble(zy(i))
      enddo
      do i=1,nl
         zld(i) = dble( zl(i) )
      end do

!     tvtbs
      call interhuntdp (tvtbs,zyd,nzy, v626t1,zld,nl, 1 )

!     tvt0
      if (ib.eq.2 .or. ib.eq.3 .or. ib.eq.4) then
         call interhuntdp (tvt0,zyd,nzy, v626t1,zld,nl, 1 )
      else
         do i=1,nzy
            tvt0(i) = dble( ty(i) )
         end do
      end if

c     factor
      do i=1,nzy

         xlower = exp( ee*dble(elow(isot,ib)) *
     @        ( 1.d0/dble(ty(i))-1.d0/tvt0(i) ) )
         xes = 1.0d0
         xqv = ( 1.d0-exp( -ee*nu11/tvtbs(i) ) ) /
     @        (1.d0-exp( -ee*nu11/dble(ty(i)) ))
         xfactor = xlower * xqv**2.d0 * xes

         con(i) = con(i) * xfactor
         if (i.eq.nzy) coninf = coninf * xfactor

      end do


      return
      end


c     ***********************************************************************

      subroutine mzescape_normaliz ( taustar, istyle )

c     ***********************************************************************

      implicit none
      include 'nlte_paramdef.h'

c     arguments
      real*8 		taustar(nl) ! o
      integer         istyle    ! i

c     local variables and constants
      integer 	i, imaximum
      real*8          maximum

c     ***************

      taustar(nl) = taustar(nl-1)

      if ( istyle .eq. 1 ) then
         imaximum = nl
         maximum = taustar(nl)
         do i=1,nl-1
	    if (taustar(i).gt.maximum) taustar(i) = taustar(nl)
         enddo
      elseif ( istyle .eq. 2 ) then
         imaximum = nl
         maximum = taustar(nl)
         do i=nl-1,1,-1
	    if (taustar(i).gt.maximum) then
	       maximum = taustar(i)
	       imaximum = i
	    endif
         enddo
         do i=imaximum,nl
	    if (taustar(i).lt.maximum) taustar(i) = maximum
         enddo
      endif

      do i=1,nl
         taustar(i) = taustar(i) / maximum
      enddo


c     end
      return
      end

c     ***********************************************************************

      subroutine mzescape_normaliz_02 ( taustar, nn, istyle )

c     ***********************************************************************

      implicit none

c     arguments
      real*8 		taustar(nn) ! i,o
      integer         istyle    ! i
      integer         nn        ! i

c     local variables and constants
      integer 	i, imaximum
      real*8          maximum

c     ***************

      taustar(nn) = taustar(nn-1)

      if ( istyle .eq. 1 ) then
         imaximum = nn
         maximum = taustar(nn)
         do i=1,nn-1
	    if (taustar(i).gt.maximum) taustar(i) = taustar(nn)
         enddo
      elseif ( istyle .eq. 2 ) then
         imaximum = nn
         maximum = taustar(nn)
         do i=nn-1,1,-1
	    if (taustar(i).gt.maximum) then
	       maximum = taustar(i)
	       imaximum = i
	    endif
         enddo
         do i=imaximum,nn
	    if (taustar(i).lt.maximum) taustar(i) = maximum
         enddo
      endif

      do i=1,nn
         taustar(i) = taustar(i) / maximum
      enddo


c     end
      return
      end


c     *** interdp_ESCTVCISO_dlvr11.f ***

c***********************************************************************

      subroutine interdp_ESCTVCISO

c***********************************************************************

      implicit none

      include 'nlte_paramdef.h'
      include 'nlte_commons.h'

c     local variables
      integer    i
      real*8     lnpnb(nl)


c***********************************************************************

c     Use pressure in the NLTE grid but in log and in nb
      do i=1,nl
         lnpnb(i) = log( dble( pl(i) * 1013.25 * 1.e6) )
      enddo

c     Interpolations

      call interhuntdp3veces
     @     ( taustar21,taustar31,taustar41,    lnpnb, nl,
     @     tstar21tab,tstar31tab,tstar41tab, lnpnbtab, nztabul,
     @     1 )

      call interhuntdp3veces ( vc210,vc310,vc410, lnpnb, nl,
     @     vc210tab,vc310tab,vc410tab, lnpnbtab, nztabul, 2 )

c     end
      return
      end


c     *** hunt_cts.f ***

cccc  
      SUBROUTINE hunt_cts(xx,n,n_cts,x,jlo)  
c     
c     La dif con hunt es el uso de un indice superior (n_cts) mas bajito que (n)
c     
c     Arguments
      INTEGER jlo               ! O
      INTEGER n                 ! I
      INTEGER n_cts             ! I
      REAL  xx(n)               ! I
      REAL  x                   ! I

c     Local variables
      INTEGER inc,jhi,jm  
      LOGICAL ascnd  
c     
cccc  
c     
      ascnd=xx(n_cts).ge.xx(1)  
      if(jlo.le.0.or.jlo.gt.n_cts)then  
         jlo=0  
         jhi=n_cts+1  
         goto 3  
      endif  
      inc=1  
      if(x.ge.xx(jlo).eqv.ascnd)then  
 1       jhi=jlo+inc  
!     write (*,*) jlo
         if(jhi.gt.n_cts)then  
            jhi=n_cts+1  
!     write (*,*) jhi-1
         else if(x.ge.xx(jhi).eqv.ascnd)then  
            jlo=jhi  
            inc=inc+inc  
!     write (*,*) jlo
            goto 1  
         endif  
      else  
         jhi=jlo  
 2       jlo=jhi-inc  
!     write (*,*) jlo
         if(jlo.lt.1)then  
            jlo=0  
         else if(x.lt.xx(jlo).eqv.ascnd)then  
            jhi=jlo  
            inc=inc+inc  
            goto 2  
         endif  
      endif  
 3    if(jhi-jlo.eq.1)then  
         if(x.eq.xx(n_cts))jlo=n_cts-1  
         if(x.eq.xx(1))jlo=1  
!     write (*,*) jlo
         return  
      endif  
      jm=(jhi+jlo)/2  
      if(x.ge.xx(jm).eqv.ascnd)then  
         jlo=jm  
      else  
         jhi=jm  
      endif  
!     write (*,*) jhi-1
      goto 3  
c     
      END  

      
c     *** huntdp.f ***

cccc  
      SUBROUTINE huntdp(xx,n,x,jlo)  
c     
c     Arguments
      INTEGER jlo               ! O
      INTEGER n                 ! I
      REAL*8  xx(n)             ! I
      REAL*8  x                 ! I

c     Local variables
      INTEGER inc,jhi,jm  
      LOGICAL ascnd  
c     
cccc  
c     
      ascnd=xx(n).ge.xx(1)  
      if(jlo.le.0.or.jlo.gt.n)then  
         jlo=0  
         jhi=n+1  
         goto 3  
      endif  
      inc=1  
      if(x.ge.xx(jlo).eqv.ascnd)then  
 1       jhi=jlo+inc  
         if(jhi.gt.n)then  
            jhi=n+1  
         else if(x.ge.xx(jhi).eqv.ascnd)then  
            jlo=jhi  
            inc=inc+inc  
            goto 1  
         endif  
      else  
         jhi=jlo  
 2       jlo=jhi-inc  
         if(jlo.lt.1)then  
            jlo=0  
         else if(x.lt.xx(jlo).eqv.ascnd)then  
            jhi=jlo  
            inc=inc+inc  
            goto 2  
         endif  
      endif  
 3    if(jhi-jlo.eq.1)then  
         if(x.eq.xx(n))jlo=n-1  
         if(x.eq.xx(1))jlo=1  
         return  
      endif  
      jm=(jhi+jlo)/2  
      if(x.ge.xx(jm).eqv.ascnd)then  
         jlo=jm  
      else  
         jhi=jm  
      endif  
      goto 3  
c     
      END  

      
c     *** hunt.f ***

cccc  
      SUBROUTINE hunt(xx,n,x,jlo)  
c     
c     Arguments
      INTEGER jlo               ! O
      INTEGER n                 ! I
      REAL  xx(n)               ! I
      REAL  x                   ! I

c     Local variables
      INTEGER inc,jhi,jm  
      LOGICAL ascnd  
c     
cccc  
c     
      ascnd=xx(n).ge.xx(1)  
      if(jlo.le.0.or.jlo.gt.n)then  
         jlo=0  
         jhi=n+1  
         goto 3  
      endif  
      inc=1  
      if(x.ge.xx(jlo).eqv.ascnd)then  
 1       jhi=jlo+inc  
!     write (*,*) jlo
         if(jhi.gt.n)then  
            jhi=n+1  
!     write (*,*) jhi-1
         else if(x.ge.xx(jhi).eqv.ascnd)then  
            jlo=jhi  
            inc=inc+inc  
!     write (*,*) jlo
            goto 1  
         endif  
      else  
         jhi=jlo  
 2       jlo=jhi-inc  
!     write (*,*) jlo
         if(jlo.lt.1)then  
            jlo=0  
         else if(x.lt.xx(jlo).eqv.ascnd)then  
            jhi=jlo  
            inc=inc+inc  
            goto 2  
         endif  
      endif  
 3    if(jhi-jlo.eq.1)then  
         if(x.eq.xx(n))jlo=n-1  
         if(x.eq.xx(1))jlo=1  
!     write (*,*) jlo
         return  
      endif  
      jm=(jhi+jlo)/2  
      if(x.ge.xx(jm).eqv.ascnd)then  
         jlo=jm  
      else  
         jhi=jm  
      endif  
!     write (*,*) jhi-1
      goto 3  
c     
      END  

      
c     *** interdp_limits.f ***

c     ***********************************************************************

      subroutine interdp_limits ( yy, zz, m,   i1,i2, 
     @     y,  z, n,   j1,j2,  opt)

c     Interpolation soubroutine. 
c     Returns values between indexes i1 & i2, donde  1 =< i1 =< i2 =< m
c     Solo usan los indices de los inputs entre j1,j2, 1 =< j1 =< j2 =< n    
c     Input values: y(n) , z(n)  (solo se usarann los valores entre j1,j2)
c     zz(m) (solo se necesita entre i1,i2)
c     Output values: yy(m) (solo se calculan entre i1,i2)
c     Options:    opt=1 -> lineal ,,  opt=2 -> logarithmic
c     Difference with interdp:  
c     here interpolation proceeds between indexes i1,i2 only 
c     if i1=1 & i2=m, both subroutines are exactly the same
c     thus previous calls to interdp or interdp2 could be easily replaced

c     JAN 98 	MALV 		Version for mz1d
c     ***********************************************************************

      implicit none

!     Arguments 
      integer 	n,m             ! I. Dimensions
      integer 	i1, i2, j1, j2, opt ! I
      real*8 		zz(m)   ! I
      real*8 		yy(m)   ! O
      real*8		z(n),y(n) ! I

!     Local variables
      integer 	i,j
      real*8 		zmin,zzmin,zmax,zzmax

c     *******************************

!     write (*,*) ' d interpolating '
!     call mindp_limits (z,n,zmin, j1,j2)
!     call mindp_limits (zz,m,zzmin, i1,i2)
!     call maxdp_limits (z,n,zmax, j1,j2)
!     call maxdp_limits (zz,m,zzmax, i1,i2)
      zmin=minval(z(j1:j2))
      zzmin=minval(zz(i1:i2))
      zmax=maxval(z(j1:j2))
      zzmax=maxval(zz(i1:i2))

      if(zzmin.lt.zmin)then
         write (*,*) 'from d interp: new variable out of limits'
         write (*,*) zzmin,'must be .ge. ',zmin
         stop
!     elseif(zzmax.gt.zmax)then
!     type *,'from interp: new variable out of limits'
!     type *,zzmax, 'must be .le. ',zmax
!     stop
      end if

      do 1,i=i1,i2

         do 2,j=j1,j2-1
            if(zz(i).ge.z(j).and.zz(i).lt.z(j+1)) goto 3
 2       continue
c     in this case (zz(i2).eq.z(j2)) and j leaves the loop with j=j2-1+1=j2
         if(opt.eq.1)then
            yy(i)=y(j2-1)+(y(j2)-y(j2-1))*(zz(i)-z(j2-1))/
     $           (z(j2)-z(j2-1))
         elseif(opt.eq.2)then
            if(y(j2).eq.0.0d0.or.y(j2-1).eq.0.0d0)then
               yy(i)=0.0d0
            else
               yy(i)=exp(log(y(j2-1))+log(y(j2)/y(j2-1))*
     @              (zz(i)-z(j2-1))/(z(j2)-z(j2-1)))
            end if
         else
            write (*,*) ' d interp : opt must be 1 or 2, opt= ',opt
         end if
         goto 1
 3       continue
         if(opt.eq.1)then
            yy(i)=y(j)+(y(j+1)-y(j))*(zz(i)-z(j))/(z(j+1)-z(j))
!     type *, ' '
!     type *, ' z(j),z(j+1) =', z(j),z(j+1)
!     type *, ' t(j),t(j+1) =', y(j),y(j+1)
!     type *, ' zz, tt =  ', zz(i), yy(i)
         elseif(opt.eq.2)then
            if(y(j+1).eq.0.0d0.or.y(j).eq.0.0d0)then
               yy(i)=0.0d0
            else
               yy(i)=exp(log(y(j))+log(y(j+1)/y(j))*
     @              (zz(i)-z(j))/(z(j+1)-z(j)))
            end if
         else
            write (*,*) ' interp : opt must be 1 or 2, opt= ',opt
         end if
 1    continue
      return
      end



c     *** interhunt2veces.f ***

c     ***********************************************************************

      subroutine interhunt2veces ( y1,y2,  zz,m, 
     @     x1,x2,  z,n,  opt)

c     interpolation soubroutine basada en Numerical Recipes HUNT.FOR
c     input values: y(n) at z(n) 
c     output values: yy(m) at zz(m)
c     options: 1 -> lineal
c     2 -> logarithmic
c     ***********************************************************************

      implicit none

!     Arguments
      integer	n,m,opt         ! I
      real	zz(m),z(n)      ! I
      real    y1(m),y2(m)       ! O
      real    x1(n),x2(n)       ! I


!     Local variables 
      integer i, j 
      real    factor 
      real    zaux

!!!!  

      j = 1                     ! initial first guess (=n/2 is anothr pssblty)

      do 1,i=1,m                ! 

                                ! Busca indice j donde ocurre q zz(i) esta entre [z(j),z(j+1)]
         zaux = zz(i)
         if (abs(zaux-z(1)).le.0.01) then 
            zaux=z(1)
         elseif (abs(zaux-z(n)).le.0.01) then
            zaux=z(n)
         endif
         call hunt ( z,n, zaux, j )
         if ( j.eq.0 .or. j.eq.n ) then 
	    write (*,*) ' HUNT/ Limits input grid:', z(1),z(n)
	    write (*,*) ' HUNT/ location in new grid:', zz(i)
            stop ' interhunt2/ Interpolat error. zz out of limits.'
         endif

                                ! Perform interpolation
         factor = (zz(i)-z(j))/(z(j+1)-z(j))
         if (opt.eq.1) then
	    y1(i) = x1(j) + (x1(j+1)-x1(j)) * factor 
	    y2(i) = x2(j) + (x2(j+1)-x2(j)) * factor
         else
	    y1(i) = exp( log(x1(j)) + log(x1(j+1)/x1(j)) * factor )
	    y2(i) = exp( log(x2(j)) + log(x2(j+1)/x2(j)) * factor )
         end if

 1    continue

      return
      end


c     *** interhunt5veces.f ***

c     ***********************************************************************

      subroutine interhunt5veces ( y1,y2,y3,y4,y5,  zz,m, 
     @     x1,x2,x3,x4,x5,  z,n,  opt)

c     interpolation soubroutine basada en Numerical Recipes HUNT.FOR
c     input values: y(n) at z(n) 
c     output values: yy(m) at zz(m)
c     options: 1 -> lineal
c     2 -> logarithmic
c     ***********************************************************************

      implicit none
!     Arguments
      integer	n,m,opt         ! I
      real	zz(m),z(n)      ! I
      real    y1(m),y2(m),y3(m),y4(m),y5(m) ! O
      real    x1(n),x2(n),x3(n),x4(n),x5(n) ! I


!     Local variables 
      integer i, j 
      real    factor 
      real    zaux

!!!!  

      j = 1                     ! initial first guess (=n/2 is anothr pssblty)

      do 1,i=1,m                ! 

                                ! Busca indice j donde ocurre q zz(i) esta entre [z(j),z(j+1)]
         zaux = zz(i)
         if (abs(zaux-z(1)).le.0.01) then 
            zaux=z(1)
         elseif (abs(zaux-z(n)).le.0.01) then
            zaux=z(n)
         endif
         call hunt ( z,n, zaux, j )
         if ( j.eq.0 .or. j.eq.n ) then 
	    write (*,*) ' HUNT/ Limits input grid:', z(1),z(n)
	    write (*,*) ' HUNT/ location in new grid:', zz(i)
            stop ' interhunt5/ Interpolat error. zz out of limits.'
         endif

                                ! Perform interpolation
         factor = (zz(i)-z(j))/(z(j+1)-z(j))
         if (opt.eq.1) then
	    y1(i) = x1(j) + (x1(j+1)-x1(j)) * factor 
	    y2(i) = x2(j) + (x2(j+1)-x2(j)) * factor
	    y3(i) = x3(j) + (x3(j+1)-x3(j)) * factor
	    y4(i) = x4(j) + (x4(j+1)-x4(j)) * factor
	    y5(i) = x5(j) + (x5(j+1)-x5(j)) * factor
         else
	    y1(i) = exp( log(x1(j)) + log(x1(j+1)/x1(j)) * factor )
	    y2(i) = exp( log(x2(j)) + log(x2(j+1)/x2(j)) * factor )
	    y3(i) = exp( log(x3(j)) + log(x3(j+1)/x3(j)) * factor )
	    y4(i) = exp( log(x4(j)) + log(x4(j+1)/x4(j)) * factor )
	    y5(i) = exp( log(x5(j)) + log(x5(j+1)/x5(j)) * factor )
         end if

 1    continue

      return
      end



c     *** interhuntdp3veces.f ***

c     ***********************************************************************

      subroutine interhuntdp3veces ( y1,y2,y3, zz,m, 
     @     x1,x2,x3,  z,n,  opt)

c     interpolation soubroutine basada en Numerical Recipes HUNT.FOR
c     input values: x(n) at z(n) 
c     output values: y(m) at zz(m)
c     options: opt = 1 -> lineal
c     opt=/=1 -> logarithmic
c     ***********************************************************************
!     Arguments
      integer	n,m,opt         ! I
      real*8	zz(m),z(n)      ! I
      real*8    y1(m),y2(m),y3(m) ! O
      real*8    x1(n),x2(n),x3(n) ! I


!     Local variables 
      integer i, j 
      real*8    factor 
      real*8    zaux

!!!!  

      j = 1                     ! initial first guess (=n/2 is anothr pssblty)

      do 1,i=1,m                ! 

                                ! Busca indice j donde ocurre q zz(i) esta entre [z(j),z(j+1)]
         zaux = zz(i)
         if (abs(zaux-z(1)).le.0.01d0) then 
            zaux=z(1)
         elseif (abs(zaux-z(n)).le.0.01d0) then
            zaux=z(n)
         endif
         call huntdp ( z,n, zaux, j )
         if ( j.eq.0 .or. j.eq.n ) then 
	    write (*,*) ' HUNT/ Limits input grid:', z(1),z(n)
	    write (*,*) ' HUNT/ location in new grid:', zz(i)
            stop ' INTERHUNTDP3/ Interpolat error. zz out of limits.'
         endif

                                ! Perform interpolation
         factor = (zz(i)-z(j))/(z(j+1)-z(j))
         if (opt.eq.1) then
	    y1(i) = x1(j) + (x1(j+1)-x1(j)) * factor 
	    y2(i) = x2(j) + (x2(j+1)-x2(j)) * factor
	    y3(i) = x3(j) + (x3(j+1)-x3(j)) * factor
         else
	    y1(i) = dexp( dlog(x1(j)) + dlog(x1(j+1)/x1(j)) * factor )
	    y2(i) = dexp( dlog(x2(j)) + dlog(x2(j+1)/x2(j)) * factor )
	    y3(i) = dexp( dlog(x3(j)) + dlog(x3(j+1)/x3(j)) * factor )
         end if

 1    continue

      return
      end


c     *** interhuntdp4veces.f ***

c     ***********************************************************************

      subroutine interhuntdp4veces ( y1,y2,y3,y4, zz,m, 
     @     x1,x2,x3,x4,  z,n,  opt)

c     interpolation soubroutine basada en Numerical Recipes HUNT.FOR
c     input values: x1(n),x2(n),x3(n),x4(n) at z(n) 
c     output values: y1(m),y2(m),y3(m),y4(m) at zz(m)
c     options: 1 -> lineal
c     2 -> logarithmic
c     ***********************************************************************

      implicit none

!     Arguments
      integer	n,m,opt         ! I
      real*8	zz(m),z(n)      ! I
      real*8    y1(m),y2(m),y3(m),y4(m) ! O
      real*8    x1(n),x2(n),x3(n),x4(n) ! I


!     Local variables 
      integer i, j 
      real*8    factor 
      real*8    zaux

!!!!  

      j = 1                     ! initial first guess (=n/2 is anothr pssblty)

      do 1,i=1,m                ! 

                                ! Caza del indice j donde ocurre que zz(i) esta entre [z(j),z(j+1)]
         zaux = zz(i)
         if (abs(zaux-z(1)).le.0.01d0) then 
            zaux=z(1)
         elseif (abs(zaux-z(n)).le.0.01d0) then
            zaux=z(n)
         endif
         call huntdp ( z,n, zaux, j )
         if ( j.eq.0 .or. j.eq.n ) then 
	    write (*,*) ' HUNT/ Limits input grid:', z(1),z(n)
	    write (*,*) ' HUNT/ location in new grid:', zz(i)
            stop ' INTERHUNTDP4/ Interpolat error. zz out of limits.'
         endif

                                ! Perform interpolation
         factor = (zz(i)-z(j))/(z(j+1)-z(j))
         if (opt.eq.1) then
	    y1(i) = x1(j) + (x1(j+1)-x1(j)) * factor 
	    y2(i) = x2(j) + (x2(j+1)-x2(j)) * factor
	    y3(i) = x3(j) + (x3(j+1)-x3(j)) * factor
	    y4(i) = x4(j) + (x4(j+1)-x4(j)) * factor
         else
	    y1(i) = dexp( dlog(x1(j)) + dlog(x1(j+1)/x1(j)) * factor )
	    y2(i) = dexp( dlog(x2(j)) + dlog(x2(j+1)/x2(j)) * factor )
	    y3(i) = dexp( dlog(x3(j)) + dlog(x3(j+1)/x3(j)) * factor )
	    y4(i) = dexp( dlog(x4(j)) + dlog(x4(j+1)/x4(j)) * factor )
         end if

 1    continue

      return
      end


c     *** interhuntdp.f ***

c     ***********************************************************************

      subroutine interhuntdp ( y1, zz,m, 
     @     x1,  z,n,  opt)

c     interpolation soubroutine basada en Numerical Recipes HUNT.FOR
c     input values: x1(n) at z(n) 
c     output values: y1(m) at zz(m)
c     options: 1 -> lineal
c     2 -> logarithmic
c     ***********************************************************************

      implicit none 

!     Arguments
      integer	n,m,opt         ! I
      real*8	zz(m),z(n)      ! I
      real*8    y1(m)           ! O
      real*8    x1(n)           ! I


!     Local variables 
      integer i, j 
      real*8    factor 
      real*8    zaux

!!!!  

      j = 1                     ! initial first guess (=n/2 is anothr pssblty)

      do 1,i=1,m                ! 

                                ! Caza del indice j donde ocurre que zz(i) esta entre [z(j),z(j+1)]
         zaux = zz(i)
         if (abs(zaux-z(1)).le.0.01d0) then 
            zaux=z(1)
         elseif (abs(zaux-z(n)).le.0.01d0) then
            zaux=z(n)
         endif
         call huntdp ( z,n, zaux, j )
         if ( j.eq.0 .or. j.eq.n ) then 
	    write (*,*) ' HUNT/ Limits input grid:', z(1),z(n)
	    write (*,*) ' HUNT/ location in new grid:', zz(i)
            stop ' INTERHUNT/ Interpolat error. zz out of limits.'
         endif

                                ! Perform interpolation
         factor = (zz(i)-z(j))/(z(j+1)-z(j))
         if (opt.eq.1) then
	    y1(i) = x1(j) + (x1(j+1)-x1(j)) * factor 
         else
	    y1(i) = dexp( dlog(x1(j)) + dlog(x1(j+1)/x1(j)) * factor )
         end if

 1    continue

      return
      end


c     *** interhunt.f ***

c***********************************************************************

      subroutine interhunt ( y1, zz,m, 
     @     x1,  z,n,  opt)

c     interpolation soubroutine basada en Numerical Recipes HUNT.FOR
c     input values: x1(n) at z(n) 
c     output values: y1(m) at zz(m)
c     options: 1 -> lineal
c     2 -> logarithmic
c***********************************************************************

      implicit none

!     Arguments
      integer	n,m,opt         ! I
      real	zz(m),z(n)      ! I
      real    y1(m)             ! O
      real    x1(n)             ! I


!     Local variables 
      integer i, j 
      real    factor 
      real    zaux

!!!!  

      j = 1                     ! initial first guess (=n/2 is anothr pssblty)

      do 1,i=1,m                ! 

                                ! Busca indice j donde ocurre q zz(i) esta entre [z(j),z(j+1)]
         zaux = zz(i)
         if (abs(zaux-z(1)).le.0.01) then 
            zaux=z(1)
         elseif (abs(zaux-z(n)).le.0.01) then
            zaux=z(n)
         endif
         call hunt ( z,n, zaux, j )
         if ( j.eq.0 .or. j.eq.n ) then 
	    write (*,*) ' HUNT/ Limits input grid:', z(1),z(n)
	    write (*,*) ' HUNT/ location in new grid:', zz(i)
            stop ' interhunt/ Interpolat error. z out of limits.'
         endif

                                ! Perform interpolation
         factor = (zz(i)-z(j))/(z(j+1)-z(j))
         if (opt.eq.1) then
	    y1(i) = x1(j) + (x1(j+1)-x1(j)) * factor 
         else
	    y1(i) = exp( log(x1(j)) + log(x1(j+1)/x1(j)) * factor )
         end if


 1    continue

      return
      end


c     *** interhuntlimits2veces.f ***

c***********************************************************************

      subroutine interhuntlimits2veces 
     @     ( y1,y2, zz,m, limite1,limite2, 
     @     x1,x2,  z,n, opt)

c     Interpolation soubroutine basada en Numerical Recipes HUNT.FOR
c     Input values:  x1,x2(n) at z(n) 
c     Output values: 
c     y1,y2(m) at zz(m)   pero solo entre los indices de zz
c     siguientes: [limite1,limite2]
c     Options: 1 -> linear in z and linear in x
c     2 -> linear in z and logarithmic in x
c     3 -> logarithmic in z and linear in x
c     4 -> logarithmic in z and logaritmic in x
c     
c     NOTAS: Esta subrutina extiende y generaliza la usual  
c     "interhunt5veces" en 2 direcciones: 
c     - la condicion en los limites es que zz(limite1:limite2)
c     esté dentro de los limites de z (pero quizas no todo zz)
c     - se han añadido 3 opciones mas al caso de interpolacion
c     logaritmica, ahora se hace en log de z, de x o de ambos.
c     Notese que esta subrutina engloba a la interhunt5veces 
c     ( esta es reproducible haciendo  limite1=1  y  limite2=m 
c     y usando una de las 2 primeras opciones  opt=1,2 )
c***********************************************************************

      implicit none

!     Arguments
      integer	n,m,opt, limite1,limite2 ! I
      real	zz(m),z(n)      ! I
      real    y1(m),y2(m)       ! O
      real    x1(n),x2(n)       ! I


!     Local variables 
      integer i, j 
      real    factor 
      real    zaux

!!!!  

      j = 1                     ! initial first guess (=n/2 is anothr pssblty)

      do 1,i=limite1,limite2              

                                ! Busca indice j donde ocurre q zz(i) esta entre [z(j),z(j+1)]
         zaux = zz(i)
         if (abs(zaux-z(1)).le.0.01) then 
            zaux=z(1)
         elseif (abs(zaux-z(n)).le.0.01) then
            zaux=z(n)
         endif
         call hunt ( z,n, zaux, j )
         if ( j.eq.0 .or. j.eq.n ) then 
	    write (*,*) ' HUNT/ Limits input grid:', z(1),z(n)
	    write (*,*) ' HUNT/ location in new grid:', zz(i)
            stop ' interhuntlimits/ Interpolat error. z out of limits.'
         endif

                                ! Perform interpolation
         if (opt.eq.1) then
	    factor = (zz(i)-z(j))/(z(j+1)-z(j))
	    y1(i) = x1(j) + (x1(j+1)-x1(j)) * factor 
	    y2(i) = x2(j) + (x2(j+1)-x2(j)) * factor 

         elseif (opt.eq.2) then 
	    factor = (zz(i)-z(j))/(z(j+1)-z(j))
	    y1(i) = exp( log(x1(j)) + log(x1(j+1)/x1(j)) * factor )
	    y2(i) = exp( log(x2(j)) + log(x2(j+1)/x2(j)) * factor )
         elseif (opt.eq.3) then 
	    factor = (log(zz(i))-log(z(j)))/(log(z(j+1))-log(z(j)))
	    y1(i) = x1(j) + (x1(j+1)-x1(j)) * factor 
	    y2(i) = x2(j) + (x2(j+1)-x2(j)) * factor 
         elseif (opt.eq.4) then 
	    factor = (log(zz(i))-log(z(j)))/(log(z(j+1))-log(z(j)))
	    y1(i) = exp( log(x1(j)) + log(x1(j+1)/x1(j)) * factor )
	    y2(i) = exp( log(x2(j)) + log(x2(j+1)/x2(j)) * factor )
         end if


 1    continue

      return
      end


c     *** interhuntlimits5veces.f ***

c***********************************************************************

      subroutine interhuntlimits5veces 
     @     ( y1,y2,y3,y4,y5, zz,m, limite1,limite2, 
     @     x1,x2,x3,x4,x5,  z,n, opt)

c     Interpolation soubroutine basada en Numerical Recipes HUNT.FOR
c     Input values:  x1,x2,..,x5(n) at z(n) 
c     Output values: 
c     y1,y2,...,y5(m) at zz(m)   pero solo entre los indices de zz
c     siguientes: [limite1,limite2]
c     Options: 1 -> linear in z and linear in x
c     2 -> linear in z and logarithmic in x
c     3 -> logarithmic in z and linear in x
c     4 -> logarithmic in z and logaritmic in x
c     
c     NOTAS: Esta subrutina extiende y generaliza la usual  
c     "interhunt5veces" en 2 direcciones: 
c     - la condicion en los limites es que zz(limite1:limite2)
c     esté dentro de los limites de z (pero quizas no todo zz)
c     - se han añadido 3 opciones mas al caso de interpolacion
c     logaritmica, ahora se hace en log de z, de x o de ambos.
c     Notese que esta subrutina engloba a la interhunt5veces 
c     ( esta es reproducible haciendo  limite1=1  y  limite2=m 
c     y usando una de las 2 primeras opciones  opt=1,2 )
c***********************************************************************

      implicit none

!     Arguments
      integer	n,m,opt, limite1,limite2 ! I
      real	zz(m),z(n)      ! I
      real    y1(m),y2(m),y3(m),y4(m),y5(m) ! O
      real    x1(n),x2(n),x3(n),x4(n),x5(n) ! I


!     Local variables 
      integer i, j 
      real    factor 
      real    zaux

!!!!  

      j = 1                     ! initial first guess (=n/2 is anothr pssblty)

      do 1,i=limite1,limite2              

                                ! Busca indice j donde ocurre q zz(i) esta entre [z(j),z(j+1)]
         zaux = zz(i)
         if (abs(zaux-z(1)).le.0.01) then 
            zaux=z(1)
         elseif (abs(zaux-z(n)).le.0.01) then
            zaux=z(n)
         endif
         call hunt ( z,n, zaux, j )
         if ( j.eq.0 .or. j.eq.n ) then 
	    write (*,*) ' HUNT/ Limits input grid:', z(1),z(n)
	    write (*,*) ' HUNT/ location in new grid:', zz(i)
            stop ' interhuntlimits/ Interpolat error. z out of limits.'
         endif

                                ! Perform interpolation
         if (opt.eq.1) then
	    factor = (zz(i)-z(j))/(z(j+1)-z(j))
	    y1(i) = x1(j) + (x1(j+1)-x1(j)) * factor 
	    y2(i) = x2(j) + (x2(j+1)-x2(j)) * factor 
	    y3(i) = x3(j) + (x3(j+1)-x3(j)) * factor 
	    y4(i) = x4(j) + (x4(j+1)-x4(j)) * factor 
	    y5(i) = x5(j) + (x5(j+1)-x5(j)) * factor 
         elseif (opt.eq.2) then 
	    factor = (zz(i)-z(j))/(z(j+1)-z(j))
	    y1(i) = exp( log(x1(j)) + log(x1(j+1)/x1(j)) * factor )
	    y2(i) = exp( log(x2(j)) + log(x2(j+1)/x2(j)) * factor )
	    y3(i) = exp( log(x3(j)) + log(x3(j+1)/x3(j)) * factor )
	    y4(i) = exp( log(x4(j)) + log(x4(j+1)/x4(j)) * factor )
	    y5(i) = exp( log(x5(j)) + log(x5(j+1)/x5(j)) * factor )
         elseif (opt.eq.3) then 
	    factor = (log(zz(i))-log(z(j)))/(log(z(j+1))-log(z(j)))
	    y1(i) = x1(j) + (x1(j+1)-x1(j)) * factor 
	    y2(i) = x2(j) + (x2(j+1)-x2(j)) * factor 
	    y3(i) = x3(j) + (x3(j+1)-x3(j)) * factor 
	    y4(i) = x4(j) + (x4(j+1)-x4(j)) * factor 
	    y5(i) = x5(j) + (x5(j+1)-x5(j)) * factor 
         elseif (opt.eq.4) then 
	    factor = (log(zz(i))-log(z(j)))/(log(z(j+1))-log(z(j)))
	    y1(i) = exp( log(x1(j)) + log(x1(j+1)/x1(j)) * factor )
	    y2(i) = exp( log(x2(j)) + log(x2(j+1)/x2(j)) * factor )
	    y3(i) = exp( log(x3(j)) + log(x3(j+1)/x3(j)) * factor )
	    y4(i) = exp( log(x4(j)) + log(x4(j+1)/x4(j)) * factor )
	    y5(i) = exp( log(x5(j)) + log(x5(j+1)/x5(j)) * factor )
         end if


 1    continue

      return
      end



c     *** interhuntlimits.f ***

c***********************************************************************

      subroutine interhuntlimits ( y, zz,m, limite1,limite2, 
     @     x,  z,n, opt)

c     Interpolation soubroutine basada en Numerical Recipes HUNT.FOR
c     Input values:  x(n) at z(n) 
c     Output values: y(m) at zz(m)   pero solo entre los indices de zz
c     siguientes: [limite1,limite2]
c     Options: 1 -> linear in z and linear in x
c     2 -> linear in z and logarithmic in x
c     3 -> logarithmic in z and linear in x
c     4 -> logarithmic in z and logaritmic in x
c     
c     NOTAS: Esta subrutina extiende y generaliza la usual  "interhunt"
c     en 2 direcciones: 
c     - la condicion en los limites es que zz(limite1:limite2)
c     esté dentro de los limites de z (pero quizas no todo zz)
c     - se han añadido 3 opciones mas al caso de interpolacion
c     logaritmica, ahora se hace en log de z, de x o de ambos.
c     Notese que esta subrutina engloba a la usual interhunt 
c     ( esta es reproducible haciendo  limite1=1  y  limite2=m 
c     y usando una de las 2 primeras opciones  opt=1,2 )
c***********************************************************************

      implicit none

!     Arguments
      integer	n,m,opt, limite1,limite2 ! I
      real	zz(m),z(n)      ! I
      real    y(m)              ! O
      real    x(n)              ! I


!     Local variables 
      integer i, j 
      real    factor 
      real    zaux

!!!!  

      j = 1                     ! initial first guess (=n/2 is anothr pssblty)

      do 1,i=limite1,limite2              

                                ! Busca indice j donde ocurre q zz(i) esta entre [z(j),z(j+1)]
         zaux = zz(i)
         if (abs(zaux-z(1)).le.0.01) then 
            zaux=z(1)
         elseif (abs(zaux-z(n)).le.0.01) then
            zaux=z(n)
         endif
         call hunt ( z,n, zaux, j )
         if ( j.eq.0 .or. j.eq.n ) then 
	    write (*,*) ' HUNT/ Limits input grid:', z(1),z(n)
	    write (*,*) ' HUNT/ location in new grid:', zz(i)
            stop ' interhuntlimits/ Interpolat error. z out of limits.'
         endif

                                ! Perform interpolation
         if (opt.eq.1) then
	    factor = (zz(i)-z(j))/(z(j+1)-z(j))
	    y(i) = x(j) + (x(j+1)-x(j)) * factor 
         elseif (opt.eq.2) then 
	    factor = (zz(i)-z(j))/(z(j+1)-z(j))
	    y(i) = exp( log(x(j)) + log(x(j+1)/x(j)) * factor )
         elseif (opt.eq.3) then 
	    factor = (log(zz(i))-log(z(j)))/(log(z(j+1))-log(z(j)))
	    y(i) = x(j) + (x(j+1)-x(j)) * factor 
         elseif (opt.eq.4) then 
	    factor = (log(zz(i))-log(z(j)))/(log(z(j+1))-log(z(j)))
	    y(i) = exp( log(x(j)) + log(x(j+1)/x(j)) * factor )
         end if


 1    continue

      return
      end


c     *** lubksb_dp.f ***

      subroutine lubksb_dp(a,n,np,indx,b)     

      implicit none

      integer,intent(in) :: n,np
      real*8,intent(in) :: a(np,np) 
      integer,intent(in) :: indx(n)
      real*8,intent(out) :: b(n) 

      real*8 sum
      integer ii, ll, i, j

      ii=0            
      do 12 i=1,n           
         ll=indx(i)          
         sum=b(ll)           
         b(ll)=b(i)          
         if (ii.ne.0)then    
            do 11 j=ii,i-1        
               sum=sum-a(i,j)*b(j)     
 11         continue                  
         else if (sum.ne.0.0) then  
            ii=i                      
         endif                       
         b(i)=sum                    
 12   continue                      
      do 14 i=n,1,-1                
         sum=b(i)                    
         if(i.lt.n)then              
            do 13 j=i+1,n             
               sum=sum-a(i,j)*b(j)     
 13         continue                  
         endif                       
         b(i)=sum/a(i,i)             
 14   continue                      
      return   
      end      


c     *** ludcmp_dp.f ***

      subroutine ludcmp_dp(a,n,np,indx,d)

      implicit none

      integer,intent(in) :: n, np
      real*8,intent(inout) :: a(np,np)
      real*8,intent(out) :: d
      integer,intent(out) :: indx(n)
      
      integer nmax, i, j, k, imax
      real*8 tiny
      parameter (nmax=100,tiny=1.0d-20)   
      real*8 vv(nmax), aamax, sum, dum


      d=1.0d0
      do 12 i=1,n             
         aamax=0.0d0
         do 11 j=1,n           
            if (abs(a(i,j)).gt.aamax) aamax=abs(a(i,j))             
 11      continue              
         if (aamax.eq.0.0) then
            write(*,*) 'ludcmp_dp: singular matrix!'
            stop
         endif            
         vv(i)=1.0d0/aamax  
 12   continue                
      do 19 j=1,n             
         if (j.gt.1) then      
            do 14 i=1,j-1       
               sum=a(i,j)        
               if (i.gt.1)then               
                  do 13 k=1,i-1               
                     sum=sum-a(i,k)*a(k,j)     
 13               continue    
                  a(i,j)=sum      
               endif             
 14         continue            
         endif                 
         aamax=0.0d0           
         do 16 i=j,n           
            sum=a(i,j)          
            if (j.gt.1)then     
               do 15 k=1,j-1                     
                  sum=sum-a(i,k)*a(k,j)                     
 15            continue              
               a(i,j)=sum            
            endif                   
            dum=vv(i)*abs(sum)      
            if (dum.ge.aamax) then  
               imax=i                
               aamax=dum             
            endif                   
 16      continue                  
         if (j.ne.imax)then        
            do 17 k=1,n             
               dum=a(imax,k)         
               a(imax,k)=a(j,k)      
               a(j,k)=dum            
 17         continue                
            d=-d                    
            vv(imax)=vv(j)          
         endif                     
         indx(j)=imax              
         if(j.ne.n)then            
            if(a(j,j).eq.0.0)a(j,j)=tiny               
            dum=1.0d0/a(j,j)      
            do 18 i=j+1,n           
               a(i,j)=a(i,j)*dum     
 18         continue                
         endif                     
 19   continue                    
      if(a(n,n).eq.0.0)a(n,n)=tiny               
      return                      
      end   


c     *** LUdec.f ***

ccccccccccccccccccccccccccccccccccccccccccccccccccccccccc
c     
c     Solution of linear equation without inverting matrix
c     using LU decomposition: 
c     AA * xx = bb         AA, bb: known
c     xx: to be found
c     AA and bb are not modified in this subroutine
c     
c     MALV , Sep 2007
ccccccccccccccccccccccccccccccccccccccccccccccccccccccccc

      subroutine LUdec(xx,aa,bb,m,n)

      implicit none

!     Arguments 
      integer,intent(in) ::     m, n
      real*8,intent(in) ::      aa(m,m), bb(m)
      real*8,intent(out) ::     xx(m)


!     Local variables
      real*8      a(n,n), b(n), x(n), d
      integer    i, j, indx(n)      


!     Subrutinas utilizadas
!     ludcmp_dp, lubksb_dp

!!!!!!!!!!!!!!!Comienza el programa !!!!!!!!!!!!!!
      
      do i=1,n
         b(i) = bb(i+1)
         do j=1,n
            a(i,j) = aa(i+1,j+1)
         enddo
      enddo

                                ! Descomposicion de auxm1
      call ludcmp_dp ( a, n, n, indx, d)

                                ! Sustituciones foward y backwards para hallar la solucion
      do i=1,n
         x(i) = b(i)
      enddo
      call lubksb_dp( a, n, n, indx, x )

      do i=1,n
         xx(i+1) = x(i)
      enddo

      return
      end


c     *** mat_oper.f ***

ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc

c     ***********************************************************************
      subroutine unit(a,n)
c     store the unit value in the diagonal of a 
c     ***********************************************************************
      implicit none
      real*8 a(n,n)
      integer n,i,j,k
      do 1,i=2,n-1
         do 2,j=2,n-1
	    if(i.eq.j) then
               a(i,j) = 1.d0
	    else
               a(i,j)=0.0d0
	    end if
 2       continue
 1    continue
      do k=1,n
         a(n,k) = 0.0d0
         a(1,k) = 0.0d0
         a(k,1) = 0.0d0
         a(k,n) = 0.0d0
      end do
      return
      end

c     ***********************************************************************
      subroutine diago(a,v,n)
c     store the vector v in the diagonal elements of the square matrix a
c     ***********************************************************************
      implicit none

      integer n,i,j,k
      real*8 a(n,n),v(n)

      do 1,i=2,n-1
         do 2,j=2,n-1
	    if(i.eq.j) then
               a(i,j) = v(i)
	    else
               a(i,j)=0.0d0
	    end if
 2       continue
 1    continue
      do k=1,n
         a(n,k) = 0.0d0
         a(1,k) = 0.0d0
         a(k,1) = 0.0d0
         a(k,n) = 0.0d0
      end do
      return
      end  

c     ***********************************************************************
      subroutine invdiag(a,b,n)
c     inverse of a diagonal matrix 
c     ***********************************************************************
      implicit none

      integer n,i,j,k
      real*8 a(n,n),b(n,n)

      do 1,i=2,n-1
         do 2,j=2,n-1
	    if (i.eq.j) then
               a(i,j) = 1.d0/b(i,i)
	    else
               a(i,j)=0.0d0
	    end if
 2       continue
 1    continue
      do k=1,n
         a(n,k) = 0.0d0
         a(1,k) = 0.0d0
         a(k,1) = 0.0d0
         a(k,n) = 0.0d0
      end do
      return
      end


c     ***********************************************************************
      subroutine samem (a,m,n)
c     store the matrix m in the matrix a 
c     ***********************************************************************
      implicit none
      real*8 a(n,n),m(n,n)
      integer n,i,j,k
      do 1,i=2,n-1
         do 2,j=2,n-1
            a(i,j) = m(i,j)  
 2       continue
 1    continue 	
      do k=1,n
         a(n,k) = 0.0d0
         a(1,k) = 0.0d0
         a(k,1) = 0.0d0
         a(k,n) = 0.0d0
      end do
      return
      end 


c     ***********************************************************************
      subroutine mulmv(a,b,c,n)
c     do a(i)=b(i,j)*c(j). a, b, and c must be distint
c     ***********************************************************************
      implicit none

      integer n,i,j
      real*8 a(n),b(n,n),c(n),sum

      do 1,i=2,n-1
         sum=0.0d0
         do 2,j=2,n-1
	    sum = sum + b(i,j) * c(j)
 2       continue
         a(i)=sum
 1    continue
      a(1) = 0.0d0
      a(n) = 0.0d0
      return
      end


c     ***********************************************************************
      subroutine trucodiag(a,b,c,d,e,n)
c     inputs: matrices b,c,d,e
c     output: matriz diagonal a
c     Operacion a realizar:  a = b * c^(-1) * d + e
c     La matriz c va a ser invertida
c     Todas las matrices de entrada son diagonales excepto b
c     Aprovechamos esa condicion para invertir c, acelerar el calculo, y 
c     ademas, para forzar que a sea diagonal
c     ***********************************************************************
      implicit none
      real*8 a(n,n),b(n,n),c(n,n),d(n,n),e(n,n), sum
      integer n,i,j,k
      do 1,i=2,n-1
         sum=0.0d0
         do 2,j=2,n-1
	    sum=sum+ b(i,j) * d(j,j)/c(j,j)
 2       continue
         a(i,i) = sum + e(i,i)
 1    continue
      do k=1,n
         a(n,k) = 0.0d0
         a(1,k) = 0.0d0
         a(k,1) = 0.0d0
         a(k,n) = 0.0d0
      end do
      return
      end


c     ***********************************************************************
      subroutine trucommvv(v,b,c,u,w,n)
c     inputs: matrices b,c , vectores u,w
c     output: vector v 
c     Operacion a realizar:  v = b * c^(-1) * u + w
c     La matriz c va a ser invertida 
c     c es diagonal, b no
c     Aprovechamos esa condicion para invertir c, y acelerar el calculo
c     ***********************************************************************
      implicit none
      real*8 v(n),b(n,n),c(n,n),u(n),w(n), sum
      integer n,i,j
      do 1,i=2,n-1
         sum=0.0d0
         do 2,j=2,n-1
	    sum=sum+ b(i,j) * u(j)/c(j,j)
 2       continue
         v(i) = sum + w(i)
 1    continue
      v(1) = 0.d0
      v(n) = 0.d0
      return
      end


c     ***********************************************************************
      subroutine sypvmv(v,u,c,w,n)
c     inputs: matriz diagonal c , vectores u,w
c     output: vector v 
c     Operacion a realizar:  v = u + c * w 
c     ***********************************************************************
      implicit none
      real*8 v(n),u(n),c(n,n),w(n)
      integer n,i
      do 1,i=2,n-1
         v(i)= u(i) + c(i,i) * w(i)
 1    continue
      v(1) = 0.0d0
      v(n) = 0.0d0
      return
      end


c     ***********************************************************************
      subroutine sumvv(a,b,c,n)
c     a(i)=b(i)+c(i)
c     ***********************************************************************
      implicit none

      integer n,i
      real*8 a(n),b(n),c(n)

      do 1,i=2,n-1
         a(i)= b(i) + c(i)
 1    continue
      a(1) = 0.0d0
      a(n) = 0.0d0
      return
      end


c     ***********************************************************************
      subroutine sypvvv(a,b,c,d,n)
c     a(i)=b(i)+c(i)*d(i)
c     ***********************************************************************
      implicit none
      real*8 a(n),b(n),c(n),d(n)
      integer n,i
      do 1,i=2,n-1
         a(i)= b(i) + c(i) * d(i)
 1    continue
      a(1) = 0.0d0
      a(n) = 0.0d0
      return
      end


c     ***********************************************************************
!      subroutine zerom(a,n)
c     a(i,j)= 0.0
c     ***********************************************************************
!      implicit none
!      integer n,i,j
!      real*8 a(n,n)

!      do 1,i=1,n
!         do 2,j=1,n
!	    a(i,j) = 0.0d0
! 2       continue
! 1    continue
!      return
!      end


c     ***********************************************************************
      subroutine zero4m(a,b,c,d,n)
c     a(i,j) = b(i,j) = c(i,j) = d(i,j) = 0.0 
c     ***********************************************************************
      implicit none
      real*8 a(n,n), b(n,n), c(n,n), d(n,n)
      integer n
      a(1:n,1:n)=0.d0
      b(1:n,1:n)=0.d0
      c(1:n,1:n)=0.d0
      d(1:n,1:n)=0.d0
!      do 1,i=1,n
!         do 2,j=1,n
!	    a(i,j) = 0.0d0
!	    b(i,j) = 0.0d0
!	    c(i,j) = 0.0d0
!	    d(i,j) = 0.0d0
! 2       continue
! 1    continue
      return
      end


c     ***********************************************************************
      subroutine zero3m(a,b,c,n)
c     a(i,j) = b(i,j) = c(i,j) = 0.0 
c     **********************************************************************
      implicit none
      real*8 a(n,n), b(n,n), c(n,n)
      integer n
      a(1:n,1:n)=0.d0
      b(1:n,1:n)=0.d0
      c(1:n,1:n)=0.d0
!      do 1,i=1,n
!         do 2,j=1,n
!	    a(i,j) = 0.0d0
!	    b(i,j) = 0.0d0
!	    c(i,j) = 0.0d0
! 2       continue
! 1    continue
      return
      end


c     ***********************************************************************
      subroutine zero2m(a,b,n)
c     a(i,j) = b(i,j) = 0.0 
c     ***********************************************************************
      implicit none
      real*8 a(n,n), b(n,n)
      integer n
      a(1:n,1:n)=0.d0
      b(1:n,1:n)=0.d0
!      do 1,i=1,n
!         do 2,j=1,n
!	    a(i,j) = 0.0d0
!	    b(i,j) = 0.0d0
! 2       continue
! 1    continue
      return
      end


c     ***********************************************************************
!      subroutine zerov(a,n)
c     a(i)= 0.0
c     ***********************************************************************
!      implicit none
!      real*8 a(n)
!      integer n,i
!      do 1,i=1,n
!         a(i) = 0.0d0
! 1    continue
!      return
!      end


c     ***********************************************************************
      subroutine zero4v(a,b,c,d,n)
c     a(i) = b(i) = c(i) = d(i,j) = 0.0
c     ***********************************************************************
      implicit none
      real*8 a(n), b(n), c(n), d(n)
      integer n
      a(1:n)=0.d0
      b(1:n)=0.d0
      c(1:n)=0.d0
      d(1:n)=0.d0
!      do 1,i=1,n
!         a(i) = 0.0d0
!         b(i) = 0.0d0
!         c(i) = 0.0d0
!         d(i) = 0.0d0
! 1    continue
      return
      end


c     ***********************************************************************
      subroutine zero3v(a,b,c,n)
c     a(i) = b(i) = c(i) = 0.0
c     ***********************************************************************
      implicit none
      real*8 a(n), b(n), c(n) 
      integer n
      a(1:n)=0.d0
      b(1:n)=0.d0
      c(1:n)=0.d0
!      do 1,i=1,n
!         a(i) = 0.0d0
!         b(i) = 0.0d0
!         c(i) = 0.0d0
! 1    continue
      return
      end


c     ***********************************************************************
      subroutine zero2v(a,b,n)
c     a(i) = b(i) = 0.0
c     ***********************************************************************
      implicit none
      real*8 a(n), b(n) 
      integer n
      a(1:n)=0.d0
      b(1:n)=0.d0
!      do 1,i=1,n
!         a(i) = 0.0d0
!         b(i) = 0.0d0
! 1    continue
      return
      end

c     ***********************************************************************


c****************************************************************************

c     *** suaviza.f ***

c*****************************************************************************
c     
      subroutine suaviza ( x, n, ismooth, y )
c     
c     x - input and return values 
c     y - auxiliary vector
c     ismooth = 0  --> no smoothing is performed
c     ismooth = 1  --> weak smoothing (5 points, centred weighted)
c     ismooth = 2  --> normal smoothing (3 points, evenly weighted)
c     ismooth = 3  --> strong smoothing (5 points, evenly weighted)


c     august 1991
c*****************************************************************************

      implicit none

      integer	n, imax, imin, i, ismooth
      real*8	x(n), y(n)
c*****************************************************************************

      imin=1
      imax=n

      if (ismooth.eq.0) then

         return 

      elseif (ismooth.eq.1) then ! 5 points, with central weighting 

         do i=imin,imax 
	    if(i.eq.imin)then
               y(i)=x(imin)
	    elseif(i.eq.imax)then
               y(i)=x(imax-1)+(x(imax-1)-x(imax-3))/2.d0
	    elseif(i.gt.(imin+1) .and. i.lt.(imax-1) )then
               y(i) = ( x(i+2)/4.d0 + x(i+1)/2.d0 + 2.d0*x(i)/3.d0 + 
     @              x(i-1)/2.d0 + x(i-2)/4.d0 )* 6.d0/13.d0
	    else
               y(i)=(x(i+1)/2.d0+x(i)+x(i-1)/2.d0)/2.d0
	    end if
         end do	

      elseif (ismooth.eq.2) then ! 3 points, evenly spaced

         do i=imin,imax 
	    if(i.eq.imin)then
               y(i)=x(imin)
	    elseif(i.eq.imax)then
               y(i)=x(imax-1)+(x(imax-1)-x(imax-3))/2.d0
	    else
               y(i) = ( x(i+1)+x(i)+x(i-1) )/3.d0
	    end if
         end do	
         
      elseif (ismooth.eq.3) then ! 5 points, evenly spaced

         do i=imin,imax 
	    if(i.eq.imin)then
               y(i) = x(imin)
	    elseif(i.eq.(imin+1) .or. i.eq.(imax-1))then
               y(i) = ( x(i+1)+x(i)+x(i-1) )/3.d0 
	    elseif(i.eq.imax)then
               y(i) = ( x(imax-1) + x(imax-1) + x(imax-2) ) / 3.d0
	    else
               y(i) = ( x(i+2)+x(i+1)+x(i)+x(i-1)+x(i-2) )/5.d0
	    end if
         end do	

      else

         write (*,*) ' Error in suaviza.f   Wrong ismooth value.'
         stop

      endif

c     rehago el cambio, para devolver x(i)
      do i=imin,imax 
         x(i)=y(i)
      end do

      return
      end


c     ***********************************************************************
      subroutine mulmmf90(a,b,c,n)
c     ***********************************************************************
      implicit none
      real*8 a(n,n), b(n,n), c(n,n)
      integer n

      a=matmul(b,c)
      a(1,:)=0.d0
      a(:,1)=0.d0
      a(n,:)=0.d0
      a(:,n)=0.d0

      return
      end


c     ***********************************************************************
      subroutine resmmf90(a,b,c,n)
c     ***********************************************************************
      implicit none
      real*8 a(n,n), b(n,n), c(n,n)
      integer n

      a=b-c
      a(1,:)=0.d0
      a(:,1)=0.d0
      a(n,:)=0.d0
      a(:,n)=0.d0

      return
      end


c*******************************************************************

      subroutine gethist_03 (ihist) 

c*******************************************************************

      implicit none

      include	'nlte_paramdef.h'
      include	'nlte_commons.h'


c     arguments 
      integer         ihist

c     local variables
      integer 	      j, r

c     ***************

      nbox = nbox_stored(ihist)
      do j=1,mm_stored(ihist)
         thist(j) = thist_stored(ihist,j) 
         do r=1,nbox_stored(ihist)
	    no(r) = no_stored(ihist,r) 
            sk1(j,r) = sk1_stored(ihist,j,r) 
            xls1(j,r) = xls1_stored(ihist,j,r)
            xld1(j,r) = xld1_stored(ihist,j,r)
         enddo
      enddo


      return
      end


c     *******************************************************************

      subroutine rhist_03 (ihist) 

c     *******************************************************************

      implicit none

      include	'nlte_paramdef.h'
      include	'nlte_commons.h'


c     arguments 
      integer         ihist

c     local variables
      integer 	      j, r
      real*8          xx

c     ***************

      open(unit=3,file=hisfile,status='old')

      read(3,*)
      read(3,*)
      read(3,*) mm_stored(ihist)
      read(3,*)
      read(3,*) nbox_stored(ihist)
      read(3,*)

      if ( nbox_stored(ihist) .gt. nbox_max ) then
         write (*,*) ' nbox too large in input file ', hisfile
         stop ' Check maximum number nbox_max in mz1d.par '
      endif

      do j=mm_stored(ihist),1,-1
         read(3,*) thist_stored(ihist,j) 
         do r=1,nbox_stored(ihist)
	    read(3,*) no_stored(ihist,r), 
     &           sk1_stored(ihist,j,r), 
     &           xls1_stored(ihist,j,r),
     &           xx,
     &           xld1_stored(ihist,j,r)
         enddo

      enddo

      close(unit=3)


      return
      end
