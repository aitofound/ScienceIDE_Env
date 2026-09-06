!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
! Fast scheme for NLTE cooling rates at 15um by CO2 in a Martian GCM !
!                 Version dlvr11_03. 2012.                           !
! Software written and provided by IAA/CSIC, Granada, Spain,         !
! under ESA contract "Mars Climate Database and Physical Models"     !
! Person of contact: Miguel Angel Lopez Valverde  valverde@iaa.es    !
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
c**********************************************************************
c     
c     Contains the following old 1-d model subroutines:
c     
c     -NLTEdlvr11_TCOOL_03
c     -NLTEdlvr11_CZALU_03
c     -NLTEdlvr11_FB626CTS_03
c     -NLTEdlvr11_ERRORS
c
c     
c     
c     *** Old NLTEdlvr11_TCOOL_02 ***
c     
c***********************************************************************

c***********************************************************************

      subroutine nlte_tcool(ngridgcm,n_gcm,
     $     p_gcm, t_gcm, z_gcm,
     $     co2vmr_gcm, n2vmr_gcm, covmr_gcm, o3pvmr_gcm,
     $     q15umco2_gcm , ierr, varerr)

c***********************************************************************

      use conc_mod, only: cpnew, mmean
      implicit none

      include 'nlte_paramdef.h'
      include 'nlte_commons.h'
      include "chimiedata.h"


c     Arguments
      integer n_gcm,ngridgcm
      real p_gcm(ngridgcm,n_gcm), t_gcm(ngridgcm,n_gcm)
      real z_gcm(ngridgcm,n_gcm)
      real co2vmr_gcm(ngridgcm,n_gcm), n2vmr_gcm(ngridgcm,n_gcm)
      real covmr_gcm(ngridgcm,n_gcm), o3pvmr_gcm(ngridgcm,n_gcm)
      real q15umco2_gcm(ngridgcm,n_gcm)
!     real auxgcm(n_gcm)
      real*8 auxgcmd(n_gcm), aux2gcmd(n_gcm)
      real zmin_gcm
      integer ierr
      real*8 varerr



c     local variables and constants
      integer   i,ig,l, indice, nl_cts_real, nzy_cts_real    
      real*8	  q15umco2_nltot(nltot),  zld(nltot)
      real*8	  hr110CTS(nl_cts)
      real      xx,factor

      real p_ig(n_gcm),z_ig(n_gcm)
      real t_ig(n_gcm)
      real co2_ig(n_gcm),n2_ig(n_gcm),co_ig(n_gcm),o3p_ig(n_gcm)
      real mmean_ig(n_gcm),cpnew_ig(n_gcm)


c***************
c***************

      do ig=1,ngridgcm
         ierr = 0
         nl_cts_real = 0
         nzy_cts_real = 0
         do l=1,n_gcm
            p_ig(l)=p_gcm(ig,l)
            t_ig(l)=t_gcm(ig,l)
            co2_ig(l)=co2vmr_gcm(ig,l)
            n2_ig(l)=n2vmr_gcm(ig,l)
            o3p_ig(l)=o3pvmr_gcm(ig,l)
            co_ig(l)=covmr_gcm(ig,l)
            z_ig(l)=z_gcm(ig,l)/1000.
            mmean_ig(l)=mmean(ig,l)
            cpnew_ig(l)=cpnew(ig,l)
         enddo 

                                ! From GCM's grid to NLTE's grid
         call NLTEdlvr11_ZGRID (n_gcm,
     $        p_ig, t_ig, z_ig,
     $        co2_ig, n2_ig, co_ig, o3p_ig,
     $        mmean_ig,cpnew_ig,
     $        nl_cts_real, nzy_cts_real )


                                ! Isotopic Tstar & VC at the NLTE grid
         call interdp_ESCTVCISO

                                ! Tstar para NLTE-CTS
         call MZESC110 ( ig,nl_cts_real, nzy_cts_real,ierr,varerr ) 
         if (ierr .gt. 0) call ERRORS (ierr,varerr)

         ! 626FB C.M.
         call leetvt
         c110(1:nl,1:nl)=0.d0
!         call zerom (c110, nl)
         call zero2v (vc110,taustar11, nl)
         call MZTUD110 ( ierr, varerr )
         if (ierr .gt. 0) call ERRORS (ierr,varerr)

         input_cza = 0
         call NLTEdlvr11_CZALU(ierr,varerr)
         if (ierr .gt. 0) call ERRORS (ierr,varerr)

         input_cza = 1
         call NLTEdlvr11_CZALU(ierr,varerr)
         if (ierr .gt. 0) call ERRORS (ierr,varerr)

                                !  call NLTEdlvr11_FB626CTS
                                ! Falta un merging del hr110CTS con el HR110


!     ! Interpolation of Tstar11(nl) to the GCM grid (será auxgcm)
!     ! solo entre jlowerboundary y jtopboundary (la extension del NLTE
!     ! model)
!     call interhuntlimits( auxgcm, p_gcm,n_gcm,
!     @                        jlowerboundary,jtopboundary,
!     @                        taustar11, pl,   nl, 3 )
!     ! Mejor inter+extra polacion de Tstar11(nl) to the GCM grid
!     call TSTAR11_extension (n_gcm, p_gcm, auxgcm )

                                ! NLTE-CTS
         call NLTEdlvr11_FB626CTS ( hr110CTS , nl_cts_real )



                                ! total TCR
         do i = 1, nl
            q15umco2_nltot(i) =hr110(i) + hr210(i) + hr310(i) + hr410(i)
     @           + hr121(i)
         enddo

         
                                ! Merging con / actualizacion del HR_total 
                                !   Eliminamos el ultimo pto de hrTotal, y en el penultimo 
                                !   (que coincide con i=1 en el grid nl_cts)
                                !   hacemos la media entre hrTotal y hr110CTS :
         i=nl-1
         q15umco2_nltot(i) = 0.5d0*( q15umco2_nltot(i) + hr110CTS(1) )
         do i=2,nl_cts_real
            indice = (nl-2) + i
            q15umco2_nltot(indice) = hr110CTS(i)
         enddo 
         do i=nl_cts_real+1,nl_cts
            indice = (nl-2) + i
	    q15umco2_nltot(indice) = 0.0d0
         enddo

                                ! Interpol to original Pgrid
                                !  
                                ! Primero, la parte conocida ([1,nl_cts_real])
         do i=1,nl
            zld(i) = - dble ( alog(pl(i)) )
                                !write (*,*) i, zld(i), q15umco2_nltot(i)
         enddo
         do i=3,nl_cts_real
            indice = (nl-2) + i
            zld(indice) = - dble ( alog(pl_cts(i)) )
                                !write (*,*) indice, zld(indice), q15umco2_nltot(indice)
         enddo
                                ! En caso que nl_cts_real < nl_cts , extrapolo el grid alegremente
         factor = pl_cts(nl_cts_real)/pl_cts(nl_cts_real-1)
         xx = pl_cts(nl_cts_real) 
         do i=nl_cts_real+1,nl_cts 
            indice = (nl-2) + i
            xx = xx * factor  
            zld(indice) = - dble ( alog(xx) )
         enddo

         do i=1,n_gcm
            auxgcmd(i) = - dble( alog(p_gcm(ig,i)) )
         enddo
!         call zerov( aux2gcmd, n_gcm )
         aux2gcmd(1:n_gcm)=0.d0
         call interdp_limits
     $        (     aux2gcmd, auxgcmd, n_gcm,   jlowerboundary,jtopCTS,
     $        q15umco2_nltot,     zld, nltot,                1,  nltot,
     $        1 )

                                ! Smoothing
         call suaviza ( aux2gcmd, n_gcm, 1, auxgcmd )

         do i=1,n_gcm
            q15umco2_gcm(ig,i) = sngl( aux2gcmd(i) )
         enddo

      enddo
c     end subroutine
      return
      end


c***********************************************************************

      subroutine NLTEdlvr11_ZGRID (n_gcm,
     $     p_gcm, t_gcm, z_gcm, co2vmr_gcm, n2vmr_gcm, 
     $     covmr_gcm, o3pvmr_gcm, mmean_gcm,cpnew_gcm,
     $     nl_cts_real, nzy_cts_real )

c***********************************************************************

      implicit none
      
      include 'nlte_paramdef.h'
      include 'nlte_commons.h'

c     Arguments
      integer n_gcm             ! I
      real p_gcm(n_gcm), t_gcm(n_gcm) ! I
      real co2vmr_gcm(n_gcm), n2vmr_gcm(n_gcm) ! I
      real covmr_gcm(n_gcm), o3pvmr_gcm(n_gcm) ! I
      real z_gcm(n_gcm)         ! I
      real mmean_gcm(n_gcm)     ! I
      real cpnew_gcm(n_gcm)     ! I
      integer   nl_cts_real, nzy_cts_real ! O
      real zaux_gcm(n_gcm)

c     local variables
      integer i, iz 
      real  distancia, meanm, gz, Hkm
      real  zmin, zmax
      real  mmean_nlte(n_gcm),cpnew_nlte(n_gcm)
      real  gg,masa,radio,kboltzman

c     functions
      external 	hrkday_convert
      real 	hrkday_convert

c***********************************************************************

!     Define el working grid para MZ1D (NL, ZL, ZMIN)
!     y otro mas fino para M.Curtis (NZ, ZX, ZXMIN = ZMIN)
!     Tambien el working grid para MZESC110 (NL_cts, ZL_cts, ZMIN_cts=??)
!     Para ello hace falta una z de ref del GCM, que voy a suponer la inferior

!     Primero, construimos escala z_gcm

!      zaux_gcm(1) = z_gcm(1)             ! [km]
!      gg=6.67259e-8
!      masa=6.4163e26 
!      radio=3390.
!      kboltzman=1.381e-16 

!      do iz = 2, n_gcm
!         distancia = ( radio + zaux_gcm(iz-1) )*1.e5
!         gz = gg * masa / ( distancia * distancia )
!         Hkm = 0.5*( t_gcm(iz)+t_gcm(iz-1) ) / 
!     $        ( mmean_gcm(iz)/6.023e23 * gz )
!         Hkm = kboltzman * Hkm *1e-5 ! [km]
!         zaux_gcm(iz) = zaux_gcm(iz-1) - 
!     $        Hkm * log( p_gcm(iz)/p_gcm(iz-1) )
!      enddo
      

!     Segundo, definimos los límites de los 2 modelos de NLTE.
!     NLTE model completo: indices [jlowerboundary,jtopboundary]
!     NLTE CTS : indices [jbotCTS,jtopCTS] donde jbotCTS = jtopboundary-2

!!!!!!!!!Primero el NLTE completo  !!!!!!!!

                                ! Bottom boundary for NLTE model :
                                !   Pbot_atm = 2e-2 mb = 1.974e-5 atm , lnp(nb)=9.9   (see mz1d.par)
      jlowerboundary = 1
      do while ( p_gcm(jlowerboundary) .gt. Pbottom_atm )
         jlowerboundary = jlowerboundary + 1
         if (jlowerboundary .gt. n_gcm) then
            write (*,*) 'Error in lower boundary pressure.'
            write (*,*) ' p_gcm too low or wrong. '
	    write (*,*) ' p_gcm, Pbottom_atm =',
     $           p_gcm(n_gcm), Pbottom_atm
            stop ' Check input value "p_gcm" or modify "Pbottom_atm" '
         endif
      enddo

                                ! Top boundary for NLTE model :
                                !   Ptop_atm = 1e-9 atm                          (see mz1d.par)
      jtopboundary = jlowerboundary
      do while ( p_gcm(jtopboundary) .gt. Ptop_atm )
         jtopboundary = jtopboundary + 1
         if (jtopboundary .gt. n_gcm) then
            write (*,*) '!!!!!!!! Warning in top boundary pressure. '
            write (*,*) ' Ptop_atm too high for p_gcm. '
            write (*,*) ' p_gcm, Ptop_atm =',
     $           p_gcm(n_gcm), Ptop_atm
            write (*,*) '!!!!!!!! NLTE upper boundary modified '//
     $           ' to match p_gcm'
            jtopboundary=n_gcm
            goto 5000
         endif
      enddo
 5000 continue

                                ! Grid steps

      zmin = z_gcm(jlowerboundary)
      zmax = z_gcm(jtopboundary)
      deltaz = (zmax-zmin) / (nl-1)
      do i=1,nl
         zl(i) = zmin + (i-1) * deltaz
      enddo


                                ! Creamos el perfil del NLTE modelo completo interpolando

      call interhunt (    pl,zl,nl,      p_gcm,z_gcm,n_gcm, 2) ! [atm]
      call interhunt5veces
     $     ( t, co2vmr, n2vmr, covmr, o3pvmr,
     $     zl, nl,
     $     t_gcm, co2vmr_gcm, n2vmr_gcm, covmr_gcm, o3pvmr_gcm,
     $     z_gcm, n_gcm,
     $     1 )
      call interhunt ( mmean_nlte,zl,nl,mmean_gcm,z_gcm,n_gcm,1)
      call interhunt ( cpnew_nlte,zl,nl,cpnew_gcm,z_gcm,n_gcm,1)

      do i = 1, nl
         nt(i) = 7.339e+21 * pl(i) / t(i) ! --> [cm-3]
         co2(i) = nt(i) * co2vmr(i)
         n2(i) = nt(i) * n2vmr(i)
         co(i) = nt(i) * covmr(i)
         o3p(i) = nt(i) * o3pvmr(i)
!     hrkday_factor(i) =  hrkday_convert( t(i),
!     $        	  co2vmr(i), o3pvmr(i), n2vmr(i), covmr(i) )
         hrkday_factor(i) = hrkday_convert(mmean_nlte(i)
     &        ,cpnew_nlte(i))
      enddo
      
                                !  Comprobar que las temps no se salen del grid del histograma

      do i=1,nl
         if (t(i) .gt. 500.0) then
            write (*,*) '!!!! WARNING    Temp higher than Histogram.'
            write (*,*) ' Histogram will be extrapolated. '
            write (*,*) ' i, t(i), pl(i) =', i, t(i), pl(i)
         endif
         if (t(i) .lt. 50.0) then
            write (*,*) '!!!! WARNING    Temp lower than Histogram.'
            write (*,*) ' Histogram will be extrapolated. '
            write (*,*) ' i, t(i), pl(i) =', i, t(i), pl(i)
         endif
      enddo

                                !  Fine grid for transmittance calculations

      zmin = z_gcm(jlowerboundary)
      zmax = z_gcm(jtopboundary)
      deltazy = (zmax-zmin) / (nzy-1)
      do i=1,nzy
         zy(i) = zmin + (i-1) * deltazy
      enddo
      call interhunt (    py,zy,nzy,      p_gcm,z_gcm,n_gcm, 2) ! [atm]
      call interhunt2veces ( ty,co2y, zy,nzy,
     $     t_gcm,co2vmr_gcm, z_gcm,n_gcm, 1)

      do i=1,nzy
         nty(i) = 7.339e+21 * py(i) / ty(i) ! --> [cm-3]
         co2y(i) = co2y(i) * nty(i)
      enddo


!!!!!!!!!Segundo, el NLTE - CTS  !!!!!!!!

                                ! Grid steps
      deltaz_cts = deltaz
      zl_cts(1) = zl(nl-1)
      nl_cts_real = 1
      do i=2,nl_cts
         zl_cts(i) = zl_cts(1) + (i-1)*deltaz_cts
         if (zl_cts(i) .gt. z_gcm(n_gcm)) then
!            write (*,*) '!!!!!!!! Warning in top CTS layers. '
!            write (*,*) ' zl_Cts too high for z_gcm. '
!            write (*,*) ' z_gcm, zl_cts(i), i =',
!     $           z_gcm(n_gcm), zl_cts(i), i
!            write (*,*) '!!!!!!!! NLTE-CTS upper boundary modified '//
!     $           ' to match z_gcm'
            nl_cts_real=i-1
!            write (*,*) '  Original,Real NL_CTS=', nl_cts,nl_cts_real
            goto 6000
         endif
      enddo
      nl_cts_real = nl_cts
 6000 continue
      
                                ! Creamos perfil por interpolacion

      call interhuntlimits ( pl_cts,zl_cts,nl_cts, 1,nl_cts_real,
     $     p_gcm,z_gcm,n_gcm, 2)
      call interhuntlimits5veces
     $     ( t_cts, co2vmr_cts, n2vmr_cts, covmr_cts, o3pvmr_cts,
     $     zl_cts, nl_cts,
     $     1,nl_cts_real,
     $     t_gcm, co2vmr_gcm, n2vmr_gcm, covmr_gcm, o3pvmr_gcm,
     $     z_gcm, n_gcm,
     $     1 )
      call interhuntlimits( cpnew_cts,zl_cts,nl_cts,1,nl_cts_real,
     $     cpnew_gcm,z_gcm,n_gcm, 1)
      call interhuntlimits( mmean_cts,zl_cts,nl_cts,1,nl_cts_real,
     $     mmean_gcm,z_gcm,n_gcm, 1)

      do i = 1, nl_cts_real
         nt_cts(i) = 7.339e+21 * pl_cts(i) / t_cts(i) ! --> [cm-3]
         co2_cts(i) = nt_cts(i) * co2vmr_cts(i)
         n2_cts(i) = nt_cts(i) * n2vmr_cts(i)
         co_cts(i) = nt_cts(i) * covmr_cts(i)
         o3p_cts(i) = nt_cts(i) * o3pvmr_cts(i)
         hrkday_factor_cts(i) =  hrkday_convert( mmean_cts(i)
     &        ,cpnew_cts(i) )
      enddo

                                !  Comprobar que las temps no se salen del grid del histograma
      do i=1,nl_cts_real
         if (t_cts(i) .gt. thist_stored(1,mm_stored(1))) then
            write (*,*) '!!!! WARNING    Temp higher than Histogram.'
            write (*,*) ' ZGRID: Histogram will be extrapolated. '
            write (*,*) ' i, t(i), pl(i) =', i, t_cts(i), pl_cts(i)
         endif
         if (t_cts(i) .lt. 50.0) then
            write (*,*) '!!!! WARNING    Temp lower than Histogram.'
            write (*,*) ' ZGRID: Histogram will be extrapolated. '
            write (*,*) ' i, t(i), pl(i) =', i, t_cts(i), pl_cts(i)
         endif
      enddo

                                ! Calculo del indice maximo del GCM hasta donde llega el NLTE-CTS
      jtopCTS = jtopboundary
      do while ( p_gcm(jtopCTS) .gt. pl_cts(nl_cts_real) )
         jtopCTS = jtopCTS + 1
         if (jtopCTS .gt. n_gcm) then
            write (*,*) '!!!!!!!! Warning in top boundary pressure. '
            write (*,*) ' Ptop_NLTECTS too high for p_gcm. '
            write (*,*) ' p_gcm, Ptop_NLTECTS =',
     $           p_gcm(n_gcm), pl_cts(nl_cts_real)
            write (*,*) '!!!!!!!! NLTE-CTS upper boundary modified '//
     $           ' to match p_gcm'
            jtopCTS=n_gcm
            goto 7000
         endif
      enddo
 7000 continue

                                !  Fine grid for transmittance calculations

      deltazy_cts = 0.25*deltaz_cts ! Comprobar el factor 4 en mz1d.par
      do i=1,nzy_cts
         zy_cts(i) = zl_cts(1) + (i-1) * deltazy_cts
      enddo
      nzy_cts_real = (nl_cts_real - 1)*4 + 1
      call interhuntlimits ( py_cts,zy_cts,nzy_cts, 1,nzy_cts_real,
     $     p_gcm, z_gcm, n_gcm,   2) ! [atm]
      call interhuntlimits2veces
     $     ( ty_cts,co2y_cts, zy_cts,nzy_cts,  1,nzy_cts_real,
     $     t_gcm,co2vmr_gcm, z_gcm,n_gcm, 1)

      do i=1,nzy_cts_real
         nty_cts(i) = 7.339e+21 * py_cts(i) / ty_cts(i) ! --> [cm-3]
         co2y_cts(i) = co2y_cts(i) * nty_cts(i)
      enddo

!     write (*,*) '  NL = ', NL
!     write (*,*) '  Original,Real NL_CTS=', nl_cts,nl_cts_real
!     write (*,*) '  Original,Real NZY_CTS =', nzy_cts,nzy_cts_real



c     end
      return
      end


c     *** Old NLTEdlvr11_CZALU_03 ***

c**********************************************************************


      subroutine NLTEdlvr11_CZALU(ierr,varerr)

c***********************************************************************

      implicit none

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!common variables and constants

      include 'nlte_paramdef.h'
      include 'nlte_commons.h'


c     Arguments

      integer ierr
      real*8 varerr


c     local variables

!     matrixes and vectors

      real*8 e110(nl), e210(nl), e310(nl), e410(nl)
      real*8 e121(nl)
      real*8 f1(nl,nl)

      real*8 cax1(nl,nl), cax2(nl,nl), cax3(nl,nl)
      real*8 v1(nl), v2(nl), v3(nl)
      real*8 alf11(nl,nl), alf12(nl,nl)
      real*8 alf21(nl,nl), alf31(nl,nl), alf41(nl,nl)
      real*8 a11(nl), a1112(nl,nl)
      real*8 		a1121(nl,nl), a1131(nl,nl), a1141(nl,nl)
      real*8 a21(nl), a2131(nl,nl), a2141(nl,nl)
      real*8 		a2111(nl,nl), a2112(nl,nl)
      real*8 a31(nl), a3121(nl,nl), a3141(nl,nl)
      real*8 		a3111(nl,nl), a3112(nl,nl)
      real*8 a41(nl), a4121(nl,nl), a4131(nl,nl)
      real*8 		a4111(nl,nl), a4112(nl,nl)
      real*8 a12(nl), a1211(nl,nl)
      real*8 		a1221(nl,nl), a1231(nl,nl), a1241(nl,nl)

      real*8 aalf11(nl,nl),aalf21(nl,nl),
     @     aalf31(nl,nl),aalf41(nl,nl)
      real*8 aa11(nl), aa1121(nl,nl), aa1131(nl,nl), aa1141(nl,nl)
      real*8 aa21(nl), aa2111(nl,nl), aa2131(nl,nl), aa2141(nl,nl)
      real*8 aa31(nl), aa3111(nl,nl), aa3121(nl,nl), aa3141(nl,nl)
      real*8 aa41(nl), aa4111(nl,nl), aa4121(nl,nl), aa4131(nl,nl)
      real*8 aa1211(nl,nl),aa1221(nl,nl),
     @     aa1231(nl,nl),aa1241(nl,nl)
      real*8 aa1112(nl,nl),aa2112(nl,nl),
     @     aa3112(nl,nl),aa4112(nl,nl)

      real*8 aaalf11(nl,nl), aaalf31(nl,nl), aaalf41(nl,nl)
      real*8 aaa11(nl),aaa1131(nl,nl),aaa1141(nl,nl)
      real*8 aaa31(nl),aaa3111(nl,nl),aaa3141(nl,nl)
      real*8 aaa41(nl),aaa4111(nl,nl),aaa4131(nl,nl)

      real*8 aaaalf11(nl,nl),aaaalf41(nl,nl)
      real*8 aaaa11(nl),aaaa1141(nl,nl)
      real*8 aaaa41(nl),aaaa4111(nl,nl)


!     populations
      real*8 n10(nl), n11(nl), n12(nl)
      real*8 n20(nl), n21(nl)
      real*8 n30(nl), n31(nl)
      real*8 n40(nl), n41(nl)

!     productions and loses
      real*8 d19b1,d19c1
      real*8 d19bp1,d19cp1
      real*8 d19c2
      real*8 d19cp2
      real*8 d19c3
      real*8 d19cp3
      real*8 d19c4
      real*8 d19cp4

      real*8 l11, l12, l21, l31, l41
      real*8 p11, p12, p21, p31, p41
      real*8 p1112, p1211, p1221, p1231, p1241
      real*8 p1121, p1131, p1141
      real*8 p2111, p2112, p2131, p2141
      real*8 p3111, p3112, p3121, p3141
      real*8 p4111, p4112, p4121, p4131

      real*8 pl11, pl12, pl21, pl31, pl41

      real*8 minvt11, minvt21, minvt31, minvt41

c     local constants and indexes

      real*8 co2t, o3pdbl, codble, n2dble
      real*8 a12_einst(nl)
      real*8 a21_einst(nl), a31_einst(nl), a41_einst(nl)
      real tsurf

      integer i, isot

c     external functions and subroutines

      external planckdp
      real*8 	planckdp


!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!start program

      ierr = 0
      varerr = 0.d0

      call zero4v( aa11, aa21, aa31, aa41, nl)
      call zero4m( aa1121, aa1131, aa1141, aalf11, nl)
      call zero4m( aa2111, aa2131, aa2141, aalf21, nl)
      call zero4m( aa3111, aa3121, aa3141, aalf31, nl)
      call zero4m( aa4111, aa4121, aa4131, aalf41, nl)
      call zero4m( aa1112, aa2112, aa3112, aa4112, nl)
      call zero4m( aa1211, aa1221, aa1231, aa1241, nl)
      call zero3v( aaa41, aaa31, aaa11, nl )
      call zero3m( aaa4111, aaa4131, aaalf41, nl)
      call zero3m( aaa3111, aaa3141, aaalf31, nl)
      call zero3m( aaa1131, aaa1141, aaalf11, nl)
      call zero2v( aaaa11, aaaa41, nl )
      call zero2m( aaaa1141, aaaalf11, nl)
      call zero2m( aaaa4111, aaaalf41, nl)

      call zero2v (vt11,vt12,nl)
      call zero3v (vt21,vt31,vt41,nl)
      call zero2v (hr110,hr121,nl)
      call zero3v (hr210,hr310,hr410,nl)
      call zero2v (sl110,sl121,nl)
      call zero3v (sl210,sl310,sl410,nl)

      call zero4v (el11,el21,el31,el41,nl)
      call zero4v (e110,e210,e310,e410,nl)
      call zero2v (el12,e121,nl)

      call zero3m (cax1,cax2,cax3,nl)
      f1(1:nl,1:nl)=0.d0
!      call zerom (f1,nl)

      call zero3v (v1,v2,v3,nl)

      call zero4m (alf11,alf21,alf31,alf41,nl)
      alf12(1:nl,1:nl)=0.d0
!      call zerom (alf12,nl)
      call zero2v (a11,a12,nl)
      call zero3v (a21,a31,a41,nl)

      call zero3m (a1121,a1131,a1141,nl)
      a1112(1:nl,1:nl)=0.d0
!      call zerom (a1112,nl)

      call zero3m (a1221,a1231,a1241,nl)
      a1211(1:nl,1:nl)=0.d0
!      call zerom (a1211,nl)

      call zero2m (a2111,a2112,nl)
      call zero2m (a2131,a2141,nl)
      call zero2m (a3111,a3112,nl)
      call zero2m (a3121,a3141,nl)
      call zero2m (a4111,a4112,nl)
      call zero2m (a4121,a4131,nl)

      call zero2v (n11,n12,nl)
      call zero3v (n21,n31,n41,nl)

      nu11 = dble(nu(1,1))
      nu12 = dble(nu(1,2))
      nu121 =  nu12-nu11
      nu21 =  dble(nu(2,1))
      nu31 =  dble(nu(3,1))
      nu41 =  dble(nu(4,1))

c     
c     
      do i=1,nl
         n10(i) = dble( co2(i) * imr(1) )
         n20(i) = dble( co2(i) * imr(2) )
         n30(i) = dble( co2(i) * imr(3) )
         n40(i) = dble( co2(i) * imr(4) )
         if ( input_cza.ge.1 ) then
	    n11(i) = n10(i) *2.d0 *exp( -ee*nu11/v626t1(i) )
	    n21(i) = n20(i) *2.d0 *exp( -ee*nu21/v628t1(i) )
	    n31(i) = n30(i) *2.d0* exp( -ee*nu31/v636t1(i) )
	    n41(i) = n40(i) *2.d0* exp( -ee*nu41/v627t1(i) )
         end if
      enddo

c     
c     curtis matrix calculation
c     
      call zero3m (c210,c310,c410, nl)

      if ( input_cza.ge.1 ) then

         if (itt_cza.eq.15 ) then

	    call MZMC121

         elseif (itt_cza.eq.13) then

!            call zerom ( c121, nl )
            c121(1:nl,1:nl)=0.d0
            call MZESC121
            call MZTVC121( ierr,varerr )
            if (ierr .gt. 0) call ERRORS (ierr,varerr)

         endif

      endif

                                ! Lower Boundary
      tsurf = t(1)
      do i=1,nl
         sl110(i) = vc110(i) * planckdp( tsurf, nu11 )
         sl210(i) = vc210(i) * planckdp( tsurf, nu21 )
         sl310(i) = vc310(i) * planckdp( tsurf, nu31 )
         sl410(i) = vc410(i) * planckdp( tsurf, nu41 )
      end do
      if (input_cza.ge.1) then
         do i=1,nl
	    sl121(i) = vc121(i) * planckdp( tsurf, nu121 )
         end do
      endif



      do 4,i=nl,1,-1            !----------------------------------------------

         co2t = dble( co2(i) *(imr(1)+imr(3)+imr(2)+imr(4)) )
         o3pdbl = dble( o3p(i) )
         n2dble = dble( n2(i) )
         codble = dble ( co(i) )

         call GETK_dlvr11 ( t(i) )

                                ! V-T productions and losses V-T

         isot = 1
         d19b1 = k19ba(isot)*co2t + k19bb(isot)*n2dble
     @        + k19bc(isot)*codble
         d19c1 = k19ca(isot)*co2t + k19cb(isot)*n2dble
     @        + k19cc(isot)*codble
         d19bp1 = k19bap(isot)*co2t + k19bbp(isot)*n2dble
     @        + k19bcp(isot)*codble
         d19cp1 = k19cap(isot)*co2t + k19cbp(isot)*n2dble
     @        + k19ccp(isot)*codble
         isot = 2
         d19c2 = k19ca(isot)*co2t + k19cb(isot)*n2dble
     @        + k19cc(isot)*codble
         d19cp2 = k19cap(isot)*co2t + k19cbp(isot)*n2dble
     @        + k19ccp(isot)*codble
         isot = 3
         d19c3 = k19ca(isot)*co2t + k19cb(isot)*n2dble
     @        + k19cc(isot)*codble
         d19cp3 = k19cap(isot)*co2t + k19cbp(isot)*n2dble
     @        + k19ccp(isot)*codble
         isot = 4
         d19c4 = k19ca(isot)*co2t + k19cb(isot)*n2dble
     @        + k19cc(isot)*codble
         d19cp4 = k19cap(isot)*co2t + k19cbp(isot)*n2dble
     @        + k19ccp(isot)*codble
                                !
         l11 = d19c1 + k20c(1)*o3pdbl
         p11 = ( d19cp1 + k20cp(1)*o3pdbl ) * n10(i)
         l21 = d19c2 + k20c(2)*o3pdbl
         p21 = ( d19cp2 + k20cp(2)*o3pdbl ) *n20(i)
         l31 = d19c3 + k20c(3)*o3pdbl
         p31 = ( d19cp3 + k20cp(3)*o3pdbl ) *n30(i)
         l41 = d19c4 + k20c(4)*o3pdbl
         p41 = ( d19cp4 + k20cp(4)*o3pdbl ) *n40(i)

                                ! Addition of V-V

         l11 = l11 + k21cp(2)*n20(i) + k21cp(3)*n30(i)
     @        + k21cp(4)*n40(i)
         p1121 = k21c(2) * n10(i)
         p1131 = k21c(3) * n10(i)
         p1141 = k21c(4) * n10(i)
                                !
         l21 = l21 + k21c(2)*n10(i) + k23k21c*n30(i) + k24k21c*n40(i)
         p2111 = k21cp(2) * n20(i)
         p2131 = k23k21cp * n20(i)
         p2141 = k24k21cp * n20(i)
                                !
         l31 = l31 + k21c(3)*n10(i) + k23k21cp*n20(i) + k34k21c*n40(i)
         p3111 = k21cp(3)* n30(i)
         p3121 = k23k21c * n30(i)
         p3141 = k34k21cp* n30(i)
                                !
         l41 = l41 + k21c(4)*n10(i) + k24k21cp*n20(i) + k34k21cp*n30(i)
         p4111 = k21cp(4)* n40(i)
         p4121 = k24k21c * n40(i)
         p4131 = k34k21c * n40(i)


         if ( input_cza.ge.1 ) then

	    l12 = d19b1
     @           + k20b(1)*o3pdbl
     @           + k21b(1)*n10(i)
     @           + k33c*( n20(i) + n30(i) + n40(i) )
	    p12 = k21bp(1)*n11(i) * n11(i)
	    p1211 = d19bp1 + k20bp(1)*o3pdbl
	    p1221 = k33cp(2)*n11(i)
	    p1231 = k33cp(3)*n11(i)
	    p1241 = k33cp(4)*n11(i)

	    l11 = l11 + d19bp1
     @           + k20bp(1)*o3pdbl
     @           + 2.d0 * k21bp(1) * n11(i)
     @           +   k33cp(2)*n21(i) + k33cp(3)*n31(i) + k33cp(4)*n41(i)
	    p1112 = d19b1
     @           + k20b(1)*o3pdbl
     @           + 2.d0*k21b(1)*n10(i)
     @           + k33c*( n20(i) + n30(i) + n40(i) )

	    l21 = l21 + k33cp(2)*n11(i)
	    p2112 = k33c*n20(i)

	    l31 = l31 + k33cp(3)*n11(i)
	    p3112 = k33c*n30(i)

	    l41 = l41 + k33cp(4)*n11(i)
	    p4112 = k33c*n40(i)

         end if

  
         ! For ITT=13,15
         a21_einst(i) = a2_010_000 * 1.8d0 / 4.d0 * taustar21(i)
         a31_einst(i) = a3_010_000 * 1.8d0 / 4.d0 * taustar31(i)
         a41_einst(i) = a4_010_000 * 1.8d0 / 4.d0 * taustar41(i)

         l21 = l21 + a21_einst(i)
         l31 = l31 + a31_einst(i)
         l41 = l41 + a41_einst(i)

         ! For ITT=13
         if (input_cza.ge.1 .and. itt_cza.eq.13) then
            a12_einst(i) = a1_020_010/3.d0 * 1.8d0/4.d0 * taustar12(i)
            l12=l12+a12_einst(i)
         endif


         ! Checking for collisional severe errors
         if (l11 .le. 0.0d0) then
            ierr = 21
            varerr = l11
            return
         elseif (l21 .le. 0.0d0) then
            ierr = 22
            varerr = l21
            return
         elseif (l31 .le. 0.0d0) then
            ierr = 23
            varerr = l31
            return
         elseif (l41 .le. 0.0d0) then
            ierr = 24
            varerr = l41
            return
         endif
         if (input_cza.ge.1) then
	    if (l12 .lt. 0.0d0) then
               ierr = 25
               varerr = l12
               return
            endif
         endif
         !   

         a11(i) = gamma*nu11**3.d0 * 1.d0/2.d0 * (p11) /
     @        (n10(i)*l11)
         a1121(i,i) = (nu11/nu21)**3.d0 * n20(i)/n10(i) * p1121/l11
         a1131(i,i) = (nu11/nu31)**3.d0 * n30(i)/n10(i) * p1131/l11
         a1141(i,i) = (nu11/nu41)**3.d0 * n40(i)/n10(i) * p1141/l11
         e110(i) = 2.d0* vlight*nu11**2.d0 * 1.d0/2.d0 /
     @        ( n10(i) * l11 )

         a21(i) = gamma*nu21**3.d0 * 1.d0/2.d0 *
     @        (p21)/(n20(i)*l21)
         a2111(i,i) = (nu21/nu11)**3.d0 * n10(i)/n20(i) * p2111/l21
         a2131(i,i) = (nu21/nu31)**3.d0 * n30(i)/n20(i) * p2131/l21
         a2141(i,i) = (nu21/nu41)**3.d0 * n40(i)/n20(i) * p2141/l21
         e210(i) = 2.d0*vlight*nu21**2.d0 * 1.d0/2.d0 /
     @        ( n20(i) * l21 )

         a31(i) = gamma*nu31**3.d0 * 1.d0/2.d0 * (p31) /
     @        (n30(i)*l31)
         a3111(i,i) = (nu31/nu11)**3.d0 * n10(i)/n30(i) * p3111/l31
         a3121(i,i) = (nu31/nu21)**3.d0 * n20(i)/n30(i) * p3121/l31
         a3141(i,i) = (nu31/nu41)**3.d0 * n40(i)/n30(i) * p3141/l31
         e310(i) = 2.d0*vlight*nu31**2.d0 * 1.d0/2.d0 /
     @        ( n30(i) * l31 )

         a41(i) = gamma*nu41**3.d0 * 1.d0/2.d0 * (p41) /
     @        (n40(i)*l41)
         a4111(i,i) = (nu41/nu11)**3.d0 * n10(i)/n40(i) * p4111/l41
         a4121(i,i) = (nu41/nu21)**3.d0 * n20(i)/n40(i) * p4121/l41
         a4131(i,i) = (nu41/nu31)**3.d0 * n30(i)/n40(i) * p4131/l41
         e410(i) = 2.d0*vlight*nu41**2.d0 * 1.d0/2.d0 /
     @        ( n40(i) * l41 )

         if (input_cza.ge.1) then

	    a1112(i,i) = (nu11/nu121)**3.d0 * n11(i)/n10(i) *
     @           p1112/l11
	    a2112(i,i) = (nu21/nu121)**3.d0 * n11(i)/n20(i) *
     @           p2112/l21
	    a3112(i,i) = (nu31/nu121)**3.d0 * n11(i)/n30(i) *
     @           p3112/l31
	    a4112(i,i) = (nu41/nu121)**3.d0 * n11(i)/n40(i) *
     @           p4112/l41
	    a12(i) = gamma*nu121**3.d0 *2.d0/4.d0* (p12)/
     @           (n11(i)*l12)
	    a1211(i,i) = (nu121/nu11)**3.d0 * n10(i)/n11(i) *
     @           p1211/l12
	    a1221(i,i) = (nu121/nu21)**3.d0 * n20(i)/n11(i) *
     @           p1221/l12
	    a1231(i,i) = (nu121/nu31)**3.d0 * n30(i)/n11(i) *
     @           p1231/l12
	    a1241(i,i) = (nu121/nu41)**3.d0 * n40(i)/n11(i) *
     @           p1241/l12
	    e121(i) = 2.d0*vlight*nu121**2.d0 *2.d0/4.d0 /
     @           ( n11(i) * l12 )

         end if


 4    continue                  !-------------------------------------------------------



                                !!!!!!!!!!!! Solucion del sistema

                                !! Paso 0 :  Calculo de los alphas   alf11, alf21, alf31, alf41, alf12

      call unit  ( cax2, nl )

      call diago ( cax1, e110, nl )
      call mulmmf90 ( cax3, cax1,c110, nl )
      call resmmf90 ( alf11, cax2,cax3, nl )

      call diago ( cax1, e210, nl )
      call mulmmf90 ( cax3, cax1,c210, nl )
      call resmmf90 ( alf21, cax2,cax3, nl )

      call diago ( cax1, e310, nl )
      call mulmmf90 ( cax3, cax1,c310, nl )
      call resmmf90 ( alf31, cax2,cax3, nl )

      call diago ( cax1, e410, nl )
      call mulmmf90 ( cax3, cax1,c410, nl )
      call resmmf90 ( alf41, cax2,cax3, nl )

      if (input_cza.ge.1) then
         call diago ( cax1, e121, nl )
         call mulmmf90 ( cax3, cax1,c121, nl )
         call resmmf90 ( alf12, cax2,cax3, nl )
      endif

                                !! Paso 1 :  Calculo de vectores y matrices con 1 barra (aa***)

      if (input_cza.eq.0) then  !  Skip paso 1, pues el12 no se calcula

                                ! el11
         call sypvvv( aa11, a11,e110,sl110, nl )
         call samem( aa1121, a1121, nl )
         call samem( aa1131, a1131, nl )
         call samem( aa1141, a1141, nl )
         call samem( aalf11, alf11, nl )

                                ! el21
         call sypvvv( aa21, a21,e210,sl210, nl )
         call samem( aa2111, a2111, nl )
         call samem( aa2131, a2131, nl )
         call samem( aa2141, a2141, nl )
         call samem( aalf21, alf21, nl )

                                ! el31
         call sypvvv( aa31, a31,e310,sl310, nl )
         call samem( aa3111, a3111, nl )
         call samem( aa3121, a3121, nl )
         call samem( aa3141, a3141, nl )
         call samem( aalf31, alf31, nl )

                                ! el41
         call sypvvv( aa41, a41,e410,sl410, nl )
         call samem( aa4111, a4111, nl )
         call samem( aa4121, a4121, nl )
         call samem( aa4131, a4131, nl )
         call samem( aalf41, alf41, nl )


      else                      !      (input_cza.ge.1) ,   FH !


         call sypvvv( v1, a12,e121,sl121, nl ) ! a12 + e121 * sl121

                                ! aa11
         call sypvvv( v2, a11,e110,sl110, nl )
         call trucommvv( aa11 , alf12,a1112,v2, v1, nl )

                                ! aalf11
         call invdiag( cax1, a1112, nl )
         call mulmmf90( cax2, alf12, cax1, nl ) ! alf12 * (1/a1112)
         call mulmmf90( cax3, cax2, alf11, nl )
         call resmmf90( aalf11, cax3, a1211, nl )
                                ! aa1121
         call trucodiag(aa1121, alf12,a1112,a1121, a1221, nl)
                                ! aa1131
         call trucodiag(aa1131, alf12,a1112,a1131, a1231, nl)
                                ! aa1141
         call trucodiag(aa1141, alf12,a1112,a1141, a1241, nl)


                                ! aa21
         call sypvvv( v2, a21,e210,sl210, nl )
         call trucommvv( aa21 , alf12,a2112,v2, v1, nl )

                                ! aalf21
         call invdiag( cax1, a2112, nl )
         call mulmmf90( cax2, alf12, cax1, nl ) ! alf12 * (1/a2112)
         call mulmmf90( cax3, cax2, alf21, nl )
         call resmmf90( aalf21, cax3, a1221, nl )
                                ! aa2111
         call trucodiag(aa2111, alf12,a2112,a2111, a1211, nl)
                                ! aa2131
         call trucodiag(aa2131, alf12,a2112,a2131, a1231, nl)
                                ! aa2141
         call trucodiag(aa2141, alf12,a2112,a2141, a1241, nl)


                                ! aa31
         call sypvvv ( v2, a31,e310,sl310, nl )
         call trucommvv( aa31 , alf12,a3112,v2, v1, nl )
                                ! aalf31
         call invdiag( cax1, a3112, nl )
         call mulmmf90( cax2, alf12, cax1, nl ) ! alf12 * (1/a3112)
         call mulmmf90( cax3, cax2, alf31, nl )
         call resmmf90( aalf31, cax3, a1231, nl )
                                ! aa3111
         call trucodiag(aa3111, alf12,a3112,a3111, a1211, nl)
                                ! aa3121
         call trucodiag(aa3121, alf12,a3112,a3121, a1221, nl)
                                ! aa3141
         call trucodiag(aa3141, alf12,a3112,a3141, a1241, nl)


                                ! aa41
         call sypvvv( v2, a41,e410,sl410, nl )
         call trucommvv( aa41 , alf12,a4112,v2, v1, nl )
                                ! aalf41
         call invdiag( cax1, a4112, nl )
         call mulmmf90( cax2, alf12, cax1, nl ) ! alf12 * (1/a4112)
         call mulmmf90( cax3, cax2, alf41, nl )
         call resmmf90( aalf41, cax3, a1241, nl )
                                ! aa4111
         call trucodiag(aa4111, alf12,a4112,a4111, a1211, nl)
                                ! aa4121
         call trucodiag(aa4121, alf12,a4112,a4121, a1221, nl)
                                ! aa4131
         call trucodiag(aa4131, alf12,a4112,a4131, a1231, nl)

      endif                     ! Final  caso input_cza.ge.1


                                !! Paso 2 :  Calculo de vectores y matrices con 2 barras (aaa***)

                                ! aaalf41
      call invdiag( cax1, aa4121, nl )
      call mulmmf90( cax2, aalf21, cax1, nl ) ! alf21 * (1/a4121)
      call mulmmf90( cax3, cax2, aalf41, nl )
      call resmmf90( aaalf41, cax3, aa2141, nl )
                                ! aaa41
      call trucommvv(aaa41, aalf21,aa4121,aa41, aa21, nl)
                                ! aaa4111
      call trucodiag(aaa4111, aalf21,aa4121,aa4111, aa2111, nl)
                                ! aaa4131
      call trucodiag(aaa4131, aalf21,aa4121,aa4131, aa2131, nl)

                                ! aaalf31
      call invdiag( cax1, aa3121, nl )
      call mulmmf90( cax2, aalf21, cax1, nl ) ! alf21 * (1/a3121)
      call mulmmf90( cax3, cax2, aalf31, nl )
      call resmmf90( aaalf31, cax3, aa2131, nl )
                                ! aaa31
      call trucommvv(aaa31, aalf21,aa3121,aa31, aa21, nl)
                                ! aaa3111
      call trucodiag(aaa3111, aalf21,aa3121,aa3111, aa2111, nl)
                                ! aaa3141
      call trucodiag(aaa3141, aalf21,aa3121,aa3141, aa2141, nl)

                                ! aaalf11
      call invdiag( cax1, aa1121, nl )
      call mulmmf90( cax2, aalf21, cax1, nl ) ! alf21 * (1/a1121)
      call mulmmf90( cax3, cax2, aalf11, nl )
      call resmmf90( aaalf11, cax3, aa2111, nl )
                                ! aaa11
      call trucommvv(aaa11, aalf21,aa1121,aa11, aa21, nl)
                                ! aaa1131
      call trucodiag(aaa1131, aalf21,aa1121,aa1131, aa2131, nl)
                                ! aaa1141
      call trucodiag(aaa1141, aalf21,aa1121,aa1141, aa2141, nl)


                                !! Paso 3 :  Calculo de vectores y matrices con 3 barras (aaaa***)

                                ! aaaalf41
      call invdiag( cax1, aaa4131, nl )
      call mulmmf90( cax2, aaalf31, cax1, nl ) ! aaalf31 * (1/aaa4131)
      call mulmmf90( cax3, cax2, aaalf41, nl )
      call resmmf90( aaaalf41, cax3, aaa3141, nl )
                                ! aaaa41
      call trucommvv(aaaa41, aaalf31,aaa4131,aaa41, aaa31, nl)
                                ! aaaa4111
      call trucodiag(aaaa4111, aaalf31,aaa4131,aaa4111,aaa3111, nl)

                                ! aaaalf11
      call invdiag( cax1, aaa1131, nl )
      call mulmmf90( cax2, aaalf31, cax1, nl ) ! aaalf31 * (1/aaa4131)
      call mulmmf90( cax3, cax2, aaalf11, nl )
      call resmmf90( aaaalf11, cax3, aaa3111, nl )
                                ! aaaa11
      call trucommvv(aaaa11, aaalf31,aaa1131,aaa11, aaa31, nl)
                                ! aaaa1141
      call trucodiag(aaaa1141, aaalf31,aaa1131,aaa1141,aaa3141, nl)


                                !! Paso 4 :  Calculo de vectores y matrices finales y calculo de J1

      call trucommvv(v1, aaaalf41,aaaa1141,aaaa11, aaaa41, nl)
                                !
      call invdiag( cax1, aaaa1141, nl )
      call mulmmf90( cax2, aaaalf41, cax1, nl ) ! aaaalf41 * (1/aaaa1141)
      call mulmmf90( cax3, cax2, aaaalf11, nl )
      call resmmf90( cax1, cax3, aaaa4111, nl )
                                !
      call LUdec ( el11, cax1, v1, nl, nl2 )

                                ! Solucion para el41
      call sypvmv( v1, aaaa41, aaaa4111,el11, nl )
      call LUdec ( el41, aaaalf41, v1, nl, nl2 )

                                ! Solucion para el31
      call sypvmv( v2, aaa31, aaa3111,el11, nl )
      call sypvmv( v1,    v2, aaa3141,el41, nl )
      call LUdec ( el31, aaalf31, v1, nl, nl2 )

                                ! Solucion para el21
      call sypvmv( v3, aa21, aa2111,el11, nl )
      call sypvmv( v2,   v3, aa2131,el31, nl )
      call sypvmv( v1,   v2, aa2141,el41, nl )
      call LUdec ( el21, aalf21, v1, nl, nl2 )

                                !!!
      el11(1) = planckdp( t(1), nu11 )
      el21(1) = planckdp( t(1), nu21 )
      el31(1) = planckdp( t(1), nu31 )
      el41(1) = planckdp( t(1), nu41 )
      el11(nl) = 2.d0 * el11(nl-1) - el11(nl2)
      el21(nl) = 2.d0 * el21(nl-1) - el21(nl2)
      el31(nl) = 2.d0 * el31(nl-1) - el31(nl2)
      el41(nl) = 2.d0 * el41(nl-1) - el41(nl2)

      call mulmv ( v1, c110,el11, nl )
      call sumvv ( hr110, v1,sl110, nl )

                                ! Solucion para el12
      if (input_cza.ge.1) then

         call sypvmv( v1, a12, a1211,el11, nl )
         call sypvmv( v3,  v1, a1221,el21, nl )
         call sypvmv( v2,  v3, a1231,el31, nl )
         call sypvmv( v1,  v2, a1241,el41, nl )
         call LUdec ( el12, alf12, v1, nl, nl2 )

         el12(1) = planckdp( t(1), nu121 )
         el12(nl) = 2.d0 * el12(nl-1) - el12(nl2)

         if (itt_cza.eq.15) then
            call mulmv ( v1, c121,el12, nl )
            call sumvv ( hr121, v1,sl121, nl )
         endif

      end if



      if (input_cza.lt.1) then

         minvt11 = 1.d6
         minvt21 = 1.d6
         minvt31 = 1.d6
         minvt41 = 1.d6
         do i=1,nl
	    pl11 = el11(i)/( gamma * nu11**3.0d0  * 1.d0/2.d0 /n10(i) )
	    pl21 = el21(i)/( gamma * nu21**3.0d0  * 1.d0/2.d0 /n20(i) )
	    pl31 = el31(i)/( gamma * nu31**3.0d0  * 1.d0/2.d0 /n30(i) )
	    pl41 = el41(i)/( gamma * nu41**3.0d0  * 1.d0/2.d0 /n40(i) )
	    vt11(i) = -ee*nu11 / log( abs(pl11) / (2.0d0*n10(i)) )
	    vt21(i) = -ee*nu21 / log( abs(pl21) / (2.0d0*n20(i)) )
	    vt31(i) = -ee*nu31 / log( abs(pl31) / (2.0d0*n30(i)) )
	    vt41(i) = -ee*nu41 / log( abs(pl41) / (2.0d0*n40(i)) )
	    hr210(i) = sl210(i) -hplanck*vlight*nu21 *a21_einst(i)*pl21
	    hr310(i) = sl310(i) -hplanck*vlight*nu31 *a31_einst(i)*pl31
	    hr410(i) = sl410(i) -hplanck*vlight*nu41 *a41_einst(i)*pl41

            minvt11 = min( minvt11,vt11(i) )
	    minvt21 = min( minvt21,vt21(i) )
	    minvt31 = min( minvt31,vt31(i) )
	    minvt41 = min( minvt41,vt41(i) )
         enddo

         ! Checking for errors in Tvibs
         if (minvt11 .le. 0.d0) then
            ierr = 26
            varerr = minvt11
            return
         elseif (minvt21 .le. 0.d0) then
            ierr = 27
            varerr = minvt21
            return
         elseif (minvt31 .le. 0.d0) then
            ierr = 28
            varerr = minvt31
            return
         elseif (minvt41 .le. 0.d0) then
            ierr = 29
            varerr = minvt41
            return
         endif

         v626t1(1:nl)=vt11(1:nl)
         v628t1(1:nl)=vt21(1:nl)
         v636t1(1:nl)=vt31(1:nl)
         v627t1(1:nl)=vt41(1:nl)
!         call dinterconnection( v626t1, vt11 )
!         call dinterconnection ( v628t1, vt21 )
!         call dinterconnection ( v636t1, vt31 )
!         call dinterconnection ( v627t1, vt41 )

      else

         do i=1,nl
	    pl21 = el21(i)/( gamma * nu21**3.0d0 * 1.d0/2.d0 / n20(i) )
	    pl31 = el31(i)/( gamma * nu31**3.0d0 * 1.d0/2.d0 / n30(i) )
	    pl41 = el41(i)/( gamma * nu41**3.0d0 * 1.d0/2.d0 / n40(i) )
	    hr210(i) = sl210(i) -hplanck*vlight*nu21 *a21_einst(i)*pl21
	    hr310(i) = sl310(i) -hplanck*vlight*nu31 *a31_einst(i)*pl31
	    hr410(i) = sl410(i) -hplanck*vlight*nu41 *a41_einst(i)*pl41
 	    if (itt_cza.eq.13) then
               pl12 = el12(i)/( gamma*nu121**3.0d0 * 2.d0/4.d0 /n11(i) )
               hr121(i) = - hplanck*vlight * nu121 * a12_einst(i)*pl12
               hr121(i) = hr121(i) + sl121(i)
            endif
         enddo

      endif

                                ! K/Dday
      do i=1,nl
         hr110(i)=hr110(i)*dble( hrkday_factor(i) / nt(i) )
         hr210(i)=hr210(i)*dble( hrkday_factor(i) / nt(i) )
         hr310(i)=hr310(i)*dble( hrkday_factor(i) / nt(i) )
         hr410(i)=hr410(i)*dble( hrkday_factor(i) / nt(i) )
         hr121(i)=hr121(i)*dble( hrkday_factor(i) / nt(i) )
      end do


c     final
      return
c     
      end


c *** Old NLTEdlvr11_FB626CTS_02 ***

c***********************************************************************
      
      subroutine NLTEdlvr11_FB626CTS ( hr110CTS, nl_cts_real )

c***********************************************************************

      implicit none

!!!!!!!!!!!!!!!!!! common variables and constants

      include 'nlte_paramdef.h'
      include 'nlte_commons.h'
	

c Arguments 
      real*8 hr110CTS(nl_cts)   ! output
      integer  nl_cts_real      ! i

c local variables

      real*8 n11CTS(nl_cts), slopeTstar110(nl_cts)
      real*8 n10(nl_cts), co2t, codbl, n2dbl, o3pdbl
      real*8 d19c1, d19cp1, l11, p11
      real*8 a11_einst(nl_cts), hcv, maxslope
      integer i, isot

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!  start program

      nu11 = dble(nu(1,1))
      hcv =  hplanck*vlight*nu11

      call zero2v (hr110CTS,n11CTS,nl_cts)

      do i=1,nl_cts_real

         co2t = dble ( co2_cts(i) *(imr(1)+imr(3)+imr(2)+imr(4)) )
         n10(i) = dble( co2_cts(i) * imr(1) )
         codbl = dble(co_cts(i))
         o3pdbl = dble(o3p_cts(i))
         n2dbl = dble(n2_cts(i))

         call GETK_dlvr11 ( t_cts(i) )
         isot = 1
         d19c1 = k19ca(isot)*co2t + k19cb(isot)*n2dbl
     $        + k19cc(isot)*codbl
         d19cp1 = k19cap(isot)*co2t + k19cbp(isot)*n2dbl
     $        + k19ccp(isot)*codbl
         l11 = d19c1 + k20c(1)*o3pdbl
         p11 = ( d19cp1 + k20cp(1)*o3pdbl ) * n10(i)
         
         a11_einst(i) = a1_010_000 * 1.8d0/4.d0 * taustar11_cts(i)
         
         n11CTS(i) = p11 / (l11 + a11_einst(i))

         hr110CTS(i) = - n11CTS(i) * a11_einst(i) * hcv
         hr110CTS(i) = hr110CTS(i)*
     $        dble( hrkday_factor_cts(i) / nt_cts(i) ) !K/Day

      enddo


c calculo de la altura de transicion, a partir de Tstar
c y merging con el hr110(i), ya calculado con CZALU

      slopeTstar110(1) = taustar11_cts(2)-taustar11_cts(1)
      slopeTstar110(nl_cts_real) = taustar11_cts(nl_cts_real) -
     $     taustar11_cts(nl_cts_real-1)
      maxslope = max( slopeTstar110(1),slopeTstar110(nl_cts_real)) 
      if (nl_cts_real .gt. 2) then 
         do i=2,nl_cts_real-1
            slopeTstar110(i) = ( taustar11_cts(i+1) -
     $           taustar11_cts(i-1) ) * 0.5d0
            if ( slopeTstar110(i) .gt. maxslope ) then
                                !write (*,*) i, pl_cts(i), maxslope, slopeTstar110(i)
               maxslope=slopeTstar110(i)
            endif
         enddo
      endif

c
      return
      end


c***********************************************************************
c     hrkday_convert.f                              
c     
c     fortran function that returns the factor for conversion from          
c     hr' [erg s-1 cm-3] to hr [ k day-1 ]           
c     
c     mar 2010        fgg      adapted to GCM
c     jan 99          malv     add o2 as major component. 
c     ago 98          malv     also returns cp_avg,pm_avg 
c     jul 98 		malv	 first version.	                
c***********************************************************************
      
      function hrkday_convert                        
     @     ( mmean_nlte,cpmean_nlte )         
      use time_phylmdz_mod, only: daysec
      use param_v4_h, only: n_avog
      implicit none                           
      
c     argumentos                                    
      real mmean_nlte,cpmean_nlte
      real hrkday_convert                           
      
ccccccccccccccccccccccccccccccccccccc
      
      hrkday_convert = daysec * n_avog / 
     &     ( cpmean_nlte * 1.e4 * mmean_nlte ) 
      
c     end                                           
      return                                  
      end        


c     *** Old NLTEdlvr11_ERRORS ***
c     
c***********************************************************************



      subroutine ERRORS (ierr,varerr)

c***********************************************************************

      implicit none

c Arguments
      integer ierr
      real*8 varerr
      
c***************

      if (ierr .eq. 15) then
         write (*,*) ' ERROR in MZESC110.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   c2 < 0 after INTZHUNT_CTS'
         
      elseif (ierr .eq. 16) then
         write (*,*) ' ERROR in MZESC110.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   p2 < 0 after INTZHUNT_CTS'
         
      elseif (ierr .eq. 17) then
         write (*,*) ' ERROR in MZESC110.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   mr2 < 0 after INTZHUNT_CTS'
         
      elseif (ierr .eq. 18) then
         write (*,*) ' ERROR in MZESC110.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   t2 < 0 after INTZHUNT_CTS'
         
      elseif (ierr .eq. 19) then
         write (*,*) ' ERROR in MZESC110.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   st2 < 0 after INTZHUNT_CTS'
         
      elseif (ierr .eq. 33) then
         write (*,*) ' ERROR in MZESC110.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   [CO2] < 0 at TOA.'
         
      elseif (ierr .eq. 42) then
         write (*,*) ' ERROR in MZTUD110.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   Atmospheric transmittance too large. '
         
      elseif (ierr .eq. 43) then
         write (*,*) ' ERROR in MZTUD110.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   [CO2] < 0 at  CurtisMatrix top.'
         
      elseif (ierr .eq. 45) then
         write (*,*) ' ERROR in MZTUD110.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   c2 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 46) then
         write (*,*) ' ERROR in MZTUD110.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   p2 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 47) then
         write (*,*) ' ERROR in MZTUD110.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   mr2 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 48) then
         write (*,*) ' ERROR in MZTUD110.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   t2 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 49) then
         write (*,*) ' ERROR in MZTUD110.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   st2 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 75) then
         write (*,*) ' ERROR in MZTUD110.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   c1 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 76) then
         write (*,*) ' ERROR in MZTUD110.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   p1 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 77) then
         write (*,*) ' ERROR in MZTUD110.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   mr1 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 78) then
         write (*,*) ' ERROR in MZTUD110.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   t1 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 79) then
         write (*,*) ' ERROR in MZTUD110.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   st1 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 83) then
         write (*,*) ' ERROR in MZTUD121.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   [CO2] < 0 at  CurtisMatrix top.'
         
      elseif (ierr .eq. 85) then
         write (*,*) ' ERROR in MZTUD121.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   c1 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 86) then
         write (*,*) ' ERROR in MZTUD121.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   p1 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 87) then
         write (*,*) ' ERROR in MZTUD121.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   mr1 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 88) then
         write (*,*) ' ERROR in MZTUD121.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   t1 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 89) then
         write (*,*) ' ERROR in MZTUD121.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   st1 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 51) then
         write (*,*) ' ERROR in MZTVC121.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   Ground transmittance vector VC < 0 '
         
      elseif (ierr .eq. 52) then
         write (*,*) ' ERROR in MZTVC121.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   Atmospheric transmittance too large. '
         
      elseif (ierr .eq. 53) then
         write (*,*) ' ERROR in MZTVC121.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   [CO2] < 0 at  CurtisMatrix top.'
         
      elseif (ierr .eq. 55) then
         write (*,*) ' ERROR in MZTVC121.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   c2 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 56) then
         write (*,*) ' ERROR in MZTVC121.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   p2 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 57) then
         write (*,*) ' ERROR in MZTVC121.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   mr2 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 58) then
         write (*,*) ' ERROR in MZTVC121.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   t2 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 59) then
         write (*,*) ' ERROR in MZTVC121.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   st2 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 63) then
         write (*,*) ' ERROR in MZESC121sub.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   [CO2] < 0 at  CurtisMatrix top.'
         
      elseif (ierr .eq. 65) then
         write (*,*) ' ERROR in MZESC121sub.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   c2 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 66) then
         write (*,*) ' ERROR in MZESC121sub.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   p2 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 67) then
         write (*,*) ' ERROR in MZESC121sub.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   mr2 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 68) then
         write (*,*) ' ERROR in MZESC121sub.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   t2 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 69) then
         write (*,*) ' ERROR in MZESC121sub.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   st2 < 0 after INTZHUNT'
         
      elseif (ierr .eq. 21) then
         write (*,*) ' ERROR in CZA.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   l11 < 0 '
         
      elseif (ierr .eq. 22) then
         write (*,*) ' ERROR in CZA.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   l21 < 0 '
         
      elseif (ierr .eq. 23) then
         write (*,*) ' ERROR in CZA.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   l31 < 0 '
         
      elseif (ierr .eq. 24) then
         write (*,*) ' ERROR in CZA.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   l41 < 0 '
         
      elseif (ierr .eq. 25) then
         write (*,*) ' ERROR in CZA.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   l12 < 0 '
         
      elseif (ierr .eq. 26) then
         write (*,*) ' ERROR in CZA.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   Negative vibr.temp   xvt11 < 0 '
         
      elseif (ierr .eq. 27) then
         write (*,*) ' ERROR in CZA.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   Negative vibr.temp   xvt21 < 0 '
         
      elseif (ierr .eq. 28) then
         write (*,*) ' ERROR in CZA.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   Negative vibr.temp   xvt31 < 0 '
         
      elseif (ierr .eq. 29) then
         write (*,*) ' ERROR in CZA.    ierr=',ierr
         write (*,*) '                VAR available=', varerr
         write (*,*) '   Negative vibr.temp   xvt41 < 0 '
         

      endif


      stop ' Stopped in NLTE scheme due to severe error.'
      end
