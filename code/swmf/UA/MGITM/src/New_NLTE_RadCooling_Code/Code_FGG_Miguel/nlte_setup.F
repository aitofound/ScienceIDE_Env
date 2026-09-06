c***********************************************************************

      subroutine nlte_setup

c     malv    Oct 09          Adapt mz1d_onlyTCR_MUCHASveces.f to "V09"
c     malv    Sep 07          Add LU deccomp & repetition option to test CPU
c     malv    Jan 07          Add new vertical fine-grid for NLTE
c     apr 06  malv            Read date,effuv from Driver. T fixed at zbott.
c     2003    fgg             Double precission in UV, Photoq, Conduct & Diff
c     oct 02  malv            V02: New scheme to allow for continuity eq.
c     dec 01  malv            See changes/progress of the code in mz1d.actual
c     nov 01  malv            adapt for parameterizations of tcr y shr
c     nov 98  malv            add chemical & photochem. processes
c     jul 98  malv            transic hiperb con zs fuera de la region
c     equil hidrostatico. smoothing en cr y sh
c     jan 98	malv		first version
c***********************************************************************

      implicit none

      include   'datafile.h'
      include	'nlte_paramdef.h'
      include	'nlte_commons.h'


c***************

c     local variables

      integer 	i, k, lun1, lun2
      real*8          xx
      character	isotcode*2


c     formats
 132  format (i2)

c**********************************************************************

c     *** Groups old 1-d model subroutines SETTINGS and LeeESCTVCISO_dlvr11
c     *** Both were called in old NLTEdlvr11_SETUP ***

c     *** Old SETTINGS ***

      lun1 = 1
      lun2 = 2

      do k=1,nisot
         write (isotcode,132) indexisot(k)
         open (lun1,
     $        file=trim(datafile)//'/NLTEDAT/enelow'
     $        //isotcode//'.dat',status='old')
         open (lun2,
     $        file=trim(datafile)//'/NLTEDAT/deltanu'
     $        //isotcode//'.dat',status='old')
         read (lun1,*)
         read (lun2,*)
         read (lun1,*) (elow(k,i), i=1,nb)
         read (lun2,*) (deltanu(k,i), i=1,nb)
         close (lun1)
         close (lun2)
      end do

      a1_010_000 = 1.3546d00
      a2_010_000 = 1.3452d00
      a3_010_000 = 1.1878d00
      a4_010_000 = 1.2455d00
      a1_020_010 = 4.35d0
      

c     *** Old LeeESCTVCISO_dlvr11 ***

      open( 11, file=trim(datafile)//
     $     '/NLTEDAT/parametp_Tstar_IAA1204.dat' )
      read (11, *)
      do i=1,nztabul
         read (11,*) lnpnbtab(i), tstar11tab(i),
     $        tstar21tab(i), tstar31tab(i), tstar41tab(i)
      enddo
      close (11)

      open( 12, file=trim(datafile)//
     $     '/NLTEDAT/parametp_VC_IAA1204.dat' )
      read (12, *)
      do i=1,nztabul
         read (12,*) xx, vc210tab(i), vc310tab(i), vc410tab(i)
      enddo
      close (12)
      xx=xx



      call LeeHISTOGRMS

c     end subroutine

      return
      end



c***********************************************************************
      subroutine LeeHISTOGRMS
c***********************************************************************

      implicit none

      include 'datafile.h'
      include	'nlte_paramdef.h'
      include	'nlte_commons.h'


c     local variables and constants
      integer 	ihist


c***********************************************************************

                                ! Banda fundamental 
                                !
      hisfile = trim(datafile)//
     $     '/NLTEDAT/hid26-1.dat'
      ihist = 1
      call rhist_03 (ihist)


                                ! First Hot bands	
                                !     
      hisfile = trim(datafile)//
     $     '/NLTEDAT/hid26-2.dat'
      ihist = 2
      call rhist_03 (ihist)

      hisfile = trim(datafile)//
     $     '/NLTEDAT/hid26-3.dat'
      ihist = 3
      call rhist_03 (ihist)

      hisfile = trim(datafile)//
     $     '/NLTEDAT/hid26-4.dat'
      ihist = 4
      call rhist_03 (ihist)




      return
      end


c     *** Old GETK_dlvr11.f ***

c***********************************************************************

      subroutine GETK_dlvr11 (tt)

c***********************************************************************

      implicit none

      include 'nlte_paramdef.h'
      include 'nlte_commons.h'

c     arguments
      real 		tt	! i. temperature

!     ! local variables:
      real*8 k20x, k20xb, k20xc
      real*8 k19xca,k19xcb,k19xcc
      real*8 k19xba,k19xbb,k19xbc
      real*8 k21x,k21xa,k21xb,k21xc
      real*8 anu, factor , tdt
      integer 	i

c***********************************************************************

      tdt = dble(tt)

                                !! k19 & k20

      k20x = 3.d-12
      k20xc = k20x * rf20
      k20xb = 2.d0 * k20xc

      k19xca = 4.2d-12 * exp( -2988.d0/tdt + 303930.d0/tdt**2.d0 )
      if (tt.le.175.) k19xca = 3.3d-15
      k19xcb = 2.1d-12 * exp( -2659.d0/tdt + 223052.d0/tdt**2.d0 )
      if (tt.le.175.) k19xcb = 7.6d-16
      k19xca = k19xca * rf19
      k19xcb = k19xcb * rf19
      k19xcc = k19xcb

      factor = 2.5d0
      k19xba = factor * k19xca
      k19xbb = factor * k19xcb
      k19xbc = factor * k19xcc

      do i = 1, nisot

         k19ba(i) = k19xba
         k19ca(i) = k19xca
         k19bb(i) = k19xbb
         k19cb(i) = k19xcb
         k19bc(i) = k19xbc
         k19cc(i) = k19xcc

         k20b(i) = k20xb
         k20c(i) = k20xc

         anu = dble( nu(i,2)-nu(i,1) )

         k19bap(i) = k19ba(i) * 2.d0 * exp( -ee*anu/tdt )
         k19bbp(i) = k19bb(i) * 2.d0 * exp( -ee*anu/tdt )
         k19bcp(i) = k19bc(i) * 2.d0 * exp( -ee*anu/tdt )

         k20bp(i) = k20b(i)*4.d0/2.d0 * exp( -ee/tdt * anu )

         anu = dble( nu(i,1) )

         k19cap(i) = k19ca(i) * 2.d0 * exp( -ee*anu/tdt )
         k19cbp(i) = k19cb(i) * 2.d0 * exp( -ee*anu/tdt )
         k19ccp(i) = k19cc(i) * 2.d0 * exp( -ee*anu/tdt )

         k20cp(i) = k20c(i)*2.d0/1.d0 * exp( -ee/tdt * anu )

      end do


                                !! k21 &  k23k21c &  k24k21c & k34k21c

      k21x = 2.49d-11
      k21xb = k21x
      k21xa = 3.d0/2.d0 * k21xb
      k21xc = k21xb / 2.d0

      k21xa = k21xa * rf21a
      k21xb = k21xb * rf21b
      k21xc = k21xc * rf21c

      do i = 1, nisot
	 k21b(i) = k21xb
	 k21c(i) = k21xc
	 k21bp(i) = k21b(i) *
     @        exp( -ee/tdt* dble(nu(i,2)-nu(i,1)-nu(1,1)) )
	 k21cp(i) = k21c(i) *
     @        exp( -ee/tdt * dble(nu(i,1)-nu(1,1)) )
      end do

      k23k21c = k21xc
      k24k21c = k21xc
      k34k21c = k21xc
      k23k21cp = k23k21c*2.d0/2.d0 *
     @     exp( -ee/tdt* dble(nu(2,1)-nu(3,1)) )
      k24k21cp = k24k21c*2.d0/2.d0 *
     @     exp( -ee/tdt* dble(nu(2,1)-nu(4,1)) )
      k34k21cp = k34k21c*2.d0/2.d0 *
     @     exp( -ee/tdt* dble(nu(3,1)-nu(4,1)) )


                                !! k33

      k33c = k21x * rf33bc
      do i=2,nisot
	 k33cp(i) = k33c *
     @        exp( -ee/tdt * dble(nu(1,2)-nu(1,1)-nu(i,1)) )
      end do


      return
      end







