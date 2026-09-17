c****************************************************************************
c
c       Merging of different common blocks used in the new NLTE 15um param
c
c       jan 2012    fgg+malv
c****************************************************************************
c *** Old datitos.cmn ***
c
	common /spectralv11/ elow, deltanu
	real elow(nisot,nb), deltanu(nisot,nb)


	common/nu_levs_bands_v11/ nu11, nu12, nu121,  
     @  	nu21, nu31, nu41
	real*8 nu11, nu12, nu121
	real*8 nu21
	real*8 nu31
	real*8 nu41


	common /aeinstein1v11/ a1_010_000, a1_020_010
	common /aeinstein2v11/ a2_010_000        
	common /aeinstein3v11/ a3_010_000        
	common /aeinstein4v11/ a4_010_000        

	real*8 a1_010_000, a1_020_010
	real*8 a2_010_000        
	real*8 a3_010_000        
	real*8 a4_010_000


c *** Old tabulation.cmn ***

	common/input_tab_v11/ lnpnbtab, 
     @  	tstar11tab, tstar21tab, tstar31tab, tstar41tab,
     @  	vc210tab, vc310tab, vc410tab

	real*8 lnpnbtab(nztabul)
	real*8 vc210tab(nztabul), vc310tab(nztabul), vc410tab(nztabul)
	real*8 tstar11tab(nztabul), tstar21tab(nztabul), 
     @         tstar31tab(nztabul), tstar41tab(nztabul)


c *** Old nlte_results.cmn ***

	common/input_avilable_from/ input_cza
	integer input_cza 

c temperatura vibracional de entrada:
	common/temp626/ v626t1
	common/temp628/ v628t1 
	common/temp636/ v636t1 
	common/temp627/ v627t1 
	real*8 v626t1(nl)
	real*8 v628t1(nl)
	real*8 v636t1(nl)
	real*8 v627t1(nl)

c output de cza.for
	common /tv15um/	vt11, vt12, vt21, vt31, vt41
	real*8  vt11(nl), vt12(nl), vt21(nl), vt31(nl), vt41(nl)

	common /hr15um/	hr110,hr210,hr310,hr410,hr121
	real*8  hr110(nl),hr121(nl), 
     @      	hr210(nl),hr310(nl),hr410(nl)

        common/sf15um/ el11,el12, el21, el31, el41
        real*8 el11(nl), el12(nl)
        real*8 el21(nl)
        real*8 el31(nl)
        real*8 el41(nl)

        common/sl15um/ sl110,sl121, sl210,sl310,sl410
        real*8 sl110(nl), sl121(nl)
        real*8 sl210(nl)
        real*8 sl310(nl)
        real*8 sl410(nl)


c *** Old matrices.cmn ***


c curtis matrix de cza:
	common/curtis_matrixes_15um/ c110,c121, c210,
     @  	c310,c410,
     @  	vc110,vc121,vc210,vc310,vc410
	real*8 c110(nl,nl), c121(nl,nl)
	real*8 c210(nl,nl)
	real*8 c310(nl,nl)
	real*8 c410(nl,nl)
	real*8 vc110(nl), vc121(nl)
	real*8 vc210(nl), vc310(nl), vc410(nl)
 
! for the cool-to-space formulation:
!
	common/taustar_15um/ taustar11, taustar21, taustar31, 
     @         taustar41, taustar12, taustar11_cts
	real*8 taustar11(nl), taustar21(nl), taustar31(nl)
	real*8 taustar41(nl), taustar12(nl)
	real*8 taustar11_cts(nl_cts)


c *** Old atmref.cmn ***


c NLTE Subgrid 
c
        common /atm_nl/ zl, t, pl, nt, co2, n2, co, o3p, 
     @    co2vmr, n2vmr, covmr, o3pvmr, 
     @    hrkday_factor

        real zl(nl), t(nl), pl(nl), nt(nl),  
     @    co2(nl), n2(nl), co(nl), o3p(nl), 
     @    co2vmr(nl), n2vmr(nl), covmr(nl), o3pvmr(nl), 
     @    hrkday_factor(nl)


c Subgrid Transmittances 
c
        common /atm_ny/ zy, ty, py, nty, co2y
        real zy(nzy), ty(nzy), py(nzy), nty(nzy), co2y(nzy)

c Grids and indexes
	common/deltazetas/ deltaz, deltazy, deltaz_cts, deltazy_cts, 
     @        jlowerboundary, jtopboundary, jtopCTS
	real    deltaz, deltazy, deltaz_cts, deltazy_cts
	integer jlowerboundary, jtopboundary, jtopCTS


c NLTE-CTS Subgrid 
c
        common /atm_nl_cts/ zl_cts, t_cts, pl_cts, nt_cts, 
     @    co2_cts, n2_cts, co_cts, o3p_cts, 
     @    co2vmr_cts, n2vmr_cts, covmr_cts, o3pvmr_cts, 
     @    hrkday_factor_cts,mmean_cts,cpnew_cts

        real zl_cts(nl_cts), t_cts(nl_cts), pl_cts(nl_cts), 
     @    nt_cts(nl_cts), co2_cts(nl_cts), 
     @    n2_cts(nl_cts), co_cts(nl_cts),
     @    o3p_cts(nl_cts), co2vmr_cts(nl_cts), n2vmr_cts(nl_cts), 
     @    covmr_cts(nl_cts), o3pvmr_cts(nl_cts), 
     @    hrkday_factor_cts(nl_cts),mmean_cts(nl_cts),
     @    cpnew_cts(nl_cts)


c CTS Subgrid Transmittances 
c
        common /atm_ny_cts/ zy_cts, ty_cts, py_cts, nty_cts, co2y_cts
        real zy_cts(nzy_cts), ty_cts(nzy_cts), py_cts(nzy_cts), 
     @          nty_cts(nzy_cts), co2y_cts(nzy_cts)


c *** Old rates.cmn ***

	common/rates_vt/ 
     @      k19ba(4),k19bb(4),k19bc(4), k19bap(4),k19bbp(4),k19bcp(4),
     @      k19ca(4),k19cb(4),k19cc(4), k19cap(4),k19cbp(4),k19ccp(4),
     @      k20b(4),k20c(4), k20bp(4),k20cp(4)

	real*8 k19ba,k19bb,k19bc, k19bap,k19bbp,k19bcp
	real*8 k19ca,k19cb,k19cc, k19cap,k19cbp,k19ccp
	real*8 k20b,k20c, k20bp,k20cp

	common/rates_vv/ 
     @      	k21b(4),k21c(4), k21bp(4),k21cp(4),
     @      	k33c, k33cp(2:4)

	real*8 k21b,k21c, k21bp,k21cp
	real*8 k33c, k33cp

	common/rates_last/ k23k21c, k24k21c, k34k21c, 
     @      	k23k21cp, k24k21cp, k34k21cp

	real*8 k23k21c,k24k21c,k34k21c, k23k21cp,k24k21cp,k34k21cp 



c *** Old curtis.cmn ***

        common /ini_file/ ibcode1
	character ibcode1*1

	common/block1/ alsa,alda,ka,kr
	real*8 ka(nbox_max),alsa(nbox_max),alda(nbox_max)
	integer kr

	common/block2/ hisfile
	character hisfile*75

	common/block3/ pp,ta,w
	real*8 pp,ta(nbox_max),w

	common/block4/ no,sk1,xls1,xld1,thist,nbox
	real*8	sk1(nhist,nbox_max)
	real*8  xls1(nhist,nbox_max)	
	real*8 	xld1(nhist,nbox_max)	
	real*8	thist(nhist)		
	real*8	no(nbox_max)		
	integer nbox		

	common/block5/eqw, aa,  cc, dd, ddbox, ccbox, mr, mr_cts
	real*8 eqw, aa, cc, dd
	real*8 ddbox(nbox_max), ccbox(nbox_max)
	real*8  mr(nzy), mr_cts(nzy_cts)

        common/blockstore/no_stored, sk1_stored, xls1_stored, 
     &          xld1_stored, thist_stored, nbox_stored, 
     &          mm_stored
         real*8 sk1_stored(nb,nhist,nbox_max)
	 real*8 xls1_stored(nb,nhist,nbox_max)	
	 real*8 xld1_stored(nb,nhist,nbox_max)	
	 real*8 thist_stored(nb,nhist)		
	 real*8 no_stored(nb,nbox_max)		
	 integer nbox_stored(nb), mm_stored(nb) 

c*****************************************************


c*************************************************************




c****************************************************************************



