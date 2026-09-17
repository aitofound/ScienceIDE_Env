!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
! Fast scheme for NLTE cooling rates at 15um by CO2 in a Martian GCM !
!                 Version dlvr11_03. 2012.                           !
! Software written and provided by IAA/CSIC, Granada, Spain,         !
! under ESA contract "Mars Climate Database and Physical Models"     !
! Person of contact: Miguel Angel Lopez Valverde  valverde@iaa.es    !
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
c**********************************************************************
c     
c     Includes the following 1-d model subroutines:
c     
c     -MZESC110_dlvr11_03.f
c     -MZTUD110_dlvr11_03.f
c     -MZMC121_dlvr11_03.f
c     -MZTUD121_dlvr11_03.f
c     -MZESC121_dlvr11_03.f
c     -MZESC121sub_dlvr11_03.f
c     -MZTVC121_dlvr11.f
c     -MZTVC121sub_dlvr11_03.f



c     *** Old MZESC110_dlvr11_03.f

c**********************************************************************

c***********************************************************************
      subroutine MZESC110 (ig,nl_cts_real, nzy_cts_real,ierr,varerr)
c***********************************************************************

      implicit none

      include 'datafile.h'
      include 'nlte_paramdef.h'
      include 'nlte_commons.h'

c     arguments 
      integer     nl_cts_real, nzy_cts_real ! i
      integer     ig

c     old arguments
      integer         ierr      ! o
      real*8          varerr    ! o

c     local variables and constants
      integer 	i, iaquiHIST , iaquiZ
      integer   isot
      real*8    argumento
      real*8 	tauinf(nl_cts)
      real*8 	con(nzy_cts), coninf
      real*8 	c1, c2 , ccc
      real*8 	t1, t2
      real*8 	p1, p2
      real*8	mr1, mr2
      real*8 	st1, st2
      real*8 	c1box(nbox_max), c2box(nbox_max)
      real*8	ff      ! to avoid too small numbers
      real*8 	st, beta, ts
      real*8  	tyd(nzy_cts)
      real*8 	correc
      real*8	deltanudbl, deltazdbl
      real*8    yy

c     external function
      external  we_clean
      real*8    we_clean

c***********************************************************************
      ierr = 0
      varerr = 0.d0
c     
      beta = 1.8d5
      ibcode1 = '1'
      isot = 1
      deltanudbl = dble(deltanu(1,1))
      deltazdbl = dble(deltaz_cts)
      ff=1.0d10

ccc   
      do i=1,nzy_cts_real
         tyd(i) = dble(ty_cts(i))
         con(i) =  dble( co2y_cts(i) * imr(isot) )
         correc = 2.d0 * dexp( -ee*dble(elow(isot,2))/tyd(i) )
         con(i) = con(i) * ( 1.d0 - correc )
         mr_cts(i) = dble(co2y_cts(i)/nty_cts(i))
      end do
      if ( con(nzy_cts_real) .le. 0.0d0 ) then
         ierr = 33
         varerr = con(nzy_cts_real)
         return
      elseif ( con(nzy_cts_real-1) .le. con(nzy_cts_real) ) then
         write (*,*) ' WARNING in MZESC110 '
         write (*,*) '    [CO2] growing with altitude at TOA.'
         write (*,*) '    [CO2] @ TOA = ', con(nzy_cts_real)
         coninf = dble( con(nzy_cts_real) )
      else
         coninf = dble( con(nzy_cts_real) /
     @        log( con(nzy_cts_real-1) / con(nzy_cts_real) ) )
      endif
ccc   
      call gethist_03 ( 1 )

c     
c     tauinf
c     
      call initial

      iaquiHIST = nhist/2
      iaquiZ = nzy_cts_real - 2

      do i=nl_cts_real,1,-1

         if(i.eq.nl_cts_real)then

            call intzhunt_cts (iaquiZ, zl_cts(i), nzy_cts_real, 
     @           c2,p2,mr2,t2, con)
            do kr=1,nbox
               ta(kr)=t2
	    end do
            call interstrhunt (iaquiHIST, st2,t2,ka,ta)
                                ! Check interpolation errors :
            if (c2.le.0.0d0) then
               ierr=15
               varerr=c2
               return
       	    elseif (p2.le.0.0d0) then
               ierr=16
               varerr=p2
               return
	    elseif (mr2.le.0.0d0) then
               ierr=17
               varerr=mr2
               return
	    elseif (t2.le.0.0d0) then
               ierr=18
               varerr=t2
               return
	    elseif (st2.le.0.0d0) then
               ierr=19
               varerr=st2
               return
	    endif
                                !
            aa = p2 * coninf * mr2 * (st2 * ff)
            cc = coninf * st2
            dd = t2 * coninf * st2
            do kr=1,nbox
               ccbox(kr) = coninf * ka(kr)
               ddbox(kr) = t2 * ccbox(kr)
               c2box(kr) = c2 * ka(kr) * deltazdbl
            end do
            c2 = c2 * st2 * deltazdbl

         else

            call intzhunt_cts (iaquiZ, zl_cts(i), nzy_cts_real, 
     @           c1,p1,mr1,t1, con)
            do kr=1,nbox
               ta(kr)=t1
            end do
            call interstrhunt (iaquiHIST, st1,t1,ka,ta)
            do kr=1,nbox
               c1box(kr) = c1 * ka(kr) * deltazdbl
            end do
            c1 = c1 * st1 * deltazdbl
            aa = aa + ( p1*mr1*(c1*ff) + p2*mr2*(c2*ff)) / 2.d0
            cc = cc + ( c1 + c2 ) / 2.d0
            ccc = ( c1 + c2 ) / 2.d0
            dd = dd + ( t1*c1 + t2*c2 ) / 2.d0
            do kr=1,nbox
               ccbox(kr) = ccbox(kr) +
     @              ( c1box(kr) + c2box(kr) )/2.d0
               ddbox(kr) = ddbox(kr) +
     @              ( t1*c1box(kr)+t2*c2box(kr) )/2.d0
            end do

            mr2 = mr1
            c2=c1
            do kr=1,nbox
               c2box(kr) = c1box(kr)
            end do
            t2=t1
            p2=p1
         end if

         pp = aa / (cc*ff)

         ts = dd/cc
         do kr=1,nbox
   	    ta(kr) = ddbox(kr) / ccbox(kr)
         end do
         call interstrhunt(iaquiHIST, st,ts,ka,ta)
         call intershphunt(iaquiHIST, alsa,alda,ta)

c     
         eqw=0.0d0
         do  kr=1,nbox
            yy = ccbox(kr) * beta
            w = we_clean ( yy, pp, alsa(kr),alda(kr) )
            eqw = eqw + no(kr)*w
         end do

         argumento = eqw / deltanudbl
         tauinf(i) = dexp( - argumento )
         if (i.eq.nl_cts_real) then
            taustar11_cts(i) = 0.0d0
         else
            taustar11_cts(i) = deltanudbl * (tauinf(i+1)-tauinf(i))
     @           / ( beta * ccc )
         endif

      end do


      call mzescape_normaliz_02 ( taustar11_cts, nl_cts_real, 2 )

c     end
      return
      end


c     *** Old MZTUD110_dlvr11_03.f

c***********************************************************************
      subroutine MZTUD110( ierr, varerr )
c***********************************************************************

      implicit none

      include 'datafile.h'
      include 'nlte_paramdef.h'
      include 'nlte_commons.h'


c     arguments
      integer         ierr      ! o
      real*8          varerr    ! o

c     local variables and constants
      integer 	      i, in, ir, iaquiHIST , iaquiZ
      integer         ib, isot
      real*8 	      tau(nl,nl), argumento
      real*8 	      tauinf(nl)
      real*8 	      con(nzy), coninf
      real*8 	      c1, c2
      real*8 	      t1, t2
      real*8 	      p1, p2
      real*8	      mr1, mr2
      real*8 	      st1, st2
      real*8 	      c1box(nbox_max), c2box(nbox_max)
      real*8	      ff      ! to avoid too small numbers
      real*8	      tvtbs(nzy)
      real*8 	      st, beta, ts
      real*8  	      zld(nl), zyd(nzy), deltazdbl
      real*8 	      correc
      real*8	      deltanudbl
      real*8          maxtau, yy

c     external function
      external        we_clean
      real*8          we_clean

c***********************************************************************

      ierr = 0
      varerr = 0.d0
c     
      ib = 1
      beta = 1.8d5
      ibcode1 = '1'
      isot = 1
      deltanudbl = dble(deltanu(1,1))
      deltazdbl = dble(deltaz)
      ff=1.0d10

ccc   
      do i=1,nzy
         zyd(i) = dble(zy(i))
      enddo
      do i=1,nl
         zld(i) = dble(zl(i))
      enddo
      call interhuntdp ( tvtbs,zyd,nzy, v626t1,zld,nl, 1 )
      do i=1,nzy
         con(i) =  dble( co2y(i) * imr(isot) )
         correc = 2.d0 * dexp( -ee*dble(elow(isot,2))/tvtbs(i) )
         con(i) = con(i) * ( 1.d0 - correc )
         mr(i) = dble(co2y(i)/nty(i))
      end do
      if ( con(nzy) .le. 0.0d0 ) then
         ierr = 43
         varerr = con(nzy)
         return
      elseif ( con(nzy-1) .le. con(nzy) ) then
         write (*,*) ' WARNING in MZTUD110 '
         write (*,*) '    [CO2] grows with height at CurtisMatrix top.'
         write (*,*) '    [CO2] @ top = ', con(nzy)
         coninf = dble( con(nzy) )
      else
         coninf = dble( con(nzy) / log( con(nzy-1) / con(nzy) ) )
      endif
      call mztf_correccion ( coninf, con, ib )

ccc   
      call gethist_03 ( 1 )

c     
c     tauinf
c     
      call initial

      iaquiHIST = nhist/2
      iaquiZ = nzy - 2

      do i=nl,1,-1

         if(i.eq.nl)then

            call intzhunt (iaquiZ, zl(i),c2,p2,mr2,t2, con)
            do kr=1,nbox
               ta(kr)=t2
            end do
            call interstrhunt (iaquiHIST, st2,t2,ka,ta)
            ! Check interpolation errors :
            if (c2.le.0.0d0) then
               ierr=45
               varerr=c2
               return
            elseif (p2.le.0.0d0) then
               ierr=46
               varerr=p2
               return
            elseif (mr2.le.0.0d0) then
               ierr=47
               varerr=mr2
               return
            elseif (t2.le.0.0d0) then
               ierr=48
               varerr=t2
               return
            elseif (st2.le.0.0d0) then
               ierr=49
               varerr=st2
               return
            endif
                                !
            aa = p2 * coninf * mr2 * (st2 * ff)
            cc = coninf * st2
            dd = t2 * coninf * st2
            do kr=1,nbox
               ccbox(kr) = coninf * ka(kr)
               ddbox(kr) = t2 * ccbox(kr)
               c2box(kr) = c2 * ka(kr) * deltazdbl
            end do
            c2 = c2 * st2 * deltazdbl

         else

            call intzhunt (iaquiZ, zl(i),c1,p1,mr1,t1, con)
            do kr=1,nbox
               ta(kr)=t1
            end do
            call interstrhunt (iaquiHIST, st1,t1,ka,ta)
            do kr=1,nbox
               c1box(kr) = c1 * ka(kr) * deltazdbl
            end do
            ! Check interpolation errors :
            if (c1.le.0.0d0) then
               ierr=75
               varerr=c1
               return
            elseif (p1.le.0.0d0) then
               ierr=76
               varerr=p1
               return
            elseif (mr1.le.0.0d0) then
               ierr=77
               varerr=mr1
               return
            elseif (t1.le.0.0d0) then
               ierr=78
               varerr=t1
               return
            elseif (st1.le.0.0d0) then
               ierr=79
               varerr=st1
               return
            endif
	    !
            c1 = c1 * st1 * deltazdbl
            aa = aa + ( p1*mr1*(c1*ff) + p2*mr2*(c2*ff)) / 2.d0
            cc = cc + ( c1 + c2 ) / 2.d0
            dd = dd + ( t1*c1 + t2*c2 ) / 2.d0
            do kr=1,nbox
               ccbox(kr) = ccbox(kr) +
     @              ( c1box(kr) + c2box(kr) )/2.d0
               ddbox(kr) = ddbox(kr) +
     @              ( t1*c1box(kr)+t2*c2box(kr) )/2.d0
            end do

            mr2 = mr1
            c2=c1
            do kr=1,nbox
               c2box(kr) = c1box(kr)
            end do
            t2=t1
            p2=p1
         end if

         pp = aa / (cc*ff)

         ts = dd/cc
         do kr=1,nbox
   	    ta(kr) = ddbox(kr) / ccbox(kr)
         end do
         call interstrhunt(iaquiHIST, st,ts,ka,ta)
         call intershphunt(iaquiHIST, alsa,alda,ta)

c     
         eqw=0.0d0
         do  kr=1,nbox
            yy = ccbox(kr) * beta
            w = we_clean ( yy, pp, alsa(kr),alda(kr) )
            eqw = eqw + no(kr)*w
         end do

         argumento = eqw / deltanudbl
         tauinf(i) = dexp( - argumento )

      end do


c     
c     tau
c     

      iaquiZ = 2
      do 1 in=1,nl-1

         call initial
         call intzhunt (iaquiZ, zl(in), c1,p1,mr1,t1, con)
         do kr=1,nbox
            ta(kr) = t1
         end do
         call interstrhunt (iaquiHIST, st1,t1,ka,ta)
         do kr=1,nbox
            c1box(kr) = c1 * ka(kr) * deltazdbl
         end do
         c1 = c1 * st1 * deltazdbl

         do 2 ir=in,nl-1

            if (ir.eq.in) then
               tau(in,ir) = 1.d0
               goto 2
            end if

            call intzhunt (iaquiZ, zl(ir), c2,p2,mr2,t2, con)
            do kr=1,nbox
               ta(kr) = t2
            end do
            call interstrhunt (iaquiHIST, st2,t2,ka,ta)
            do kr=1,nbox
               c2box(kr) = c2 * ka(kr) * deltazdbl
            end do
            c2 = c2 * st2 * deltazdbl

            aa = aa + ( p1*mr1*(c1*ff) + p2*mr2*(c2*ff)) / 2.d0
            cc = cc + ( c1 + c2 ) / 2.d0
            dd = dd + ( t1*c1 + t2*c2 ) / 2.d0
            do kr=1,nbox
               ccbox(kr) = ccbox(kr) + 
     $              ( c1box(kr) + c2box(kr) ) / 2.d0
               ddbox(kr) = ddbox(kr) + 
     $              ( t1*c1box(kr) + t2*c2box(kr) ) / 2.d0
            end do

            mr1=mr2
            t1=t2
            c1=c2
            p1=p2
            do kr=1,nbox
               c1box(kr) = c2box(kr)
            end do

            pp = aa / (cc * ff)

            ts = dd/cc
            do kr=1,nbox
               ta(kr) = ddbox(kr) / ccbox(kr)
            end do
            call interstrhunt(iaquiHIST, st,ts,ka,ta)
            call intershphunt(iaquiHIST, alsa,alda,ta)
c     
            eqw=0.0d0
            do kr=1,nbox
               yy = ccbox(kr) * beta
               w = we_clean ( yy, pp, alsa(kr),alda(kr) )
               eqw = eqw + no(kr)*w
            end do

            argumento = eqw / deltanudbl
            tau(in,ir) = exp( - argumento )


 2       continue

 1    continue


c     
c     tau(in,ir) for n>r
c     

      in=nl

      call initial

      iaquiZ = nzy - 2
      call intzhunt (iaquiZ, zl(in), c1,p1,mr1,t1, con)
      do kr=1,nbox
         ta(kr) = t1
      end do
      call interstrhunt (iaquiHIST,st1,t1,ka,ta)
      do kr=1,nbox
         c1box(kr) = c1 * ka(kr) * deltazdbl
      end do
      c1 = c1 * st1 * deltazdbl

      do 4 ir=in-1,1,-1

         call intzhunt (iaquiZ, zl(ir), c2,p2,mr2,t2, con)
         do kr=1,nbox
            ta(kr) = t2
         end do
         call interstrhunt (iaquiHIST, st2,t2,ka,ta)
         do kr=1,nbox
            c2box(kr) = c2 * ka(kr) * deltazdbl
         end do
         c2 = c2 * st2 * deltazdbl

         aa = aa + ( p1*mr1*(c1*ff) + p2*mr2*(c2*ff)) / 2.d0
         cc = cc + ( c1 + c2 ) / 2.d0
         dd = dd + ( t1*c1 + t2*c2 ) / 2.d0
         do kr=1,nbox
            ccbox(kr) = ccbox(kr) + 
     $           ( c1box(kr) + c2box(kr) ) / 2.d0
            ddbox(kr) = ddbox(kr) + 
     $           ( t1*c1box(kr) + t2*c2box(kr) ) / 2.d0
         end do

         mr1=mr2
         c1=c2
         t1=t2
         p1=p2
         do kr=1,nbox
            c1box(kr) = c2box(kr)
         end do

         pp = aa / (cc * ff)
         ts = dd / cc
         do kr=1,nbox
            ta(kr) = ddbox(kr) / ccbox(kr)
         end do
         call interstrhunt (iaquiHIST, st,ts,ka,ta)
         call intershphunt (iaquiHIST, alsa,alda,ta)

c     

         eqw=0.0d0
         do kr=1,nbox
            yy = ccbox(kr) * beta
            w = we_clean ( yy, pp, alsa(kr),alda(kr) )
            eqw = eqw + no(kr)*w
         end do

         argumento = eqw / deltanudbl
         tau(in,ir) = exp( - argumento )


 4    continue

c     
c     
c     
      do in=nl-1,2,-1
         do ir=in-1,1,-1
            tau(in,ir) = tau(ir,in)
         end do
      end do

c     
c     Tracking potential numerical errors
c     
      maxtau = 0.0d0
      do in=nl-1,2,-1
         do ir=in-1,1,-1
            maxtau = max( maxtau, tau(in,ir) )
         end do
      end do
      if (maxtau .gt. 1.0d0) then
         ierr = 42
         varerr = maxtau
         return
      endif


c     
      call MZCUD110 ( tauinf,tau )

c     end
      return
      end


c     *** Old file MZCUD_dlvr11.f ***

c***********************************************************************

      subroutine MZCUD110 ( tauinf,tau )

c***********************************************************************

      implicit none

      include 'nlte_paramdef.h'
      include 'nlte_commons.h'

c     arguments
      real*8 		tau(nl,nl) ! i
      real*8		tauinf(nl) ! i


c     local variables
      integer 	i, in, ir
      real*8		a(nl,nl), cf(nl,nl), pideltanu, deltazdp, pi

c***********************************************************************

      pi = 3.141592
      pideltanu = pi * dble(deltanu(1,1))
      deltazdp = 2.0d5 * dble(deltaz)

      do in=1,nl
         do ir=1,nl
            cf(in,ir) = 0.0d0
            c110(in,ir) = 0.0d0
            a(in,ir) = 0.0d0
         end do
         vc110(in) = 0.0d0
      end do

c     
      do in=1,nl
         do ir=1,nl

            if (ir.eq.1) then
               cf(in,ir) = tau(in,ir) - tau(in,1)
            elseif (ir.eq.nl) then
               cf(in,ir) = tauinf(in) - tau(in,ir-1)
            else
               cf(in,ir) = tau(in,ir) - tau(in,ir-1)
            end if

         end do
      end do

c     
      do in=2,nl-1
         do ir=1,nl
            if (ir.eq.in+1) a(in,ir) = -1.d0
            if (ir.eq.in-1) a(in,ir) = +1.d0
            a(in,ir) = a(in,ir) / deltazdp
         end do
      end do

c     
      do in=1,nl
         do ir=1,nl
	    cf(in,ir) = cf(in,ir) * pideltanu
         end do
      end do

      do in=2,nl-1
         do ir=1,nl
	    do i=1,nl
               c110(in,ir) = c110(in,ir) + a(in,i) * cf(i,ir)
	    end do
         end do
      end do

      do in=2,nl-1
         vc110(in) =  pideltanu/deltazdp *
     @        ( tau(in-1,1) - tau(in+1,1) )
      end do


c     
      do in=2,nl-1
         c110(in,nl-2) = c110(in,nl-2) - c110(in,nl)
         c110(in,nl-1) = c110(in,nl-1) + 2.d0*c110(in,nl)
      end do

c     end
      return
      end


c     *** Old MZMC121_dlvr11_03.f ***

c***********************************************************************

      subroutine MZMC121

c***********************************************************************

      implicit none

                                ! common variables & constants
      
      include 'nlte_paramdef.h'
      include 'nlte_commons.h'

                                ! local variables

      real*8  cax1(nl,nl)
      real*8  v1(nl), cm_factor, vc_factor
      real    nuaux1, nuaux2, nuaux3
      real*8  faux2,faux3, daux2,daux3
      real*8  varerr

      integer i,j,ik,ib
      integer ierr      

************************************************************************

      c121(1:nl,1:nl)=0.d0
!      call zerom (c121,nl)
      vc121(1:nl)=0.d0
!      call zerov (vc121,nl)

      nuaux1 = nu(1,2) - nu(1,1) ! 667.75
      nuaux2 = nu12_0200-nu(1,1) ! 618.03
      nuaux3 = nu12_1000-nu(1,1) ! 720.81
      faux2 = dble(nuaux2/nuaux1)
      faux3 = dble(nuaux3/nuaux1)
      daux2 = dble(nuaux1-nuaux2)
      daux3 = dble(nuaux1-nuaux3)

      do 11, ik=1,3

         ib=ik+1
         cax1(1:nl,1:nl)=0.d0
!         call zerom (cax1,nl)
         call MZTUD121 ( cax1,v1, ib, ierr, varerr )
         if (ierr .gt. 0) call ERRORS (ierr,varerr)

         do i=1,nl

	    if(ik.eq.1)then
               cm_factor = faux2**2.d0 * exp( daux2*ee/dble(t(i)) )
               vc_factor = 1.d0/faux2
	    elseif(ik.eq.2)then
               cm_factor = 1.d0
               vc_factor = 1.d0
	    elseif(ik.eq.3)then
               cm_factor = faux3**2.d0 * exp( daux3*ee/dble(t(i)) )
               vc_factor = 1.d0 / faux3
            else
               write (*,*) ' Error in 626 hot band index  ik =', ik
               stop ' ik can only be = 2,3,4.   Check needed.'
	    end if
	    do j=1,nl
               c121(i,j) = c121(i,j) + cax1(i,j) * cm_factor
	    end do

	    vc121(i) = vc121(i) + v1(i) * vc_factor

         end do

 11   continue

      return
      end


c     *** Old MZTUD121_dlvr11_03.f ***

c***********************************************************************
      subroutine MZTUD121 ( cf,vc, ib, ierr, varerr )
c***********************************************************************

      implicit none

      include 'datafile.h'
      include 'nlte_paramdef.h'
      include 'nlte_commons.h'
      

c     arguments
      real*8  	      cf(nl,nl)	! o
      real*8	      vc(nl)    ! o
      integer	      ib        ! i
      integer         ierr      ! o
      real*8          varerr    ! o


c     local variables and constants
      integer 	      i, in, ir, iaquiHIST, iaquiZ
      integer 	      isot
      real*8          tau(nl,nl), argumento, deltazdbl
      real*8 	      tauinf(nl)
      real*8 	      con(nzy), coninf
      real*8 	      c1, c2
      real*8 	      t1, t2
      real*8 	      p1, p2
      real*8	      mr1, mr2
      real*8 	      st1, st2
      real*8 	      c1box(nbox_max), c2box(nbox_max)
      real*8	      ff      ! to avoid too small numbers
      real*8	      tvtbs(nzy)
      real*8 	      st, beta, ts
      real*8  	      zld(nl), zyd(nzy)
      real*8 	      correc
      real*8  	      deltanudbl
      real*8          yy

c     external function
      external        we_clean
      real*8          we_clean


c     formats
 101  format(i1)
c***********************************************************************

      ierr = 0
      varerr = 0.d0

c     some values
      beta = 1.8d5
      isot = 1
      write (ibcode1,101) ib
      deltanudbl = dble( deltanu(isot,ib) )
      ff=1.0d10
      deltazdbl = dble(deltaz)

ccc   
ccc   
ccc   
      do i=1,nl
         zld(i) = dble(zl(i))
      enddo
      do i=1,nzy
         zyd(i) = dble(zy(i))
      enddo

      call interhuntdp ( tvtbs,zyd,nzy, v626t1,zld,nl, 1 )

      do i=1,nzy
         con(i) =  dble( co2y(i) * imr(isot) )
         correc = 2.d0 * exp( -ee*dble(elow(isot,2))/tvtbs(i) )
         con(i) = con(i) * ( 1.d0 - correc )
         mr(i) = dble( co2y(i) / nty(i) )
      end do

      if ( con(nzy) .le. 0.0d0 ) then
         ierr = 83
         varerr = con(nzy)
         return
      elseif ( con(nzy-1) .le. con(nzy) ) then
         write (*,*) ' WARNING in MZTUD121 '
         write (*,*) '    [CO2] grows with height at CurtisMatrix top.'
         write (*,*) '    [CO2] @ top = ', con(nzy)
         coninf = dble( con(nzy) )
      else
         coninf = dble( con(nzy) / log( con(nzy-1) / con(nzy) ) )
      endif
      call mztf_correccion ( coninf, con, ib )

ccc   
      call gethist_03 ( ib )


c     
c     tauinf(nl)
c     
      call initial

      iaquiZ = nzy - 2
      iaquiHIST = nhist / 2

      do i=nl,1,-1

         if(i.eq.nl)then

            call intzhunt ( iaquiZ, zl(i),c2,p2,mr2,t2, con)
            do kr=1,nbox
               ta(kr)=t2
            end do
            call interstrhunt (iaquiHIST, st2,t2,ka,ta)
            aa = p2 * coninf * mr2 * (st2 * ff)
            cc = coninf * st2
            dd = t2 * coninf * st2
            do kr=1,nbox
               ccbox(kr) = coninf * ka(kr)
               ddbox(kr) = t2 * ccbox(kr)
               c2box(kr) = c2 * ka(kr) * deltazdbl
            end do
            c2 = c2 * st2 * deltazdbl

         else
            call intzhunt ( iaquiZ, zl(i),c1,p1,mr1,t1, con)
            do kr=1,nbox
               ta(kr)=t1
            end do
            call interstrhunt (iaquiHIST, st1,t1,ka,ta)
            do kr=1,nbox
               c1box(kr) = c1 * ka(kr) * deltazdbl
            end do
            ! Check interpolation errors :
            if (c1.le.0.0d0) then
               ierr=85
               varerr=c1
               return
            elseif (p1.le.0.0d0) then
               ierr=86
               varerr=p1
               return
            elseif (mr1.le.0.0d0) then
               ierr=87
               varerr=mr1
               return
            elseif (t1.le.0.0d0) then
               ierr=88
               varerr=t1
               return
            elseif (st1.le.0.0d0) then
               ierr=89
               varerr=st1
               return
            endif
	    !
            c1 = c1 * st1 * deltazdbl
            aa = aa + ( p1*mr1*(c1*ff) + p2*mr2*(c2*ff)) / 2.d0
            cc = cc + ( c1 + c2 ) / 2.d0
            dd = dd + ( t1*c1 + t2*c2 ) / 2.d0
            do kr=1,nbox
               ccbox(kr) = ccbox(kr) +
     @              ( c1box(kr) + c2box(kr) )/2.d0
               ddbox(kr) = ddbox(kr) +
     @              ( t1*c1box(kr)+t2*c2box(kr) )/2.d0
            end do

            mr2 = mr1
            c2=c1
            do kr=1,nbox
               c2box(kr) = c1box(kr)
            end do
            t2=t1
            p2=p1
         end if

         pp = aa / (cc*ff)

         ts = dd/cc
         do kr=1,nbox
   	    ta(kr) = ddbox(kr) / ccbox(kr)
         end do
         call interstrhunt(iaquiHIST, st,ts,ka,ta)
         call intershphunt(iaquiHIST, alsa,alda,ta)

c     

         eqw = 0.0d0
         do  kr=1,nbox
            yy = ccbox(kr) * beta
            w = we_clean ( yy, pp, alsa(kr),alda(kr) )
            eqw = eqw + no(kr)*w
         end do

         argumento = eqw / deltanudbl
         tauinf(i) = dexp( - argumento )


      end do                    ! i continue


c     
c     tau(in,ir) for n<=r
c     

      iaquiZ = 2
      do 1 in=1,nl-1

         call initial
         call intzhunt ( iaquiZ, zl(in), c1,p1,mr1,t1, con)
         do kr=1,nbox
            ta(kr) = t1
         end do
         call interstrhunt (iaquiHIST, st1,t1,ka,ta)
         do kr=1,nbox
            c1box(kr) = c1 * ka(kr) * deltazdbl
         end do
         c1 = c1 * st1 * deltazdbl

         do 2 ir=in,nl-1

            if (ir.eq.in) then
               tau(in,ir) = 1.d0
               goto 2
            end if

            call intzhunt ( iaquiZ, zl(ir), c2,p2,mr2,t2, con)
            do kr=1,nbox
               ta(kr) = t2
            end do
            call interstrhunt (iaquiHIST, st2,t2,ka,ta)
            do kr=1,nbox
               c2box(kr) = c2 * ka(kr) * deltazdbl
            end do
            c2 = c2 * st2 * deltazdbl

            aa = aa + ( p1*mr1*(c1*ff) + p2*mr2*(c2*ff)) / 2.d0
            cc = cc + ( c1 + c2 ) / 2.d0
            dd = dd + ( t1*c1 + t2*c2 ) / 2.d0
            do kr=1,nbox
               ccbox(kr) = ccbox(kr) + 
     $              ( c1box(kr) + c2box(kr) ) / 2.d0
               ddbox(kr) = ddbox(kr) + 
     $              ( t1*c1box(kr) + t2*c2box(kr) ) / 2.d0
            end do

            mr1=mr2
            t1=t2
            c1=c2
            p1=p2
            do kr=1,nbox
               c1box(kr) = c2box(kr)
            end do

            pp = aa / (cc * ff)

            ts = dd/cc
            do kr=1,nbox
               ta(kr) = ddbox(kr) / ccbox(kr)
            end do
            call interstrhunt(iaquiHIST, st,ts,ka,ta)
            call intershphunt(iaquiHIST, alsa,alda,ta)

c     

            eqw = 0.0d0
            do kr=1,nbox
               yy = ccbox(kr) * beta
               w = we_clean ( yy, pp, alsa(kr),alda(kr) )
               eqw = eqw + no(kr)*w
            end do

            argumento = eqw / deltanudbl
            tau(in,ir) = dexp( - argumento )

 2       continue

 1    continue

c     
c     tau(in,ir) for n>r
c     

      in=nl

      call initial
      iaquiZ = nzy - 2
      call intzhunt ( iaquiZ, zl(in), c1,p1,mr1,t1, con)
      do kr=1,nbox
         ta(kr) = t1
      end do
      call interstrhunt (iaquiHIST, st1,t1,ka,ta)
      do kr=1,nbox
         c1box(kr) = c1 * ka(kr) * deltazdbl
      end do
      c1 = c1 * st1 * deltazdbl

      do 4 ir=in-1,1,-1

         call intzhunt ( iaquiZ, zl(ir), c2,p2,mr2,t2, con)
         do kr=1,nbox
            ta(kr) = t2
         end do
         call interstrhunt (iaquiHIST, st2,t2,ka,ta)
         do kr=1,nbox
            c2box(kr) = c2 * ka(kr) * deltazdbl
         end do
         c2 = c2 * st2 * deltazdbl

         aa = aa + ( p1*mr1*(c1*ff) + p2*mr2*(c2*ff)) / 2.d0
         cc = cc + ( c1 + c2 ) / 2.d0
         dd = dd + ( t1*c1 + t2*c2 ) / 2.d0
         do kr=1,nbox
            ccbox(kr) = ccbox(kr) + 
     $           ( c1box(kr) + c2box(kr) ) / 2.d0
            ddbox(kr) = ddbox(kr) + 
     $           ( t1*c1box(kr) + t2*c2box(kr) ) / 2.d0
         end do

         mr1=mr2
         c1=c2
         t1=t2
         p1=p2
         do kr=1,nbox
            c1box(kr) = c2box(kr)
         end do

         pp = aa / (cc * ff)
         ts = dd / cc
         do kr=1,nbox
            ta(kr) = ddbox(kr) / ccbox(kr)
         end do
         call interstrhunt (iaquiHIST, st,ts,ka,ta)
         call intershphunt (iaquiHIST, alsa,alda,ta)

c     
         eqw=0.0d0
         do kr=1,nbox
            yy = ccbox(kr) * beta
            w = we_clean ( yy, pp, alsa(kr),alda(kr) )
            eqw = eqw + no(kr)*w
         end do

         argumento = eqw / deltanudbl
         tau(in,ir) = dexp( - argumento )

 4    continue

c     
c     
c     
      do in=nl-1,2,-1
         do ir=in-1,1,-1
            tau(in,ir) = tau(ir,in)
         end do
      end do

c     
      call MZCUD121 ( tauinf,tau, cf, vc, ib )


c     end
      return
      end



c     *** Old MZCUD121_dlvr11.f ***

c***********************************************************************

      subroutine MZCUD121 ( tauinf,tau, c,vc, ib )

c***********************************************************************

      implicit none

      include 'nlte_paramdef.h'
      include 'nlte_commons.h'


c     arguments
      real*8		c(nl,nl) ! o
      real*8 		vc(nl)  ! o
      real*8 		tau(nl,nl) ! i
      real*8		tauinf(nl) ! i
      integer		ib      ! i

c     local variables
      integer 	i, in, ir, isot
      real*8		a(nl,nl), cf(nl,nl), pideltanu, deltazdbl,pi

c***********************************************************************

      pi=3.141592
      isot = 1
      pideltanu = pi*dble(deltanu(isot,ib))
      deltazdbl = dble(deltaz)
c     
      do in=1,nl

         do ir=1,nl

            cf(in,ir) = 0.0d0
            c(in,ir) = 0.0d0
            a(in,ir) = 0.0d0

         end do

         vc(in) = 0.0d0

      end do


c     
      do in=1,nl
         do ir=1,nl

            if (ir.eq.1) then
               cf(in,ir) = tau(in,ir) - tau(in,1)
            elseif (ir.eq.nl) then
               cf(in,ir) = tauinf(in) - tau(in,ir-1)
            else
               cf(in,ir) = tau(in,ir) - tau(in,ir-1)
            end if

         end do
      end do


c     
      do in=2,nl-1
         do ir=1,nl
            if (ir.eq.in+1) a(in,ir) = -1.d0
            if (ir.eq.in-1) a(in,ir) = +1.d0
            a(in,ir) = a(in,ir) / ( 2.d5*deltazdbl )
         end do
      end do

c     
      do in=1,nl
         do ir=1,nl
	    cf(in,ir) = cf(in,ir) * pideltanu
         end do
      end do


      do in=2,nl-1
         do ir=1,nl
	    do i=1,nl
               c(in,ir) = c(in,ir) + a(in,i) * cf(i,ir)
	    end do
         end do
         vc(in) =  pideltanu /( 2.d5*deltazdbl ) *
     @        ( tau(in-1,1) - tau(in+1,1) )
      end do

c     
      do in=2,nl-1
         c(in,nl-2) = c(in,nl-2) - c(in,nl)
         c(in,nl-1) = c(in,nl-1) + 2.d0*c(in,nl)
      end do


c     end
      return
      end



c     *** Old MZESC121_dlvr11_03.f ***

c***********************************************************************
      subroutine MZESC121
c***********************************************************************

      implicit none

      include 'nlte_paramdef.h'
      include 'nlte_commons.h'


c     local variables
      integer 	      i,ierr
      real*8          factor0200, factor0220, factor1000
      real*8          aux_0200(nl), aux2_0200(nl)
      real*8          aux_0220(nl), aux2_0220(nl)
      real*8          aux_1000(nl), aux2_1000(nl)
      real*8          varerr 

c***********************************************************************

!      call zerov (taustar12, nl)
      taustar12(1:nl)=0.d0
      call zero2v(aux_0200,aux2_0200, nl)
      call zero2v(aux_0220,aux2_0220, nl)
      call zero2v(aux_1000,aux2_1000, nl)

      call MZESC121sub (aux_0200,aux2_0200, 2 , ierr, varerr)
      if (ierr .gt. 0) call ERRORS (ierr,varerr)
      call MZESC121sub (aux_0220,aux2_0220, 3 , ierr, varerr)
      if (ierr .gt. 0) call ERRORS (ierr,varerr)
      call MZESC121sub (aux_1000,aux2_1000, 4 , ierr, varerr)
      if (ierr .gt. 0) call ERRORS (ierr,varerr)

      factor0220 = 1.d0
      factor0200 = dble( (nu(1,2)-nu(1,1)) / (nu12_0200-nu(1,1)) )
      factor1000 = dble( (nu(1,2)-nu(1,1)) / (nu12_1000-nu(1,1)) )
      do i=1,nl
         taustar12(i) = taustar12(i)
     @        + aux_0200(i) * factor0200
     @        + aux_0220(i) * factor0220
     @        + aux_1000(i) * factor1000
      enddo

      call mzescape_normaliz ( taustar12, 2 )

c     end
      return
      end


c     *** Old MZESC121sub_dlvr11_03.f ***

c***********************************************************************

      subroutine MZESC121sub (taustar,tauinf, ib, ierr, varerr )

c***********************************************************************

      implicit none

      include 'datafile.h'
      include 'nlte_paramdef.h'
      include 'nlte_commons.h'


c     arguments
      real*8          taustar(nl) ! o
      real*8 	      tauinf(nl)  ! o
      integer	      ib          ! i
      integer         ierr        ! o
      real*8          varerr      ! o


c     local variables and constants
      integer 	      i, iaquiHIST, iaquiZ, isot
      real*8 	      con(nzy), coninf
      real*8 	      c1, c2, ccc
      real*8 	      t1, t2
      real*8 	      p1, p2
      real*8	      mr1, mr2
      real*8 	      st1, st2
      real*8 	      c1box(70), c2box(70)
      real*8	      ff      ! to avoid too small numbers
      real*8	      tvtbs(nzy)
      real*8 	      st, beta, ts
      real*8  	      zld(nl), zyd(nzy)
      real*8 	      correc
      real*8 	      deltanudbl, deltazdbl
      real*8          yy

c     external function
      external        we_clean
      real*8          we_clean

c     formats
 101  format(i1)

c***********************************************************************

      ierr = 0
      varerr = 0.d0
c     
      beta = 1.8d5
      isot = 1
      write ( ibcode1, 101) ib
      deltanudbl = dble( deltanu(isot,ib) )
      ff=1.0d10
      deltazdbl = dble(deltaz)

c     
      do i=1,nzy
         zyd(i) = dble(zy(i))
      enddo
      do i=1,nl
         zld(i) = dble(zl(i))
      enddo

      call interhuntdp ( tvtbs,zyd,nzy, v626t1,zld,nl, 1 )

      do i=1,nzy
         con(i) =  dble( co2y(i) * imr(isot) )
         correc = 2.d0 * dexp( -ee*dble(elow(isot,2))/tvtbs(i) )
         con(i) = con(i) * ( 1.d0 - correc )
         mr(i) = dble(co2y(i)/nty(i))
      end do
      if ( con(nzy) .le. 0.0d0 ) then
         ierr = 63
         varerr = con(nzy)
         return
      elseif ( con(nzy-1) .le. con(nzy) ) then
         write (*,*) ' WARNING in MZESC121sub '
         write (*,*) '    [CO2] grows with height at CurtisMatrix top.'
         write (*,*) '    [CO2] @ top = ', con(nzy)
         coninf = dble( con(nzy) )
      else
         coninf = dble( con(nzy) / log( con(nzy-1) / con(nzy) ) )
      endif
      call mztf_correccion ( coninf, con, ib )

c     
      call gethist_03 ( ib )

c     
c     tauinf
c     
      call initial

      iaquiHIST = nhist/2
      iaquiZ = nzy - 2

      do i=nl,1,-1

         if(i.eq.nl)then

            call intzhunt (iaquiZ, zl(i),c2,p2,mr2,t2, con)
            do kr=1,nbox
               ta(kr)=t2
	    end do
            call interstrhunt (iaquiHIST, st2,t2,ka,ta)
            ! Check interpolation errors :
            if (c2.le.0.0d0) then
               ierr=65
               varerr=c2
               return
            elseif (p2.le.0.0d0) then
               ierr=66
               varerr=p2
               return
            elseif (mr2.le.0.0d0) then
               ierr=67
               varerr=mr2
               return
            elseif (t2.le.0.0d0) then
               ierr=68
               varerr=t2
               return
            elseif (st2.le.0.0d0) then
               ierr=69
               varerr=st2
               return
            endif
	    !
            aa = p2 * coninf * mr2 * (st2 * ff)
            cc = coninf * st2
            dd = t2 * coninf * st2
            do kr=1,nbox
               ccbox(kr) = coninf * ka(kr)
               ddbox(kr) = t2 * ccbox(kr)
               c2box(kr) = c2 * ka(kr) * deltazdbl
            end do
            c2 = c2 * st2 * deltazdbl

         else
            call intzhunt (iaquiZ, zl(i),c1,p1,mr1,t1, con)
            do kr=1,nbox
               ta(kr)=t1
            end do
            call interstrhunt (iaquiHIST,st1,t1,ka,ta)
            do kr=1,nbox
               c1box(kr) = c1 * ka(kr) * deltazdbl
            end do
            c1 = c1 * st1 * deltazdbl
            aa = aa + ( p1*mr1*(c1*ff) + p2*mr2*(c2*ff)) / 2.d0
            cc = cc + ( c1 + c2 ) / 2.d0
            ccc = ( c1 + c2 ) / 2.d0
            dd = dd + ( t1*c1 + t2*c2 ) / 2.d0
            do kr=1,nbox
               ccbox(kr) = ccbox(kr) +
     @              ( c1box(kr) + c2box(kr) )/2.d0
               ddbox(kr) = ddbox(kr) +
     @              ( t1*c1box(kr)+t2*c2box(kr) )/2.d0
            end do

            mr2 = mr1
            c2=c1
            do kr=1,nbox
               c2box(kr) = c1box(kr)
            end do
            t2=t1
            p2=p1
         end if

         pp = aa / (cc*ff)

         ts = dd/cc
         do kr=1,nbox
   	    ta(kr) = ddbox(kr) / ccbox(kr)
         end do
         call interstrhunt(iaquiHIST,st,ts,ka,ta)
         call intershphunt(iaquiHIST,alsa,alda,ta)

c     
         eqw=0.0d0
         do  kr=1,nbox
            yy = ccbox(kr) * beta
            w = we_clean ( yy, pp, alsa(kr),alda(kr) )
            eqw = eqw + no(kr)*w
         end do
         tauinf(i) = dexp( - eqw / deltanudbl )
         if (tauinf(i).lt.0.d0) tauinf(i) = 0.0d0

         if (i.eq.nl) then
            taustar(i) = 0.0d0
         else
            taustar(i) = deltanudbl * (tauinf(i+1)-tauinf(i))
     @           / ( beta * ccc  )
         endif

      end do



c     end
      return
      end


c     *** Old MZTVC121_dlvr11.f *** 

c***********************************************************************

      subroutine MZTVC121

c***********************************************************************

      implicit none

!!!!!!!!!!!!!!!!!!!!!!!
!     common variables & constants

      include 'nlte_paramdef.h'
      include 'nlte_commons.h'


      integer ierr
      real*8 varerr


!     local variables

      real*8 v1(nl), vc_factor
      integer i,ik,ib

************************************************************************

!      call zerov( vc121, nl )
      vc121(1:nl)=0.d0

      do 11, ik=1,3

         ib=ik+1

         call MZTVC121sub (v1, ib, ierr,varerr )

         do i=1,nl

	    if(ik.eq.1)then
               vc_factor =
     @              dble( (nu(1,2)-nu(1,1)) / (nu12_0200-nu(1,1)) )
	    elseif(ik.eq.2)then
               vc_factor = 1.d0
	    elseif(ik.eq.3)then
               vc_factor =
     @              dble( (nu(1,2)-nu(1,1)) / (nu12_1000-nu(1,1)) )
	    end if

	    vc121(i) = vc121(i) + v1(i) * vc_factor

         end do

 11   continue


      return
      end


c     *** Old MZTVC121sub_dlvr11_03.f ***

c***********************************************************************

      subroutine MZTVC121sub  ( vc, ib,  ierr, varerr )

c***********************************************************************

      implicit none

      include 'datafile.h'
      include 'nlte_paramdef.h'
      include 'nlte_commons.h'


c     arguments
      real*8	    vc(nl)  ! o
      integer	    ib      ! i
      integer       ierr    ! o
      real*8        varerr  ! o

c     local variables and constants
      integer 	    i, in, ir, iaquiHIST , iaquiZ, isot
      real*8 	    tau(nl,nl), argumento
      real*8 	    con(nzy), coninf
      real*8 	    c1, c2
      real*8 	    t1, t2
      real*8 	    p1, p2
      real*8	    mr1, mr2
      real*8 	    st1, st2
      real*8 	    c1box(70), c2box(70)
      real*8	    ff      ! to avoid too small numbers
      real*8	    tvtbs(nzy)
      real*8 	    st, beta, ts
      real*8  	    zld(nl), zyd(nzy), deltazdbl
      real*8 	    correc
      real*8 	    deltanudbl, pideltanu,pi
      real*8        yy
      real*8        minvc, maxtau

c     external function
      external      we_clean
      real*8        we_clean

c     formats
 101  format(i1)

c***********************************************************************
      
      ierr = 0
      varerr = 0.d0
c     
      pi=3.141592
      isot = 1
      beta = 1.8d5
      write (ibcode1,101) ib
      deltanudbl = dble( deltanu(isot,ib) )
      pideltanu = pi*deltanudbl
      ff=1.0d10
      deltazdbl = dble(deltaz)
c     
c     
c     

      do i=1,nzy
         zyd(i) = dble(zy(i))
      enddo
      do i=1,nl
         zld(i) = dble(zl(i))
      enddo

      call interhuntdp ( tvtbs,zyd,nzy, v626t1,zld,nl, 1 )

      do i=1,nzy
         con(i) =  dble( co2y(i) * imr(isot) )
         correc = 2.d0 * dexp( -ee*dble(elow(isot,2))/tvtbs(i) )
         con(i) = con(i) * ( 1.d0 - correc )
         mr(i) = dble(co2y(i)/nty(i))
      end do

      if ( con(nzy) .le. 0.0d0 ) then
         ierr = 53
         varerr = con(nzy)
         return
      elseif ( con(nzy-1) .le. con(nzy) ) then
         write (*,*) ' WARNING in MZTVC121sub '
         write (*,*) '    [CO2] grows with height at CurtisMatrix top.'
         write (*,*) '    [CO2] @ top = ', con(nzy)
         coninf = dble( con(nzy) )
      else
         coninf = dble( con(nzy) / log( con(nzy-1) / con(nzy) ) )
      endif
      call mztf_correccion ( coninf, con, ib )

ccc   
      call gethist_03 ( ib )

c     
c     tau(1,ir)
c     
      call initial

      iaquiHIST = nhist/2

      in=1

      tau(in,1) = 1.d0

      call initial
      iaquiZ = 2
      call intzhunt ( iaquiZ, zl(in), c1,p1,mr1,t1, con)
      do kr=1,nbox
         ta(kr) = t1
      end do
      call interstrhunt (iaquiHIST, st1,t1,ka,ta)
      do kr=1,nbox
         c1box(kr) = c1 * ka(kr) * deltazdbl
      end do
      c1 = c1 * st1 * deltazdbl
                                ! Check interpolation errors :
      if (c1.le.0.0d0) then
         ierr=55
         varerr=c1
         return
      elseif (p1.le.0.0d0) then
         ierr=56
         varerr=p1
         return
      elseif (mr1.le.0.0d0) then
         ierr=57
         varerr=mr1
         return
      elseif (t1.le.0.0d0) then
         ierr=58
         varerr=t1
         return
      elseif (st1.le.0.0d0) then
         ierr=59
         varerr=st1
         return
      endif
                                !

      do 2 ir=2,nl

         call intzhunt (iaquiZ, zl(ir), c2,p2,mr2,t2, con)
         do kr=1,nbox
            ta(kr) = t2
         end do
         call interstrhunt (iaquiHIST, st2,t2,ka,ta)
         do kr=1,nbox
            c2box(kr) = c2 * ka(kr) * deltazdbl
         end do
         c2 = c2 * st2 * deltazdbl

         aa = aa + ( p1*mr1*(c1*ff) + p2*mr2*(c2*ff)) / 2.d0
         cc = cc + ( c1 + c2 ) / 2.d0
         dd = dd + ( t1*c1 + t2*c2 ) / 2.d0
         do kr=1,nbox
            ccbox(kr) = ccbox(kr) + ( c1box(kr) + c2box(kr) ) /2.d0
            ddbox(kr) = ddbox(kr) + 
     $           ( t1*c1box(kr) + t2*c2box(kr) ) / 2.d0
         end do

         mr1=mr2
         t1=t2
         c1=c2
         p1=p2
         do kr=1,nbox
            c1box(kr) = c2box(kr)
         end do

         pp = aa / (cc * ff)

         ts = dd/cc
         do kr=1,nbox
   	    ta(kr) = ddbox(kr) / ccbox(kr)
         end do
         call interstrhunt(iaquiHIST, st,ts,ka,ta)
         call intershphunt(iaquiHIST, alsa,alda,ta)

         eqw=0.0d0
         do kr=1,nbox
            yy = ccbox(kr) * beta
            w = we_clean ( yy, pp, alsa(kr),alda(kr) )
            eqw = eqw + no(kr)*w
         end do

         argumento = eqw / deltanudbl
         tau(in,ir) = dexp( - argumento )

 2    continue


c     
c     
c     
      do in=nl,2,-1
         tau(in,1) = tau(1,in)
      end do

c     
      vc(1) = 0.0d0
      vc(nl) = 0.0d0
      do in=2,nl-1
         vc(in) =  pideltanu /( 2.d5*deltazdbl ) *
     @        ( tau(in-1,1) - tau(in+1,1) )
         if (vc(in) .lt. 0.0d0) vc(in) = vc(in-1)
      end do

c     
c     Tracking potential numerical errors
c     
      minvc = 1.d6
      maxtau = tau(nl,1)
      do in=2,nl-1
         minvc = min( minvc, vc(in) )
         maxtau = max( maxtau, tau(in,1) )
      end do
      if (maxtau .gt. 1.0d0) then
         ierr = 52
         varerr = maxtau
         return
      else if (minvc .lt. 0.0d0) then
         ierr = 51
         varerr = minvc
         return
      endif

c     end
      return
      end









