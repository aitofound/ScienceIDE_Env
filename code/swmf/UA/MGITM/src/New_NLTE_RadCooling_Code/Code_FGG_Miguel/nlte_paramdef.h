c****************************************************************************
c
c       Merging of different parameters definitions for new NLTE 15um param
c
c       jul 2012    fgg+malv
c****************************************************************************
c *** Old mz1d.par ***
! Grids parameters :

	integer nztabul          ! # points in tabulation of Tesc & VC (ISO)
	parameter ( nztabul=79 ) 

! NLTE parameters:

	integer nltot		! incluye el actual # alt in NLTE module 
	parameter ( nltot=20 )  ! y el # alturas del Tstar110

	integer nl		! actual # alt in NLTE module & C.Matrix 
	parameter ( nl=12 )
	integer nl2		! = nl-2, needed for matrix inversion (mmh2)
	parameter ( nl2=nl-2 )	

	integer nzy
	parameter ( nzy = (nl-1)*4 + 1 )  ! Fine grid for C.Matrix

	integer nl_cts         ! actual # alt para Tstar110
	parameter ( nl_cts = 2 + nltot-nl )
	integer nzy_cts         ! fine grid for transmit calculation
	parameter ( nzy_cts = (nl_cts-1)*4 + 1 ) 


!  Other NLTE parameters:
	integer 	nisot		! number of isotopes considered
	integer 	nb 		! number of bands included
	parameter ( nisot=4, nb=41 )

	integer 	nhist			! # of temps in histogr.
	parameter 	( nhist = 36 )          ! (get it from histograms!)

	integer         nbox_max
	parameter       ( nbox_max = 4 )       ! max.# boxes in histogram


c *** Old tcr_15um.h ***

        integer itt_cza                        ! Selection of NLTE scheme
        parameter       ( itt_cza = 13 )

        real    Ptop_atm, Pbottom_atm          ! Upper and lower limits of
                                               ! NLTE model
        parameter       ( Ptop_atm = 3.e-10 , Pbottom_atm = 2.e-5 ) 
        

        real*8  rf19,rf20,rf21a,rf21b,rf21c,rf33bc
        parameter       ( rf19 = 1.d0, rf20 = 1.d0, rf21a = 1.d0)
        parameter       ( rf21b = 1.d0, rf21c = 1.d0, rf33bc = 1.d0 )


c *** Old bloque_dlvr11.f ***

        real nu(nisot,8)
c data
        data nu(1,1),nu(1,2) /667.3801, 1335.1317/
        data nu(2,1)/662.3734/
        data nu(3,1)/648.4784/
        data nu(4,1)/664.7289/

        real nu12_0200,nu12_1000
        parameter      (nu12_0200 = 1285.4087)
	parameter      (nu12_1000 = 1388.1847) 

        integer indexisot(nisot)
        data indexisot/26,28,36,27/

	! ctes en el sistema cgs
        real*8  vlight, ee, hplanck, gamma
        parameter (vlight       = 2.9979245e10)
        parameter (ee           = 1.43876866)
        parameter (hplanck      = 6.6260755e-27)
        parameter (gamma        = 1.191043934e-5)


	! datos de marte
        real imr(nisot)
        data imr / 0.987, 0.00408, 0.0112, 0.000742 /




